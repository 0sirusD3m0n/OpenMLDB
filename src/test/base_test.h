/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * base_test.h
 *
 * Author: chenjing
 * Date: 2020/7/10
 *--------------------------------------------------------------------------
 **/

#ifndef SRC_TEST_BASE_TEST_H_
#define SRC_TEST_BASE_TEST_H_
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/texttable.h"
#include "case/sql_case.h"
#include "gtest/gtest.h"
#include "sdk/base.h"
#include "sdk/result_set.h"

namespace rtidb {
namespace test {
class SQLCaseTest : public ::testing::TestWithParam<fesql::sqlcase::SQLCase> {
 public:
    SQLCaseTest() {}
    virtual ~SQLCaseTest() {}

    static std::string GenRand(const std::string &prefix);
    static const std::string AutoTableName();
    static std::string FindRtidbDirPath(const std::string &dirname);
    static std::vector<fesql::sqlcase::SQLCase> InitCases(
        const std::string &yaml_path);
    static void InitCases(
        const std::string &dir_path, const std::string &yaml_path,
        std::vector<fesql::sqlcase::SQLCase> &cases);  // NOLINT

    static void PrintSchema(const fesql::vm::Schema &schema);
    static void PrintSdkSchema(const fesql::sdk::Schema &schema);

    static void CheckSchema(const fesql::vm::Schema &schema,
                            const fesql::vm::Schema &exp_schema);
    static void CheckSchema(const fesql::vm::Schema &schema,
                            const fesql::sdk::Schema &exp_schema);
    static void PrintRows(const fesql::vm::Schema &schema,
                          const std::vector<fesql::codec::Row> &rows);
    static const std::vector<fesql::codec::Row> SortRows(
        const fesql::vm::Schema &schema,
        const std::vector<fesql::codec::Row> &rows,
        const std::string &order_col);
    static void CheckRow(fesql::codec::RowView &row_view,  // NOLINT
                         std::shared_ptr<fesql::sdk::ResultSet> rs);
    static void CheckRows(const fesql::vm::Schema &schema,
                          const std::string &order_col,
                          const std::vector<fesql::codec::Row> &rows,
                          std::shared_ptr<fesql::sdk::ResultSet> rs);
    static void CheckRows(const fesql::vm::Schema &schema,
                          const std::vector<fesql::codec::Row> &rows,
                          const std::vector<fesql::codec::Row> &exp_rows);
    static void CheckRows(
        const fesql::vm::Schema &schema, const std::string &order_col,
        const std::vector<fesql::codec::Row> &rows,
        std::vector<std::shared_ptr<fesql::sdk::ResultSet>> results);

    static void PrintResultSet(std::shared_ptr<fesql::sdk::ResultSet> rs);
    static void PrintResultSet(
        std::vector<std::shared_ptr<fesql::sdk::ResultSet>> results);
    static bool IsNaN(float x) { return x != x; }
    static bool IsNaN(double x) { return x != x; }
};

}  // namespace test
}  // namespace rtidb
#endif  // SRC_TEST_BASE_TEST_H_
