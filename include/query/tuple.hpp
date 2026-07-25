#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include <utility>

enum class ValueType : uint8_t {
    integer = 1,
    text = 2,
    date = 3,
    hour = 4
};

const char* value_type_name(ValueType type);

struct DateValue {
    int32_t encoded { 0 }; // YYYYMMDD

    bool operator==(const DateValue& other) const { return encoded == other.encoded; }
    bool operator<(const DateValue& other) const { return encoded < other.encoded; }
};

struct HourValue {
    int32_t seconds { 0 }; // Segundos desde 00:00:00

    bool operator==(const HourValue& other) const { return seconds == other.seconds; }
    bool operator<(const HourValue& other) const { return seconds < other.seconds; }
};

class Value {
public:
    Value();
    Value(int32_t value);

    static Value text(std::string value);
    static Value date(const std::string& value);
    static Value hour(const std::string& value);
    static Value from_date_encoded(int32_t value);
    static Value from_hour_seconds(int32_t value);

    ValueType type() const;
    int32_t as_int() const;
    const std::string& as_text() const;
    int32_t date_encoded() const;
    int32_t hour_seconds() const;
    std::string to_string() const;
    Value coerce(ValueType target) const;
    int compare(const Value& other) const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }
    bool operator<(const Value& other) const { return compare(other) < 0; }
    bool operator<=(const Value& other) const { return compare(other) <= 0; }
    bool operator>(const Value& other) const { return compare(other) > 0; }
    bool operator>=(const Value& other) const { return compare(other) >= 0; }

private:
    using Storage = std::variant<int32_t, std::string, DateValue, HourValue>;
    explicit Value(Storage storage);

    Storage storage_;
};

std::ostream& operator<<(std::ostream& output, const Value& value);

// Contenedor compatible con el uso historico de Tuple::values, ahora tipado.
class ValueList {
public:
    using storage_type = std::vector<Value>;
    using value_type = Value;
    using iterator = storage_type::iterator;
    using const_iterator = storage_type::const_iterator;

    ValueList() = default;
    ValueList(std::initializer_list<Value> values)
        : values_(values)
    {}
    explicit ValueList(std::vector<Value> values)
        : values_(std::move(values))
    {}

    size_t size() const { return values_.size(); }
    bool empty() const { return values_.empty(); }
    void reserve(size_t size) { values_.reserve(size); }
    void push_back(const Value& value) { values_.push_back(value); }
    void push_back(Value&& value) { values_.push_back(std::move(value)); }

    Value& operator[](size_t index) { return values_[index]; }
    const Value& operator[](size_t index) const { return values_[index]; }
    Value& at(size_t index) { return values_.at(index); }
    const Value& at(size_t index) const { return values_.at(index); }

    iterator begin() { return values_.begin(); }
    iterator end() { return values_.end(); }
    const_iterator begin() const { return values_.begin(); }
    const_iterator end() const { return values_.end(); }
    const_iterator cbegin() const { return values_.cbegin(); }
    const_iterator cend() const { return values_.cend(); }

    iterator insert(iterator position, const_iterator first, const_iterator last) {
        return values_.insert(position, first, last);
    }

    bool operator==(const ValueList& other) const { return values_ == other.values_; }
    bool operator!=(const ValueList& other) const { return !(*this == other); }

private:
    storage_type values_;
};

bool operator==(const ValueList& left, const std::vector<int32_t>& right);
bool operator==(const std::vector<int32_t>& left, const ValueList& right);
bool operator!=(const ValueList& left, const std::vector<int32_t>& right);
bool operator!=(const std::vector<int32_t>& left, const ValueList& right);
std::ostream& operator<<(std::ostream& output, const ValueList& values);

struct Tuple {
    ValueList values;

    // Conserva la API anterior para operadores e indices que requieren INT.
    int32_t get(size_t index) const { return values.at(index).as_int(); }
    const Value& value(size_t index) const { return values.at(index); }
    size_t size() const { return values.size(); }
};

using Table = std::vector<Tuple>;
