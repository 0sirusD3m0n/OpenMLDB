/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * date_ir_builder.cc
 *
 * Author: chenjing
 * Date: 2020/6/1
 *--------------------------------------------------------------------------
 **/

#include "codegen/date_ir_builder.h"
#include <string>
#include <vector>
#include "codegen/arithmetic_expr_ir_builder.h"
namespace fesql {
namespace codegen {
DateIRBuilder::DateIRBuilder(::llvm::Module* m) : StructTypeIRBuilder(m) {
    InitStructType();
}
DateIRBuilder::~DateIRBuilder() {}
void DateIRBuilder::InitStructType() {
    std::string name = "fe.date";
    ::llvm::StringRef sr(name);
    ::llvm::StructType* stype = m_->getTypeByName(sr);
    if (stype != NULL) {
        struct_type_ = stype;
        return;
    }
    stype = ::llvm::StructType::create(m_->getContext(), name);
    ::llvm::Type* ts_ty = (::llvm::Type::getInt32Ty(m_->getContext()));
    std::vector<::llvm::Type*> elements;
    elements.push_back(ts_ty);
    stype->setBody(::llvm::ArrayRef<::llvm::Type*>(elements));
    struct_type_ = stype;
    return;
}
bool DateIRBuilder::CreateDefault(::llvm::BasicBlock* block,
                                  ::llvm::Value** output) {
    return NewDate(block, output);
}
bool DateIRBuilder::NewDate(::llvm::BasicBlock* block, ::llvm::Value** output) {
    if (block == NULL || output == NULL) {
        LOG(WARNING) << "the output ptr or block is NULL ";
        return false;
    }
    ::llvm::Value* date;
    if (!Create(block, &date)) {
        return false;
    }
    if (!SetDate(block, date,
                 ::llvm::ConstantInt::get(
                     ::llvm::Type::getInt32Ty(m_->getContext()), 0, false))) {
        return false;
    }
    *output = date;
    return true;
}
bool DateIRBuilder::NewDate(::llvm::BasicBlock* block, ::llvm::Value* days,
                            ::llvm::Value** output) {
    if (block == NULL || output == NULL) {
        LOG(WARNING) << "the output ptr or block is NULL ";
        return false;
    }
    ::llvm::Value* date;
    if (!Create(block, &date)) {
        return false;
    }
    if (!SetDate(block, date, days)) {
        return false;
    }
    *output = date;
    return true;
}
bool DateIRBuilder::CopyFrom(::llvm::BasicBlock* block, ::llvm::Value* src,
                             ::llvm::Value* dist) {
    if (nullptr == src || nullptr == dist) {
        LOG(WARNING) << "Fail to copy string: src or dist is null";
        return false;
    }
    if (!IsDatePtr(src->getType()) || !IsDatePtr(dist->getType())) {
        LOG(WARNING) << "Fail to copy string: src or dist isn't Date Ptr";
        return false;
    }
    ::llvm::Value* days;
    if (!GetDate(block, src, &days)) {
        return false;
    }
    if (!SetDate(block, dist, days)) {
        return false;
    }
    return true;
}
bool DateIRBuilder::GetDate(::llvm::BasicBlock* block, ::llvm::Value* date,
                            ::llvm::Value** output) {
    return Get(block, date, 0, output);
}
bool DateIRBuilder::SetDate(::llvm::BasicBlock* block, ::llvm::Value* date,
                            ::llvm::Value* code) {
    return Set(block, date, 0, code);
}

// return dayOfYear
// *day = date & 0x0000000FF;
bool DateIRBuilder::Day(::llvm::BasicBlock* block, ::llvm::Value* date,
                        ::llvm::Value** output, base::Status& status) {
    ::llvm::Value* code;
    if (!GetDate(block, date, &code)) {
        LOG(WARNING) << "Fail to GetDate";
        return false;
    }

    ::llvm::IRBuilder<> builder(block);
    codegen::ArithmeticIRBuilder arithmetic_ir_builder(block);
    if (!arithmetic_ir_builder.BuildAnd(block, code, builder.getInt32(255),
                                        &code, status)) {
        LOG(WARNING) << "Fail Compute Day of Date: " << status.msg;
        return false;
    }
    *output = code;
    return true;
}
// Return Month
//    *day = date & 0x0000000FF;
//    date = date >> 8;
//    *month = 1 + (date & 0x0000FF);
//    *year = 1900 + (date >> 8);
bool DateIRBuilder::Month(::llvm::BasicBlock* block, ::llvm::Value* date,
                          ::llvm::Value** output, base::Status& status) {
    ::llvm::Value* code;
    if (!GetDate(block, date, &code)) {
        LOG(WARNING) << "Fail to GetDate";
        return false;
    }

    ::llvm::IRBuilder<> builder(block);
    codegen::ArithmeticIRBuilder arithmetic_ir_builder(block);

    if (!arithmetic_ir_builder.BuildLShiftRight(
            block, code, builder.getInt32(8), &code, status)) {
        LOG(WARNING) << "Fail Compute Month of Date: " << status.msg;
        return false;
    }

    if (!arithmetic_ir_builder.BuildAnd(block, code, builder.getInt32(255),
                                        &code, status)) {
        LOG(WARNING) << "Fail Compute Month of Date: " << status.msg;
        return false;
    }

    if (!arithmetic_ir_builder.BuildAddExpr(block, code, builder.getInt32(1),
                                            &code, status)) {
        LOG(WARNING) << "Fail Compute Month of Date: " << status.msg;
        return false;
    }
    *output = code;
    return true;
}
// Return Year
//    *year = 1900 + (date >> 16);
bool DateIRBuilder::Year(::llvm::BasicBlock* block, ::llvm::Value* date,
                         ::llvm::Value** output, base::Status& status) {
    ::llvm::Value* code;
    if (!GetDate(block, date, &code)) {
        LOG(WARNING) << "Fail to GetDate";
        return false;
    }

    ::llvm::IRBuilder<> builder(block);
    codegen::ArithmeticIRBuilder arithmetic_ir_builder(block);

    if (!arithmetic_ir_builder.BuildLShiftRight(
            block, code, builder.getInt32(16), &code, status)) {
        LOG(WARNING) << "Fail Compute Year of Date: " << status.msg;
        return false;
    }
    if (!arithmetic_ir_builder.BuildAddExpr(block, code, builder.getInt32(1900),
                                            &code, status)) {
        LOG(WARNING) << "Fail Compute Year of Date: " << status.msg;
        return false;
    }
    *output = code;
    return true;
}

}  // namespace codegen
}  // namespace fesql
