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

base_throughput1=300
base_throughput2=300

#throughput_percentage=(1 5 10 15 20 25 30 35 40 41 42 43 44 46 48 50)
throughput_percentage=(52)

path="/my_mount/rFaaS"
#path="/my_mount/old_version/sledge-serverless-framework/runtime/tests"
#path="/my_mount/edf_interrupt/sledge-serverless-framework/runtime/tests"
for(( i=0;i<${#throughput_percentage[@]};i++ )) do
        per_throughput1=$(( (${throughput_percentage[i]} * base_throughput1) / 100 ))
	per_throughput2=$(( (${throughput_percentage[i]} * base_throughput2) / 100 ))
        total_throughput=$((base_throughput1 * 8 * throughput_percentage[i] + base_throughput2 * throughput_percentage[i]))
	total_throughput=$((total_throughput / 100))
        python3 ./generate_config.py "hash" 9 0 $per_throughput1 0 1 0 0 4
	server_log="server-${total_throughput}-${throughput_percentage[i]}.log"
	client_log="client-${total_throughput}-${throughput_percentage[i]}.log"
	cpu_log="cpu-${total_throughput}-${throughput_percentage[i]}.log"
        sed -i "s/^--output-stats .*/--output-stats $client_log/g" client_config
        mv client_config client_config_log_normal 
        echo "start server for ${throughput_percentage[i]} testing..."
	ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip "cd $path && ./start_server.sh > /dev/null 2>&1 &" &
        sleep 4
        echo "start client..."
        ./start_closeloop_log_normal.sh
	#ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "$path/stop_monitor.sh"
        ssh -o stricthostkeychecking=no -i ./id_rsa xiaosuGW@$remote_ip  "cd $path && sudo ./kill_rfaas.sh"
        sleep 4
done
folder_name="results"
rm -rf $folder_name
mkdir $folder_name
mv ./client-*.log $folder_name

