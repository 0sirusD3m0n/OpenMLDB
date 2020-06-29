/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * arithmetic_expr_ir_builder_test.cc
 *
 * Author: chenjing
 * Date: 2020/1/8
 *--------------------------------------------------------------------------
 **/
#include "codegen/arithmetic_expr_ir_builder.h"
#include <memory>
#include <utility>
#include "codegen/ir_base_builder.h"
#include "codegen/string_ir_builder.h"
#include "codegen/timestamp_ir_builder.h"
#include "gtest/gtest.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "node/node_manager.h"
#include "udf/udf.h"

using namespace llvm;       // NOLINT
using namespace llvm::orc;  // NOLINT
ExitOnError ExitOnErr;
namespace fesql {
namespace codegen {
using fesql::codec::Timestamp;
class ArithmeticIRBuilderTest : public ::testing::Test {
 public:
    ArithmeticIRBuilderTest() { manager_ = new node::NodeManager(); }
    ~ArithmeticIRBuilderTest() { delete manager_; }

 protected:
    node::NodeManager *manager_;
};

template <class V1, class V2, class R>
void BinaryArithmeticExprCheck(::fesql::node::DataType left_type,
                               ::fesql::node::DataType right_type,
                               ::fesql::node::DataType dist_type, V1 value1,
                               V2 value2, R expected,
                               fesql::node::FnOperator op) {
    // Create an LLJIT instance.
    // Create an LLJIT instance.
    auto ctx = llvm::make_unique<LLVMContext>();
    auto m = make_unique<Module>("arithmetic_func", *ctx);

    llvm::Type *left_llvm_type = NULL;
    llvm::Type *right_llvm_type = NULL;
    llvm::Type *dist_llvm_type = NULL;
    ASSERT_TRUE(
        ::fesql::codegen::GetLLVMType(m.get(), left_type, &left_llvm_type));
    ASSERT_TRUE(
        ::fesql::codegen::GetLLVMType(m.get(), right_type, &right_llvm_type));
    ASSERT_TRUE(GetLLVMType(m.get(), dist_type, &dist_llvm_type));
    if (!dist_llvm_type->isPointerTy()) {
        dist_llvm_type = dist_llvm_type->getPointerTo();
    }

    Function *load_fn = Function::Create(
        FunctionType::get(::llvm::Type::getVoidTy(*ctx),
                          {left_llvm_type, right_llvm_type, dist_llvm_type},
                          false),
        Function::ExternalLinkage, "load_fn", m.get());
    BasicBlock *entry_block = BasicBlock::Create(*ctx, "EntryBlock", load_fn);
    IRBuilder<> builder(entry_block);
    auto iter = load_fn->arg_begin();
    Argument *arg0 = &(*iter);
    iter++;
    Argument *arg1 = &(*iter);
    iter++;
    Argument *arg2 = &(*iter);

    ScopeVar scope_var;
    scope_var.Enter("fn_base");
    scope_var.AddVar("a", NativeValue::Create(arg0));
    scope_var.AddVar("b", NativeValue::Create(arg1));
    ArithmeticIRBuilder arithmetic_ir_builder(entry_block);
    llvm::Value *output;
    base::Status status;

    bool ok;
    switch (op) {
        case fesql::node::kFnOpAdd:
            ok =
                arithmetic_ir_builder.BuildAddExpr(arg0, arg1, &output, status);
            break;
        case fesql::node::kFnOpMinus:
            ok =
                arithmetic_ir_builder.BuildSubExpr(arg0, arg1, &output, status);
            break;
        case fesql::node::kFnOpMulti:
            ok = arithmetic_ir_builder.BuildMultiExpr(arg0, arg1, &output,
                                                      status);
            break;
        case fesql::node::kFnOpFDiv:
            ok = arithmetic_ir_builder.BuildFDivExpr(arg0, arg1, &output,
                                                     status);
            break;
        case fesql::node::kFnOpMod:
            ok =
                arithmetic_ir_builder.BuildModExpr(arg0, arg1, &output, status);
            break;
        default: {
            FAIL();
        }
    }
    ASSERT_TRUE(ok);

    switch (dist_type) {
        case node::kTimestamp: {
            codegen::TimestampIRBuilder timestamp_builder(m.get());
            ASSERT_TRUE(timestamp_builder.CopyFrom(builder.GetInsertBlock(),
                                                   output, arg2));
            break;
        }
        case node::kVarchar: {
            codegen::StringIRBuilder string_builder(m.get());
            ASSERT_TRUE(string_builder.CopyFrom(builder.GetInsertBlock(),
                                                output, arg2));
            break;
        }
        default: {
            builder.CreateStore(output, arg2);
        }
    }
    builder.CreateRetVoid();
    m->print(::llvm::errs(), NULL);
    auto J = ExitOnErr(::llvm::orc::LLJITBuilder().create());
    auto &jd = J->getMainJITDylib();
    ::llvm::orc::MangleAndInterner mi(J->getExecutionSession(),
                                      J->getDataLayout());

    ::fesql::udf::InitCLibSymbol(jd, mi);
    ExitOnErr(J->addIRModule(ThreadSafeModule(std::move(m), std::move(ctx))));
    auto load_fn_jit = ExitOnErr(J->lookup("load_fn"));
    void (*decode)(V1, V2, R *) =
        (void (*)(V1, V2, R *))load_fn_jit.getAddress();
    R result;
    decode(value1, value2, &result);
    ASSERT_EQ(expected, result);
}

TEST_F(ArithmeticIRBuilderTest, test_add_int16_x_expr) {
    BinaryArithmeticExprCheck<int16_t, int16_t, int16_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kInt16, 1,
        1, 2, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<int16_t, int32_t, int32_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt32, ::fesql::node::kInt32, 1,
        1, 2, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<int16_t, int64_t, int64_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt64, ::fesql::node::kInt64, 1,
        8000000000L, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<int16_t, float, float>(
        ::fesql::node::kInt16, ::fesql::node::kFloat, ::fesql::node::kFloat, 1,
        12345678.5f, 12345678.5f + 1.0f, ::fesql::node::kFnOpAdd);
    BinaryArithmeticExprCheck<int16_t, double, double>(
        ::fesql::node::kInt16, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 12345678.5 + 1.0, ::fesql::node::kFnOpAdd);
}

TEST_F(ArithmeticIRBuilderTest, test_add_int32_x_expr) {
    BinaryArithmeticExprCheck<int32_t, int16_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kInt32, 1,
        1, 2, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<int32_t, int32_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kInt32, 1,
        1, 2, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<int32_t, int64_t, int64_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt64, ::fesql::node::kInt64, 1,
        8000000000L, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<int32_t, float, float>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kFloat, 1,
        12345678.5f, 12345678.5f + 1.0f, ::fesql::node::kFnOpAdd);
    BinaryArithmeticExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 12345678.5 + 1.0, ::fesql::node::kFnOpAdd);
}

