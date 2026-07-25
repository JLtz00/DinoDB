#include "buffer/buffer_manager.hpp"
#include "index/bplus_tree.hpp"
#include "query/database.hpp"
#include "query/index_scan.hpp"
#include "query/persistent_table.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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
    std::cout << "DinoDB CLI - Mini SGBD\n"
              << "SQL sencillo sobre almacenamiento persistente y modelo Volcano\n\n"
              << "Uso:\n"
              << "  dinodb_cli shell [data_dir]\n"
              << "  dinodb_cli sql \"<sentencia>\" [data_dir]\n\n"
              << "Lenguaje:\n"
              << "  CREATE TABLE eventos (id INT, nombre TEXT, fecha DATE, hora HOUR);\n"
              << "  INSERT INTO eventos VALUES (1, 'Demo', '2026-07-24', '14:30');\n"
              << "  SELECT id, nombre FROM eventos WHERE fecha >= '2026-07-01';\n"
              << "  SHOW TABLES;\n"
              << "  DESCRIBE alumnos;\n\n"
              << "Comandos de la demo B+ Tree anterior:\n"
              << "  dinodb_cli init [data_dir]\n"
              << "  dinodb_cli insert <key> <value> [data_dir]\n"
              << "  dinodb_cli insert-bulk <n> [data_dir]\n"
              << "  dinodb_cli find <key> [data_dir]\n"
              << "  dinodb_cli range <start> <end> [data_dir]\n"
              << "  dinodb_cli stats [data_dir]\n"
              << "  dinodb_cli reopen-check <key> [data_dir]\n\n"
              << "Por defecto usa data/dinodb_cli_table.db y data/dinodb_cli_index.db.\n";
}

std::string trim(std::string text) {
    const std::string whitespace = " \t\r\n";
    size_t first = text.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return "";
    }
    size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

std::optional<size_t> sql_terminator(const std::string& sql) {
    bool in_string = false;
    for (size_t i = 0; i < sql.size(); ++i) {
        if (sql[i] == '\'') {
            if (in_string && i + 1 < sql.size() && sql[i + 1] == '\'') {
                ++i;
                continue;
            }
            in_string = !in_string;
            continue;
        }
        if (sql[i] == ';' && !in_string) {
            return i;
        }
    }
    return std::nullopt;
}

void print_query_result(const QueryResult& result) {
    if (result.has_rows()) {
        std::vector<std::vector<std::string>> cells = result.text_rows;
        for (const Tuple& tuple : result.rows) {
            std::vector<std::string> row;
            row.reserve(tuple.size());
            for (const Value& value : tuple.values) {
                row.push_back(value.to_string());
            }
            cells.push_back(std::move(row));
        }

        std::vector<size_t> widths(result.columns.size(), 0);
        for (size_t i = 0; i < result.columns.size(); ++i) {
            widths[i] = result.columns[i].size();
        }
        for (const auto& row : cells) {
            for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
                widths[i] = std::max(widths[i], row[i].size());
            }
        }

        auto separator = [&]() {
            std::cout << '+';
            for (size_t width : widths) {
                std::cout << std::string(width + 2, '-') << '+';
            }
            std::cout << '\n';
        };
        auto print_row = [&](const std::vector<std::string>& row) {
            std::cout << '|';
            for (size_t i = 0; i < widths.size(); ++i) {
                const std::string value = i < row.size() ? row[i] : "";
                std::cout << ' ' << std::left << std::setw(static_cast<int>(widths[i]))
                          << value << " |";
            }
            std::cout << '\n';
        };

        separator();
        print_row(result.columns);
        separator();
        for (const auto& row : cells) {
            print_row(row);
        }
        separator();
    }
    if (!result.plan.empty()) {
        std::cout << "Plan: " << result.plan << '\n';
    }
    if (!result.message.empty()) {
        std::cout << result.message << '\n';
    }
}

int execute_sql(Database& database, const std::string& statement) {
    auto start = std::chrono::steady_clock::now();
    QueryResult result = database.execute(statement);
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start);
    print_query_result(result);
    std::cout << "Tiempo: " << elapsed.count() << " us\n";
    return 0;
}

void print_shell_help() {
    std::cout << "Finalice cada sentencia SQL con ';'.\n"
              << "Comandos: .tables, .schema <tabla>, .help, .exit\n"
              << "Tipos: INT, TEXT, DATE (YYYY-MM-DD), HOUR (HH:MM[:SS])\n"
              << "SQL: CREATE TABLE, INSERT INTO, SELECT, SHOW TABLES, DESCRIBE\n";
}

int cmd_shell(const std::filesystem::path& data_dir) {
    Database database(data_dir);
    std::cout << "DinoDB SQL (" << data_dir << ")\n"
              << "Escriba .help para ayuda; .exit para salir.\n";

    std::string pending;
    std::string line;
    while (true) {
        std::cout << (pending.empty() ? "dinodb> " : "   ...> ") << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }
        std::string command = trim(line);
        if (pending.empty() && !command.empty() && command.front() == '.') {
            if (command == ".exit" || command == ".quit") {
                return 0;
            }
            try {
                if (command == ".help") {
                    print_shell_help();
                } else if (command == ".tables") {
                    execute_sql(database, "SHOW TABLES");
                } else if (command.rfind(".schema ", 0) == 0) {
                    execute_sql(database, "DESCRIBE " + trim(command.substr(8)));
                } else {
                    std::cerr << "Comando desconocido. Use .help\n";
                }
            } catch (const std::exception& ex) {
                std::cerr << "error: " << ex.what() << '\n';
            }
            continue;
        }

        pending += line;
        pending += '\n';
        std::optional<size_t> end;
        while ((end = sql_terminator(pending)).has_value()) {
            std::string statement = trim(pending.substr(0, *end + 1));
            pending.erase(0, *end + 1);
            if (statement.empty() || statement == ";") {
                continue;
            }
            try {
                execute_sql(database, statement);
            } catch (const std::exception& ex) {
                std::cerr << "error: " << ex.what() << '\n';
            }
        }
        pending = trim(pending);
        if (!pending.empty()) {
            pending += '\n';
        }
    }

    if (!trim(pending).empty()) {
        try {
            execute_sql(database, pending);
        } catch (const std::exception& ex) {
            std::cerr << "error: " << ex.what() << '\n';
            return 1;
        }
    }
    return 0;
}

int cmd_sql(const std::string& statement, const std::filesystem::path& data_dir) {
    Database database(data_dir);
    return execute_sql(database, statement);
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
        if (command == "shell") {
            std::filesystem::path data_dir = argc > 2 ? argv[2] : "data";
            return cmd_shell(data_dir);
        }
        if (command == "sql") {
            if (argc < 3) {
                print_usage();
                return 1;
            }
            std::filesystem::path data_dir = argc > 3 ? argv[3] : "data";
            return cmd_sql(argv[2], data_dir);
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
