#!/bin/bash
function usage {
        echo "$0"
        exit 1
}

if [ $# != 0 ] ; then
        usage
        exit 1;
fi

chmod 400 ./id_rsa
remote_ip="128.110.219.0"

worker_count=(1 3 6 9 12 15)

path="/my_mount/rFaaS"
for(( i=0;i<${#worker_count[@]};i++ )) do  

        python3 ./generate_config.py ${worker_count[i]} 0 0 0 1 0
        client_log="client-${worker_count[i]}.log"
        sed -i "s/^--output-stats .*/--output-stats $client_log/g" client_config
        cp client_config throughput_config
	
	#cpu_log="cpu-${total_throughput}-${throughput_percentage[i]}.log"
        echo "start server with worker ${worker_count[i]} testing..."
        ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "cd $path && ./start_server.sh > /dev/null 2>&1 &" &
        sleep 4 
	#echo "start cpu monitoring"
	#ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "$path/start_monitor.sh $cpu_log > /dev/null 2>&1 &"
	echo "start client..."
        ./start_throught_measurement.sh
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
rm -rf $folder_name 
mkdir $folder_name
mv ./client-*.log $folder_name

