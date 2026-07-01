#pragma once

#include "query/operator.hpp"
#include <functional>
#include <memory>
#include <vector>

class NestedLoopJoin : public Operator {
public:
    NestedLoopJoin(std::unique_ptr<Operator> left,
                   std::unique_ptr<Operator> right,
                   std::function<bool(const Tuple&, const Tuple&)> predicate);

    void open() override;
    std::optional<Tuple> next() override;
    void close() override;

private:
    std::unique_ptr<Operator> left_;
    std::unique_ptr<Operator> right_;
    std::function<bool(const Tuple&, const Tuple&)> predicate_;
    std::vector<Tuple> right_rows_;
    std::optional<Tuple> current_left_;
    size_t right_index_ { 0 };
};
