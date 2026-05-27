#include "index/bplus_tree.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

BPlusTree::BPlusTree(BufferManager* buffer_manager)
    : buffer_(buffer_manager)
{
    if (buffer_ == nullptr) {
        throw std::invalid_argument("BPlusTree: buffer_manager no puede ser null");
    }

    root_page_id_ = create_leaf();
}

void BPlusTree::insert(int32_t key, RID rid) {
    insert_into_leaf(root_page_id_, key, rid);
}

std::optional<RID> BPlusTree::search(int32_t key) {
    LeafNode leaf = read_leaf(root_page_id_);

    for (uint16_t i = 0; i < leaf.size; ++i) {
        if (leaf.entries[i].key == key) {
            return leaf.entries[i].rid;
        }
    }
    return std::nullopt;
}

BPlusTree::LeafNode BPlusTree::read_leaf(page_id_t page_id) {
    Page* page = buffer_->fetch_page(page_id);
    LeafNode node {};
    std::memcpy(&node, page->raw_data(), sizeof(LeafNode));
    buffer_->unpin_page(page_id, false);

    if (node.magic != NODE_MAGIC || !node.is_leaf) {
        throw std::runtime_error("BPlusTree::read_leaf: pagina no es hoja valida");
    }
    return node;
}

void BPlusTree::write_leaf(page_id_t page_id, const LeafNode& node) {
    Page* page = buffer_->fetch_page(page_id);
    std::memset(page->raw_data(), 0, PAGE_SIZE);
    std::memcpy(page->raw_data(), &node, sizeof(LeafNode));
    buffer_->unpin_page(page_id, true);
}

page_id_t BPlusTree::create_leaf() {
    page_id_t page_id = INVALID_PAGE_ID;
    Page* page = buffer_->new_page(page_id);
    LeafNode leaf {};
    std::memset(page->raw_data(), 0, PAGE_SIZE);
    std::memcpy(page->raw_data(), &leaf, sizeof(LeafNode));
    buffer_->unpin_page(page_id, true);
    return page_id;
}

void BPlusTree::insert_into_leaf(page_id_t leaf_page, int32_t key, RID rid) {
    LeafNode leaf = read_leaf(leaf_page);

    std::vector<IndexValue> values(leaf.entries, leaf.entries + leaf.size);
    auto it = std::lower_bound(values.begin(), values.end(), key, [](const IndexValue& value, int32_t target) {
        return value.key < target;
    });

    if (it != values.end() && it->key == key) {
        it->rid = rid;
    } else {
        if (values.size() >= MAX_LEAF_ENTRIES) {
            throw std::runtime_error("BPlusTree: hoja llena; split pendiente");
        }
        values.insert(it, IndexValue { key, rid });
    }

    leaf.size = static_cast<uint16_t>(values.size());
    std::fill(std::begin(leaf.entries), std::end(leaf.entries), IndexValue {});
    std::copy(values.begin(), values.end(), leaf.entries);
    write_leaf(leaf_page, leaf);
}
