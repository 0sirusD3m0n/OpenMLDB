/*
 * type_codec.cc
 * Copyright (C) 4paradigm.com 2019 wangtaize <wangtaize@4paradigm.com>
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

#include "codec/type_codec.h"
#include <string>
#include <utility>
#include "codec/list_iterator_codec.h"
#include "glog/logging.h"
#include "proto/fe_type.pb.h"

namespace fesql {
namespace codec {
namespace v1 {

using fesql::codec::ListV;
using fesql::codec::Row;

int32_t GetStrField(const butil::IOBuf& row, uint32_t field_offset,
                    uint32_t next_str_field_offset, uint32_t str_start_offset,
                    uint32_t addr_space, butil::IOBuf* output) {
    if (output == NULL) return -1;
    uint32_t str_offset = 0;
    uint32_t next_str_offset = 0;
    switch (addr_space) {
        case 1: {
            int32_t i8_str_pos = str_start_offset + field_offset;
            uint8_t i8_tmp_offset = 0;
            row.copy_to(reinterpret_cast<void*>(&i8_tmp_offset), 1, i8_str_pos);
            str_offset = i8_tmp_offset;
            if (next_str_field_offset > 0) {
                i8_str_pos = str_start_offset + next_str_field_offset;
                row.copy_to(reinterpret_cast<void*>(&i8_tmp_offset), 1,
                            i8_str_pos);
                next_str_offset = i8_tmp_offset;
            }
            break;
        }
        case 2: {
            int32_t i16_str_pos = str_start_offset + field_offset * 2;
            uint16_t i16_tmp_offset = 0;
            row.copy_to(reinterpret_cast<void*>(&i16_tmp_offset), 2,
                        i16_str_pos);
            str_offset = i16_tmp_offset;
            if (next_str_field_offset > 0) {
                i16_str_pos = str_start_offset + next_str_field_offset * 2;
                row.copy_to(reinterpret_cast<void*>(&i16_tmp_offset), 2,
                            i16_str_pos);
                next_str_offset = i16_tmp_offset;
            }
            break;
        }
        case 3: {
            uint8_t i8_tmp_offset = 0;
            int32_t i8_str_pos = str_start_offset + field_offset * 3;
            row.copy_to(reinterpret_cast<void*>(&i8_tmp_offset), 1, i8_str_pos);
            str_offset = i8_tmp_offset;
            row.copy_to(reinterpret_cast<void*>(&i8_tmp_offset), 1,
                        i8_str_pos + 1);
            str_offset = (str_offset << 8) + i8_tmp_offset;
            row.copy_to(reinterpret_cast<void*>(&i8_tmp_offset), 1,
                        i8_str_pos + 2);
            str_offset = (str_offset << 8) + i8_tmp_offset;
            if (next_str_field_offset > 0) {
                i8_str_pos = str_start_offset + next_str_field_offset * 3;
                row.copy_to(reinterpret_cast<void*>(&i8_tmp_offset), 1,
                            i8_str_pos);
                next_str_offset = i8_tmp_offset;
                row.copy_to(reinterpret_cast<void*>(&i8_tmp_offset), 1,
                            i8_str_pos + 1);
                next_str_offset = (next_str_offset << 8) + i8_tmp_offset;
                row.copy_to(reinterpret_cast<void*>(&i8_tmp_offset), 1,
                            i8_str_pos + 2);
                next_str_offset = (next_str_offset << 8) + i8_tmp_offset;
            }
            break;
        }
        case 4: {
            int32_t i32_str_pos = str_start_offset + field_offset * 4;
            row.copy_to(reinterpret_cast<void*>(&str_offset), 4, i32_str_pos);
            if (next_str_field_offset > 0) {
                i32_str_pos = str_start_offset + next_str_field_offset * 4;
                row.copy_to(reinterpret_cast<void*>(&next_str_offset), 4,
                            i32_str_pos);
            }
            break;
        }
        default: {
            return -2;
        }
    }
    if (next_str_field_offset <= 0) {
        uint32_t tmp_size = 0;
        row.copy_to(reinterpret_cast<void*>(&tmp_size), SIZE_LENGTH,
                    VERSION_LENGTH);
        uint32_t size = tmp_size - str_offset;
        row.append_to(output, size, str_offset);

    } else {
        uint32_t size = next_str_offset - str_offset;
        row.append_to(output, size, str_offset);
    }
    return 0;
}

int32_t GetStrField(const int8_t* row, uint32_t field_offset,
                    uint32_t next_str_field_offset, uint32_t str_start_offset,
                    uint32_t addr_space, int8_t** data, uint32_t* size) {
    if (row == NULL || data == NULL || size == NULL) return -1;
    const int8_t* row_with_offset = row + str_start_offset;
    uint32_t str_offset = 0;
    uint32_t next_str_offset = 0;
    switch (addr_space) {
        case 1: {
            str_offset = (uint8_t)(*(row_with_offset + field_offset));
            if (next_str_field_offset > 0) {
                next_str_offset =
                    (uint8_t)(*(row_with_offset + next_str_field_offset));
            }
            break;
        }
        case 2: {
            str_offset = *(reinterpret_cast<const uint16_t*>(
                row_with_offset + field_offset * addr_space));
            if (next_str_field_offset > 0) {
                next_str_offset = *(reinterpret_cast<const uint16_t*>(
                    row_with_offset + next_str_field_offset * addr_space));
            }
            break;
        }
        case 3: {
            const int8_t* cur_row_with_offset =
                row_with_offset + field_offset * addr_space;
            str_offset = (uint8_t)(*cur_row_with_offset);
            str_offset =
                (str_offset << 8) + (uint8_t)(*(cur_row_with_offset + 1));
            str_offset =
                (str_offset << 8) + (uint8_t)(*(cur_row_with_offset + 2));
            if (next_str_field_offset > 0) {
                const int8_t* next_row_with_offset =
                    row_with_offset + next_str_field_offset * addr_space;
                next_str_offset = (uint8_t)(*(next_row_with_offset));
                next_str_offset = (next_str_offset << 8) +
                                  (uint8_t)(*(next_row_with_offset + 1));
                next_str_offset = (next_str_offset << 8) +
                                  (uint8_t)(*(next_row_with_offset + 2));
            }
            break;
        }
        case 4: {
            str_offset = *(reinterpret_cast<const uint32_t*>(
                row_with_offset + field_offset * addr_space));
            if (next_str_field_offset > 0) {
                next_str_offset = *(reinterpret_cast<const uint32_t*>(
                    row_with_offset + next_str_field_offset * addr_space));
            }
            break;
        }
        default: {
            return -2;
        }
    }
    const int8_t* ptr = row + str_offset;
    *data = (int8_t*)(ptr);  // NOLINT
    if (next_str_field_offset <= 0) {
        uint32_t total_length =
            *(reinterpret_cast<const uint32_t*>(row + VERSION_LENGTH));
        *size = total_length - str_offset;
    } else {
        *size = next_str_offset - str_offset;
    }
    return 0;
}

int32_t AppendString(int8_t* buf_ptr, uint32_t buf_size, int8_t* val,
                     uint32_t size, uint32_t str_start_offset,
                     uint32_t str_field_offset, uint32_t str_addr_space,
                     uint32_t str_body_offset) {
    uint32_t str_offset = str_start_offset + str_field_offset * str_addr_space;
    if (str_offset + size > buf_size) {
        LOG(WARNING) << "invalid str size expect " << buf_size << " but "
                     << str_offset + size;
        return -1;
    }
    int8_t* ptr_offset = buf_ptr + str_offset;
    switch (str_addr_space) {
        case 1: {
            *(reinterpret_cast<uint8_t*>(ptr_offset)) =
                (uint8_t)str_body_offset;
            break;
        }

        case 2: {
            *(reinterpret_cast<uint16_t*>(ptr_offset)) =
                (uint16_t)str_body_offset;
            break;
        }

        case 3: {
            *(reinterpret_cast<uint8_t*>(ptr_offset)) = str_body_offset >> 16;
            *(reinterpret_cast<uint8_t*>(ptr_offset + 1)) =
                (str_body_offset & 0xFF00) >> 8;
            *(reinterpret_cast<uint8_t*>(ptr_offset + 2)) =
                str_body_offset & 0x00FF;
            break;
        }

        default: {
            *(reinterpret_cast<uint32_t*>(ptr_offset)) = str_body_offset;
        }
    }

    if (size != 0) {
        memcpy(reinterpret_cast<char*>(buf_ptr + str_body_offset), val, size);
    }

    return str_body_offset + size;
}

int32_t GetStrCol(int8_t* input, int32_t row_idx, int32_t str_field_offset,
                  int32_t next_str_field_offset, int32_t str_start_offset,
                  int32_t type_id, int8_t* data) {
    if (nullptr == input || nullptr == data) {
        return -2;
    }

    ListV<Row>* w = reinterpret_cast<ListV<Row>*>(input);
    fesql::type::Type type = static_cast<fesql::type::Type>(type_id);
    switch (type) {
        case fesql::type::kVarchar: {
            new (data)
                StringColumnImpl(w, row_idx, str_field_offset,
                                 next_str_field_offset, str_start_offset);
            break;
        }
        default: {
            return -2;
        }
    }
    return 0;
}

int32_t GetCol(int8_t* input, int32_t row_idx, int32_t offset, int32_t type_id,
               int8_t* data) {
    fesql::type::Type type = static_cast<fesql::type::Type>(type_id);
    if (nullptr == input || nullptr == data) {
        return -2;
    }
    ListV<Row>* w = reinterpret_cast<ListV<Row>*>(input);
    switch (type) {
        case fesql::type::kInt32: {
            new (data) ColumnImpl<int>(w, row_idx, offset);
            break;
        }
        case fesql::type::kInt16: {
            new (data) ColumnImpl<int16_t>(w, row_idx, offset);
            break;
        }
        case fesql::type::kInt64: {
            new (data) ColumnImpl<int64_t>(w, row_idx, offset);
            break;
        }
        case fesql::type::kFloat: {
            new (data) ColumnImpl<float>(w, row_idx, offset);
            break;
        }
        case fesql::type::kDouble: {
            new (data) ColumnImpl<double>(w, row_idx, offset);
            break;
        }
        case fesql::type::kTimestamp: {
            new (data) TimestampColumnImpl(w, row_idx, offset);
            break;
        }
        case fesql::type::kDate: {
            new (data) DateColumnImpl(w, row_idx, offset);
            break;
        }
        default: {
            LOG(WARNING) << "cannot get col for type "
                         << ::fesql::type::Type_Name(type) << " type id "
                         << type_id;
            return -2;
        }
    }
    return 0;
}

}  // namespace v1
}  // namespace codec
}  // namespace fesql
