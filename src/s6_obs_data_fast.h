#ifndef _S6_OBS_DATA_FAST_H
#define _S6_OBS_DATA_FAST_H

#include "s6_databuf.h"

#define N_ADCS_PER_ROACH2 8     // should this be N_BEAM_SLOTS/2 ?

#define FASTSTATUS_STRING_SIZE 32
#define FASTSTATUS_BIG_STRING_SIZE 256

#define CURRENT_MJD ((time(NULL) / 86400.0 ) + 40587.0)             // 40587.0 is the MJD of the unix epoch

// idle status reasons bitmap
#define idle_redis_error                   0
#define idle_bad_rms                       1 
#define idle_meta_data_stale               2 
//#define idle_redis_error                   0x000000000000001
//#define idle_bad_rms                       0x000000000000002 
//#define idle_placeholder		   0x000000000000004

typedef struct faststatus {

   double   TIME;		// fractional unix time from observatory redis timestamp
   double   DUT1;		// current UT1 - UTC difference that is being broadcast by NIST
   double   TIME_LAG;		// diff, in secs, between current system time and faststatus time

   char     RECEIVER[FASTSTATUS_STRING_SIZE];  

   double   PHAPOSX;
   double   PHAPOSY;
   double   PHAPOSZ;
   double   ANGLEM;

   double   POINTRA[19]; 
   double   POINTDEC[19];

   double   CLOCKFRQ;

   double   ADCRMS;							// ADC RMS
   time_t   ADCRMSTM;							// ADC RMS timestamp 
   //double   ADCRMSP0;							// polarization 0
   //double   ADCRMSP1;							// polarization 1

   int      DUMPTIME;
   int      DUMPVOLT;

   int       coarse_chan_id;                       // will always be 0 for FAST (not coarse channelized)
} faststatus_t;

int get_obs_fast_info_from_redis(faststatus_t *faststatus, char *hostname, int port);
int put_obs_fast_info_to_redis(char * fits_filename, faststatus_t * faststatus, int instance, char *hostname, int port);

#endif  // _S6_OBS_DATA_FAST_H

