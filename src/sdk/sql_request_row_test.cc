/*
 * sql_request_row_test.cc
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

#include "sdk/sql_request_row.h"

#include "codec/fe_row_codec.h"
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "sdk/base_impl.h"
#include "vm/catalog.h"

namespace rtidb {
namespace sdk {

class SQLRequestRowTest : public ::testing::Test {};

TEST_F(SQLRequestRowTest, str_null) {
    ::fesql::vm::Schema schema;
    {
        ::fesql::type::ColumnDef* column = schema.Add();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("col0");
    }
    ::fesql::sdk::SchemaImpl* schema_impl =
        new ::fesql::sdk::SchemaImpl(schema);
    std::shared_ptr<::fesql::sdk::Schema> schema_shared(schema_impl);
    SQLRequestRow rr(schema_shared);
    rr.Init(0);
    ASSERT_TRUE(rr.AppendNULL());
    ASSERT_TRUE(rr.Build());
}

TEST_F(SQLRequestRowTest, not_null) {
    ::fesql::vm::Schema schema;
    {
        ::fesql::type::ColumnDef* column = schema.Add();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("col0");
        column->set_is_not_null(true);
    }
    ::fesql::sdk::SchemaImpl* schema_impl =
        new ::fesql::sdk::SchemaImpl(schema);
    std::shared_ptr<::fesql::sdk::Schema> schema_shared(schema_impl);
    SQLRequestRow rr(schema_shared);
    rr.Init(0);
    ASSERT_FALSE(rr.AppendNULL());
    ASSERT_FALSE(rr.Build());
}

TEST_F(SQLRequestRowTest, invalid_str_len) {
    ::fesql::vm::Schema schema;
    {
        ::fesql::type::ColumnDef* column = schema.Add();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("col0");
    }
    ::fesql::sdk::SchemaImpl* schema_impl =
        new ::fesql::sdk::SchemaImpl(schema);
    std::shared_ptr<::fesql::sdk::Schema> schema_shared(schema_impl);
    SQLRequestRow rr(schema_shared);
    rr.Init(1);
    std::string hello = "hello";
    ASSERT_FALSE(rr.AppendString(hello));
    ASSERT_FALSE(rr.Build());
}

void InitSimpleSchema(::fesql::vm::Schema* schema) {
    {
        ::fesql::type::ColumnDef* column = schema->Add();
        column->set_type(::fesql::type::kInt32);
        column->set_name("col0");
    }
    {
        ::fesql::type::ColumnDef* column = schema->Add();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("col1");
    }
    {
        ::fesql::type::ColumnDef* column = schema->Add();
        column->set_type(::fesql::type::kInt64);
        column->set_name("col2");
    }
}

TEST_F(SQLRequestRowTest, normal_test) {
    ::fesql::vm::Schema schema;
    InitSimpleSchema(&schema);

    ::fesql::sdk::SchemaImpl* schema_impl =
        new ::fesql::sdk::SchemaImpl(schema);
    std::shared_ptr<::fesql::sdk::Schema> schema_shared(schema_impl);
    SQLRequestRow rr(schema_shared);
    ASSERT_TRUE(rr.Init(5));
    ASSERT_TRUE(rr.AppendInt32(32));
    ASSERT_TRUE(rr.AppendString("hello"));
    ASSERT_TRUE(rr.AppendInt64(64));
    ASSERT_TRUE(rr.Build());
    ::fesql::codec::RowView rv(schema);
    bool ok = rv.Reset(reinterpret_cast<const int8_t*>(rr.GetRow().c_str()),
                       rr.GetRow().size());
    ASSERT_TRUE(ok);
    int32_t i32 = 0;
    rv.GetInt32(0, &i32);
    ASSERT_EQ(32, i32);
}


class SQLRequestRowBatchTest : public ::testing::Test {
 public:
    SQLRequestRowBatch* NewSimpleBatch(const std::vector<size_t>& common_column_indices) {
        ::fesql::vm::Schema schema;
        InitSimpleSchema(&schema);

        ::fesql::sdk::SchemaImpl* schema_impl =
            new ::fesql::sdk::SchemaImpl(schema);
        std::shared_ptr<::fesql::sdk::Schema> schema_shared(schema_impl);

        auto r1 = std::make_shared<SQLRequestRow>(schema_shared);
        r1->Init(5);
        r1->AppendInt32(32);
        r1->AppendString("hello");
        r1->AppendInt64(64);
        r1->Build();

        auto r2 = std::make_shared<SQLRequestRow>(schema_shared);
        r2->Init(5);
        r2->AppendInt32(32);
        r2->AppendString("world");
        r2->AppendInt64(64);
        r2->Build();

        common_schema.Clear();
        non_common_schema.Clear();
        for (int i = 0; i < schema.size(); ++i) {
            if (std::find(common_column_indices.begin(), common_column_indices.end(), i)
                    != common_column_indices.end() &&
                common_column_indices.size() != static_cast<size_t>(schema.size())) {
                *common_schema.Add() = schema.Get(i);
            } else {
                *non_common_schema.Add() = schema.Get(i);
            }
        }

        auto indice_set = std::make_shared<ColumnIndicesSet>(schema_shared);
        for (size_t idx : common_column_indices) {
            indice_set->AddCommonColumnIdx(idx);
        }

        auto batch = new SQLRequestRowBatch(schema_shared, indice_set);
        batch->AddRow(r1);
        batch->AddRow(r2);
        return batch;
    }

    ::fesql::vm::Schema common_schema;
    ::fesql::vm::Schema non_common_schema;
};


TEST_F(SQLRequestRowBatchTest, batch_test_non_trival) {
    std::vector<size_t> common_indices = {0, 2};
    SQLRequestRowBatch* batch = NewSimpleBatch(common_indices);

    ::fesql::codec::RowView common_view(common_schema);
    ::fesql::codec::RowView non_common_view(non_common_schema);

    const auto common_slice = batch->GetCommonSlice();
    common_view.Reset(reinterpret_cast<const int8_t*>(common_slice->c_str()),
                      common_slice->size());
    ASSERT_EQ(common_view.GetInt32Unsafe(0), 32);
    ASSERT_EQ(common_view.GetInt64Unsafe(1), 64);

    const auto non_common_slice1 = batch->GetNonCommonSlice(0);
    non_common_view.Reset(reinterpret_cast<const int8_t*>(non_common_slice1->c_str()),
                      non_common_slice1->size());
    ASSERT_EQ(non_common_view.GetStringUnsafe(0), "hello");

    const auto non_common_slice2 = batch->GetNonCommonSlice(1);
    non_common_view.Reset(reinterpret_cast<const int8_t*>(non_common_slice2->c_str()),
                      non_common_slice2->size());
    ASSERT_EQ(non_common_view.GetStringUnsafe(0), "world");
}

TEST_F(SQLRequestRowBatchTest, batch_test_trival) {
    std::vector<size_t> common_indices = {0, 1, 2};
    SQLRequestRowBatch* batch = NewSimpleBatch(common_indices);

    ::fesql::codec::RowView non_common_view(non_common_schema);

    const auto non_common_slice1 = batch->GetNonCommonSlice(0);
    non_common_view.Reset(reinterpret_cast<const int8_t*>(non_common_slice1->c_str()),
                      non_common_slice1->size());
    ASSERT_EQ(non_common_view.GetStringUnsafe(1), "hello");

    const auto non_common_slice2 = batch->GetNonCommonSlice(1);
    non_common_view.Reset(reinterpret_cast<const int8_t*>(non_common_slice2->c_str()),
                      non_common_slice2->size());
    ASSERT_EQ(non_common_view.GetStringUnsafe(1), "world");
}

TEST_F(SQLRequestRowBatchTest, batch_test_all_common) {
    std::vector<size_t> common_indices = {};
    SQLRequestRowBatch* batch = NewSimpleBatch(common_indices);

    ::fesql::codec::RowView non_common_view(non_common_schema);

    const auto non_common_slice1 = batch->GetNonCommonSlice(0);
    non_common_view.Reset(reinterpret_cast<const int8_t*>(non_common_slice1->c_str()),
                      non_common_slice1->size());
    ASSERT_EQ(non_common_view.GetStringUnsafe(1), "hello");

    const auto non_common_slice2 = batch->GetNonCommonSlice(1);
    non_common_view.Reset(reinterpret_cast<const int8_t*>(non_common_slice2->c_str()),
                      non_common_slice2->size());
    ASSERT_EQ(non_common_view.GetStringUnsafe(1), "world");
}


}  // namespace sdk
}  // namespace rtidb

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
