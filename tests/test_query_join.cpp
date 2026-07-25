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

TEST(QueryWeek12Test, NestedLoopJoinTransportaValoresTipados) {
    Table events {
        Tuple {{ 1, Value::text("aula-a"), Value::date("2026-07-24") }},
        Tuple {{ 2, Value::text("aula-b"), Value::date("2026-07-25") }}
    };
    Table rooms {
        Tuple {{ Value::text("aula-b"), Value::hour("09:30") }}
    };

    auto left = std::make_unique<SeqScan>(events);
    auto right = std::make_unique<SeqScan>(rooms);
    NestedLoopJoin join(std::move(left), std::move(right), [](const Tuple& l, const Tuple& r) {
        return l.value(1) == r.value(0);
    });

    auto rows = collect(join);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].get(0), 2);
    EXPECT_EQ(rows[0].value(1).as_text(), "aula-b");
    EXPECT_EQ(rows[0].value(2).to_string(), "2026-07-25");
    EXPECT_EQ(rows[0].value(3).as_text(), "aula-b");
    EXPECT_EQ(rows[0].value(4).to_string(), "09:30:00");
}
