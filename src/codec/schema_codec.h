//
// schema_codec.h
// Copyright (C) 2017 4paradigm.com
// Author vagrant
// Date 2017-09-23
//

#ifndef SRC_CODEC_SCHEMA_CODEC_H_
#define SRC_CODEC_SCHEMA_CODEC_H_

#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "base/status.h"
#include "codec/codec.h"
#include "proto/name_server.pb.h"
#include "boost/lexical_cast.hpp"
#include "codec/field_codec.h"

namespace rtidb {
namespace codec {
// 1M
const uint32_t MAX_ROW_BYTE_SIZE = 1024 * 1024;
const uint32_t HEADER_BYTE_SIZE = 3;

const std::string NONETOKEN = "None#*@!";  // NOLINT
const std::string DEFAULT_LONG = "1";      // NOLINT

static const std::unordered_map<std::string, ::rtidb::type::DataType>
    DATA_TYPE_MAP = {{"bool", ::rtidb::type::kBool},
                     {"smallint", ::rtidb::type::kSmallInt},
                     {"uint16", ::rtidb::type::kSmallInt},
                     {"int16", ::rtidb::type::kSmallInt},
                     {"int", ::rtidb::type::kInt},
                     {"int32", ::rtidb::type::kInt},
                     {"uint32", ::rtidb::type::kInt},
                     {"bigint", ::rtidb::type::kBigInt},
                     {"int64", ::rtidb::type::kBigInt},
                     {"uint64", ::rtidb::type::kBigInt},
                     {"float", ::rtidb::type::kFloat},
                     {"double", ::rtidb::type::kDouble},
                     {"varchar", ::rtidb::type::kVarchar},
                     {"string", rtidb::type::DataType::kString},
                     {"date", ::rtidb::type::kDate},
                     {"timestamp", ::rtidb::type::kTimestamp},
                     {"blob", ::rtidb::type::kBlob}};

static const std::unordered_map<std::string, ::rtidb::type::IndexType>
    INDEX_TYPE_MAP = {{"unique", ::rtidb::type::kUnique},
                      {"nounique", ::rtidb::type::kNoUnique},
                      {"primarykey", ::rtidb::type::kPrimaryKey},
                      {"autogen", ::rtidb::type::kAutoGen},
                      {"increment", ::rtidb::type::kIncrement}};

static const std::unordered_map<::rtidb::type::DataType, std::string>
    DATA_TYPE_STR_MAP = {{::rtidb::type::kBool, "bool"},
                         {::rtidb::type::kSmallInt, "smallInt"},
                         {::rtidb::type::kInt, "int"},
                         {::rtidb::type::kBigInt, "bigInt"},
                         {::rtidb::type::kFloat, "float"},
                         {::rtidb::type::kDouble, "double"},
                         {::rtidb::type::kTimestamp, "timestamp"},
                         {::rtidb::type::kDate, "date"},
                         {::rtidb::type::kVarchar, "varchar"},
                         {::rtidb::type::kString, "string"},
                         {::rtidb::type::kBlob, "blob"}};

enum ColType {
    kString = 0,
    kFloat = 1,
    kInt32 = 2,
    kInt64 = 3,
    kDouble = 4,
    kNull = 5,
    kUInt32 = 6,
    kUInt64 = 7,
    kTimestamp = 8,
    kDate = 9,
    kInt16 = 10,
    kUInt16 = 11,
    kBool = 12,
    kEmptyString = 100,
    kUnknown = 200
};

struct Column {
    ColType type;
    std::string buffer;
};

struct ColumnDesc {
    ColType type;
    std::string name;
    bool add_ts_idx;
    bool is_ts_col;
};

class SchemaCodec {
 public:
    bool Encode(const std::vector<ColumnDesc>& columns,
                std::string& buffer) {  // NOLINT
        uint32_t byte_size = GetSize(columns);
        if (byte_size > MAX_ROW_BYTE_SIZE) {
            return false;
        }
        buffer.resize(byte_size);
        char* cbuffer = reinterpret_cast<char*>(&(buffer[0]));
        for (uint32_t i = 0; i < columns.size(); i++) {
            uint8_t type = (uint8_t)columns[i].type;
            memcpy(cbuffer, static_cast<const void*>(&type), 1);
            cbuffer += 1;
            uint8_t add_ts_idx = 0;
            if (columns[i].add_ts_idx) {
                add_ts_idx = 1;
            }
            memcpy(cbuffer, static_cast<const void*>(&add_ts_idx), 1);
            cbuffer += 1;
            const std::string& name = columns[i].name;
            if (name.size() >= 128) {
                return false;
            }
            uint8_t name_size = (uint8_t)name.size();
            memcpy(cbuffer, static_cast<const void*>(&name_size), 1);
            cbuffer += 1;
            memcpy(cbuffer, static_cast<const void*>(name.c_str()), name_size);
            cbuffer += name_size;
        }
        return true;
    }

