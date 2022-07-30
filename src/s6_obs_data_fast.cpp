#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <hiredis/hiredis.h>

#include "hashpipe.h"
#include "s6_obs_data_fast.h"

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
static int s6_strcpy(char * dest, char * src, int strsize=FASTSTATUS_STRING_SIZE) {
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
int put_obs_fast_info_to_redis(char * fits_filename, faststatus_t * faststatus, int instance, char *hostname, int port) {
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
    if(!rv && faststatus->DUMPVOLT) {
        sprintf(time_str, "%ld", time(NULL));
        reply = (redisReply *)redisCommand(c,"MSET  %s %s %s", "DUMPRAW", time, "0");
        freeReplyObject(reply);
    }

    if(c) redisFree(c);       // TODO do I really want to free each time?

    return(rv);
}

//----------------------------------------------------------
int get_obs_fast_info_from_redis(faststatus_t * faststatus,     
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
    const char * host_observatory = "172.18.0.2";
    int port_observatory = 6379;
    const char * host_pw = "fast";

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
            redisFree(c);
        } else {
            hashpipe_error(__FUNCTION__, "Connection error: can't allocate redis context");
        }
        exit(1);
    }
	rv = s6_redis_get(c_observatory, &reply,"AUTH fast");
#endif

	gethostname(computehostname, sizeof(computehostname));

#if 0
    // ADC RMS's
	sprintf(query_string, "HMGET       ADCRMS_%s       ADCRMSTM ADCRMSP0 ADCRMSP1", computehostname);
    if(!rv && !(rv = s6_redis_get(c, &reply, query_string))) {
        faststatus->ADCRMSTM = atoi(reply->element[0]->str);
        faststatus->ADCRMSP0 = atof(reply->element[1]->str);
        faststatus->ADCRMSP1 = atof(reply->element[2]->str);
        freeReplyObject(reply);
    } 
#endif

#if 0
    // Raw data dump request
    if(!rv && !(rv = s6_redis_get(c, &reply,"HMGET DUMPRAW      DUMPTIME DUMPVOLT"))) {
        faststatus->DUMPTIME = atoi(reply->element[0]->str);
        faststatus->DUMPVOLT = atof(reply->element[1]->str);
        freeReplyObject(reply);
    } 
#endif

	// Get observatory data
	// RA and DEC gathered by name rather than a looped redis query so that all meta data is of a 
	// single point in time
	if(!rv) rv = s6_redis_get(c_observatory, &reply,"hmget KY_ZK_RUN_DATA_RESULT_HASH TimeStamp DUT1 Receiver SDP_PhaPos_X SDP_PhaPos_Y SDP_PhaPos_Z SDP_AngleM SDP_Beam00_RA SDP_Beam00_DEC SDP_Beam01_RA SDP_Beam01_DEC SDP_Beam02_RA SDP_Beam02_DEC SDP_Beam03_RA SDP_Beam03_DEC SDP_Beam04_RA SDP_Beam04_DEC SDP_Beam05_RA SDP_Beam05_DEC SDP_Beam06_RA SDP_Beam06_DEC SDP_Beam07_RA SDP_Beam07_DEC SDP_Beam08_RA SDP_Beam08_DEC SDP_Beam09_RA SDP_Beam09_DEC SDP_Beam10_RA SDP_Beam10_DEC SDP_Beam11_RA SDP_Beam11_DEC SDP_Beam12_RA SDP_Beam12_DEC SDP_Beam13_RA SDP_Beam13_DEC SDP_Beam14_RA SDP_Beam14_DEC SDP_Beam15_RA SDP_Beam15_DEC SDP_Beam16_RA SDP_Beam16_DEC SDP_Beam17_RA SDP_Beam17_DEC SDP_Beam18_RA SDP_Beam18_DEC");
	if(!rv) {
		faststatus->TIME      = atof(reply->element[0]->str)/1000.0;	// observatory gives us millisecs, we record as decimal seconds
		faststatus->DUT1      = atof(reply->element[1]->str);

		char receiver[FASTSTATUS_STRING_SIZE];
		strncpy(receiver, reply->element[2]->str, FASTSTATUS_STRING_SIZE);
		// strip out any parentheses from receiver name
		int i, j, receiver_name_length;
		receiver_name_length = strlen(receiver);
		for(i=0, j=0; i < receiver_name_length; i++, j++) { 
			if(receiver[i] == '(') faststatus->RECEIVER[j] = '_'; 	// replace open paren with _
			else if(receiver[i] == ')') j--;			// replace close paren with nothing
			else faststatus->RECEIVER[j] = receiver[i];		// copy non-paren as is
		}
		faststatus->RECEIVER[j] = '\0';	

		faststatus->PHAPOSX   = atof(reply->element[3]->str);
		faststatus->PHAPOSY   = atof(reply->element[4]->str);
		faststatus->PHAPOSZ   = atof(reply->element[5]->str);
		faststatus->ANGLEM    = atof(reply->element[6]->str);
		// so that we do not have to deal with 38 names!
		int RAi, DECi, beam; 
		for(RAi=7, DECi=8, beam=0; beam<19; RAi += 2, DECi += 2, beam++) {
			faststatus->POINTRA[beam]   = atof(reply->element[RAi]->str);
			faststatus->POINTDEC[beam]  = atof(reply->element[DECi]->str);
//fprintf(stderr, "time %ld %f beam %d i %d ra %f dec %f\n", faststatus->TIME, faststatus->TIMEFRAC, beam, RAi, atof(reply->element[RAi]->str), atof(reply->element[DECi]->str));
		}
		faststatus->CLOCKFRQ = CLOCK_FREQ; 	// hard coded
	}
   
    if(c) redisFree(c);       // TODO do I really want to free each time?
    if(c_observatory) redisFree(c_observatory);       // TODO do I really want to free each time?

    return rv;         
}
