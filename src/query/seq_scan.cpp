#include "query/seq_scan.hpp"

SeqScan::SeqScan(const Table& table)
    : table_(table)
{}

void SeqScan::open() {
    cursor_ = 0;
    opened_ = true;
}

std::optional<Tuple> SeqScan::next() {
    if (!opened_ || cursor_ >= table_.size()) {
        return std::nullopt;
    }
    return table_[cursor_++];
}

void SeqScan::close() {
    opened_ = false;
}
