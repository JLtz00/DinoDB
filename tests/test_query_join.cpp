#include <gtest/gtest.h>
#include "query/nested_loop_join.hpp"
#include "query/seq_scan.hpp"
#include <memory>

namespace {

std::vector<Tuple> collect(Operator& op) {
    std::vector<Tuple> rows;
    op.open();
    while (auto tuple = op.next()) {
        rows.push_back(*tuple);
    }
    op.close();
    return rows;
}

} // namespace

TEST(QueryWeek12Test, NestedLoopJoin) {
    Table employees {
        Tuple {{ 1, 10 }},
        Tuple {{ 2, 20 }},
        Tuple {{ 3, 10 }}
    };
    Table departments {
        Tuple {{ 10, 1000 }},
        Tuple {{ 30, 3000 }}
    };

    auto left = std::make_unique<SeqScan>(employees);
    auto right = std::make_unique<SeqScan>(departments);
    NestedLoopJoin join(std::move(left), std::move(right), [](const Tuple& l, const Tuple& r) {
        return l.get(1) == r.get(0);
    });

    auto rows = collect(join);

    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].values, std::vector<int32_t>({ 1, 10, 10, 1000 }));
    EXPECT_EQ(rows[1].values, std::vector<int32_t>({ 3, 10, 10, 1000 }));
}
