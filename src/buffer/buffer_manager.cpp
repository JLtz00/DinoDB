#include "buffer/buffer_manager.hpp"
#include <stdexcept>

void Frame::reset() {
    page = Page(INVALID_PAGE_ID);
    page_id = INVALID_PAGE_ID;
    pin_count = 0;
    is_dirty = false;
}

BufferManager::BufferManager(size_t pool_size, DiskManager* disk_manager)
    : pool_size_(pool_size),
      disk_(disk_manager),
      frames_(pool_size),
      replacer_(std::make_unique<LRUReplacer>(pool_size))
{
    if (pool_size_ == 0) {
        throw std::invalid_argument("BufferManager: pool_size debe ser mayor que 0");
    }
    if (disk_ == nullptr) {
        throw std::invalid_argument("BufferManager: disk_manager no puede ser null");
    }
}

Page* BufferManager::fetch_page(page_id_t page_id) {
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        ++cache_hits_;
        Frame& frame = frames_[it->second];
        ++frame.pin_count;
        replacer_->pin(it->second);
        return &frame.page;
    }

    ++cache_misses_;
    frame_id_t frame_id = find_victim_frame();
    if (frame_id == INVALID_FRAME_ID) {
        throw std::runtime_error("BufferManager::fetch_page: pool lleno sin victimas");
    }

    evict_frame(frame_id);

    Frame& frame = frames_[frame_id];
    frame.page = disk_->read_page(page_id);
    frame.page_id = page_id;
    frame.pin_count = 1;
    frame.is_dirty = false;
    page_table_[page_id] = frame_id;
    replacer_->pin(frame_id);

    return &frame.page;
}

bool BufferManager::unpin_page(page_id_t page_id, bool is_dirty) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        throw std::runtime_error("BufferManager::unpin_page: pagina no esta en el pool");
    }

    Frame& frame = frames_[it->second];
    if (frame.pin_count <= 0) {
        throw std::runtime_error("BufferManager::unpin_page: pagina ya estaba unpinned");
    }

    if (is_dirty) {
        frame.is_dirty = true;
    }

    --frame.pin_count;
    if (frame.pin_count == 0) {
        replacer_->unpin(it->second);
    }

    return true;
}

Page* BufferManager::new_page(page_id_t& page_id) {
    frame_id_t frame_id = find_victim_frame();
    if (frame_id == INVALID_FRAME_ID) {
        throw std::runtime_error("BufferManager::new_page: pool lleno sin victimas");
    }

    evict_frame(frame_id);

    page_id = disk_->allocate_page();
    Frame& frame = frames_[frame_id];
    frame.page = Page(page_id);
    frame.page_id = page_id;
    frame.pin_count = 1;
    frame.is_dirty = true;
    page_table_[page_id] = frame_id;
    replacer_->pin(frame_id);

    return &frame.page;
}

bool BufferManager::flush_page(page_id_t page_id) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;
    }

    Frame& frame = frames_[it->second];
    disk_->write_page(page_id, frame.page);
    disk_->flush();
    frame.is_dirty = false;
    return true;
}

void BufferManager::flush_all() {
    for (Frame& frame : frames_) {
        if (!frame.is_free() && frame.is_dirty) {
            disk_->write_page(frame.page_id, frame.page);
            frame.is_dirty = false;
        }
    }
    disk_->flush();
}

bool BufferManager::delete_page(page_id_t page_id) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return true;
    }

    frame_id_t frame_id = it->second;
    Frame& frame = frames_[frame_id];
    if (frame.pin_count > 0) {
        return false;
    }

    replacer_->pin(frame_id);
    page_table_.erase(it);
    frame.reset();
    return true;
}

size_t BufferManager::free_frames() const {
    size_t count = 0;
    for (const Frame& frame : frames_) {
        if (frame.is_free()) {
            ++count;
        }
    }
    return count;
}

bool BufferManager::in_pool(page_id_t page_id) const {
    return page_table_.find(page_id) != page_table_.end();
}

double BufferManager::hit_rate() const {
    size_t total = cache_hits_ + cache_misses_;
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(cache_hits_) / static_cast<double>(total);
}

void BufferManager::reset_metrics() {
    cache_hits_ = 0;
    cache_misses_ = 0;
}

frame_id_t BufferManager::find_free_frame() const {
    for (frame_id_t i = 0; i < static_cast<frame_id_t>(frames_.size()); ++i) {
        if (frames_[i].is_free()) {
            return i;
        }
    }
    return INVALID_FRAME_ID;
}

frame_id_t BufferManager::find_victim_frame() {
    frame_id_t free_frame = find_free_frame();
    if (free_frame != INVALID_FRAME_ID) {
        return free_frame;
    }

    auto victim = replacer_->victim();
    return victim.value_or(INVALID_FRAME_ID);
}

void BufferManager::evict_frame(frame_id_t frame_id) {
    Frame& frame = frames_[frame_id];
    if (frame.is_free()) {
        return;
    }

    if (frame.is_dirty) {
        disk_->write_page(frame.page_id, frame.page);
        disk_->flush();
    }

    page_table_.erase(frame.page_id);
    frame.reset();
}
