#pragma once

#include "buffer/buffer_manager.hpp"
#include "common/types.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

struct IndexValue {
    int32_t key { 0 };
    RID rid {};
};

class BPlusTree {
public:
    explicit BPlusTree(BufferManager* buffer_manager);

    void insert(int32_t key, RID rid);
    std::optional<RID> search(int32_t key);

    page_id_t root_page_id() const { return root_page_id_; }
    size_t height() const { return 1; }

private:
    static constexpr uint32_t NODE_MAGIC = 0x44425849; // DBXI
    static constexpr uint16_t MAX_LEAF_ENTRIES = 31;

    struct LeafNode {
        uint32_t magic { NODE_MAGIC };
        bool is_leaf { true };
        uint16_t size { 0 };
        page_id_t next { INVALID_PAGE_ID };
        IndexValue entries[MAX_LEAF_ENTRIES] {};
    };

    BufferManager* buffer_;
    page_id_t root_page_id_ { INVALID_PAGE_ID };

    LeafNode read_leaf(page_id_t page_id);
    void write_leaf(page_id_t page_id, const LeafNode& node);
    page_id_t create_leaf();
    void insert_into_leaf(page_id_t leaf_page, int32_t key, RID rid);
};
