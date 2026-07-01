#include <gtest/gtest.h>
#include "query/projection.hpp"
#include "query/selection.hpp"
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

TEST(QueryWeek11Test, SelectionProjectionVolcano) {
    Table table {
        Tuple {{ 1, 10, 100 }},
        Tuple {{ 2, 20, 200 }},
        Tuple {{ 3, 30, 300 }}
    };

    auto scan = std::make_unique<SeqScan>(table);
    auto select = std::make_unique<Selection>(std::move(scan), [](const Tuple& row) {
        return row.get(1) >= 20;
    });
    Projection project(std::move(select), { 0, 2 });

    auto rows = collect(project);

    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].values, std::vector<int32_t>({ 2, 200 }));
    EXPECT_EQ(rows[1].values, std::vector<int32_t>({ 3, 300 }));
}
