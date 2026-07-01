#pragma once

#include "query/operator.hpp"
#include <functional>
#include <memory>

class Selection : public Operator {
public:
    Selection(std::unique_ptr<Operator> child, std::function<bool(const Tuple&)> predicate);

    void open() override;
    std::optional<Tuple> next() override;
    void close() override;

private:
    std::unique_ptr<Operator> child_;
    std::function<bool(const Tuple&)> predicate_;
};
