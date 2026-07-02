cmake_minimum_required(VERSION 3.20)
project(MiniSGBD VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(include)

add_subdirectory(src/common)
add_subdirectory(src/storage)
add_subdirectory(src/buffer)
add_subdirectory(src/index)
add_subdirectory(src/query)


add_executable(dinodb_cli src/cli/dinodb_cli.cpp)
target_include_directories(dinodb_cli PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dinodb_cli PRIVATE query index buffer storage)

add_executable(dinodb_demo src/demo/dinodb_demo.cpp)
target_include_directories(dinodb_demo PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dinodb_demo PRIVATE query index buffer storage)

add_executable(bench_scan_vs_index src/bench/bench_scan_vs_index.cpp)
target_include_directories(bench_scan_vs_index PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(bench_scan_vs_index PRIVATE query index buffer storage)

add_executable(bench_buffer_hit_rate src/bench/bench_buffer_hit_rate.cpp)
target_include_directories(bench_buffer_hit_rate PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(bench_buffer_hit_rate PRIVATE buffer storage)

enable_testing()
add_subdirectory(tests)