    void Decode(const std::string& schema,
                std::vector<ColumnDesc>& columns) {  // NOLINT
        const char* buffer = schema.c_str();
        uint32_t read_size = 0;
        while (read_size < schema.size()) {
            if (schema.size() - read_size < HEADER_BYTE_SIZE) {
                return;
            }
            uint8_t type = 0;
            memcpy(static_cast<void*>(&type), buffer, 1);
            buffer += 1;
            uint8_t add_ts_idx = 0;
            memcpy(static_cast<void*>(&add_ts_idx), buffer, 1);
            buffer += 1;
            uint8_t name_size = 0;
            memcpy(static_cast<void*>(&name_size), buffer, 1);
            buffer += 1;
            uint32_t total_size = HEADER_BYTE_SIZE + name_size;
            if (schema.size() - read_size < total_size) {
                return;
            }
            std::string name(buffer, name_size);
            buffer += name_size;
            read_size += total_size;
            ColumnDesc desc;
            desc.name = name;
            desc.type = static_cast<ColType>(type);
            desc.add_ts_idx = add_ts_idx;
            columns.push_back(desc);
        }
    }

    static ::rtidb::codec::ColType ConvertType(const std::string& raw_type) {
        ::rtidb::codec::ColType type;
        if (raw_type == "int32") {
            type = ::rtidb::codec::ColType::kInt32;
        } else if (raw_type == "int64") {
            type = ::rtidb::codec::ColType::kInt64;
        } else if (raw_type == "uint32") {
            type = ::rtidb::codec::ColType::kUInt32;
        } else if (raw_type == "uint64") {
            type = ::rtidb::codec::ColType::kUInt64;
        } else if (raw_type == "float") {
            type = ::rtidb::codec::ColType::kFloat;
        } else if (raw_type == "double") {
            type = ::rtidb::codec::ColType::kDouble;
        } else if (raw_type == "string") {
            type = ::rtidb::codec::ColType::kString;
        } else if (raw_type == "timestamp") {
            type = ::rtidb::codec::ColType::kTimestamp;
        } else if (raw_type == "int16") {
            type = ::rtidb::codec::ColType::kInt16;
        } else if (raw_type == "uint16") {
            type = ::rtidb::codec::ColType::kUInt16;
        } else if (raw_type == "bool") {
            type = ::rtidb::codec::ColType::kBool;
        } else if (raw_type == "date") {
            type = ::rtidb::codec::ColType::kDate;
        } else {
            type = ::rtidb::codec::ColType::kUnknown;
        }
        return type;
    }

    static int ConvertColumnDesc(
        const ::rtidb::nameserver::TableInfo& table_info,
        std::vector<ColumnDesc>& columns) {  // NOLINT
        return ConvertColumnDesc(table_info, columns, 0);
    }

    static int ConvertColumnDesc(
        const ::rtidb::nameserver::TableInfo& table_info,
        std::vector<ColumnDesc>& columns, int modify_index) {  // NOLINT
        columns.clear();
        if (table_info.column_desc_v1_size() > 0) {
            if (modify_index > 0) {
                return ConvertColumnDesc(table_info.column_desc_v1(), columns,
                                         table_info.added_column_desc());
            }
            return ConvertColumnDesc(table_info.column_desc_v1(), columns);
        }
        for (int idx = 0; idx < table_info.column_desc_size(); idx++) {
            ::rtidb::codec::ColType type =
                ConvertType(table_info.column_desc(idx).type());
            if (type == ::rtidb::codec::ColType::kUnknown) {
                return -1;
            }
            ColumnDesc column_desc;
            column_desc.type = type;
            column_desc.name = table_info.column_desc(idx).name();
            column_desc.add_ts_idx = table_info.column_desc(idx).add_ts_idx();
            column_desc.is_ts_col = false;
            columns.push_back(column_desc);
        }
        if (modify_index > 0) {
            for (int idx = 0; idx < modify_index; idx++) {
                ::rtidb::codec::ColType type =
                    ConvertType(table_info.added_column_desc(idx).type());
                if (type == ::rtidb::codec::ColType::kUnknown) {
                    return -1;
                }
                ColumnDesc column_desc;
                column_desc.type = type;
                column_desc.name = table_info.added_column_desc(idx).name();
                column_desc.add_ts_idx = false;
                column_desc.is_ts_col = false;
                columns.push_back(column_desc);
            }
        }
        return 0;
    }

