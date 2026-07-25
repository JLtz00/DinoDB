#include "query/database.hpp"
#include "query/operator.hpp"
#include "query/persistent_table.hpp"
#include "query/projection.hpp"
#include "query/selection.hpp"
#include "query/seq_scan.hpp"
#include "storage/disk_manager.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace {

constexpr const char* CATALOG_HEADER = "DINODB_CATALOG_V2";
constexpr const char* LEGACY_CATALOG_HEADER = "DINODB_CATALOG_V1";

std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, separator)) {
        parts.push_back(part);
    }
    return parts;
}

size_t column_index(const TableSchema& schema, const std::string& column) {
    auto found = std::find(schema.columns.begin(), schema.columns.end(), lowercase(column));
    if (found == schema.columns.end()) {
        throw std::runtime_error("columna '" + column + "' no existe en tabla '" + schema.name + "'");
    }
    return static_cast<size_t>(std::distance(schema.columns.begin(), found));
}

ValueType parse_catalog_type(const std::string& text) {
    if (text == "INT") {
        return ValueType::integer;
    }
    if (text == "TEXT") {
        return ValueType::text;
    }
    if (text == "DATE") {
        return ValueType::date;
    }
    if (text == "HOUR") {
        return ValueType::hour;
    }
    throw std::runtime_error("tipo desconocido en catalogo: " + text);
}

bool compare(const Value& left, ComparisonOperator op, const Value& right) {
    switch (op) {
    case ComparisonOperator::equal:
        return left == right;
    case ComparisonOperator::not_equal:
        return left != right;
    case ComparisonOperator::less:
        return left < right;
    case ComparisonOperator::less_equal:
        return left <= right;
    case ComparisonOperator::greater:
        return left > right;
    case ComparisonOperator::greater_equal:
        return left >= right;
    }
    return false;
}

std::function<bool(const Tuple&)> compile_predicate(
    const std::shared_ptr<WhereExpression>& expression,
    const TableSchema& schema) {
    if (!expression) {
        return [](const Tuple&) { return true; };
    }
    if (expression->kind == WhereExpression::Kind::comparison) {
        size_t index = column_index(schema, expression->comparison.column);
        ComparisonOperator op = expression->comparison.op;
        Value value;
        try {
            value = expression->comparison.value.coerce(schema.types.at(index));
        } catch (const std::exception& error) {
            throw std::runtime_error(
                "condicion sobre columna '" + schema.columns.at(index) + "': " + error.what());
        }
        return [index, op, value](const Tuple& tuple) {
            return compare(tuple.value(index), op, value);
        };
    }

    auto left = compile_predicate(expression->left, schema);
    auto right = compile_predicate(expression->right, schema);
    if (expression->kind == WhereExpression::Kind::logical_and) {
        return [left = std::move(left), right = std::move(right)](const Tuple& tuple) {
            return left(tuple) && right(tuple);
        };
    }
    return [left = std::move(left), right = std::move(right)](const Tuple& tuple) {
        return left(tuple) || right(tuple);
    };
}

} // namespace

Database::Database(std::filesystem::path data_directory)
    : data_directory_(std::move(data_directory))
{
    std::filesystem::create_directories(data_directory_);
    load_catalog();
}

QueryResult Database::execute(const std::string& sql) {
    SqlStatement statement = SqlParser::parse(sql);
    return std::visit([this](const auto& value) { return execute(value); }, statement);
}

std::vector<std::string> Database::list_tables() const {
    std::vector<std::string> names;
    names.reserve(schemas_.size());
    for (const auto& entry : schemas_) {
        names.push_back(entry.first);
    }
    return names;
}

TableSchema Database::describe(const std::string& table) const {
    std::string name = lowercase(table);
    auto found = schemas_.find(name);
    if (found == schemas_.end()) {
        throw std::runtime_error("tabla '" + name + "' no existe");
    }
    return found->second;
}

std::filesystem::path Database::catalog_path() const {
    return data_directory_ / "catalog.meta";
}

