#include "query/external_merge_sort.hpp"
#include <algorithm>

Table ExternalMergeSort::sort(const Table& input, size_t key_column, size_t memory_limit_rows) {
    if (memory_limit_rows == 0) {
        memory_limit_rows = 1;
    }

    std::vector<Table> runs;
    for (size_t i = 0; i < input.size(); i += memory_limit_rows) {
        size_t end = std::min(input.size(), i + memory_limit_rows);
        Table run(input.begin() + static_cast<std::ptrdiff_t>(i), input.begin() + static_cast<std::ptrdiff_t>(end));
        std::sort(run.begin(), run.end(), [key_column](const Tuple& a, const Tuple& b) {
            return a.get(key_column) < b.get(key_column);
        });
        runs.push_back(std::move(run));
    }

    Table result;
    result.reserve(input.size());
    std::vector<size_t> cursors(runs.size(), 0);

    while (result.size() < input.size()) {
        size_t best_run = runs.size();
        for (size_t i = 0; i < runs.size(); ++i) {
            if (cursors[i] >= runs[i].size()) {
                continue;
            }
            if (best_run == runs.size() ||
                runs[i][cursors[i]].get(key_column) < runs[best_run][cursors[best_run]].get(key_column)) {
                best_run = i;
            }
        }
        result.push_back(runs[best_run][cursors[best_run]++]);
    }

    return result;
}
