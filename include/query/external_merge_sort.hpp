#pragma once

#include "query/tuple.hpp"
#include <cstddef>
#include <filesystem>
#include <vector>

class ExternalMergeSort {
public:
    static Table sort(const Table& input, size_t key_column, size_t memory_limit_rows);
    static Table sort(const Table& input,
                      size_t key_column,
                      size_t memory_limit_rows,
                      const std::filesystem::path& temp_dir);
};
