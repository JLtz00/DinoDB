#include <gtest/gtest.h>
#include "buffer/buffer_manager.hpp"
#include "index/bplus_tree.hpp"
#include "query/index_scan.hpp"
#include <filesystem>

namespace {

std::filesystem::path test_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

void cleanup(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
}

} // namespace

TEST(QueryIndexScanTest, BuscaClavePuntualConBPlusTree) {
    auto path = test_path("dinodb_query_index_point.db");
    cleanup(path);

    Table table {
        Tuple {{ 10, 100 }},
        Tuple {{ 20, 200 }},
        Tuple {{ 30, 300 }}
    };

    DiskManager disk(path.string());
    BufferManager buffer(8, &disk);
    BPlusTree index(&buffer);
    for (size_t i = 0; i < table.size(); ++i) {
        index.insert(table[i].get(0), RID { static_cast<page_id_t>(i), 0 });
    }

    IndexScan scan(index, table, 20);
    scan.open();
    auto row = scan.next();
    auto done = scan.next();
    scan.close();

    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->values, std::vector<int32_t>({ 20, 200 }));
    EXPECT_FALSE(done.has_value());

    cleanup(path);
}

TEST(QueryIndexScanTest, RecorreRangoConBPlusTree) {
    auto path = test_path("dinodb_query_index_range.db");
    cleanup(path);

    Table table;
    DiskManager disk(path.string());
    BufferManager buffer(8, &disk);
    BPlusTree index(&buffer);

    for (int i = 0; i < 80; ++i) {
        table.push_back(Tuple {{ i, i * 10 }});
        index.insert(i, RID { static_cast<page_id_t>(i), 0 });
    }

    IndexScan scan(index, table, 25, 34);
    scan.open();
    std::vector<int32_t> keys;
    while (auto row = scan.next()) {
        keys.push_back(row->get(0));
    }
    scan.close();

    ASSERT_EQ(keys.size(), 10u);
    EXPECT_EQ(keys.front(), 25);
    EXPECT_EQ(keys.back(), 34);

    cleanup(path);
}
