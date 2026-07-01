#include <gtest/gtest.h>
#include "query/external_merge_sort.hpp"

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