    static int ConvertColumnDesc(
        const google::protobuf::RepeatedPtrField<::rtidb::common::ColumnDesc>&
            column_desc_field,
        std::vector<ColumnDesc>& columns,  // NOLINT
        const google::protobuf::RepeatedPtrField<::rtidb::common::ColumnDesc>&
            added_column_field) {
        columns.clear();
        for (const auto& cur_column_desc : column_desc_field) {
            ::rtidb::codec::ColType type = ConvertType(cur_column_desc.type());
            if (type == ::rtidb::codec::ColType::kUnknown) {
                return -1;
            }
            ColumnDesc column_desc;
            column_desc.type = type;
            column_desc.name = cur_column_desc.name();
            column_desc.add_ts_idx = cur_column_desc.add_ts_idx();
            column_desc.is_ts_col = cur_column_desc.is_ts_col();
            columns.push_back(column_desc);
        }
        if (!added_column_field.empty()) {
            for (int idx = 0; idx < added_column_field.size(); idx++) {
                ::rtidb::codec::ColType type =
                    ConvertType(added_column_field.Get(idx).type());
                if (type == ::rtidb::codec::ColType::kUnknown) {
                    return -1;
                }
                ColumnDesc column_desc;
                column_desc.type = type;
                column_desc.name = added_column_field.Get(idx).name();
                column_desc.add_ts_idx = false;
                column_desc.is_ts_col = false;
                columns.push_back(column_desc);
            }
        }
        return 0;
    }

    static int ConvertColumnDesc(
        const google::protobuf::RepeatedPtrField<::rtidb::common::ColumnDesc>&
            column_desc_field,
        std::vector<ColumnDesc>& columns) {  // NOLINT
        google::protobuf::RepeatedPtrField<::rtidb::common::ColumnDesc>
            added_column_field;
        return ConvertColumnDesc(column_desc_field, columns,
                                 added_column_field);
    }

    static bool HasTSCol(const std::vector<ColumnDesc>& columns) {
        for (const auto& column_desc : columns) {
            if (column_desc.is_ts_col) {
                return true;
            }
        }
        return false;
    }

 private:
    // calc the total size of schema
    uint32_t GetSize(const std::vector<ColumnDesc>& columns) {
        uint32_t byte_size = 0;
        for (uint32_t i = 0; i < columns.size(); i++) {
            byte_size += (HEADER_BYTE_SIZE + columns[i].name.size());
        }
        return byte_size;
    }
};

class RowSchemaCodec {
 public:
    static int ConvertColumnDesc(
        const google::protobuf::RepeatedPtrField<::rtidb::common::ColumnDesc>&
            column_desc_field,
        google::protobuf::RepeatedPtrField<rtidb::common::ColumnDesc>&
            columns,  // NOLINT
        const google::protobuf::RepeatedPtrField<::rtidb::common::ColumnDesc>&
            added_column_field) {
        columns.Clear();
        for (const auto& cur_column_desc : column_desc_field) {
            rtidb::common::ColumnDesc* column_desc = columns.Add();
            column_desc->CopyFrom(cur_column_desc);
            if (!cur_column_desc.has_data_type()) {
                auto iter = DATA_TYPE_MAP.find(cur_column_desc.type());
                if (iter == DATA_TYPE_MAP.end()) {
                    return -1;
                } else {
                    column_desc->set_data_type(iter->second);
                }
            }
        }
        for (const auto& cur_column_desc : added_column_field) {
            rtidb::common::ColumnDesc* column_desc = columns.Add();
            column_desc->CopyFrom(cur_column_desc);
            if (!cur_column_desc.has_data_type()) {
                auto iter = DATA_TYPE_MAP.find(cur_column_desc.type());
                if (iter == DATA_TYPE_MAP.end()) {
                    return -1;
                } else {
                    column_desc->set_data_type(iter->second);
                }
            }
        }
        return 0;
    }

