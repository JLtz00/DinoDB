#include "query/tuple.hpp"
#include "query/tuple_codec.hpp"
#include <cstring>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace {

constexpr uint32_t TUPLE_MAGIC = 0x31565444; // "DTV1" en little endian

bool digits(const std::string& value, size_t begin, size_t count) {
    if (begin + count > value.size()) {
        return false;
    }
    for (size_t i = begin; i < begin + count; ++i) {
        if (value[i] < '0' || value[i] > '9') {
            return false;
        }
    }
    return true;
}

int parse_part(const std::string& value, size_t begin, size_t count) {
    return std::stoi(value.substr(begin, count));
}

bool leap_year(int year) {
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

int days_in_month(int year, int month) {
    static constexpr int DAYS[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && leap_year(year)) {
        return 29;
    }
    return DAYS[month - 1];
}

DateValue parse_date(const std::string& value) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-' ||
        !digits(value, 0, 4) || !digits(value, 5, 2) || !digits(value, 8, 2)) {
        throw std::runtime_error("DATE invalido '" + value + "'; use YYYY-MM-DD");
    }
    int year = parse_part(value, 0, 4);
    int month = parse_part(value, 5, 2);
    int day = parse_part(value, 8, 2);
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > days_in_month(year, month)) {
        throw std::runtime_error("DATE fuera de rango '" + value + "'");
    }
    return DateValue { year * 10000 + month * 100 + day };
}

HourValue parse_hour(const std::string& value) {
    bool has_seconds = value.size() == 8;
    if ((value.size() != 5 && !has_seconds) || value[2] != ':' ||
        (has_seconds && value[5] != ':') || !digits(value, 0, 2) ||
        !digits(value, 3, 2) || (has_seconds && !digits(value, 6, 2))) {
        throw std::runtime_error("HOUR invalido '" + value + "'; use HH:MM o HH:MM:SS");
    }
    int hour = parse_part(value, 0, 2);
    int minute = parse_part(value, 3, 2);
    int second = has_seconds ? parse_part(value, 6, 2) : 0;
    if (hour > 23 || minute > 59 || second > 59) {
        throw std::runtime_error("HOUR fuera de rango '" + value + "'");
    }
    return HourValue { hour * 3600 + minute * 60 + second };
}

std::string format_date(int32_t encoded) {
    int year = encoded / 10000;
    int month = (encoded / 100) % 100;
    int day = encoded % 100;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << year << '-'
           << std::setw(2) << month << '-' << std::setw(2) << day;
    return output.str();
}

std::string format_hour(int32_t seconds) {
    int hour = seconds / 3600;
    int minute = (seconds % 3600) / 60;
    int second = seconds % 60;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << hour << ':'
           << std::setw(2) << minute << ':' << std::setw(2) << second;
    return output.str();
}

template <typename T>
void append_binary(std::string& output, T value) {
    static_assert(std::is_trivially_copyable<T>::value, "tipo binario requerido");
    const char* bytes = reinterpret_cast<const char*>(&value);
    output.append(bytes, sizeof(T));
}

template <typename T>
T read_binary(const char* data, size_t length, size_t& cursor) {
    static_assert(std::is_trivially_copyable<T>::value, "tipo binario requerido");
    if (cursor + sizeof(T) > length) {
        throw std::runtime_error("TupleCodec: registro truncado");
    }
    T value {};
    std::memcpy(&value, data + cursor, sizeof(T));
    cursor += sizeof(T);
    return value;
}

} // namespace

const char* value_type_name(ValueType type) {
    switch (type) {
    case ValueType::integer:
        return "INT";
    case ValueType::text:
        return "TEXT";
    case ValueType::date:
        return "DATE";
    case ValueType::hour:
        return "HOUR";
    }
    return "UNKNOWN";
}

Value::Value()
    : storage_(int32_t { 0 })
{}

Value::Value(int32_t value)
    : storage_(value)
{}

