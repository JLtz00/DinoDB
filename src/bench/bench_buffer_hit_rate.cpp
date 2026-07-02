#include "buffer/buffer_manager.hpp"
#include <filesystem>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    size_t pages = argc > 1 ? static_cast<size_t>(std::stoul(argv[1])) : 256;
    std::vector<size_t> pool_sizes { 4, 16, 64 };

    std::filesystem::create_directories("data");
    std::filesystem::path db_path = "data/bench_buffer_hit_rate.db";
    std::filesystem::remove(db_path);

    {
        DiskManager disk(db_path.string());
        for (size_t i = 0; i < pages; ++i) {
            page_id_t page_id = disk.allocate_page();
            Page page = disk.read_page(page_id);
            int32_t value = static_cast<int32_t>(i);
            page.insert(reinterpret_cast<const char*>(&value), sizeof(value));
            disk.write_page(page_id, page);
        }
        disk.flush();
    }

    std::cout << "benchmark,pool_size,pages,hits,misses,hit_rate\n";
    for (size_t pool_size : pool_sizes) {
        DiskManager disk(db_path.string());
        BufferManager buffer(pool_size, &disk);

        for (int pass = 0; pass < 2; ++pass) {
            for (page_id_t page_id = 0; page_id < pages; ++page_id) {
                Page* page = buffer.fetch_page(page_id);
                (void)page;
                buffer.unpin_page(page_id, false);
            }
        }

        std::cout << "buffer_hit_rate," << pool_size << ',' << pages << ','
                  << buffer.cache_hits() << ',' << buffer.cache_misses() << ',' << buffer.hit_rate() << "\n";
    }

    return 0;
}
