add_library(buffer
    lru_replacer.cpp
    buffer_manager.cpp
)

target_include_directories(buffer PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(buffer PUBLIC storage)
