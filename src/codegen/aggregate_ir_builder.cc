/*
 * aggregate_ir_builder.cc
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
#include "codegen/aggregate_ir_builder.h"

#include <stdlib.h>
#include <algorithm>
#include <limits>
#include <map>
#include <memory>

#include "codegen/expr_ir_builder.h"
#include "codegen/ir_base_builder.h"
#include "codegen/variable_ir_builder.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
namespace fesql {
namespace codegen {

AggregateIRBuilder::AggregateIRBuilder(const vm::SchemasContext* sc,
                                       ::llvm::Module* module,
                                       node::FrameNode* frame_node, uint32_t id)
    : schema_context_(sc), module_(module), frame_node_(frame_node), id_(id) {
    available_agg_func_set_.insert("sum");
    available_agg_func_set_.insert("avg");
    available_agg_func_set_.insert("count");
    available_agg_func_set_.insert("min");
    available_agg_func_set_.insert("max");
}

bool AggregateIRBuilder::IsAggFuncName(const std::string& fname) {
    return available_agg_func_set_.find(fname) != available_agg_func_set_.end();
}

bool AggregateIRBuilder::CollectAggColumn(const fesql::node::ExprNode* expr,
                                          size_t output_idx,
                                          fesql::type::Type* res_agg_type) {
    switch (expr->expr_type_) {
        case node::kExprCall: {
            auto call = dynamic_cast<const node::CallExprNode*>(expr);
            std::string agg_func_name = "";
            switch (call->GetFnDef()->GetType()) {
                case node::kExternalFnDef: {
                    agg_func_name =
                        dynamic_cast<const node::ExternalFnDefNode*>(
                            call->GetFnDef())
                            ->function_name();
                    break;
                }
                case node::kUDAFDef: {
                    agg_func_name =
                        dynamic_cast<const node::UDAFDefNode*>(call->GetFnDef())
                            ->GetSimpleName();
                    break;
                }
                default:
                    break;
            }
            if (!IsAggFuncName(agg_func_name)) {
                break;
            }
            if (call->GetChildNum() != 1) {
                break;
            }
            auto input_expr = call->GetChild(0);
            if (input_expr->expr_type_ != node::kExprColumnRef) {
                break;
            }
            auto col = dynamic_cast<node::ColumnRefNode*>(
                const_cast<node::ExprNode*>(input_expr));
            const std::string& rel_name = col->GetRelationName();
            const std::string& col_name = col->GetColumnName();
            const RowSchemaInfo* info;
            if (!schema_context_->ColumnRefResolved(rel_name, col_name,
                                                    &info)) {
                LOG(ERROR) << "fail to resolve column "
                           << rel_name + "." + col_name;
                return false;
            }
            size_t slice_idx = info->idx_;

            codec::RowDecoder decoder(*info->schema_);
            codec::ColInfo col_info;
            if (!decoder.ResolveColumn(col_name, &col_info)) {
                LOG(ERROR) << "fail to resolve column "
                           << rel_name + "." + col_name;
                return false;
            }
            auto col_type = col_info.type;
            uint32_t offset = col_info.offset;

            // resolve llvm agg type
            node::DataType node_type;
            if (!SchemaType2DataType(col_type, &node_type)) {
                LOG(ERROR) << "unrecognized data type "
                           << fesql::type::Type_Name(col_type);
                return false;
            }
            if (GetOutputLLVMType(module_->getContext(), agg_func_name,
                                  node_type) == nullptr) {
                return false;
            }
            if (agg_func_name == "count") {
                *res_agg_type = ::fesql::type::kInt64;
            } else if (agg_func_name == "avg") {
                *res_agg_type = ::fesql::type::kDouble;
            } else {
                *res_agg_type = col_type;
            }

            std::string col_key = rel_name + "." + col_name;
            auto iter = agg_col_infos_.find(col_key);
            if (iter == agg_col_infos_.end()) {
                agg_col_infos_[col_key] =
                    AggColumnInfo(col, node_type, slice_idx, offset);
            }
            agg_col_infos_[col_key].AddAgg(agg_func_name, output_idx);
            return true;
        }
        default:
            break;
    }
    return false;
}

class StatisticalAggGenerator {
 public:
    StatisticalAggGenerator(node::DataType col_type,
                            const std::vector<std::string>& col_keys)
        : col_type_(col_type),
          col_num_(col_keys.size()),
          col_keys_(col_keys),
          sum_idxs_(col_num_, -1),
          avg_idxs_(col_num_, -1),
          count_idxs_(col_num_, -1),
          min_idxs_(col_num_, -1),
          max_idxs_(col_num_, -1),
          sum_states_(col_num_, nullptr),
          avg_states_(col_num_, nullptr),
          min_states_(col_num_, nullptr),
          max_states_(col_num_, nullptr),
          count_state_(nullptr) {}

    ::llvm::Value* GenSumInitState(::llvm::IRBuilder<>* builder) {
        ::llvm::LLVMContext& llvm_ctx = builder->getContext();
        ::llvm::Type* llvm_ty =
            AggregateIRBuilder::GetOutputLLVMType(llvm_ctx, "sum", col_type_);
        ::llvm::Value* accum = builder->CreateAlloca(llvm_ty);
        if (llvm_ty->isIntegerTy()) {
            builder->CreateStore(::llvm::ConstantInt::get(llvm_ty, 0, true),
                                 accum);
        } else {
            builder->CreateStore(::llvm::ConstantFP::get(llvm_ty, 0.0), accum);
        }
        return accum;
    }

    ::llvm::Value* GenAvgInitState(::llvm::IRBuilder<>* builder) {
        ::llvm::LLVMContext& llvm_ctx = builder->getContext();
        ::llvm::Type* llvm_ty =
            AggregateIRBuilder::GetOutputLLVMType(llvm_ctx, "avg", col_type_);
        ::llvm::Value* accum = builder->CreateAlloca(llvm_ty);
        builder->CreateStore(::llvm::ConstantFP::get(llvm_ty, 0.0), accum);
        return accum;
    }

    ::llvm::Value* GenCountInitState(::llvm::IRBuilder<>* builder) {
        ::llvm::LLVMContext& llvm_ctx = builder->getContext();
        ::llvm::Type* int64_ty = ::llvm::Type::getInt64Ty(llvm_ctx);
        ::llvm::Value* cnt = builder->CreateAlloca(int64_ty);
        builder->CreateStore(::llvm::ConstantInt::get(int64_ty, 0, true), cnt);
        return cnt;
    }

    ::llvm::Value* GenMinInitState(::llvm::IRBuilder<>* builder) {
        ::llvm::LLVMContext& llvm_ctx = builder->getContext();
        ::llvm::Type* llvm_ty =
            AggregateIRBuilder::GetOutputLLVMType(llvm_ctx, "min", col_type_);
        ::llvm::Value* accum = builder->CreateAlloca(llvm_ty);
        ::llvm::Value* min;
        if (llvm_ty == ::llvm::Type::getInt16Ty(llvm_ctx)) {
            min = ::llvm::ConstantInt::get(
                llvm_ty, std::numeric_limits<int16_t>::max(), true);
        } else if (llvm_ty == ::llvm::Type::getInt32Ty(llvm_ctx)) {
            min = ::llvm::ConstantInt::get(
                llvm_ty, std::numeric_limits<int32_t>::max(), true);
        } else if (llvm_ty == ::llvm::Type::getInt64Ty(llvm_ctx)) {
            min = ::llvm::ConstantInt::get(
                llvm_ty, std::numeric_limits<int64_t>::max(), true);
        } else if (llvm_ty == ::llvm::Type::getFloatTy(llvm_ctx)) {
            min = ::llvm::ConstantFP::get(llvm_ty,
                                          std::numeric_limits<float>::max());
        } else {
            min = ::llvm::ConstantFP::get(llvm_ty,
                                          std::numeric_limits<double>::max());
        }
        builder->CreateStore(min, accum);
        return accum;
    }

    ::llvm::Value* GenMaxInitState(::llvm::IRBuilder<>* builder) {
        ::llvm::LLVMContext& llvm_ctx = builder->getContext();
        ::llvm::Type* llvm_ty =
            AggregateIRBuilder::GetOutputLLVMType(llvm_ctx, "max", col_type_);
        ::llvm::Value* accum = builder->CreateAlloca(llvm_ty);
        ::llvm::Value* max;
        if (llvm_ty == ::llvm::Type::getInt16Ty(llvm_ctx)) {
            max = ::llvm::ConstantInt::get(
                llvm_ty, std::numeric_limits<int16_t>::min(), true);
        } else if (llvm_ty == ::llvm::Type::getInt32Ty(llvm_ctx)) {
            max = ::llvm::ConstantInt::get(
                llvm_ty, std::numeric_limits<int32_t>::min(), true);
        } else if (llvm_ty == ::llvm::Type::getInt64Ty(llvm_ctx)) {
            max = ::llvm::ConstantInt::get(
                llvm_ty, std::numeric_limits<int64_t>::min(), true);
        } else if (llvm_ty == ::llvm::Type::getFloatTy(llvm_ctx)) {
            max = ::llvm::ConstantFP::get(llvm_ty,
                                          std::numeric_limits<float>::min());
        } else {
            max = ::llvm::ConstantFP::get(llvm_ty,
                                          std::numeric_limits<double>::min());
        }
        builder->CreateStore(max, accum);
        return accum;
    }

    void GenInitState(::llvm::IRBuilder<>* builder) {
        for (size_t i = 0; i < col_num_; ++i) {
            if (sum_idxs_[i] >= 0) {
                sum_states_[i] = GenSumInitState(builder);
            }
            if (avg_idxs_[i] >= 0) {
                if (col_type_ == ::fesql::node::kDouble) {
                    sum_states_[i] = GenSumInitState(builder);
                } else {
                    avg_states_[i] = GenAvgInitState(builder);
                }
                if (count_state_ == nullptr) {
                    count_state_ = GenCountInitState(builder);
                }
            }
            if (count_idxs_[i] >= 0) {
                if (count_state_ == nullptr) {
                    count_state_ = GenCountInitState(builder);
                }
            }
            if (min_idxs_[i] >= 0) {
                min_states_[i] = GenMinInitState(builder);
            }
            if (max_idxs_[i] >= 0) {
                max_states_[i] = GenMaxInitState(builder);
            }
        }
    }

    void GenSumUpdate(size_t i, ::llvm::Value* input,
                      ::llvm::IRBuilder<>* builder) {
        ::llvm::Value* accum = builder->CreateLoad(sum_states_[i]);
        ::llvm::Value* add;
        if (input->getType()->isIntegerTy()) {
            add = builder->CreateAdd(accum, input);
        } else {
            add = builder->CreateFAdd(accum, input);
        }
        builder->CreateStore(add, sum_states_[i]);
    }

    void GenAvgUpdate(size_t i, ::llvm::Value* input,
                      ::llvm::IRBuilder<>* builder) {
        ::llvm::Value* accum = builder->CreateLoad(avg_states_[i]);
        if (input->getType()->isIntegerTy()) {
            input = builder->CreateSIToFP(input, accum->getType());
        } else {
            input = builder->CreateFPCast(input, accum->getType());
        }
        builder->CreateStore(builder->CreateFAdd(accum, input), avg_states_[i]);
    }

    void GenCountUpdate(::llvm::IRBuilder<>* builder) {
        ::llvm::Value* one = ::llvm::ConstantInt::get(
            reinterpret_cast<::llvm::PointerType*>(count_state_->getType())
                ->getElementType(),
            1, true);
        ::llvm::Value* cnt = builder->CreateLoad(count_state_);
        cnt = builder->CreateAdd(cnt, one);
        builder->CreateStore(cnt, count_state_);
    }

    void GenMinUpdate(size_t i, ::llvm::Value* input,
                      ::llvm::IRBuilder<>* builder) {
        ::llvm::Value* accum = builder->CreateLoad(min_states_[i]);
        ::llvm::Type* min_ty = accum->getType();
        ::llvm::Value* cmp;
        if (min_ty->isIntegerTy()) {
            cmp = builder->CreateICmpSLT(accum, input);
        } else {
            cmp = builder->CreateFCmpOLT(accum, input);
        }
        ::llvm::Value* min = builder->CreateSelect(cmp, accum, input);
        builder->CreateStore(min, min_states_[i]);
    }

    void GenMaxUpdate(size_t i, ::llvm::Value* input,
                      ::llvm::IRBuilder<>* builder) {
        ::llvm::Value* accum = builder->CreateLoad(max_states_[i]);
        ::llvm::Type* max_ty = accum->getType();
        ::llvm::Value* cmp;
        if (max_ty->isIntegerTy()) {
            cmp = builder->CreateICmpSLT(accum, input);
        } else {
            cmp = builder->CreateFCmpOLT(accum, input);
        }
        ::llvm::Value* max = builder->CreateSelect(cmp, input, accum);
        builder->CreateStore(max, max_states_[i]);
    }

    void GenUpdate(::llvm::IRBuilder<>* builder,
                   const std::vector<::llvm::Value*>& inputs) {
        bool count_updated = false;
        for (size_t i = 0; i < col_num_; ++i) {
            if (sum_idxs_[i] >= 0 ||
                (avg_idxs_[i] >= 0 && avg_states_[i] == nullptr)) {
                GenSumUpdate(i, inputs[i], builder);
            }
            if (avg_idxs_[i] >= 0 && avg_states_[i] != nullptr) {
                GenAvgUpdate(i, inputs[i], builder);
            }
            if ((avg_idxs_[i] >= 0 || count_idxs_[i] >= 0) && !count_updated) {
                GenCountUpdate(builder);
                count_updated = true;
            }
            if (min_idxs_[i] >= 0) {
                GenMinUpdate(i, inputs[i], builder);
            }
            if (max_idxs_[i] >= 0) {
                GenMaxUpdate(i, inputs[i], builder);
            }
        }
    }

    void GenOutputs(::llvm::IRBuilder<>* builder,
                    std::vector<std::pair<size_t, llvm::Value*>>* outputs) {
        for (size_t i = 0; i < col_num_; ++i) {
            if (sum_idxs_[i] >= 0) {
                ::llvm::Value* accum = builder->CreateLoad(sum_states_[i]);
                outputs->emplace_back(std::make_pair(sum_idxs_[i], accum));
            }
            if (avg_idxs_[i] >= 0) {
                ::llvm::Type* avg_ty = AggregateIRBuilder::GetOutputLLVMType(
                    builder->getContext(), "avg", col_type_);
                ::llvm::Value* sum;
                if (avg_states_[i] == nullptr) {
                    sum = builder->CreateLoad(sum_states_[i]);
                } else {
                    sum = builder->CreateLoad(avg_states_[i]);
                }
                ::llvm::Value* cnt = builder->CreateLoad(count_state_);
                cnt = builder->CreateSIToFP(cnt, avg_ty);
                ::llvm::Value* avg = builder->CreateFDiv(sum, cnt);
                outputs->emplace_back(std::make_pair(avg_idxs_[i], avg));
            }
            if (count_idxs_[i] >= 0) {
                ::llvm::Value* cnt = builder->CreateLoad(count_state_);
                outputs->emplace_back(std::make_pair(count_idxs_[i], cnt));
            }
            if (min_idxs_[i] >= 0) {
                ::llvm::Value* accum = builder->CreateLoad(min_states_[i]);
                outputs->emplace_back(std::make_pair(min_idxs_[i], accum));
            }
            if (max_idxs_[i] >= 0) {
                ::llvm::Value* accum = builder->CreateLoad(max_states_[i]);
                outputs->emplace_back(std::make_pair(max_idxs_[i], accum));
            }
        }
    }

    void RegisterSum(size_t pos, size_t out_idx) { sum_idxs_[pos] = out_idx; }

    void RegisterAvg(size_t pos, size_t out_idx) { avg_idxs_[pos] = out_idx; }

    void RegisterCount(size_t pos, size_t out_idx) {
        count_idxs_[pos] = out_idx;
    }

    void RegisterMin(size_t pos, size_t out_idx) { min_idxs_[pos] = out_idx; }

    void RegisterMax(size_t pos, size_t out_idx) { max_idxs_[pos] = out_idx; }

    const std::vector<std::string>& GetColKeys() const { return col_keys_; }

 private:
    node::DataType col_type_;
    size_t col_num_;
    std::vector<std::string> col_keys_;

    std::vector<int> sum_idxs_;
    std::vector<int> avg_idxs_;
    std::vector<int> count_idxs_;
    std::vector<int> min_idxs_;
    std::vector<int> max_idxs_;

    // accumulation states
    std::vector<::llvm::Value*> sum_states_;
    std::vector<::llvm::Value*> avg_states_;
    std::vector<::llvm::Value*> min_states_;
    std::vector<::llvm::Value*> max_states_;
    ::llvm::Value* count_state_;
};

llvm::Type* AggregateIRBuilder::GetOutputLLVMType(
    ::llvm::LLVMContext& llvm_ctx, const std::string& fname,
    const node::DataType& node_type) {
    ::llvm::Type* llvm_ty = nullptr;
    switch (node_type) {
        case ::fesql::node::kInt16:
            llvm_ty = ::llvm::Type::getInt16Ty(llvm_ctx);
            break;
        case ::fesql::node::kInt32:
            llvm_ty = ::llvm::Type::getInt32Ty(llvm_ctx);
            break;
        case ::fesql::node::kInt64:
            llvm_ty = ::llvm::Type::getInt64Ty(llvm_ctx);
            break;
        case ::fesql::node::kFloat:
            llvm_ty = ::llvm::Type::getFloatTy(llvm_ctx);
            break;
        case ::fesql::node::kDouble:
            llvm_ty = ::llvm::Type::getDoubleTy(llvm_ctx);
            break;
        default: {
            LOG(ERROR) << "Unknown data type: " << DataTypeName(node_type);
            return nullptr;
        }
    }
    if (fname == "count") {
        return ::llvm::Type::getInt64Ty(llvm_ctx);
    } else if (fname == "avg") {
        return ::llvm::Type::getDoubleTy(llvm_ctx);
    }
    return llvm_ty;
}

size_t GetTypeByteSize(node::DataType dtype) {
    switch (dtype) {
        case ::fesql::node::kInt16:
            return 2;
        case ::fesql::node::kInt32:
            return 4;
        case ::fesql::node::kInt64:
            return 8;
        case ::fesql::node::kFloat:
            return 4;
        case ::fesql::node::kDouble:
            return 8;
        default:
            return 0;
    }
}

bool ScheduleAggGenerators(
    std::unordered_map<std::string, AggColumnInfo>& agg_col_infos,  // NOLINT
    std::vector<StatisticalAggGenerator>* res) {
    // collect and sort used input columns
    std::vector<std::string> col_keys;
    for (auto& pair : agg_col_infos) {
        col_keys.emplace_back(pair.first);
    }
    std::sort(col_keys.begin(), col_keys.end(),
              [&agg_col_infos](const std::string& l, const std::string& r) {
                  return agg_col_infos[l].offset < agg_col_infos[r].offset;
              });

    // schedule agg op generators
    // try best to find contiguous input cols of same type
    size_t idx = 0;
    int64_t cur_offset = -1;
    node::DataType cur_col_type = node::kNull;
    std::vector<std::string> agg_gen_col_seq;

    while (!agg_gen_col_seq.empty() || idx < col_keys.size()) {
        AggColumnInfo* info = nullptr;

        bool finish_seq = false;
        if (idx >= col_keys.size()) {
            finish_seq = true;
        } else {
            info = &agg_col_infos[col_keys[idx]];
            if (agg_gen_col_seq.size() >= 4) {
                finish_seq = true;
            } else if (cur_offset >= 0) {
                size_t bytes = GetTypeByteSize(cur_col_type);
                if (info->col_type != cur_col_type ||
                    info->offset - cur_offset != bytes) {
                    finish_seq = true;
                }
            }
        }

        if (finish_seq) {
            // create generator from contiguous seq
            StatisticalAggGenerator agg_gen(cur_col_type, agg_gen_col_seq);
            for (size_t i = 0; i < agg_gen_col_seq.size(); ++i) {
                auto& geninfo = agg_col_infos[agg_gen_col_seq[i]];
                geninfo.Show();
                for (size_t j = 0; j < geninfo.GetOutputNum(); j++) {
                    auto& fname = geninfo.agg_funcs[j];
                    size_t out_idx = geninfo.output_idxs[j];

                    if (fname == "sum") {
                        agg_gen.RegisterSum(i, out_idx);
                    } else if (fname == "min") {
                        agg_gen.RegisterMin(i, out_idx);
                    } else if (fname == "max") {
                        agg_gen.RegisterMax(i, out_idx);
                    } else if (fname == "count") {
                        agg_gen.RegisterCount(i, out_idx);
                    } else if (fname == "avg") {
                        agg_gen.RegisterAvg(i, out_idx);
                    } else {
                        LOG(WARNING) << "Unknown agg function name " << fname;
                        return false;
                    }
                }
            }
            res->emplace_back(agg_gen);
            agg_gen_col_seq.clear();
        }

        if (info != nullptr) {
            agg_gen_col_seq.emplace_back(col_keys[idx]);
            cur_offset = info->offset;
            cur_col_type = info->col_type;
        }
        idx += 1;
    }
    return true;
}

bool AggregateIRBuilder::BuildMulti(const std::string& base_funcname,
                                    ExprIRBuilder* expr_ir_builder,
                                    VariableIRBuilder* variable_ir_builder,
                                    ::llvm::BasicBlock* cur_block,
                                    const std::string& output_ptr_name,
                                    vm::Schema* output_schema) {
    ::llvm::LLVMContext& llvm_ctx = module_->getContext();
    ::llvm::IRBuilder<> builder(llvm_ctx);
    auto void_ty = llvm::Type::getVoidTy(llvm_ctx);
    auto int64_ty = llvm::Type::getInt64Ty(llvm_ctx);
    expr_ir_builder->set_frame(frame_node_);
    base::Status status;
    NativeValue window_ptr;
    bool ok = expr_ir_builder->BuildWindow(&window_ptr, status);
    if (!ok || window_ptr.GetRaw() == nullptr) {
        LOG(ERROR) << "fail to find window_ptr: " + status.msg;
        return false;
    }
    NativeValue output_buf_wrapper;
    ok = variable_ir_builder->LoadValue(output_ptr_name, &output_buf_wrapper,
                                        status);
    if (!ok) {
        LOG(ERROR) << "fail to get output row ptr";
        return false;
    }
    ::llvm::Value* output_buf = output_buf_wrapper.GetValue(&builder);

    std::vector<codec::RowDecoder> decoders;
    for (auto& info : schema_context_->row_schema_info_list_) {
        decoders.push_back(codec::RowDecoder(*info.schema_));
    }

    std::string fn_name =
        base_funcname + "_multi_column_agg_" + std::to_string(id_) + "__";
    auto ptr_ty = llvm::Type::getInt8Ty(llvm_ctx)->getPointerTo();
    ::llvm::FunctionType* fnt = ::llvm::FunctionType::get(
        llvm::Type::getVoidTy(llvm_ctx), {ptr_ty, ptr_ty}, false);
    ::llvm::Function* fn = ::llvm::Function::Create(
        fnt, llvm::Function::ExternalLinkage, fn_name, module_);
    builder.SetInsertPoint(cur_block);
    builder.CreateCall(
        module_->getOrInsertFunction(fn_name, fnt),
        {window_ptr.GetValue(&builder), builder.CreateLoad(output_buf)});

    ::llvm::BasicBlock* head_block =
        ::llvm::BasicBlock::Create(llvm_ctx, "head", fn);
    ::llvm::BasicBlock* enter_block =
        ::llvm::BasicBlock::Create(llvm_ctx, "enter_iter", fn);
    ::llvm::BasicBlock* body_block =
        ::llvm::BasicBlock::Create(llvm_ctx, "iter_body", fn);
    ::llvm::BasicBlock* exit_block =
        ::llvm::BasicBlock::Create(llvm_ctx, "exit_iter", fn);

    std::vector<StatisticalAggGenerator> generators;
    if (!ScheduleAggGenerators(agg_col_infos_, &generators)) {
        LOG(WARNING) << "Schedule agg ops failed";
        return false;
    }

    // gen head
    builder.SetInsertPoint(head_block);
    for (auto& agg_generator : generators) {
        agg_generator.GenInitState(&builder);
    }

    ::llvm::Value* input_arg = fn->arg_begin();
    ::llvm::Value* output_arg = fn->arg_begin() + 1;

    // on stack unique pointer
    size_t iter_bytes = sizeof(std::unique_ptr<codec::RowIterator>);
    ::llvm::Value* iter_ptr = builder.CreateAlloca(
        ::llvm::Type::getInt8Ty(llvm_ctx),
        ::llvm::ConstantInt::get(int64_ty, iter_bytes, true));
    auto get_iter_func = module_->getOrInsertFunction(
        "fesql_storage_get_row_iter", void_ty, ptr_ty, ptr_ty);
    builder.CreateCall(get_iter_func, {input_arg, iter_ptr});
    builder.CreateBr(enter_block);

    // gen iter begin
    builder.SetInsertPoint(enter_block);
    auto bool_ty = llvm::Type::getInt1Ty(llvm_ctx);
    auto has_next_func = module_->getOrInsertFunction(
        "fesql_storage_row_iter_has_next",
        ::llvm::FunctionType::get(bool_ty, {ptr_ty}, false));
    ::llvm::Value* has_next = builder.CreateCall(has_next_func, iter_ptr);
    builder.CreateCondBr(has_next, body_block, exit_block);

    // gen iter body
    builder.SetInsertPoint(body_block);
    auto get_slice_func = module_->getOrInsertFunction(
        "fesql_storage_row_iter_get_cur_slice",
        ::llvm::FunctionType::get(ptr_ty, {ptr_ty, int64_ty}, false));
    auto get_slice_size_func = module_->getOrInsertFunction(
        "fesql_storage_row_iter_get_cur_slice_size",
        ::llvm::FunctionType::get(int64_ty, {ptr_ty, int64_ty}, false));
    std::unordered_map<size_t, std::pair<::llvm::Value*, ::llvm::Value*>>
        used_slices;

    // compute current row's slices
    for (auto& pair : agg_col_infos_) {
        size_t slice_idx = pair.second.slice_idx;
        auto iter = used_slices.find(slice_idx);
        if (iter == used_slices.end()) {
            ::llvm::Value* idx_value =
                llvm::ConstantInt::get(int64_ty, slice_idx, true);
            ::llvm::Value* buf_ptr =
                builder.CreateCall(get_slice_func, {iter_ptr, idx_value});
            ::llvm::Value* buf_size =
                builder.CreateCall(get_slice_size_func, {iter_ptr, idx_value});
            used_slices[slice_idx] = {buf_ptr, buf_size};
        }
    }

    // compute row field fetches
    std::unordered_map<std::string, ::llvm::Value*> cur_row_fields_dict;
    for (auto& pair : agg_col_infos_) {
        auto& info = pair.second;
        std::string col_key = info.GetColKey();
        if (cur_row_fields_dict.find(col_key) == cur_row_fields_dict.end()) {
            size_t slice_idx = info.slice_idx;
            auto& slice_info = used_slices[slice_idx];

            ScopeVar dummy_scope_var;
            BufNativeIRBuilder buf_builder(
                *schema_context_->row_schema_info_list_[slice_idx].schema_,
                body_block, &dummy_scope_var);
            NativeValue field_value;
            if (!buf_builder.BuildGetField(info.col->GetColumnName(),
                                           slice_info.first, slice_info.second,
                                           &field_value)) {
                LOG(ERROR) << "fail to gen fetch column";
                return false;
            }
            cur_row_fields_dict[col_key] = field_value.GetValue(&builder);
        }
    }

    // compute accumulation
    for (auto& agg_generator : generators) {
        std::vector<::llvm::Value*> fields;
        for (auto& key : agg_generator.GetColKeys()) {
            auto iter = cur_row_fields_dict.find(key);
            if (iter == cur_row_fields_dict.end()) {
                LOG(WARNING) << "Fail to find row field of " << key;
                return false;
            }
            fields.push_back(iter->second);
        }
        agg_generator.GenUpdate(&builder, fields);
    }
    auto next_func = module_->getOrInsertFunction(
        "fesql_storage_row_iter_next",
        ::llvm::FunctionType::get(void_ty, {ptr_ty}, false));
    builder.CreateCall(next_func, {iter_ptr});
    builder.CreateBr(enter_block);

    // gen iter end
    builder.SetInsertPoint(exit_block);
    auto delete_iter_func = module_->getOrInsertFunction(
        "fesql_storage_row_iter_delete",
        ::llvm::FunctionType::get(void_ty, {ptr_ty}, false));
    builder.CreateCall(delete_iter_func, {iter_ptr});

    // store results to output row
    std::map<uint32_t, NativeValue> dummy_map;
    BufNativeEncoderIRBuilder output_encoder(&dummy_map, *output_schema,
                                             exit_block);
    for (auto& agg_generator : generators) {
        std::vector<std::pair<size_t, ::llvm::Value*>> outputs;
        agg_generator.GenOutputs(&builder, &outputs);
        for (auto pair : outputs) {
            output_encoder.BuildEncodePrimaryField(
                output_arg, pair.first, NativeValue::Create(pair.second));
        }
    }
    builder.CreateRetVoid();
    return true;
}

}  // namespace codegen
}  // namespace fesql
