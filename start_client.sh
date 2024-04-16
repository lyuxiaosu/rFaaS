#./benchmarks/parallel_invocations --config benchmark.json --device-database client_devices.json --name empty --functions ./examples/libfunctions.so --executors-database executors_database.json -s 1
./benchmarks/cold_benchmarker $(cat cold_config)
