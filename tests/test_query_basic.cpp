#include <gtest/gtest.h>
#include "query/projection.hpp"
#include "query/selection.hpp"
#include "query/seq_scan.hpp"
#include "query/persistent_table.hpp"
#include "storage/disk_manager.hpp"
#include <filesystem>
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

TEST(QueryWeek11Test, SeqScanLeeDesdePaginasPersistentes) {
    auto path = std::filesystem::temp_directory_path() / "dinodb_seqscan_persistent.db";
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }

    DiskManager disk(path.string());
    PersistentTable table(&disk);
    table.insert(Tuple {{ 1, 10 }});
    table.insert(Tuple {{ 2, 20 }});
    table.insert(Tuple {{ 3, 30 }});
    disk.flush();

    SeqScan scan(table);
    auto rows = collect(scan);

    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[0].values, std::vector<int32_t>({ 1, 10 }));
    EXPECT_EQ(rows[2].values, std::vector<int32_t>({ 3, 30 }));

    std::filesystem::remove(path);
}
