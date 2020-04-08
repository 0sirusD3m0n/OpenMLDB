/*
 * window_ir_builder.cc
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

#include "codegen/window_ir_builder.h"
#include <string>
#include <utility>
#include <vector>
#include "codec/row_codec.h"
#include "codegen/ir_base_builder.h"
#include "glog/logging.h"

namespace fesql {
namespace codegen {

MemoryWindowDecodeIRBuilder::MemoryWindowDecodeIRBuilder(
    const vm::Schema& schema, ::llvm::BasicBlock* block)
    : schema_(schema),
      block_(block),
      types_(),
      next_str_pos_(),
      str_field_start_offset_(0) {
    uint32_t offset = codec::GetStartOffset(schema_.size());
    uint32_t string_field_cnt = 0;
    for (int32_t i = 0; i < schema_.size(); i++) {
        const ::fesql::type::ColumnDef& column = schema_.Get(i);
        fesql::node::DataType data_type;
        if (!SchemaType2DataType(column.type(), &data_type)) {
            LOG(WARNING) << "fail to convert schema type to data type " +
                                fesql::type::Type_Name(column.type());
            return;
        }
        if (data_type == ::fesql::node::kVarchar) {
            types_.insert(std::make_pair(
                column.name(), std::make_pair(data_type, string_field_cnt)));
            next_str_pos_.insert(
                std::make_pair(string_field_cnt, string_field_cnt));
            string_field_cnt += 1;
        } else {
            auto it = codec::TYPE_SIZE_MAP.find(column.type());
            if (it == codec::TYPE_SIZE_MAP.end()) {
                LOG(WARNING) << "fail to find column type "
                             << ::fesql::type::Type_Name(column.type());
            } else {
                types_.insert(std::make_pair(
                    column.name(), std::make_pair(data_type, offset)));
                offset += it->second;
            }
        }
    }
    str_field_start_offset_ = offset;
}

MemoryWindowDecodeIRBuilder::~MemoryWindowDecodeIRBuilder() {}

bool MemoryWindowDecodeIRBuilder::BuildGetCol(const std::string& name,
                                              ::llvm::Value* window_ptr,
                                              ::llvm::Value** output) {
    if (window_ptr == NULL || output == NULL) {
        LOG(WARNING) << "input args have null";
        return false;
    }

    auto it = types_.find(name);
    if (it == types_.end()) {
        LOG(WARNING) << "no column " << name << " in schema";
        return false;
    }

    ::fesql::node::DataType& fe_type = it->second.first;
    ::llvm::IRBuilder<> builder(block_);
    uint32_t offset = it->second.second;
    switch (fe_type) {
        case ::fesql::node::kInt16:
        case ::fesql::node::kInt32:
        case ::fesql::node::kInt64:
        case ::fesql::node::kFloat:
        case ::fesql::node::kDouble: {
            return BuildGetPrimaryCol("fesql_storage_get_col", window_ptr,
                                      offset, fe_type, output);
        }
        case ::fesql::node::kVarchar: {
            uint32_t next_offset = 0;
            auto nit = next_str_pos_.find(offset);
            if (nit != next_str_pos_.end()) {
                next_offset = nit->second;
            }
            DLOG(INFO) << "get string with offset " << offset << " next offset "
                       << next_offset << " for col " << name;
            return BuildGetStringCol(offset, next_offset, fe_type, window_ptr,
                                     output);
        }
        default: {
            return false;
        }
    }
}

bool MemoryWindowDecodeIRBuilder::BuildGetPrimaryCol(
    const std::string& fn_name, ::llvm::Value* row_ptr, uint32_t offset,
    const fesql::node::DataType& type, ::llvm::Value** output) {
    if (row_ptr == NULL || output == NULL) {
        LOG(WARNING) << "input args have null ptr";
        return false;
    }

    ::llvm::IRBuilder<> builder(block_);
    ::llvm::Type* i8_ty = builder.getInt8Ty();
    ::llvm::Type* i8_ptr_ty = builder.getInt8PtrTy();
    ::llvm::Type* i32_ty = builder.getInt32Ty();

    ::llvm::Type* list_ref_type = NULL;
    bool ok = GetLLVMListType(block_->getModule(), type, &list_ref_type);
    if (!ok) {
        LOG(WARNING) << "fail to get list type";
        return false;
    }
    uint32_t col_iterator_size = 0;
    ok = GetLLVMColumnSize(type, &col_iterator_size);
    if (!ok || col_iterator_size == 0) {
        LOG(WARNING) << "fail to get col iterator size";
    }
    // alloca memory on stack for col iterator
    ::llvm::ArrayType* array_type =
        ::llvm::ArrayType::get(i8_ty, col_iterator_size);
    ::llvm::Value* col_iter = builder.CreateAlloca(array_type);
    // alloca memory on stack
    ::llvm::Value* list_ref = builder.CreateAlloca(list_ref_type);
    ::llvm::Value* data_ptr_ptr =
        builder.CreateStructGEP(list_ref_type, list_ref, 0);
    data_ptr_ptr = builder.CreatePointerCast(
        data_ptr_ptr, col_iter->getType()->getPointerTo());
    builder.CreateStore(col_iter, data_ptr_ptr, false);
    col_iter = builder.CreatePointerCast(col_iter, i8_ptr_ty);

    ::llvm::Value* val_offset = builder.getInt32(offset);
    ::fesql::type::Type schema_type;
    if (!DataType2SchemaType(type, &schema_type)) {
        LOG(WARNING) << "fail to convert data type to schema type: "
                     << node::DataTypeName(type);
        return false;
    }

    ::llvm::Value* val_type_id =
        builder.getInt32(static_cast<int32_t>(schema_type));
    ::llvm::FunctionCallee callee = block_->getModule()->getOrInsertFunction(
        fn_name, i32_ty, i8_ptr_ty, i32_ty, i32_ty, i8_ptr_ty);
    builder.CreateCall(callee, ::llvm::ArrayRef<::llvm::Value*>{
                                   row_ptr, val_offset, val_type_id, col_iter});
    *output = list_ref;
    return true;
}

bool MemoryWindowDecodeIRBuilder::BuildGetStringCol(
    uint32_t offset, uint32_t next_str_field_offset,
    const fesql::node::DataType& type, ::llvm::Value* window_ptr,
    ::llvm::Value** output) {
    if (window_ptr == NULL || output == NULL) {
        LOG(WARNING) << "input args have null ptr";
        return false;
    }

    ::llvm::IRBuilder<> builder(block_);
    ::llvm::Type* i8_ty = builder.getInt8Ty();
    ::llvm::Type* i8_ptr_ty = builder.getInt8PtrTy();
    ::llvm::Type* i32_ty = builder.getInt32Ty();

    ::llvm::Type* list_ref_type = NULL;
    bool ok = GetLLVMListType(block_->getModule(), type, &list_ref_type);
    if (!ok) {
        LOG(WARNING) << "fail to get list type";
        return false;
    }
    uint32_t col_iterator_size;
    ok = GetLLVMColumnSize(type, &col_iterator_size);
    if (!ok) {
        LOG(WARNING) << "fail to get col iterator size";
    }
    // alloca memory on stack for col iterator
    ::llvm::ArrayType* array_type =
        ::llvm::ArrayType::get(i8_ty, col_iterator_size);
    ::llvm::Value* col_iter = builder.CreateAlloca(array_type);

    // alloca memory on stack
    ::llvm::Value* list_ref = builder.CreateAlloca(list_ref_type);
    ::llvm::Value* data_ptr_ptr =
        builder.CreateStructGEP(list_ref_type, list_ref, 0);
    data_ptr_ptr = builder.CreatePointerCast(
        data_ptr_ptr, col_iter->getType()->getPointerTo());
    builder.CreateStore(col_iter, data_ptr_ptr, false);
    col_iter = builder.CreatePointerCast(col_iter, i8_ptr_ty);

    // get str field declear
    ::llvm::FunctionCallee callee = block_->getModule()->getOrInsertFunction(
        "fesql_storage_get_str_col", i32_ty, i8_ptr_ty, i32_ty, i32_ty, i32_ty,
        i32_ty, i8_ptr_ty);

    ::llvm::Value* str_offset = builder.getInt32(offset);
    ::llvm::Value* next_str_offset = builder.getInt32(next_str_field_offset);
    ::fesql::type::Type schema_type;
    if (!DataType2SchemaType(type, &schema_type)) {
        LOG(WARNING) << "fail to convert data type to schema type: "
                     << node::DataTypeName(type);
        return false;
    }
    ::llvm::Value* val_type_id =
        builder.getInt32(static_cast<int32_t>(schema_type));
    builder.CreateCall(callee, ::llvm::ArrayRef<::llvm::Value*>{
                                   window_ptr, str_offset, next_str_offset,
                                   builder.getInt32(str_field_start_offset_),
                                   val_type_id, col_iter});
    *output = list_ref;
    return true;
}
uint32_t MemoryWindowDecodeIRBuilder::GetColOffset(const std::string& name) {
    auto it = types_.find(name);
    if (it == types_.end()) {
        LOG(WARNING) << "no column " << name << " in schema";
        return false;
    }
    uint32_t offset = it->second.second;
    return offset;
}

}  // namespace codegen
}  // namespace fesql
