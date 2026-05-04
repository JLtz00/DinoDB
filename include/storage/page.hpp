#pragma once

#include "common/types.hpp"
#include <cstring>
#include <stdexcept>

#pragma pack(push, 1)
struct PageHeader {
    page_id_t page_id        { INVALID_PAGE_ID };
    offset_t  free_space_end { sizeof(PageHeader) };
    slot_id_t slot_count     { 0 };
    slot_id_t free_slot_count{ 0 };
    uint8_t   _pad[6]        { 0 };
};
#pragma pack(pop)

static_assert(sizeof(PageHeader) == 16, "PageHeader debe ser 16 bytes exactos");

#pragma pack(push, 1)
struct SlotEntry {
    offset_t offset { 0 };
    offset_t length { 0 };
};
#pragma pack(pop)

static_assert(sizeof(SlotEntry) == 4, "SlotEntry debe ser 4 bytes exactos");

class Page {
public:
    explicit Page(page_id_t pid = INVALID_PAGE_ID);
    static Page from_bytes(const char* raw);
    void to_bytes(char* dest) const;

    slot_id_t insert(const char* data, offset_t length);
    const char* read(slot_id_t slot_id, offset_t& out_len) const;
    void remove(slot_id_t slot_id);

    size_t free_space() const;
    bool can_fit(offset_t length) const;
    void compact();

    page_id_t page_id()    const { return header_.page_id; }
    slot_id_t slot_count() const { return header_.slot_count; }

private:
    char data_[PAGE_SIZE];

    PageHeader& header() {
        return *reinterpret_cast<PageHeader*>(data_);
    }
    const PageHeader& header() const {
        return *reinterpret_cast<const PageHeader*>(data_);
    }
    SlotEntry* slot_dir() {
        return reinterpret_cast<SlotEntry*>(data_ + PAGE_SIZE) - header().slot_count;
    }
    const SlotEntry* slot_dir() const {
        return reinterpret_cast<const SlotEntry*>(data_ + PAGE_SIZE) - header().slot_count;
    }
    size_t slot_dir_start() const {
        return PAGE_SIZE - header().slot_count * sizeof(SlotEntry);
    }

    PageHeader& header_{ *reinterpret_cast<PageHeader*>(data_) };
};
