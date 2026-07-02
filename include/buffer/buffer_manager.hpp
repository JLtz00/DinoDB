#pragma once

#include "buffer/lru_replacer.hpp"
#include "common/types.hpp"
#include "storage/disk_manager.hpp"
#include "storage/page.hpp"
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

struct Frame {
    Page page {};
    page_id_t page_id { INVALID_PAGE_ID };
    int pin_count { 0 };
    bool is_dirty { false };

    bool is_free() const { return page_id == INVALID_PAGE_ID; }
    void reset();
};

class BufferManager {
public:
    BufferManager(size_t pool_size, DiskManager* disk_manager);

    Page* fetch_page(page_id_t page_id);
    bool unpin_page(page_id_t page_id, bool is_dirty);
    Page* new_page(page_id_t& page_id);
    bool flush_page(page_id_t page_id);
    void flush_all();
    bool delete_page(page_id_t page_id);
    page_id_t disk_page_count() const;

    size_t free_frames() const;
    bool in_pool(page_id_t page_id) const;
    size_t cache_hits() const { return cache_hits_; }
    size_t cache_misses() const { return cache_misses_; }
    double hit_rate() const;
    void reset_metrics();

private:
    frame_id_t find_free_frame() const;
    frame_id_t find_victim_frame();
    void evict_frame(frame_id_t frame_id);

    size_t pool_size_;
    DiskManager* disk_;
    std::vector<Frame> frames_;
    std::unordered_map<page_id_t, frame_id_t> page_table_;
    std::unique_ptr<LRUReplacer> replacer_;
    size_t cache_hits_ { 0 };
    size_t cache_misses_ { 0 };
};
