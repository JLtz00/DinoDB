#include "buffer/buffer_manager.hpp"
#include "index/bplus_tree.hpp"
#include "query/index_scan.hpp"
#include "query/persistent_table.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

struct Paths {
    std::filesystem::path dir { "data" };
    std::filesystem::path table_path() const { return dir / "dinodb_cli_table.db"; }
    std::filesystem::path index_path() const { return dir / "dinodb_cli_index.db"; }
};

Paths paths_from_arg(int argc, char** argv, int index) {
    Paths paths;
    if (argc > index) {
        paths.dir = argv[index];
    }
    return paths;
}

int32_t parse_i32(const char* text, const std::string& name) {
    try {
        size_t consumed = 0;
        int value = std::stoi(text, &consumed);
        if (text[consumed] != '\0') {
            throw std::invalid_argument("trailing");
        }
        return static_cast<int32_t>(value);
    } catch (const std::exception&) {
        throw std::runtime_error("argumento invalido para " + name + ": " + text);
    }
}

size_t parse_size(const char* text, const std::string& name) {
    try {
        size_t consumed = 0;
        unsigned long value = std::stoul(text, &consumed);
        if (text[consumed] != '\0') {
            throw std::invalid_argument("trailing");
        }
        return static_cast<size_t>(value);
    } catch (const std::exception&) {
        throw std::runtime_error("argumento invalido para " + name + ": " + text);
    }
}

void print_usage() {
    std::cout << "DinoDB CLI - Semana 13\n"
              << "Demo de Storage + Buffer + B+ Tree persistente\n\n"
              << "Uso:\n"
              << "  dinodb_cli init [data_dir]\n"
              << "  dinodb_cli insert <key> <value> [data_dir]\n"
              << "  dinodb_cli insert-bulk <n> [data_dir]\n"
              << "  dinodb_cli find <key> [data_dir]\n"
              << "  dinodb_cli range <start> <end> [data_dir]\n"
              << "  dinodb_cli stats [data_dir]\n"
              << "  dinodb_cli reopen-check <key> [data_dir]\n\n"
              << "Por defecto usa data/dinodb_cli_table.db y data/dinodb_cli_index.db.\n";
}

void print_tuple(const Tuple& tuple) {
    std::cout << "(";
    for (size_t i = 0; i < tuple.values.size(); ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << tuple.values[i];
    }
    std::cout << ")";
}

void ensure_dir(const Paths& paths) {
    std::filesystem::create_directories(paths.dir);
}

int cmd_init(const Paths& paths) {
    ensure_dir(paths);
    std::filesystem::remove(paths.table_path());
    std::filesystem::remove(paths.index_path());

    DiskManager table_disk(paths.table_path().string());
    DiskManager index_disk(paths.index_path().string());
    BufferManager buffer(64, &index_disk);
    BPlusTree index(&buffer);
    buffer.flush_all();
    table_disk.flush();

    std::cout << "Inicializado DinoDB CLI\n"
              << "table=" << paths.table_path() << "\n"
              << "index=" << paths.index_path() << "\n"
              << "bplus_root=" << index.root_page_id() << " height=" << index.height() << "\n";
    return 0;
}

int cmd_insert(const Paths& paths, int32_t key, int32_t value) {
    ensure_dir(paths);
    DiskManager table_disk(paths.table_path().string());
    DiskManager index_disk(paths.index_path().string());
    PersistentTable table(&table_disk);
    BufferManager buffer(64, &index_disk);
    BPlusTree index(&buffer);

    RID rid = table.insert(Tuple {{ key, value }});
    index.insert(key, rid);
    table_disk.flush();
    buffer.flush_all();

    std::cout << "insert key=" << key << " value=" << value
              << " rid=(page=" << rid.page_id << ", slot=" << rid.slot_id << ")\n";
    return 0;
}

int cmd_insert_bulk(const Paths& paths, size_t count) {
    ensure_dir(paths);
    DiskManager table_disk(paths.table_path().string());
    DiskManager index_disk(paths.index_path().string());
    PersistentTable table(&table_disk);
    BufferManager buffer(128, &index_disk);
    BPlusTree index(&buffer);

    for (size_t i = 0; i < count; ++i) {
        int32_t key = static_cast<int32_t>(i);
        int32_t value = static_cast<int32_t>(i * 10);
        RID rid = table.insert(Tuple {{ key, value }});
        index.insert(key, rid);
    }
    table_disk.flush();
    buffer.flush_all();

    std::cout << "insert-bulk rows=" << count
              << " table_pages=" << table_disk.page_count()
              << " index_pages=" << index_disk.page_count()
              << " bplus_height=" << index.height() << "\n";
    return 0;
}

int cmd_find(const Paths& paths, int32_t key) {
    DiskManager table_disk(paths.table_path().string());
    DiskManager index_disk(paths.index_path().string());
    PersistentTable table(&table_disk);
    BufferManager buffer(64, &index_disk);
    BPlusTree index(&buffer);
    buffer.reset_metrics();

    IndexScan scan(index, table, key);
    scan.open();
    auto tuple = scan.next();
    scan.close();

    auto rid = index.search(key);
    if (!tuple.has_value() || !rid.has_value()) {
        std::cout << "not-found key=" << key << "\n";
        return 1;
    }

    std::cout << "found key=" << key << " rid=(page=" << rid->page_id << ", slot=" << rid->slot_id << ") tuple=";
    print_tuple(*tuple);
    std::cout << " buffer_hits=" << buffer.cache_hits()
              << " buffer_misses=" << buffer.cache_misses()
              << " hit_rate=" << buffer.hit_rate() << "\n";
    return 0;
}

