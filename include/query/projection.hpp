#pragma once

#include "query/operator.hpp"
#include <memory>
#include <vector>

class Projection : public Operator {
public:
    Projection(std::unique_ptr<Operator> child, std::vector<size_t> columns);

    void open() override;
    std::optional<Tuple> next() override;
    void close() override;

private:
    std::unique_ptr<Operator> child_;
    std::vector<size_t> columns_;
};
