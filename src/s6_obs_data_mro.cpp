#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <hiredis/hiredis.h>

#include "hashpipe.h"
#include "s6_obs_data_mro.h"

//----------------------------------------------------------
#define MAX_STRING_LENGTH 32  
#define MAX_TOKENS         4
int tokenize_string(char * &pInputString, char * Delimiter, char * pToken[MAX_TOKENS]) {
//----------------------------------------------------------
  int i=0;

  pToken[i] = strtok(pInputString, Delimiter);
  i++;

  while ((pToken[i] = strtok(NULL, Delimiter)) != NULL){
    i++;
	if(i >= MAX_TOKENS) {
		i = -1;
		break;
	}
  }

  return i;
}

//----------------------------------------------------------
int coord_string_to_decimal(char * &coord_string, double * coord_decimal) {
//----------------------------------------------------------
// Takes string of the form DH:MM:SS, including any sign, and
// returns decimal degrees or hours.  DH can be degrees or hours.

	char * pTokens[MAX_TOKENS];
	int rv;

	rv = tokenize_string(coord_string, ":", pTokens);
	if(rv == 3) {
		*coord_decimal = (atof(pTokens[0]) + atof(pTokens[1])/60.0 + atof(pTokens[2])/3600.0);
		rv = 0;
	} else {
        hashpipe_error(__FUNCTION__, "Malformed coordinate string : %s", coord_string);
		rv = 1;
	}

	return(rv);
}

//----------------------------------------------------------
static redisContext * redis_connect(char *hostname, int port) {
//----------------------------------------------------------
    redisContext *c;
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds

    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            hashpipe_error(__FUNCTION__, c->errstr);
            redisFree(c);   // get rid of the in-error context
            c = NULL;       // indicate error to caller (TODO - does redisFree null the context pointer?)
        } else {
            hashpipe_error(__FUNCTION__, "Connection error: can't allocate redis context");
        }
    }

    return(c);

}

//----------------------------------------------------------
static int s6_strcpy(char * dest, char * src, int strsize=MROSTATUS_STRING_SIZE) {
//----------------------------------------------------------

    strncpy(dest, src, strsize);
    if(dest[strsize-1] != '\0') {
        dest[strsize-1] = '\0';
        hashpipe_error(__FUNCTION__, "FAST status string exceeded buffer size of %d, truncated : %s", strsize, dest);
    }
}

//----------------------------------------------------------
static int s6_redis_get(redisContext *c, redisReply ** reply, const char * query) {
//----------------------------------------------------------

    int rv = 0;
    int i;
    char * errstr;


    *reply = (redisReply *)redisCommand(c, query);

    if(*reply == NULL) {
        errstr = c->errstr;
        rv = 1;
    } else if((*reply)->type == REDIS_REPLY_ERROR) {
        errstr = (*reply)->str;
        rv = 1;
    } else if((*reply)->type == REDIS_REPLY_ARRAY) {
        for(i=0; i < (*reply)->elements; i++) {
            if(!(*reply)->element[i]->str) {
                errstr = (char *)"At least one element in the array was empty";
                rv = 1;
                break;
            }
        }
    }
    if(rv) {
        hashpipe_error(__FUNCTION__, "redis query (%s) returned an error : %s", query, errstr);
    }

    return(rv); 
}

//----------------------------------------------------------
int put_obs_mro_info_to_redis(char * fits_filename, mrostatus_t * mrostatus, int instance, char *hostname, int port) {
//----------------------------------------------------------
    redisContext *c;
    redisContext *c_observatory;
    redisReply *reply;
    char key[200];
    char time_str[200];
    char my_hostname[200];
    int rv=0;

	const char * host_observatory = "10.128.8.8";
	int port_observatory = 6379;

    // TODO - sane rv

#if 0
    if(!rv) {
        // update current filename
        // On success, zero is returned.  On error, -1 is returned, and errno is set appropriately.
        rv =  gethostname(my_hostname, sizeof(my_hostname));
        sprintf(key, "FN%s_%02d", my_hostname, instance);
        reply = (redisReply *)redisCommand(c,"SET %s %s", key, fits_filename);
        freeReplyObject(reply);
    }
#endif

    // TODO - possible race condition with FRB proccess
    if(!rv && mrostatus->DUMPVOLT) {
        sprintf(time_str, "%ld", time(NULL));
        reply = (redisReply *)redisCommand(c,"MSET  %s %s %s", "DUMPRAW", time, "0");
        freeReplyObject(reply);
    }

    if(c) redisFree(c);       // TODO do I really want to free each time?

    return(rv);
}

//----------------------------------------------------------
int get_obs_mro_info_from_redis(mrostatus_t * mrostatus,     
                            char    *hostname, 
                            int     port) {
//----------------------------------------------------------

    redisContext *c;
    redisContext *c_observatory;
    redisReply *reply;
    int rv = 0;

    //const char * host_observatory = "10.128.8.8";
    //int port_observatory = 6379;
    //const char * host_observatory = "10.128.1.65";
    //int port_observatory = 8002;
    const char * host_observatory = "172.17.0.2";
    int port_observatory = 6379;
    const char * host_pw = "mro";

    char computehostname[32];
    char query_string[64];

    double mjd_now;  

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds

	// Local instrument DB
    // TODO make c static?
    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            hashpipe_error(__FUNCTION__, c->errstr);
            redisFree(c);
        } else {
            hashpipe_error(__FUNCTION__, "Connection error: can't allocate redis context");
        }
        exit(1);
    }

