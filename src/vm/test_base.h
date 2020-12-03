/*
 * test_base.h
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

#ifndef SRC_VM_TEST_BASE_H_
#define SRC_VM_TEST_BASE_H_

#include <memory>
#include <sstream>
#include <string>
#include "glog/logging.h"
#include "plan/planner.h"
#include "tablet/tablet_catalog.h"
#include "vm/catalog.h"
#include "vm/engine.h"

namespace fesql {
namespace vm {

void BuildTableDef(::fesql::type::TableDef& table) {  // NOLINT
    table.set_name("t1");
    table.set_catalog("db");
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("col0");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt32);
        column->set_name("col1");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt16);
        column->set_name("col2");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kFloat);
        column->set_name("col3");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kDouble);
        column->set_name("col4");
    }

    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt64);
        column->set_name("col5");
    }

    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("col6");
    }
}

void BuildTableA(::fesql::type::TableDef& table) {  // NOLINT
    table.set_name("ta");
    table.set_catalog("db");
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("c0");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt32);
        column->set_name("c1");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt16);
        column->set_name("c2");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kFloat);
        column->set_name("c3");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kDouble);
        column->set_name("c4");
    }

    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt64);
        column->set_name("c5");
    }

    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("c6");
    }
}

void BuildTableT2Def(::fesql::type::TableDef& table) {  // NOLINT
    table.set_name("t2");
    table.set_catalog("db");
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("str0");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("str1");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kFloat);
        column->set_name("col3");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kDouble);
        column->set_name("col4");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt16);
        column->set_name("col2");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt32);
        column->set_name("col1");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt64);
        column->set_name("col5");
    }
}
void BuildBuf(int8_t** buf, uint32_t* size) {
    ::fesql::type::TableDef table;
    BuildTableDef(table);
    codec::RowBuilder builder(table.columns());
    uint32_t total_size = builder.CalTotalLength(2);
    int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
    builder.SetBuffer(ptr, total_size);
    builder.AppendString("0", 1);
    builder.AppendInt32(32);
    builder.AppendInt16(16);
    builder.AppendFloat(2.1f);
    builder.AppendDouble(3.1);
    builder.AppendInt64(64);
    builder.AppendString("1", 1);
    *buf = ptr;
    *size = total_size;
}
void BuildT2Buf(int8_t** buf, uint32_t* size) {
    ::fesql::type::TableDef table;
    BuildTableT2Def(table);
    codec::RowBuilder builder(table.columns());
    uint32_t total_size = builder.CalTotalLength(2);
    int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
    builder.SetBuffer(ptr, total_size);
    builder.AppendString("A", 1);
    builder.AppendString("B", 1);
    builder.AppendFloat(22.1f);
    builder.AppendDouble(33.1);
    builder.AppendInt16(160);
    builder.AppendInt32(32);
    builder.AppendInt64(640);
    *buf = ptr;
    *size = total_size;
}

void BuildRows(::fesql::type::TableDef& table,  // NOLINT
               std::vector<Row>& rows) {        // NOLINT
    BuildTableDef(table);
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "1";
        std::string str0 = "0";
        uint32_t total_size = builder.CalTotalLength(str.size() + str0.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));

        builder.SetBuffer(ptr, total_size);
        builder.AppendString("0", 1);
        builder.AppendInt32(1);
        builder.AppendInt16(5);
        builder.AppendFloat(1.1f);
        builder.AppendDouble(11.1);
        builder.AppendInt64(1);
        builder.AppendString(str.c_str(), 1);
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "22";
        std::string str0 = "0";
        uint32_t total_size = builder.CalTotalLength(str.size() + str0.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendString("0", 1);
        builder.AppendInt32(2);
        builder.AppendInt16(5);
        builder.AppendFloat(2.2f);
        builder.AppendDouble(22.2);
        builder.AppendInt64(2);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "333";
        std::string str0 = "0";
        uint32_t total_size = builder.CalTotalLength(str.size() + str0.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendString("1", 1);
        builder.AppendInt32(3);
        builder.AppendInt16(55);
        builder.AppendFloat(3.3f);
        builder.AppendDouble(33.3);
        builder.AppendInt64(1);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "4444";
        std::string str0 = "0";
        uint32_t total_size = builder.CalTotalLength(str.size() + str0.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendString("1", 1);
        builder.AppendInt32(4);
        builder.AppendInt16(55);
        builder.AppendFloat(4.4f);
        builder.AppendDouble(44.4);
        builder.AppendInt64(2);
        builder.AppendString("4444", str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "a";
        std::string str0 = "0";
        uint32_t total_size = builder.CalTotalLength(str.size() + str0.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendString("2", 1);
        builder.AppendInt32(5);
        builder.AppendInt16(55);
        builder.AppendFloat(5.5f);
        builder.AppendDouble(55.5);
        builder.AppendInt64(3);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
}
void BuildT2Rows(::fesql::type::TableDef& table,  // NOLINT
                 std::vector<Row>& rows) {        // NOLINT
    BuildTableT2Def(table);
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "A";
        std::string str0 = "0";
        uint32_t total_size = builder.CalTotalLength(str.size() + str0.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));

        builder.SetBuffer(ptr, total_size);
        builder.AppendString(str0.c_str(), 1);
        builder.AppendString(str.c_str(), 1);
        builder.AppendFloat(1.1f);
        builder.AppendDouble(11.1);
        builder.AppendInt16(50);
        builder.AppendInt32(1);
        builder.AppendInt64(1);
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "BB";
        std::string str0 = "0";
        uint32_t total_size = builder.CalTotalLength(str.size() + str0.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendString(str0.c_str(), 1);
        builder.AppendString(str.c_str(), str.size());
        builder.AppendFloat(2.2f);
        builder.AppendDouble(22.2);
        builder.AppendInt16(50);
        builder.AppendInt32(2);
        builder.AppendInt64(2);
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "CCC";
        std::string str0 = "1";
        uint32_t total_size = builder.CalTotalLength(str.size() + str0.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendString(str0.c_str(), 1);
        builder.AppendString(str.c_str(), str.size());
        builder.AppendFloat(3.3f);
        builder.AppendDouble(33.3);
        builder.AppendInt16(550);
        builder.AppendInt32(3);
        builder.AppendInt64(1);
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "DDDD";
        std::string str0 = "1";
        uint32_t total_size = builder.CalTotalLength(str.size() + str0.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendString(str0.c_str(), 1);
        builder.AppendString(str.c_str(), str.size());
        builder.AppendFloat(4.4f);
        builder.AppendDouble(44.4);
        builder.AppendInt16(550);
        builder.AppendInt32(4);
        builder.AppendInt64(2);
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "EEEEE";
        std::string str0 = "2";
        uint32_t total_size = builder.CalTotalLength(str.size() + str0.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendString(str0.c_str(), 1);
        builder.AppendString(str.c_str(), str.size());
        builder.AppendFloat(5.5f);
        builder.AppendDouble(55.5);
        builder.AppendInt16(550);
        builder.AppendInt32(5);
        builder.AppendInt64(3);
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
}
void ExtractExprListFromSimpleSQL(::fesql::node::NodeManager* nm,
                                  const std::string& sql,
                                  node::ExprListNode* output) {
    std::cout << sql << std::endl;
    ::fesql::node::PlanNodeList plan_trees;
    ::fesql::base::Status base_status;
    ::fesql::plan::SimplePlanner planner(nm);
    ::fesql::parser::FeSQLParser parser;
    ::fesql::node::NodePointVector parser_trees;
    parser.parse(sql, parser_trees, nm, base_status);
    ASSERT_EQ(0, base_status.code);
    if (planner.CreatePlanTree(parser_trees, plan_trees, base_status) == 0) {
        std::cout << base_status.str();
        std::cout << *(plan_trees[0]) << std::endl;
    } else {
        std::cout << base_status.str();
    }
    ASSERT_EQ(0, base_status.code);
    std::cout.flush();

    ASSERT_EQ(node::kPlanTypeProject, plan_trees[0]->GetChildren()[0]->type_);
    auto project_plan_node =
        dynamic_cast<node::ProjectPlanNode*>(plan_trees[0]->GetChildren()[0]);
    ASSERT_EQ(1u, project_plan_node->project_list_vec_.size());

    auto project_list = dynamic_cast<node::ProjectListNode*>(
        project_plan_node->project_list_vec_[0]);
    for (auto project : project_list->GetProjects()) {
        output->AddChild(
            dynamic_cast<node::ProjectNode*>(project)->GetExpression());
    }
}
void ExtractExprFromSimpleSQL(::fesql::node::NodeManager* nm,
                              const std::string& sql, node::ExprNode** output) {
    std::cout << sql << std::endl;
    ::fesql::node::PlanNodeList plan_trees;
    ::fesql::base::Status base_status;
    ::fesql::plan::SimplePlanner planner(nm);
    ::fesql::parser::FeSQLParser parser;
    ::fesql::node::NodePointVector parser_trees;
    parser.parse(sql, parser_trees, nm, base_status);
    ASSERT_EQ(0, base_status.code);
    if (planner.CreatePlanTree(parser_trees, plan_trees, base_status) == 0) {
        std::cout << base_status.str();
        std::cout << *(plan_trees[0]) << std::endl;
    } else {
        std::cout << base_status.str();
    }
    ASSERT_EQ(0, base_status.code);
    std::cout.flush();

    ASSERT_EQ(node::kPlanTypeProject, plan_trees[0]->GetChildren()[0]->type_);
    auto project_plan_node =
        dynamic_cast<node::ProjectPlanNode*>(plan_trees[0]->GetChildren()[0]);
    ASSERT_EQ(1u, project_plan_node->project_list_vec_.size());

    auto project_list = dynamic_cast<node::ProjectListNode*>(
        project_plan_node->project_list_vec_[0]);
    auto project =
        dynamic_cast<node::ProjectNode*>(project_list->GetProjects()[0]);
    *output = project->GetExpression();
}

bool AddTable(const std::shared_ptr<tablet::TabletCatalog>& catalog,
              const fesql::type::TableDef& table_def,
              std::shared_ptr<fesql::storage::Table> table) {
    std::shared_ptr<tablet::TabletTableHandler> handler(
        new tablet::TabletTableHandler(table_def.columns(), table_def.name(),
                                       table_def.catalog(), table_def.indexes(),
                                       table));
    bool ok = handler->Init();
    if (!ok) {
        return false;
    }
    return catalog->AddTable(handler);
}

bool AddTable(const std::shared_ptr<tablet::TabletCatalog>& catalog,
              const fesql::type::TableDef& table_def,
              std::shared_ptr<fesql::storage::Table> table, Engine* engine) {
    auto local_tablet =
        std::shared_ptr<vm::Tablet>(new vm::LocalTablet(engine,
                                                        std::shared_ptr<CompileInfoCache>()));
    std::shared_ptr<tablet::TabletTableHandler> handler(
        new tablet::TabletTableHandler(table_def.columns(), table_def.name(),
                                       table_def.catalog(), table_def.indexes(),
                                       table, local_tablet));
    bool ok = handler->Init();
    if (!ok) {
        return false;
    }
    return catalog->AddTable(handler);
}

std::shared_ptr<tablet::TabletCatalog> BuildCommonCatalog(
    const fesql::type::TableDef& table_def,
    std::shared_ptr<fesql::storage::Table> table) {
    std::shared_ptr<tablet::TabletCatalog> catalog(new tablet::TabletCatalog());
    bool ok = catalog->Init();
    if (!ok) {
        return std::shared_ptr<tablet::TabletCatalog>();
    }
    if (!AddTable(catalog, table_def, table)) {
        return std::shared_ptr<tablet::TabletCatalog>();
    }
    type::Database database;
    database.set_name(table_def.catalog());
    *database.add_tables() = table_def;
    catalog->AddDB(database);
    return catalog;
}

std::shared_ptr<tablet::TabletCatalog> BuildCommonCatalog() {
    std::shared_ptr<tablet::TabletCatalog> catalog(new tablet::TabletCatalog());
    return catalog;
}
std::shared_ptr<tablet::TabletCatalog> BuildCommonCatalog(
    const fesql::type::TableDef& table_def) {
    std::shared_ptr<::fesql::storage::Table> table(
        new ::fesql::storage::Table(1, 1, table_def));
    return BuildCommonCatalog(table_def, table);
}

void PrintSchema(std::ostringstream& ss, const Schema& schema) {
    for (int32_t i = 0; i < schema.size(); i++) {
        if (i > 0) {
            ss << "\n";
        }
        const type::ColumnDef& column = schema.Get(i);
        ss << column.name() << " " << type::Type_Name(column.type());
    }
}

void PrintSchema(const Schema& schema) {
    std::ostringstream ss;
    PrintSchema(ss, schema);
    LOG(INFO) << "\n" << ss.str();
}

}  // namespace vm
}  // namespace fesql

#endif  // SRC_VM_TEST_BASE_H_
