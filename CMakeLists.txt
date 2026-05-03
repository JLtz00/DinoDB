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
# add_subdirectory(src/storage)   # Fase 1
# add_subdirectory(src/buffer)    # Fase 2
# add_subdirectory(src/index)     # Fase 3
# add_subdirectory(src/query)     # Fase 4

# enable_testing()
# add_subdirectory(tests)
