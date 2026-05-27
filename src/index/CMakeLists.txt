add_library(index
    bplus_tree.cpp
)

target_include_directories(index PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(index PUBLIC buffer)
