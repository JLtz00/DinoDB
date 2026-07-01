#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Tuple {
    std::vector<int32_t> values;

    int32_t get(size_t index) const { return values.at(index); }
    size_t size() const { return values.size(); }
};

using Table = std::vector<Tuple>;
