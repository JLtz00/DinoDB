add_library(query
    database.cpp
    sql_parser.cpp
    tuple.cpp
    seq_scan.cpp
    persistent_table.cpp
    selection.cpp
    projection.cpp
    nested_loop_join.cpp
    external_merge_sort.cpp
    index_scan.cpp
)

target_include_directories(query PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(query PUBLIC index)
