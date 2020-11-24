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
#include <set>
#include <string>
#include <vector>
#include "base/fe_status.h"
#include "llvm/IR/Module.h"
#include "parser/parser.h"
#include "proto/fe_common.pb.h"
#include "udf/udf_library.h"
#include "vm/catalog.h"
#include "vm/jit.h"
#include "vm/runner.h"

namespace fesql {
namespace vm {

using fesql::base::Status;

enum EngineMode { kBatchMode, kRequestMode, kBatchRequestMode };

std::string EngineModeName(EngineMode mode);

struct SQLContext {
    // mode: batch|request|batch request
    EngineMode engine_mode;
    bool is_performance_sensitive = false;
    bool is_cluster_optimized = false;
    // the sql content
    std::string sql;
    // the database
    std::string db;
    // the logical plan
    ::fesql::node::PlanNodeList logical_plan;
    PhysicalOpNode* physical_plan = nullptr;
    fesql::vm::ClusterJob cluster_job;
    // TODO(wangtaize) add a light jit engine
    // eg using bthead to compile ir
    std::unique_ptr<FeSQLJIT> jit = nullptr;
    Schema schema;
    Schema request_schema;
    std::string request_name;
    uint32_t row_size;
    std::string ir;
    std::string logical_plan_str;
    std::string physical_plan_str;
    std::string encoded_schema;
    std::string encoded_request_schema;
    ::fesql::node::NodeManager nm;
    ::fesql::udf::UDFLibrary* udf_library = nullptr;

    // common column indices in batch request mode
    std::set<size_t> common_column_indices;

    SQLContext() {}
    ~SQLContext() {}
};

void InitCodecSymbol(::llvm::orc::JITDylib& jd,            // NOLINT
                     ::llvm::orc::MangleAndInterner& mi);  // NOLINT
void InitCodecSymbol(vm::FeSQLJIT* jit_ptr);

bool RegisterFeLibs(udf::UDFLibrary* lib, base::Status& status,  // NOLINT
                    const std::string& libs_home = "",
                    const std::string& libs_name = "");
bool GetLibsFiles(const std::string& dir_path,
                  std::vector<std::string>& filenames,  // NOLINT
                  base::Status& status);                // NOLINT
const std::string FindFesqlDirPath();

class SQLCompiler {
 public:
    SQLCompiler(const std::shared_ptr<Catalog>& cl, bool keep_ir = false,
                bool dump_plan = false, bool plan_only = false);

    ~SQLCompiler();

    bool Compile(SQLContext& ctx,                 // NOLINT
                 Status& status);                 // NOLINT
    bool Parse(SQLContext& ctx, Status& status);  // NOLINT
    bool BuildClusterJob(SQLContext& ctx,         // NOLINT
                         Status& status);         // NOLINT

 private:
    void KeepIR(SQLContext& ctx, llvm::Module* m);  // NOLINT

    bool ResolvePlanFnAddress(PhysicalOpNode* node,
                              std::unique_ptr<FeSQLJIT>& jit,  // NOLINT
                              Status& status);                 // NOLINT

    Status BuildPhysicalPlan(SQLContext* ctx,
                             const ::fesql::node::PlanNodeList& plan_list,
                             ::llvm::Module* llvm_module,
                             PhysicalOpNode** output);

 private:
    const std::shared_ptr<Catalog> cl_;
    bool keep_ir_;
    bool dump_plan_;
    bool plan_only_;
};
}  // namespace vm
}  // namespace fesql
#endif  // SRC_VM_SQL_COMPILER_H_
