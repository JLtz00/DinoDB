#include "query/index_scan.hpp"

IndexScan::IndexScan(BPlusTree& index, const Table& table, int32_t key)
    : index_(index),
      table_(table),
      start_key_(key),
      end_key_(key),
      point_lookup_(true)
{}

IndexScan::IndexScan(BPlusTree& index, const Table& table, int32_t start_key, int32_t end_key)
    : index_(index),
      table_(table),
      start_key_(start_key),
      end_key_(end_key),
      point_lookup_(false)
{}

void IndexScan::open() {
    matches_.clear();
    cursor_ = 0;
    opened_ = true;

    if (point_lookup_) {
        auto rid = index_.search(start_key_);
        if (rid.has_value()) {
            matches_.push_back(*rid);
        }
        return;
    }

    matches_ = index_.range_scan(start_key_, end_key_);
}

std::optional<Tuple> IndexScan::next() {
    while (opened_ && cursor_ < matches_.size()) {
        RID rid = matches_[cursor_++];
        if (rid.page_id < table_.size()) {
            return table_[rid.page_id];
        }
    }
    return std::nullopt;
}

void IndexScan::close() {
    opened_ = false;
    matches_.clear();
    cursor_ = 0;
}
