#! /bin/bash
# export cuda lib
export PATH=/usr/local/cuda/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/cuda/lib64:$LD_LIBRARY_PATH

# get the data path
# if it's not defined, the default path will be /data01/serendip6_data
DATA_DIR=$S6_DATA_DIR
if [ ! ${DATA_DIR} ]; then 
    DATA_DIR=/data01/serendip6_data
fi
# check if the data dir exist or not
if [ -d ${DATA_DIR} ];then 
    echo "Data Path: "${DATA_DIR}
else
    echo "*************************************************"
    echo "The data directory doesn't exist!"
    echo "Please create it, and then run the script again."
    echo "*************************************************"
    exit 1
fi

# Ready to go!
cd ${DATA_DIR} ; pkill -f "hashpipe -p serendip6" ; /usr/local/bin/s6_init_mro.sh
hashpipe_check_status -k RUNALWYS -I 0 -s 1
hashpipe_check_status -k RUNALWYS -I 1 -s 1
hashpipe_check_status -k IDLE     -I 0 -s 0
hashpipe_check_status -k IDLE     -I 1 -s 0