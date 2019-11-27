/*
 * ir_types.h
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

#ifndef SRC_CODEGEN_IR_TYPES_H_
#define SRC_CODEGEN_IR_TYPES_H_

namespace fesql {
namespace codegen {

struct IRString {
    int32_t size;
    char* data;
};

inline const bool ConvertFeSQLType2LLVMType (const node::DataType &data_type, ::llvm::LLVMContext &ctx, ::llvm::Type **llvm_type){
    switch (data_type) {
        case node::kTypeVoid:
            *llvm_type = (::llvm::Type::getVoidTy(ctx));
            break;
        case node::kTypeInt16:
            *llvm_type = (::llvm::Type::getInt16Ty(ctx));
            break;
        case node::kTypeInt32:
            *llvm_type = (::llvm::Type::getInt32Ty(ctx));
            break;
        case node::kTypeInt64:
            *llvm_type = (::llvm::Type::getInt64Ty(ctx));
            break;
        case node::kTypeFloat:
            *llvm_type = (::llvm::Type::getFloatTy(ctx));
            break;
        case node::kTypeDouble:
            *llvm_type = (::llvm::Type::getDoubleTy(ctx));
            break;
        case node::kTypeInt8Ptr:
            *llvm_type = (::llvm::Type::getInt8PtrTy(ctx));
            break;
        default: {
            return false;
        }
    }
    return true;
};

}  // namespace codegen
}  // namespace fesql
#endif  // SRC_CODEGEN_IR_TYPES_H_
