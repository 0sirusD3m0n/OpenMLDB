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
#include "codec/fe_row_codec.h"
#include "codegen/ir_base_builder.h"
#include "glog/logging.h"

namespace fesql {
namespace codegen {
MemoryWindowDecodeIRBuilder::MemoryWindowDecodeIRBuilder(
    const vm::Schema& schema, ::llvm::BasicBlock* block)
    : block_(block), decoder_list_({codec::RowDecoder(schema)}) {}

MemoryWindowDecodeIRBuilder::MemoryWindowDecodeIRBuilder(
    const std::vector<vm::RowSchemaInfo>& schema_list,
    ::llvm::BasicBlock* block)
    : block_(block), decoder_list_() {
    for (auto info : schema_list) {
        decoder_list_.push_back(codec::RowDecoder(*info.schema_));
    }
}

MemoryWindowDecodeIRBuilder::~MemoryWindowDecodeIRBuilder() {}
bool MemoryWindowDecodeIRBuilder::BuildInnerRowsList(::llvm::Value* list_ptr,
                                                     int64_t start_offset,
                                                     int64_t end_offset,
                                                     ::llvm::Value** output) {
    if (list_ptr == NULL || output == NULL) {
        LOG(WARNING) << "input args have null";
        return false;
    }
    if (list_ptr == NULL || output == NULL) {
        LOG(WARNING) << "input args have null ptr";
        return false;
    }

    ::llvm::IRBuilder<> builder(block_);
    ::llvm::Type* i8_ty = builder.getInt8Ty();
    ::llvm::Type* i8_ptr_ty = builder.getInt8PtrTy();
    ::llvm::Type* i32_ty = builder.getInt32Ty();
    ::llvm::Type* i64_ty = builder.getInt64Ty();
    uint32_t inner_list_size =
        sizeof(::fesql::codec::InnerRowsList<fesql::codec::Row>);
    // alloca memory on stack for col iterator
    ::llvm::ArrayType* array_type =
        ::llvm::ArrayType::get(i8_ty, inner_list_size);
    ::llvm::Value* inner_list_ptr = builder.CreateAlloca(array_type);
    inner_list_ptr = builder.CreatePointerCast(inner_list_ptr, i8_ptr_ty);

    ::llvm::Value* val_start_offset = builder.getInt64(start_offset);
    ::llvm::Value* val_end_offset = builder.getInt64(end_offset);
    ::llvm::FunctionCallee callee = block_->getModule()->getOrInsertFunction(
        "fesql_storage_get_inner_rows_list", i32_ty, i8_ptr_ty, i64_ty, i64_ty,
        i8_ptr_ty);
    builder.CreateCall(callee, ::llvm::ArrayRef<::llvm::Value*>{
                                   list_ptr, val_start_offset, val_end_offset,
                                   inner_list_ptr});
    *output = inner_list_ptr;
    return true;
}
bool MemoryWindowDecodeIRBuilder::BuildInnerRangeList(::llvm::Value* list_ptr,
                                                      int64_t start_offset,
                                                      int64_t end_offset,
                                                      ::llvm::Value** output) {
    if (list_ptr == NULL || output == NULL) {
        LOG(WARNING) << "input args have null";
        return false;
    }
    if (list_ptr == NULL || output == NULL) {
        LOG(WARNING) << "input args have null ptr";
        return false;
    }

    ::llvm::IRBuilder<> builder(block_);
    ::llvm::Type* i8_ty = builder.getInt8Ty();
    ::llvm::Type* i8_ptr_ty = builder.getInt8PtrTy();
    ::llvm::Type* i32_ty = builder.getInt32Ty();
    ::llvm::Type* i64_ty = builder.getInt64Ty();
    uint32_t inner_list_size =
        sizeof(::fesql::codec::InnerRangeList<fesql::codec::Row>);
    // alloca memory on stack for col iterator
    ::llvm::ArrayType* array_type =
        ::llvm::ArrayType::get(i8_ty, inner_list_size);
    ::llvm::Value* inner_list_ptr = builder.CreateAlloca(array_type);
    inner_list_ptr = builder.CreatePointerCast(inner_list_ptr, i8_ptr_ty);

    ::llvm::Value* val_start_offset = builder.getInt64(start_offset);
    ::llvm::Value* val_end_offset = builder.getInt64(end_offset);
    ::llvm::FunctionCallee callee = block_->getModule()->getOrInsertFunction(
        "fesql_storage_get_inner_range_list", i32_ty, i8_ptr_ty, i64_ty, i64_ty,
        i8_ptr_ty);
    builder.CreateCall(callee, ::llvm::ArrayRef<::llvm::Value*>{
                                   list_ptr, val_start_offset, val_end_offset,
                                   inner_list_ptr});
    *output = inner_list_ptr;
    return true;
}
bool MemoryWindowDecodeIRBuilder::BuildGetCol(const std::string& name,
                                              ::llvm::Value* window_ptr,
                                              ::llvm::Value** output) {
    return BuildGetCol(name, window_ptr, 0, output);
}
bool MemoryWindowDecodeIRBuilder::BuildGetCol(const std::string& name,
                                              ::llvm::Value* window_ptr,
                                              uint32_t row_idx,
                                              ::llvm::Value** output) {
    if (window_ptr == NULL || output == NULL) {
        LOG(WARNING) << "input args have null";
        return false;
    }
    ::fesql::node::TypeNode data_type;
    codec::ColInfo col_info;
    if (!ResolveFieldInfo(name, row_idx, &col_info, &data_type)) {
        LOG(WARNING) << "fail to get filed offset " << name;
        return false;
    }
    ::llvm::IRBuilder<> builder(block_);
    switch (data_type.base_) {
        case ::fesql::node::kBool:
        case ::fesql::node::kInt16:
        case ::fesql::node::kInt32:
        case ::fesql::node::kInt64:
        case ::fesql::node::kFloat:
        case ::fesql::node::kDouble:
        case ::fesql::node::kTimestamp:
        case ::fesql::node::kDate: {
            return BuildGetPrimaryCol("fesql_storage_get_col", window_ptr,
                                      row_idx, col_info.idx, col_info.offset,
                                      &data_type, output);
        }
        case ::fesql::node::kVarchar: {
            codec::StringColInfo str_col_info;
            if (!decoder_list_[row_idx].ResolveStringCol(name, &str_col_info)) {
                LOG(WARNING)
                    << "fail to get string filed offset and next offset"
                    << name;
            }
            DLOG(INFO) << "get string with offset " << str_col_info.offset
                       << " next offset " << str_col_info.str_next_offset
                       << " for col " << name;
            return BuildGetStringCol(
                row_idx, str_col_info.idx, str_col_info.offset,
                str_col_info.str_next_offset, str_col_info.str_start_offset,
                &data_type, window_ptr, output);
        }
        default: {
            LOG(WARNING) << "Fail get col, invalid data type "
                         << data_type.GetName();
            return false;
        }
    }
}  // namespace codegen

bool MemoryWindowDecodeIRBuilder::BuildGetPrimaryCol(
    const std::string& fn_name, ::llvm::Value* row_ptr, uint32_t row_idx,
    uint32_t col_idx, uint32_t offset, fesql::node::TypeNode* type,
    ::llvm::Value** output) {
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

    ::llvm::Value* val_row_idx = builder.getInt32(row_idx);
    ::llvm::Value* val_col_idx = builder.getInt32(col_idx);
    ::llvm::Value* val_offset = builder.getInt32(offset);
    ::fesql::type::Type schema_type;
    if (!DataType2SchemaType(*type, &schema_type)) {
        LOG(WARNING) << "fail to convert data type to schema type: "
                     << type->GetName();
        return false;
    }

    ::llvm::Value* val_type_id =
        builder.getInt32(static_cast<int32_t>(schema_type));
    ::llvm::FunctionCallee callee = block_->getModule()->getOrInsertFunction(
        fn_name, i32_ty, i8_ptr_ty, i32_ty, i32_ty, i32_ty, i32_ty, i8_ptr_ty);
    builder.CreateCall(callee, {row_ptr, val_row_idx, val_col_idx, val_offset,
                                val_type_id, col_iter});
    *output = list_ref;
    return true;
}

bool MemoryWindowDecodeIRBuilder::BuildGetStringCol(
    uint32_t row_idx, uint32_t col_idx, uint32_t offset,
    uint32_t next_str_field_offset, uint32_t str_start_offset,
    fesql::node::TypeNode* type, ::llvm::Value* window_ptr,
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
        i32_ty, i32_ty, i32_ty, i8_ptr_ty);