#if 1
	// Observatory DB
    c_observatory = redisConnectWithTimeout((char *)host_observatory, port_observatory, timeout);
    if (c == NULL || c->err) {
        if (c) {
            hashpipe_error(__FUNCTION__, c->errstr);
            redisFree(c);37824118
        } else {
            hashpipe_error(__FUNCTION__, "Connection error: can't allocate redis context");
        }
        exit(1);
    }
	//rv = s6_redis_get(c_observatory, &reply,"AUTH mro");
#endif

	gethostname(computehostname, sizeof(computehostname));

#if 0
    // ADC RMS's
	sprintf(query_string, "HMGET       ADCRMS_%s       ADCRMSTM ADCRMSP0 ADCRMSP1", computehostname);
    if(!rv && !(rv = s6_redis_get(c, &reply, query_string))) {
        mrostatus->ADCRMSTM = atoi(reply->element[0]->str);
        mrostatus->ADCRMSP0 = atof(reply->element[1]->str);
        mrostatus->ADCRMSP1 = atof(reply->element[2]->str);
        freeReplyObject(reply);
    } 
#endif

#if 0
    // Raw data dump request
    if(!rv && !(rv = s6_redis_get(c, &reply,"HMGET DUMPRAW      DUMPTIME DUMPVOLT"))) {
        mrostatus->DUMPTIME = atoi(reply->element[0]->str);
        mrostatus->DUMPVOLT = atof(reply->element[1]->str);
        freeReplyObject(reply);
    } 
#endif

	// Get observatory data 
	// RA and DEC gathered by name rather than a looped redis query so that all meta data is of a 
	// single point in time
	if(!rv) rv = s6_redis_get(c_observatory, &reply,"hmget ITA_DATA_RESULT_HASH \
                                                    TimeStamp       \
                                                    DUT1            \
                                                    Receiver        \
                                                    Teor_RA         \
                                                    Teor_DEC        \
                                                    Apparent_RA     \
                                                    Apparent_DEC    \
                                                    Cur_RA          \
                                                    Cur_DEC         \
                                                    RA_Rate         \
                                                    DEC_Rate        \
                                                    Sys_Temp        \
                                                    RA_Offset       \
                                                    DEC_Offset      \
                                                    Commanded_Az    \
                                                    Commanded_El    \
                                                    Actual_Az       \
                                                    Actual_El       \
                                                    Az_Error        \
                                                    El_Error        \
                                                    Az_Rate         \
                                                    El_Rate         \
                                                    Sky_Error       \
                                                    Cys_Sec         \
                                                    Receiver_Temp   \
                                                    Atmo_Pressure   \
                                                    Humidity        \
                                                    Receiver_Vac    \
                                                    Epoch");
	/*
    00: TimeStamp       
    01: DUT1            
    02: Receiver        
    03: Teor_RA         
    04: Teor_DEC        
    05: Apparent_RA     
    06: Apparent_DEC    
    07: Cur_RA          
    08: Cur_DEC         
    09: RA_Rate         
    10: DEC_Rate        
    11: Sys_Temp        
    12: RA_Offset       
    13: DEC_Offset      
    14: Commanded_Az    
    15: Commanded_El    
    16: Actual_Az       
    17: Actual_El       
    18: Az_Error        
    19: El_Error        
    20: Az_Rate         
    21: El_Rate         
    22: Sky_Error       
    23: Cys_Sec         
    24: Receiver_Temp   
    25: Atmo_Pressure   
    26: Humidity        
    27: Receiver_Vac    
    28: Epoch
    */
    if(!rv) {
		mrostatus->ADCRMS   = 0;
        mrostatus->ADCRMSTM = 0;
        mrostatus->DUMPVOLT = 0;
        mrostatus->DUMPTIME = 0;
        mrostatus->coarse_chan_id = 0;

        mrostatus->TIME      = atof(reply->element[0]->str)/1000.0;	// observatory gives us millisecs, we record as decimal seconds

        // strip out any parentheses from receiver name
		strncpy(mrostatus->RECEIVER, reply->element[2]->str, MROSTATUS_STRING_SIZE);
		int receiver_name_length;
		receiver_name_length= strlen(mrostatus->RECEIVER);
		mrostatus->RECEIVER[receiver_name_length] = '\0';	

        mrostatus->POINTRA  = atof(reply->element[7]->str);
		mrostatus->POINTDEC = atof(reply->element[8]->str);
        mrostatus->SYS_TEMP = atof(reply->element[4]->str);
        mrostatus->RECEIVER_TEMP = atof(reply->element[5]->str);
        mrostatus->ATMO_PRESSURE = atof(reply->element[6]->str);
        mrostatus->HUMIDITY = atof(reply->element[7]->str);
        mrostatus->EPOCH    = aoof(reply->element[8]->str);
	}
   
    if(c) redisFree(c);       // TODO do I really want to free each time?
    if(c_observatory) redisFree(c_observatory);       // TODO do I really want to free each time?

    return rv;         
}
