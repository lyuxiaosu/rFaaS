import sys
import os


def generate_config(loop_count):
    loop_count = int(loop_count)
    function = "./examples/libfibonacci.so"
    name = "fibonacci"
    type1 = "1"
    type2 = "2"
    para1 = "1"
    para2 = "32"

    config = []
    config.append(f"--config multi_functions.json")
    config.append("--device-database client_devices.json")
    config.append("--names " + ",".join([name]*loop_count))
    config.append("--functions " + ",".join([function]*loop_count))
    parameter1 = [para1] * (loop_count // 2)
    parameter2 = [para2] * (loop_count // 2)
    config.append("--req-parameters " + ",".join(parameter1) + "," + ",".join(parameter2))
    req_type1 = [type1] * (loop_count // 2)
    req_type2 = [type2] * (loop_count // 2)
    config.append("--req-type " + ",".join(req_type1) + "," + ",".join(req_type2))
    config.append("--executors-database executors_database.json")
    config.append("--test-ms 10000")
    config.append("--input-size 1")
    config.append("--output-size 4")
    config.append(f"--output-stats client.log")
    return "\n".join(config)

if len(sys.argv) != 2:
    print("Usage: <loop_count>")
    sys.exit(1)

loop_count = sys.argv[1]

config_content = generate_config(loop_count)
with open("client_config", "w") as f:
    f.write(config_content)

