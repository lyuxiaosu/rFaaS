import sys
import json
from collections import defaultdict
import numpy as np


seperate_time_dist = defaultdict(list)
total_seperate_delayed_time_dist = defaultdict(list)
seperate_slow_down = defaultdict(list)
total_time_list = []
total_slow_down = []

def parse_file(file_path):
    #key is request type, value is latency
    fo = open(file_path, "r+")
    total_count = 0
    deadline_miss_count = 0
    for line in fo:
        line = line.strip()
        #latency log line
        if "thread id" not in line and "is" not in line and "sending rate" not in line:
            total_count = total_count + 1
            r_type = line.split(" ")[1]
            latency = line.split(" ")[2]
            pure_cpu_time = line.split(" ")[3]
            total_delayed_latency = pure_cpu_time 
            deadline = float(pure_cpu_time) * 10
            if float(latency) > deadline:
                deadline_miss_count = deadline_miss_count + 1
            #if pure_cpu_time == '0':
            #    continue 
            seperate_time_dist[r_type].append(float(latency))
            total_time_list.append(float(latency))
            total_seperate_delayed_time_dist[r_type].append(float(total_delayed_latency))
            #seperate_slow_down[r_type].append(round((float(latency) / int(pure_cpu_time))))
            #total_slow_down.append(round((float(latency) / int(pure_cpu_time))))

        if "total sending" in line:
            sending_rate = line.split(" ")[3]
            service_rate = line.split(" ")[6]
            print("sending rate:", sending_rate, " service rate:", service_rate)
        if "type" in line and "sending rate" in line:
            r_type = line.split(" ")[1] 
            sending_rate = line.split(" ")[4] 
            service_rate = line.split(" ")[7] 
            print("type ", r_type, " sending rate", sending_rate, " service rate ", service_rate)
    miss_rate = (deadline_miss_count / total_count) * 100
    miss_rate_formatted = "{:.4f}".format(miss_rate)
    print("miss rate ", miss_rate_formatted)
if  __name__ == "__main__":
    argv = sys.argv[1:]
    if len(argv) < 1:
        print("usage ", sys.argv[0], " <file path>")
        sys.exit()
    parse_file(argv[0])
    total_time_array = np.array(total_time_list)
    print("total time array length ", len(total_time_array))
    min_latency = min(total_time_array)
    print("min latency ", min_latency)
    p_1 = np.percentile(total_time_array, 1)
    p_99 = np.percentile(total_time_array, 99)
    p_99_9 = np.percentile(total_time_array, 99.9)
    p_99_99 = np.percentile(total_time_array, 99.99)

    print("99 percentile latency is ", p_99)
    print("99.9 percentile latency is ", p_99_9)
    print("99.99 percentile latency is ", p_99_99)
    print("1 percentile latency is ", p_1)

    #total_slow_down_array = np.array(total_slow_down)
    #p_99 = np.percentile(total_slow_down_array, 99)
    #p_99_9 = np.percentile(total_slow_down_array, 99.9)
    #p_99_99 = np.percentile(total_slow_down_array, 99.99)

    print("99 percentile total slow down is ", p_99)
    print("99.9 percentile total slow down is ", p_99_9)
    print("99.99 percentile total slow down is ", p_99_99)

    for key, value in seperate_time_dist.items():
        seperate_time_array = np.array(value)
        p_99 = np.percentile(seperate_time_array, 99)
        p_99_9 = np.percentile(seperate_time_array, 99.9)
        p_99_99 = np.percentile(seperate_time_array, 99.99)
        print("type ", key, " 99 percentile latency is ", p_99)
        print("type ", key, " 99.9 percentile latency is ", p_99_9)
        print("type ", key, " 99.99 percentile latency is ", p_99_99)

    for key, value in seperate_slow_down.items():
        seperate_slow_down_array = np.array(value)
        p_99 = np.percentile(seperate_slow_down_array, 99)
        p_99_9 = np.percentile(seperate_slow_down_array, 99.9)
        p_99_99 = np.percentile(seperate_slow_down_array, 99.99)
        print("type ", key, " 99 percentile slow down is ", p_99)
        print("type ", key, " 99.9 percentile slow down is ", p_99_9)
        print("type ", key, " 99.99 percentile slow down is ", p_99_99)


    js1 = json.dumps(seperate_time_dist)
    f1 = open("seperate_latency.txt", 'w')
    f1.write(js1)
    f1.close()

    js2 = json.dumps(total_time_list)
    f2 = open("total_latency.txt", 'w')
    f2.write(js2)
    f2.close()

    js4 = json.dumps(total_seperate_delayed_time_dist)
    f4 = open("total_delayed_seperate_latency.txt", 'w')
    f4.write(js4)
    f4.close()