Value::Value(Storage storage)
    : storage_(std::move(storage))
{}

Value Value::text(std::string value) {
    return Value(Storage { std::move(value) });
}

Value Value::date(const std::string& value) {
    return Value(Storage { parse_date(value) });
}

Value Value::hour(const std::string& value) {
    return Value(Storage { parse_hour(value) });
}

Value Value::from_date_encoded(int32_t value) {
    std::string formatted = format_date(value);
    return date(formatted);
}

Value Value::from_hour_seconds(int32_t value) {
    if (value < 0 || value >= 24 * 3600) {
        throw std::runtime_error("TupleCodec: HOUR almacenado fuera de rango");
    }
    return Value(Storage { HourValue { value } });
}

ValueType Value::type() const {
    switch (storage_.index()) {
    case 0:
        return ValueType::integer;
    case 1:
        return ValueType::text;
    case 2:
        return ValueType::date;
    case 3:
        return ValueType::hour;
    default:
        throw std::runtime_error("Value: tipo interno invalido");
    }
}

int32_t Value::as_int() const {
    if (const auto* value = std::get_if<int32_t>(&storage_)) {
        return *value;
    }
    throw std::runtime_error(
        std::string("se esperaba INT y se obtuvo ") + value_type_name(type()));
}

const std::string& Value::as_text() const {
    if (const auto* value = std::get_if<std::string>(&storage_)) {
        return *value;
    }
    throw std::runtime_error(
        std::string("se esperaba TEXT y se obtuvo ") + value_type_name(type()));
}

int32_t Value::date_encoded() const {
    if (const auto* value = std::get_if<DateValue>(&storage_)) {
        return value->encoded;
    }
    throw std::runtime_error("se esperaba DATE");
}

int32_t Value::hour_seconds() const {
    if (const auto* value = std::get_if<HourValue>(&storage_)) {
        return value->seconds;
    }
    throw std::runtime_error("se esperaba HOUR");
}

std::string Value::to_string() const {
    switch (type()) {
    case ValueType::integer:
        return std::to_string(as_int());
    case ValueType::text:
        return as_text();
    case ValueType::date:
        return format_date(date_encoded());
    case ValueType::hour:
        return format_hour(hour_seconds());
    }
    throw std::runtime_error("Value: tipo no imprimible");
}

Value Value::coerce(ValueType target) const {
    if (type() == target) {
        return *this;
    }
    if (type() == ValueType::text && target == ValueType::date) {
        return date(as_text());
    }
    if (type() == ValueType::text && target == ValueType::hour) {
        return hour(as_text());
    }
    throw std::runtime_error(
        std::string("no se puede convertir ") + value_type_name(type()) +
        " a " + value_type_name(target));
}

int Value::compare(const Value& other) const {
    if (type() != other.type()) {
        throw std::runtime_error(
            std::string("no se pueden comparar ") + value_type_name(type()) +
            " y " + value_type_name(other.type()));
    }
    if (*this == other) {
        return 0;
    }
    switch (type()) {
    case ValueType::integer:
        return as_int() < other.as_int() ? -1 : 1;
    case ValueType::text:
        return as_text() < other.as_text() ? -1 : 1;
    case ValueType::date:
        return date_encoded() < other.date_encoded() ? -1 : 1;
    case ValueType::hour:
        return hour_seconds() < other.hour_seconds() ? -1 : 1;
    }
    return 0;
}

bool Value::operator==(const Value& other) const {
    return storage_ == other.storage_;
}

std::ostream& operator<<(std::ostream& output, const Value& value) {
    return output << value.to_string();
}

bool operator==(const ValueList& left, const std::vector<int32_t>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t i = 0; i < right.size(); ++i) {
        if (left[i].type() != ValueType::integer || left[i].as_int() != right[i]) {
            return false;
        }
    }
    return true;
}

bool operator==(const std::vector<int32_t>& left, const ValueList& right) {
    return right == left;
}