std::filesystem::path Database::table_path(const std::string& table) const {
    return data_directory_ / (table + ".table.db");
}

void Database::load_catalog() {
    schemas_.clear();
    if (!std::filesystem::exists(catalog_path())) {
        persist_catalog();
        return;
    }

    std::ifstream input(catalog_path());
    if (!input) {
        throw std::runtime_error("no se pudo abrir el catalogo: " + catalog_path().string());
    }

    std::string line;
    if (!std::getline(input, line) ||
        (line != CATALOG_HEADER && line != LEGACY_CATALOG_HEADER)) {
        throw std::runtime_error("catalogo DinoDB invalido o incompatible");
    }
    bool legacy_catalog = line == LEGACY_CATALOG_HEADER;

    size_t line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }
        size_t separator = line.find('|');
        if (separator == std::string::npos) {
            throw std::runtime_error("catalogo corrupto en linea " + std::to_string(line_number));
        }
        TableSchema schema;
        schema.name = line.substr(0, separator);
        std::vector<std::string> encoded_columns = split(line.substr(separator + 1), ',');
        for (const std::string& encoded : encoded_columns) {
            if (legacy_catalog) {
                schema.columns.push_back(encoded);
                schema.types.push_back(ValueType::integer);
                continue;
            }
            size_t type_separator = encoded.find(':');
            if (type_separator == std::string::npos) {
                throw std::runtime_error("catalogo corrupto en linea " + std::to_string(line_number));
            }
            schema.columns.push_back(encoded.substr(0, type_separator));
            schema.types.push_back(parse_catalog_type(encoded.substr(type_separator + 1)));
        }
        if (schema.name.empty() || schema.columns.empty() || schemas_.count(schema.name) != 0) {
            throw std::runtime_error("catalogo corrupto en linea " + std::to_string(line_number));
        }
        schemas_.emplace(schema.name, std::move(schema));
    }
}

void Database::persist_catalog() const {
    std::filesystem::path temporary = catalog_path();
    temporary += ".tmp";

    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            throw std::runtime_error("no se pudo escribir el catalogo");
        }
        output << CATALOG_HEADER << '\n';
        for (const auto& entry : schemas_) {
            output << entry.second.name << '|';
            for (size_t i = 0; i < entry.second.columns.size(); ++i) {
                if (i > 0) {
                    output << ',';
                }
                output << entry.second.columns[i] << ':'
                       << value_type_name(entry.second.types.at(i));
            }
            output << '\n';
        }
        output.flush();
        if (!output) {
            throw std::runtime_error("fallo al persistir el catalogo");
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary, catalog_path(), error);
    if (error) {
        std::filesystem::remove(catalog_path(), error);
        error.clear();
        std::filesystem::rename(temporary, catalog_path(), error);
    }
    if (error) {
        throw std::runtime_error("no se pudo reemplazar el catalogo: " + error.message());
    }
}

QueryResult Database::execute(const CreateTableStatement& statement) {
    if (schemas_.count(statement.table) != 0) {
        throw std::runtime_error("tabla '" + statement.table + "' ya existe");
    }
    if (statement.columns.empty()) {
        throw std::runtime_error("la tabla requiere al menos una columna");
    }

    std::set<std::string> unique_columns;
    std::vector<std::string> column_names;
    std::vector<ValueType> column_types;
    column_names.reserve(statement.columns.size());
    column_types.reserve(statement.columns.size());
    for (const ColumnDefinition& column : statement.columns) {
        if (!unique_columns.insert(column.name).second) {
            throw std::runtime_error("columna duplicada '" + column.name + "'");
        }
        column_names.push_back(column.name);
        column_types.push_back(column.type);
    }
    if (std::filesystem::exists(table_path(statement.table))) {
        throw std::runtime_error("ya existe el archivo fisico de tabla '" + statement.table + "'");
    }

    {
        std::ofstream table_file(table_path(statement.table), std::ios::binary);
        if (!table_file) {
            throw std::runtime_error("no se pudo crear la tabla '" + statement.table + "'");
        }
    }

    schemas_.emplace(statement.table, TableSchema {
        statement.table, std::move(column_names), std::move(column_types)
    });
    try {
        persist_catalog();
    } catch (...) {
        schemas_.erase(statement.table);
        std::error_code ignored;
        std::filesystem::remove(table_path(statement.table), ignored);
        throw;
    }

    QueryResult result;
    result.message = "Tabla '" + statement.table + "' creada";
    return result;
}

