#include "buffer/buffer_manager.hpp"
#include "index/bplus_tree.hpp"
#include "query/index_scan.hpp"
#include "query/persistent_table.hpp"
#include "query/seq_scan.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>

namespace {

void load_table_and_index(PersistentTable& table, BPlusTree& index, size_t rows) {
    for (size_t i = 0; i < rows; ++i) {
        int32_t key = static_cast<int32_t>((i * 37) % rows);
        RID rid = table.insert(Tuple {{ key, static_cast<int32_t>(i), static_cast<int32_t>(key * 10) }});
        index.insert(key, rid);
    }
}

std::optional<Tuple> sequential_find(const PersistentTable& table, int32_t key) {
    SeqScan scan(table);
    scan.open();
    while (auto row = scan.next()) {
        if (row->get(0) == key) {
            scan.close();
            return row;
        }
    }
    scan.close();
    return std::nullopt;
}

} // namespace

int main() {
    constexpr size_t rows = 10000;
    constexpr int32_t target_key = 7777;
    const std::filesystem::path data_dir = "data";
    const std::filesystem::path table_path = data_dir / "dinodb_demo_table.db";
    const std::filesystem::path index_path = data_dir / "dinodb_demo_index.db";
    std::filesystem::create_directories(data_dir);
    std::filesystem::remove(table_path);
    std::filesystem::remove(index_path);

    DiskManager table_disk(table_path.string());
    DiskManager index_disk(index_path.string());
    PersistentTable table(&table_disk);
    BufferManager buffer(128, &index_disk);
    BPlusTree index(&buffer);
    load_table_and_index(table, index, rows);
    table_disk.flush();
    buffer.flush_all();

    auto scan_start = std::chrono::steady_clock::now();
    auto scan_row = sequential_find(table, target_key);
    auto scan_end = std::chrono::steady_clock::now();

    buffer.reset_metrics();
    auto index_start = std::chrono::steady_clock::now();
    IndexScan index_scan(index, table, target_key);
    index_scan.open();
    auto index_row = index_scan.next();
    index_scan.close();
    auto index_end = std::chrono::steady_clock::now();

    IndexScan range_scan(index, table, 100, 109);
    range_scan.open();
    size_t range_count = 0;
    while (range_scan.next()) {
        ++range_count;
    }
    range_scan.close();

    auto scan_us = std::chrono::duration_cast<std::chrono::microseconds>(scan_end - scan_start).count();
    auto index_us = std::chrono::duration_cast<std::chrono::microseconds>(index_end - index_start).count();

    std::cout << "DinoDB demo final\n";
    std::cout << "Registros cargados: " << rows << "\n";
    std::cout << "Altura B+ Tree: " << index.height() << "\n";
    std::cout << "Busqueda secuencial key=" << target_key << ": "
              << (scan_row ? "encontrado" : "no encontrado") << " en " << scan_us << " us\n";
    std::cout << "Busqueda por indice key=" << target_key << ": "
              << (index_row ? "encontrado" : "no encontrado") << " en " << index_us << " us\n";
    std::cout << "Buffer hits: " << buffer.cache_hits()
              << ", misses: " << buffer.cache_misses()
              << ", hit_rate: " << buffer.hit_rate() << "\n";
    std::cout << "Range IndexScan [100,109]: " << range_count << " tuplas\n";

    return (scan_row && index_row && scan_row->values == index_row->values && range_count == 10) ? 0 : 1;
}
