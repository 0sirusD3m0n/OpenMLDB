/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * codegen_base_test.cc
 *
 * Author: chenjing
 * Date: 2020/2/14
 *--------------------------------------------------------------------------
 **/

#include "codegen/codegen_base_test.h"
#include <proto/fe_type.pb.h>
#include "codec/fe_row_codec.h"
namespace fesql {
namespace codegen {

using fesql::codec::ArrayListV;
using fesql::codec::Row;

void BuildBuf(int8_t** buf, uint32_t* size) {
    ::fesql::type::TableDef table;
    table.set_name("t1");
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

    codec::RowBuilder builder(table.columns());
    uint32_t total_size = builder.CalTotalLength(1);
    int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
    builder.SetBuffer(ptr, total_size);
    builder.AppendInt32(32);
    builder.AppendInt16(16);
    builder.AppendFloat(2.1f);
    builder.AppendDouble(3.1);
    builder.AppendInt64(64);
    builder.AppendString("1", 1);
    *buf = ptr;
    *size = total_size;
}

void BuildWindow(std::vector<Row>& rows,  // NOLINT
                 int8_t** buf) {
    ::fesql::type::TableDef table;
    table.set_name("t1");
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

    {
        codec::RowBuilder builder(table.columns());
        std::string str = "1";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));

        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(32);
        builder.AppendInt16(16);
        builder.AppendFloat(2.1f);
        builder.AppendDouble(3.1);
        builder.AppendInt64(64);
        builder.AppendString(str.c_str(), 1);
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "22";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(32);
        builder.AppendInt16(16);
        builder.AppendFloat(2.1f);
        builder.AppendDouble(3.1);
        builder.AppendInt64(64);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "333";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(32);
        builder.AppendInt16(16);
        builder.AppendFloat(2.1f);
        builder.AppendDouble(3.1);
        builder.AppendInt64(64);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "4444";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(32);
        builder.AppendInt16(16);
        builder.AppendFloat(2.1f);
        builder.AppendDouble(3.1);
        builder.AppendInt64(64);
        builder.AppendString("4444", str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "a";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(32);
        builder.AppendInt16(16);
        builder.AppendFloat(2.1f);
        builder.AppendDouble(3.1);
        builder.AppendInt64(64);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }

    ArrayListV<Row>* w =
        new ArrayListV<Row>(&rows);
    *buf = reinterpret_cast<int8_t*>(w);
}

void BuildWindow2(std::vector<Row>& rows,  // NOLINT
                  int8_t** buf) {
    ::fesql::type::TableDef table;
    table.set_name("t1");
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

    {
        codec::RowBuilder builder(table.columns());
        std::string str = "1";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));

        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(1);
        builder.AppendInt16(2);
        builder.AppendFloat(3.1f);
        builder.AppendDouble(4.1);
        builder.AppendInt64(5);
        builder.AppendString(str.c_str(), 1);
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "22";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(11);
        builder.AppendInt16(22);
        builder.AppendFloat(33.1f);
        builder.AppendDouble(44.1);
        builder.AppendInt64(55);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "333";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(111);
        builder.AppendInt16(222);
        builder.AppendFloat(333.1f);
        builder.AppendDouble(444.1);
        builder.AppendInt64(555);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str = "4444";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(1111);
        builder.AppendInt16(2222);
        builder.AppendFloat(3333.1f);
        builder.AppendDouble(4444.1);
        builder.AppendInt64(5555);
        builder.AppendString("4444", str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }
    {
        codec::RowBuilder builder(table.columns());
        std::string str =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "a";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(11111);
        builder.AppendInt16(22222);
        builder.AppendFloat(33333.1f);
        builder.AppendDouble(44444.1);
        builder.AppendInt64(55555);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(Row(base::RefCountedSlice::Create(ptr, total_size)));
    }

    ArrayListV<Row> *w = new ArrayListV<Row>(&rows);
    *buf = reinterpret_cast<int8_t*>(w);
}
}  // namespace codegen
}  // namespace fesql
