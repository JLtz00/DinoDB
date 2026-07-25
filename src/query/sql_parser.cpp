#include "query/sql_parser.hpp"
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

enum class TokenType {
    identifier,
    number,
    string_literal,
    comma,
    left_paren,
    right_paren,
    star,
    semicolon,
    equal,
    not_equal,
    less,
    less_equal,
    greater,
    greater_equal,
    end
};

struct Token {
    TokenType type { TokenType::end };
    std::string text;
    size_t position { 0 };
};

std::string lowercase(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

class Lexer {
public:
    explicit Lexer(const std::string& input)
        : input_(input)
    {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (position_ < input_.size()) {
            unsigned char current = static_cast<unsigned char>(input_[position_]);
            if (std::isspace(current)) {
                ++position_;
                continue;
            }
            if (std::isalpha(current) || input_[position_] == '_') {
                tokens.push_back(identifier());
                continue;
            }
            if (std::isdigit(current) ||
                (input_[position_] == '-' && position_ + 1 < input_.size() &&
                 std::isdigit(static_cast<unsigned char>(input_[position_ + 1])))) {
                tokens.push_back(number());
                continue;
            }
            if (input_[position_] == '\'') {
                tokens.push_back(string_literal());
                continue;
            }

            size_t token_position = position_;
            char ch = input_[position_++];
            switch (ch) {
            case ',':
                tokens.push_back({ TokenType::comma, ",", token_position });
                break;
            case '(':
                tokens.push_back({ TokenType::left_paren, "(", token_position });
                break;
            case ')':
                tokens.push_back({ TokenType::right_paren, ")", token_position });
                break;
            case '*':
                tokens.push_back({ TokenType::star, "*", token_position });
                break;
            case ';':
                tokens.push_back({ TokenType::semicolon, ";", token_position });
                break;
            case '=':
                tokens.push_back({ TokenType::equal, "=", token_position });
                break;
            case '!':
                if (match('=')) {
                    tokens.push_back({ TokenType::not_equal, "!=", token_position });
                } else {
                    fail(token_position, "se esperaba '=' despues de '!'");
                }
                break;
            case '<':
                if (match('=')) {
                    tokens.push_back({ TokenType::less_equal, "<=", token_position });
                } else if (match('>')) {
                    tokens.push_back({ TokenType::not_equal, "<>", token_position });
                } else {
                    tokens.push_back({ TokenType::less, "<", token_position });
                }
                break;
            case '>':
                if (match('=')) {
                    tokens.push_back({ TokenType::greater_equal, ">=", token_position });
                } else {
                    tokens.push_back({ TokenType::greater, ">", token_position });
                }
                break;
            default:
                fail(token_position, std::string("caracter inesperado '") + ch + "'");
            }
        }
        tokens.push_back({ TokenType::end, "", position_ });
        return tokens;
    }

private:
    Token identifier() {
        size_t start = position_++;
        while (position_ < input_.size()) {
            unsigned char ch = static_cast<unsigned char>(input_[position_]);
            if (!std::isalnum(ch) && input_[position_] != '_') {
                break;
            }
            ++position_;
        }
        return { TokenType::identifier, lowercase(input_.substr(start, position_ - start)), start };
    }

    Token number() {
        size_t start = position_;
        if (input_[position_] == '-') {
            ++position_;
        }
        while (position_ < input_.size() &&
               std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
        return { TokenType::number, input_.substr(start, position_ - start), start };
    }

    Token string_literal() {
        size_t start = position_++;
        std::string value;
        while (position_ < input_.size()) {
            char ch = input_[position_++];
            if (ch != '\'') {
                value.push_back(ch);
                continue;
            }
            if (position_ < input_.size() && input_[position_] == '\'') {
                value.push_back('\'');
                ++position_;
                continue;
            }
            return { TokenType::string_literal, std::move(value), start };
        }
        fail(start, "cadena de texto sin comilla de cierre");
    }

    bool match(char expected) {
        if (position_ >= input_.size() || input_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    [[noreturn]] static void fail(size_t position, const std::string& message) {
        throw std::runtime_error("SQL en posicion " + std::to_string(position + 1) + ": " + message);
    }

    const std::string& input_;
    size_t position_ { 0 };
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens)
        : tokens_(std::move(tokens))
    {}

    SqlStatement parse() {
        SqlStatement statement;
        if (keyword("create")) {
            statement = parse_create();
        } else if (keyword("insert")) {
            statement = parse_insert();
        } else if (keyword("select")) {
            statement = parse_select();
        } else if (keyword("show")) {
            consume_keyword("tables");
            statement = ShowTablesStatement {};
        } else if (keyword("describe") || keyword("desc")) {
            statement = DescribeStatement { consume_identifier("nombre de tabla") };
        } else {
            fail(current(), "se esperaba CREATE, INSERT, SELECT, SHOW o DESCRIBE");
        }

        match(TokenType::semicolon);
        if (current().type != TokenType::end) {
            fail(current(), "texto adicional despues de la sentencia");
        }
        return statement;
    }

private:
    CreateTableStatement parse_create() {
        consume_keyword("table");
        CreateTableStatement statement;
        statement.table = consume_identifier("nombre de tabla");
        consume(TokenType::left_paren, "'('");
        do {
            std::string column = consume_identifier("nombre de columna");
            statement.columns.push_back(ColumnDefinition {
                std::move(column), consume_data_type()
            });
        } while (match(TokenType::comma));
        consume(TokenType::right_paren, "')'");
        return statement;
    }

    InsertStatement parse_insert() {
        consume_keyword("into");
        InsertStatement statement;
        statement.table = consume_identifier("nombre de tabla");
        consume_keyword("values");
        consume(TokenType::left_paren, "'('");
        do {
            statement.values.push_back(consume_literal());
        } while (match(TokenType::comma));
        consume(TokenType::right_paren, "')'");
        return statement;
    }

    SelectStatement parse_select() {
        SelectStatement statement;
        if (match(TokenType::star)) {
            statement.select_all = true;
        } else {
            do {
                statement.columns.push_back(consume_identifier("nombre de columna"));
            } while (match(TokenType::comma));
        }
        consume_keyword("from");
        statement.table = consume_identifier("nombre de tabla");
        if (keyword("where")) {
            statement.where = parse_or();
        }
        return statement;
    }

    std::shared_ptr<WhereExpression> parse_or() {
        auto expression = parse_and();
        while (keyword("or")) {
            expression = logical(WhereExpression::Kind::logical_or, expression, parse_and());
        }
        return expression;
    }

    std::shared_ptr<WhereExpression> parse_and() {
        auto expression = parse_primary();
        while (keyword("and")) {
            expression = logical(WhereExpression::Kind::logical_and, expression, parse_primary());
        }
        return expression;
    }

    std::shared_ptr<WhereExpression> parse_primary() {
        if (match(TokenType::left_paren)) {
            auto expression = parse_or();
            consume(TokenType::right_paren, "')'");
            return expression;
        }

        auto expression = std::make_shared<WhereExpression>();
        expression->kind = WhereExpression::Kind::comparison;
        expression->comparison.column = consume_identifier("columna de la condicion");
        expression->comparison.op = consume_comparison();
        expression->comparison.value = consume_literal();
        return expression;
    }

    static std::shared_ptr<WhereExpression> logical(
        WhereExpression::Kind kind,
        std::shared_ptr<WhereExpression> left,
        std::shared_ptr<WhereExpression> right) {
        auto expression = std::make_shared<WhereExpression>();
        expression->kind = kind;
        expression->left = std::move(left);
        expression->right = std::move(right);
        return expression;
    }

    ComparisonOperator consume_comparison() {
        TokenType type = current().type;
        ++cursor_;
        switch (type) {
        case TokenType::equal:
            return ComparisonOperator::equal;
        case TokenType::not_equal:
            return ComparisonOperator::not_equal;
        case TokenType::less:
            return ComparisonOperator::less;
        case TokenType::less_equal:
            return ComparisonOperator::less_equal;
        case TokenType::greater:
            return ComparisonOperator::greater;
        case TokenType::greater_equal:
            return ComparisonOperator::greater_equal;
        default:
            --cursor_;
            fail(current(), "se esperaba un comparador (=, !=, <, <=, > o >=)");
        }
    }

    int32_t consume_number() {
        Token token = consume(TokenType::number, "un entero");
        try {
            size_t consumed = 0;
            long long value = std::stoll(token.text, &consumed);
            if (consumed != token.text.size() ||
                value < std::numeric_limits<int32_t>::min() ||
                value > std::numeric_limits<int32_t>::max()) {
                throw std::out_of_range("int32");
            }
            return static_cast<int32_t>(value);
        } catch (const std::exception&) {
            fail(token, "entero fuera de rango: " + token.text);
        }
    }

    Value consume_literal() {
        if (current().type == TokenType::number) {
            return Value(consume_number());
        }
        if (current().type == TokenType::string_literal) {
            return Value::text(consume(TokenType::string_literal, "una cadena").text);
        }
        if (keyword("date")) {
            Token token = consume(TokenType::string_literal, "una fecha entre comillas");
            try {
                return Value::date(token.text);
            } catch (const std::exception& error) {
                fail(token, error.what());
            }
        }
        if (keyword("hour") || keyword("time")) {
            Token token = consume(TokenType::string_literal, "una hora entre comillas");
            try {
                return Value::hour(token.text);
            } catch (const std::exception& error) {
                fail(token, error.what());
            }
        }
        fail(current(), "se esperaba un entero o un valor entre comillas");
    }

    ValueType consume_data_type() {
        if (keyword("int") || keyword("integer")) {
            return ValueType::integer;
        }
        if (keyword("text")) {
            return ValueType::text;
        }
        if (keyword("date")) {
            return ValueType::date;
        }
        if (keyword("hour") || keyword("time")) {
            return ValueType::hour;
        }
        fail(current(), "se esperaba un tipo INT, TEXT, DATE o HOUR");
    }

    std::string consume_identifier(const std::string& expected) {
        return consume(TokenType::identifier, expected).text;
    }

    bool keyword(const std::string& expected) {
        if (current().type == TokenType::identifier && current().text == expected) {
            ++cursor_;
            return true;
        }
        return false;
    }

    void consume_keyword(const std::string& expected) {
        if (!keyword(expected)) {
            fail(current(), "se esperaba " + expected);
        }
    }

    bool match(TokenType type) {
        if (current().type != type) {
            return false;
        }
        ++cursor_;
        return true;
    }

    Token consume(TokenType type, const std::string& expected) {
        if (current().type != type) {
            fail(current(), "se esperaba " + expected);
        }
        return tokens_[cursor_++];
    }

    const Token& current() const {
        return tokens_[cursor_];
    }

    [[noreturn]] static void fail(const Token& token, const std::string& message) {
        throw std::runtime_error("SQL en posicion " + std::to_string(token.position + 1) + ": " + message);
    }

    std::vector<Token> tokens_;
    size_t cursor_ { 0 };
};

} // namespace

SqlStatement SqlParser::parse(const std::string& sql) {
    return Parser(Lexer(sql).tokenize()).parse();
}
