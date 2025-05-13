#!/bin/bash
function usage {
        echo "$0"
        exit 1
}

if [ $# != 0 ] ; then
        usage
        exit 1;
fi

rps=40000
chmod 400 ./id_rsa
remote_ip="128.110.218.253"

concurrency=(1 10 20 24 28 30 32 40 50 60 64)
#concurrency=(64)

path="/my_mount/rFaaS"
for(( i=0;i<${#concurrency[@]};i++ )) do
	echo "i is $i"
        per_rps=$(($rps / ${concurrency[i]}))
        per_thread_con=1
        python3 ./generate_config.py fibonacci ${concurrency[i]} 0 $per_rps 0 1 0 $per_thread_con
	client_log="client-${concurrency[i]}.log"
        sed -i "s/^--output-stats .*/--output-stats $client_log/g" client_config
        mv client_config client_config_density 
	echo "start rfaas server for concurrency ${concurrency[i]} testing..."
        ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "cd $path && ./start_server.sh > /dev/null 2>&1 &" &
	sleep 4
        #mem_log="${concurrency[i]}_mem.log"
        perf_log="${concurrency[i]}_perf.log"
	
	#ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "$path/run_perf.sh ${concurrency[i]} > /dev/null 2>&1 &"
	ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "$path/run_perf.sh $perf_log > /dev/null 2>&1 &"
        #echo "start cpu monitoring"
	#ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "$path/start_monitor.sh $cpu_log > /dev/null 2>&1 &" 
	echo "start client..."
        ./start_func_density.sh 
	return_value=$?
	if [ "$return_value" -eq 1 ]; then
		i=$((i - 1))
		echo "failure, continue with i=$i"
		continue
	fi
	#ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "$path/stop_monitor.sh"
        ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "cd $path && sudo ./kill_rfaas.sh"
        ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "cd $path && sudo ./kill_perf.sh"
        sleep 4 
done

folder_name="rFaaS"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "mkdir $path/$folder_name"
ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "mv *_perf* $path/$folder_name"

mkdir $folder_name
mv ./client-*.log $folder_name
