/*
 * sql_sdk_test.cc
 * Copyright (C) 4paradigm.com 2020 chenjing <chenjing@4paradigm.com>
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

#include <sched.h>
#include <timer.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/glog_wapper.h"  // NOLINT
#include "boost/algorithm/string.hpp"
#include "catalog/schema_adapter.h"
#include "codec/fe_row_codec.h"
#include "gflags/gflags.h"
#include "sdk/mini_cluster.h"
#include "sdk/sql_router.h"
#include "test/base_test.h"
#include "vm/catalog.h"
#include "sdk/sql_sdk_test.h"
namespace rtidb {
namespace sdk {

MiniCluster* mc_ = nullptr;
static std::shared_ptr<SQLRouter> GetNewSQLRouter(const fesql::sqlcase::SQLCase& sql_case) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    sql_opt.enable_debug = sql_case.debug() || fesql::sqlcase::SQLCase::IS_DEBUG();
    return NewClusterSQLRouter(sql_opt);
}
TEST_P(SQLSDKTest, sql_sdk_batch_test) {
    auto sql_case = GetParam();
    LOG(INFO) << "ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    sql_opt.enable_debug = sql_case.debug() || fesql::sqlcase::SQLCase::IS_DEBUG();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) {
        FAIL() << "Fail new cluster sql router";
        return;
    }
    SQLSDKTest::RunBatchModeSDK(sql_case, router, mc_->GetTbEndpoint());
}

TEST_P(SQLSDKQueryTest, sql_sdk_request_test) {
    auto sql_case = GetParam();
    LOG(INFO) << "ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
    if (boost::contains(sql_case.mode(), "rtidb-unsupport") ||
        boost::contains(sql_case.mode(), "rtidb-request-unsupport") ||
        boost::contains(sql_case.mode(), "request-unsupport")) {
        LOG(WARNING) << "Unsupport mode: " << sql_case.mode();
        return;
    }
    auto router = GetNewSQLRouter(sql_case);
    ASSERT_TRUE(router != nullptr) << "Fail new cluster sql router";
    RunRequestModeSDK(sql_case, router);
    LOG(INFO) << "Finish sql_sdk_request_test: ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
}

TEST_P(SQLSDKBatchRequestQueryTest, sql_sdk_batch_request_test) {
    auto sql_case = GetParam();
    if (boost::contains(sql_case.mode(), "rtidb-unsupport") ||
        boost::contains(sql_case.mode(), "rtidb-request-unsupport") ||
        boost::contains(sql_case.mode(), "request-unsupport")) {
        LOG(WARNING) << "Unsupport mode: " << sql_case.mode();
        return;
    }
    if (sql_case.batch_request().columns_.empty()) {
        LOG(WARNING) << "No batch request specified";
        return;
    }
    LOG(INFO) << "ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
    auto router = GetNewSQLRouter(sql_case);
    ASSERT_TRUE(router != nullptr) << "Fail new cluster sql router";
    RunBatchRequestModeSDK(sql_case, router);
    LOG(INFO) << "Finish sql_sdk_request_test: ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
}

TEST_P(SQLSDKQueryTest, sql_sdk_batch_test) {
    auto sql_case = GetParam();
    LOG(INFO) << "ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
    if (boost::contains(sql_case.mode(), "rtidb-unsupport") ||
        boost::contains(sql_case.mode(), "rtidb-batch-unsupport") ||
        boost::contains(sql_case.mode(), "batch-unsupport")) {
        LOG(WARNING) << "Unsupport mode: " << sql_case.mode();
        return;
    }
    auto router = GetNewSQLRouter(sql_case);
    ASSERT_TRUE(router != nullptr) << "Fail new cluster sql router";
    RunBatchModeSDK(sql_case, router, mc_->GetTbEndpoint());
}

TEST_P(SQLSDKQueryTest, sql_sdk_request_procedure_test) {
    auto sql_case = GetParam();
    LOG(INFO) << "ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
    if (boost::contains(sql_case.mode(), "rtidb-unsupport") ||
        boost::contains(sql_case.mode(), "rtidb-request-unsupport") ||
        boost::contains(sql_case.mode(), "request-unsupport")) {
        LOG(WARNING) << "Unsupport mode: " << sql_case.mode();
        return;
    }
    auto router = GetNewSQLRouter(sql_case);
    ASSERT_TRUE(router != nullptr) << "Fail new cluster sql router";
    RunRequestProcedureModeSDK(sql_case, router, false);
    LOG(INFO) << "Finish sql_sdk_request_procedure_test: ID: "
        << sql_case.id() << ", DESC: " << sql_case.desc();
}

TEST_P(SQLSDKQueryTest, sql_sdk_request_procedure_asyn_test) {
    auto sql_case = GetParam();
    LOG(INFO) << "ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
    if (boost::contains(sql_case.mode(), "rtidb-unsupport") ||
        boost::contains(sql_case.mode(), "rtidb-request-unsupport") ||
        boost::contains(sql_case.mode(), "request-unsupport")) {
        LOG(WARNING) << "Unsupport mode: " << sql_case.mode();
        return;
    }
    auto router = GetNewSQLRouter(sql_case);
    ASSERT_TRUE(router != nullptr) << "Fail new cluster sql router";
    RunRequestProcedureModeSDK(sql_case, router, true);
    LOG(INFO) << "Finish sql_sdk_request_procedure_asyn_test: ID: "
        << sql_case.id() << ", DESC: " << sql_case.desc();
}

TEST_P(SQLSDKBatchRequestQueryTest, sql_sdk_batch_request_procedure_test) {
    auto sql_case = GetParam();
    if (boost::contains(sql_case.mode(), "rtidb-unsupport") ||
        boost::contains(sql_case.mode(), "rtidb-request-unsupport") ||
        boost::contains(sql_case.mode(), "request-unsupport")) {
        LOG(WARNING) << "Unsupport mode: " << sql_case.mode();
        return;
    }
    if (sql_case.batch_request().columns_.empty()) {
        LOG(WARNING) << "No batch request specified";
        return;
    }
    LOG(INFO) << "ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
    auto router = GetNewSQLRouter(sql_case);
    ASSERT_TRUE(router != nullptr) << "Fail new cluster sql router";
    RunBatchRequestProcedureModeSDK(sql_case, router, false);
    LOG(INFO) << "Finish sql_sdk_request_test: ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
}

TEST_P(SQLSDKBatchRequestQueryTest, sql_sdk_batch_request_procedure_asyn_test) {
    auto sql_case = GetParam();
    if (boost::contains(sql_case.mode(), "rtidb-unsupport") ||
        boost::contains(sql_case.mode(), "rtidb-request-unsupport") ||
        boost::contains(sql_case.mode(), "request-unsupport")) {
        LOG(WARNING) << "Unsupport mode: " << sql_case.mode();
        return;
    }
    if (sql_case.batch_request().columns_.empty()) {
        LOG(WARNING) << "No batch request specified";
        return;
    }
    LOG(INFO) << "ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
    auto router = GetNewSQLRouter(sql_case);
    ASSERT_TRUE(router != nullptr) << "Fail new cluster sql router";
    RunBatchRequestProcedureModeSDK(sql_case, router, true);
    LOG(INFO) << "Finish sql_sdk_request_test: ID: " << sql_case.id() << ", DESC: " << sql_case.desc();
}

TEST_F(SQLSDKQueryTest, execute_where_test) {
    std::string ddl =
        "create table trans(c_sk_seq string,\n"
        "                   cust_no string,\n"
        "                   pay_cust_name string,\n"
        "                   pay_card_no string,\n"
        "                   payee_card_no string,\n"
        "                   card_type string,\n"
        "                   merch_id string,\n"
        "                   txn_datetime string,\n"
        "                   txn_amt double,\n"
        "                   txn_curr string,\n"
        "                   card_balance double,\n"
        "                   day_openbuy double,\n"
        "                   credit double,\n"
        "                   remainning_credit double,\n"
        "                   indi_openbuy double,\n"
        "                   lgn_ip string,\n"
        "                   IEMI string,\n"
        "                   client_mac string,\n"
        "                   chnl_type int32,\n"
        "                   cust_idt int32,\n"
        "                   cust_idt_no string,\n"
        "                   province string,\n"
        "                   city string,\n"
        "                   latitudeandlongitude string,\n"
        "                   txn_time timestamp,\n"
        "                   index(key=pay_card_no, ts=txn_time),\n"
        "                   index(key=merch_id, ts=txn_time));";
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    sql_opt.enable_debug = fesql::sqlcase::SQLCase::IS_DEBUG();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) {
        FAIL() << "Fail new cluster sql router";
    }
    std::string db = "sql_where_test";
    fesql::sdk::Status status;
    ASSERT_TRUE(router->CreateDB(db, &status));
    ASSERT_TRUE(router->ExecuteDDL(db, ddl, &status));
    ASSERT_TRUE(router->RefreshCatalog());
    int64_t ts = 1594800959827;
    char buffer[4096];
    sprintf(buffer,  // NOLINT
            "insert into trans "
            "values('c_sk_seq0','cust_no0','pay_cust_name0','card_%d','"
            "payee_card_no0','card_type0','mc_%d','2020-"
            "10-20 "
            "10:23:50',1.0,'txn_curr',2.0,3.0,4.0,5.0,6.0,'lgn_ip0','iemi0'"
            ",'client_mac0',10,20,'cust_idt_no0','"
            "province0',"
            "'city0', 'longitude', %s);",
            0, 0, std::to_string(ts++).c_str());  // NOLINT
    std::string insert_sql = std::string(buffer, strlen(buffer));
    ASSERT_TRUE(router->ExecuteInsert(db, insert_sql, &status));
    std::string where_exist = "select * from trans where merch_id='mc_0';";
    auto rs = router->ExecuteSQL(db, where_exist, &status);
    if (!rs) {
        FAIL() << "fail to execute sql";
    }
    ASSERT_EQ(rs->Size(), 1);
    std::string where_not_exist = "select * from trans where merch_id='mc_1';";
    rs = router->ExecuteSQL(db, where_not_exist, &status);
    if (!rs) {
        FAIL() << "fail to execute sql";
    }
    ASSERT_EQ(rs->Size(), 0);
}

TEST_F(SQLSDKQueryTest, execute_insert_loops_test) {
    std::string ddl =
        "create table trans(c_sk_seq string,\n"
        "                   cust_no string,\n"
        "                   pay_cust_name string,\n"
        "                   pay_card_no string,\n"
        "                   payee_card_no string,\n"
        "                   card_type string,\n"
        "                   merch_id string,\n"
        "                   txn_datetime string,\n"
        "                   txn_amt double,\n"
        "                   txn_curr string,\n"
        "                   card_balance double,\n"
        "                   day_openbuy double,\n"
        "                   credit double,\n"
        "                   remainning_credit double,\n"
        "                   indi_openbuy double,\n"
        "                   lgn_ip string,\n"
        "                   IEMI string,\n"
        "                   client_mac string,\n"
        "                   chnl_type int32,\n"
        "                   cust_idt int32,\n"
        "                   cust_idt_no string,\n"
        "                   province string,\n"
        "                   city string,\n"
        "                   latitudeandlongitude string,\n"
        "                   txn_time timestamp,\n"
        "                   index(key=pay_card_no, ts=txn_time),\n"
        "                   index(key=merch_id, ts=txn_time));";
    int64_t ts = 1594800959827;
    int card = 0;
    int mc = 0;
    int64_t error_cnt = 0;
    int64_t cnt = 0;
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    sql_opt.enable_debug = fesql::sqlcase::SQLCase::IS_DEBUG();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) {
        FAIL() << "Fail new cluster sql router";
    }
    std::string db = "leak_test";
    fesql::sdk::Status status;
    router->CreateDB(db, &status);
    router->ExecuteDDL(db, "drop table trans;", &status);
    ASSERT_TRUE(router->RefreshCatalog());
    if (!router->ExecuteDDL(db, ddl, &status)) {
        ASSERT_TRUE(router->RefreshCatalog());
        LOG(WARNING) << "fail to create table";
        return;
    }
    ASSERT_TRUE(router->RefreshCatalog());
    while (true) {
        char buffer[4096];
        sprintf(buffer,  // NOLINT
                "insert into trans "
                "values('c_sk_seq0','cust_no0','pay_cust_name0','card_%d','"
                "payee_card_no0','card_type0','mc_%d','2020-"
                "10-20 "
                "10:23:50',1.0,'txn_curr',2.0,3.0,4.0,5.0,6.0,'lgn_ip0','iemi0'"
                ",'client_mac0',10,20,'cust_idt_no0','"
                "province0',"
                "'city0', 'longitude', %s);",
                card++, mc++, std::to_string(ts++).c_str());  // NOLINT
        std::string insert_sql = std::string(buffer, strlen(buffer));
        //        LOG(INFO) << insert_sql;
        fesql::sdk::Status status;
        if (!router->ExecuteInsert(db, insert_sql, &status)) {
            error_cnt += 1;
        }

        if (cnt % 10000 == 0) {
            LOG(INFO) << "process ...... " << cnt << " error: " << error_cnt;
        }
        cnt++;
        break;
    }
}

TEST_F(SQLSDKQueryTest, request_procedure_test) {
    // create table
    std::string ddl =
        "create table trans(c1 string,\n"
        "                   c3 int,\n"
        "                   c4 bigint,\n"
        "                   c5 float,\n"
        "                   c6 double,\n"
        "                   c7 timestamp,\n"
        "                   c8 date,\n"
        "                   index(key=c1, ts=c7));";
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    sql_opt.enable_debug = fesql::sqlcase::SQLCase::IS_DEBUG();
    auto router = NewClusterSQLRouter(sql_opt);
    if (!router) {
        FAIL() << "Fail new cluster sql router";
    }
    std::string db = "test";
    fesql::sdk::Status status;
    router->CreateDB(db, &status);
    router->ExecuteDDL(db, "drop table trans;", &status);
    ASSERT_TRUE(router->RefreshCatalog());
    if (!router->ExecuteDDL(db, ddl, &status)) {
        FAIL() << "fail to create table";
    }
    ASSERT_TRUE(router->RefreshCatalog());
    // insert
    std::string insert_sql =
        "insert into trans values(\"bb\",24,34,1.5,2.5,1590738994000,\"2020-05-05\");";
    ASSERT_TRUE(router->ExecuteInsert(db, insert_sql, &status));
    // create procedure
    std::string sp_name = "sp";
    std::string sql =
        "SELECT c1, c3, sum(c4) OVER w1 as w1_c4_sum FROM trans WINDOW w1 AS"
        " (PARTITION BY trans.c1 ORDER BY trans.c7 ROWS BETWEEN 2 PRECEDING AND CURRENT ROW);";
    std::string sp_ddl =
        "create procedure " + sp_name +
        " (const c1 string, const c3 int, c4 bigint, c5 float, c6 double, c7 timestamp, c8 date" + ")" +
        " begin " + sql + " end;";
    if (!router->ExecuteDDL(db, sp_ddl, &status)) {
        FAIL() << "fail to create procedure";
    }
    // call procedure
    ASSERT_TRUE(router->RefreshCatalog());
    auto request_row = router->GetRequestRow(db, sql, &status);
    ASSERT_TRUE(request_row);
    request_row->Init(2);
    ASSERT_TRUE(request_row->AppendString("bb"));
    ASSERT_TRUE(request_row->AppendInt32(23));
    ASSERT_TRUE(request_row->AppendInt64(33));
    ASSERT_TRUE(request_row->AppendFloat(1.5f));
    ASSERT_TRUE(request_row->AppendDouble(2.5));
    ASSERT_TRUE(request_row->AppendTimestamp(1590738994000));
    ASSERT_TRUE(request_row->AppendDate(1234));
    ASSERT_TRUE(request_row->Build());
    auto rs = router->CallProcedure(db, sp_name, request_row, &status);
    if (!rs) FAIL() << "call procedure failed";
    auto schema = rs->GetSchema();
    ASSERT_EQ(schema->GetColumnCnt(), 3u);
    ASSERT_TRUE(rs->Next());
    ASSERT_EQ(rs->GetStringUnsafe(0), "bb");
    ASSERT_EQ(rs->GetInt32Unsafe(1), 23);
    ASSERT_EQ(rs->GetInt64Unsafe(2), 67);
    ASSERT_FALSE(rs->Next());
    // show procedure
    std::string msg;
    auto sp_info = router->ShowProcedure(db, sp_name, &status);
    ASSERT_TRUE(sp_info);
    ASSERT_EQ(sp_info->GetDbName(), db);
    ASSERT_EQ(sp_info->GetSpName(), sp_name);
    ASSERT_EQ(sp_info->GetMainTable(), "trans");
    ASSERT_EQ(sp_info->GetTables().size(), 1u);
    ASSERT_EQ(sp_info->GetTables().at(0), "trans");
    auto& input_schema = sp_info->GetInputSchema();
    ASSERT_EQ(input_schema.GetColumnCnt(), 7u);
    ASSERT_EQ(input_schema.GetColumnName(0), "c1");
    ASSERT_EQ(input_schema.GetColumnName(1), "c3");
    ASSERT_EQ(input_schema.GetColumnName(2), "c4");
    ASSERT_EQ(input_schema.GetColumnName(3), "c5");
    ASSERT_EQ(input_schema.GetColumnName(4), "c6");
    ASSERT_EQ(input_schema.GetColumnName(5), "c7");
    ASSERT_EQ(input_schema.GetColumnName(6), "c8");
    ASSERT_EQ(input_schema.GetColumnType(0), fesql::sdk::kTypeString);
    ASSERT_EQ(input_schema.GetColumnType(1), fesql::sdk::kTypeInt32);
    ASSERT_EQ(input_schema.GetColumnType(2), fesql::sdk::kTypeInt64);
    ASSERT_EQ(input_schema.GetColumnType(3), fesql::sdk::kTypeFloat);
    ASSERT_EQ(input_schema.GetColumnType(4), fesql::sdk::kTypeDouble);
    ASSERT_EQ(input_schema.GetColumnType(5), fesql::sdk::kTypeTimestamp);
    ASSERT_EQ(input_schema.GetColumnType(6), fesql::sdk::kTypeDate);

    auto& output_schema = sp_info->GetOutputSchema();
    ASSERT_EQ(output_schema.GetColumnCnt(), 3u);
    ASSERT_EQ(output_schema.GetColumnName(0), "c1");
    ASSERT_EQ(output_schema.GetColumnName(1), "c3");
    ASSERT_EQ(output_schema.GetColumnName(2), "w1_c4_sum");
    ASSERT_EQ(output_schema.GetColumnType(0), fesql::sdk::kTypeString);
    ASSERT_EQ(output_schema.GetColumnType(1), fesql::sdk::kTypeInt32);
    ASSERT_EQ(output_schema.GetColumnType(2), fesql::sdk::kTypeInt64);
    // drop procedure
    ASSERT_TRUE(router->ExecuteDDL(db, "drop table trans;", &status));
}

TEST_F(SQLSDKTest, create_table) {
    SQLRouterOptions sql_opt;
    sql_opt.zk_cluster = mc_->GetZkCluster();
    sql_opt.zk_path = mc_->GetZkPath();
    auto router = NewClusterSQLRouter(sql_opt);
    ASSERT_TRUE(router != nullptr);
    std::string db = "db" + rand() % 1000;  // NOLINT
    ::fesql::sdk::Status status;
    bool ok = router->CreateDB(db, &status);
    ASSERT_TRUE(ok);
    for (int i = 0; i < 2; i++) {
        std::string name = "test" + std::to_string(i);
        std::string ddl = "create table " + name +
                          "("
                          "col1 string, col2 bigint,"
                          "index(key=col1, ts=col2));";
        ok = router->ExecuteDDL(db, ddl, &status);
        ASSERT_TRUE(ok);
    }
    ASSERT_TRUE(router->RefreshCatalog());
    auto ns_client = mc_->GetNsClient();
    std::vector<::rtidb::nameserver::TableInfo> tables;
    std::string msg;
    ASSERT_TRUE(ns_client->ShowTable("", db, false, tables, msg));
    ASSERT_TRUE(!tables.empty());
    std::map<std::string, int> pid_map;
    for (const auto& table : tables) {
        for (const auto& partition : table.table_partition()) {
            for (const auto& meta : partition.partition_meta()) {
                if (pid_map.find(meta.endpoint()) == pid_map.end()) {
                    pid_map.emplace(meta.endpoint(), 0);
                }
                pid_map[meta.endpoint()]++;
            }
        }
    }
    ASSERT_EQ(pid_map.size(), 1);
    ASSERT_TRUE(router->ExecuteDDL(db, "drop table test0;", &status));
    ASSERT_TRUE(router->ExecuteDDL(db, "drop table test1;", &status));
    ASSERT_TRUE(router->DropDB(db, &status));
}

}  // namespace sdk
}  // namespace rtidb

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    srand(time(NULL));
    FLAGS_zk_session_timeout = 100000;
    ::rtidb::sdk::MiniCluster mc(6181);
    ::rtidb::sdk::mc_ = &mc;
    int ok = ::rtidb::sdk::mc_->SetUp(2);
    sleep(1);
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    ok = RUN_ALL_TESTS();
    ::rtidb::sdk::mc_->Close();
    return ok;
}