TEST_F(ArithmeticIRBuilderTest, test_add_int64_x_expr) {
    BinaryArithmeticExprCheck<int64_t, int16_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt16, ::fesql::node::kInt64,
        8000000000L, 1, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<int64_t, int32_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt32, ::fesql::node::kInt64,
        8000000000L, 1, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<int64_t, int64_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kInt64, 1L,
        8000000000L, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<int64_t, float, float>(
        ::fesql::node::kInt64, ::fesql::node::kFloat, ::fesql::node::kFloat, 1L,
        12345678.5f, 12345678.5f + 1.0f, ::fesql::node::kFnOpAdd);
    BinaryArithmeticExprCheck<int64_t, double, double>(
        ::fesql::node::kInt64, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 12345678.5 + 1.0, ::fesql::node::kFnOpAdd);
}

TEST_F(ArithmeticIRBuilderTest, test_add_timestamp_expr) {
    Timestamp t1(8000000000L);
    Timestamp t2(1L);
    BinaryArithmeticExprCheck<Timestamp *, Timestamp *, Timestamp>(
        ::fesql::node::kTimestamp, ::fesql::node::kTimestamp,
        ::fesql::node::kTimestamp, &t1, &t2, Timestamp(8000000001L),
        ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<Timestamp *, int64_t, Timestamp>(
        ::fesql::node::kTimestamp, ::fesql::node::kInt64,
        ::fesql::node::kTimestamp, &t1, 1L, Timestamp(8000000001L),
        ::fesql::node::kFnOpAdd);
    BinaryArithmeticExprCheck<Timestamp *, int32_t, Timestamp>(
        ::fesql::node::kTimestamp, ::fesql::node::kInt32,
        ::fesql::node::kTimestamp, &t1, 1, Timestamp(8000000001L),
        ::fesql::node::kFnOpAdd);
}

TEST_F(ArithmeticIRBuilderTest, test_sub_timestamp_expr) {
    Timestamp t1(8000000000L);
    Timestamp t2(1L);
    BinaryArithmeticExprCheck<Timestamp *, Timestamp *, int64_t>(
        ::fesql::node::kTimestamp, ::fesql::node::kTimestamp,
        ::fesql::node::kInt64, &t1, &t2, 7999999999L,
        ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<Timestamp *, int64_t, int64_t>(
        ::fesql::node::kTimestamp, ::fesql::node::kInt64, ::fesql::node::kInt64,
        &t1, 1L, 7999999999L, ::fesql::node::kFnOpMinus);
    BinaryArithmeticExprCheck<Timestamp *, int32_t, int64_t>(
        ::fesql::node::kTimestamp, ::fesql::node::kInt32, ::fesql::node::kInt64,
        &t1, 1, 7999999999L, ::fesql::node::kFnOpMinus);
}
TEST_F(ArithmeticIRBuilderTest, test_add_float_x_expr) {
    BinaryArithmeticExprCheck<float, int16_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt16, ::fesql::node::kFloat,
        1.0f, 1, 2.0f, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<float, int32_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt32, ::fesql::node::kFloat,
        8000000000L, 1, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<float, int64_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt64, ::fesql::node::kFloat,
        1.0f, 200000L, 1.0f + 200000.0f, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<float, float, float>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kFloat,
        1.0f, 12345678.5f, 12345678.5f + 1.0f, ::fesql::node::kFnOpAdd);
    BinaryArithmeticExprCheck<float, double, double>(
        ::fesql::node::kFloat, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1.0f, 12345678.5, 12345678.5 + 1.0, ::fesql::node::kFnOpAdd);
}

TEST_F(ArithmeticIRBuilderTest, test_add_double_x_expr) {
    BinaryArithmeticExprCheck<double, int16_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt16, ::fesql::node::kDouble,
        1.0, 1, 2.0, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<double, int32_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt32, ::fesql::node::kDouble,
        8000000000L, 1, 8000000001.0, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<double, int64_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt64, ::fesql::node::kDouble,
        1.0f, 200000L, 200001.0, ::fesql::node::kFnOpAdd);

    BinaryArithmeticExprCheck<double, float, double>(
        ::fesql::node::kDouble, ::fesql::node::kFloat, ::fesql::node::kDouble,
        1.0, 12345678.5f, static_cast<double>(12345678.5f) + 1.0,
        ::fesql::node::kFnOpAdd);
    BinaryArithmeticExprCheck<double, double, double>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1.0, 12345678.5, 12345678.5 + 1.0, ::fesql::node::kFnOpAdd);
}

