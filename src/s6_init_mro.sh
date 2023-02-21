#!/bin/bash

VERS6SW=0.8.0                   \
VERS6GW=0.1.0                   \

# Add directory containing this script to PATH
PATH="$(dirname $0):${PATH}"

hostname=`hostname -s`
net_thread="s6_pktsock_thread"
beam=0

#iface_pol0="enp216s0f1"
#iface_pol1="enp216s0f0"
iface_pol0=$1
iface_pol1=$2

# Remove old semaphore
echo removing old semaphore, if any
rm /dev/shm/sem.serendip6_gpu_sem_device_*

# Setup parameters for two instances.
instance_i=("1" "2")
#instance_i=("1")
log_timestamp=`date +%Y%m%d_%H%M%S`
instances=(
  # NOTE: when changing any of the following it is good practice to run:
  # sudo ipcrm -a
  # in order to have initial shared memory allocations occur on the local NUMA node.
  #
  # Production config:
  # run s6 on NUMA node 1 (odd CPUs on FAST compute nodes).  See script config_numa_affinity.sh 
  # for one time (per system boot) setup.
  #
  # fastburst to use: 		numactl --physcpubind=14,16,18 --membind=0
  # heimdall to use: 		CPU 12 and GPU 0
  # and, optionally,
  # second heimdall to use:	CPU  8 and GPU 0
  #
  # hashpipe command line parameters (serendip6 will run as hashpipe instances 1 and 2):
  " place holder for unused instance 0.  fastburst uses instance 0"
  "--physcpubind=16,17,18   --membind=0,1 ${iface_pol0} 0   7  9 11  ${beam} 0  $log_timestamp" # Instance 1
  "--physcpubind=24,25,26 --membind=0,1 ${iface_pol1} 1  24 25 26  ${beam} 1  $log_timestamp" # Instance 2
)

function init() {
  instance=${1}
  numaops=${2}
  membind=${3}
  bindhost=${4}
  gpudev=${5}
  netcpu=${6}
  gpucpu=${7}
  outcpu=${8}
  beam=${9}
  pol=${10}
  log_timestamp=${11}

  if [ -z "${numaops}" ]
  then
    echo "Invalid instance number '${instance}' (ignored)"
    return 1
  fi

  if [ -z "$outcpu" ]
  then
    echo "Invalid configuration for host ${hostname} instance ${instance} (ignored)"
    return 1
  fi

  if [ $net_thread == 's6_pktsock_thread' ]
  then
    echo "binding $net_thread to $bindhost"
  fi

  echo numactl $numaops $membind       \
  hashpipe -p serendip6_mro -I $instance   \
    -o VERS6SW=$VERS6SW                \
    -o VERS6GW=$VERS6GW                \
    -o RUNALWYS=1                      \
    -o MAXHITS=2048                    \
	-o POWTHRSH=40					   \
    -o BINDHOST=$bindhost              \
    -o BINDPORT=12345                  \
    -o GPUDEV=$gpudev                  \
    -o MROBEAM=$beam                  \
    -o MROPOL=$pol                    \
    -c $netcpu $net_thread             \
    -c $gpucpu s6_gpu_thread           \
    -c $outcpu s6_output_thread    

  numactl $numaops $membind            \
  /usr/local/bin/hashpipe -p serendip6_mro -I $instance   \
    -o VERS6SW=$VERS6SW                \
    -o VERS6GW=$VERS6GW                \
    -o RUNALWYS=1                      \
    -o MAXHITS=2048                    \
	-o POWTHRSH=40					   \
    -o BINDHOST=$bindhost              \
    -o BINDPORT=12345                  \
    -o GPUDEV=$gpudev                  \
    -o MROBEAM=$beam                  \
    -o MROPOL=$pol                    \
    -c $netcpu $net_thread             \
    -c $gpucpu s6_gpu_thread           \
    -c $outcpu s6_output_thread        \
     < /dev/null                       \
    1> s6.${hostname}.out.${log_timestamp}.${pol} \
    2> s6.${hostname}.err.${log_timestamp}.${pol} &
}

# Start all instances
for instidx in ${instance_i[@]}
do
  args="${instances[$instidx]}"
  if [ -n "${args}" ]
  then
    echo
    echo Starting instance s6c$mys6cn/$instidx
    init $instidx $args
    echo Instance s6c$mys6cn/$instidx pid $!
  else
    echo Instance $instidx not defined for host $hostname
  fi
done

echo Sleeping to let instances come up...
sleep 10

if [ $net_thread == 's6_pktsock_thread' ]
then
  # Zero out MISSEDPK counts
  for instidx in ${instance_i[@]}
  do
    for key in MISSEDPK NETDRPTL NETPKTTL
    do
      echo Resetting $key count for s6c$mys6cn/$instidx
      hashpipe_check_status -I $instidx -k $key -s 0
    done
  done
else
  # Zero out MISSPKTL counts
  for instidx in ${instance_i[@]}
  do
    echo Resetting MISSPKTL count for s6c$mys6cn/$instidx
    hashpipe_check_status -I $instidx -k MISSPKTL -s 0
  done

  # Release NETHOLD
  for instidx in ${instance_i[@]}
  do
    echo Releasing NETHOLD for s6c$mys6cn/$instidx
    hashpipe_check_status -I $instidx -k NETHOLD -s 0
  done
fi

# test mode
for instidx in ${instance_i[@]}
do
  echo Turning on TESTMODE for $mys6cn/$instidx
  hashpipe_check_status -I $instidx -k TESTMODE -s 0
done

# test mode
for instidx in ${instance_i[@]}
do
  echo Turning on RUNALWYS for $mys6cn/$instidx
  hashpipe_check_status -I RUNALWYS $instidx -k  -s 1
done

