ulimit -n 655350 
#PATH=./bin:$PATH gdb -ex run --args \
PATH=./bin:$PATH \
./bin/executor_manager -c executor_manager.json --device-database server_devices.json --skip-resource-manager --max-funcs 100 
