#!/bin/bash
function usage {
        echo "$0 [total expected rps]"
        exit 1
}

if [ $# != 1 ] ; then
        usage
        exit 1;
fi

rps=$1

chmod 400 ./id_rsa
remote_ip="128.110.219.0"

concurrency=(1 2 6 8 10 12 14 16)

path="/my_mount/rFaaS"
for(( i=0;i<${#concurrency[@]};i++ )) do
	echo "i is $i"
        python3 ./generate_config.py ${concurrency[i]} 0 $(($rps / ${concurrency[i]})) 0 1 1 
	client_log="client-${concurrency[i]}.log"
        sed -i "s/^--output-stats .*/--output-stats $client_log/g" client_config
        mv client_config client_config_exp
	#cpu_log="cpu-${total_throughput}-${throughput_percentage[i]}.log"
	echo "start rfaas server for concurrency ${concurrency[i]} testing..."
        ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "cd $path && ./start_server.sh > /dev/null 2>&1 &" &
	sleep 4
        #echo "start cpu monitoring"
	#ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "$path/start_monitor.sh $cpu_log > /dev/null 2>&1 &" 
	echo "start client..."
        ./start_closeloop_exponential.sh 
	return_value=$?
	if [ "$return_value" -eq 1 ]; then
		i=$((i - 1))
		echo "failure, continue with i=$i"
		continue
	fi
	#ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "$path/stop_monitor.sh"
        ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "cd $path && sudo ./kill_rfaas.sh"
        sleep 4 
done
folder_name="results"
mkdir $folder_name
mv ./client-*.log $folder_name
