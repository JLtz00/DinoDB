#pragma once

#include "index/bplus_tree.hpp"
#include "query/operator.hpp"
#include "query/persistent_table.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

class IndexScan : public Operator {
public:
    IndexScan(BPlusTree& index, const Table& table, int32_t key);
    IndexScan(BPlusTree& index, const Table& table, int32_t start_key, int32_t end_key);
    IndexScan(BPlusTree& index, const PersistentTable& table, int32_t key);
    IndexScan(BPlusTree& index, const PersistentTable& table, int32_t start_key, int32_t end_key);

    void open() override;
    std::optional<Tuple> next() override;
    void close() override;

private:
    BPlusTree& index_;
    const Table* table_ { nullptr };
    const PersistentTable* persistent_table_ { nullptr };
    int32_t start_key_ { 0 };
    int32_t end_key_ { 0 };
    bool point_lookup_ { true };
    std::vector<RID> matches_;
    size_t cursor_ { 0 };
    bool opened_ { false };
};
