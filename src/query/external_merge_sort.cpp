#include "query/external_merge_sort.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

void write_tuple(std::ofstream& out, const Tuple& tuple) {
    uint32_t columns = static_cast<uint32_t>(tuple.values.size());
    out.write(reinterpret_cast<const char*>(&columns), sizeof(columns));
    for (int32_t value : tuple.values) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }
}

bool read_tuple(std::ifstream& in, Tuple& tuple) {
    uint32_t columns = 0;
    if (!in.read(reinterpret_cast<char*>(&columns), sizeof(columns))) {
        return false;
    }

    tuple.values.assign(columns, 0);
    for (uint32_t i = 0; i < columns; ++i) {
        if (!in.read(reinterpret_cast<char*>(&tuple.values[i]), sizeof(int32_t))) {
            throw std::runtime_error("ExternalMergeSort: run temporal corrupto");
        }
    }
    return true;
}

class RunReader {
public:
    explicit RunReader(const std::filesystem::path& path)
        : path_(path), in_(path, std::ios::binary)
    {
        if (!in_) {
            throw std::runtime_error("ExternalMergeSort: no se pudo abrir run temporal");
        }
        advance();
    }

    ~RunReader() {
        in_.close();
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    bool has_value() const { return current_.has_value(); }
    const Tuple& current() const { return *current_; }

    Tuple pop() {
        Tuple value = *current_;
        advance();
        return value;
    }

private:
    void advance() {
        Tuple tuple;
        if (read_tuple(in_, tuple)) {
            current_ = std::move(tuple);
        } else {
            current_.reset();
        }
    }

    std::filesystem::path path_;
    std::ifstream in_;
    std::optional<Tuple> current_;
};

std::filesystem::path make_run_path(const std::filesystem::path& temp_dir, size_t index) {
    return temp_dir / ("dinodb_sort_run_" + std::to_string(::getpid()) + "_" + std::to_string(index) + ".bin");
}

} // namespace

Table ExternalMergeSort::sort(const Table& input, size_t key_column, size_t memory_limit_rows) {
    return sort(input, key_column, memory_limit_rows, std::filesystem::temp_directory_path());
}

Table ExternalMergeSort::sort(const Table& input,
                              size_t key_column,
                              size_t memory_limit_rows,
                              const std::filesystem::path& temp_dir) {
    if (memory_limit_rows == 0) {
        memory_limit_rows = 1;
    }
    std::filesystem::create_directories(temp_dir);

    std::vector<std::filesystem::path> runs;
    for (size_t i = 0; i < input.size(); i += memory_limit_rows) {
        size_t end = std::min(input.size(), i + memory_limit_rows);
        Table run(input.begin() + static_cast<std::ptrdiff_t>(i), input.begin() + static_cast<std::ptrdiff_t>(end));
        std::sort(run.begin(), run.end(), [key_column](const Tuple& a, const Tuple& b) {
            return a.get(key_column) < b.get(key_column);
        });

        std::filesystem::path path = make_run_path(temp_dir, runs.size());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("ExternalMergeSort: no se pudo crear run temporal");
        }
        for (const Tuple& tuple : run) {
            write_tuple(out, tuple);
        }
        out.close();
        runs.push_back(path);
    }

    std::vector<std::unique_ptr<RunReader>> readers;
    readers.reserve(runs.size());
    for (const auto& run : runs) {
        readers.push_back(std::make_unique<RunReader>(run));
    }

    Table result;
    result.reserve(input.size());
    while (result.size() < input.size()) {
        size_t best = readers.size();
        for (size_t i = 0; i < readers.size(); ++i) {
            if (!readers[i]->has_value()) {
                continue;
            }
            if (best == readers.size() ||
                readers[i]->current().get(key_column) < readers[best]->current().get(key_column)) {
                best = i;
            }
        }
        if (best == readers.size()) {
            throw std::runtime_error("ExternalMergeSort: merge incompleto");
        }
        result.push_back(readers[best]->pop());
    }

    return result;
}
