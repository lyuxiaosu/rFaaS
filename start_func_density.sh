ulimit -n 65535
#gdb -ex run --args \
./benchmarks/function_density $(cat client_config_density)
