#include <gtest/gtest.h>
#include "buffer/buffer_manager.hpp"
#include "buffer/lru_replacer.hpp"
#include <filesystem>
#include <string>

namespace {

std::filesystem::path test_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

void cleanup(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
}

std::string read_string(const Page& page, slot_id_t slot_id) {
    offset_t len = 0;
    const char* data = page.read(slot_id, len);
    return std::string(data, len);
}

} // namespace

TEST(LRUReplacerTest, VictimEnPoolVacio) {
    LRUReplacer replacer(3);

    EXPECT_FALSE(replacer.victim().has_value());
}

TEST(LRUReplacerTest, UnpinYVictim) {
    LRUReplacer replacer(3);
    replacer.unpin(0);
    replacer.unpin(1);
    replacer.unpin(2);

    EXPECT_EQ(replacer.victim().value(), 0);
}

TEST(LRUReplacerTest, PinNoEsVictima) {
    LRUReplacer replacer(3);
    replacer.unpin(0);
    replacer.unpin(1);
    replacer.pin(0);

    EXPECT_EQ(replacer.victim().value(), 1);
    EXPECT_FALSE(replacer.victim().has_value());
}

TEST(LRUReplacerTest, UnpinActualizaOrden) {
    LRUReplacer replacer(3);
    replacer.unpin(0);
    replacer.unpin(1);
    replacer.unpin(0);

    EXPECT_EQ(replacer.victim().value(), 1);
}

TEST(LRUReplacerTest, SizeCorrecto) {
    LRUReplacer replacer(3);
    replacer.unpin(0);
    replacer.unpin(1);
    replacer.pin(0);

    EXPECT_EQ(replacer.size(), 1u);
}

TEST(BufferManagerTest, NewPageAsignaId) {
    auto path = test_path("dinodb_buffer_new.db");
    cleanup(path);
    DiskManager disk(path.string());
    BufferManager buffer(2, &disk);

    page_id_t page_id = INVALID_PAGE_ID;
    Page* page = buffer.new_page(page_id);

    ASSERT_NE(page, nullptr);
    EXPECT_EQ(page_id, 0u);
    EXPECT_EQ(page->page_id(), 0u);
    cleanup(path);
}

TEST(BufferManagerTest, FetchPaginaExistente) {
    auto path = test_path("dinodb_buffer_fetch.db");
    cleanup(path);
    DiskManager disk(path.string());
    page_id_t page_id = disk.allocate_page();
    BufferManager buffer(2, &disk);

    Page* page = buffer.fetch_page(page_id);

    ASSERT_NE(page, nullptr);
    EXPECT_TRUE(buffer.in_pool(page_id));
    cleanup(path);
}

TEST(BufferManagerTest, EscrituraPersistente) {
    auto path = test_path("dinodb_buffer_persist.db");
    cleanup(path);
    DiskManager disk(path.string());
    BufferManager buffer(1, &disk);

    page_id_t page_id = INVALID_PAGE_ID;
    Page* page = buffer.new_page(page_id);
    slot_id_t slot = page->insert("buffer", 6);
    buffer.unpin_page(page_id, true);
    buffer.flush_page(page_id);

    DiskManager reopened(path.string());
    Page restored = reopened.read_page(page_id);

    EXPECT_EQ(read_string(restored, slot), "buffer");
    cleanup(path);
}

TEST(BufferManagerTest, FramesLibresIniciales) {
    auto path = test_path("dinodb_buffer_free.db");
    cleanup(path);
    DiskManager disk(path.string());
    BufferManager buffer(3, &disk);

    EXPECT_EQ(buffer.free_frames(), 3u);
    page_id_t page_id = INVALID_PAGE_ID;
    buffer.new_page(page_id);
    EXPECT_EQ(buffer.free_frames(), 2u);
    cleanup(path);
}

TEST(BufferManagerTest, MultiplesPaginasEnPool) {
    auto path = test_path("dinodb_buffer_many.db");
    cleanup(path);
    DiskManager disk(path.string());
    BufferManager buffer(4, &disk);

    for (int i = 0; i < 4; ++i) {
        page_id_t page_id = INVALID_PAGE_ID;
        buffer.new_page(page_id);
        EXPECT_TRUE(buffer.in_pool(page_id));
    }

    EXPECT_EQ(buffer.free_frames(), 0u);
    cleanup(path);
}

TEST(BufferManagerTest, DesalojoLRUPoolLleno) {
    auto path = test_path("dinodb_buffer_lru.db");
    cleanup(path);
    DiskManager disk(path.string());
    BufferManager buffer(2, &disk);

    page_id_t first = INVALID_PAGE_ID;
    page_id_t second = INVALID_PAGE_ID;
    buffer.new_page(first);
    buffer.unpin_page(first, true);
    buffer.new_page(second);
    buffer.unpin_page(second, true);

    page_id_t third = INVALID_PAGE_ID;
    buffer.new_page(third);

    EXPECT_FALSE(buffer.in_pool(first));
    EXPECT_TRUE(buffer.in_pool(second));
    EXPECT_TRUE(buffer.in_pool(third));
    cleanup(path);
}

TEST(BufferManagerTest, PoolLlenoSinVictimas) {
    auto path = test_path("dinodb_buffer_full.db");
    cleanup(path);
    DiskManager disk(path.string());
    BufferManager buffer(1, &disk);

    page_id_t first = INVALID_PAGE_ID;
    buffer.new_page(first);

    page_id_t second = INVALID_PAGE_ID;
    EXPECT_THROW(buffer.new_page(second), std::runtime_error);
    cleanup(path);
}

TEST(BufferManagerTest, FlushPageNoFalla) {
    auto path = test_path("dinodb_buffer_flush_page.db");
    cleanup(path);
    DiskManager disk(path.string());
    BufferManager buffer(1, &disk);

    page_id_t page_id = INVALID_PAGE_ID;
    Page* page = buffer.new_page(page_id);
    page->insert("flush", 5);
    buffer.unpin_page(page_id, true);

    EXPECT_TRUE(buffer.flush_page(page_id));
    cleanup(path);
}

TEST(BufferManagerTest, FlushAllNoFalla) {
    auto path = test_path("dinodb_buffer_flush_all.db");
    cleanup(path);
    DiskManager disk(path.string());
    BufferManager buffer(2, &disk);

    page_id_t first = INVALID_PAGE_ID;
    page_id_t second = INVALID_PAGE_ID;
    buffer.new_page(first)->insert("uno", 3);
    buffer.unpin_page(first, true);
    buffer.new_page(second)->insert("dos", 3);
    buffer.unpin_page(second, true);

    EXPECT_NO_THROW(buffer.flush_all());
    cleanup(path);
}

TEST(BufferManagerTest, DeletePaginaLibera) {
    auto path = test_path("dinodb_buffer_delete.db");
    cleanup(path);
    DiskManager disk(path.string());
    BufferManager buffer(1, &disk);

    page_id_t page_id = INVALID_PAGE_ID;
    buffer.new_page(page_id);
    buffer.unpin_page(page_id, false);

    EXPECT_TRUE(buffer.delete_page(page_id));
    EXPECT_FALSE(buffer.in_pool(page_id));
    EXPECT_EQ(buffer.free_frames(), 1u);
    cleanup(path);
}

TEST(BufferManagerTest, UnpinNoEnPoolLanza) {
    auto path = test_path("dinodb_buffer_unpin_missing.db");
    cleanup(path);
    DiskManager disk(path.string());
    BufferManager buffer(1, &disk);

    EXPECT_THROW(buffer.unpin_page(99, false), std::runtime_error);
    cleanup(path);
}
