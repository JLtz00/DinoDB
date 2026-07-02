#include "query/seq_scan.hpp"

SeqScan::SeqScan(const Table& table)
    : table_(&table)
{}

SeqScan::SeqScan(const PersistentTable& table)
    : persistent_table_(&table)
{}

void SeqScan::open() {
    cursor_ = 0;
    persistent_cursor_ = PersistentTable::Cursor {};
    opened_ = true;
}

std::optional<Tuple> SeqScan::next() {
    if (!opened_) {
        return std::nullopt;
    }

    if (table_ != nullptr) {
        if (cursor_ >= table_->size()) {
            return std::nullopt;
        }
        return (*table_)[cursor_++];
    }

    return persistent_table_->next(persistent_cursor_);
}

void SeqScan::close() {
    opened_ = false;
}
