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

    if (buffer_->disk_page_count() == 0) {
        initialize_new_tree();
    } else {
        load_header();
    }
}

void BPlusTree::insert(int32_t key, RID rid) {
    auto root_split = insert_recursive(root_page_id_, key, rid);
    if (!root_split.has_value()) {
        return;
    }

    page_id_t old_root = root_page_id_;
    page_id_t new_root = create_internal();
    InternalNode root {};
    root.is_leaf = false;
    root.size = 1;
    root.keys[0] = root_split->separator;
    root.children[0] = old_root;
    root.children[1] = root_split->right_page;
    write_internal(new_root, root);

    root_page_id_ = new_root;
    ++height_;
    write_header();
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

void BPlusTree::initialize_new_tree() {
    page_id_t header_page_id = INVALID_PAGE_ID;
    Page* header = buffer_->new_page(header_page_id);
    if (header_page_id != HEADER_PAGE_ID) {
        throw std::runtime_error("BPlusTree: la pagina header debe ser 0");
    }
    std::memset(header->raw_data(), 0, PAGE_SIZE);
    buffer_->unpin_page(header_page_id, true);

    root_page_id_ = create_leaf();
    height_ = 1;
    write_header();
}

void BPlusTree::load_header() {
    Page* page = buffer_->fetch_page(HEADER_PAGE_ID);
    HeaderPage header {};
    std::memcpy(&header, page->raw_data(), sizeof(HeaderPage));
    buffer_->unpin_page(HEADER_PAGE_ID, false);

    if (header.magic != HEADER_MAGIC || header.version != METADATA_VERSION ||
        header.root_page_id == INVALID_PAGE_ID || header.height == 0) {
        throw std::runtime_error("BPlusTree: metadata de indice invalida");
    }

    root_page_id_ = header.root_page_id;
    height_ = header.height;
}

void BPlusTree::write_header() {
    Page* page = buffer_->fetch_page(HEADER_PAGE_ID);
    HeaderPage header {};
    header.root_page_id = root_page_id_;
    header.height = static_cast<uint16_t>(height_);
    std::memset(page->raw_data(), 0, PAGE_SIZE);
    std::memcpy(page->raw_data(), &header, sizeof(HeaderPage));
    buffer_->unpin_page(HEADER_PAGE_ID, true);
}

bool BPlusTree::is_leaf_page(page_id_t page_id) {
    Page* page = buffer_->fetch_page(page_id);
    uint32_t magic = 0;
    bool is_leaf = false;
    std::memcpy(&magic, page->raw_data(), sizeof(uint32_t));
    std::memcpy(&is_leaf, page->raw_data() + sizeof(uint32_t), sizeof(bool));
    buffer_->unpin_page(page_id, false);

    if (magic != NODE_MAGIC) {
        throw std::runtime_error("BPlusTree::is_leaf_page: pagina de nodo invalida");
    }
    return is_leaf;
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
    page_id_t current = root_page_id_;
    while (!is_leaf_page(current)) {
        InternalNode node = read_internal(current);
        uint16_t child_index = 0;
        while (child_index < node.size && key >= node.keys[child_index]) {
            ++child_index;
        }
        current = node.children[child_index];
    }
    return current;
}

std::optional<BPlusTree::SplitResult> BPlusTree::insert_recursive(page_id_t page_id, int32_t key, RID rid) {
    if (is_leaf_page(page_id)) {
        return insert_into_leaf(page_id, key, rid);
    }

    InternalNode node = read_internal(page_id);
    uint16_t child_index = 0;
    while (child_index < node.size && key >= node.keys[child_index]) {
        ++child_index;
    }

    auto child_split = insert_recursive(node.children[child_index], key, rid);
    if (!child_split.has_value()) {
        return std::nullopt;
    }
    return insert_into_internal(page_id, *child_split);
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
        std::fill(std::begin(leaf.entries), std::end(leaf.entries), IndexValue {});
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
    std::fill(std::begin(right_leaf.entries), std::end(right_leaf.entries), IndexValue {});
    std::copy(right.begin(), right.end(), right_leaf.entries);
    right_leaf.next = leaf.next;
    leaf.next = right_page;

    write_leaf(leaf_page, leaf);
    write_leaf(right_page, right_leaf);

    return SplitResult { right.front().key, right_page };
}

std::optional<BPlusTree::SplitResult> BPlusTree::insert_into_internal(page_id_t internal_page, const SplitResult& child_split) {
    InternalNode node = read_internal(internal_page);

    std::vector<int32_t> keys(node.keys, node.keys + node.size);
    std::vector<page_id_t> children(node.children, node.children + node.size + 1);

    auto it = std::upper_bound(keys.begin(), keys.end(), child_split.separator);
    size_t pos = static_cast<size_t>(std::distance(keys.begin(), it));
    keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(pos), child_split.separator);
    children.insert(children.begin() + static_cast<std::ptrdiff_t>(pos + 1), child_split.right_page);

    if (keys.size() <= MAX_INTERNAL_KEYS) {
        node.size = static_cast<uint16_t>(keys.size());
        std::fill(std::begin(node.keys), std::end(node.keys), 0);
        std::fill(std::begin(node.children), std::end(node.children), INVALID_PAGE_ID);
        std::copy(keys.begin(), keys.end(), node.keys);
        std::copy(children.begin(), children.end(), node.children);
        write_internal(internal_page, node);
        return std::nullopt;
    }

    size_t mid = keys.size() / 2;
    int32_t promoted = keys[mid];

    std::vector<int32_t> left_keys(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(mid));
    std::vector<int32_t> right_keys(keys.begin() + static_cast<std::ptrdiff_t>(mid + 1), keys.end());
    std::vector<page_id_t> left_children(children.begin(), children.begin() + static_cast<std::ptrdiff_t>(mid + 1));
    std::vector<page_id_t> right_children(children.begin() + static_cast<std::ptrdiff_t>(mid + 1), children.end());

    node.size = static_cast<uint16_t>(left_keys.size());
    std::fill(std::begin(node.keys), std::end(node.keys), 0);
    std::fill(std::begin(node.children), std::end(node.children), INVALID_PAGE_ID);
    std::copy(left_keys.begin(), left_keys.end(), node.keys);
    std::copy(left_children.begin(), left_children.end(), node.children);

    page_id_t right_page = create_internal();
    InternalNode right = read_internal(right_page);
    right.size = static_cast<uint16_t>(right_keys.size());
    std::fill(std::begin(right.keys), std::end(right.keys), 0);
    std::fill(std::begin(right.children), std::end(right.children), INVALID_PAGE_ID);
    std::copy(right_keys.begin(), right_keys.end(), right.keys);
    std::copy(right_children.begin(), right_children.end(), right.children);

    write_internal(internal_page, node);
    write_internal(right_page, right);

    return SplitResult { promoted, right_page };
}
