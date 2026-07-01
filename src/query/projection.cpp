#include "query/projection.hpp"

Projection::Projection(std::unique_ptr<Operator> child, std::vector<size_t> columns)
    : child_(std::move(child)),
      columns_(std::move(columns))
{}

void Projection::open() {
    child_->open();
}

std::optional<Tuple> Projection::next() {
    auto tuple = child_->next();
    if (!tuple) {
        return std::nullopt;
    }

    Tuple projected;
    projected.values.reserve(columns_.size());
    for (size_t column : columns_) {
        projected.values.push_back(tuple->get(column));
    }
    return projected;
}

void Projection::close() {
    child_->close();
}