int cmd_range(const Paths& paths, int32_t start, int32_t end) {
    DiskManager table_disk(paths.table_path().string());
    DiskManager index_disk(paths.index_path().string());
    PersistentTable table(&table_disk);
    BufferManager buffer(64, &index_disk);
    BPlusTree index(&buffer);
    buffer.reset_metrics();

    IndexScan scan(index, table, start, end);
    scan.open();
    size_t printed = 0;
    while (auto tuple = scan.next()) {
        std::cout << "row tuple=";
        print_tuple(*tuple);
        std::cout << "\n";
        ++printed;
    }
    scan.close();

    std::cout << "range start=" << start << " end=" << end
              << " rows=" << printed
              << " buffer_hits=" << buffer.cache_hits()
              << " buffer_misses=" << buffer.cache_misses()
              << " hit_rate=" << buffer.hit_rate() << "\n";
    return 0;
}

int cmd_stats(const Paths& paths) {
    DiskManager table_disk(paths.table_path().string());
    DiskManager index_disk(paths.index_path().string());
    PersistentTable table(&table_disk);
    BufferManager buffer(64, &index_disk);
    BPlusTree index(&buffer);

    size_t rows = 0;
    std::optional<int32_t> sample_key;
    PersistentTable::Cursor cursor;
    while (auto tuple = table.next(cursor)) {
        if (!sample_key.has_value() && tuple->size() > 0) {
            sample_key = tuple->get(0);
        }
        ++rows;
    }

    buffer.reset_metrics();
    if (sample_key.has_value()) {
        index.search(*sample_key);
        index.search(*sample_key);
    }

    std::cout << "stats rows=" << rows
              << " table_pages=" << table_disk.page_count()
              << " index_pages=" << index_disk.page_count()
              << " bplus_root=" << index.root_page_id()
              << " bplus_height=" << index.height()
              << " sample_key=" << (sample_key.has_value() ? std::to_string(*sample_key) : "none")
              << " buffer_hits=" << buffer.cache_hits()
              << " buffer_misses=" << buffer.cache_misses()
              << " hit_rate=" << buffer.hit_rate() << "\n";
    return 0;
}

int cmd_reopen_check(const Paths& paths, int32_t key) {
    std::optional<RID> first;
    std::optional<Tuple> first_tuple;
    {
        DiskManager table_disk(paths.table_path().string());
        DiskManager index_disk(paths.index_path().string());
        PersistentTable table(&table_disk);
        BufferManager buffer(16, &index_disk);
        BPlusTree index(&buffer);
        first = index.search(key);
        if (first.has_value()) {
            first_tuple = table.read(*first);
        }
    }

    std::optional<RID> second;
    std::optional<Tuple> second_tuple;
    {
        DiskManager table_disk(paths.table_path().string());
        DiskManager index_disk(paths.index_path().string());
        PersistentTable table(&table_disk);
        BufferManager buffer(16, &index_disk);
        BPlusTree index(&buffer);
        second = index.search(key);
        if (second.has_value()) {
            second_tuple = table.read(*second);
        }
    }

    bool ok = first.has_value() && second.has_value() && first_tuple.has_value() && second_tuple.has_value() &&
              *first == *second && first_tuple->values == second_tuple->values;

    std::cout << "reopen-check key=" << key << " status=" << (ok ? "ok" : "failed");
    if (ok) {
        std::cout << " rid=(page=" << first->page_id << ", slot=" << first->slot_id << ") tuple=";
        print_tuple(*first_tuple);
    }
    std::cout << "\n";
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            print_usage();
            return 1;
        }

        std::string command = argv[1];
        if (command == "help" || command == "--help" || command == "-h") {
            print_usage();
            return 0;
        }
        if (command == "init") {
            return cmd_init(paths_from_arg(argc, argv, 2));
        }
        if (command == "insert") {
            if (argc < 4) {
                print_usage();
                return 1;
            }
            return cmd_insert(paths_from_arg(argc, argv, 4), parse_i32(argv[2], "key"), parse_i32(argv[3], "value"));
        }
        if (command == "insert-bulk") {
            if (argc < 3) {
                print_usage();
                return 1;
            }
            return cmd_insert_bulk(paths_from_arg(argc, argv, 3), parse_size(argv[2], "n"));
        }
        if (command == "find") {
            if (argc < 3) {
                print_usage();
                return 1;
            }
            return cmd_find(paths_from_arg(argc, argv, 3), parse_i32(argv[2], "key"));
        }
        if (command == "range") {
            if (argc < 4) {
                print_usage();
                return 1;
            }
            return cmd_range(paths_from_arg(argc, argv, 4), parse_i32(argv[2], "start"), parse_i32(argv[3], "end"));
        }
        if (command == "stats") {
            return cmd_stats(paths_from_arg(argc, argv, 2));
        }
        if (command == "reopen-check") {
            if (argc < 3) {
                print_usage();
                return 1;
            }
            return cmd_reopen_check(paths_from_arg(argc, argv, 3), parse_i32(argv[2], "key"));
        }

        std::cerr << "Comando desconocido: " << command << "\n";
        print_usage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
