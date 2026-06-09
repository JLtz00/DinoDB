#pragma once

#include "buffer/buffer_manager.hpp"
#include "common/types.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct IndexValue {
    int32_t key { 0 };
    RID rid {};
};

class BPlusTree {
public:
    explicit BPlusTree(BufferManager* buffer_manager);

    void insert(int32_t key, RID rid);
    std::optional<RID> search(int32_t key);
    std::vector<RID> range_scan(int32_t start_key, int32_t end_key);

    page_id_t root_page_id() const { return root_page_id_; }
    size_t height() const { return root_is_leaf_ ? 1 : 2; }

private:
    static constexpr uint32_t NODE_MAGIC = 0x44425849; // DBXI
    static constexpr uint16_t MAX_LEAF_ENTRIES = 31;
    static constexpr uint16_t MAX_INTERNAL_KEYS = 63;

    struct LeafNode {
        uint32_t magic { NODE_MAGIC };
        bool is_leaf { true };
        uint16_t size { 0 };
        page_id_t next { INVALID_PAGE_ID };
        IndexValue entries[MAX_LEAF_ENTRIES] {};
    };

    struct InternalNode {
        uint32_t magic { NODE_MAGIC };
        bool is_leaf { false };
        uint16_t size { 0 };
        page_id_t children[MAX_INTERNAL_KEYS + 1] {};
        int32_t keys[MAX_INTERNAL_KEYS] {};
    };

    struct SplitResult {
        int32_t separator { 0 };
        page_id_t right_page { INVALID_PAGE_ID };
    };

    BufferManager* buffer_;
    page_id_t root_page_id_ { INVALID_PAGE_ID };
    bool root_is_leaf_ { true };

    LeafNode read_leaf(page_id_t page_id);
    void write_leaf(page_id_t page_id, const LeafNode& node);
    InternalNode read_internal(page_id_t page_id);
    void write_internal(page_id_t page_id, const InternalNode& node);

    page_id_t create_leaf();
    page_id_t create_internal();
    page_id_t find_leaf_page(int32_t key);
    std::optional<SplitResult> insert_into_leaf(page_id_t leaf_page, int32_t key, RID rid);
    void insert_into_root_internal(int32_t separator, page_id_t right_page);
};
