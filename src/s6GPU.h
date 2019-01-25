#ifndef _S6GPU_H
#define _S6GPU_H

#include <semaphore.h>

#include <vector_functions.h>
#include <thrust/device_vector.h>
#include <thrust/transform.h>
#include <thrust/copy.h>
#include <thrust/scan.h>
#include <thrust/gather.h>
#include <thrust/binary_search.h>
#include <thrust/device_ptr.h>

// include cub allocator and its required includes
#include <iostream>
#include <sstream>
#include <thrust/system/cuda/vector.h>
#include <cub/util_allocator.cuh>
#include "cub_allocator.hpp"

#include "s6_databuf.h"

typedef struct {
    int  nfft_;             // FFT length
	cufftType fft_type;     // complex to complex or real to complex
    size_t  nbatch;         // number of FFT batches to do (only FFT the utilized chans)      
    int     istride;        // this can effectively transpose the input data (must stride over all (max) chans)
    int     ostride;        // normally no transpose needed on the output
    int     idist;          // distance between 1st input elements of consecutive batches
    int     odist;          // distance between 1st output elements of consecutive batches
} cufft_config_t;

typedef struct {
#ifdef SOURCE_FAST
    thrust::device_vector<float> * fft_data_p;             
    thrust::device_vector<char>  * raw_timeseries_p;       // input time series, correctly ordered

#else	// not SOURCE_FAST
    thrust::device_vector<float2> * fft_data_p;             // 
    thrust::device_vector<char2>  * raw_timeseries_p;       // input time series, correctly ordered

#endif	// ifdef SOURCE_FAST

    thrust::device_vector<float2> * fft_data_out_p;         
    thrust::device_vector<float>  * powspec_p;              // detected power spectra
    thrust::device_vector<float>  * scanned_p;              // mean powers
    thrust::device_vector<float>  * baseline_p;             // running mean by region, using scanned_p
    thrust::device_vector<float>  * normalised_p;           // normalized spectra, using baseline_p.  This is S/N.
    thrust::device_vector<int>    * hit_indices_p;          // indexes subset of normalized, baseline, and powspec for hits that exceed threshold
    thrust::device_vector<float>  * hit_powers_p;           // non-normalized hit powers, reported to caller
    thrust::device_vector<float>  * hit_baselines_p;        // mean power at hit bins, reported to caller. 
                                                            //   hit_power[n]/hit_baseline[n] = S/N
    thrust::device_vector<float>  * spectra_sums_p;
    thrust::device_vector<int>    * spectra_indices_p;
} device_vectors_t;

device_vectors_t * init_device_vectors();

void delete_device_vectors( device_vectors_t * dv_p);

void get_gpu_mem_info(const char * comment);

int init_device(int gpu_dev);

void gpu_fini();

void create_fft_plan_1d(cufftHandle* plan,
                            int          istride,
                            int          idist,
                            int          ostride,
                            int          odist,
                            size_t       nfft_,
                            size_t       nbatch,
							cufftType    fft_type);

int spectroscopy(int n_cc,
                 int n_fc,
                 int n_ts,
                 int n_pol, // either the pol itself or the number of pols
                 int beam,
                 size_t maxhits,
                 size_t maxgpuhits,
                 float power_thresh,
                 float smooth_scale,
                 uint64_t * input_data,
                 size_t n_input_data_bytes,
                 s6_output_block_t *s6_output_block,
				 sem_t * gpu_sem);

#if 0
int spectroscopy_one_pol(int n_subband,
                 int n_chan,
                 int n_input,
                 int beam,
                 size_t maxhits,
                 size_t maxgpuhits,
                 float power_thresh,
                 float smooth_scale,
                 uint64_t * input_data,
                 size_t n_input_data_bytes,
                 s6_output_block_t *s6_output_block,
                 device_vectors_t    *dv_p,
                 cufftHandle *fft_plan);
#endif

#endif  // _S6GPU_TEST_H
