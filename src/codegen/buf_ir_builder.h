/*
 * buf_ir_builder.h
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

#ifndef SRC_CODEGEN_BUF_IR_BUILDER_H_
#define SRC_CODEGEN_BUF_IR_BUILDER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "codec/fe_row_codec.h"
#include "codegen/row_ir_builder.h"
#include "codegen/scope_var.h"
#include "codegen/variable_ir_builder.h"
#include "llvm/IR/IRBuilder.h"
#include "node/node_enum.h"
#include "proto/fe_type.pb.h"
#include "vm/catalog.h"

namespace fesql {
namespace codegen {

class BufNativeEncoderIRBuilder : public RowEncodeIRBuilder {
 public:
    BufNativeEncoderIRBuilder(const std::map<uint32_t, ::llvm::Value*>* outputs,
                              const vm::Schema& schema,
                              ::llvm::BasicBlock* block);

    ~BufNativeEncoderIRBuilder();

    // the output_ptr like int8_t**
    bool BuildEncode(::llvm::Value* output_ptr);

    bool BuildEncodePrimaryField(
      ::llvm::Value* buf, size_t idx, ::llvm::Value* val);

 private:
    bool CalcTotalSize(::llvm::Value** output, ::llvm::Value* str_addr_space);
    bool CalcStrBodyStart(::llvm::Value** output, ::llvm::Value* str_add_space);
    bool AppendPrimary(::llvm::Value* i8_ptr, ::llvm::Value* val,
                       uint32_t field_offset);

    bool AppendString(::llvm::Value* i8_ptr, ::llvm::Value* buf_size,
                      ::llvm::Value* str_val, ::llvm::Value* str_addr_space,
                      ::llvm::Value* str_body_offset, uint32_t str_field_idx,
                      ::llvm::Value** output);

    bool AppendHeader(::llvm::Value* i8_ptr, ::llvm::Value* size,
                      ::llvm::Value* bitmap_size);

 private:
    const std::map<uint32_t, ::llvm::Value*>* outputs_;
    vm::Schema schema_;
    uint32_t str_field_start_offset_;
    std::vector<uint32_t> offset_vec_;
    uint32_t str_field_cnt_;
    ::llvm::BasicBlock* block_;
};

class BufNativeIRBuilder : public RowDecodeIRBuilder {
 public:
    BufNativeIRBuilder(const vm::Schema& schema, ::llvm::BasicBlock* block,
                       ScopeVar* scope_var);
    ~BufNativeIRBuilder();

    bool BuildGetField(const std::string& name, ::llvm::Value* row_ptr,
                       ::llvm::Value* row_size, ::llvm::Value** output);

 private:
    bool BuildGetPrimaryField(const std::string& fn_name,
                              ::llvm::Value* row_ptr, uint32_t offset,
                              ::llvm::Type* type, ::llvm::Value** output);
    bool BuildGetStringField(uint32_t offset, uint32_t next_str_field_offset,
                             uint32_t str_start_offset, ::llvm::Value* row_ptr,
                             ::llvm::Value* size, ::llvm::Value** output);
    bool GetFiledOffsetType(const std::string& name, uint32_t* offset_ptr,
                            node::DataType* data_type_ptr);

 private:
    ::llvm::BasicBlock* block_;
    ScopeVar* sv_;
    codec::RowDecoder decoder_;
    VariableIRBuilder variable_ir_builder_;
};

}  // namespace codegen
}  // namespace fesql
#endif  // SRC_CODEGEN_BUF_IR_BUILDER_H_
