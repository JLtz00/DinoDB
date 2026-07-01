#include "query/nested_loop_join.hpp"

NestedLoopJoin::NestedLoopJoin(std::unique_ptr<Operator> left,
                               std::unique_ptr<Operator> right,
                               std::function<bool(const Tuple&, const Tuple&)> predicate)
    : left_(std::move(left)),
      right_(std::move(right)),
      predicate_(std::move(predicate))
{}

void NestedLoopJoin::open() {
    right_rows_.clear();
    right_->open();
    while (auto tuple = right_->next()) {
        right_rows_.push_back(*tuple);
    }
    right_->close();

    left_->open();
    current_left_ = left_->next();
    right_index_ = 0;
}

std::optional<Tuple> NestedLoopJoin::next() {
    while (current_left_) {
        while (right_index_ < right_rows_.size()) {
            const Tuple& right = right_rows_[right_index_++];
            if (predicate_(*current_left_, right)) {
                Tuple joined = *current_left_;
                joined.values.insert(joined.values.end(), right.values.begin(), right.values.end());
                return joined;
            }
        }
        current_left_ = left_->next();
        right_index_ = 0;
    }
    return std::nullopt;
}

void NestedLoopJoin::close() {
    left_->close();
    current_left_.reset();
    right_rows_.clear();
    right_index_ = 0;
}
