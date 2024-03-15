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

   double   ADCRMS;							         // ADC RMS
   time_t   ADCRMSTM;						         // ADC RMS timestamp 
   int      DUMPTIME;
   int      DUMPVOLT;

   int      coarse_chan_id;                     // will always be 0 for MRO (not coarse channelized)
   
   char     SOURCE[MROSTATUS_STRING_SIZE];      // source name
   double   SRA;                                // source ra
   double   SDEC;                               // source dec
   double   RAC;                                // ra commanded
   double   DEC;                                // dec commanded
   double   RAER;                               // ra error
   double   DECER;                              // dec error
   double   AZC;                                // az commanded
   double   ELC;                                // el commanded
   double   AZA;                                // az actual
   double   ELA;                                // el actual
   double   AZER;                               // az error
   double   ELER;                               // el error
   double   RAA;                                // ra in hour
   double   DEA;                                // dec in decimal
   long     ONSOURCE;                           // 1 = on source; 0 = offsource
   char     SITE[MROSTATUS_STRING_SIZE];        // site = "Mc"
   char     RX_CODE[MROSTATUS_STRING_SIZE];     // receiver in use= sxp, xxp, llp, kkc,  etc…   (p=primary focus, C=cassegrain)
   char     YEAR_DOY_UTC[MROSTATUS_STRING_SIZE];// utc year?
   long     YEAR;                               // year
   long     DOY_UTC;                            // day of year
   long     UTC;                                // utc
   double   LO_FREQ;                            // lo frequency
   double   TSYS;                               // system temp ??
   double   XC;                                 // xc
   double   YC;                                 // yc
   double   Z1C;                                // z1c
   double   Z2C;                                // z2c
   double   Z3C;                                // z3c
   double   XA;                                 // xa
   double   YA;                                 // ya
   double   Z1A;                                // z1a
   double   Z2A;                                // z2a
   double   Z3A;                                // z3a
   long     SUBMODE;                            // submode = 0
   char     RX_SUB[MROSTATUS_STRING_SIZE];      // rx sub
   long     SCU_STATUS;                         // scu status 
} mrostatus_t;



int get_obs_mro_info_from_redis(mrostatus_t *mrostatus, char *hostname, int port);
int put_obs_mro_info_to_redis(char * fits_filename, mrostatus_t * mrostatus, int instance, char *hostname, int port);

#endif  // _S6_OBS_DATA_MRO_H

