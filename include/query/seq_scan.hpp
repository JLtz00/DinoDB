#pragma once

#include "query/operator.hpp"
#include "query/persistent_table.hpp"

class SeqScan : public Operator {
public:
    explicit SeqScan(const Table& table);
    explicit SeqScan(const PersistentTable& table);

    void open() override;
    std::optional<Tuple> next() override;
    void close() override;

private:
    const Table* table_ { nullptr };
    const PersistentTable* persistent_table_ { nullptr };
    size_t cursor_ { 0 };
    PersistentTable::Cursor persistent_cursor_ {};
    bool opened_ { false };
};
