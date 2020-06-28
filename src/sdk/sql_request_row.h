/*
 * sql_request_row.h
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

#ifndef SRC_SDK_SQL_REQUEST_ROW_H_
#define SRC_SDK_SQL_REQUEST_ROW_H_

#include <memory>
#include <string>
#include <vector>

#include "sdk/base.h"

namespace rtidb {
namespace sdk {

class SQLRequestRow {
 public:
    SQLRequestRow() {}
    explicit SQLRequestRow(std::shared_ptr<fesql::sdk::Schema> schema);
    ~SQLRequestRow() {}
    bool Init(int32_t str_length);
    bool AppendBool(bool val);
    bool AppendInt32(int32_t val);
    bool AppendInt16(int16_t val);
    bool AppendInt64(int64_t val);
    bool AppendTimestamp(int64_t val);
    bool AppendFloat(float val);
    bool AppendDouble(double val);
    bool AppendString(const std::string& val);
    bool AppendNULL();
    bool Build();
    inline const std::string& GetRow() { return val_; }
    inline const std::shared_ptr<fesql::sdk::Schema> GetSchema() {
        return schema_;
    }

 private:
    bool Check(fesql::sdk::DataType type);
 private:
    std::shared_ptr<fesql::sdk::Schema> schema_;
    uint32_t cnt_;
    uint32_t size_;
    uint32_t str_field_cnt_;
    uint32_t str_addr_length_;
    uint32_t str_field_start_offset_;
    uint32_t str_offset_;
    std::vector<uint32_t> offset_vec_;
    std::string val_;
    int8_t* buf_;
    uint32_t str_length_expect_;
    uint32_t str_length_current_;
    bool has_error_;
};

}  // namespace sdk
}  // namespace rtidb
#endif  // SRC_SDK_SQL_REQUEST_ROW_H_
