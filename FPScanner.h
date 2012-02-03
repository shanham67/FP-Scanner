#ifndef FPScanner_h
#define FPScanner_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#define BUFFSIZE 200

class FPScanner
{
   public:
     static const int FPS_ACK=0;
     static const int FPS_NO_ACK=-1;
     static const int FPS_TIMEOUT=255;
     static const int FPS_NO_MATCH=-1;
     static const int FPS_NO_SCAN=254;

     FPScanner( int(*available)(),int (*read)(),void(*write)(void*,int) );

// LOW LEVEL INTERFACE
// Each of these simply send the corresponding command to the scanner.  They do not block
// but return immediately.  They are used by the higher level interface below but exported
// here for others to use as they see fit
     void Cmd_PS_GetImage(void); //send a PS_GetImage command
     void Cmd_PS_GenChar(int buf); //send a PS_GenChar command
     void Cmd_PS_Search(void); //send a PS_Search command
     void Cmd_PS_ReadSysPara(void); //send a PS_ReadSysPara command
     void Cmd_PS_Empty(void);        //send a PS_Empty command to delete all stored fingerprints
     void Cmd_PS_RegModel(void); //send a PS_RegModel command
     void Cmd_PS_StoreChar(int ndx); //send a PS_RegModel command
     void Cmd_PS_ValidTempleteNum(void); //send a PS_ValidTempleteNum(sic) command
      int Poll_Response(void);//tries for a while to get a reponse message, returns true for positive acknowledgement
                              //NOTE: the actual interface returns 0x00 for positive acknowledgement.  This function
                              //      inverts this for cleaner use in if() statements, etc.
     int Flush(void);//read whatever is left on the serial interface
// HIGH LEVEL INTERFACE
     int Scan_for_Match(void);//returns the index of the matched print
     int Matched_Index(void);//returns the index of the last matched fingerprint.  Good until Scan_for_Match called again.
     int Enrolled_Count(void);//returns number of enrolled prints or FPS_TIMEOUT
     int Enroll_New(void);
    void Set_Log_Level(int lvl);

   private:
     void enroll_timeout_reset();
     int  enroll_timeout_check();
     void write_to_scanner( byte msg[], int nbytes );
     byte _ack[BUFFSIZE];
     int _enrolled_count;
     int _nbytes;
     int _matched_fingerprint;
     byte _debug_flag;
     unsigned long _previous_millis;

     int (*_available)();
     int (*_read)();
     void (*_write)(void*,int);
 
};

#endif