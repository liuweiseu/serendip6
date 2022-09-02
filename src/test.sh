#! /bin/bash

hashpipe -p serendip6 -I 1 -o VERS6SW=0.8.0 -o VERS6GW=0.1.0 -o RUNALWYS=1 -o MAXHITS=2048 -o POWTHRSH=40 -o BINDHOST=enp216s0f0 -o BINDPORT=12345 -o GPUDEV=0 -o FASTBEAM=2 -o FASTPOL=0 s6_pktsock_thread s6_gpu_thread s6_output_thread
