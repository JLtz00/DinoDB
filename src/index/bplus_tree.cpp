#include "index/bplus_tree.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

BPlusTree::BPlusTree(BufferManager* buffer_manager)
    : buffer_(buffer_manager)
{
    if (buffer_ == nullptr) {
        throw std::invalid_argument("BPlusTree: buffer_manager no puede ser null");
    }

    root_page_id_ = create_leaf();
}

void BPlusTree::insert(int32_t key, RID rid) {
    page_id_t leaf_page = find_leaf_page(key);
    auto split = insert_into_leaf(leaf_page, key, rid);
    if (split.has_value()) {
        insert_into_root_internal(split->separator, split->right_page);
    }
}

std::optional<RID> BPlusTree::search(int32_t key) {
    page_id_t leaf_page = find_leaf_page(key);
    LeafNode leaf = read_leaf(leaf_page);

    for (uint16_t i = 0; i < leaf.size; ++i) {
        if (leaf.entries[i].key == key) {
            return leaf.entries[i].rid;
        }
    }
    return std::nullopt;
}

std::vector<RID> BPlusTree::range_scan(int32_t start_key, int32_t end_key) {
    std::vector<RID> result;
    if (end_key < start_key) {
        return result;
    }

    page_id_t page_id = find_leaf_page(start_key);
    while (page_id != INVALID_PAGE_ID) {
        LeafNode leaf = read_leaf(page_id);
        for (uint16_t i = 0; i < leaf.size; ++i) {
            int32_t key = leaf.entries[i].key;
            if (key >= start_key && key <= end_key) {
                result.push_back(leaf.entries[i].rid);
            }
            if (key > end_key) {
                return result;
            }
        }
        page_id = leaf.next;
    }

    return result;
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

BPlusTree::InternalNode BPlusTree::read_internal(page_id_t page_id) {
    Page* page = buffer_->fetch_page(page_id);
    InternalNode node {};
    std::memcpy(&node, page->raw_data(), sizeof(InternalNode));
    buffer_->unpin_page(page_id, false);

    if (node.magic != NODE_MAGIC || node.is_leaf) {
        throw std::runtime_error("BPlusTree::read_internal: pagina no es interna valida");
    }
    return node;
}

void BPlusTree::write_internal(page_id_t page_id, const InternalNode& node) {
    Page* page = buffer_->fetch_page(page_id);
    std::memset(page->raw_data(), 0, PAGE_SIZE);
    std::memcpy(page->raw_data(), &node, sizeof(InternalNode));
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

page_id_t BPlusTree::create_internal() {
    page_id_t page_id = INVALID_PAGE_ID;
    Page* page = buffer_->new_page(page_id);
    InternalNode internal {};
    std::memset(page->raw_data(), 0, PAGE_SIZE);
    std::memcpy(page->raw_data(), &internal, sizeof(InternalNode));
    buffer_->unpin_page(page_id, true);
    return page_id;
}

page_id_t BPlusTree::find_leaf_page(int32_t key) {
    if (root_is_leaf_) {
        return root_page_id_;
    }

    InternalNode root = read_internal(root_page_id_);
    uint16_t child_index = 0;
    while (child_index < root.size && key >= root.keys[child_index]) {
        ++child_index;
    }
    return root.children[child_index];
}

std::optional<BPlusTree::SplitResult> BPlusTree::insert_into_leaf(page_id_t leaf_page, int32_t key, RID rid) {
    LeafNode leaf = read_leaf(leaf_page);

    std::vector<IndexValue> values(leaf.entries, leaf.entries + leaf.size);
    auto it = std::lower_bound(values.begin(), values.end(), key, [](const IndexValue& value, int32_t target) {
        return value.key < target;
    });

    if (it != values.end() && it->key == key) {
        it->rid = rid;
    } else {
        values.insert(it, IndexValue { key, rid });
    }

    if (values.size() <= MAX_LEAF_ENTRIES) {
        leaf.size = static_cast<uint16_t>(values.size());
        std::copy(values.begin(), values.end(), leaf.entries);
        write_leaf(leaf_page, leaf);
        return std::nullopt;
    }

    size_t split_at = values.size() / 2;
    std::vector<IndexValue> left(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(split_at));
    std::vector<IndexValue> right(values.begin() + static_cast<std::ptrdiff_t>(split_at), values.end());

    page_id_t right_page = create_leaf();
    LeafNode right_leaf = read_leaf(right_page);

    leaf.size = static_cast<uint16_t>(left.size());
    std::fill(std::begin(leaf.entries), std::end(leaf.entries), IndexValue {});
    std::copy(left.begin(), left.end(), leaf.entries);

    right_leaf.size = static_cast<uint16_t>(right.size());
    std::copy(right.begin(), right.end(), right_leaf.entries);
    right_leaf.next = leaf.next;
    leaf.next = right_page;

    write_leaf(leaf_page, leaf);
    write_leaf(right_page, right_leaf);

    return SplitResult { right.front().key, right_page };
}

void BPlusTree::insert_into_root_internal(int32_t separator, page_id_t right_page) {
    if (root_is_leaf_) {
        page_id_t old_root = root_page_id_;
        page_id_t new_root = create_internal();
        InternalNode root {};
        root.is_leaf = false;
        root.size = 1;
        root.keys[0] = separator;
        root.children[0] = old_root;
        root.children[1] = right_page;
        write_internal(new_root, root);
        root_page_id_ = new_root;
        root_is_leaf_ = false;
        return;
    }

    InternalNode root = read_internal(root_page_id_);
    if (root.size >= MAX_INTERNAL_KEYS) {
        throw std::runtime_error("BPlusTree: raiz interna llena; split interno aun no implementado");
    }

    uint16_t pos = 0;
    while (pos < root.size && separator > root.keys[pos]) {
        ++pos;
    }
    for (uint16_t i = root.size; i > pos; --i) {
        root.keys[i] = root.keys[i - 1];
        root.children[i + 1] = root.children[i];
    }
    root.keys[pos] = separator;
    root.children[pos + 1] = right_page;
    ++root.size;
    write_internal(root_page_id_, root);
}