    static void GetSchemaData(
        const std::map<std::string, std::string>& columns_map,
        const Schema& schema, Schema& new_schema) {  // NOLINT
        for (int i = 0; i < schema.size(); i++) {
            const ::rtidb::common::ColumnDesc& col = schema.Get(i);
            const std::string& col_name = col.name();
            auto iter = columns_map.find(col_name);
            if (iter != columns_map.end()) {
                ::rtidb::common::ColumnDesc* tmp = new_schema.Add();
                tmp->CopyFrom(col);
            }
        }
    }

    static int32_t CalStrLength(
        const std::map<std::string, std::string>& str_map, const Schema& schema,
        ::rtidb::base::ResultMsg& rm) {  // NOLINT
        int32_t str_len = 0;
        for (int i = 0; i < schema.size(); i++) {
            const ::rtidb::common::ColumnDesc& col = schema.Get(i);
            if (col.data_type() == ::rtidb::type::kVarchar ||
                col.data_type() == ::rtidb::type::kString) {
                auto iter = str_map.find(col.name());
                if (iter == str_map.end()) {
                    rm.code = -1;
                    rm.msg = col.name() + " not in str_map";
                    return -1;
                }
                if (!col.not_null() &&
                    (iter->second == "null" || iter->second == NONETOKEN)) {
                    continue;
                } else if (iter->second == "null" ||
                           iter->second == NONETOKEN) {
                    rm.code = -1;
                    rm.msg = col.name() + " should not be null";
                    return -1;
                }
                str_len += iter->second.length();
            }
        }
        return str_len;
    }

    static ::rtidb::base::ResultMsg Encode(
        const std::map<std::string, std::string>& str_map, const Schema& schema,
        std::string& row) {  // NOLINT
        ::rtidb::base::ResultMsg rm;
        if (str_map.size() == 0 || schema.size() == 0 ||
            str_map.size() - schema.size() != 0) {
            rm.code = -1;
            rm.msg = "input error";
            return rm;
        }
        int32_t str_len = CalStrLength(str_map, schema, rm);
        if (str_len < 0) {
            return rm;
        }
        ::rtidb::codec::RowBuilder builder(schema);
        uint32_t size = builder.CalTotalLength(str_len);
        row.resize(size);
        builder.SetBuffer(reinterpret_cast<int8_t*>(&(row[0])), size);
        for (int i = 0; i < schema.size(); i++) {
            const ::rtidb::common::ColumnDesc& col = schema.Get(i);
            auto iter = str_map.find(col.name());
            if (iter == str_map.end()) {
                rm.code = -1;
                rm.msg = col.name() + " not in str_map";
                return rm;
            }
            if (!col.not_null() &&
                (iter->second == "null" || iter->second == NONETOKEN)) {
                builder.AppendNULL();
                continue;
            } else if (iter->second == "null" || iter->second == NONETOKEN) {
                rm.code = -1;
                rm.msg = col.name() + " should not be null";
                return rm;
            }
            bool ok = false;
            try {
                switch (col.data_type()) {
                    case rtidb::type::kString:
                    case rtidb::type::kVarchar:
                        ok = builder.AppendString(iter->second.c_str(),
                                                  iter->second.length());
                        break;
                    case rtidb::type::kBool:
                        ok = builder.AppendBool(
                            boost::lexical_cast<bool>(iter->second));
                        break;
                    case rtidb::type::kSmallInt:
                        ok = builder.AppendInt16(
                            boost::lexical_cast<uint16_t>(iter->second));
                        break;
                    case rtidb::type::kInt:
                        ok = builder.AppendInt32(
                            boost::lexical_cast<uint32_t>(iter->second));
                        break;
                    case rtidb::type::kBigInt:
                        ok = builder.AppendInt64(
                            boost::lexical_cast<uint64_t>(iter->second));
                        break;
                    case rtidb::type::kTimestamp:
                        ok = builder.AppendTimestamp(
                            boost::lexical_cast<uint64_t>(iter->second));
                        break;
                    case rtidb::type::kFloat:
                        ok = builder.AppendFloat(
                            boost::lexical_cast<float>(iter->second));
                        break;
                    case rtidb::type::kDouble:
                        ok = builder.AppendDouble(
                            boost::lexical_cast<double>(iter->second));
                        break;
                    default:
                        rm.code = -1;
                        rm.msg = "unsupported data type";
                        return rm;
                }
                if (!ok) {
                    rm.code = -1;
                    rm.msg = "append " +
                             ::rtidb::type::DataType_Name(col.data_type()) +
                             " error";
                    return rm;
                }
            } catch (std::exception const& e) {
                rm.code = -1;
                rm.msg = "input format error";
                return rm;
            }
        }
        rm.code = 0;
        rm.msg = "ok";
        return rm;
    }

