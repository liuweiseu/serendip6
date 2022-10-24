#ifndef _S6_OBS_DATA_MRO_H
#define _S6_OBS_DATA_MRO_H

#include "s6_databuf.h"

#define N_ADCS_PER_ROACH2 8     // should this be N_BEAM_SLOTS/2 ?

#define MROSTATUS_STRING_SIZE 32
#define MROSTATUS_BIG_STRING_SIZE 256

#define CURRENT_MJD ((time(NULL) / 86400.0 ) + 40587.0)             // 40587.0 is the MJD of the unix epoch

// idle status reasons bitmap
#define idle_redis_error                   0
#define idle_bad_rms                       1 
//#define idle_redis_error                   0x000000000000001
//#define idle_bad_rms                       0x000000000000002 
//#define idle_placeholder		   0x000000000000004

typedef struct mrostatus {

   double   TIME;		// fractional unix time from observatory redis timestamp
   //double   DUT1;		// current UT1 - UTC difference that is being broadcast by NIST

   char     RECEIVER[MROSTATUS_STRING_SIZE];  

   double   POINTRA; 
   double   POINTDEC;
   
   /*
   double   PHAPOSX;
   double   PHAPOSY;
   double   PHAPOSZ;
   double   ANGLEM;
   */
   
   //double   CLOCKFRQ;

   double   ADCRMS;							// ADC RMS
   time_t   ADCRMSTM;						// ADC RMS timestamp 
   
   int      DUMPTIME;
   int      DUMPVOLT;

   int      coarse_chan_id;                       // will always be 0 for MRO (not coarse channelized)

   double   SYS_TEMP;
   double   RECEIVER_TEMP;
   double   ATMO_PRESSURE;
   double   HUMIDITY;
   double   EPOCH;
} mrostatus_t;



int get_obs_mro_info_from_redis(mrostatus_t *mrostatus, char *hostname, int port);
int put_obs_mro_info_to_redis(char * fits_filename, mrostatus_t * mrostatus, int instance, char *hostname, int port);

#endif  // _S6_OBS_DATA_MRO_H

