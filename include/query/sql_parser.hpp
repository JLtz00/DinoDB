#pragma once

#include "query/tuple.hpp"
#include <memory>
#include <string>
#include <variant>
#include <vector>

enum class ComparisonOperator {
    equal,
    not_equal,
    less,
    less_equal,
    greater,
    greater_equal
};

struct ComparisonExpression {
    std::string column;
    ComparisonOperator op { ComparisonOperator::equal };
    Value value;
};

struct WhereExpression {
    enum class Kind {
        comparison,
        logical_and,
        logical_or
    };

    Kind kind { Kind::comparison };
    ComparisonExpression comparison;
    std::shared_ptr<WhereExpression> left;
    std::shared_ptr<WhereExpression> right;
};

struct ColumnDefinition {
    std::string name;
    ValueType type { ValueType::integer };
};

struct CreateTableStatement {
    std::string table;
    std::vector<ColumnDefinition> columns;
};

struct InsertStatement {
    std::string table;
    std::vector<Value> values;
};

struct SelectStatement {
    std::string table;
    bool select_all { false };
    std::vector<std::string> columns;
    std::shared_ptr<WhereExpression> where;
};

struct ShowTablesStatement {};

struct DescribeStatement {
    std::string table;
};

using SqlStatement = std::variant<CreateTableStatement,
                                  InsertStatement,
                                  SelectStatement,
                                  ShowTablesStatement,
                                  DescribeStatement>;

class SqlParser {
public:
    static SqlStatement parse(const std::string& sql);
};
