/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * physical_pass.h
 *
 * Author: chenjing
 * Date: 2020/3/13
 *--------------------------------------------------------------------------
 **/
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/fe_object.h"
#include "base/fe_status.h"
#include "node/node_manager.h"
#include "passes/pass_base.h"
#include "udf/udf_library.h"
#include "vm/physical_op.h"

#ifndef SRC_PASSES_PHYSICAL_PHYSICAL_PASS_H_
#define SRC_PASSES_PHYSICAL_PHYSICAL_PASS_H_

namespace fesql {
namespace vm {

using fesql::base::Status;

class PhysicalPlanContext {
 public:
    PhysicalPlanContext(node::NodeManager* nm, const udf::UDFLibrary* library,
                        const std::string& db,
                        const std::shared_ptr<Catalog>& catalog)
        : nm_(nm), library_(library), db_(db), catalog_(catalog) {}
    ~PhysicalPlanContext() {}

    /**
     * Get unique column id by named column from table.
     */
    Status GetSourceID(const std::string& table_name,
                       const std::string& column_name, size_t* column_id);
    Status GetRequestSourceID(const std::string& table_name,
                              const std::string& column_name,
                              size_t* column_id);

    const auto& GetRequestColumnIDMapping() const {
        return request_column_id_to_source_id_;
    }

    /**
     * Get new unique column id computed by expression.
     */
    size_t GetNewColumnID();

    /**
     * Generate function def for expression list.
     */
    Status InitFnDef(const ColumnProjects& projects,
                     const SchemasContext* schemas_ctx, bool is_row_project,
                     FnComponent* fn_component);
    Status InitFnDef(const node::ExprListNode* projects,
                     const SchemasContext* schemas_ctx, bool is_row_project,
                     FnComponent* fn_component);

    template <typename Op, typename... Args>
    Status CreateOp(Op** result_op, Args&&... args) {
        Op* op = new Op(std::forward<Args>(args)...);
        auto status = op->InitSchema(this);
        if (!status.isOK()) {
            delete op;
            return status;
        }
        op->FinishSchema();
        *result_op = nm_->RegisterNode(op);
        return Status::OK();
    }

    node::NodeManager* node_manager() const { return nm_; }
    const udf::UDFLibrary* library() const { return library_; }
    const std::string& db() { return db_; }
    std::shared_ptr<Catalog> catalog() { return catalog_; }

    // temp dict for legacy udf
    // TODO(xxx): support udf type infer
    std::map<std::string, type::Type> legacy_udf_dict_;

 private:
    node::NodeManager* nm_;
    const udf::UDFLibrary* library_;
    const std::string db_;
    std::shared_ptr<Catalog> catalog_;

    Status InitializeSourceIdMappings(const std::string& table_name);

    // manage column unique ids
    size_t column_id_counter_ = 0;
    std::map<std::string, std::map<std::string, size_t>> table_column_id_map_;
    // TODO(xxx): pass in request name
    std::map<std::string, std::map<std::string, size_t>> request_column_id_map_;

    // request column id -> source table column id
    std::map<size_t, size_t> request_column_id_to_source_id_;

    // source column id -> column info
    std::map<size_t, std::pair<std::string, std::string>> column_id_to_name_;

    // unique id counter for codegen function name id
    size_t codegen_func_id_counter_ = 0;
};

class PhysicalPass
    : public passes::PassBase<PhysicalOpNode, PhysicalPlanContext> {
 public:
    PhysicalPass() = default;
    virtual ~PhysicalPass() {}
};

}  // namespace vm
}  // namespace fesql
#endif  // SRC_PASSES_PHYSICAL_PHYSICAL_PASS_H_
