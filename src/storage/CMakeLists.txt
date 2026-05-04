add_library(storage
    file_writer.cpp
    page.cpp
    disk_manager.cpp
)

target_include_directories(storage PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(storage PUBLIC common)