#include "buffer/lru_replacer.hpp"

LRUReplacer::LRUReplacer(size_t capacity)
    : capacity_(capacity)
{}

std::optional<frame_id_t> LRUReplacer::victim() {
    if (lru_list_.empty()) {
        return std::nullopt;
    }

    frame_id_t victim = lru_list_.back();
    lru_list_.pop_back();
    frame_map_.erase(victim);
    return victim;
}

void LRUReplacer::pin(frame_id_t frame_id) {
    auto it = frame_map_.find(frame_id);
    if (it == frame_map_.end()) {
        return;
    }

    lru_list_.erase(it->second);
    frame_map_.erase(it);
}

void LRUReplacer::unpin(frame_id_t frame_id) {
    auto it = frame_map_.find(frame_id);
    if (it != frame_map_.end()) {
        lru_list_.erase(it->second);
        frame_map_.erase(it);
    }

    if (lru_list_.size() >= capacity_) {
        return;
    }

    lru_list_.push_front(frame_id);
    frame_map_[frame_id] = lru_list_.begin();
}

bool LRUReplacer::contains(frame_id_t frame_id) const {
    return frame_map_.find(frame_id) != frame_map_.end();
}
