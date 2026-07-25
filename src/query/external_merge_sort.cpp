#include "query/external_merge_sort.hpp"
#include "query/tuple_codec.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

void write_tuple(std::ofstream& out, const Tuple& tuple) {
    std::string encoded = TupleCodec::serialize(tuple);
    uint32_t length = static_cast<uint32_t>(encoded.size());
    out.write(reinterpret_cast<const char*>(&length), sizeof(length));
    out.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
}

bool read_tuple(std::ifstream& in, Tuple& tuple) {
    uint32_t length = 0;
    if (!in.read(reinterpret_cast<char*>(&length), sizeof(length))) {
        return false;
    }
    std::string encoded(length, '\0');
    if (!in.read(encoded.data(), static_cast<std::streamsize>(length))) {
        throw std::runtime_error("ExternalMergeSort: run temporal corrupto");
    }
    tuple = TupleCodec::deserialize(encoded.data(), encoded.size());
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

struct HeapEntry {
    Tuple tuple;
    size_t reader_index { 0 };
    size_t sequence { 0 };
};

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
            return a.value(key_column) < b.value(key_column);
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

    auto compare = [key_column](const HeapEntry& a, const HeapEntry& b) {
        const Value& a_key = a.tuple.value(key_column);
        const Value& b_key = b.tuple.value(key_column);
        if (a_key != b_key) {
            return a_key > b_key;
        }
        return a.sequence > b.sequence;
    };
    std::priority_queue<HeapEntry, std::vector<HeapEntry>, decltype(compare)> heap(compare);

    size_t sequence = 0;
    for (size_t i = 0; i < readers.size(); ++i) {
        if (readers[i]->has_value()) {
            heap.push(HeapEntry { readers[i]->pop(), i, sequence++ });
        }
    }

    while (!heap.empty()) {
        HeapEntry entry = heap.top();
        heap.pop();
        size_t reader_index = entry.reader_index;
        result.push_back(std::move(entry.tuple));

        if (readers[reader_index]->has_value()) {
            heap.push(HeapEntry { readers[reader_index]->pop(), reader_index, sequence++ });
        }
    }

    if (result.size() != input.size()) {
        throw std::runtime_error("ExternalMergeSort: merge incompleto");
    }

    return result;
}
