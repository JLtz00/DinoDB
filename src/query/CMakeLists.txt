add_library(query
    seq_scan.cpp
    selection.cpp
    projection.cpp
)

target_include_directories(query PUBLIC ${PROJECT_SOURCE_DIR}/include)
