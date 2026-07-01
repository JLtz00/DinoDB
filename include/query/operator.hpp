#pragma once

#include "query/tuple.hpp"
#include <optional>

class Operator {
public:
    virtual ~Operator() = default;
    virtual void open() = 0;
    virtual std::optional<Tuple> next() = 0;
    virtual void close() = 0;
};