QueryResult Database::execute(const InsertStatement& statement) {
    TableSchema schema = describe(statement.table);
    if (statement.values.size() != schema.columns.size()) {
        throw std::runtime_error(
            "INSERT esperaba " + std::to_string(schema.columns.size()) +
            " valores y recibio " + std::to_string(statement.values.size()));
    }

    Tuple tuple;
    tuple.values.reserve(statement.values.size());
    for (size_t i = 0; i < statement.values.size(); ++i) {
        try {
            tuple.values.push_back(statement.values[i].coerce(schema.types.at(i)));
        } catch (const std::exception& error) {
            throw std::runtime_error(
                "valor para columna '" + schema.columns.at(i) + "': " + error.what());
        }
    }

    DiskManager disk(table_path(schema.name).string());
    PersistentTable table(&disk);
    table.insert(tuple);
    disk.flush();

    QueryResult result;
    result.affected_rows = 1;
    result.message = "1 fila insertada en '" + schema.name + "'";
    return result;
}

QueryResult Database::execute(const SelectStatement& statement) {
    TableSchema schema = describe(statement.table);
    std::vector<size_t> projection;
    std::vector<std::string> output_columns;

    if (statement.select_all) {
        output_columns = schema.columns;
        projection.reserve(schema.columns.size());
        for (size_t i = 0; i < schema.columns.size(); ++i) {
            projection.push_back(i);
        }
    } else {
        output_columns = statement.columns;
        projection.reserve(statement.columns.size());
        for (const std::string& column : statement.columns) {
            projection.push_back(column_index(schema, column));
        }
    }

    DiskManager disk(table_path(schema.name).string());
    PersistentTable table(&disk);
    std::unique_ptr<Operator> plan = std::make_unique<SeqScan>(table);
    std::string plan_text = "SeqScan(" + schema.name + ")";

    if (statement.where) {
        plan = std::make_unique<Selection>(
            std::move(plan), compile_predicate(statement.where, schema));
        plan_text += " -> Selection";
    }
    plan = std::make_unique<Projection>(std::move(plan), projection);
    plan_text += " -> Projection";

    QueryResult result;
    result.columns = std::move(output_columns);
    result.plan = std::move(plan_text);
    plan->open();
    try {
        while (auto tuple = plan->next()) {
            result.rows.push_back(std::move(*tuple));
        }
        plan->close();
    } catch (...) {
        plan->close();
        throw;
    }
    result.affected_rows = result.rows.size();
    result.message = std::to_string(result.rows.size()) + " fila(s)";
    return result;
}

QueryResult Database::execute(const ShowTablesStatement&) {
    QueryResult result;
    result.columns = { "table_name" };
    for (const auto& entry : schemas_) {
        result.text_rows.push_back({ entry.first });
    }
    result.affected_rows = schemas_.size();
    result.message = std::to_string(schemas_.size()) + " tabla(s)";
    return result;
}

QueryResult Database::execute(const DescribeStatement& statement) {
    TableSchema schema = describe(statement.table);
    QueryResult result;
    result.columns = { "column", "type" };
    result.affected_rows = schema.columns.size();
    for (size_t i = 0; i < schema.columns.size(); ++i) {
        result.text_rows.push_back({
            schema.columns[i], value_type_name(schema.types.at(i))
        });
    }
    result.message = std::to_string(schema.columns.size()) + " columna(s)";
    return result;
}
