/*
 * result_set_impl.h
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

#ifndef SRC_SDK_RESULT_SET_IMPL_H_
#define SRC_SDK_RESULT_SET_IMPL_H_

#include <memory>
#include "codec/row_codec.h"
#include "proto/tablet.pb.h"
#include "sdk/base_impl.h"
#include "sdk/result_set.h"
#include "brpc/controller.h"
#include "butil/iobuf.h"

namespace fesql {
namespace sdk {

class ResultSetImpl : public ResultSet {
 public:
    ResultSetImpl(std::unique_ptr<tablet::QueryResponse> response,
                  std::unique_ptr<brpc::Controller> cntl);

    ~ResultSetImpl();

    bool Init();

    bool Next();

    bool IsNULL(uint32_t index) {}

    bool GetString(uint32_t index,
                   char** result, 
                   uint32_t* size);

    bool GetBool(uint32_t index, bool* result);

    bool GetChar(uint32_t index, char* result);

    bool GetInt16(uint32_t index, int16_t* result);

    bool GetInt32(uint32_t index, int32_t* result);

    bool GetInt64(uint32_t index, int64_t* result);

    bool GetFloat(uint32_t index, float* result);

    bool GetDouble(uint32_t index, double* result);

    bool GetDate(uint32_t index, uint32_t* days);

    int32_t GetDateUnsafe(uint32_t index) {}

    bool GetTime(uint32_t index, int64_t* mills);

    inline const Schema& GetSchema() {
        return schema_;
    }

    inline int32_t Size() {
        return  response_->count();
    }

 private:
    inline uint32_t GetRecordSize() {
        return response_->count();
    }
 private:
    std::unique_ptr<tablet::QueryResponse> response_;
    int32_t index_;
    int32_t byte_size_;
    uint32_t position_;
    std::unique_ptr<codec::RowView> row_view_;
    vm::Schema internal_schema_;
    SchemaImpl schema_;
    std::unique_ptr<brpc::Controller> cntl_;
    butil::IOBuf row_buf_;
};

}  // namespace sdk
}  // namespace fesql
#endif  // SRC_SDK_RESULT_SET_IMPL_H_