TEST_F(ArithmeticIRBuilderTest, test_sub_int16_x_expr) {
    BinaryArithmeticExprCheck<int16_t, int16_t, int16_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kInt16, 2,
        1, 1, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<int16_t, int32_t, int32_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt32, ::fesql::node::kInt32, 2,
        1, 1, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<int16_t, int64_t, int64_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt64, ::fesql::node::kInt64, 1,
        8000000000L, 1L - 8000000000L, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<int16_t, float, float>(
        ::fesql::node::kInt16, ::fesql::node::kFloat, ::fesql::node::kFloat, 1,
        12345678.5f, 1.0f - 12345678.5f, ::fesql::node::kFnOpMinus);
    BinaryArithmeticExprCheck<int16_t, double, double>(
        ::fesql::node::kInt16, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 1.0 - 12345678.5, ::fesql::node::kFnOpMinus);
}

TEST_F(ArithmeticIRBuilderTest, test_sub_int32_x_expr) {
    BinaryArithmeticExprCheck<int32_t, int16_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kInt32, 2,
        1, 1, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<int32_t, int32_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kInt32, 2,
        1, 1, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<int32_t, int64_t, int64_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt64, ::fesql::node::kInt64, 1,
        8000000000L, 1L - 8000000000L, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<int32_t, float, float>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kFloat, 1,
        12345678.5f, 1.0f - 12345678.5f, ::fesql::node::kFnOpMinus);
    BinaryArithmeticExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 1.0 - 12345678.5, ::fesql::node::kFnOpMinus);
}

