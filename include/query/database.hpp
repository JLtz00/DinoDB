#pragma once

#include "query/sql_parser.hpp"
#include "query/tuple.hpp"
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct TableSchema {
    std::string name;
    std::vector<std::string> columns;
    std::vector<ValueType> types;
};

struct QueryResult {
    std::vector<std::string> columns;
    Table rows;
    std::vector<std::vector<std::string>> text_rows;
    size_t affected_rows { 0 };
    std::string message;
    std::string plan;

    bool has_rows() const { return !columns.empty(); }
};

class Database {
public:
    explicit Database(std::filesystem::path data_directory);

    QueryResult execute(const std::string& sql);
    std::vector<std::string> list_tables() const;
    TableSchema describe(const std::string& table) const;

private:
    std::filesystem::path data_directory_;
    std::map<std::string, TableSchema> schemas_;

    std::filesystem::path catalog_path() const;
    std::filesystem::path table_path(const std::string& table) const;
    void load_catalog();
    void persist_catalog() const;

    QueryResult execute(const CreateTableStatement& statement);
    QueryResult execute(const InsertStatement& statement);
    QueryResult execute(const SelectStatement& statement);
    QueryResult execute(const ShowTablesStatement& statement);
    QueryResult execute(const DescribeStatement& statement);
};
