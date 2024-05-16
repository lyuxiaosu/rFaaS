import sys
import os


def generate_config(func_name, type1_con, type2_con, type1_rps, type2_rps, type1_param, type2_param, func_types=0,input_size=1):
    function = "./examples/lib" + func_name + ".so"
    name = func_name 
    type1 = "1"
    type2 = "2"

    config = []
    config.append(f"--config multi_functions.json")
    config.append("--device-database client_devices.json")
    config.append("--names " + ",".join([name]*(type1_con + type2_con)))
    config.append("--functions " + ",".join([function]*(type1_con + type2_con)))
    if func_types != "0":
        config.append("--connections {}".format(func_types))
  
    if type1_con == 0:
        if type2_rps != "0":
            rps2 = [type2_rps] * type2_con 
            config.append("--rps " + ",".join(rps2))
        parameter2 = [type2_param] * type2_con
        config.append("--req-parameters " + ",".join(parameter2))
        req_type2 = [type2] * type2_con
        config.append("--req-type " + ",".join(req_type2))
    elif type2_con == 0:
        if type1_rps != "0":
            rps1 = [type1_rps] * type1_con
            config.append("--rps " + ",".join(rps1))
        parameter1 = [type1_param] * type1_con
        config.append("--req-parameters " + ",".join(parameter1))
        req_type1 = [type1] * type1_con
        config.append("--req-type " + ",".join(req_type1))
    else:
        if type1_rps != "0" and type2_rps == "0":
            rps1 = [type1_rps] * type1_con
            config.append("--rps " + ",".join(rps1))
        if type2_rps != "0" and type1_rps == "0":
            rps2 = [type2_rps] * type2_con
            config.append("--rps " + ",".join(rps2))
        if type1_rps != "0" and type2_rps != "0":
            rps1 = [type1_rps] * type1_con
            rps2 = [type2_rps] * type2_con 
            config.append("--rps " + ",".join(rps1) + "," + ",".join(rps2)) 
        parameter1 = [type1_param] * type1_con
        parameter2 = [type2_param] * type2_con
        config.append("--req-parameters " + ",".join(parameter1) + "," + ",".join(parameter2))
        req_type1 = [type1] * type1_con
        req_type2 = [type2] * type2_con
        config.append("--req-type " + ",".join(req_type1) + "," + ",".join(req_type2))
    config.append("--executors-database executors_database.json")
    config.append("--test-ms 10000")
    config.append("--input-size {}".format(input_size))
    config.append("--output-size 4")
    config.append(f"--output-stats client.log")
    return "\n".join(config)

import sys

if len(sys.argv) < 8 or len(sys.argv) > 10:
    print("Usage: <func_name> <type1_concurrency> <type2_concurrency> <type1_rps> <type2_rps> <type1_param> <type2_param> [optional_func_type] [optional_input_size]")
    sys.exit(1)

func_name = sys.argv[1]
type1_con = int(sys.argv[2])
type2_con = int(sys.argv[3])
type1_rps = sys.argv[4]
type2_rps = sys.argv[5]
type1_param = sys.argv[6]
type2_param = sys.argv[7]

optional_cons = sys.argv[8] if len(sys.argv) >= 9 else "0"
input_size = sys.argv[9] if len(sys.argv) >= 10 else "1"

config_content = generate_config(func_name, type1_con, type2_con, type1_rps, type2_rps, type1_param, type2_param, optional_cons, input_size)
with open("client_config", "w") as f:
    f.write(config_content)

