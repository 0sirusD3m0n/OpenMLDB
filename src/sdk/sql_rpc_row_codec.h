/*
 * sql_rpc_row_codec.h
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

#ifndef SRC_SDK_SQL_RPC_ROW_CODEC_H_
#define SRC_SDK_SQL_RPC_ROW_CODEC_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/fe_slice.h"
#include "butil/iobuf.h"
#include "codec/fe_row_codec.h"
#include "codec/row.h"
#include "sdk/base.h"

namespace rtidb {
namespace sdk {

bool DecodeRpcRow(const butil::IOBuf& buf, size_t offset, size_t size, size_t slice_num, fesql::codec::Row* row) {
    if (row == nullptr || slice_num == 0 || size == 0) {
        return false;
    }
    size_t cur_offset = offset;
    if (cur_offset >= buf.size()) {
        LOG(WARNING) << "Offset " << cur_offset << " out of bound, buf size=" << buf.size();
        return false;
    }
    for (size_t i = 0; i < slice_num; ++i) {
        uint32_t slice_size;
        buf.copy_to(&slice_size, sizeof(uint32_t), cur_offset + 2);
        size_t next_offset;
        if (slice_size == 0) {
            next_offset = cur_offset + 2 + sizeof(uint32_t);
        } else {
            next_offset = cur_offset + slice_size;
        }
        if (next_offset > buf.size()) {
            LOG(WARNING) << "Size " << slice_size << " for " << i
                         << "th row slice out of bound, buf size=" << buf.size() << " cur offset=" << cur_offset;
            return false;
        }
        if (slice_size == 0) {
            if (i == 0) {
                *row = fesql::codec::Row();
            } else {
                row->Append(fesql::base::RefCountedSlice());
            }
        } else {
            int8_t* slice_buf = reinterpret_cast<int8_t*>(malloc(slice_size));
            buf.copy_to(slice_buf, slice_size, cur_offset);
            if (i == 0) {
                *row = fesql::codec::Row(fesql::base::RefCountedSlice::CreateManaged(slice_buf, slice_size));
            } else {
                row->Append(fesql::base::RefCountedSlice::CreateManaged(slice_buf, slice_size));
            }
        }
        cur_offset = next_offset;
    }
    if (offset + size != cur_offset) {
        LOG(WARNING) << "Illegal total row size " << (cur_offset - offset) << ", expect size=" << size;
        return false;
    }
    return true;
}

bool EncodeRpcRow(const fesql::codec::Row& row, butil::IOBuf* buf, size_t* total_size) {
    if (buf == nullptr) {
        return false;
    }
    *total_size = 0;
    size_t slice_num = row.GetRowPtrCnt();
    for (size_t i = 0; i < slice_num; ++i) {
        int8_t* slice_buf = row.buf(i);
        size_t slice_size = row.size(i);
        int code;
        if (slice_buf == nullptr || slice_size == 0) {
            char empty_header[6] = {1, 1, 0, 0, 0, 0};
            code = buf->append(empty_header, 6);
            *total_size += 6;
        } else {
            code = buf->append(slice_buf, slice_size);
            *total_size += slice_size;
        }
        if (code != 0) {
            LOG(WARNING) << "Append " << i << "th slice of size " << slice_size << " failed";
            return false;
        }
    }
    return true;
}

}  // namespace sdk
}  // namespace rtidb
#endif  // SRC_SDK_SQL_RPC_ROW_CODEC_H_
