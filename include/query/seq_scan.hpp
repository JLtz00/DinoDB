#pragma once

#include "query/operator.hpp"

class SeqScan : public Operator {
public:
    explicit SeqScan(const Table& table);

    void open() override;
    std::optional<Tuple> next() override;
    void close() override;

private:
    const Table& table_;
    size_t cursor_ { 0 };
    bool opened_ { false };
};