TEST_F(ArithmeticIRBuilderTest, test_sub_int64_x_expr) {
    BinaryArithmeticExprCheck<int64_t, int16_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt16, ::fesql::node::kInt64,
        8000000000L, 1, 8000000000L - 1L, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<int64_t, int32_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt32, ::fesql::node::kInt64,
        8000000000L, 1, 8000000000L - 1L, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<int64_t, int64_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kInt64, 1L,
        8000000000L, 1L - 8000000000L, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<int64_t, float, float>(
        ::fesql::node::kInt64, ::fesql::node::kFloat, ::fesql::node::kFloat, 1L,
        12345678.5f, 1.0f - 12345678.5f, ::fesql::node::kFnOpMinus);
    BinaryArithmeticExprCheck<int64_t, double, double>(
        ::fesql::node::kInt64, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 1.0 - 12345678.5, ::fesql::node::kFnOpMinus);
}

TEST_F(ArithmeticIRBuilderTest, test_sub_float_x_expr) {
    BinaryArithmeticExprCheck<float, int16_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt16, ::fesql::node::kFloat,
        2.0f, 1, 1.0f, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<float, int32_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt32, ::fesql::node::kFloat,
        8000000000L, 1, 8000000000.0f - 1.0f, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<float, int64_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt64, ::fesql::node::kFloat,
        1.0f, 200000L, 1.0f - 200000.0f, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<float, float, float>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kFloat,
        1.0f, 12345678.5f, 1.0f - 12345678.5f, ::fesql::node::kFnOpMinus);
    BinaryArithmeticExprCheck<float, double, double>(
        ::fesql::node::kFloat, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1.0f, 12345678.5, 1.0 - 12345678.5, ::fesql::node::kFnOpMinus);
}

TEST_F(ArithmeticIRBuilderTest, test_sub_double_x_expr) {
    BinaryArithmeticExprCheck<double, int16_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt16, ::fesql::node::kDouble,
        2.0, 1, 1.0, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<double, int32_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt32, ::fesql::node::kDouble,
        8000000000L, 1, 8000000000.0 - 1.0, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<double, int64_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt64, ::fesql::node::kDouble,
        1.0f, 200000L, 1.0 - 200000.0, ::fesql::node::kFnOpMinus);

    BinaryArithmeticExprCheck<double, float, double>(
        ::fesql::node::kDouble, ::fesql::node::kFloat, ::fesql::node::kDouble,
        1.0, 12345678.5f, 1.0 - static_cast<double>(12345678.5f),
        ::fesql::node::kFnOpMinus);
    BinaryArithmeticExprCheck<double, double, double>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1.0, 12345678.5, 1.0 - 12345678.5, ::fesql::node::kFnOpMinus);
}

