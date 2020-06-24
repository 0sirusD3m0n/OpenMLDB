/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * struct_ir_builder.cc
 *
 * Author: chenjing
 * Date: 2020/5/26
 *--------------------------------------------------------------------------
 **/
#include "codegen/struct_ir_builder.h"
#include "codegen/date_ir_builder.h"
#include "codegen/ir_base_builder.h"
#include "codegen/string_ir_builder.h"
#include "codegen/timestamp_ir_builder.h"
namespace fesql {
namespace codegen {
StructTypeIRBuilder::StructTypeIRBuilder(::llvm::Module* m)
    : TypeIRBuilder(), m_(m), struct_type_(nullptr) {}
StructTypeIRBuilder::~StructTypeIRBuilder() {}
bool StructTypeIRBuilder::StructCastFrom(::llvm::BasicBlock* block,
                                         ::llvm::Value* src,
                                         ::llvm::Type* cast_type,
                                         ::llvm::Value** dist) {
    StructTypeIRBuilder* struct_builder =
        CreateStructTypeIRBuilder(block->getModule(), cast_type);
    bool ok = struct_builder->CastFrom(block, src, dist);
    delete struct_builder;
    return ok;
}
bool StructTypeIRBuilder::StructCopyFrom(::llvm::BasicBlock* block,
                                         ::llvm::Value* src,
                                         ::llvm::Value* dist) {
    StructTypeIRBuilder* struct_builder =
        CreateStructTypeIRBuilder(block->getModule(), src->getType());
    bool ok = struct_builder->CopyFrom(block, src, dist);
    delete struct_builder;
    return ok;
}
StructTypeIRBuilder* StructTypeIRBuilder::CreateStructTypeIRBuilder(
    ::llvm::Module* m, ::llvm::Type* type) {
    node::TypeNode type_node;
    if (!GetTypeNode(type, &type_node)) {
        return nullptr;
    }

    switch (type_node.base_) {
        case node::kTimestamp:
            return new TimestampIRBuilder(m);
        case node::kDate:
            return new DateIRBuilder(m);
        case node::kVarchar:
            return new StringIRBuilder(m);
        default: {
            LOG(WARNING) << "fail to create struct type ir builder for "
                         << type_node.GetName();
            return nullptr;
        }
    }
    return nullptr;
}
::llvm::Type* StructTypeIRBuilder::GetType() { return struct_type_; }
bool StructTypeIRBuilder::Create(::llvm::BasicBlock* block,
                                 ::llvm::Value** output) {
    if (block == NULL || output == NULL) {
        LOG(WARNING) << "the output ptr or block is NULL ";
        return false;
    }
    ::llvm::IRBuilder<> builder(block);
    ::llvm::Value* value = builder.CreateAlloca(struct_type_);
    *output = value;
    return true;
}
bool StructTypeIRBuilder::Get(::llvm::BasicBlock* block,
                              ::llvm::Value* struct_value, unsigned int idx,
                              ::llvm::Value** output) {
    if (block == NULL) {
        LOG(WARNING) << "the output ptr or block is NULL ";
        return false;
    }
    if (!IsStructPtr(struct_value->getType())) {
        LOG(WARNING) << "Fail get Struct value: struct pointer is required";
        return false;
    }
    if (struct_value->getType()->getPointerElementType() != struct_type_) {
        LOG(WARNING) << "Fail get Struct value: struct value type invalid "
                     << struct_value->getType()
                            ->getPointerElementType()
                            ->getStructName()
                            .str();
        return false;
    }
    ::llvm::IRBuilder<> builder(block);
    ::llvm::Value* value_ptr =
        builder.CreateStructGEP(struct_type_, struct_value, idx);
    *output = builder.CreateLoad(value_ptr);
    return true;
}
bool StructTypeIRBuilder::Set(::llvm::BasicBlock* block,
                              ::llvm::Value* struct_value, unsigned int idx,
                              ::llvm::Value* value) {
    if (block == NULL) {
        LOG(WARNING) << "the output ptr or block is NULL ";
        return false;
    }
    if (!IsStructPtr(struct_value->getType())) {
        LOG(WARNING) << "Fail set Struct value: struct pointer is required";
        return false;
    }
    if (struct_value->getType()->getPointerElementType() != struct_type_) {
        LOG(WARNING) << "Fail set Struct value: struct value type invalid "
                     << struct_value->getType()
                            ->getPointerElementType()
                            ->getStructName()
                            .str();
        return false;
    }
    ::llvm::IRBuilder<> builder(block);
    builder.getInt64(1);
    ::llvm::Value* value_ptr =
        builder.CreateStructGEP(struct_type_, struct_value, idx);
    if (nullptr == builder.CreateStore(value, value_ptr)) {
        LOG(WARNING) << "Fail Set Struct Value idx = " << idx;
        return false;
    }
    return true;
}
}  // namespace codegen
}  // namespace fesql
