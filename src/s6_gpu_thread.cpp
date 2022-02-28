/*
 * s6_gpu_thread.c
 *
 * Performs spectroscopy of incoming data using s6GPU
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sched.h>

#include <cuda.h>
#include <cufft.h>
#include <cuda_runtime_api.h>

#include <s6GPU.h>
#include "hashpipe.h"
#include "s6_databuf.h"
#include "s6GPU.h"
#include "s6_obs_data_fast.h"

#define ELAPSED_NS(start,stop) \
  (((int64_t)stop.tv_sec-start.tv_sec)*1000*1000*1000+(stop.tv_nsec-start.tv_nsec))

int init_gpu_memory(uint64_t num_coarse_chan, cufftHandle *fft_plan_p, int initial) {

    extern cufft_config_t cufft_config;

    struct timespec start, stop;
    
    const char * re[2] = {"re", ""};

    if(num_coarse_chan == 0) {  
        hashpipe_error(__FUNCTION__, "Cannot configure cuFFT with 0 coarse channels");
        return -1;
    }

    fprintf(stderr, "%sconfiguring cuFFT for %ld coarse channels...\n", initial ? re[1] : re[0], num_coarse_chan);

    // Configure cuFFT...
    // The maximum number of coarse channels is one determining factor 
    // of input data buffer size and is set at compile time. At run time 
    // the number of coarse channels can change but this does not affect 
    // the size of the input data buffer.  
    cufft_config.nfft_     = N_TIME_SAMPLES;                                
    cufft_config.ostride   = 1;                                
    cufft_config.idist     = 1;                                
    cufft_config.odist     = cufft_config.nfft_;                        
#ifdef SOURCE_FAST
	fprintf(stderr, "configuring cuFFT for real to complex transforms (cufftType %d)\n", CUFFT_R2C);
	cufft_config.fft_type = CUFFT_R2C;								
    cufft_config.nbatch    = (num_coarse_chan);                             // only FFT the utilized chans      
    cufft_config.istride   = N_COARSE_CHAN / N_SUBSPECTRA_PER_SPECTRUM;     // (must stride over all (max) chans)
#else
	fprintf(stderr, "configuring cuFFT for complex to complex transforms (cufftType %d)\n", cufft_config.fft_type);
	cufft_config.fft_type = CUFFT_C2C;													
    cufft_config.nbatch    = (num_coarse_chan*N_POLS_PER_BEAM);                             // only FFT the utilized chans   
    cufft_config.istride   = N_COARSE_CHAN / N_SUBSPECTRA_PER_SPECTRUM * N_POLS_PER_BEAM;   // must stride over all (max) chans
#endif


    //get_gpu_mem_info("right before creating fft plan");
    //clock_gettime(CLOCK_MONOTONIC, &stop);
    //create_fft_plan_1d(fft_plan_p, cufft_config.istride, cufft_config.idist, cufft_config.ostride, cufft_config.odist, cufft_config.nfft_, cufft_config.nbatch, cufft_config.fft_type);
    //clock_gettime(CLOCK_MONOTONIC, &stop);
    //get_gpu_mem_info("right after creating fft plan");

    //fprintf(stderr, "...done (in %lu nanosec) : nfft : %lu nbatch : %lu istride : %d \n", cufft_config.nfft_, cufft_config.nbatch, cufft_config.istride, ELAPSED_NS(start, stop));

    return 0;
}

static void *run(hashpipe_thread_args_t * args)
{
    // Local aliases to shorten access to args fields
    s6_input_databuf_t *db_in = (s6_input_databuf_t *)args->ibuf;
    s6_output_databuf_t *db_out = (s6_output_databuf_t *)args->obuf;
    hashpipe_status_t st = args->st;
    const char * status_key = args->thread_desc->skey;

#ifdef DEBUG_SEMS
    fprintf(stderr, "s/tid %lu/                      GPU/\n", pthread_self());
#endif

    int rv;
    uint64_t start_mcount, last_mcount=0;
    int s6gpu_error = 0;
    int curblock_in=0;
    int curblock_out=0;
    int error_count = 0, max_error_count = 0;
    float error, max_error = 0.0;

    struct timespec start, stop;
    uint64_t elapsed_gpu_ns  = 0;
    uint64_t gpu_block_count = 0;

#ifdef SOURCE_FAST
    faststatus_t faststatus;
    faststatus_t * faststatus_p = &faststatus;
#endif


#if 0
    // raise this thread to maximum scheduling priority
    struct sched_param SchedParam;
    int retval;
    SchedParam.sched_priority = sched_get_priority_max(SCHED_FIFO);
    fprintf(stderr, "Setting scheduling priority to %d\n", SchedParam.sched_priority);
    retval = sched_setscheduler(0, SCHED_FIFO, &SchedParam);
    if(retval) {
        perror("sched_setscheduler :");
    }
#endif

    // init s6GPU
    int gpu_dev=0;          			// default to 0
    int maxhits = MAXHITS; 				// default
	float power_thresh = POWER_THRESH;	// default
    hashpipe_status_lock_safe(&st);
    hgeti4(st.buf, "GPUDEV", &gpu_dev);
    hgeti4(st.buf, "MAXHITS", &maxhits);
    hgetr4(st.buf, "POWTHRSH", &power_thresh);
    hputr4(st.buf, "POWTHRSH", power_thresh);
    hashpipe_status_unlock_safe(&st);
    init_device(gpu_dev);
	char gpu_sem_name[256];
	sem_t * gpu_sem;
	sprintf(gpu_sem_name, "serendip6_gpu_sem_device_%d", gpu_dev);
	gpu_sem = sem_open(gpu_sem_name, O_CREAT, S_IRWXU, 1);
	
    
    // pin the databufs from cudu's point of view
    cudaHostRegister((void *) db_in, sizeof(s6_input_databuf_t), cudaHostRegisterPortable);
    cudaHostRegister((void *) db_out, sizeof(s6_output_databuf_t), cudaHostRegisterPortable);

    cufftHandle fft_plan;
    cufftHandle *fft_plan_p = &fft_plan;

    uint64_t num_coarse_chan = N_COARSE_CHAN;
    init_gpu_memory(num_coarse_chan/N_SUBSPECTRA_PER_SPECTRUM, fft_plan_p, 1);

    //hashpipe_status_lock_safe(&st);
    //hputr4(st.buf, "POWTHRSH", POWER_THRESH);
    //hashpipe_status_unlock_safe(&st);

    while (run_threads()) {

        hashpipe_status_lock_safe(&st);
        hputi4(st.buf, "GPUBLKIN", curblock_in);
        hputs(st.buf, status_key, "waiting");
        hputi4(st.buf, "GPUBKOUT", curblock_out);
        hashpipe_status_unlock_safe(&st);

        // Wait for new input block to be filled
        while ((rv=hashpipe_databuf_wait_filled((hashpipe_databuf_t *)db_in, curblock_in)) != HASHPIPE_OK) {
            if (rv==HASHPIPE_TIMEOUT) {
                hashpipe_status_lock_safe(&st);
                hputs(st.buf, status_key, "blocked");
                hashpipe_status_unlock_safe(&st);
                continue;
            } else {
                hashpipe_error(__FUNCTION__, "error waiting for filled databuf");
                pthread_exit(NULL);
                break;
            }
        }

        // Got a new data block, update status and determine how to handle it
        hashpipe_status_lock_safe(&st);
        hputu8(st.buf, "GPUMCNT", db_in->block[curblock_in].header.mcnt);
        hashpipe_status_unlock_safe(&st);

#ifdef SOURCE_S6
        if(db_in->block[curblock_in].header.num_coarse_chan != num_coarse_chan) {
            // number of coarse channels has changed!  Redo GPU memory / FFT plan
            num_coarse_chan = db_in->block[curblock_in].header.num_coarse_chan;
            init_gpu_memory(num_coarse_chan/N_SUBSPECTRA_PER_SPECTRUM, fft_plan_p, 0);
        }
#endif

        if(db_in->block[curblock_in].header.mcnt >= last_mcount) {
          // Wait for new output block to be free
          while ((rv=s6_output_databuf_wait_free(db_out, curblock_out)) != HASHPIPE_OK) {
              if (rv==HASHPIPE_TIMEOUT) {
                  hashpipe_status_lock_safe(&st);
                  hputs(st.buf, status_key, "blocked gpu out");
                  hashpipe_status_unlock_safe(&st);
                  continue;
              } else {
                  hashpipe_error(__FUNCTION__, "error waiting for free databuf");
                  pthread_exit(NULL);
                  break;
              }
          }
        }

        // Note processing status
        hashpipe_status_lock_safe(&st);
        hputs(st.buf, status_key, "processing gpu");
        hashpipe_status_unlock_safe(&st);

        clock_gettime(CLOCK_MONOTONIC, &start);

        // pass input metadata to output
        db_out->block[curblock_out].header.mcnt            = db_in->block[curblock_in].header.mcnt;
        db_out->block[curblock_out].header.coarse_chan_id  = db_in->block[curblock_in].header.coarse_chan_id;
        db_out->block[curblock_out].header.num_coarse_chan = db_in->block[curblock_in].header.num_coarse_chan;
        db_out->block[curblock_out].header.time_sec        = db_in->block[curblock_in].header.time_sec;
        db_out->block[curblock_out].header.time_nsec       = db_in->block[curblock_in].header.time_nsec;
        memcpy(&db_out->block[curblock_out].header.missed_pkts, 
               &db_in->block[curblock_in].header.missed_pkts, 
               sizeof(uint64_t) * N_BEAM_SLOTS);
#ifdef SOURCE_FAST
	db_out->block[curblock_out].header.sid = db_in->block[curblock_in].header.sid;
#endif

        // only do spectroscopy if there are more than zero channels!
        size_t total_hits = 0;
        if(num_coarse_chan) {
            // do spectroscopy and hit detection on this block.
            // spectroscopy() writes directly to the output buffer.
#ifdef SOURCE_S6
            // At AO, data are grouped by beam
            int n_bors = N_BEAMS;
            uint64_t n_bytes_per_bors  = N_BYTES_PER_BEAM;
#elif SOURCE_DIBAS
            // At GBT, data are grouped by subspectra
            int n_bors = N_SUBSPECTRA_PER_SPECTRUM;
            uint64_t n_bytes_per_bors  = N_BYTES_PER_SUBSPECTRUM * N_FINE_CHAN;
#elif SOURCE_FAST
#if 1
	    // first get the RMS and determine if data are good
	    char * samples = (char *) &db_in->block[curblock_in].data;
	    double sum_squared, rms;
	    int rms_i;
	    for(rms_i = 0, sum_squared = 0; rms_i < RMS_LENGTH; rms_i++) {
	    	//val = db_in->block[curblock_in].data[rms_i];
        	sum_squared += samples[rms_i] * samples[rms_i];
//fprintf(stderr, "val %d sum_squared %lf\n", (int)samples[rms_i], sum_squared);
	    }
    	    rms = sqrt((double)sum_squared/RMS_LENGTH);
//fprintf(stderr, "rms %lf\n", rms);	
	    //db_out->block[curblock_out].header.voltage_rms = rms;
  	    faststatus_p->ADCRMS = rms;
  	    faststatus_p->ADCRMSTM = time(NULL);
#endif

            // At FAST, data are not grouped
            int n_bors = N_SUBSPECTRA_PER_SPECTRUM;
            uint64_t n_bytes_per_bors  = N_BYTES_PER_SUBSPECTRUM * N_TIME_SAMPLES;
#endif
            for(int bors_i = 0; bors_i < n_bors; bors_i++) {
                size_t nhits = 0; 
                // TODO there is no real c error checking in spectroscopy()
                //      Errors are handled via c++ exceptions
#if 0
fprintf(stderr, "(n_)pol = %lu num_coarse_chan = %lu n_bytes_per_bors = %lu  bors addr = %p\n", 
        db_in->block[curblock_in].header.sid % 2, num_coarse_chan, n_bytes_per_bors, 
        &db_in->block[curblock_in].data[bors_i*n_bytes_per_bors/sizeof(uint64_t)]);
#endif

#ifdef SOURCE_FAST
                nhits = spectroscopy(num_coarse_chan/N_SUBSPECTRA_PER_SPECTRUM,     // n_cc  
                                     N_FINE_CHAN,                                   // n_fc    
                                     N_TIME_SAMPLES,                                // n_ts
                                     db_in->block[curblock_in].header.sid % 2,      // n_pol, the pol itself, one per data strem
                                     bors_i,                                        // bors         
                                     maxhits,                                       // maxhits
                                     MAXGPUHITS,                                    // maxgpuhits
                                     power_thresh,                                  // power_thresh
                                     SMOOTH_SCALE,                                  // smooth_scale
                                     &db_in->block[curblock_in].data[bors_i*n_bytes_per_bors/sizeof(uint64_t)], // input_data   0,1
                                     n_bytes_per_bors,                              // input_data_bytes                         /2
                                     &db_out->block[curblock_out],                  // s6_output_block
                                     gpu_sem);                                      // semaphore to serialize GPU access
#else
                nhits = spectroscopy(num_coarse_chan/N_SUBSPECTRA_PER_SPECTRUM,     // n_cc   
                                     N_FINE_CHAN,                                   // n_fc     
                                     N_TIME_SAMPLES,                                // n_ts     
                                     N_POLS_PER_BEAM,                               // n_pol     
                                     bors_i,                                        // bors         
                                     maxhits,                                       // maxhits
                                     MAXGPUHITS,                                    // maxgpuhits
                                     power_thresh,                                  // power_thresh
                                     SMOOTH_SCALE,                                  // smooth_scale
                                     &db_in->block[curblock_in].data[bors_i*n_bytes_per_bors/sizeof(uint64_t)], // input_data   0,1
                                     n_bytes_per_bors,                              // input_data_bytes                         /2
                                     &db_out->block[curblock_out],                  // s6_output_block
                                     gpu_sem);                                      // semaphore to serialize GPU access
#endif

//fprintf(stderr, "spectroscopy() returned %ld for beam %d\n", nhits, beam_i);
                total_hits += nhits;
                clock_gettime(CLOCK_MONOTONIC, &stop);
                elapsed_gpu_ns += ELAPSED_NS(start, stop);
                gpu_block_count++;

            }  //  for(int beam_i = 0; beam_i < N_BEAMS; beam_i++)
        }  // if(num_coarse_chan)

        hashpipe_status_lock_safe(&st);
       	hputi4(st.buf, "NUMHITS", total_hits);
        hputr4(st.buf, "GPUMXERR", max_error);
        hputi4(st.buf, "GPUERCNT", error_count);
        hputi4(st.buf, "GPUMXECT", max_error_count);
#ifdef SOURCE_FAST
	hputr8(st.buf, "ADCRMS", faststatus_p->ADCRMS);
	hputi8(st.buf, "ADCRMSTM", faststatus_p->ADCRMSTM);
#endif
        hashpipe_status_unlock_safe(&st);

        // Mark output block as full and advance
        s6_output_databuf_set_filled(db_out, curblock_out);
        curblock_out = (curblock_out + 1) % db_out->header.n_block;

        // Mark input block as free and advance
        //memset((void *)&db_in->block[curblock_in], 0, sizeof(s6_input_block_t));     // TODO re-init first
        hashpipe_databuf_set_free((hashpipe_databuf_t *)db_in, curblock_in);
        curblock_in = (curblock_in + 1) % db_in->header.n_block;

        /* Check for cancel */
        pthread_testcancel();
    }

    // Thread success!
    gpu_fini();		// take care of any gpu cleanup, eg profiler flushing
    // unpin the databufs from cudu's point of view
    cudaHostUnregister((void *) db_in);
    cudaHostUnregister((void *) db_out);
	sem_unlink(gpu_sem_name);
    return NULL;
}

static hashpipe_thread_desc_t gpu_thread = {
    name: "s6_gpu_thread",
    skey: "GPUSTAT",
    init: NULL,
    run:  run,
    ibuf_desc: {s6_input_databuf_create},
    obuf_desc: {s6_output_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&gpu_thread);
}
