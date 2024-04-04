
add_library(benchmarks STATIC benchmarks/settings.cpp)
target_include_directories(benchmarks PRIVATE $<TARGET_PROPERTY:rfaaslib,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(benchmarks PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(benchmarks INTERFACE rfaaslib)

add_executable(warm_benchmarker benchmarks/warm_benchmark.cpp benchmarks/warm_benchmark_opts.cpp)
add_executable(parallel_invocations benchmarks/parallel_invocations.cpp benchmarks/parallel_invocations_opts.cpp)
add_executable(cold_benchmarker benchmarks/cold_benchmark.cpp benchmarks/cold_benchmark_opts.cpp)
add_executable(cpp_interface benchmarks/cpp_interface.cpp benchmarks/cpp_interface_opts.cpp)
add_executable(multi_functions benchmarks/multi_functions.cpp benchmarks/multi_functions_opts.cpp)
add_executable(high_concurrency benchmarks/high_concurrency.cpp benchmarks/high_concurrency_opts.cpp)
set(tests_targets "warm_benchmarker" "cold_benchmarker" "parallel_invocations" "cpp_interface" "multi_functions" "high_concurrency")
foreach(target ${tests_targets})
  add_dependencies(${target} cxxopts::cxxopts)
  add_dependencies(${target} rdmalib)
  add_dependencies(${target} rfaaslib)
  add_dependencies(${target} benchmarks)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rdmalib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rfaaslib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
  target_link_libraries(${target} PRIVATE spdlog::spdlog)
  target_link_libraries(${target} PRIVATE rfaaslib)
  target_link_libraries(${target} PRIVATE benchmarks)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY benchmarks)
endforeach()