    ::llvm::Value* val_row_idx = builder.getInt32(row_idx);
    ::llvm::Value* val_col_idx = builder.getInt32(col_idx);
    ::llvm::Value* str_offset = builder.getInt32(offset);
    ::llvm::Value* next_str_offset = builder.getInt32(next_str_field_offset);
    ::fesql::type::Type schema_type;
    if (!DataType2SchemaType(*type, &schema_type)) {
        LOG(WARNING) << "fail to convert data type to schema type: "
                     << type->GetName();
        return false;
    }
    ::llvm::Value* val_type_id =
        builder.getInt32(static_cast<int32_t>(schema_type));
    builder.CreateCall(
        callee,
        {window_ptr, val_row_idx, val_col_idx, str_offset, next_str_offset,
         builder.getInt32(str_start_offset), val_type_id, col_iter});
    *output = list_ref;
    return true;
}

bool MemoryWindowDecodeIRBuilder::ResolveFieldInfo(
    const std::string& name, uint32_t row_idx, codec::ColInfo* info,
    node::TypeNode* data_type_ptr) {
    if (!decoder_list_[row_idx].ResolveColumn(name, info)) {
        return false;
    }
    if (!SchemaType2DataType(info->type, data_type_ptr)) {
        LOG(WARNING) << "unrecognized data type " +
                            fesql::type::Type_Name(info->type);
        return false;
    }
    return true;
}

}  // namespace codegen
}  // namespace fesql