    static void Decode(const google::protobuf::RepeatedPtrField<
                           rtidb::common::ColumnDesc>& schema,  // NOLINT
                       const std::string& value,
                       std::vector<std::string>& value_vec) {  // NOLINT
        rtidb::codec::RowView rv(
            schema, reinterpret_cast<int8_t*>(const_cast<char*>(&value[0])),
            value.size());
        Decode(schema, rv, value_vec);
    }

    static void Decode(const google::protobuf::RepeatedPtrField<
                           rtidb::common::ColumnDesc>& schema,  // NOLINT
                       rtidb::codec::RowView& rv,               // NOLINT
                       std::vector<std::string>& value_vec) {   // NOLINT
        for (int32_t i = 0; i < schema.size(); i++) {
            if (rv.IsNULL(i)) {
                value_vec.push_back(NONETOKEN);
                continue;
            }
            std::string col = "";
            auto type = schema.Get(i).data_type();
            if (type == rtidb::type::kInt) {
                int32_t val;
                int ret = rv.GetInt32(i, &val);
                if (ret == 0) {
                    col = std::to_string(val);
                }
            } else if (type == rtidb::type::kTimestamp) {
                int64_t val;
                int ret = rv.GetTimestamp(i, &val);
                if (ret == 0) {
                    col = std::to_string(val);
                }
            } else if (type == rtidb::type::kBigInt) {
                int64_t val;
                int ret = rv.GetInt64(i, &val);
                if (ret == 0) {
                    col = std::to_string(val);
                }
            } else if (type == rtidb::type::kBool) {
                bool val;
                int ret = rv.GetBool(i, &val);
                if (ret == 0) {
                    col = std::to_string(val);
                }
            } else if (type == rtidb::type::kFloat) {
                float val;
                int ret = rv.GetFloat(i, &val);
                if (ret == 0) {
                    col = std::to_string(val);
                }
            } else if (type == rtidb::type::kSmallInt) {
                int16_t val;
                int ret = rv.GetInt16(i, &val);
                if (ret == 0) {
                    col = std::to_string(val);
                }
            } else if (type == rtidb::type::kDouble) {
                double val;
                int ret = rv.GetDouble(i, &val);
                if (ret == 0) {
                    col = std::to_string(val);
                }
            } else if (type == rtidb::type::kVarchar ||
                       type == rtidb::type::kString) {
                char* ch = NULL;
                uint32_t length = 0;
                int ret = rv.GetString(i, &ch, &length);
                if (ret == 0) {
                    col.assign(ch, length);
                }
            }
            value_vec.push_back(col);
        }
    }
    static rtidb::base::ResultMsg GetCdColumns(const Schema& schema,
            const std::map<std::string, std::string>& cd_columns_map,
            ::google::protobuf::RepeatedPtrField<::rtidb::api::Columns>*
            cd_columns) {
        rtidb::base::ResultMsg rm;
        std::map<std::string, ::rtidb::type::DataType> name_type_map;
        for (const auto& col_desc : schema) {
            name_type_map.insert(std::make_pair(
                        col_desc.name(), col_desc.data_type()));
        }
        for (const auto& kv : cd_columns_map) {
            auto iter = name_type_map.find(kv.first);
            if (iter == name_type_map.end()) {
                rm.code = -1;
                rm.msg = "query failed! col_name " + kv.first + " not exist";
                return rm;
            }
            ::rtidb::api::Columns* index = cd_columns->Add();
            index->add_name(kv.first);
            ::rtidb::type::DataType type = iter->second;
            std::string val = "";
            if (!::rtidb::codec::Convert(kv.second, type, &val)) {
                rm.code = -1;
                rm.msg = "convert str " + kv.second + "  failed!";
                return rm;
            }
            index->set_value(val);
        }
        rm.code = 0;
        rm.msg = "ok";
        return rm;
    }
};
}  // namespace codec
}  // namespace rtidb

#endif  // SRC_CODEC_SCHEMA_CODEC_H_
