#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "FPScanner.h"

#define TIMEOUT_READ_CT 40000
#define FPS_MSG(m) m,sizeof(m)


byte ARA_HEADER[6]={0xEF,0x01,0xFF,0xFF,0xFF,0xFF};

byte         PS_GetImage[6] ={0x01,0x00,0x03,0x01,0x00,0x05};
byte          PS_GenChar[7] ={0x01,0x00,0x04,0x02,0x01,0x00,0x08};
byte           PS_Search[11]={0x01,0x00,0x08,0x04,0x01,0x00,0x00,0x00,0x79,0x00,0x87};
byte         PS_RegModel[6] ={0x01,0x00,0x03,0x05,0x00,0x09};
byte        PS_StoreChar[9] ={0x01,0x00,0x06,0x06,0x01,0x00,0x02,0x00,0x10};
byte            PS_Empty[6] ={0x01,0x00,0x03,0x0D,0x00,0x11};
byte      PS_ReadSysPara[6] ={0x01,0x00,0x03,0x0F,0x00,0x13};
byte PS_ValidTempleteNum[6] ={0x01,0x00,0x03,0x1D,0x00,0x21};


void FPScanner::enroll_timeout_reset(){
  this->_previous_millis = millis();
}

int FPScanner::enroll_timeout_check(){
  if(millis() - this->_previous_millis >= 10000) {
    this->_previous_millis += 10000;
    return 1;
  }
  return 0;
}

FPScanner::FPScanner( int(*available)(),int(*read)(),void(*write)(void*,int) ){
  _enrolled_count = 0;
  _available = available;
  _read      = read;
  _write     = write;
}

void FPScanner::Cmd_PS_GetImage(void)        { write_to_scanner( FPS_MSG(PS_GetImage) ); }
void FPScanner::Cmd_PS_Search(void)          { write_to_scanner( FPS_MSG(PS_Search) ); }
void FPScanner::Cmd_PS_ReadSysPara(void)     { write_to_scanner( FPS_MSG(PS_ReadSysPara) );}
void FPScanner::Cmd_PS_RegModel(void)        { write_to_scanner( FPS_MSG(PS_RegModel) );}
void FPScanner::Cmd_PS_ValidTempleteNum(void){ write_to_scanner( FPS_MSG(PS_ValidTempleteNum) );}
void FPScanner::Cmd_PS_Empty(void)            { write_to_scanner( FPS_MSG(PS_Empty) );}

int FPScanner::Matched_Index(void)        { return _matched_fingerprint; }

void Log(char* txt){
  Serial.print(txt);
}
void Log(int ival){
  Serial.print(ival);
}
void Log(byte bval, int format){
  Serial.print(bval,format);
}

void Logln(char* txt){
  Serial.println(txt);
}
void Logln(int ival){
  Serial.println(ival);
}

void FPScanner::Cmd_PS_GenChar(int buf)
{
  if(buf==0){
    PS_GenChar[4]=0x00;
    PS_GenChar[6]=0x07;
  }else{
    PS_GenChar[4]=0x01;
    PS_GenChar[6]=0x08;
  }
  write_to_scanner( FPS_MSG(PS_GenChar) ); 
}

void FPScanner::Cmd_PS_StoreChar(int ndx)
{
   int i;
   if(ndx >= 0 && ndx < 150){
     PS_StoreChar[6]=ndx;
     PS_StoreChar[8]=0;
 //Setup the checksum
     for(i=0;i<7;i++){
       PS_StoreChar[8]+=PS_StoreChar[i];
     }
   }
   write_to_scanner( FPS_MSG(PS_StoreChar) );
}

