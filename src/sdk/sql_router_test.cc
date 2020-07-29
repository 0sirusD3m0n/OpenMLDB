/*
 * sql_router_test.cc
 * Copyright (C) 4paradigm.com 2020 wangtaize <wangtaize@4paradigm.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sdk/sql_router.h"

#include <sched.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/glog_wapper.h"  // NOLINT
#include "catalog/schema_adapter.h"
#include "codec/fe_row_codec.h"
#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "sdk/mini_cluster.h"
#include "timer.h"  // NOLINT
#include "vm/catalog.h"

namespace rtidb {
namespace sdk {

typedef ::google::protobuf::RepeatedPtrField<::rtidb::common::ColumnDesc>
    RtiDBSchema;
typedef ::google::protobuf::RepeatedPtrField<::rtidb::common::ColumnKey>
    RtiDBIndex;

::rtidb::sdk::MiniCluster* mc_;

inline std::string GenRand() {
    return std::to_string(rand() % 10000000 + 1);  // NOLINT
}

class SQLRouterTest : public ::testing::Test {
 public:
    SQLRouterTest() {}
    ~SQLRouterTest() {}
    void SetUp() {}
    void TearDown() {}
};

TEST_F(SQLRouterTest, empty_db_test) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    ::fesql::sdk::Status status;
    ASSERT_FALSE(router->CreateDB("", &status));
}

TEST_F(SQLRouterTest, smoketest) {
    ::rtidb::nameserver::TableInfo table_info;
    table_info.set_format_version(1);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    auto ns_client = mc_->GetNsClient();
    std::string error;
    bool ok = ns_client->CreateDatabase(db, error);
    ASSERT_TRUE(ok);
    table_info.set_name(name);
    table_info.set_db(db);
    table_info.set_partition_num(1);
    RtiDBSchema* schema = table_info.mutable_column_desc_v1();
    auto col1 = schema->Add();
    col1->set_name("col1");
    col1->set_data_type(::rtidb::type::kVarchar);
    col1->set_type("string");
    auto col2 = schema->Add();
    col2->set_name("col2");
    col2->set_data_type(::rtidb::type::kBigInt);
    col2->set_type("int64");
    col2->set_is_ts_col(true);
    RtiDBIndex* index = table_info.mutable_column_key();
    auto key1 = index->Add();
    key1->set_index_name("index0");
    key1->add_col_name("col1");
    key1->add_ts_name("col2");
    ok = ns_client->CreateTable(table_info, error);

    ::fesql::vm::Schema fe_schema;
    ::rtidb::catalog::SchemaAdapter::ConvertSchema(table_info.column_desc_v1(),
                                                   &fe_schema);
    ::fesql::codec::RowBuilder rb(fe_schema);
    std::string pk = "pk1";
    uint64_t ts = 1589780888000l;
    uint32_t size = rb.CalTotalLength(pk.size());
    std::string value;
    value.resize(size);
    rb.SetBuffer(reinterpret_cast<int8_t*>(&(value[0])), size);
    rb.AppendString(pk.c_str(), pk.size());
    rb.AppendInt64(ts);

    ASSERT_TRUE(ok);
    ClusterOptions option;
    option.zk_cluster = mc_->GetZkCluster();
    option.zk_path = mc_->GetZkPath();

    ClusterSDK sdk(option);
    ASSERT_TRUE(sdk.Init());
    std::vector<std::shared_ptr<::rtidb::client::TabletClient>> tablet;
    ok = sdk.GetTabletByTable(db, name, &tablet);
    ASSERT_TRUE(ok);
    ASSERT_EQ(1, tablet.size());
    uint32_t tid = sdk.GetTableId(db, name);
    ASSERT_NE(tid, 0);
    ok = tablet[0]->Put(tid, 0, pk, ts, value, 1);
    ASSERT_TRUE(ok);

    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string sql = "select col1, col2 + 1 from " + name + " ;";
    ::fesql::sdk::Status status;
    auto rs = router->ExecuteSQL(db, sql, &status);
    if (!rs) ASSERT_TRUE(false);
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ(1, rs->Size());
    ASSERT_EQ(2, rs->GetSchema()->GetColumnCnt());
    ASSERT_FALSE(rs->IsNULL(0));
    ASSERT_FALSE(rs->IsNULL(1));
    ASSERT_EQ(ts + 1, rs->GetInt64Unsafe(1));
    ASSERT_EQ(pk, rs->GetStringUnsafe(0));
}

TEST_F(SQLRouterTest, db_api_test) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::vector<std::string> dbs;
    ASSERT_TRUE(router->ShowDB(&dbs, &status));
    ASSERT_EQ(1u, dbs.size());
    ASSERT_EQ(db, dbs[0]);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, create_and_drop_table_test) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl = "create table " + name +
                      "("
                      "col1 string, col2 bigint,"
                      "index(key=col1, ts=col2));";
    std::string insert = "insert into " + name + " values('hello', 1590);";
    std::string select = "select * from " + name + ";";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());

    ok = router->ExecuteInsert(db, insert, &status);

    auto rs = router->ExecuteSQL(db, select, &status);
    ASSERT_TRUE(rs != nullptr);
    ASSERT_EQ(1, rs->Size());

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);

    std::string ddl_fake = "create table " + name +
                           "("
                           "col1 int, col2 bigint,"
                           "index(key=col1, ts=col2));";

    ok = router->ExecuteDDL(db, ddl_fake, &status);
    ASSERT_TRUE(ok);

    ASSERT_TRUE(router->RefreshCatalog());

    rs = router->ExecuteSQL(db, select, &status);
    ASSERT_TRUE(rs != nullptr);
    ASSERT_EQ(0, rs->Size());
    // db still has table, drop fail
    ok = router->DropDB(db, &status);
    ASSERT_FALSE(ok);

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, test_sql_insert_placeholder) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl = "create table " + name +
                      "("
                      "col1 string, col2 bigint,"
                      "index(key=col1, ts=col2));";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());

    std::string insert = "insert into " + name + " values('hello', 1590);";
    std::string insert_placeholder1 = "insert into " + name + " values(?, ?);";
    std::string insert_placeholder2 =
        "insert into " + name + " values(?, 1592);";
    std::string insert_placeholder3 =
        "insert into " + name + " values('hi', ?);";

    ok = router->ExecuteInsert(db, insert, &status);
    ASSERT_TRUE(ok);

    std::shared_ptr<SQLInsertRow> insert_row1 =
        router->GetInsertRow(db, insert_placeholder1, &status);
    ASSERT_EQ(status.code, 0);
    ASSERT_TRUE(insert_row1->Init(5));
    ASSERT_TRUE(insert_row1->AppendString("world"));
    ASSERT_TRUE(insert_row1->AppendInt64(1591));
    ok = router->ExecuteInsert(db, insert_placeholder1, insert_row1, &status);
    ASSERT_TRUE(ok);

    std::shared_ptr<SQLInsertRow> insert_row2 =
        router->GetInsertRow(db, insert_placeholder2, &status);
    ASSERT_EQ(status.code, 0);
    ASSERT_TRUE(insert_row2->Init(4));
    ASSERT_TRUE(insert_row2->AppendString("word"));
    ok = router->ExecuteInsert(db, insert_placeholder2, insert_row2, &status);
    ASSERT_TRUE(ok);

    std::shared_ptr<SQLInsertRow> insert_row3 =
        router->GetInsertRow(db, insert_placeholder3, &status);
    ASSERT_EQ(status.code, 0);
    ASSERT_TRUE(insert_row3->Init(0));
    ASSERT_TRUE(insert_row3->AppendInt64(1593));
    ok = router->ExecuteInsert(db, insert_placeholder3, insert_row3, &status);
    ASSERT_TRUE(ok);

    std::shared_ptr<SQLInsertRows> insert_rows1 =
        router->GetInsertRows(db, insert_placeholder1, &status);
    ASSERT_EQ(status.code, 0);
    std::shared_ptr<SQLInsertRow> insert_rows1_1 = insert_rows1->NewRow();
    ASSERT_TRUE(insert_rows1_1->Init(2));
    ASSERT_TRUE(insert_rows1_1->AppendString("11"));
    ASSERT_TRUE(insert_rows1_1->AppendInt64(1594));
    std::shared_ptr<SQLInsertRow> insert_rows1_2 = insert_rows1->NewRow();
    ASSERT_TRUE(insert_rows1_2->Init(2));
    ASSERT_TRUE(insert_rows1_2->AppendString("12"));
    ASSERT_TRUE(insert_rows1_2->AppendInt64(1595));
    ok = router->ExecuteInsert(db, insert_placeholder1, insert_rows1, &status);
    ASSERT_TRUE(ok);

    std::shared_ptr<SQLInsertRows> insert_rows2 =
        router->GetInsertRows(db, insert_placeholder2, &status);
    ASSERT_EQ(status.code, 0);
    std::shared_ptr<SQLInsertRow> insert_rows2_1 = insert_rows2->NewRow();
    ASSERT_TRUE(insert_rows2_1->Init(2));
    ASSERT_TRUE(insert_rows2_1->AppendString("21"));
    std::shared_ptr<SQLInsertRow> insert_rows2_2 = insert_rows2->NewRow();
    ASSERT_TRUE(insert_rows2_2->Init(2));
    ASSERT_TRUE(insert_rows2_2->AppendString("22"));
    ok = router->ExecuteInsert(db, insert_placeholder2, insert_rows2, &status);
    ASSERT_TRUE(ok);

    std::shared_ptr<SQLInsertRows> insert_rows3 =
        router->GetInsertRows(db, insert_placeholder3, &status);
    ASSERT_EQ(status.code, 0);
    std::shared_ptr<SQLInsertRow> insert_rows3_1 = insert_rows3->NewRow();
    ASSERT_TRUE(insert_rows3_1->Init(0));
    ASSERT_TRUE(insert_rows3_1->AppendInt64(1596));
    std::shared_ptr<SQLInsertRow> insert_rows3_2 = insert_rows3->NewRow();
    ASSERT_TRUE(insert_rows3_2->Init(0));
    ASSERT_TRUE(insert_rows3_2->AppendInt64(1597));
    ok = router->ExecuteInsert(db, insert_placeholder3, insert_rows3, &status);
    ASSERT_TRUE(ok);

    ASSERT_TRUE(router->RefreshCatalog());
    std::string sql_select = "select col1, col2 from " + name + ";";
    auto rs = router->ExecuteSQL(db, sql_select, &status);
    if (!rs) ASSERT_TRUE(false);
    ASSERT_EQ(10, rs->Size());
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    ASSERT_EQ(1590, rs->GetInt64Unsafe(1));
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("world", rs->GetStringUnsafe(0));
    ASSERT_EQ(1591, rs->GetInt64Unsafe(1));
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("22", rs->GetStringUnsafe(0));
    ASSERT_EQ(1592, rs->GetInt64Unsafe(1));
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("11", rs->GetStringUnsafe(0));
    ASSERT_EQ(1594, rs->GetInt64Unsafe(1));
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hi", rs->GetStringUnsafe(0));
    ASSERT_EQ(1597, rs->GetInt64Unsafe(1));
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hi", rs->GetStringUnsafe(0));
    ASSERT_EQ(1596, rs->GetInt64Unsafe(1));
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hi", rs->GetStringUnsafe(0));
    ASSERT_EQ(1593, rs->GetInt64Unsafe(1));
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("12", rs->GetStringUnsafe(0));
    ASSERT_EQ(1595, rs->GetInt64Unsafe(1));
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("21", rs->GetStringUnsafe(0));
    ASSERT_EQ(1592, rs->GetInt64Unsafe(1));
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("word", rs->GetStringUnsafe(0));
    ASSERT_EQ(1592, rs->GetInt64Unsafe(1));
    ASSERT_FALSE(rs->Next());

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, test_sql_insert_with_column_list) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl = "create table " + name +
                      "("
                      "col1 int, col2 int, col3 string NOT NULL, col4 "
                      "bigint NOT NULL, index(key=col3, ts=col4));";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());

    // normal insert
    std::string insert1 =
        "insert into " + name + "(col3, col4) values('hello', 1000);";
    ok = router->ExecuteInsert(db, insert1, &status);
    ASSERT_TRUE(ok);

    // col3 shouldn't be null
    std::string insert2 = "insert into " + name + "(col4) values(1000);";
    ok = router->ExecuteInsert(db, insert2, &status);
    ASSERT_FALSE(ok);

    // col5 not exist
    std::string insert3 = "insert into " + name + "(col5) values(1000);";
    ok = router->ExecuteInsert(db, insert3, &status);
    ASSERT_FALSE(ok);

    // duplicate col4
    std::string insert4 =
        "insert into " + name + "(col4, col4) values(1000, 1000);";
    ok = router->ExecuteInsert(db, insert4, &status);
    ASSERT_FALSE(ok);

    // normal placeholder insert
    std::string insert5 =
        "insert into " + name + "(col2, col3, col4) values(?, 'hello', ?);";
    std::shared_ptr<SQLInsertRow> r5 =
        router->GetInsertRow(db, insert5, &status);
    ASSERT_TRUE(r5->Init(0));
    ASSERT_TRUE(r5->AppendInt32(123));
    ASSERT_TRUE(r5->AppendInt64(1001));
    ok = router->ExecuteInsert(db, insert5, r5, &status);
    ASSERT_TRUE(ok);

    // todo: if placeholders are out of order. eg: insert into [table] (col4,
    // col3, col2) value (?, 'hello', ?);

    std::string select = "select * from " + name + ";";
    auto rs = router->ExecuteSQL(db, select, &status);
    ASSERT_EQ(2, rs->Size());

    ASSERT_TRUE(rs->Next());
    ASSERT_TRUE(rs->IsNULL(0));
    ASSERT_EQ(123, rs->GetInt32Unsafe(1));
    ASSERT_EQ("hello", rs->GetStringUnsafe(2));
    ASSERT_EQ(1001, rs->GetInt64Unsafe(3));

    ASSERT_TRUE(rs->Next());
    ASSERT_TRUE(rs->IsNULL(0));
    ASSERT_TRUE(rs->IsNULL(1));
    ASSERT_EQ("hello", rs->GetStringUnsafe(2));
    ASSERT_EQ(1000, rs->GetInt64Unsafe(3));

    ASSERT_FALSE(rs->Next());

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, test_sql_insert_placeholder_with_date_column_key) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl = "create table " + name +
                      "("
                      "col1 int, col2 date NOT NULL, col3 "
                      "bigint NOT NULL, index(key=col2, ts=col3));";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());

    std::string insert1 = "insert into " + name + " values(?, ?, ?);";
    std::shared_ptr<SQLInsertRow> r1 =
        router->GetInsertRow(db, insert1, &status);
    ASSERT_TRUE(r1->Init(0));
    ASSERT_TRUE(r1->AppendInt32(123));
    ASSERT_TRUE(r1->AppendDate(2020, 7, 22));
    ASSERT_TRUE(r1->AppendInt64(1000));
    ok = router->ExecuteInsert(db, insert1, r1, &status);
    ASSERT_TRUE(ok);
    std::string select = "select * from " + name + ";";
    auto rs = router->ExecuteSQL(db, select, &status);
    ASSERT_EQ(1, rs->Size());
    int32_t year;
    int32_t month;
    int32_t day;

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ(123, rs->GetInt32Unsafe(0));
    ASSERT_TRUE(rs->GetDate(1, &year, &month, &day));
    ASSERT_EQ(2020, year);
    ASSERT_EQ(7, month);
    ASSERT_EQ(22, day);
    ASSERT_EQ(1000, rs->GetInt64Unsafe(2));

    ASSERT_FALSE(rs->Next());

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, test_sql_insert_placeholder_with_column_key_1) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl = "create table " + name +
                      "("
                      "col1 int, col2 int NOT NULL, col3 string NOT NULL, col4 "
                      "bigint NOT NULL, index(key=(col2, col3), ts=col4));";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());

    std::string insert1 = "insert into " + name + " values(?, ?, ?, ?);";
    std::shared_ptr<SQLInsertRow> r1 =
        router->GetInsertRow(db, insert1, &status);
    ASSERT_TRUE(r1->Init(5));
    ASSERT_TRUE(r1->AppendInt32(123));
    ASSERT_TRUE(r1->AppendInt32(321));
    ASSERT_TRUE(r1->AppendString("hello"));
    ASSERT_TRUE(r1->AppendInt64(1000));
    ok = router->ExecuteInsert(db, insert1, r1, &status);
    ASSERT_TRUE(ok);

    std::string insert2 = "insert into " + name + " values(?, ?, 'hello', ?);";
    std::shared_ptr<SQLInsertRow> r2 =
        router->GetInsertRow(db, insert2, &status);
    ASSERT_TRUE(r2->Init(0));
    ASSERT_TRUE(r2->AppendInt32(456));
    ASSERT_TRUE(r2->AppendInt32(654));
    ASSERT_TRUE(r2->AppendInt64(1001));
    ok = router->ExecuteInsert(db, insert2, r2, &status);
    ASSERT_TRUE(ok);

    std::string insert3 = "insert into " + name + " values(?, 987, ?, ?);";
    std::shared_ptr<SQLInsertRow> r3 =
        router->GetInsertRow(db, insert3, &status);
    ASSERT_TRUE(r3->Init(5));
    ASSERT_TRUE(r3->AppendInt32(789));
    ASSERT_TRUE(r3->AppendString("hello"));
    ASSERT_TRUE(r3->AppendInt64(1002));
    ok = router->ExecuteInsert(db, insert3, r3, &status);
    ASSERT_TRUE(ok);

    std::string insert4 = "insert into " + name + " values(?, 0,'hello', ?);";
    std::shared_ptr<SQLInsertRow> r4 =
        router->GetInsertRow(db, insert4, &status);
    ASSERT_TRUE(r4->Init(0));
    ASSERT_TRUE(r4->AppendInt32(1));
    ASSERT_TRUE(r4->AppendInt64(1003));
    ok = router->ExecuteInsert(db, insert4, r4, &status);
    ASSERT_TRUE(ok);

    std::string select = "select * from " + name + ";";
    auto rs = router->ExecuteSQL(db, select, &status);
    ASSERT_EQ(4, rs->Size());

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ(1, rs->GetInt32Unsafe(0));
    ASSERT_EQ(0, rs->GetInt32Unsafe(1));
    ASSERT_EQ("hello", rs->GetStringUnsafe(2));
    ASSERT_EQ(rs->GetInt64Unsafe(3), 1003);

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ(123, rs->GetInt32Unsafe(0));
    ASSERT_EQ(321, rs->GetInt32Unsafe(1));
    ASSERT_EQ("hello", rs->GetStringUnsafe(2));
    ASSERT_EQ(rs->GetInt64Unsafe(3), 1000);

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ(789, rs->GetInt32Unsafe(0));
    ASSERT_EQ(987, rs->GetInt32Unsafe(1));
    ASSERT_EQ("hello", rs->GetStringUnsafe(2));
    ASSERT_EQ(rs->GetInt64Unsafe(3), 1002);

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ(456, rs->GetInt32Unsafe(0));
    ASSERT_EQ(654, rs->GetInt32Unsafe(1));
    ASSERT_EQ("hello", rs->GetStringUnsafe(2));
    ASSERT_EQ(rs->GetInt64Unsafe(3), 1001);

    ASSERT_FALSE(rs->Next());

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, test_sql_insert_placeholder_with_column_key_2) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl =
        "create table " + name +
        "("
        "col1 string NOT NULL, col2 bigint NOT NULL, col3 date NOT NULL, col4 "
        "int NOT NULL, index(key=(col1, col4), ts=col2));";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());

    std::string insert1 = "insert into " + name + " values(?, ?, ?, ?);";
    std::shared_ptr<SQLInsertRow> r1 =
        router->GetInsertRow(db, insert1, &status);
    ASSERT_TRUE(r1->Init(5));
    ASSERT_TRUE(r1->AppendString("hello"));
    ASSERT_TRUE(r1->AppendInt64(1000));
    ASSERT_TRUE(r1->AppendDate(2020, 7, 13));
    ASSERT_TRUE(r1->AppendInt32(123));
    ok = router->ExecuteInsert(db, insert1, r1, &status);
    ASSERT_TRUE(ok);

    std::string insert2 = "insert into " + name + " values('hello', ?, ?, ?);";
    std::shared_ptr<SQLInsertRow> r2 =
        router->GetInsertRow(db, insert2, &status);
    ASSERT_TRUE(r2->Init(0));
    ASSERT_TRUE(r2->AppendInt64(1001));
    ASSERT_TRUE(r2->AppendDate(2020, 7, 20));
    ASSERT_TRUE(r2->AppendInt32(456));
    ok = router->ExecuteInsert(db, insert2, r2, &status);
    ASSERT_TRUE(ok);

    std::string insert3 = "insert into " + name + " values(?, ?, ?, 789);";
    std::shared_ptr<SQLInsertRow> r3 =
        router->GetInsertRow(db, insert3, &status);
    ASSERT_TRUE(r3->Init(5));
    ASSERT_TRUE(r3->AppendString("hello"));
    ASSERT_TRUE(r3->AppendInt64(1002));
    ASSERT_TRUE(r3->AppendDate(2020, 7, 22));
    ok = router->ExecuteInsert(db, insert3, r3, &status);
    ASSERT_TRUE(ok);

    std::string insert4 =
        "insert into " + name + " values('hello', ?, ?, 000);";
    std::shared_ptr<SQLInsertRow> r4 =
        router->GetInsertRow(db, insert4, &status);
    ASSERT_TRUE(r4->Init(0));
    ASSERT_TRUE(r4->AppendInt64(1003));
    ASSERT_TRUE(r4->AppendDate(2020, 7, 22));
    ok = router->ExecuteInsert(db, insert4, r4, &status);
    ASSERT_TRUE(ok);

    std::string insert5 =
        "insert into " + name + " values('hello', 1004, '2020-07-31', 001);";
    ok = router->ExecuteInsert(db, insert5, &status);
    ASSERT_TRUE(ok);

    std::string insert6 =
        "insert into " + name + " values('hello', 1004, '2020-07-31', ?);";
    ok = router->ExecuteInsert(db, insert6, &status);
    ASSERT_FALSE(ok);

    int32_t year;
    int32_t month;
    int32_t day;
    std::string select = "select * from " + name + ";";
    auto rs = router->ExecuteSQL(db, select, &status);
    ASSERT_EQ(5, rs->Size());

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    ASSERT_EQ(rs->GetInt64Unsafe(1), 1001);
    ASSERT_TRUE(rs->GetDate(2, &year, &month, &day));
    ASSERT_EQ(year, 2020);
    ASSERT_EQ(month, 7);
    ASSERT_EQ(day, 20);
    ASSERT_EQ(456, rs->GetInt32Unsafe(3));

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    ASSERT_EQ(rs->GetInt64Unsafe(1), 1003);
    ASSERT_TRUE(rs->GetDate(2, &year, &month, &day));
    ASSERT_EQ(year, 2020);
    ASSERT_EQ(month, 7);
    ASSERT_EQ(day, 22);
    ASSERT_EQ(0, rs->GetInt32Unsafe(3));

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    ASSERT_EQ(rs->GetInt64Unsafe(1), 1002);
    ASSERT_TRUE(rs->GetDate(2, &year, &month, &day));
    ASSERT_EQ(year, 2020);
    ASSERT_EQ(month, 7);
    ASSERT_EQ(day, 22);
    ASSERT_EQ(789, rs->GetInt32Unsafe(3));

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    ASSERT_EQ(rs->GetInt64Unsafe(1), 1000);
    ASSERT_TRUE(rs->GetDate(2, &year, &month, &day));
    ASSERT_EQ(year, 2020);
    ASSERT_EQ(month, 7);
    ASSERT_EQ(day, 13);
    ASSERT_EQ(123, rs->GetInt32Unsafe(3));

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    ASSERT_EQ(rs->GetInt64Unsafe(1), 1004);
    ASSERT_TRUE(rs->GetDate(2, &year, &month, &day));
    ASSERT_EQ(year, 2020);
    ASSERT_EQ(month, 7);
    ASSERT_EQ(day, 31);
    ASSERT_EQ(1, rs->GetInt32Unsafe(3));

    ASSERT_FALSE(rs->Next());

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, test_sql_insert_placeholder_with_type_check) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl =
        "create table " + name +
        "("
        "col1 string NOT NULL, col2 bigint NOT NULL, col3 date NOT NULL, col4 "
        "int, col5 smallint, col6 float, col7 double,"
        "index(key=col1, ts=col2));";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());

    // test null
    std::string insert1 =
        "insert into " + name + " values(?, ?, ?, ?, ?, ?, ?);";
    std::shared_ptr<SQLInsertRow> r1 =
        router->GetInsertRow(db, insert1, &status);

    // test schema
    std::shared_ptr<fesql::sdk::Schema> schema = r1->GetSchema();
    ASSERT_EQ(schema->GetColumnCnt(), 7);
    ASSERT_EQ(schema->GetColumnName(0), "col1");
    ASSERT_EQ(schema->GetColumnType(0), fesql::sdk::kTypeString);
    ASSERT_EQ(schema->GetColumnName(1), "col2");
    ASSERT_EQ(schema->GetColumnType(1), fesql::sdk::kTypeInt64);
    ASSERT_EQ(schema->GetColumnName(2), "col3");
    ASSERT_EQ(schema->GetColumnType(2), fesql::sdk::kTypeDate);
    ASSERT_EQ(schema->GetColumnName(3), "col4");
    ASSERT_EQ(schema->GetColumnType(3), fesql::sdk::kTypeInt32);
    ASSERT_EQ(schema->GetColumnName(4), "col5");
    ASSERT_EQ(schema->GetColumnType(4), fesql::sdk::kTypeInt16);
    ASSERT_EQ(schema->GetColumnName(5), "col6");
    ASSERT_EQ(schema->GetColumnType(5), fesql::sdk::kTypeFloat);
    ASSERT_EQ(schema->GetColumnName(6), "col7");
    ASSERT_EQ(schema->GetColumnType(6), fesql::sdk::kTypeDouble);

    ASSERT_TRUE(r1->Init(5));
    ASSERT_TRUE(r1->AppendString("hello"));
    ASSERT_TRUE(r1->AppendInt64(1000));
    ASSERT_FALSE(r1->AppendNULL());
    ASSERT_TRUE(r1->AppendDate(2020, 7, 13));
    ASSERT_TRUE(r1->AppendNULL());
    // appendnull automatically
    ok = router->ExecuteInsert(db, insert1, r1, &status);
    ASSERT_TRUE(ok);

    // test int convert and float convert
    std::string insert2 =
        "insert into " + name +
        " value('hello', ?, '2020-02-29', NULL, 123, 2.33, NULL);";
    std::shared_ptr<SQLInsertRow> r2 =
        router->GetInsertRow(db, insert2, &status);
    ASSERT_EQ(status.code, 0);
    ASSERT_TRUE(r2->Init(0));
    ASSERT_TRUE(r2->AppendInt64(1001));
    ok = router->ExecuteInsert(db, insert2, r2, &status);
    ASSERT_TRUE(ok);

    // test int to float
    std::string insert3 =
        "insert into " + name +
        " value('hello', ?, '2020-12-31', NULL, NULL, 123, 123);";
    std::shared_ptr<SQLInsertRow> r3 =
        router->GetInsertRow(db, insert3, &status);
    ASSERT_EQ(status.code, 0);
    ASSERT_TRUE(r3->Init(0));
    ASSERT_TRUE(r3->AppendInt64(1002));
    ok = router->ExecuteInsert(db, insert3, r3, &status);
    ASSERT_TRUE(ok);

    // test float to int
    std::string insert4 =
        "insert into " + name +
        " value('hello', ?, '2020-02-29', 2.33, 2.33, 123, 123);";
    std::shared_ptr<SQLInsertRow> r4 =
        router->GetInsertRow(db, insert4, &status);
    ASSERT_EQ(status.code, 1);

    int32_t year;
    int32_t month;
    int32_t day;
    std::string select = "select * from " + name + ";";
    auto rs = router->ExecuteSQL(db, select, &status);
    ASSERT_EQ(3, rs->Size());

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    ASSERT_EQ(rs->GetInt64Unsafe(1), 1002);
    ASSERT_TRUE(rs->GetDate(2, &year, &month, &day));
    ASSERT_EQ(year, 2020);
    ASSERT_EQ(month, 12);
    ASSERT_EQ(day, 31);
    ASSERT_TRUE(rs->IsNULL(3));
    ASSERT_TRUE(rs->IsNULL(4));
    ASSERT_FLOAT_EQ(rs->GetFloatUnsafe(5), 123.0);
    ASSERT_DOUBLE_EQ(rs->GetDoubleUnsafe(6), 123.0);

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    ASSERT_EQ(rs->GetInt64Unsafe(1), 1001);
    ASSERT_TRUE(rs->GetDate(2, &year, &month, &day));
    ASSERT_EQ(year, 2020);
    ASSERT_EQ(month, 2);
    ASSERT_EQ(day, 29);
    ASSERT_TRUE(rs->IsNULL(3));
    ASSERT_EQ(rs->GetInt16Unsafe(4), 123);
    ASSERT_FLOAT_EQ(rs->GetFloatUnsafe(5), 2.33);
    ASSERT_TRUE(rs->IsNULL(6));

    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    ASSERT_EQ(rs->GetInt64Unsafe(1), 1000);
    ASSERT_TRUE(rs->GetDate(2, &year, &month, &day));
    ASSERT_EQ(year, 2020);
    ASSERT_EQ(month, 7);
    ASSERT_EQ(day, 13);
    ASSERT_TRUE(rs->IsNULL(3));
    ASSERT_TRUE(rs->IsNULL(4));
    ASSERT_TRUE(rs->IsNULL(5));
    ASSERT_TRUE(rs->IsNULL(6));

    ASSERT_FALSE(rs->Next());

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, smoketest_on_sql) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl = "create table " + name +
                      "("
                      "col1 string, col2 bigint,"
                      "index(key=col1, ts=col2));";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());
    std::string insert = "insert into " + name + " values('hello', 1590);";
    ok = router->ExecuteInsert(db, insert, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());
    std::string sql_select = "select col1 from " + name + " ;";
    auto rs = router->ExecuteSQL(db, sql_select, &status);
    if (!rs) ASSERT_TRUE(false);
    ASSERT_EQ(1, rs->Size());
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    std::string sql_window_batch =
        "select sum(col2) over w from " + name + " window w as (partition by " +
        name + ".col1 order by " + name +
        ".col2 ROWS BETWEEN 3 PRECEDING AND CURRENT ROW);";
    rs = router->ExecuteSQL(db, sql_window_batch, &status);
    ASSERT_EQ(1, rs->Size());
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ(1590, rs->GetInt64Unsafe(0));
    {
        std::shared_ptr<SQLRequestRow> row =
            router->GetRequestRow(db, sql_window_batch, &status);
        if (!row) ASSERT_FALSE(true);
        ASSERT_EQ(2, row->GetSchema()->GetColumnCnt());
        ASSERT_TRUE(row->Init(5));
        ASSERT_TRUE(row->AppendString("hello"));
        ASSERT_TRUE(row->AppendInt64(100));
        ASSERT_TRUE(row->Build());

        std::string sql_window_request =
            "select sum(col2)  over w as sum_col2 from " + name +
            " window w as (partition by " + name + ".col1 order by " + name +
            ".col2 ROWS BETWEEN 3 PRECEDING AND CURRENT ROW);";

        rs = router->ExecuteSQL(db, sql_window_request, row, &status);
        if (!rs) ASSERT_FALSE(true);
        ASSERT_EQ(1, rs->Size());
        ASSERT_TRUE(rs->Next());
        ASSERT_EQ(100, rs->GetInt64Unsafe(0));
    }
    {
        std::shared_ptr<SQLRequestRow> row =
            router->GetRequestRow(db, sql_window_batch, &status);
        if (!row) ASSERT_FALSE(true);
        ASSERT_EQ(2, row->GetSchema()->GetColumnCnt());
        ASSERT_TRUE(row->Init(5));
        ASSERT_TRUE(row->AppendString("hello"));
        ASSERT_TRUE(row->AppendInt64(100));
        ASSERT_TRUE(row->Build());

        std::string sql_window_request =
            "select sum(col2)  over w as sum_col2 from " + name +
            " window w as (partition by " + name + ".col1 order by " + name +
            ".col2 ROWS BETWEEN 3 PRECEDING AND CURRENT ROW);";

        rs = router->ExecuteSQL(db, sql_window_request, row, &status);
        if (!rs) ASSERT_FALSE(true);
        ASSERT_EQ(1, rs->Size());
        ASSERT_TRUE(rs->Next());
        ASSERT_EQ(100, rs->GetInt64Unsafe(0));
    }

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, smoke_explain_on_sql) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl = "create table " + name +
                      "("
                      "col1 string, col2 timestamp, col3 date,"
                      "index(key=col1, ts=col2));";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());
    std::string insert = "insert into " + name +
                         " values('hello', 1591174600000l, '2020-06-03');";
    ok = router->ExecuteInsert(db, insert, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());
    std::string sql_select = "select * from " + name + " ;";
    auto explain = router->Explain(db, sql_select, &status);
    if (explain) {
        ASSERT_TRUE(true);
    } else {
        ASSERT_TRUE(false);
    }

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, smoke_not_null) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl = "create table " + name +
                      "("
                      "col1 string, col2 timestamp, col3 date not null,"
                      "index(key=col1, ts=col2));";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());
    std::string insert =
        "insert into " + name + " values('hello', 1591174600000l, null);";
    ok = router->ExecuteInsert(db, insert, &status);
    ASSERT_FALSE(ok);

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

TEST_F(SQLRouterTest, smoketimestamptest_on_sql) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) ASSERT_TRUE(false);
    std::string name = "test" + GenRand();
    std::string db = "db" + GenRand();
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    std::string ddl = "create table " + name +
                      "("
                      "col1 string, col2 timestamp, col3 date,"
                      "index(key=col1, ts=col2));";
    ok = router->ExecuteDDL(db, ddl, &status);
    ASSERT_TRUE(ok);

    ASSERT_TRUE(router->RefreshCatalog());
    std::string insert = "insert into " + name +
                         " values('hello', 1591174600000l, '2020-06-03');";
    ok = router->ExecuteInsert(db, insert, &status);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(router->RefreshCatalog());
    std::string sql_select = "select * from " + name + " ;";
    auto rs = router->ExecuteSQL(db, sql_select, &status);
    if (!rs) ASSERT_TRUE(false);
    ASSERT_EQ(1, rs->Size());
    ASSERT_EQ(3, rs->GetSchema()->GetColumnCnt());
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ("hello", rs->GetStringUnsafe(0));
    ASSERT_EQ(1591174600000l, rs->GetTimeUnsafe(1));
    int32_t year = 0;
    int32_t month = 0;
    int32_t day = 0;
    ASSERT_TRUE(rs->GetDate(2, &year, &month, &day));
    ASSERT_EQ(2020, year);
    ASSERT_EQ(6, month);
    ASSERT_EQ(3, day);
    ASSERT_FALSE(rs->Next());

    ok = router->ExecuteDDL(db, "drop table " + name + ";", &status);
    ASSERT_TRUE(ok);
    ok = router->DropDB(db, &status);
    ASSERT_TRUE(ok);
}

}  // namespace sdk
}  // namespace rtidb

int main(int argc, char** argv) {
    FLAGS_zk_session_timeout = 100000;
    ::rtidb::sdk::MiniCluster mc(6181);
    ::rtidb::sdk::mc_ = &mc;
    int ok = ::rtidb::sdk::mc_->SetUp();
    sleep(1);
    ::testing::InitGoogleTest(&argc, argv);
    srand(time(NULL));
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    ok = RUN_ALL_TESTS();
    ::rtidb::sdk::mc_->Close();
    return ok;
}
