#include <gtest/gtest.h>
#include "buffer/buffer_manager.hpp"
#include "index/bplus_tree.hpp"
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

TEST(BPlusTreeWeek07Test, InsertaYBuscaClaveEnHojaRaiz) {
    auto path = test_path("dinodb_week07_btree_search.db");
    cleanup(path);

    DiskManager disk(path.string());
    BufferManager buffer(4, &disk);
    BPlusTree tree(&buffer);

    tree.insert(10, RID { 1, 2 });
    tree.insert(4, RID { 3, 1 });

    auto found = tree.search(10);

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->page_id, 1u);
    EXPECT_EQ(found->slot_id, 2u);
    EXPECT_FALSE(tree.search(99).has_value());

    cleanup(path);
}

TEST(BPlusTreeWeek07Test, ActualizaRidCuandoLaClaveExiste) {
    auto path = test_path("dinodb_week07_btree_update.db");
    cleanup(path);

    DiskManager disk(path.string());
    BufferManager buffer(4, &disk);
    BPlusTree tree(&buffer);

    tree.insert(7, RID { 1, 1 });
    tree.insert(7, RID { 2, 5 });

    auto found = tree.search(7);

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->page_id, 2u);
    EXPECT_EQ(found->slot_id, 5u);

    cleanup(path);
}
