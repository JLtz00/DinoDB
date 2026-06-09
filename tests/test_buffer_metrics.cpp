#include <gtest/gtest.h>
#include "buffer/buffer_manager.hpp"
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

TEST(BufferMetricsWeek09Test, CalculaHitRate) {
    auto path = test_path("dinodb_week09_buffer_metrics.db");
    cleanup(path);

    DiskManager disk(path.string());
    page_id_t page_id = disk.allocate_page();
    BufferManager buffer(2, &disk);

    buffer.fetch_page(page_id);
    buffer.unpin_page(page_id, false);
    buffer.fetch_page(page_id);
    buffer.unpin_page(page_id, false);

    EXPECT_EQ(buffer.cache_misses(), 1u);
    EXPECT_EQ(buffer.cache_hits(), 1u);
    EXPECT_DOUBLE_EQ(buffer.hit_rate(), 0.5);

    cleanup(path);
}

TEST(BufferMetricsWeek09Test, ReiniciaMetricas) {
    auto path = test_path("dinodb_week09_buffer_metrics_reset.db");
    cleanup(path);

    DiskManager disk(path.string());
    page_id_t page_id = disk.allocate_page();
    BufferManager buffer(2, &disk);

    buffer.fetch_page(page_id);
    buffer.unpin_page(page_id, false);
    buffer.fetch_page(page_id);
    buffer.unpin_page(page_id, false);
    buffer.reset_metrics();

    EXPECT_EQ(buffer.cache_misses(), 0u);
    EXPECT_EQ(buffer.cache_hits(), 0u);
    EXPECT_DOUBLE_EQ(buffer.hit_rate(), 0.0);

    cleanup(path);
}
