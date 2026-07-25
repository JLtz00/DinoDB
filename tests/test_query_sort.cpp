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

TEST(QueryWeek13Test, ExternalMergeSortOrdenaTextDateYHour) {
    Table text_input {
        Tuple {{ Value::text("zorro"), Value::date("2026-01-03") }},
        Tuple {{ Value::text("abeja"), Value::date("2026-01-02") }},
        Tuple {{ Value::text("mono"), Value::date("2026-01-01") }}
    };
    Table text_sorted = ExternalMergeSort::sort(text_input, 0, 1);
    ASSERT_EQ(text_sorted.size(), 3u);
    EXPECT_EQ(text_sorted[0].value(0).as_text(), "abeja");
    EXPECT_EQ(text_sorted[2].value(0).as_text(), "zorro");

    Table date_sorted = ExternalMergeSort::sort(text_input, 1, 2);
    EXPECT_EQ(date_sorted[0].value(1).to_string(), "2026-01-01");
    EXPECT_EQ(date_sorted[2].value(1).to_string(), "2026-01-03");

    Table hour_input {
        Tuple {{ Value::hour("18:00") }},
        Tuple {{ Value::hour("07:30:05") }},
        Tuple {{ Value::hour("12:15") }}
    };
    Table hour_sorted = ExternalMergeSort::sort(hour_input, 0, 1);
    EXPECT_EQ(hour_sorted[0].value(0).to_string(), "07:30:05");
    EXPECT_EQ(hour_sorted[2].value(0).to_string(), "18:00:00");
}
