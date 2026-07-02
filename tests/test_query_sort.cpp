#include <gtest/gtest.h>
#include "query/external_merge_sort.hpp"
#include <filesystem>

TEST(QueryWeek13Test, ExternalMergeSortOrdenaPorColumna) {
    Table input {
        Tuple {{ 1, 30 }},
        Tuple {{ 2, 10 }},
        Tuple {{ 3, 20 }},
        Tuple {{ 4, 5 }}
    };

    Table sorted = ExternalMergeSort::sort(input, 1, 2);

    ASSERT_EQ(sorted.size(), 4u);
    EXPECT_EQ(sorted[0].values, std::vector<int32_t>({ 4, 5 }));
    EXPECT_EQ(sorted[1].values, std::vector<int32_t>({ 2, 10 }));
    EXPECT_EQ(sorted[2].values, std::vector<int32_t>({ 3, 20 }));
    EXPECT_EQ(sorted[3].values, std::vector<int32_t>({ 1, 30 }));
}

TEST(QueryWeek13Test, ExternalMergeSortUsaRunsTemporales) {
    auto temp_dir = std::filesystem::temp_directory_path() / "dinodb_sort_runs_test";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    Table input;
    for (int i = 20; i >= 1; --i) {
        input.push_back(Tuple {{ i, i * 2 }});
    }

    Table sorted = ExternalMergeSort::sort(input, 0, 3, temp_dir);

    ASSERT_EQ(sorted.size(), 20u);
    EXPECT_EQ(sorted.front().get(0), 1);
    EXPECT_EQ(sorted.back().get(0), 20);

    size_t leftovers = 0;
    for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
        if (entry.path().filename().string().find("dinodb_sort_run_") == 0) {
            ++leftovers;
        }
    }
    EXPECT_EQ(leftovers, 0u);
    std::filesystem::remove_all(temp_dir);
}
