//
// tablet_impl_ssd_test.cc
// Copyright (C) 2017 4paradigm.com
// Author wangtaize
// Date 2017-04-05
//

#include <fcntl.h>
#include <gflags/gflags.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <sys/stat.h>
#include <boost/lexical_cast.hpp>
#include "base/file_util.h"
#include "base/kv_iterator.h"
#include "base/strings.h"
#include "codec/flat_array.h"
#include "codec/schema_codec.h"
#include "gtest/gtest.h"
#include "log/log_reader.h"
#include "log/log_writer.h"
#include "base/glog_wapper.h" // NOLINT
#include "proto/tablet.pb.h"
#include "storage/mem_table.h"
#include "storage/ticket.h"
#include "tablet/tablet_impl.h"
#include "timer.h" // NOLINT

DECLARE_string(db_root_path);
DECLARE_string(recycle_bin_root_path);
DECLARE_string(zk_cluster);
DECLARE_string(zk_root_path);
DECLARE_int32(gc_interval);
DECLARE_int32(gc_safe_offset);
DECLARE_int32(make_snapshot_threshold_offset);
DECLARE_int32(binlog_delete_interval);

namespace rtidb {
namespace tablet {
class MockClosure : public ::google::protobuf::Closure {
 public:
    MockClosure() {}
    ~MockClosure() {}
    void Run() {}
};

using ::rtidb::api::TableStatus;

inline std::string GenRand() {
    return std::to_string(rand() % 10000000 + 1); // NOLINT
}

void CreateBaseTablet(::rtidb::tablet::TabletImpl& tablet,  // NOLINT
                      const ::rtidb::api::TTLType& ttl_type, uint64_t ttl,
                      uint64_t start_ts, uint32_t tid, uint32_t pid) {
    ::rtidb::api::CreateTableRequest crequest;
    ::rtidb::api::TableMeta* table_meta = crequest.mutable_table_meta();
    table_meta->set_name("table");
    table_meta->set_tid(tid);
    table_meta->set_pid(pid);
    table_meta->set_ttl(ttl);
    table_meta->set_seg_cnt(8);
    table_meta->set_mode(::rtidb::api::TableMode::kTableLeader);
    table_meta->set_key_entry_max_height(8);
    table_meta->set_ttl_type(ttl_type);
    ::rtidb::common::ColumnDesc* desc = table_meta->add_column_desc();
    desc->set_name("card");
    desc->set_type("string");
    desc->set_add_ts_idx(true);
    desc = table_meta->add_column_desc();
    desc->set_name("mcc");
    desc->set_type("string");
    desc->set_add_ts_idx(true);
    desc = table_meta->add_column_desc();
    desc->set_name("price");
    desc->set_type("int64");
    desc->set_add_ts_idx(false);
    desc = table_meta->add_column_desc();
    desc->set_name("ts1");
    desc->set_type("int64");
    desc->set_add_ts_idx(false);
    desc->set_is_ts_col(true);
    desc = table_meta->add_column_desc();
    desc->set_name("ts2");
    desc->set_type("int64");
    desc->set_add_ts_idx(false);
    desc->set_is_ts_col(true);
    desc->set_ttl(ttl);
    ::rtidb::common::ColumnKey* column_key = table_meta->add_column_key();
    column_key->set_index_name("card");
    column_key->add_ts_name("ts1");
    column_key = table_meta->add_column_key();
    column_key->set_index_name("mcc");
    column_key->add_ts_name("ts2");
    ::rtidb::api::CreateTableResponse cresponse;
    MockClosure closure;
    tablet.CreateTable(NULL, &crequest, &cresponse, &closure);
    ASSERT_EQ(0, cresponse.code());

    for (int i = 0; i < 1000; i++) {
        ::rtidb::api::PutRequest request;
        request.set_tid(tid);
        request.set_pid(pid);
        ::rtidb::api::Dimension* dim = request.add_dimensions();
        dim->set_idx(0);
        std::string k1 = "card" + std::to_string(i % 100);
        dim->set_key(k1);
        dim = request.add_dimensions();
        dim->set_idx(1);
        std::string k2 = "mcc" + std::to_string(i % 100);
        dim->set_key(k2);
        ::rtidb::api::TSDimension* ts = request.add_ts_dimensions();
        ts->set_idx(0);
        uint64_t time = start_ts + i;
        ts->set_ts(time);
        ts = request.add_ts_dimensions();
        ts->set_idx(1);
        ts->set_ts(time);
        std::string value = "value" + std::to_string(i);
        request.set_value(value);
        ::rtidb::api::PutResponse response;
        MockClosure closure;
        tablet.Put(NULL, &request, &response, &closure);
        ASSERT_EQ(0, response.code());
        {
            ::rtidb::api::GetRequest request;
            request.set_tid(tid);
            request.set_pid(pid);
            request.set_key(k1);
            request.set_ts(time);
            request.set_idx_name("card");
            request.set_ts_name("ts1");
            ::rtidb::api::GetResponse response;
            MockClosure closure;
            tablet.Get(NULL, &request, &response, &closure);
            ASSERT_EQ(0, response.code());
            ASSERT_EQ(value.c_str(), response.value());
        }

        {
            ::rtidb::api::GetRequest request;
            request.set_tid(tid);
            request.set_pid(pid);
            request.set_key(k2);
            request.set_ts(time);
            request.set_idx_name("mcc");
            request.set_ts_name("ts2");
            ::rtidb::api::GetResponse response;
            MockClosure closure;
            tablet.Get(NULL, &request, &response, &closure);
            ASSERT_EQ(0, response.code());
            ASSERT_EQ(value.c_str(), response.value());
        }
    }
    ::rtidb::api::DropTableRequest dr;
    dr.set_tid(tid);
    dr.set_pid(pid);
    ::rtidb::api::DropTableResponse drs;
    tablet.DropTable(NULL, &dr, &drs, &closure);
    ASSERT_EQ(0, drs.code());
}

void CreateTableWithoutDBRootPath(
    ::rtidb::tablet::TabletImpl& tablet,  // NOLINT
    const ::rtidb::api::TTLType& ttl_type, uint64_t ttl, uint64_t start_ts,
    uint32_t tid, uint32_t pid) {
    ::rtidb::api::CreateTableRequest crequest;
    ::rtidb::api::TableMeta* table_meta = crequest.mutable_table_meta();
    table_meta->set_name("table");
    table_meta->set_tid(tid);
    table_meta->set_pid(pid);
    table_meta->set_ttl(ttl);
    table_meta->set_seg_cnt(8);
    table_meta->set_mode(::rtidb::api::TableMode::kTableLeader);
    table_meta->set_key_entry_max_height(8);
    table_meta->set_ttl_type(ttl_type);
    ::rtidb::api::CreateTableResponse cresponse;
    MockClosure closure;
    tablet.CreateTable(NULL, &crequest, &cresponse, &closure);
    ASSERT_EQ(138, cresponse.code());
}

// create table use advance ttl
void CreateAdvanceTablet(::rtidb::tablet::TabletImpl& tablet,  // NOLINT
                         const ::rtidb::api::TTLType& ttl_type,
                         uint64_t abs_ttl, uint64_t lat_ttl, uint64_t start_ts,
                         uint32_t tid, uint32_t pid,
                         uint64_t col_abs_ttl, uint64_t col_lat_ttl) {
    ::rtidb::api::CreateTableRequest crequest;
    ::rtidb::api::TableMeta* table_meta = crequest.mutable_table_meta();
    table_meta->set_name("table");
    table_meta->set_tid(tid);
    table_meta->set_pid(pid);
    ::rtidb::api::TTLDesc* ttl_desc = table_meta->mutable_ttl_desc();
    ttl_desc->set_abs_ttl(abs_ttl);
    ttl_desc->set_lat_ttl(lat_ttl);
    ttl_desc->set_ttl_type(ttl_type);
    table_meta->set_seg_cnt(8);
    table_meta->set_mode(::rtidb::api::TableMode::kTableLeader);
    table_meta->set_key_entry_max_height(8);
    ::rtidb::common::ColumnDesc* desc = table_meta->add_column_desc();
    desc->set_name("card");
    desc->set_type("string");
    desc->set_add_ts_idx(true);
    desc = table_meta->add_column_desc();
    desc->set_name("mcc");
    desc->set_type("string");
    desc->set_add_ts_idx(true);
    desc = table_meta->add_column_desc();
    desc->set_name("price");
    desc->set_type("int64");
    desc->set_add_ts_idx(false);
    desc = table_meta->add_column_desc();
    desc->set_name("ts1");
    desc->set_type("int64");
    desc->set_add_ts_idx(false);
    desc->set_is_ts_col(true);
    desc = table_meta->add_column_desc();
    desc->set_name("ts2");
    desc->set_type("int64");
    desc->set_add_ts_idx(false);
    desc->set_is_ts_col(true);
    desc->set_abs_ttl(col_abs_ttl);
    desc->set_lat_ttl(col_lat_ttl);
    ::rtidb::common::ColumnKey* column_key = table_meta->add_column_key();
    column_key->set_index_name("card");
    column_key->add_ts_name("ts1");
    column_key = table_meta->add_column_key();
    column_key->set_index_name("mcc");
    column_key->add_ts_name("ts2");
    ::rtidb::api::CreateTableResponse cresponse;
    MockClosure closure;
    tablet.CreateTable(NULL, &crequest, &cresponse, &closure);
    ASSERT_EQ(0, cresponse.code());
    int count1 = 0;
    int count2 = 0;
    uint64_t time = 0;
    for (int i = 0; i < 1000; i++) {
        uint64_t expire_time_ts1 =
            ::baidu::common::timer::get_micros() / 1000 - abs_ttl * (60 * 1000);
        uint64_t expire_time_ts2 = ::baidu::common::timer::get_micros() / 1000 -
                                   col_abs_ttl * (60 * 1000);
        ::rtidb::api::PutRequest request;
        request.set_tid(tid);
        request.set_pid(pid);
        ::rtidb::api::Dimension* dim = request.add_dimensions();
        dim->set_idx(0);
        std::string k1 = "card" + std::to_string(i % 100);
        dim->set_key(k1);
        dim = request.add_dimensions();
        dim->set_idx(1);
        std::string k2 = "mcc" + std::to_string(i % 100);
        dim->set_key(k2);
        ::rtidb::api::TSDimension* ts = request.add_ts_dimensions();
        ts->set_idx(0);
        time = start_ts + i * (60 * 1000);
        ts->set_ts(time);
        ts = request.add_ts_dimensions();
        ts->set_idx(1);
        ts->set_ts(time);
        std::string value = "value" + std::to_string(i);
        request.set_value(value);
        ::rtidb::api::PutResponse response;
        MockClosure closure;
        tablet.Put(NULL, &request, &response, &closure);
        ASSERT_EQ(0, response.code());
        {
            ::rtidb::api::GetRequest request;
            request.set_tid(tid);
            request.set_pid(pid);
            request.set_key(k1);
            request.set_ts(time);
            request.set_idx_name("card");
            request.set_ts_name("ts1");
            ::rtidb::api::GetResponse response;
            MockClosure closure;
            tablet.Get(NULL, &request, &response, &closure);
            if (time <= expire_time_ts1 &&
                ttl_type == ::rtidb::api::TTLType::kAbsOrLat) {
                ASSERT_EQ(307, response.code());
            } else {
                ++count1;
                ASSERT_EQ(0, response.code());
                ASSERT_EQ(value.c_str(), response.value());
            }
        }

        {
            ::rtidb::api::GetRequest request;
            request.set_tid(tid);
            request.set_pid(pid);
            request.set_key(k2);
            request.set_ts(time);
            request.set_idx_name("mcc");
            request.set_ts_name("ts2");
            ::rtidb::api::GetResponse response;
            MockClosure closure;
            tablet.Get(NULL, &request, &response, &closure);
            if (time <= expire_time_ts2 &&
                ttl_type == ::rtidb::api::TTLType::kAbsOrLat) {
                ASSERT_EQ(307, response.code());
            } else {
                ++count2;
                ASSERT_EQ(0, response.code());
                ASSERT_EQ(value.c_str(), response.value());
            }
        }
    }

    ::rtidb::api::DropTableRequest dr;
    dr.set_tid(tid);
    dr.set_pid(pid);
    ::rtidb::api::DropTableResponse drs;
    tablet.DropTable(NULL, &dr, &drs, &closure);
    ASSERT_EQ(0, drs.code());
}

class TabletMultiPathTest : public ::testing::Test {
 public:
    TabletMultiPathTest() {}
    ~TabletMultiPathTest() {}
};

TEST_F(TabletMultiPathTest, CreateWithoutDBPath) {
    std::string old_db_path = FLAGS_db_root_path;
    FLAGS_db_root_path = "";
    ::rtidb::tablet::TabletImpl tablet_impl;
    tablet_impl.Init("");
    CreateTableWithoutDBRootPath(tablet_impl,
                                 ::rtidb::api::TTLType::kAbsoluteTime, 0, 1000,
                                 100, 0);
    CreateTableWithoutDBRootPath(tablet_impl,
                                 ::rtidb::api::TTLType::kAbsoluteTime, 0, 1000,
                                 101, 0);
    CreateTableWithoutDBRootPath(tablet_impl,
                                 ::rtidb::api::TTLType::kAbsoluteTime, 0, 1000,
                                 102, 0);
    FLAGS_db_root_path = old_db_path;
}

TEST_F(TabletMultiPathTest, Memory_Test_read_write_absolute) {
    ::rtidb::tablet::TabletImpl tablet_impl;
    tablet_impl.Init("");
    for (uint32_t i = 0; i < 100; i++) {
        CreateBaseTablet(tablet_impl, ::rtidb::api::TTLType::kAbsoluteTime, 0,
                         1000, i + 1, i % 10);
    }
}

TEST_F(TabletMultiPathTest, Memory_Test_read_write_latest) {
    ::rtidb::tablet::TabletImpl tablet_impl;
    tablet_impl.Init("");
    for (uint32_t i = 100; i < 200; i++) {
        CreateBaseTablet(tablet_impl, ::rtidb::api::TTLType::kLatestTime, 10,
                         1000, i + 1, i % 10);
    }
}

TEST_F(TabletMultiPathTest, HDD_Test_read_write) {
    ::rtidb::tablet::TabletImpl tablet_impl;
    tablet_impl.Init("");
    for (uint32_t i = 0; i < 100; i++) {
        CreateBaseTablet(tablet_impl, ::rtidb::api::TTLType::kLatestTime, 10,
                         1000, i + 1, i % 10);
    }
}

TEST_F(TabletMultiPathTest, SSD_Test_read_write) {
    ::rtidb::tablet::TabletImpl tablet_impl;
    tablet_impl.Init("");
    for (uint32_t i = 0; i < 100; i++) {
        CreateBaseTablet(tablet_impl, ::rtidb::api::TTLType::kLatestTime, 10,
                         1000, i + 1, i % 10);
    }
}

TEST_F(TabletMultiPathTest, Memory_Test_read_write_abs_and_lat) {
    ::rtidb::tablet::TabletImpl tablet_impl;
    tablet_impl.Init("");
    uint64_t now = ::baidu::common::timer::get_micros() / 1000;
    for (uint32_t i = 20; i < 30; i++) {
        CreateAdvanceTablet(tablet_impl, ::rtidb::api::TTLType::kAbsAndLat,
                            2000, 500, now - 3000 * (60 * 1000) - 1000, i + 1,
                            i % 10, 3000, 500);
    }
}

TEST_F(TabletMultiPathTest, Memory_Test_read_write_abs_or_lat) {
    ::rtidb::tablet::TabletImpl tablet_impl;
    tablet_impl.Init("");
    uint64_t now = ::baidu::common::timer::get_micros() / 1000;
    for (uint32_t i = 30; i < 40; i++) {
        CreateAdvanceTablet(tablet_impl, ::rtidb::api::TTLType::kAbsOrLat, 2000,
                            500, now - 3000 * (60 * 1000) - 1000, i + 1, i % 10,
                            1000, 500);
    }
}

}  // namespace tablet
}  // namespace rtidb

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    srand(time(NULL));
    std::string k1 = ::rtidb::tablet::GenRand();
    std::string k2 = ::rtidb::tablet::GenRand();
    FLAGS_db_root_path = "/tmp/db" + k1 + ",/tmp/db" + k2;
    FLAGS_recycle_bin_root_path = "/tmp/recycle" + k1 + ",/tmp/recycle" + k2;
    return RUN_ALL_TESTS();
}