TEST_F(ArithmeticIRBuilderTest, test_mul_int16_x_expr) {
    BinaryArithmeticExprCheck<int16_t, int16_t, int16_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kInt16, 2,
        3, 2 * 3, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<int16_t, int32_t, int32_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt32, ::fesql::node::kInt32, 2,
        3, 2 * 3, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<int16_t, int64_t, int64_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt64, ::fesql::node::kInt64, 2,
        8000000000L, 2L * 8000000000L, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<int16_t, float, float>(
        ::fesql::node::kInt16, ::fesql::node::kFloat, ::fesql::node::kFloat, 2,
        12345678.5f, 2.0f * 12345678.5f, ::fesql::node::kFnOpMulti);
    BinaryArithmeticExprCheck<int16_t, double, double>(
        ::fesql::node::kInt16, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2, 12345678.5, 2.0 * 12345678.5, ::fesql::node::kFnOpMulti);
}
TEST_F(ArithmeticIRBuilderTest, test_multi_int32_x_expr) {
    BinaryArithmeticExprCheck<int32_t, int16_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kInt32, 2,
        3, 2 * 3, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<int32_t, int32_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kInt32, 2,
        3, 2 * 3, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<int32_t, int64_t, int64_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt64, ::fesql::node::kInt64, 2,
        8000000000L, 2L * 8000000000L, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<int32_t, float, float>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kFloat, 2,
        12345678.5f, 2.0f * 12345678.5f, ::fesql::node::kFnOpMulti);
    BinaryArithmeticExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2, 12345678.5, 2.0 * 12345678.5, ::fesql::node::kFnOpMulti);
}
TEST_F(ArithmeticIRBuilderTest, test_multi_int64_x_expr) {
    BinaryArithmeticExprCheck<int64_t, int16_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt16, ::fesql::node::kInt64,
        8000000000L, 2L, 8000000000L * 2L, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<int64_t, int32_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt32, ::fesql::node::kInt64,
        8000000000L, 2, 8000000000L * 2L, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<int64_t, int64_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kInt64, 2L,
        8000000000L, 2L * 8000000000L, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<int64_t, float, float>(
        ::fesql::node::kInt64, ::fesql::node::kFloat, ::fesql::node::kFloat, 2L,
        12345678.5f, 2.0f * 12345678.5f, ::fesql::node::kFnOpMulti);
    BinaryArithmeticExprCheck<int64_t, double, double>(
        ::fesql::node::kInt64, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2, 12345678.5, 2.0 * 12345678.5, ::fesql::node::kFnOpMulti);
}

TEST_F(ArithmeticIRBuilderTest, test_multi_float_x_expr) {
    BinaryArithmeticExprCheck<float, int16_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt16, ::fesql::node::kFloat,
        2.0f, 3.0f, 2.0f * 3.0f, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<float, int32_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt32, ::fesql::node::kFloat,
        8000000000L, 2, 8000000000.0f * 2.0f, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<float, int64_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt64, ::fesql::node::kFloat,
        2.0f, 200000L, 2.0f * 200000.0f, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<float, float, float>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kFloat,
        2.0f, 12345678.5f, 2.0f * 12345678.5f, ::fesql::node::kFnOpMulti);
    BinaryArithmeticExprCheck<float, double, double>(
        ::fesql::node::kFloat, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2.0f, 12345678.5, 2.0 * 12345678.5, ::fesql::node::kFnOpMulti);
}
TEST_F(ArithmeticIRBuilderTest, test_multi_double_x_expr) {
    BinaryArithmeticExprCheck<double, int16_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt16, ::fesql::node::kDouble,
        2.0, 3, 2.0 * 3.0, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<double, int32_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt32, ::fesql::node::kDouble,
        8000000000L, 2, 8000000000.0 * 2.0, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<double, int64_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt64, ::fesql::node::kDouble,
        2.0f, 200000L, 2.0 * 200000.0, ::fesql::node::kFnOpMulti);

    BinaryArithmeticExprCheck<double, float, double>(
        ::fesql::node::kDouble, ::fesql::node::kFloat, ::fesql::node::kDouble,
        2.0, 12345678.5f, 2.0 * static_cast<double>(12345678.5f),
        ::fesql::node::kFnOpMulti);
    BinaryArithmeticExprCheck<double, double, double>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2.0, 12345678.5, 2.0 * 12345678.5, ::fesql::node::kFnOpMulti);
}

