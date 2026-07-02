#include "buffer/buffer_manager.hpp"
#include "index/bplus_tree.hpp"
#include "query/persistent_table.hpp"
#include "query/seq_scan.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>

namespace {

std::optional<Tuple> scan_find(PersistentTable& table, int32_t key) {
    SeqScan scan(table);
    scan.open();
    while (auto tuple = scan.next()) {
        if (tuple->get(0) == key) {
            scan.close();
            return tuple;
        }
    }
    scan.close();
    return std::nullopt;
}

} // namespace

int main(int argc, char** argv) {
    size_t rows = argc > 1 ? static_cast<size_t>(std::stoul(argv[1])) : 10000;
    int32_t target = argc > 2 ? std::stoi(argv[2]) : static_cast<int32_t>(rows * 3 / 4);

    std::filesystem::create_directories("data");
    std::filesystem::path table_path = "data/bench_scan_vs_index_table.db";
    std::filesystem::path index_path = "data/bench_scan_vs_index_index.db";
    std::filesystem::remove(table_path);
    std::filesystem::remove(index_path);

    DiskManager table_disk(table_path.string());
    DiskManager index_disk(index_path.string());
    PersistentTable table(&table_disk);
    BufferManager buffer(128, &index_disk);
    BPlusTree index(&buffer);

    for (size_t i = 0; i < rows; ++i) {
        int32_t key = static_cast<int32_t>(i);
        RID rid = table.insert(Tuple {{ key, static_cast<int32_t>(i * 10) }});
        index.insert(key, rid);
    }
    table_disk.flush();
    buffer.flush_all();

    auto scan_start = std::chrono::steady_clock::now();
    auto scan_tuple = scan_find(table, target);
    auto scan_end = std::chrono::steady_clock::now();

    buffer.reset_metrics();
    auto index_start = std::chrono::steady_clock::now();
    auto rid = index.search(target);
    auto index_tuple = rid ? table.read(*rid) : std::nullopt;
    auto index_end = std::chrono::steady_clock::now();

    auto scan_us = std::chrono::duration_cast<std::chrono::microseconds>(scan_end - scan_start).count();
    auto index_us = std::chrono::duration_cast<std::chrono::microseconds>(index_end - index_start).count();

    std::cout << "benchmark,rows,target,scan_us,index_us,buffer_hits,buffer_misses,hit_rate\n";
    std::cout << "scan_vs_index," << rows << ',' << target << ',' << scan_us << ',' << index_us << ','
              << buffer.cache_hits() << ',' << buffer.cache_misses() << ',' << buffer.hit_rate() << "\n";

    return (scan_tuple && index_tuple && scan_tuple->values == index_tuple->values) ? 0 : 1;
}