bool operator!=(const ValueList& left, const std::vector<int32_t>& right) {
    return !(left == right);
}

bool operator!=(const std::vector<int32_t>& left, const ValueList& right) {
    return !(right == left);
}

std::ostream& operator<<(std::ostream& output, const ValueList& values) {
    output << '[';
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            output << ", ";
        }
        output << values[i];
    }
    return output << ']';
}

std::string TupleCodec::serialize(const Tuple& tuple) {
    if (tuple.size() > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("TupleCodec: demasiadas columnas");
    }

    std::string output;
    append_binary(output, TUPLE_MAGIC);
    append_binary(output, static_cast<uint16_t>(tuple.size()));
    for (const Value& value : tuple.values) {
        append_binary(output, static_cast<uint8_t>(value.type()));
        switch (value.type()) {
        case ValueType::integer:
            append_binary(output, value.as_int());
            break;
        case ValueType::text: {
            const std::string& text = value.as_text();
            if (text.size() > std::numeric_limits<uint32_t>::max()) {
                throw std::runtime_error("TupleCodec: TEXT demasiado largo");
            }
            append_binary(output, static_cast<uint32_t>(text.size()));
            output.append(text);
            break;
        }
        case ValueType::date:
            append_binary(output, value.date_encoded());
            break;
        case ValueType::hour:
            append_binary(output, value.hour_seconds());
            break;
        }
    }
    return output;
}

Tuple TupleCodec::deserialize(const char* data, size_t length) {
    if (length < sizeof(uint16_t)) {
        throw std::runtime_error("TupleCodec: registro demasiado corto");
    }

    size_t cursor = 0;
    if (length >= sizeof(uint32_t) + sizeof(uint16_t)) {
        uint32_t magic = 0;
        std::memcpy(&magic, data, sizeof(magic));
        if (magic == TUPLE_MAGIC) {
            cursor += sizeof(uint32_t);
            uint16_t columns = read_binary<uint16_t>(data, length, cursor);
            Tuple tuple;
            tuple.values.reserve(columns);
            for (uint16_t i = 0; i < columns; ++i) {
                ValueType type = static_cast<ValueType>(
                    read_binary<uint8_t>(data, length, cursor));
                switch (type) {
                case ValueType::integer:
                    tuple.values.push_back(read_binary<int32_t>(data, length, cursor));
                    break;
                case ValueType::text: {
                    uint32_t text_length = read_binary<uint32_t>(data, length, cursor);
                    if (cursor + text_length > length) {
                        throw std::runtime_error("TupleCodec: TEXT truncado");
                    }
                    tuple.values.push_back(
                        Value::text(std::string(data + cursor, data + cursor + text_length)));
                    cursor += text_length;
                    break;
                }
                case ValueType::date:
                    tuple.values.push_back(Value::from_date_encoded(
                        read_binary<int32_t>(data, length, cursor)));
                    break;
                case ValueType::hour:
                    tuple.values.push_back(Value::from_hour_seconds(
                        read_binary<int32_t>(data, length, cursor)));
                    break;
                default:
                    throw std::runtime_error("TupleCodec: etiqueta de tipo invalida");
                }
            }
            if (cursor != length) {
                throw std::runtime_error("TupleCodec: bytes adicionales en registro");
            }
            return tuple;
        }
    }

    // Formato historico: [u16 columnas][i32 ...].
    cursor = 0;
    uint16_t columns = read_binary<uint16_t>(data, length, cursor);
    size_t expected = sizeof(uint16_t) + static_cast<size_t>(columns) * sizeof(int32_t);
    if (length != expected) {
        throw std::runtime_error("TupleCodec: registro legacy corrupto");
    }
    Tuple tuple;
    tuple.values.reserve(columns);
    for (uint16_t i = 0; i < columns; ++i) {
        tuple.values.push_back(read_binary<int32_t>(data, length, cursor));
    }
    return tuple;
}
