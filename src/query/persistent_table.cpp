#include "query/persistent_table.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void append_u16(std::string& out, uint16_t value) {
    const char* bytes = reinterpret_cast<const char*>(&value);
    out.append(bytes, sizeof(value));
}

void append_i32(std::string& out, int32_t value) {
    const char* bytes = reinterpret_cast<const char*>(&value);
    out.append(bytes, sizeof(value));
}

uint16_t read_u16(const char* data) {
    uint16_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

int32_t read_i32(const char* data) {
    int32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

} // namespace

PersistentTable::PersistentTable(DiskManager* disk_manager)
    : disk_(disk_manager)
{
    if (disk_ == nullptr) {
        throw std::invalid_argument("PersistentTable: disk_manager no puede ser null");
    }
}

RID PersistentTable::insert(const Tuple& tuple) {
    std::string encoded = serialize(tuple);
    if (encoded.size() > std::numeric_limits<offset_t>::max()) {
        throw std::runtime_error("PersistentTable::insert: tupla demasiado grande");
    }

    page_id_t page_id = disk_->page_count();
    Page page;
    if (page_id == 0) {
        page_id = disk_->allocate_page();
        page = disk_->read_page(page_id);
    } else {
        page_id = disk_->page_count() - 1;
        page = disk_->read_page(page_id);
        if (!page.can_fit(static_cast<offset_t>(encoded.size()))) {
            page_id = disk_->allocate_page();
            page = disk_->read_page(page_id);
        }
    }

    slot_id_t slot_id = page.insert(encoded.data(), static_cast<offset_t>(encoded.size()));
    disk_->write_page(page_id, page);
    return RID { page_id, slot_id };
}

std::optional<Tuple> PersistentTable::read(RID rid) const {
    if (!rid.is_valid() || rid.page_id >= disk_->page_count()) {
        return std::nullopt;
    }

    Page page = disk_->read_page(rid.page_id);
    if (rid.slot_id >= page.slot_count()) {
        return std::nullopt;
    }

    try {
        offset_t length = 0;
        const char* data = page.read(rid.slot_id, length);
        return deserialize(data, length);
    } catch (const std::runtime_error&) {
        return std::nullopt;
    }
}

std::optional<Tuple> PersistentTable::next(Cursor& cursor) const {
    while (cursor.page_id < disk_->page_count()) {
        Page page = disk_->read_page(cursor.page_id);
        while (cursor.slot_id < page.slot_count()) {
            slot_id_t current_slot = cursor.slot_id++;
            try {
                offset_t length = 0;
                const char* data = page.read(current_slot, length);
                return deserialize(data, length);
            } catch (const std::runtime_error&) {
            }
        }
        ++cursor.page_id;
        cursor.slot_id = 0;
    }
    return std::nullopt;
}

page_id_t PersistentTable::page_count() const {
    return disk_->page_count();
}

std::string PersistentTable::serialize(const Tuple& tuple) {
    if (tuple.values.size() > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("PersistentTable::serialize: demasiadas columnas");
    }

    std::string encoded;
    encoded.reserve(sizeof(uint16_t) + tuple.values.size() * sizeof(int32_t));
    append_u16(encoded, static_cast<uint16_t>(tuple.values.size()));
    for (int32_t value : tuple.values) {
        append_i32(encoded, value);
    }
    return encoded;
}

Tuple PersistentTable::deserialize(const char* data, offset_t length) {
    if (length < sizeof(uint16_t)) {
        throw std::runtime_error("PersistentTable::deserialize: registro corrupto");
    }

    uint16_t columns = read_u16(data);
    size_t expected = sizeof(uint16_t) + static_cast<size_t>(columns) * sizeof(int32_t);
    if (length != expected) {
        throw std::runtime_error("PersistentTable::deserialize: longitud invalida");
    }

    Tuple tuple;
    tuple.values.reserve(columns);
    const char* cursor = data + sizeof(uint16_t);
    for (uint16_t i = 0; i < columns; ++i) {
        tuple.values.push_back(read_i32(cursor));
        cursor += sizeof(int32_t);
    }
    return tuple;
}
