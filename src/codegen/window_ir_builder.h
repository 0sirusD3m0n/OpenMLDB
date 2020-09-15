/*
 * window_ir_builder.h
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

#ifndef SRC_CODEGEN_WINDOW_IR_BUILDER_H_
#define SRC_CODEGEN_WINDOW_IR_BUILDER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "codec/fe_row_codec.h"
#include "codegen/ir_base_builder.h"
#include "llvm/IR/IRBuilder.h"
#include "proto/fe_type.pb.h"
#include "vm/catalog.h"
#include "vm/schemas_context.h"

namespace fesql {
namespace codegen {

class WindowDecodeIRBuilder {
 public:
    WindowDecodeIRBuilder() {}

    virtual ~WindowDecodeIRBuilder() {}

    virtual bool BuildInnerRangeList(::llvm::Value* window_ptr, int64_t start,
                                     int64_t end, ::llvm::Value** output) = 0;
    virtual bool BuildInnerRowsList(::llvm::Value* window_ptr, int64_t start,
                                    int64_t end, ::llvm::Value** output) = 0;
    virtual bool BuildGetCol(const std::string& name, ::llvm::Value* window_ptr,
                             ::llvm::Value** output) = 0;
    virtual bool BuildGetCol(const std::string& name, ::llvm::Value* window_ptr,
                             uint32_t row_idx, ::llvm::Value** output) = 0;
    virtual bool ResolveFieldInfo(const std::string& name, uint32_t row_idx,
                                  codec::ColInfo* info,
                                  node::TypeNode* type_ptr) = 0;
};

class MemoryWindowDecodeIRBuilder : public WindowDecodeIRBuilder {
 public:
    MemoryWindowDecodeIRBuilder(vm::SchemasContext* schemas_context,
                                ::llvm::BasicBlock* block);

    ~MemoryWindowDecodeIRBuilder();
    virtual bool BuildInnerRangeList(::llvm::Value* window_ptr, int64_t start,
                                     int64_t end, ::llvm::Value** output);
    virtual bool BuildInnerRowsList(::llvm::Value* window_ptr, int64_t start,
                                    int64_t end, ::llvm::Value** output);
    virtual bool BuildGetCol(const std::string& name, ::llvm::Value* window_ptr,
                             ::llvm::Value** output);
    virtual bool BuildGetCol(const std::string& name, ::llvm::Value* window_ptr,
                             uint32_t row_idx, ::llvm::Value** output);

    bool ResolveFieldInfo(const std::string& name, uint32_t row_idx,
                          codec::ColInfo* info, node::TypeNode* type_ptr);

 private:
    bool BuildGetPrimaryCol(const std::string& fn_name, ::llvm::Value* row_ptr,
                            uint32_t row_idx, uint32_t col_idx, uint32_t offset,
                            fesql::node::TypeNode* type,
                            ::llvm::Value** output);

    bool BuildGetStringCol(uint32_t row_idx, uint32_t col_idx, uint32_t offset,
                           uint32_t next_str_field_offset,
                           uint32_t str_start_offset,
                           fesql::node::TypeNode* type,
                           ::llvm::Value* window_ptr, ::llvm::Value** output);

 private:
    ::llvm::BasicBlock* block_;
    vm::SchemasContext* schemas_context_;
};

}  // namespace codegen
}  // namespace fesql
#endif  // SRC_CODEGEN_WINDOW_IR_BUILDER_H_
