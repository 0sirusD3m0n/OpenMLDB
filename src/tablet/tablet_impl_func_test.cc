//
// tablet_impl_func_test.cc
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
#include "logging.h"  // NOLINT
#include "proto/tablet.pb.h"
#include "storage/mem_table.h"
#include "storage/ticket.h"
#include "tablet/tablet_impl.h"
#include "timer.h"  // NOLINT

DECLARE_string(db_root_path);
DECLARE_string(zk_cluster);
DECLARE_string(zk_root_path);
DECLARE_int32(gc_interval);
DECLARE_int32(make_snapshot_threshold_offset);
DECLARE_int32(binlog_delete_interval);

namespace rtidb {
namespace tablet {

using ::rtidb::api::TableStatus;

inline std::string GenRand() {
    return std::to_string(rand() % 10000000 + 1); // NOLINT
}

void CreateBaseTable(::rtidb::storage::Table*& table,  // NOLINT
                     const ::rtidb::api::TTLType& ttl_type, uint64_t ttl,
                     uint64_t start_ts) {
    ::rtidb::api::TableMeta table_meta;
    table_meta.set_name("table");
    table_meta.set_tid(1);
    table_meta.set_pid(0);
    table_meta.set_ttl(ttl);
    table_meta.set_seg_cnt(8);
    table_meta.set_mode(::rtidb::api::TableMode::kTableLeader);
    table_meta.set_key_entry_max_height(8);
    table_meta.set_ttl_type(ttl_type);

    ::rtidb::common::ColumnDesc* desc = table_meta.add_column_desc();
    desc->set_name("card");
    desc->set_type("string");
    desc->set_add_ts_idx(true);

    desc = table_meta.add_column_desc();
    desc->set_name("mcc");
    desc->set_type("string");
    desc->set_add_ts_idx(true);

    desc = table_meta.add_column_desc();
    desc->set_name("price");
    desc->set_type("int64");
    desc->set_add_ts_idx(false);

    desc = table_meta.add_column_desc();
    desc->set_name("ts1");
    desc->set_type("int64");
    desc->set_add_ts_idx(false);
    desc->set_is_ts_col(true);

    desc = table_meta.add_column_desc();
    desc->set_name("ts2");
    desc->set_type("int64");
    desc->set_add_ts_idx(false);
    desc->set_is_ts_col(true);
    desc->set_ttl(ttl);

    ::rtidb::common::ColumnKey* column_key = table_meta.add_column_key();
    column_key->set_index_name("card");
    column_key->add_ts_name("ts1");
    column_key->add_ts_name("ts2");
    column_key = table_meta.add_column_key();
    column_key->set_index_name("mcc");
    column_key->add_ts_name("ts1");
    table = new ::rtidb::storage::MemTable(table_meta);
    table->Init();
    for (int i = 0; i < 1000; i++) {
        ::rtidb::api::PutRequest request;
        ::rtidb::api::Dimension* dim = request.add_dimensions();
        dim->set_idx(0);
        dim->set_key("card" + std::to_string(i % 100));
        dim = request.add_dimensions();
        dim->set_idx(1);
        dim->set_key("mcc" + std::to_string(i));
        ::rtidb::api::TSDimension* ts = request.add_ts_dimensions();
        ts->set_idx(0);
        ts->set_ts(start_ts + i);
        ts = request.add_ts_dimensions();
        ts->set_idx(1);
        ts->set_ts(start_ts + i);
        std::string value = "value" + std::to_string(i);
        ASSERT_TRUE(
            table->Put(request.dimensions(), request.ts_dimensions(), value));
    }
    return;
}

class TabletFuncTest : public ::testing::Test {
 public:
    TabletFuncTest() {}
    ~TabletFuncTest() {}
};

void RunGetTimeIndexAssert(::rtidb::storage::TableIterator* it,
                           uint64_t base_ts, uint64_t expired_ts) {
    ::rtidb::tablet::TabletImpl tablet_impl;
    std::string value;
    uint64_t ts;
    int32_t code = 0;
    ::rtidb::api::TableMeta meta;
    // get the st kSubKeyGt
    {
        // for the legacy
        ::rtidb::api::GetRequest request;
        request.set_ts(100 + base_ts);
        request.set_type(::rtidb::api::GetType::kSubKeyGt);
        request.set_et(100 + base_ts);
        request.set_et_type(::rtidb::api::GetType::kSubKeyEq);
        code = tablet_impl.GetIndex(expired_ts, 0,
                                    ::rtidb::api::TTLType::kAbsoluteTime, it,
                                    &request, meta, &value, &ts);
        ASSERT_EQ(0, code);
        ASSERT_EQ(ts, 900 + base_ts);
        ASSERT_EQ(value, "value900");
    }

    // get the st kSubKeyLe
    {
        // for the legacy
        ::rtidb::api::GetRequest request;
        request.set_ts(100 + base_ts);
        request.set_type(::rtidb::api::GetType::kSubKeyLe);
        request.set_et(100 + base_ts);
        request.set_et_type(::rtidb::api::GetType::kSubKeyGe);
        code = tablet_impl.GetIndex(expired_ts, 0,
                                    ::rtidb::api::TTLType::kAbsoluteTime, it,
                                    &request, meta, &value, &ts);
        ASSERT_EQ(0, code);
        ASSERT_EQ(ts, 100 + base_ts);
        ASSERT_EQ(value, "value100");
    }

    // get the st 900kSubKeyLe
    {
        // for the legacy
        ::rtidb::api::GetRequest request;
        request.set_ts(900 + base_ts);
        request.set_type(::rtidb::api::GetType::kSubKeyLe);
        request.set_et(100 + base_ts);
        request.set_et_type(::rtidb::api::GetType::kSubKeyGe);
        code = tablet_impl.GetIndex(expired_ts, 0,
                                    ::rtidb::api::TTLType::kAbsoluteTime, it,
                                    &request, meta, &value, &ts);
        ASSERT_EQ(0, code);
        ASSERT_EQ(ts, 900 + base_ts);
        ASSERT_EQ(value, "value900");
    }

    // get the st 899kSubKeyLe
    {
        // for the legacy
        ::rtidb::api::GetRequest request;
        request.set_ts(899 + base_ts);
        request.set_type(::rtidb::api::GetType::kSubKeyLe);
        request.set_et(100 + base_ts);
        request.set_et_type(::rtidb::api::GetType::kSubKeyGe);
        code = tablet_impl.GetIndex(expired_ts, 0,
                                    ::rtidb::api::TTLType::kAbsoluteTime, it,
                                    &request, meta, &value, &ts);
        ASSERT_EQ(0, code);
        ASSERT_EQ(ts, 800 + base_ts);
        ASSERT_EQ(value, "value800");
    }

    // get the st 800 kSubKeyLe
    {
        // for the legacy
        ::rtidb::api::GetRequest request;
        request.set_ts(899 + base_ts);
        request.set_type(::rtidb::api::GetType::kSubKeyLe);
        request.set_et(800 + base_ts);
        request.set_et_type(::rtidb::api::GetType::kSubKeyGe);
        code = tablet_impl.GetIndex(expired_ts, 0,
                                    ::rtidb::api::TTLType::kAbsoluteTime, it,
                                    &request, meta, &value, &ts);
        ASSERT_EQ(0, code);
        ASSERT_EQ(ts, 800 + base_ts);
        ASSERT_EQ(value, "value800");
    }

    // get the st 800 kSubKeyLe
    {
        // for the legacy
        ::rtidb::api::GetRequest request;
        request.set_ts(899 + base_ts);
        request.set_type(::rtidb::api::GetType::kSubKeyLe);
        request.set_et(800 + base_ts);
        request.set_et_type(::rtidb::api::GetType::kSubKeyGt);
        code = tablet_impl.GetIndex(expired_ts, 0,
                                    ::rtidb::api::TTLType::kAbsoluteTime, it,
                                    &request, meta, &value, &ts);
        ASSERT_EQ(1, code);
    }
}

void RunGetLatestIndexAssert(::rtidb::storage::TableIterator* it) {
    ::rtidb::tablet::TabletImpl tablet_impl;
    std::string value;
    uint64_t ts;
    int32_t code = 0;
    ::rtidb::api::TableMeta meta;
    // get the st kSubKeyGt
    {
        // for the legacy
        ::rtidb::api::GetRequest request;
        request.set_ts(1100);
        request.set_type(::rtidb::api::GetType::kSubKeyGt);
        request.set_et(1100);
        request.set_et_type(::rtidb::api::GetType::kSubKeyEq);
        code = tablet_impl.GetIndex(0, 10, ::rtidb::api::TTLType::kLatestTime,
                                    it, &request, meta, &value, &ts);
        ASSERT_EQ(0, code);
        ASSERT_EQ(ts, 1900);
        ASSERT_EQ(value, "value900");
    }

    // get the st == et
    {
        ::rtidb::api::GetRequest request;
        request.set_ts(1100);
        request.set_type(::rtidb::api::GetType::kSubKeyEq);
        request.set_et(1100);
        request.set_et_type(::rtidb::api::GetType::kSubKeyEq);
        code = tablet_impl.GetIndex(0, 10, ::rtidb::api::TTLType::kLatestTime,
                                    it, &request, meta, &value, &ts);
        ASSERT_EQ(0, code);
        ASSERT_EQ(ts, 1100);
        ASSERT_EQ(value, "value100");
    }

    // get the st < et
    {
        ::rtidb::api::GetRequest request;
        request.set_ts(1100);
        request.set_type(::rtidb::api::GetType::kSubKeyEq);
        request.set_et(1101);
        request.set_et_type(::rtidb::api::GetType::kSubKeyEq);
        code = tablet_impl.GetIndex(0, 10, ::rtidb::api::TTLType::kLatestTime,
                                    it, &request, meta, &value, &ts);
        ASSERT_EQ(-1, code);
    }

    // get the st > et
    {
        ::rtidb::api::GetRequest request;
        request.set_ts(1101);
        request.set_type(::rtidb::api::GetType::kSubKeyEq);
        request.set_et(1100);
        request.set_et_type(::rtidb::api::GetType::kSubKeyEq);
        code = tablet_impl.GetIndex(0, 10, ::rtidb::api::TTLType::kLatestTime,
                                    it, &request, meta, &value, &ts);
        ASSERT_EQ(-1, code);
    }

    // get the st > et
    {
        ::rtidb::api::GetRequest request;
        request.set_ts(1201);
        request.set_type(::rtidb::api::GetType::kSubKeyLe);
        request.set_et(1200);
        request.set_et_type(::rtidb::api::GetType::kSubKeyEq);
        code = tablet_impl.GetIndex(0, 10, ::rtidb::api::TTLType::kLatestTime,
                                    it, &request, meta, &value, &ts);
        ASSERT_EQ(0, code);
        ASSERT_EQ(ts, 1200);
        ASSERT_EQ(value, "value200");
    }
}

TEST_F(TabletFuncTest, GetLatestIndex_default_iterator) {
    ::rtidb::storage::Table* table;
    CreateBaseTable(table, ::rtidb::api::TTLType::kLatestTime, 10, 1000);
    ::rtidb::storage::Ticket ticket;
    ::rtidb::storage::TableIterator* it = table->NewIterator("card0", ticket);
    RunGetLatestIndexAssert(it);
}

TEST_F(TabletFuncTest, GetLatestIndex_ts0_iterator) {
    ::rtidb::storage::Table* table = NULL;
    CreateBaseTable(table, ::rtidb::api::TTLType::kLatestTime, 10, 1000);
    ::rtidb::storage::Ticket ticket;
    ::rtidb::storage::TableIterator* it =
        table->NewIterator(0, 0, "card0", ticket);
    RunGetLatestIndexAssert(it);
}

TEST_F(TabletFuncTest, GetLatestIndex_ts1_iterator) {
    ::rtidb::storage::Table* table = NULL;
    CreateBaseTable(table, ::rtidb::api::TTLType::kLatestTime, 10, 1000);
    ::rtidb::storage::Ticket ticket;
    ::rtidb::storage::TableIterator* it =
        table->NewIterator(0, 1, "card0", ticket);
    RunGetLatestIndexAssert(it);
}

TEST_F(TabletFuncTest, GetTimeIndex_default_iterator) {
    uint64_t base_ts = ::baidu::common::timer::get_micros();
    ::rtidb::storage::Table* table = NULL;
    CreateBaseTable(table, ::rtidb::api::TTLType::kAbsoluteTime, 1000, base_ts);
    ::rtidb::storage::Ticket ticket;
    ::rtidb::storage::TableIterator* it = table->NewIterator("card0", ticket);
    RunGetTimeIndexAssert(it, base_ts, base_ts - 100);
}

TEST_F(TabletFuncTest, GetTimeIndex_ts0_iterator) {
    uint64_t base_ts = ::baidu::common::timer::get_micros();
    ::rtidb::storage::Table* table = NULL;
    CreateBaseTable(table, ::rtidb::api::TTLType::kAbsoluteTime, 1000, base_ts);
    ::rtidb::storage::Ticket ticket;
    ::rtidb::storage::TableIterator* it =
        table->NewIterator(0, 0, "card0", ticket);
    RunGetTimeIndexAssert(it, base_ts, base_ts - 100);
}

TEST_F(TabletFuncTest, GetTimeIndex_ts1_iterator) {
    uint64_t base_ts = ::baidu::common::timer::get_micros();
    ::rtidb::storage::Table* table = NULL;
    CreateBaseTable(table, ::rtidb::api::TTLType::kAbsoluteTime, 1000, base_ts);
    ::rtidb::storage::Ticket ticket;
    ::rtidb::storage::TableIterator* it =
        table->NewIterator(0, 1, "card0", ticket);
    RunGetTimeIndexAssert(it, base_ts, base_ts - 100);
}

}  // namespace tablet
}  // namespace rtidb

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    srand(time(NULL));
    return RUN_ALL_TESTS();
}