int FPScanner::Enroll_New(void)
{
  int ct;
 
  enroll_timeout_reset();
//  _debug_flag = true;  
 
  Logln("First GetImage");

  do{
    if( enroll_timeout_check() ){ return 0; }
    Cmd_PS_GetImage();
  } while(!Poll_Response());

  Logln("GenChar(0)");
  Cmd_PS_GenChar(0);
  if(!Poll_Response()){
    Logln("GenChar(0) FAILED");
    return 0;
  }

  Logln("Second GetImage");
  do{
    if( enroll_timeout_check() ){ return 0; }
    Cmd_PS_GetImage();
  }while(!Poll_Response());

  Logln("GenChar(1)");
  Cmd_PS_GenChar(1);
  if(!Poll_Response()){
    Logln("GenChar(1) FAILED");
    return 0;
  }

  Cmd_PS_RegModel();
  if(!Poll_Response()){
    Logln("RegModel FAILED");
    return 0;
  }
  ct = Enrolled_Count();
  Log("Enrolled Count: ");
  Logln(ct);
  
  Log("StoreChar(");
  Log(ct);
  Logln(")");
  Cmd_PS_StoreChar(ct);
  if(!Poll_Response()){
    Logln("StoreChar FAILED");
    return 0;
  }
  
  ct = Enrolled_Count();
  Log("Enrolled Count after enroll:");
  Logln(ct);
//  _debug_flag = false;
  return ct;

}

int FPScanner::Enrolled_Count(void)
{
  _enrolled_count = FPS_TIMEOUT;

  Cmd_PS_ValidTempleteNum();
  if(Poll_Response()){
    _enrolled_count = _ack[11];
  }
  return _enrolled_count;
}

int FPScanner::Scan_for_Match(void)
{
  _matched_fingerprint = FPS_NO_MATCH;
  
  Cmd_PS_GetImage();
  if(Poll_Response()){
    Cmd_PS_GenChar(1);
    if( Poll_Response() ){
      Cmd_PS_Search();
      if( Poll_Response()){
        _matched_fingerprint = _ack[11];
        return _matched_fingerprint;
      }else{
        return FPS_NO_MATCH;
      }
    }else{
      Logln("GenChar FAIL");
      return FPS_NO_MATCH;
    }
  }else{
    return FPS_NO_SCAN;
  }
}

void FPScanner::write_to_scanner( byte msg[], int nbytes )
{
  int i;
  if(_debug_flag){
    for(i=0;i<6;i++){
      Log(ARA_HEADER[i],HEX);
      Log(" ");
    }
    for(i=0;i<nbytes;i++){
      Log(msg[i],HEX);
      Log(" ");
    }
    Logln("");
  }
  _write(ARA_HEADER,6);
  _write(msg,nbytes);

}


int FPScanner::Flush(void)
{
  long ct;
  int i=0;
  byte buf;
  for(ct=0;ct<TIMEOUT_READ_CT;ct++){
    if(_available()){

      ct = 0; //reset the timeout counter if we got something
      _ack[i] = _read();
      i++;
      if(i==BUFFSIZE){
        break;
      }
    }
  }
  return i;
}

int FPScanner::Poll_Response(void)
{
  long ct;
  int i=0;

  _nbytes = -1;
  _ack[9]=255; //set to some non-zero number
  
  for(ct=0;ct<TIMEOUT_READ_CT;ct++){
  if(_available()){
      _ack[i] = _read();

      if(i==8){
        _nbytes = _ack[i] + 8;
      }
      if(i==_nbytes||i==BUFFSIZE)
        break;
      i++;
    }
  }
  
  if(i>=sizeof(_ack)){
    Logln("BUFFER_MAX");
    _ack[9]=255;
  }else{
    if(_debug_flag){
      Log("  ");
      for(i=0;i<=_nbytes;i++){
        Log(_ack[i],HEX);
        Log(" ");
        if(i>0 && i%25==0) 
          Logln("");
      }  
      Log("_nbytes: ");
      Logln(_nbytes);
    }
  }
  
  if(ct==TIMEOUT_READ_CT)
    Logln("TIMEOUT");
    
  return _ack[9]==0; // 0 is a positive acknowledgement
}

void FPScanner::Set_Log_Level(int lvl){
 _debug_flag = lvl>0; 
}
