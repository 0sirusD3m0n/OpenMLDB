/*
 * fn_ir_builder.h
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

#ifndef SRC_CODEGEN_FN_IR_BUILDER_H_
#define SRC_CODEGEN_FN_IR_BUILDER_H_

#include <vector>
#include "base/fe_status.h"
#include "codegen/scope_var.h"
#include "llvm/IR/Module.h"
#include "node/sql_node.h"

namespace fesql {
namespace codegen {

// FnIRBuilder
class FnIRBuilder {
 public:
    // TODO(wangtaize) provide a module manager
    explicit FnIRBuilder(::llvm::Module* module);
    ~FnIRBuilder();
    bool Build(const ::fesql::node::FnNodeFnDef* node,
               ::llvm::Function** result,
               base::Status& status);  // NOLINT

    bool CreateFunction(const ::fesql::node::FnNodeFnHeander* fn_def,
                        ::llvm::Function** fn, base::Status& status);  // NOLINT
    bool BuildFnHeadWithRetStruct(const ::fesql::node::FnNodeFnHeander* fn_def,
                                  ScopeVar* sv, ::llvm::Function** fn,
                                  base::Status& status);  // NOLINT
    bool BuildFnHead(const ::fesql::node::FnNodeFnHeander* fn_def, ScopeVar* sv,
                     ::llvm::Function** fn, base::Status& status);  // NOLINT

 private:
    bool BuildParas(const ::fesql::node::FnNodeList* node,
                    std::vector<::llvm::Type*>& paras,  // NOLINT
                    base::Status& status);              // NOLINT

    bool FillArgs(const ::fesql::node::FnNodeList* node, ScopeVar* sv,
                  ::llvm::Function* fn,
                  base::Status& status);  // NOLINT
    ::llvm::Module* module_;
};

}  // namespace codegen
}  // namespace fesql
#endif  // SRC_CODEGEN_FN_IR_BUILDER_H_
