add_library(query
    seq_scan.cpp
    selection.cpp
    projection.cpp
    nested_loop_join.cpp
)

target_include_directories(query PUBLIC ${PROJECT_SOURCE_DIR}/include)