TEST_F(ArithmeticIRBuilderTest, test_fdiv_zero) {
    BinaryArithmeticExprCheck<int32_t, int16_t, double>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kDouble, 2,
        0, 2.0 / 0.0, ::fesql::node::kFnOpFDiv);
    BinaryArithmeticExprCheck<int32_t, int16_t, double>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kDouble, 2,
        0, 2.0 / 0.0, ::fesql::node::kFnOpFDiv);

    BinaryArithmeticExprCheck<int64_t, int16_t, double>(
        ::fesql::node::kInt64, ::fesql::node::kInt32, ::fesql::node::kDouble,
        99999999L, 0, 99999999.0 / 0.0, ::fesql::node::kFnOpFDiv);
    BinaryArithmeticExprCheck<int32_t, float, double>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kDouble, 2,
        0.0f, 2.0 / 0.0, ::fesql::node::kFnOpFDiv);
    BinaryArithmeticExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2, 0.0, 2.0 / 0.0, ::fesql::node::kFnOpFDiv);
    std::cout << std::to_string(1 / 0.0) << std::endl;
}

TEST_F(ArithmeticIRBuilderTest, test_fdiv_int32_x_expr) {
    BinaryArithmeticExprCheck<int32_t, int16_t, double>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kDouble, 2,
        3, 2.0 / 3.0, ::fesql::node::kFnOpFDiv);

    BinaryArithmeticExprCheck<int32_t, int32_t, double>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kDouble, 2,
        3, 2.0 / 3.0, ::fesql::node::kFnOpFDiv);

    BinaryArithmeticExprCheck<int32_t, int64_t, double>(
        ::fesql::node::kInt32, ::fesql::node::kInt64, ::fesql::node::kDouble, 2,
        8000000000L, 2.0 / 8000000000.0, ::fesql::node::kFnOpFDiv);
    //
    BinaryArithmeticExprCheck<int32_t, float, double>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kDouble, 2,
        3.0f, 2.0 / 3.0, ::fesql::node::kFnOpFDiv);
    BinaryArithmeticExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2, 12345678.5, 2.0 / 12345678.5, ::fesql::node::kFnOpFDiv);
}

TEST_F(ArithmeticIRBuilderTest, test_mod_int32_x_expr) {
    BinaryArithmeticExprCheck<int32_t, int16_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kInt32, 12,
        5, 2, ::fesql::node::kFnOpMod);

    BinaryArithmeticExprCheck<int32_t, int32_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kInt32, 12,
        5, 2, ::fesql::node::kFnOpMod);

    BinaryArithmeticExprCheck<int32_t, int64_t, int64_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt64, ::fesql::node::kInt64, 12,
        50000L, 12L, ::fesql::node::kFnOpMod);

    BinaryArithmeticExprCheck<int32_t, float, float>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kFloat, 12,
        5.1f, fmod(12.0f, 5.1f), ::fesql::node::kFnOpMod);
    BinaryArithmeticExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);
}

TEST_F(ArithmeticIRBuilderTest, test_mod_float_x_expr) {
    BinaryArithmeticExprCheck<int16_t, float, float>(
        ::fesql::node::kInt16, ::fesql::node::kFloat, ::fesql::node::kFloat, 12,
        5.1f, fmod(12.0f, 5.1f), ::fesql::node::kFnOpMod);

    BinaryArithmeticExprCheck<int32_t, float, float>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kFloat, 12,
        5.1f, fmod(12.0f, 5.1f), ::fesql::node::kFnOpMod);

    BinaryArithmeticExprCheck<int64_t, float, float>(
        ::fesql::node::kInt64, ::fesql::node::kFloat, ::fesql::node::kFloat,
        12L, 5.1f, fmod(12.0f, 5.1f), ::fesql::node::kFnOpMod);

    BinaryArithmeticExprCheck<float, float, float>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kFloat,
        12.0f, 5.1f, fmod(12.0f, 5.1f), ::fesql::node::kFnOpMod);
}

TEST_F(ArithmeticIRBuilderTest, test_mod_double_x_expr) {
    BinaryArithmeticExprCheck<int16_t, double, double>(
        ::fesql::node::kInt16, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);

    BinaryArithmeticExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);

    BinaryArithmeticExprCheck<int64_t, double, double>(
        ::fesql::node::kInt64, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12L, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);

    BinaryArithmeticExprCheck<float, double, double>(
        ::fesql::node::kFloat, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12.0f, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);
    BinaryArithmeticExprCheck<double, double, double>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12.0, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);
}
}  // namespace codegen
}  // namespace fesql
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    return RUN_ALL_TESTS();
}
