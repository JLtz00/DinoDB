#pragma once

#include "common/types.hpp"
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>

class LRUReplacer {
public:
    explicit LRUReplacer(size_t capacity);

    std::optional<frame_id_t> victim();
    void pin(frame_id_t frame_id);
    void unpin(frame_id_t frame_id);

    size_t size() const { return lru_list_.size(); }
    bool contains(frame_id_t frame_id) const;

private:
    size_t capacity_;
    std::list<frame_id_t> lru_list_;
    std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator> frame_map_;
};
