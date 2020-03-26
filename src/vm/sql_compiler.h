/*
 * sql_compiler.h
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

#ifndef SRC_VM_SQL_COMPILER_H_
#define SRC_VM_SQL_COMPILER_H_

#include <memory>
#include <string>
#include <vector>
#include "base/status.h"
#include "llvm/IR/Module.h"
#include "parser/parser.h"
#include "proto/common.pb.h"
#include "vm/catalog.h"
#include "vm/jit.h"
#include "vm/op_generator.h"

namespace fesql {
namespace vm {

using fesql::base::Status;

struct SQLContext {
    // mode: batch|request
    node::PlanModeType mode = node::kPlanModeBatch;
    // the sql content
    std::string sql;
    // the database
    std::string db;
    // the operators
    OpVector ops;
    // the physical plan
    PhysicalOpNode* plan;
    int8_t* runner;
    // TODO(wangtaize) add a light jit engine
    // eg using bthead to compile ir
    std::unique_ptr<FeSQLJIT> jit;
    Schema schema;
    uint32_t row_size;
    std::string ir;
};

class SQLCompiler {
 public:
    SQLCompiler(const std::shared_ptr<Catalog>& cl,
                         ::fesql::node::NodeManager* nm,
                         bool keep_ir = false);

    ~SQLCompiler();

    bool Compile(SQLContext& ctx,  // NOLINT
                 Status& status);  // NOLINT

 private:
    void KeepIR(SQLContext& ctx, llvm::Module* m);                     // NOLINT
    bool Parse(SQLContext& ctx, ::fesql::node::NodeManager& node_mgr,  // NOLINT
               ::fesql::node::PlanNodeList& trees, Status& status);    // NOLINT
    bool ResolvePlanFnAddress(PhysicalOpNode* node,
                                 std::unique_ptr<FeSQLJIT>& jit,
                                 Status& status);  // NOLINT

 private:
    const std::shared_ptr<Catalog> cl_;
    ::fesql::node::NodeManager* nm_;
    bool keep_ir_;
};
}  // namespace vm
}  // namespace fesql
#endif  // SRC_VM_SQL_COMPILER_H_
