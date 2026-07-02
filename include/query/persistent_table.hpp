#pragma once

#include "common/types.hpp"
#include "query/tuple.hpp"
#include "storage/disk_manager.hpp"
#include <optional>

class PersistentTable {
public:
    struct Cursor {
        page_id_t page_id { 0 };
        slot_id_t slot_id { 0 };
    };

    explicit PersistentTable(DiskManager* disk_manager);

    RID insert(const Tuple& tuple);
    std::optional<Tuple> read(RID rid) const;
    std::optional<Tuple> next(Cursor& cursor) const;
    page_id_t page_count() const;

private:
    DiskManager* disk_;

    static std::string serialize(const Tuple& tuple);
    static Tuple deserialize(const char* data, offset_t length);
};
