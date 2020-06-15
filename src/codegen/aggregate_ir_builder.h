/*
 * aggregate_ir_builder.h
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

#ifndef SRC_CODEGEN_AGGREGATE_IR_BUILDER_H_
#define SRC_CODEGEN_AGGREGATE_IR_BUILDER_H_
#include <unordered_map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "codegen/expr_ir_builder.h"
#include "codegen/variable_ir_builder.h"
#include "llvm/IR/IRBuilder.h"
#include "node/plan_node.h"
#include "proto/fe_type.pb.h"
#include "vm/catalog.h"
#include "vm/schemas_context.h"

namespace fesql {
namespace codegen {


struct AggColumnInfo {
    ::fesql::node::ColumnRefNode* col;
    node::DataType col_type;
    size_t slice_idx;
    size_t offset;

    std::vector<std::string> agg_funcs;
    std::vector<size_t> output_idxs;

    AggColumnInfo(): col(nullptr) {}

    AggColumnInfo(::fesql::node::ColumnRefNode* col,
                  const node::DataType& col_type,
                  size_t slice_idx,
                  size_t offset):
        col(col), col_type(col_type),
        slice_idx(slice_idx), offset(offset) {}

    const std::string GetColKey() const {
        return col->GetRelationName() + "." + col->GetColumnName();
    }

    void AddAgg(const std::string& fname,
                size_t output_idx) {
        agg_funcs.emplace_back(fname);
        output_idxs.emplace_back(output_idx);
    }

    size_t GetOutputNum() const {
        return output_idxs.size();
    }

    void Show() {
        std::stringstream ss;
        ss << DataTypeName(col_type) << " " << GetColKey() << ": [";
        for (size_t i = 0; i < GetOutputNum(); ++i) {
            ss << agg_funcs[i] << " -> " << output_idxs[i];
            if (i != GetOutputNum() - 1) {
                ss << ", ";
            }
        }
        ss << "]\n";
        LOG(INFO) << ss.str();
    }
};


class AggregateIRBuilder {
 public:
    AggregateIRBuilder(const vm::SchemasContext*,
                       ::llvm::Module* module);

    // TODO(someone): remove temporary implementations for row-wise agg
    static bool EnableColumnAggOpt();

    bool CollectAggColumn(const node::ExprNode* expr,
                          size_t output_idx,
                          ::fesql::type::Type* col_type);

    bool IsAggFuncName(const std::string& fname);

    static llvm::Type* GetOutputLLVMType(
        ::llvm::LLVMContext& llvm_ctx,  // NOLINT
        const std::string& fname,
        const node::DataType& node_type);

    bool BuildMulti(
      const std::string& base_funcname,
      ExprIRBuilder* expr_ir_builder,
      VariableIRBuilder* variable_ir_builder,
      ::llvm::BasicBlock* cur_block,
      const std::string& output_ptr_name,
      vm::Schema* output_schema);

    bool empty() const { return agg_col_infos_.empty();}

 private:
    const vm::SchemasContext* schema_context_;
    ::llvm::Module* module_;

    std::set<std::string> available_agg_func_set_;
    std::unordered_map<std::string, AggColumnInfo> agg_col_infos_;
};

}  // namespace codegen
}  // namespace fesql
#endif  // SRC_CODEGEN_AGGREGATE_IR_BUILDER_H_
