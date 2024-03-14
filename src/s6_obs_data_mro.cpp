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
    char colon[]=":";
	rv = tokenize_string(coord_string, colon, pTokens);
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
    return 0;
}

//----------------------------------------------------------
static int s6_redis_get(redisContext *c, redisReply ** reply, const char * query) {
//----------------------------------------------------------

    int rv = 0;
    int i;
    char * errstr;


    *reply = (redisReply *)redisCommand(c,query);

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

	//const char * host_observatory = "10.128.8.8";
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


	gethostname(computehostname, sizeof(computehostname));

	// Get observatory data 
	// RA and DEC gathered by name rather than a looped redis query so that all meta data is of a 
	// single point in time
	if(!rv && !(rv = s6_redis_get(c,&reply,"get source")))  {s6_strcpy(mrostatus->SOURCE,reply->str);       freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get raa")))     {mrostatus->SRA = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get dea")))     {mrostatus->SDEC = atof(reply->str);            freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get rac")))     {s6_strcpy(mrostatus->RAC,reply->str);          freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get dec")))     {s6_strcpy(mrostatus->DEC,reply->str);          freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get raer")))    {s6_strcpy(mrostatus->RAER,reply->str);         freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get decer")))   {s6_strcpy(mrostatus->DECER,reply->str);        freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get azc")))     {mrostatus->AZC = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get elc")))     {mrostatus->ELC = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get aza")))     {mrostatus->AZA = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get ela")))     {mrostatus->ELA = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get azer")))    {mrostatus->AZER= atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get eler")))    {mrostatus->ELER= atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get raa")))     {mrostatus->RAA = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get dea")))     {mrostatus->DEA = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get onsource")))  {mrostatus->ONSOURCE = atol(reply->str);      freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get site")))    {s6_strcpy(mrostatus->SITE,reply->str);         freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get rx_code"))) {s6_strcpy(mrostatus->RX_CODE,reply->str);      freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get year_doy_UTC")))  {s6_strcpy(mrostatus->YEAR_DOY_UTC,reply->str);     freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get year")))    {mrostatus->YEAR = atol(reply->str);            freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get doy")))     {mrostatus->DOY_UTC = atol(reply->str);         freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get UTC")))     {mrostatus->UTC = atol(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get lo_freq"))) {mrostatus->LO_FREQ = atof(reply->str);         freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get tsys")))    {mrostatus->TSYS = atof(reply->str);            freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get xc")))      {mrostatus->XC = atof(reply->str);              freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get yc")))      {mrostatus->YC = atof(reply->str);              freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get z1c")))     {mrostatus->Z1C = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get z2c")))     {mrostatus->Z2C = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get z3c")))     {mrostatus->Z3C = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get xa")))      {mrostatus->XA = atof(reply->str);              freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get ya")))      {mrostatus->YA = atof(reply->str);              freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get z1a")))     {mrostatus->Z1A = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get z2a")))     {mrostatus->Z2A = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get z3a")))     {mrostatus->Z3A = atof(reply->str);             freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get submode"))) {mrostatus->SUBMODE = atol(reply->str);         freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get rx_sub")))  {s6_strcpy(mrostatus->RX_SUB,reply->str);       freeReplyObject(reply);}
    if(!rv && !(rv = s6_redis_get(c,&reply,"get scu_status"))) {mrostatus->SCU_STATUS=atol(reply->str);     freeReplyObject(reply);}

    if(c) redisFree(c);       // TODO do I really want to free each time?

    return rv;         
}
