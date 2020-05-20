/*
 * ir_base_builder_test.cc
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

#include "codegen/expr_ir_builder.h"
#include <memory>
#include <utility>
#include "codegen/ir_base_builder.h"
#include "gtest/gtest.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/AggressiveInstCombine/AggressiveInstCombine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include "node/node_manager.h"
using namespace llvm;       // NOLINT
using namespace llvm::orc;  // NOLINT

ExitOnError ExitOnErr;

namespace fesql {
namespace codegen {

class ExprIRBuilderTest : public ::testing::Test {
 public:
    ExprIRBuilderTest() { manager_ = new node::NodeManager(); }
    ~ExprIRBuilderTest() { delete manager_; }

 protected:
    node::NodeManager *manager_;
};

void GenAddExpr(node::NodeManager *manager, ::fesql::node::ExprNode **expr) {
    // TODO(wangtaize) free
    new ::fesql::node::BinaryExpr(::fesql::node::kFnOpAdd);

    ::fesql::node::ExprNode *i32_node = (manager->MakeConstNode(1));
    ::fesql::node::ExprNode *id_node = (manager->MakeExprIdNode("a"));
    ::fesql::node::ExprNode *bexpr =
        (manager->MakeBinaryExprNode(i32_node, id_node, fesql::node::kFnOpAdd));
    *expr = bexpr;
}

TEST_F(ExprIRBuilderTest, test_add_int32) {
    // Create an LLJIT instance.
    auto ctx = llvm::make_unique<LLVMContext>();
    auto m = make_unique<Module>("int32_add", *ctx);
    // Create the add1 function entry and insert this entry into module M.  The
    // function will have a return type of "int" and take an argument of "int".
    Function *load_fn =
        Function::Create(FunctionType::get(Type::getInt32Ty(*ctx),
                                           {Type::getInt32Ty(*ctx)}, false),
                         Function::ExternalLinkage, "load_fn", m.get());
    BasicBlock *entry_block = BasicBlock::Create(*ctx, "EntryBlock", load_fn);
    IRBuilder<> builder(entry_block);
    Argument *arg0 = &*load_fn->arg_begin();
    ScopeVar scope_var;
    scope_var.Enter("fn_base");
    scope_var.AddVar("a", arg0);
    ExprIRBuilder expr_builder(entry_block, &scope_var);
    ::fesql::node::ExprNode *node = NULL;
    GenAddExpr(manager_, &node);
    llvm::Value *output;
    base::Status status;
    bool ok = expr_builder.Build(node, &output, status);
    ASSERT_TRUE(ok);
    builder.CreateRet(output);
    m->print(::llvm::errs(), NULL);
    auto J = ExitOnErr(LLJITBuilder().create());
    ExitOnErr(J->addIRModule(ThreadSafeModule(std::move(m), std::move(ctx))));
    auto load_fn_jit = ExitOnErr(J->lookup("load_fn"));
    int32_t (*decode)(int32_t) = (int32_t(*)(int32_t))load_fn_jit.getAddress();
    int32_t ret = decode(1);
    ASSERT_EQ(ret, 2);
}

template <class V1, class V2, class R>
void BinaryExprCheck(::fesql::node::DataType left_type,
                     ::fesql::node::DataType right_type,
                     ::fesql::node::DataType dist_type, V1 v1, V2 v2, R r,
                     ::fesql::node::FnOperator op) {
    node::NodeManager manager;
    ::fesql::node::ExprNode *expr_node = manager.MakeBinaryExprNode(
        manager.MakeConstNode(v1), manager.MakeConstNode(v2), op);
    // Create an LLJIT instance.
    auto ctx = llvm::make_unique<LLVMContext>();
    auto m = make_unique<Module>("binary_expr_fn", *ctx);
    // Create the add1 function entry and insert this entry into module M.  The
    // function will have a return type of "int" and take an argument of "int".
    llvm::Type *left_llvm_type = NULL;
    llvm::Type *right_llvm_type = NULL;
    llvm::Type *dist_llvm_type = NULL;
    ASSERT_TRUE(
        ::fesql::codegen::GetLLVMType(m.get(), left_type, &left_llvm_type));
    ASSERT_TRUE(
        ::fesql::codegen::GetLLVMType(m.get(), right_type, &right_llvm_type));
    ASSERT_TRUE(GetLLVMType(m.get(), dist_type, &dist_llvm_type));

    // Create the add1 function entry and insert this entry into module M.  The
    // function will have a return type of "D" and take an argument of "S".
    Function *load_fn = Function::Create(
        FunctionType::get(dist_llvm_type, {left_llvm_type, right_llvm_type},
                          false),
        Function::ExternalLinkage, "load_fn", m.get());
    BasicBlock *entry_block = BasicBlock::Create(*ctx, "EntryBlock", load_fn);
    IRBuilder<> builder(entry_block);
    ScopeVar scope_var;
    scope_var.Enter("fn_base");
    ExprIRBuilder expr_builder(entry_block, &scope_var);
    llvm::Value *output;
    base::Status status;
    bool ok = expr_builder.Build(expr_node, &output, status);
    ASSERT_TRUE(ok);
    builder.CreateRet(output);
    m->print(::llvm::errs(), NULL);
    auto J = ExitOnErr(LLJITBuilder().create());
    ExitOnErr(J->addIRModule(ThreadSafeModule(std::move(m), std::move(ctx))));
    auto load_fn_jit = ExitOnErr(J->lookup("load_fn"));
    R (*decode)(V1, V2) = (R(*)(V1, V2))load_fn_jit.getAddress();
    R ret = decode(v1, v2);
    ASSERT_EQ(ret, r);
}

TEST_F(ExprIRBuilderTest, test_add_int16_x_expr) {
    BinaryExprCheck<int16_t, int16_t, int16_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kInt16, 1,
        1, 2, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<int16_t, int32_t, int32_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt32, ::fesql::node::kInt32, 1,
        1, 2, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<int16_t, int64_t, int64_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt64, ::fesql::node::kInt64, 1,
        8000000000L, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<int16_t, float, float>(
        ::fesql::node::kInt16, ::fesql::node::kFloat, ::fesql::node::kFloat, 1,
        12345678.5f, 12345678.5f + 1.0f, ::fesql::node::kFnOpAdd);
    BinaryExprCheck<int16_t, double, double>(
        ::fesql::node::kInt16, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 12345678.5 + 1.0, ::fesql::node::kFnOpAdd);
}

TEST_F(ExprIRBuilderTest, test_add_int32_x_expr) {
    BinaryExprCheck<int32_t, int16_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kInt32, 1,
        1, 2, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<int32_t, int32_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kInt32, 1,
        1, 2, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<int32_t, int64_t, int64_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt64, ::fesql::node::kInt64, 1,
        8000000000L, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<int32_t, float, float>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kFloat, 1,
        12345678.5f, 12345678.5f + 1.0f, ::fesql::node::kFnOpAdd);
    BinaryExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 12345678.5 + 1.0, ::fesql::node::kFnOpAdd);
}

TEST_F(ExprIRBuilderTest, test_add_int64_x_expr) {
    BinaryExprCheck<int64_t, int16_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt16, ::fesql::node::kInt64,
        8000000000L, 1, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<int64_t, int32_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt32, ::fesql::node::kInt64,
        8000000000L, 1, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<int64_t, int64_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kInt64, 1L,
        8000000000L, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<int64_t, float, float>(
        ::fesql::node::kInt64, ::fesql::node::kFloat, ::fesql::node::kFloat, 1L,
        12345678.5f, 12345678.5f + 1.0f, ::fesql::node::kFnOpAdd);
    BinaryExprCheck<int64_t, double, double>(
        ::fesql::node::kInt64, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 12345678.5 + 1.0, ::fesql::node::kFnOpAdd);
}

TEST_F(ExprIRBuilderTest, test_add_float_x_expr) {
    BinaryExprCheck<float, int16_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt16, ::fesql::node::kFloat,
        1.0f, 1, 2.0f, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<float, int32_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt32, ::fesql::node::kFloat,
        8000000000L, 1, 8000000001L, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<float, int64_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt64, ::fesql::node::kFloat,
        1.0f, 200000L, 1.0f + 200000.0f, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<float, float, float>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kFloat,
        1.0f, 12345678.5f, 12345678.5f + 1.0f, ::fesql::node::kFnOpAdd);
    BinaryExprCheck<float, double, double>(
        ::fesql::node::kFloat, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1.0f, 12345678.5, 12345678.5 + 1.0, ::fesql::node::kFnOpAdd);
}

TEST_F(ExprIRBuilderTest, test_add_double_x_expr) {
    BinaryExprCheck<double, int16_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt16, ::fesql::node::kDouble,
        1.0, 1, 2.0, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<double, int32_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt32, ::fesql::node::kDouble,
        8000000000L, 1, 8000000001.0, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<double, int64_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt64, ::fesql::node::kDouble,
        1.0f, 200000L, 200001.0, ::fesql::node::kFnOpAdd);

    BinaryExprCheck<double, float, double>(
        ::fesql::node::kDouble, ::fesql::node::kFloat, ::fesql::node::kDouble,
        1.0, 12345678.5f, static_cast<double>(12345678.5f) + 1.0,
        ::fesql::node::kFnOpAdd);
    BinaryExprCheck<double, double, double>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1.0, 12345678.5, 12345678.5 + 1.0, ::fesql::node::kFnOpAdd);
}

TEST_F(ExprIRBuilderTest, test_sub_int16_x_expr) {
    BinaryExprCheck<int16_t, int16_t, int16_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kInt16, 2,
        1, 1, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<int16_t, int32_t, int32_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt32, ::fesql::node::kInt32, 2,
        1, 1, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<int16_t, int64_t, int64_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt64, ::fesql::node::kInt64, 1,
        8000000000L, 1L - 8000000000L, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<int16_t, float, float>(
        ::fesql::node::kInt16, ::fesql::node::kFloat, ::fesql::node::kFloat, 1,
        12345678.5f, 1.0f - 12345678.5f, ::fesql::node::kFnOpMinus);
    BinaryExprCheck<int16_t, double, double>(
        ::fesql::node::kInt16, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 1.0 - 12345678.5, ::fesql::node::kFnOpMinus);
}

TEST_F(ExprIRBuilderTest, test_sub_int32_x_expr) {
    BinaryExprCheck<int32_t, int16_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kInt32, 2,
        1, 1, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<int32_t, int32_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kInt32, 2,
        1, 1, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<int32_t, int64_t, int64_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt64, ::fesql::node::kInt64, 1,
        8000000000L, 1L - 8000000000L, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<int32_t, float, float>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kFloat, 1,
        12345678.5f, 1.0f - 12345678.5f, ::fesql::node::kFnOpMinus);
    BinaryExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 1.0 - 12345678.5, ::fesql::node::kFnOpMinus);
}

TEST_F(ExprIRBuilderTest, test_sub_int64_x_expr) {
    BinaryExprCheck<int64_t, int16_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt16, ::fesql::node::kInt64,
        8000000000L, 1, 8000000000L - 1L, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<int64_t, int32_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt32, ::fesql::node::kInt64,
        8000000000L, 1, 8000000000L - 1L, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<int64_t, int64_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kInt64, 1L,
        8000000000L, 1L - 8000000000L, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<int64_t, float, float>(
        ::fesql::node::kInt64, ::fesql::node::kFloat, ::fesql::node::kFloat, 1L,
        12345678.5f, 1.0f - 12345678.5f, ::fesql::node::kFnOpMinus);
    BinaryExprCheck<int64_t, double, double>(
        ::fesql::node::kInt64, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1, 12345678.5, 1.0 - 12345678.5, ::fesql::node::kFnOpMinus);
}

TEST_F(ExprIRBuilderTest, test_sub_float_x_expr) {
    BinaryExprCheck<float, int16_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt16, ::fesql::node::kFloat,
        2.0f, 1, 1.0f, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<float, int32_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt32, ::fesql::node::kFloat,
        8000000000L, 1, 8000000000.0f - 1.0f, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<float, int64_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt64, ::fesql::node::kFloat,
        1.0f, 200000L, 1.0f - 200000.0f, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<float, float, float>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kFloat,
        1.0f, 12345678.5f, 1.0f - 12345678.5f, ::fesql::node::kFnOpMinus);
    BinaryExprCheck<float, double, double>(
        ::fesql::node::kFloat, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1.0f, 12345678.5, 1.0 - 12345678.5, ::fesql::node::kFnOpMinus);
}

TEST_F(ExprIRBuilderTest, test_sub_double_x_expr) {
    BinaryExprCheck<double, int16_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt16, ::fesql::node::kDouble,
        2.0, 1, 1.0, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<double, int32_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt32, ::fesql::node::kDouble,
        8000000000L, 1, 8000000000.0 - 1.0, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<double, int64_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt64, ::fesql::node::kDouble,
        1.0f, 200000L, 1.0 - 200000.0, ::fesql::node::kFnOpMinus);

    BinaryExprCheck<double, float, double>(
        ::fesql::node::kDouble, ::fesql::node::kFloat, ::fesql::node::kDouble,
        1.0, 12345678.5f, 1.0 - static_cast<double>(12345678.5f),
        ::fesql::node::kFnOpMinus);
    BinaryExprCheck<double, double, double>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kDouble,
        1.0, 12345678.5, 1.0 - 12345678.5, ::fesql::node::kFnOpMinus);
}

TEST_F(ExprIRBuilderTest, test_mul_int16_x_expr) {
    BinaryExprCheck<int16_t, int16_t, int16_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kInt16, 2,
        3, 2 * 3, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<int16_t, int32_t, int32_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt32, ::fesql::node::kInt32, 2,
        3, 2 * 3, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<int16_t, int64_t, int64_t>(
        ::fesql::node::kInt16, ::fesql::node::kInt64, ::fesql::node::kInt64, 2,
        8000000000L, 2L * 8000000000L, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<int16_t, float, float>(
        ::fesql::node::kInt16, ::fesql::node::kFloat, ::fesql::node::kFloat, 2,
        12345678.5f, 2.0f * 12345678.5f, ::fesql::node::kFnOpMulti);
    BinaryExprCheck<int16_t, double, double>(
        ::fesql::node::kInt16, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2, 12345678.5, 2.0 * 12345678.5, ::fesql::node::kFnOpMulti);
}
TEST_F(ExprIRBuilderTest, test_multi_int32_x_expr) {
    BinaryExprCheck<int32_t, int16_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kInt32, 2,
        3, 2 * 3, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<int32_t, int32_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kInt32, 2,
        3, 2 * 3, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<int32_t, int64_t, int64_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt64, ::fesql::node::kInt64, 2,
        8000000000L, 2L * 8000000000L, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<int32_t, float, float>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kFloat, 2,
        12345678.5f, 2.0f * 12345678.5f, ::fesql::node::kFnOpMulti);
    BinaryExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2, 12345678.5, 2.0 * 12345678.5, ::fesql::node::kFnOpMulti);
}
TEST_F(ExprIRBuilderTest, test_multi_int64_x_expr) {
    BinaryExprCheck<int64_t, int16_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt16, ::fesql::node::kInt64,
        8000000000L, 2L, 8000000000L * 2L, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<int64_t, int32_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt32, ::fesql::node::kInt64,
        8000000000L, 2, 8000000000L * 2L, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<int64_t, int64_t, int64_t>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kInt64, 2L,
        8000000000L, 2L * 8000000000L, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<int64_t, float, float>(
        ::fesql::node::kInt64, ::fesql::node::kFloat, ::fesql::node::kFloat, 2L,
        12345678.5f, 2.0f * 12345678.5f, ::fesql::node::kFnOpMulti);
    BinaryExprCheck<int64_t, double, double>(
        ::fesql::node::kInt64, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2, 12345678.5, 2.0 * 12345678.5, ::fesql::node::kFnOpMulti);
}

TEST_F(ExprIRBuilderTest, test_multi_float_x_expr) {
    BinaryExprCheck<float, int16_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt16, ::fesql::node::kFloat,
        2.0f, 3.0f, 2.0f * 3.0f, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<float, int32_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt32, ::fesql::node::kFloat,
        8000000000L, 2, 8000000000.0f * 2.0f, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<float, int64_t, float>(
        ::fesql::node::kFloat, ::fesql::node::kInt64, ::fesql::node::kFloat,
        2.0f, 200000L, 2.0f * 200000.0f, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<float, float, float>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kFloat,
        2.0f, 12345678.5f, 2.0f * 12345678.5f, ::fesql::node::kFnOpMulti);
    BinaryExprCheck<float, double, double>(
        ::fesql::node::kFloat, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2.0f, 12345678.5, 2.0 * 12345678.5, ::fesql::node::kFnOpMulti);
}
TEST_F(ExprIRBuilderTest, test_multi_double_x_expr) {
    BinaryExprCheck<double, int16_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt16, ::fesql::node::kDouble,
        2.0, 3, 2.0 * 3.0, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<double, int32_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt32, ::fesql::node::kDouble,
        8000000000L, 2, 8000000000.0 * 2.0, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<double, int64_t, double>(
        ::fesql::node::kDouble, ::fesql::node::kInt64, ::fesql::node::kDouble,
        2.0f, 200000L, 2.0 * 200000.0, ::fesql::node::kFnOpMulti);

    BinaryExprCheck<double, float, double>(
        ::fesql::node::kDouble, ::fesql::node::kFloat, ::fesql::node::kDouble,
        2.0, 12345678.5f, 2.0 * static_cast<double>(12345678.5f),
        ::fesql::node::kFnOpMulti);
    BinaryExprCheck<double, double, double>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2.0, 12345678.5, 2.0 * 12345678.5, ::fesql::node::kFnOpMulti);
}

TEST_F(ExprIRBuilderTest, test_fdiv_int32_x_expr) {
    BinaryExprCheck<int32_t, int16_t, double>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kDouble, 2,
        3, 2.0 / 3.0, ::fesql::node::kFnOpFDiv);

    BinaryExprCheck<int32_t, int32_t, double>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kDouble, 2,
        3, 2.0 / 3.0, ::fesql::node::kFnOpFDiv);

    BinaryExprCheck<int32_t, int64_t, double>(
        ::fesql::node::kInt32, ::fesql::node::kInt64, ::fesql::node::kDouble, 2,
        8000000000L, 2.0 / 8000000000.0, ::fesql::node::kFnOpFDiv);
    //
    BinaryExprCheck<int32_t, float, double>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kDouble, 2,
        3.0f, 2.0 / 3.0, ::fesql::node::kFnOpFDiv);
    BinaryExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        2, 12345678.5, 2.0 / 12345678.5, ::fesql::node::kFnOpFDiv);
}

TEST_F(ExprIRBuilderTest, test_mod_int32_x_expr) {
    BinaryExprCheck<int32_t, int16_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt16, ::fesql::node::kInt32, 12,
        5, 2, ::fesql::node::kFnOpMod);

    BinaryExprCheck<int32_t, int32_t, int32_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kInt32, 12,
        5, 2, ::fesql::node::kFnOpMod);

    BinaryExprCheck<int32_t, int64_t, int64_t>(
        ::fesql::node::kInt32, ::fesql::node::kInt64, ::fesql::node::kInt64, 12,
        50000L, 12L, ::fesql::node::kFnOpMod);

    BinaryExprCheck<int32_t, float, float>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kFloat, 12,
        5.1f, fmod(12.0f, 5.1f), ::fesql::node::kFnOpMod);
    BinaryExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);
}

TEST_F(ExprIRBuilderTest, test_mod_float_x_expr) {
    BinaryExprCheck<int16_t, float, float>(
        ::fesql::node::kInt16, ::fesql::node::kFloat, ::fesql::node::kFloat, 12,
        5.1f, fmod(12.0f, 5.1f), ::fesql::node::kFnOpMod);

    BinaryExprCheck<int32_t, float, float>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kFloat, 12,
        5.1f, fmod(12.0f, 5.1f), ::fesql::node::kFnOpMod);

    BinaryExprCheck<int64_t, float, float>(
        ::fesql::node::kInt64, ::fesql::node::kFloat, ::fesql::node::kFloat,
        12L, 5.1f, fmod(12.0f, 5.1f), ::fesql::node::kFnOpMod);

    BinaryExprCheck<float, float, float>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kFloat,
        12.0f, 5.1f, fmod(12.0f, 5.1f), ::fesql::node::kFnOpMod);
}

TEST_F(ExprIRBuilderTest, test_mod_double_x_expr) {
    BinaryExprCheck<int16_t, double, double>(
        ::fesql::node::kInt16, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);

    BinaryExprCheck<int32_t, double, double>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);

    BinaryExprCheck<int64_t, double, double>(
        ::fesql::node::kInt64, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12L, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);

    BinaryExprCheck<float, double, double>(
        ::fesql::node::kFloat, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12.0f, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);
    BinaryExprCheck<double, double, double>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kDouble,
        12.0, 5.1, fmod(12.0, 5.1), ::fesql::node::kFnOpMod);
}

TEST_F(ExprIRBuilderTest, test_eq_expr_true) {
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 1,
        1, true, ::fesql::node::kFnOpEq);

    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 1,
        1, true, ::fesql::node::kFnOpEq);

    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 1,
        1, true, ::fesql::node::kFnOpEq);

    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.0f, 1.0f, true, ::fesql::node::kFnOpEq);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.0, 1.0, true, ::fesql::node::kFnOpEq);

    BinaryExprCheck<int32_t, float, bool>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kBool, 1,
        1.0f, true, ::fesql::node::kFnOpEq);

    BinaryExprCheck<int32_t, double, bool>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kBool, 1,
        1.0, true, ::fesql::node::kFnOpEq);
}

TEST_F(ExprIRBuilderTest, test_eq_expr_false) {
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 1,
        2, false, ::fesql::node::kFnOpEq);

    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 1,
        2, false, ::fesql::node::kFnOpEq);

    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 1,
        2, false, ::fesql::node::kFnOpEq);

    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.0f, 1.1f, false, ::fesql::node::kFnOpEq);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.0, 1.1, false, ::fesql::node::kFnOpEq);
}

TEST_F(ExprIRBuilderTest, test_neq_expr_true) {
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 1,
        2, true, ::fesql::node::kFnOpNeq);

    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 1,
        2, true, ::fesql::node::kFnOpNeq);

    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 1,
        2, true, ::fesql::node::kFnOpNeq);

    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.0f, 1.1f, true, ::fesql::node::kFnOpNeq);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.0, 1.1, true, ::fesql::node::kFnOpNeq);
}

TEST_F(ExprIRBuilderTest, test_neq_expr_false) {
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 1,
        1, false, ::fesql::node::kFnOpNeq);

    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 1,
        1, false, ::fesql::node::kFnOpNeq);

    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 1,
        1, false, ::fesql::node::kFnOpNeq);

    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.0f, 1.0f, false, ::fesql::node::kFnOpNeq);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.0, 1.0, false, ::fesql::node::kFnOpNeq);

    BinaryExprCheck<int32_t, float, bool>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kBool, 1,
        1.0f, false, ::fesql::node::kFnOpNeq);

    BinaryExprCheck<int32_t, double, bool>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kBool, 1,
        1.0, false, ::fesql::node::kFnOpNeq);
}

TEST_F(ExprIRBuilderTest, test_gt_expr_true) {
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 2,
        1, true, ::fesql::node::kFnOpGt);

    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 2,
        1, true, ::fesql::node::kFnOpGt);

    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 2,
        1, true, ::fesql::node::kFnOpGt);

    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.1f, 1.0f, true, ::fesql::node::kFnOpGt);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.1, 1.0, true, ::fesql::node::kFnOpGt);

    BinaryExprCheck<int32_t, float, bool>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kBool, 2,
        1.9f, true, ::fesql::node::kFnOpGt);

    BinaryExprCheck<int32_t, double, bool>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kBool, 2,
        1.9, true, ::fesql::node::kFnOpGt);
}

TEST_F(ExprIRBuilderTest, test_gt_expr_false) {
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 2,
        2, false, ::fesql::node::kFnOpGt);
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 2,
        3, false, ::fesql::node::kFnOpGt);

    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 2,
        2, false, ::fesql::node::kFnOpGt);
    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 2,
        3, false, ::fesql::node::kFnOpGt);

    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 2,
        2, false, ::fesql::node::kFnOpGt);
    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 2,
        3, false, ::fesql::node::kFnOpGt);

    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.1f, 1.2f, false, ::fesql::node::kFnOpGt);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.1, 1.2, false, ::fesql::node::kFnOpGt);

    BinaryExprCheck<int32_t, float, bool>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kBool, 2,
        2.1f, false, ::fesql::node::kFnOpGt);

    BinaryExprCheck<int32_t, double, bool>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kBool, 2,
        2.1, false, ::fesql::node::kFnOpGt);
}
TEST_F(ExprIRBuilderTest, test_ge_expr_true) {
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 2,
        1, true, ::fesql::node::kFnOpGe);
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 2,
        2, true, ::fesql::node::kFnOpGe);

    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 2,
        1, true, ::fesql::node::kFnOpGe);
    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 2,
        2, true, ::fesql::node::kFnOpGe);

    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 2,
        1, true, ::fesql::node::kFnOpGe);
    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 2,
        2, true, ::fesql::node::kFnOpGe);

    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.1f, 1.0f, true, ::fesql::node::kFnOpGe);
    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.1f, 1.1f, true, ::fesql::node::kFnOpGe);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.1, 1.0, true, ::fesql::node::kFnOpGe);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.1, 1.1, true, ::fesql::node::kFnOpGe);
    BinaryExprCheck<int32_t, float, bool>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kBool, 2,
        1.9f, true, ::fesql::node::kFnOpGe);
    BinaryExprCheck<int32_t, float, bool>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kBool, 2,
        2.0f, true, ::fesql::node::kFnOpGe);

    BinaryExprCheck<int32_t, double, bool>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kBool, 2,
        1.9, true, ::fesql::node::kFnOpGe);
    BinaryExprCheck<int32_t, double, bool>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kBool, 2,
        2.0, true, ::fesql::node::kFnOpGe);
}

TEST_F(ExprIRBuilderTest, test_ge_expr_false) {
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 2,
        3, false, ::fesql::node::kFnOpGe);

    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 2,
        3, false, ::fesql::node::kFnOpGe);

    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 2,
        3, false, ::fesql::node::kFnOpGe);

    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.1f, 1.2f, false, ::fesql::node::kFnOpGe);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.1, 1.2, false, ::fesql::node::kFnOpGe);

    BinaryExprCheck<int32_t, float, bool>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kBool, 2,
        2.1f, false, ::fesql::node::kFnOpGe);
}

TEST_F(ExprIRBuilderTest, test_lt_expr_true) {
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 2,
        3, true, ::fesql::node::kFnOpLt);

    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 2,
        3, true, ::fesql::node::kFnOpLt);

    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 2,
        3, true, ::fesql::node::kFnOpLt);

    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.1f, 1.2f, true, ::fesql::node::kFnOpLt);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.1, 1.2, true, ::fesql::node::kFnOpLt);

    BinaryExprCheck<int32_t, float, bool>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kBool, 2,
        2.1f, true, ::fesql::node::kFnOpLt);
}

TEST_F(ExprIRBuilderTest, test_lt_expr_false) {
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 2,
        1, false, ::fesql::node::kFnOpLt);
    BinaryExprCheck<int16_t, int16_t, bool>(
        ::fesql::node::kInt16, ::fesql::node::kInt16, ::fesql::node::kBool, 2,
        2, false, ::fesql::node::kFnOpLt);

    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 2,
        1, false, ::fesql::node::kFnOpLt);
    BinaryExprCheck<int32_t, int32_t, bool>(
        ::fesql::node::kInt32, ::fesql::node::kInt32, ::fesql::node::kBool, 2,
        2, false, ::fesql::node::kFnOpLt);

    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 2,
        1, false, ::fesql::node::kFnOpLt);
    BinaryExprCheck<int64_t, int64_t, bool>(
        ::fesql::node::kInt64, ::fesql::node::kInt64, ::fesql::node::kBool, 2,
        2, false, ::fesql::node::kFnOpLt);

    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.1f, 1.0f, false, ::fesql::node::kFnOpLt);
    BinaryExprCheck<float, float, bool>(
        ::fesql::node::kFloat, ::fesql::node::kFloat, ::fesql::node::kBool,
        1.1f, 1.1f, false, ::fesql::node::kFnOpLt);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.1, 1.0, false, ::fesql::node::kFnOpLt);

    BinaryExprCheck<double, double, bool>(
        ::fesql::node::kDouble, ::fesql::node::kDouble, ::fesql::node::kBool,
        1.1, 1.1, false, ::fesql::node::kFnOpLt);
    BinaryExprCheck<int32_t, float, bool>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kBool, 2,
        1.9f, false, ::fesql::node::kFnOpLt);
    BinaryExprCheck<int32_t, float, bool>(
        ::fesql::node::kInt32, ::fesql::node::kFloat, ::fesql::node::kBool, 2,
        2.0f, false, ::fesql::node::kFnOpLt);

    BinaryExprCheck<int32_t, double, bool>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kBool, 2,
        1.9, false, ::fesql::node::kFnOpLt);
    BinaryExprCheck<int32_t, double, bool>(
        ::fesql::node::kInt32, ::fesql::node::kDouble, ::fesql::node::kBool, 2,
        2.0, false, ::fesql::node::kFnOpLt);
}

TEST_F(ExprIRBuilderTest, test_and_expr_true) {
    BinaryExprCheck<bool, bool, bool>(
        ::fesql::node::kBool, ::fesql::node::kBool, ::fesql::node::kBool, true,
        true, true, ::fesql::node::kFnOpAnd);
}

TEST_F(ExprIRBuilderTest, test_and_expr_false) {
    BinaryExprCheck<bool, bool, bool>(
        ::fesql::node::kBool, ::fesql::node::kBool, ::fesql::node::kBool, false,
        true, false, ::fesql::node::kFnOpAnd);

    BinaryExprCheck<bool, bool, bool>(
        ::fesql::node::kBool, ::fesql::node::kBool, ::fesql::node::kBool, false,
        false, false, ::fesql::node::kFnOpAnd);
    BinaryExprCheck<bool, bool, bool>(
        ::fesql::node::kBool, ::fesql::node::kBool, ::fesql::node::kBool, true,
        false, false, ::fesql::node::kFnOpAnd);
}

TEST_F(ExprIRBuilderTest, test_or_expr_true) {
    BinaryExprCheck<bool, bool, bool>(
        ::fesql::node::kBool, ::fesql::node::kBool, ::fesql::node::kBool, true,
        true, true, ::fesql::node::kFnOpOr);
    BinaryExprCheck<bool, bool, bool>(
        ::fesql::node::kBool, ::fesql::node::kBool, ::fesql::node::kBool, true,
        false, true, ::fesql::node::kFnOpOr);
    BinaryExprCheck<bool, bool, bool>(
        ::fesql::node::kBool, ::fesql::node::kBool, ::fesql::node::kBool, false,
        true, true, ::fesql::node::kFnOpOr);
}

TEST_F(ExprIRBuilderTest, test_or_expr_false) {
    BinaryExprCheck<bool, bool, bool>(
        ::fesql::node::kBool, ::fesql::node::kBool, ::fesql::node::kBool, false,
        false, false, ::fesql::node::kFnOpOr);
}

}  // namespace codegen
}  // namespace fesql

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    return RUN_ALL_TESTS();
}
