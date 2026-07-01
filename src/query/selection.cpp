#include "query/selection.hpp"

Selection::Selection(std::unique_ptr<Operator> child, std::function<bool(const Tuple&)> predicate)
    : child_(std::move(child)),
      predicate_(std::move(predicate))
{}

void Selection::open() {
    child_->open();
}

std::optional<Tuple> Selection::next() {
    while (auto tuple = child_->next()) {
        if (predicate_(*tuple)) {
            return tuple;
        }
    }
    return std::nullopt;
}

void Selection::close() {
    child_->close();
}
