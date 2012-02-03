#include <SoftwareSerial.h>
#include <Metro.h>
#include <FPScanner.h>
#include <Servo.h>
#include <PWMServo.h>

#define MS_SCANNING      0
#define MS_OPENING       1
#define MS_CLOSING       2
#define MS_ENROLLING     3
#define MS_HOLDING_OPEN  4
#define MS_ENROLL_SUPER  5

unsigned long hold_time[3]={3000,3000,600000}; //duration in ms that latch is held open indexed by access level

int debug_flag=0;
int debug_pin=A1;
int sig_pin=13;
int servo_pin=9;//6;
int prev_access_level=0;
//Servo servo;
PWMServo servo;
int latch_open=50;
int latch_closed=120;
int latch_pos=latch_closed;
int machine_state=MS_SCANNING;

Metro open_timer = Metro(3000);
Metro servo_move_timer = Metro(1000);
Metro a0_poll_timer(1000);

SoftwareSerial FPSerial(2, 3); //FingerPrint(FP) serial interface on pins 2(RX) and 3(TX)

// Wrapping the interface to SoftwareSerial and just passing the available(),read() and write()
// functions into the Finger Print Scanner module.  This was done because of the interference issues
// between the SoftwareSerial library and the Servo library.  FPScanner library was developed without
// dependencies on any particular serial comm. implementation.  Just wrap the desired interface
// in the three functions below.
int  serial_available(){return FPSerial.available();}
int  serial_read(){return FPSerial.read();}
void serial_write(void* data, int len){ FPSerial.write((const uint8_t*)data,len);}

FPScanner fps(serial_available,serial_read,serial_write);

void setup()  
{
  
  FPSerial.begin(9600);
  Serial.begin(9600);
  Serial.println("Bringing up FingerPrint Interface");
  
  pinMode(debug_pin,INPUT);
  digitalWrite(debug_pin,HIGH);
  
  pinMode(sig_pin,OUTPUT);
  
  pinMode(A0,INPUT);
  digitalWrite(A0,HIGH);

  fps.Flush();
  Serial.print("Enrolled Count: ");
  Serial.println(fps.Enrolled_Count());
 
 // Holding pin A0 low for 100msec during power up will delete all finger prints
  if(!digitalRead(A0)){
    digitalWrite(sig_pin,HIGH);
    delay(100);
    if(!digitalRead(A0)){
      Serial.println("Deleting all stored prints");
      fps.Cmd_PS_Empty();
      Serial.print("Enrolled Count: ");
      Serial.println(fps.Enrolled_Count());    
      machine_state = MS_ENROLL_SUPER;
    }else{
      digitalWrite(sig_pin,LOW);
    }
  }
  
  if(fps.Enrolled_Count()==0){
    machine_state = MS_ENROLL_SUPER;
  }
  
  close_latch(); 
}

void loop() // run over and over
{

  debug_flag = !digitalRead(debug_pin); //pull debug_pin low to turn on debug
//  debug_flag = true;
  fps.Set_Log_Level(debug_flag);
 
  switch(machine_state){

    case MS_SCANNING:

        switch( fps.Scan_for_Match() ){
          case FPScanner::FPS_NO_SCAN:
            break;
          case FPScanner::FPS_NO_MATCH:
            Serial.println("No Match");
            break;
          default:
            Serial.print("Match: ");
            Serial.println( fps.Matched_Index() );
            machine_state=MS_OPENING;
            break;
        }
      break;

    case MS_OPENING:
      prev_access_level = access_level();
      Serial.print("Opening Latch for: ");
      Serial.print(hold_time[access_level()]/1000);
      Serial.print("sec. ");
      open_latch(); // this call won't return until latch is open
      open_timer.reset();
      open_timer.interval(hold_time[access_level()]);
      machine_state=MS_HOLDING_OPEN;
      break;

    case MS_HOLDING_OPEN:
      log_seconds();
      servo.attach(servo_pin);
      servo.write(latch_open);
      if( open_timer.check() ){
        Serial.println(".MS_HOLDING_OPEN: timer elapsed");
        machine_state = MS_CLOSING;
      }
      switch( fps.Scan_for_Match() ){
        case FPScanner::FPS_NO_SCAN: //any scan will close, scan of index=1 (i.e. super user second finger)
                                     //will enter enroll sequence.
          break;
        default:
          machine_state=MS_CLOSING;
// Super user is identified by indices 0 and 1.  A consecutive scan of user(0) (e.g. super-users 1st finger) then
// user(1) (e.g. super-user's 2nd finger) will initiate an 'enroll' sequence.
          if( prev_access_level == 1 && access_level() == 2 ){
            machine_state=MS_ENROLLING;
          }
          break;
      }
      break;

    case MS_CLOSING:
      close_latch();
      machine_state = MS_SCANNING;           
      break;

    case MS_ENROLLING:
      close_latch();
      Serial.println("Enrolling:");
      digitalWrite(13,HIGH);
      if(fps.Enroll_New()){
// give the user immediate feedback that the enroll happened by opening the latch
        digitalWrite(13,LOW);
        open_latch(); // this call won't return until latch is open
        open_timer.reset();
        open_timer.interval(hold_time[0]);
      }else{
        blink13(4,250);
      }
      machine_state = MS_HOLDING_OPEN;
      break;

    case MS_ENROLL_SUPER:
      Serial.println("Enrolling Super User");
      digitalWrite(13,HIGH);
      if(fps.Enroll_New()){
        digitalWrite(13,LOW);
        delay(2000);
      }else{
        blink13(4,250);
      }
      if(fps.Enrolled_Count()>=2){
        open_latch(); // this call won't return until latch is open
        open_timer.reset();
        open_timer.interval(hold_time[0]);
        machine_state = MS_HOLDING_OPEN;
      }
      break;
      
    default:
      Serial.print("Unknown machine state: ");
      Serial.println(machine_state);
      machine_state = MS_SCANNING;
      break;
  }
}

/*
  The first recorded finger print (index 0) is super user 1 
  The second recorded finger print (index 1) is super user 2 
  All others are NOT super users
*/
int access_level(){
  switch(fps.Matched_Index()){
    case 0:
      return 1;
    case 1:
      return 2;
    default:
      return 0;
  }
}

void open_latch(){
  servo.attach(servo_pin);
  servo_move_timer.reset();
  while(!servo_move_timer.check()){
    servo.write(latch_open);
  }
//  servo.detach();
}


void close_latch(){
  Serial.println("Closing latch.");
  servo.attach(servo_pin);
  servo_move_timer.reset();
  while(!servo_move_timer.check()){
    servo.write(latch_closed);
  }
//  servo.detach();
}

void log_seconds(){
  static unsigned long last_millis;
  if(millis()/1000 != last_millis){
    last_millis = millis()/1000;
    Serial.print(".");
  }
}

void blink13( int ct, long mils ){
  int i;
  Serial.print("blink13:ct:");
  Serial.print(ct);
  Serial.print("mils:");
  Serial.println(mils);
  for(i=0;i<ct;i++)
  {
    digitalWrite(13,HIGH);
    delay(mils);
    digitalWrite(13,LOW);
    delay(mils);
  }
}
