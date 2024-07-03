#!/bin/bash
function usage {
        echo "$0 [repeat count]"
        exit 1
}

if [ $# != 1 ] ; then
        usage
        exit 1;
fi

chmod 400 ./id_rsa
remote_ip="128.110.219.10"

repeat_count=$1

path="/my_mount/rFaaS"
for(( i=0;i<$repeat_count;i++ )) do
    echo "i is $i"
    echo "start server..."
    ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "cd $path && ./start_server.sh > /dev/null 2>&1 &" &
    sleep 2 
    echo "start client..."
    ./start_multi_func_client.sh 
    ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "cd $path && sudo ./kill_rfaas.sh"
done
