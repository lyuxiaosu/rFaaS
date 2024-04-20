import sys
import os


def generate_config(type1_con, type2_con, type1_rps, type2_rps, type1_param, type2_param):
    function = "./examples/libfibonacci.so"
    name = "fibonacci"
    type1 = "1"
    type2 = "2"

    config = []
    config.append(f"--config multi_functions.json")
    config.append("--device-database client_devices.json")
    config.append("--names " + ",".join([name]*(type1_con + type2_con)))
    config.append("--functions " + ",".join([function]*(type1_con + type2_con)))
  
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
    config.append("--input-size 1")
    config.append("--output-size 4")
    config.append(f"--output-stats client.log")
    return "\n".join(config)

if len(sys.argv) != 7:
    print("Usage: <type1_concurrency> <type2_concurrency> <type1_rps> <type2_rps> <type1_param> <type2_param>")
    sys.exit(1)

type1_con = int(sys.argv[1])
type2_con = int(sys.argv[2])
type1_rps = sys.argv[3]
type2_rps = sys.argv[4]
type1_param = sys.argv[5]
type2_param = sys.argv[6]

config_content = generate_config(type1_con, type2_con, type1_rps, type2_rps, type1_param, type2_param)
with open("client_config", "w") as f:
    f.write(config_content)

