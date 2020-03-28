/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * transform.cc
 *
 * Author: chenjing
 * Date: 2020/3/13
 *--------------------------------------------------------------------------
 **/
#include "vm/transform.h"
#include <set>
#include <stack>
#include <unordered_map>
#include "codegen/fn_ir_builder.h"
#include "codegen/fn_let_ir_builder.h"
#include "vm/physical_op.h"

namespace fesql {
namespace vm {

std::ostream& operator<<(std::ostream& output,
                         const fesql::vm::LogicalOp& thiz) {
    return output << *(thiz.node_);
}
bool TransformLogicalTreeToLogicalGraph(
    const ::fesql::node::PlanNode* node, LogicalGraph* graph_ptr,
    fesql::base::Status& status) {  // NOLINT

    if (nullptr == node || nullptr == graph_ptr) {
        status.msg = "node or graph_ptr is null";
        status.code = common::kOpGenError;
        LOG(WARNING) << status.msg;
        return false;
    }
    auto& graph = *graph_ptr;
    std::stack<LogicalOp> stacks;
    LogicalOp op(node);
    graph.AddVertex(op);
    stacks.push(op);
    while (!stacks.empty()) {
        auto source = stacks.top();
        stacks.pop();
        auto& children = source.node_->GetChildren();
        if (!children.empty()) {
            for (auto iter = children.cbegin(); iter != children.cend();
                 iter++) {
                LogicalOp target(*iter);
                if (!graph.IsExist(target)) {
                    stacks.push(target);
                }
                graph.AddEdge(source, target);
            }
        }
    }
    return true;
}

BatchModeTransformer::BatchModeTransformer(
    node::NodeManager* node_manager, const std::string& db,
    const std::shared_ptr<Catalog>& catalog, ::llvm::Module* module)
    : node_manager_(node_manager),
      db_(db),
      catalog_(catalog),
      module_(module),
      id_(0) {}

BatchModeTransformer::~BatchModeTransformer() {}

bool BatchModeTransformer::TransformPlanOp(const ::fesql::node::PlanNode* node,
                                           ::fesql::vm::PhysicalOpNode** ouput,
                                           ::fesql::base::Status& status) {
    if (nullptr == node || nullptr == ouput) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    LogicalOp logical_op = LogicalOp(node);
    auto map_iter = op_map_.find(logical_op);
    // logical plan node already exist
    if (map_iter != op_map_.cend()) {
        *ouput = map_iter->second;
        return true;
    }

    ::fesql::vm::PhysicalOpNode* op = nullptr;
    bool ok = true;
    switch (node->type_) {
        case node::kPlanTypeLimit:
            ok = TransformLimitOp(
                dynamic_cast<const ::fesql::node::LimitPlanNode*>(node), &op,
                status);
            break;
        case node::kPlanTypeProject:
            ok = TransformProjecPlantOp(
                dynamic_cast<const ::fesql::node::ProjectPlanNode*>(node), &op,
                status);
            break;
        case node::kPlanTypeJoin:
            ok = TransformJoinOp(
                dynamic_cast<const ::fesql::node::JoinPlanNode*>(node), &op,
                status);
            break;
        case node::kPlanTypeUnion:
            ok = TransformUnionOp(
                dynamic_cast<const ::fesql::node::UnionPlanNode*>(node), &op,
                status);
            break;
        case node::kPlanTypeGroup:
            ok = TransformGroupOp(
                dynamic_cast<const ::fesql::node::GroupPlanNode*>(node), &op,
                status);
            break;
        case node::kPlanTypeSort:
            ok = TransformSortOp(
                dynamic_cast<const ::fesql::node::SortPlanNode*>(node), &op,
                status);
            break;
        case node::kPlanTypeFilter:
            ok = TransformFilterOp(
                dynamic_cast<const ::fesql::node::FilterPlanNode*>(node), &op,
                status);
            break;
        case node::kPlanTypeTable:
            ok = TransformScanOp(
                dynamic_cast<const ::fesql::node::TablePlanNode*>(node), &op,
                status);
            break;
        case node::kPlanTypeQuery:
            ok = TransformQueryPlan(
                dynamic_cast<const ::fesql::node::QueryPlanNode*>(node), &op,
                status);
            break;
        case node::kPlanTypeRename:
            ok = TransformRenameOp(
                dynamic_cast<const ::fesql::node::RenamePlanNode*>(node), &op,
                status);
            break;
        case node::kPlanTypeDistinct:
            ok = TransformDistinctOp(
                dynamic_cast<const ::fesql::node::DistinctPlanNode*>(node), &op,
                status);
            break;
        default: {
            status.msg = "fail to transform physical plan: can't handle type " +
                         node::NameOfPlanNodeType(node->type_);
            status.code = common::kPlanError;
            LOG(WARNING) << status.msg;
            return false;
        }
    }
    if (!ok) {
        return false;
    }
    op_map_[logical_op] = op;
    *ouput = op;
    return true;
}

bool BatchModeTransformer::GenPlanNode(PhysicalOpNode* node,
                                       base::Status& status) {
    if (nullptr == node) {
        status.msg = "input node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }

    for (auto producer : node->GetProducers()) {
        if (!GenPlanNode(producer, status)) {
            return false;
        }
    }

    if (!node->GetFnName().empty()) {
        DLOG(INFO) << "already gen code for node";
        return true;
    }
    std::string fn_name = "";
    vm::Schema fn_schema;
    switch (node->type_) {
        case kPhysicalOpGroupBy: {
            auto group = dynamic_cast<PhysicalGroupNode*>(node)->groups_;
            CodeGenExprList(node->output_schema, group, true, fn_name,
                                 fn_schema, status);
            break;
        }
        case kPhysicalOpSortBy: {
            auto order_op = dynamic_cast<PhysicalSortNode*>(node);
            if (nullptr != order_op->order_) {
                CodeGenExprList(node->output_schema,
                                order_op->order_->order_by_, true, fn_name,
                                fn_schema, status);
            }
            break;
        }
        case kPhysicalOpGroupAndSort: {
            auto group_sort_op = dynamic_cast<PhysicalGroupAndSortNode*>(node);
            node::ExprListNode expr_list;

            if (!node::ExprListNullOrEmpty(group_sort_op->groups_)) {
                for (auto expr : group_sort_op->groups_->children_) {
                    expr_list.AddChild(expr);
                }
            }
            if (nullptr != group_sort_op->orders_ &&
                !node::ExprListNullOrEmpty(group_sort_op->orders_->order_by_)) {
                for (auto expr : group_sort_op->orders_->order_by_->children_) {
                    expr_list.AddChild(expr);
                }
            }
            CodeGenExprList(node->output_schema, &expr_list, true, fn_name,
                            fn_schema, status);
            break;
        }
        case kPhysicalOpFilter: {
            auto filter_op = dynamic_cast<PhysicalFliterNode*>(node);
            node::ExprListNode expr_list;
            expr_list.AddChild(
                const_cast<node::ExprNode*>(filter_op->condition_));
            CodeGenExprList(node->output_schema, &expr_list, true, fn_name,
                            fn_schema, status);
            break;
        }
        case kPhysicalOpJoin: {
            auto join_op = dynamic_cast<PhysicalJoinNode*>(node);
            node::ExprListNode expr_list;
            expr_list.AddChild(
                const_cast<node::ExprNode*>(join_op->condition_));
            CodeGenExprList(node->output_schema, &expr_list, true, fn_name,
                            fn_schema, status);
            break;
        }
        case kPhysicalOpRequestUnoin: {
            auto request_union = dynamic_cast<PhysicalRequestUnionNode*>(node);
            node::ExprListNode expr_list;
            if (!node::ExprListNullOrEmpty(request_union->groups_)) {
                for (auto expr : request_union->groups_->children_) {
                    expr_list.AddChild(expr);
                }
            }
            if (nullptr != request_union->orders_ &&
                !node::ExprListNullOrEmpty(request_union->orders_->order_by_)) {
                for (auto expr : request_union->orders_->order_by_->children_) {
                    expr_list.AddChild(expr);
                }
            }
            CodeGenExprList(node->output_schema, &expr_list, true, fn_name,
                            fn_schema, status);
            break;
        }
        default: {
            return true;
        }
    }
    node->SetFnSchema(fn_schema);
    node->SetFnName(fn_name);
    return true;
}
bool BatchModeTransformer::TransformLimitOp(const node::LimitPlanNode* node,
                                            PhysicalOpNode** output,
                                            base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    PhysicalOpNode* depend = nullptr;
    if (!TransformPlanOp(node->GetChildren()[0], &depend, status)) {
        return false;
    }
    *output = new PhysicalLimitNode(depend, node->limit_cnt_);
    node_manager_->RegisterNode(*output);
    return true;
}

bool BatchModeTransformer::TransformProjecPlantOp(
    const node::ProjectPlanNode* node, PhysicalOpNode** output,
    base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    PhysicalOpNode* depend = nullptr;
    if (!TransformPlanOp(node->GetChildren()[0], &depend, status)) {
        return false;
    }

    if (node->project_list_vec_.empty()) {
        status.msg = "fail transform project op: empty projects";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }

    if (1 == node->project_list_vec_.size()) {
        return TransformProjectOp(dynamic_cast<fesql::node::ProjectListNode*>(
                                      node->project_list_vec_[0]),
                                  depend, output, status);
    }

    std::vector<PhysicalOpNode*> ops;
    for (auto iter = node->project_list_vec_.rbegin();
         iter != node->project_list_vec_.rend(); ++iter) {
        fesql::node::ProjectListNode* project_list =
            dynamic_cast<fesql::node::ProjectListNode*>(*iter);

        // append oringinal table column after project columns
        // if there is multi
        if (node->project_list_vec_.size() > 1) {
            project_list->AddProject(node_manager_->MakeRowProjectNode(
                project_list->GetProjects().size(), "*",
                node_manager_->MakeAllNode("")));
        }

        PhysicalOpNode* project_op = nullptr;
        if (!TransformProjectOp(project_list, depend, &project_op, status)) {
            return false;
        }
        depend = project_op;
    }

    auto project_list = node_manager_->MakeProjectListPlanNode(nullptr, false);
    uint32_t pos = 0;
    for (auto iter = node->pos_mapping_.cbegin();
         iter != node->pos_mapping_.cend(); iter++) {
        auto sub_project_list = dynamic_cast<node::ProjectListNode*>(
            node->project_list_vec_[iter->first]);

        auto project_node = dynamic_cast<node::ProjectNode*>(
            sub_project_list->GetProjects().at(iter->second));
        if (node::kExprAll == project_node->GetExpression()->expr_type_) {
            auto all_expr =
                dynamic_cast<node::AllNode*>(project_node->GetExpression());
            if (all_expr->children_.empty()) {
                // expand all expression if needed
                for (auto column : depend->output_schema) {
                    all_expr->children_.push_back(
                        node_manager_->MakeColumnRefNode(
                            column.name(), all_expr->GetRelationName()));
                }
            }
            project_list->AddProject(
                node_manager_->MakeRowProjectNode(pos, "*", all_expr));
        } else {
            project_list->AddProject(node_manager_->MakeRowProjectNode(
                pos, project_node->GetName(),
                node_manager_->MakeColumnRefNode(project_node->GetName(), "")));
        }
        pos++;
    }

    return CreatePhysicalProjectNode(kTableProject, depend, project_list,
                                     output, status);
}
bool BatchModeTransformer::TransformGroupAndSortOp(
    PhysicalOpNode* depend, const node::ExprListNode* groups,
    const node::OrderByNode* orders, PhysicalOpNode** output,
    base::Status& status) {
    if (nullptr == depend || nullptr == output) {
        status.msg = "depend node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    if (kPhysicalOpDataProvider == depend->type_) {
        auto data_op = dynamic_cast<PhysicalDataProviderNode*>(depend);
        if (kProviderTypeRequest == data_op->provider_type_) {
            auto name = data_op->table_handler_->GetName();
            auto table = catalog_->GetTable(db_, name);
            if (table) {
                auto right = new PhysicalTableProviderNode(table);
                node_manager_->RegisterNode(right);
                *output =
                    new PhysicalRequestUnionNode(depend, right, groups, orders);
                node_manager_->RegisterNode(*output);
                return true;
            } else {
                status.code = common::kPlanError;
                status.msg = "fail to transform data provider op: table " +
                             name + "not exists";
                LOG(WARNING) << status.msg;
                return false;
            }
        }
    }

    PhysicalGroupAndSortNode* group_sort_op =
        new PhysicalGroupAndSortNode(depend, groups, orders);
    node_manager_->RegisterNode(group_sort_op);
    *output = group_sort_op;
    return true;
}
bool BatchModeTransformer::TransformJoinOp(const node::JoinPlanNode* node,
                                           PhysicalOpNode** output,
                                           base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    PhysicalOpNode* left = nullptr;
    PhysicalOpNode* right = nullptr;
    if (!TransformPlanOp(node->GetChildren()[0], &left, status)) {
        return false;
    }
    if (!TransformPlanOp(node->GetChildren()[1], &right, status)) {
        return false;
    }
    *output =
        new PhysicalJoinNode(left, right, node->join_type_, node->condition_);
    node_manager_->RegisterNode(*output);
    return true;
}
bool BatchModeTransformer::TransformUnionOp(const node::UnionPlanNode* node,
                                            PhysicalOpNode** output,
                                            base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    PhysicalOpNode* left = nullptr;
    PhysicalOpNode* right = nullptr;
    if (!TransformPlanOp(node->GetChildren()[0], &left, status)) {
        return false;
    }
    if (!TransformPlanOp(node->GetChildren()[1], &right, status)) {
        return false;
    }
    *output = new PhysicalUnionNode(left, right, node->is_all);
    node_manager_->RegisterNode(*output);
    return true;
}
bool BatchModeTransformer::TransformGroupOp(const node::GroupPlanNode* node,
                                            PhysicalOpNode** output,
                                            base::Status& status) {
    PhysicalOpNode* left = nullptr;
    if (!TransformPlanOp(node->GetChildren()[0], &left, status)) {
        return false;
    }

    if (kPhysicalOpDataProvider == left->type_) {
        auto data_op = dynamic_cast<PhysicalDataProviderNode*>(left);
        if (kProviderTypeRequest == data_op->provider_type_) {
            auto name = data_op->table_handler_->GetName();
            auto table = catalog_->GetTable(db_, name);
            if (table) {
                auto right = new PhysicalTableProviderNode(table);
                node_manager_->RegisterNode(right);
                *output = new PhysicalRequestUnionNode(left, right,
                                                       node->by_list_, nullptr);
                node_manager_->RegisterNode(*output);
                return true;
            } else {
                status.code = common::kPlanError;
                status.msg = "fail to transform data provider op: table " +
                             name + "not exists";
                LOG(WARNING) << status.msg;
                return false;
            }
        }
    }

    *output = new PhysicalGroupNode(left, node->by_list_);
    node_manager_->RegisterNode(*output);
    return true;
}
bool BatchModeTransformer::TransformSortOp(const node::SortPlanNode* node,
                                           PhysicalOpNode** output,
                                           base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    PhysicalOpNode* left = nullptr;
    if (!TransformPlanOp(node->GetChildren()[0], &left, status)) {
        return false;
    }
    *output = new PhysicalSortNode(left, node->order_list_);
    node_manager_->RegisterNode(*output);
    return true;
}
bool BatchModeTransformer::TransformFilterOp(const node::FilterPlanNode* node,
                                             PhysicalOpNode** output,
                                             base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    PhysicalOpNode* depend = nullptr;
    if (!TransformPlanOp(node->GetChildren()[0], &depend, status)) {
        return false;
    }
    *output = new PhysicalFliterNode(depend, node->condition_);
    node_manager_->RegisterNode(*output);
    return true;
}

bool BatchModeTransformer::TransformScanOp(const node::TablePlanNode* node,
                                           PhysicalOpNode** output,
                                           base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    if (node->IsPrimary()) {
        auto table = catalog_->GetTable("request", node->table_);
        if (table) {
            *output = new PhysicalRequestProviderNode(table);
            node_manager_->RegisterNode(*output);
        } else {
            status.msg = "fail to transform data_provider op: request." +
                         node->table_ + " not exist!";
            status.code = common::kPlanError;
            LOG(WARNING) << status.msg;
            return false;
        }
    } else {
        auto table = catalog_->GetTable(db_, node->table_);
        if (table) {
            *output = new PhysicalTableProviderNode(table);
            node_manager_->RegisterNode(*output);
        } else {
            status.msg = "fail to transform data_provider op: table " + db_ +
                         "." + node->table_ + " not exist!";
            status.code = common::kPlanError;
            LOG(WARNING) << status.msg;
            return false;
        }
    }
    return true;
}

bool BatchModeTransformer::TransformRenameOp(const node::RenamePlanNode* node,
                                             PhysicalOpNode** output,
                                             base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        return false;
    }
    PhysicalOpNode* left = nullptr;
    if (!TransformPlanOp(node->GetChildren()[0], &left, status)) {
        return false;
    }
    *output = new PhysicalRenameNode(left, node->table_);
    node_manager_->RegisterNode(*output);
    return true;
}

bool BatchModeTransformer::TransformDistinctOp(
    const node::DistinctPlanNode* node, PhysicalOpNode** output,
    base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        return false;
    }
    PhysicalOpNode* left = nullptr;
    if (!TransformPlanOp(node->GetChildren()[0], &left, status)) {
        return false;
    }
    *output = new PhysicalDistinctNode(left);
    node_manager_->RegisterNode(*output);
    return true;
}
bool BatchModeTransformer::TransformQueryPlan(
    const ::fesql::node::PlanNode* node, ::fesql::vm::PhysicalOpNode** output,
    ::fesql::base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        return false;
    }
    if (!TransformPlanOp(node->GetChildren()[0], output, status)) {
        return false;
    }
    return true;
}
bool BatchModeTransformer::AddPass(PhysicalPlanPassType type) {
    passes.push_back(type);
    return true;
}
bool BatchModeTransformer::GenProjects(const Schema& input_schema,
                                       const node::PlanNodeList& projects,
                                       const bool row_project,
                                       std::string& fn_name,    // NOLINT
                                       Schema& output_schema,   // NOLINT
                                       base::Status& status) {  // NOLINT
    // TODO(wangtaize) use ops end op output schema
    ::fesql::codegen::RowFnLetIRBuilder builder(input_schema, module_);
    fn_name = "__internal_sql_codegen_" + std::to_string(id_++);
    bool ok = builder.Build(fn_name, projects, row_project, output_schema);
    if (!ok) {
        status.code = common::kCodegenError;
        status.msg = "fail to codegen projects node";
        LOG(WARNING) << status.msg;
        return false;
    }
    return true;
}
bool BatchModeTransformer::AddDefaultPasses() {
    AddPass(kPassLeftJoinOptimized);
    AddPass(kPassGroupAndSortOptimized);
    return false;
}

bool BatchModeTransformer::CreatePhysicalProjectNode(
    const ProjectType project_type, PhysicalOpNode* node,
    node::ProjectListNode* project_list, PhysicalOpNode** output,
    base::Status& status) {
    if (nullptr == project_list || nullptr == output) {
        status.msg = "project node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    const node::PlanNodeList& projects = project_list->GetProjects();

    node::PlanNodeList new_projects;
    bool has_all_project = false;

    for (auto iter = projects.cbegin(); iter != projects.cend(); iter++) {
        auto project_node = dynamic_cast<node::ProjectNode*>(*iter);
        auto expr = project_node->GetExpression();
        if (nullptr == expr) {
            status.msg = "invalid project: expression is null";
            status.code = common::kPlanError;
            return false;
        }
        if (node::kExprAll == expr->expr_type_) {
            auto all_expr = dynamic_cast<node::AllNode*>(expr);
            if (all_expr->children_.empty()) {
                // expand all expression if needed
                for (auto column : node->output_schema) {
                    all_expr->children_.push_back(
                        node_manager_->MakeColumnRefNode(
                            column.name(), all_expr->GetRelationName()));
                }
            }
            has_all_project = true;
        }
    }

    if (has_all_project && 1 == projects.size()) {
        // skip project
        DLOG(INFO) << "skip project node: project has only kAllExpr "
                      "expression";
        *output = node;
        return true;
    }

    Schema output_schema;
    std::string fn_name;
    switch (project_type) {
        case kRowProject:
        case kTableProject: {
            if (!GenProjects(node->output_schema, projects, true, fn_name,
                             output_schema, status)) {
                return false;
            }
            break;
        }
        case kAggregation:
        case kGroupAggregation: {
            if (!GenProjects(node->output_schema, projects, false, fn_name,
                             output_schema, status)) {
                return false;
            }
            break;
        }
        case kWindowAggregation: {
            // TODO(chenjing): gen window aggregation
            if (!GenProjects(node->output_schema, projects, false, fn_name,
                             output_schema, status)) {
                return false;
            }
            break;
        }
    }

    PhysicalOpNode* op = nullptr;
    switch (project_type) {
        case kRowProject: {
            op = new PhysicalRowProjectNode(node, fn_name, output_schema);
            break;
        }
        case kTableProject: {
            op = new PhysicalTableProjectNode(node, fn_name, output_schema);
            break;
        }
        case kAggregation: {
            op = new PhysicalAggrerationNode(node, fn_name, output_schema);
            break;
        }
        case kGroupAggregation: {
            auto group_op = dynamic_cast<PhysicalGroupNode*>(node);
            op = new PhysicalGroupAggrerationNode(node, group_op->groups_,
                                                  fn_name, output_schema);
            break;
        }
        case kWindowAggregation: {
            auto group_srot_op = dynamic_cast<PhysicalGroupAndSortNode*>(node);
            op = new PhysicalWindowAggrerationNode(
                group_srot_op, group_srot_op->groups_, group_srot_op->orders_,
                fn_name, output_schema, project_list->w_ptr_->GetStartOffset(),
                project_list->w_ptr_->GetEndOffset());
            break;
        }
    }
    node_manager_->RegisterNode(op);
    *output = op;
    return true;
}

bool BatchModeTransformer::TransformProjectOp(
    node::ProjectListNode* project_list, PhysicalOpNode* node,
    PhysicalOpNode** output, base::Status& status) {
    auto depend = node;
    if (nullptr != project_list->w_ptr_) {
        node::OrderByNode* orders = nullptr;
        node::ExprListNode* groups = nullptr;

        if (!project_list->w_ptr_->ExtractWindowGroupsAndOrders(&groups,
                                                                &orders)) {
            status.msg =
                "fail to transform window aggeration: gourps and orders is "
                "null";
            LOG(WARNING) << status.msg;
            return false;
        }
        if (!TransformGroupAndSortOp(depend, groups, orders, &depend, status)) {
            return false;
        }
    }

    switch (depend->output_type) {
        case kSchemaTypeRow:
            return CreatePhysicalProjectNode(kRowProject, depend, project_list,
                                             output, status);
        case kSchemaTypeGroup:
            if (nullptr != project_list->w_ptr_) {
                return CreatePhysicalProjectNode(kWindowAggregation, depend,
                                                 project_list, output, status);
            } else {
                return CreatePhysicalProjectNode(kGroupAggregation, depend,
                                                 project_list, output, status);
            }
        case kSchemaTypeTable:
            if (project_list->is_window_agg_) {
                return CreatePhysicalProjectNode(kAggregation, depend,
                                                 project_list, output, status);
            } else {
                return CreatePhysicalProjectNode(kTableProject, depend,
                                                 project_list, output, status);
            }
    }
    return false;
}
void BatchModeTransformer::ApplyPasses(PhysicalOpNode* node,
                                       PhysicalOpNode** output) {
    auto physical_plan = node;
    for (auto type : passes) {
        switch (type) {
            case kPassGroupAndSortOptimized: {
                GroupAndSortOptimized pass(node_manager_, db_, catalog_);
                PhysicalOpNode* new_op = nullptr;
                if (pass.Apply(physical_plan, &new_op)) {
                    physical_plan = new_op;
                }
                break;
            }
            case kPassLeftJoinOptimized: {
                LeftJoinOptimized pass(node_manager_, db_, catalog_);
                PhysicalOpNode* new_op = nullptr;
                if (pass.Apply(physical_plan, &new_op)) {
                    physical_plan = new_op;
                }
                break;
            }
            default: {
                LOG(WARNING) << "can't not handle pass: "
                             << PhysicalPlanPassTypeName(type);
            }
        }
    }
    *output = physical_plan;
}
bool BatchModeTransformer::TransformPhysicalPlan(
    const ::fesql::node::PlanNodeList& trees,
    ::fesql::vm::PhysicalOpNode** output, base::Status& status) {
    if (nullptr == module_ || trees.empty()) {
        status.msg = "module or logical trees is empty";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }

    auto it = trees.begin();
    for (; it != trees.end(); ++it) {
        const ::fesql::node::PlanNode* node = *it;
        switch (node->GetType()) {
            case ::fesql::node::kPlanTypeFuncDef: {
                const ::fesql::node::FuncDefPlanNode* func_def_plan =
                    dynamic_cast<const ::fesql::node::FuncDefPlanNode*>(node);
                if (!GenFnDef(func_def_plan, status)) {
                    return false;
                }
                break;
            }
            case ::fesql::node::kPlanTypeUnion:
            case ::fesql::node::kPlanTypeQuery: {
                PhysicalOpNode* physical_plan = nullptr;
                if (!TransformQueryPlan(node, &physical_plan, status)) {
                    return false;
                }
                ApplyPasses(physical_plan, output);
                if (!GenPlanNode(*output, status)) {
                    LOG(WARNING) << "fail to gen plan";

                    return false;
                }
                break;
            }
            default: {
                LOG(WARNING) << "not supported";
                return false;
            }
        }
    }
    return true;
}
bool BatchModeTransformer::GenFnDef(const node::FuncDefPlanNode* fn_plan,
                                    base::Status& status) {
    if (nullptr == module_ || nullptr == fn_plan ||
        nullptr == fn_plan->fn_def_) {
        status.msg = "fail to codegen function: module or fn_def node is null";
        status.code = common::kOpGenError;
        LOG(WARNING) << status.msg;
        return false;
    }

    ::fesql::codegen::FnIRBuilder builder(module_);
    bool ok = builder.Build(fn_plan->fn_def_, status);
    if (!ok) {
        status.msg = "fail to codegen function: " + status.msg;
        status.code = common::kCodegenError;
        LOG(WARNING) << status.msg;
        return false;
    }
    return true;
}

bool BatchModeTransformer::CodeGenExprList(Schema input_schema,
                                           const node::ExprListNode* expr_list,
                                           bool row_mode, std::string& fn_name,
                                           Schema& output_schema,
                                           base::Status& status) {
    if (node::ExprListNullOrEmpty(expr_list)) {
        status.msg = "fail to codegen expr list: null or empty list";
        status.code = common::kCodegenError;
        return false;
    }
    node::PlanNodeList projects;
    int32_t pos = 0;
    for (auto expr : expr_list->children_) {
        projects.push_back(node_manager_->MakeRowProjectNode(
            pos++, node::ExprString(expr), expr));
    }

    return GenProjects(input_schema, projects, true, fn_name, output_schema,
                       status);
}

bool GroupAndSortOptimized::Transform(PhysicalOpNode* in,
                                      PhysicalOpNode** output) {
    switch (in->type_) {
        case kPhysicalOpGroupBy: {
            PhysicalGroupNode* group_op = dynamic_cast<PhysicalGroupNode*>(in);
            if (kPhysicalOpDataProvider == in->GetProducers()[0]->type_) {
                auto scan_op = dynamic_cast<PhysicalDataProviderNode*>(
                    in->GetProducers()[0]);
                if (kProviderTypeTable == scan_op->provider_type_) {
                    std::string index_name;
                    const node::ExprListNode* new_groups = nullptr;
                    if (!TransformGroupExpr(group_op->groups_,
                                            scan_op->table_handler_->GetIndex(),
                                            &index_name, &new_groups)) {
                        return false;
                    }

                    PhysicalScanIndexNode* scan_index_op =
                        new PhysicalScanIndexNode(scan_op->table_handler_,
                                                  index_name);
                    node_manager_->RegisterNode(scan_index_op);

                    if (nullptr == new_groups || new_groups->IsEmpty()) {
                        *output = scan_index_op;
                    } else {
                        PhysicalGroupNode* new_group_op =
                            new PhysicalGroupNode(scan_index_op, new_groups);
                        *output = new_group_op;
                    }
                    return true;
                }
            }
            break;
        }
        case kPhysicalOpGroupAndSort: {
            PhysicalGroupAndSortNode* group_sort_op =
                dynamic_cast<PhysicalGroupAndSortNode*>(in);
            if (kPhysicalOpDataProvider == in->GetProducers()[0]->type_) {
                auto scan_op = dynamic_cast<PhysicalDataProviderNode*>(
                    in->GetProducers()[0]);
                if (kProviderTypeTable == scan_op->provider_type_) {
                    std::string index_name;
                    const node::ExprListNode* new_groups = nullptr;
                    const node::OrderByNode* new_orders = nullptr;
                    auto& index_hint = scan_op->table_handler_->GetIndex();
                    if (!TransformGroupExpr(group_sort_op->groups_, index_hint,
                                            &index_name, &new_groups)) {
                        return false;
                    }

                    auto index_st = index_hint.at(index_name);
                    TransformOrderExpr(group_sort_op->orders_,
                                       scan_op->table_handler_->GetSchema(),
                                       index_st, &new_orders);

                    PhysicalScanIndexNode* scan_index_op =
                        new PhysicalScanIndexNode(scan_op->table_handler_,
                                                  index_name);
                    node_manager_->RegisterNode(scan_index_op);

                    PhysicalGroupAndSortNode* new_group_sort_op =
                        new PhysicalGroupAndSortNode(scan_index_op, new_groups,
                                                     new_orders);
                    *output = new_group_sort_op;
                    return true;
                }
            }
            break;
        }
        case kPhysicalOpRequestUnoin: {
            PhysicalRequestUnionNode* union_op =
                dynamic_cast<PhysicalRequestUnionNode*>(in);
            if (kPhysicalOpDataProvider == in->GetProducers()[1]->type_) {
                auto scan_op = dynamic_cast<PhysicalDataProviderNode*>(
                    in->GetProducers()[1]);
                if (kProviderTypeTable == scan_op->provider_type_) {
                    std::string index_name;
                    const node::ExprListNode* new_groups = nullptr;
                    const node::OrderByNode* new_orders = nullptr;
                    auto& index_hint = scan_op->table_handler_->GetIndex();
                    if (!TransformGroupExpr(union_op->groups_, index_hint,
                                            &index_name, &new_groups)) {
                        return false;
                    }

                    auto index_st = index_hint.at(index_name);
                    TransformOrderExpr(union_op->orders_,
                                       scan_op->table_handler_->GetSchema(),
                                       index_st, &new_orders);

                    PhysicalScanIndexNode* scan_index_op =
                        new PhysicalScanIndexNode(scan_op->table_handler_,
                                                  index_name);
                    node_manager_->RegisterNode(scan_index_op);

                    PhysicalRequestUnionNode* new_group_sort_op =
                        new PhysicalRequestUnionNode(
                            union_op->GetProducers()[0], scan_index_op,
                            new_groups, new_orders);
                    *output = new_group_sort_op;
                    return true;
                }
            }
            break;
        }
        default: {
            return false;
        }
    }
    return false;
}
bool GroupAndSortOptimized::TransformGroupExpr(
    const node::ExprListNode* groups, const IndexHint& index_hint,
    std::string* index_name, const node::ExprListNode** output) {
    if (nullptr == groups || nullptr == output || nullptr == index_name) {
        LOG(WARNING) << "fail to transform group expr : group exor or output "
                        "or index_name ptr is null";
        return false;
    }
    std::vector<std::string> columns;
    for (auto group : groups->children_) {
        switch (group->expr_type_) {
            case node::kExprColumnRef:
                columns.push_back(
                    dynamic_cast<node::ColumnRefNode*>(group)->GetColumnName());
                break;
            default: {
                break;
            }
        }
    }

    if (columns.empty()) {
        return false;
    }

    std::vector<bool> bitmap(columns.size(), true);
    if (MatchBestIndex(columns, index_hint, &bitmap, index_name)) {
        IndexSt index = index_hint.at(*index_name);
        node::ExprListNode* new_groups = node_manager_->MakeExprList();
        std::set<std::string> keys;
        for (auto iter = index.keys.cbegin(); iter != index.keys.cend();
             iter++) {
            keys.insert(iter->name);
        }
        for (auto group : groups->children_) {
            switch (group->expr_type_) {
                case node::kExprColumnRef: {
                    std::string column =
                        dynamic_cast<node::ColumnRefNode*>(group)
                            ->GetColumnName();
                    // skip group when match index keys
                    if (keys.find(column) == keys.cend()) {
                        new_groups->AddChild(group);
                    }
                    break;
                }
                default: {
                    new_groups->AddChild(group);
                }
            }
        }
        *output = new_groups;
        return true;
    } else {
        return false;
    }
}

bool GroupAndSortOptimized::MatchBestIndex(
    const std::vector<std::string>& columns, const IndexHint& index_hint,
    std::vector<bool>* bitmap_ptr, std::string* index_name) {
    if (nullptr == bitmap_ptr || nullptr == index_name) {
        LOG(WARNING)
            << "fail to match best index: bitmap or index_name ptr is null";
        return false;
    }
    std::set<std::string> column_set;
    auto& bitmap = *bitmap_ptr;
    for (size_t i = 0; i < columns.size(); ++i) {
        if (bitmap[i]) {
            column_set.insert(columns[i]);
        }
    }

    for (auto iter = index_hint.cbegin(); iter != index_hint.cend(); iter++) {
        IndexSt index = iter->second;
        std::set<std::string> keys;
        for (auto key_iter = index.keys.cbegin(); key_iter != index.keys.cend();
             key_iter++) {
            keys.insert(key_iter->name);
        }

        if (column_set == keys) {
            *index_name = index.name;
            return true;
        }
    }

    std::string best_index_name;
    bool succ = false;
    for (size_t i = 0; i < bitmap.size(); ++i) {
        if (bitmap[i]) {
            bitmap[i] = false;
            std::string name;
            if (MatchBestIndex(columns, index_hint, bitmap_ptr, &name)) {
                succ = true;
                if (best_index_name.empty()) {
                    best_index_name = name;
                } else {
                    auto org_index = index_hint.at(best_index_name);
                    auto new_index = index_hint.at(name);
                    if (org_index.keys.size() < new_index.keys.size()) {
                        best_index_name = name;
                    }
                }
            }
            bitmap[i] = true;
        }
    }
    *index_name = best_index_name;
    return succ;
}

// This is primarily intended to be used on top-level WHERE (or JOIN/ON)
// clauses.  It can also be used on top-level CHECK constraints, for which
// pass is_check = true.  DO NOT call it on any expression that is not known
// to be one or the other, as it might apply inappropriate simplifications.
bool CanonicalizeExprTransformPass::Transform(node::ExprNode* in,
                                              node::ExprNode** output) {
    // 1. 忽略NULL以及OR中的False/AND中的TRUE
    // 2. 拉平谓词
    // 3. 清除重复ORs
    // 4.
    return false;
}
bool TransformUpPysicalPass::Apply(PhysicalOpNode* in, PhysicalOpNode** out) {
    if (nullptr == in || nullptr == out) {
        LOG(WARNING) << "fail to apply pass: input or output is null";
        return false;
    }
    auto producer = in->GetProducers();
    for (size_t j = 0; j < producer.size(); ++j) {
        PhysicalOpNode* output = nullptr;
        if (Apply(producer[j], &output)) {
            in->UpdateProducer(j, output);
        }
    }
    return Transform(in, out);
}

bool GroupAndSortOptimized::TransformOrderExpr(
    const node::OrderByNode* order, const Schema& schema,
    const IndexSt& index_st, const node::OrderByNode** output) {
    if (nullptr == order || nullptr == output) {
        LOG(WARNING) << "fail to transform order expr : order expr or "
                        "data_provider op "
                        "or output is null";
        return false;
    }

    auto& ts_column = schema.Get(index_st.ts_pos);
    std::vector<std::string> columns;
    *output = order;

    bool succ = false;
    for (auto expr : order->order_by_->children_) {
        switch (expr->expr_type_) {
            case node::kExprColumnRef:
                columns.push_back(
                    dynamic_cast<node::ColumnRefNode*>(expr)->GetColumnName());
                if (ts_column.name() ==
                    dynamic_cast<node::ColumnRefNode*>(expr)->GetColumnName()) {
                    succ = true;
                }
                break;
            default: {
                break;
            }
        }
    }

    if (succ) {
        node::ExprListNode* expr_list = node_manager_->MakeExprList();
        for (auto expr : order->order_by_->children_) {
            switch (expr->expr_type_) {
                case node::kExprColumnRef: {
                    std::string column =
                        dynamic_cast<node::ColumnRefNode*>(expr)
                            ->GetColumnName();
                    // skip group when match index keys
                    if (ts_column.name() != column) {
                        expr_list->AddChild(expr);
                    }
                    break;
                }
                default: {
                    expr_list->AddChild(expr);
                }
            }
        }
        *output = dynamic_cast<node::OrderByNode*>(
            node_manager_->MakeOrderByNode(expr_list, order->is_asc_));
        return true;
    } else {
        return false;
    }
}

bool LeftJoinOptimized::Transform(PhysicalOpNode* in, PhysicalOpNode** output) {
    if (nullptr == in) {
        LOG(WARNING) << "LeftJoin optimized skip: node is null";
        return false;
    }
    switch (in->type_) {
        case kPhysicalOpGroupBy: {
            auto group_op = dynamic_cast<PhysicalGroupNode*>(in);
            if (node::ExprListNullOrEmpty(group_op->groups_)) {
                LOG(WARNING)
                    << "LeftJoin optimized skip: groups is null or empty";
            }
            if (kPhysicalOpJoin == in->GetProducers()[0]->type_) {
                PhysicalJoinNode* join_op =
                    dynamic_cast<PhysicalJoinNode*>(in->GetProducers()[0]);

                if (node::kJoinTypeLeft != join_op->join_type_) {
                    // skip optimized for other join type
                    return false;
                }
                if (!CheckExprListFromSchema(
                        group_op->groups_,
                        join_op->GetProducers()[0]->output_schema)) {
                    return false;
                }
                auto group_expr = group_op->groups_;
                // 符合优化条件
                PhysicalGroupNode* new_group_op = new PhysicalGroupNode(
                    join_op->GetProducers()[0], group_expr);
                PhysicalJoinNode* new_join_op = new PhysicalJoinNode(
                    new_group_op, join_op->GetProducers()[1],
                    join_op->join_type_, join_op->condition_);
                node_manager_->RegisterNode(new_group_op);
                node_manager_->RegisterNode(new_join_op);
                *output = new_join_op;
                return true;
            }

            break;
        }
        case kPhysicalOpSortBy: {
            auto sort_op = dynamic_cast<PhysicalSortNode*>(in);
            if (nullptr == sort_op->order_ ||
                node::ExprListNullOrEmpty(sort_op->order_->order_by_)) {
                LOG(WARNING)
                    << "LeftJoin optimized skip: order is null or empty";
            }
            if (kPhysicalOpJoin == in->GetProducers()[0]->type_) {
                if (kPhysicalOpJoin == in->GetProducers()[0]->type_) {
                    PhysicalJoinNode* join_op =
                        dynamic_cast<PhysicalJoinNode*>(in->GetProducers()[0]);

                    if (node::kJoinTypeLeft != join_op->join_type_) {
                        // skip optimized for other join type
                        return false;
                    }

                    if (!CheckExprListFromSchema(
                            sort_op->order_->order_by_,
                            join_op->GetProducers()[0]->output_schema)) {
                        return false;
                    }

                    auto order_expr = sort_op->order_;
                    // 符合优化条件
                    PhysicalSortNode* new_order_op = new PhysicalSortNode(
                        join_op->GetProducers()[0], order_expr);
                    node_manager_->RegisterNode(new_order_op);
                    PhysicalJoinNode* new_join_op = new PhysicalJoinNode(
                        new_order_op, join_op->GetProducers()[1],
                        join_op->join_type_, join_op->condition_);
                    node_manager_->RegisterNode(new_order_op);
                    *output = new_join_op;
                    return true;
                }
            }

            break;
        }

        case kPhysicalOpGroupAndSort: {
            auto group_sort_op = dynamic_cast<PhysicalGroupAndSortNode*>(in);
            if (node::ExprListNullOrEmpty(group_sort_op->groups_) &&
                (nullptr == group_sort_op->orders_ ||
                 node::ExprListNullOrEmpty(
                     group_sort_op->orders_->order_by_))) {
                LOG(WARNING) << "LeftJoin group and sort optimized skip: both "
                                "order and groups are empty ";
            }
            if (kPhysicalOpJoin == in->GetProducers()[0]->type_) {
                if (kPhysicalOpJoin == in->GetProducers()[0]->type_) {
                    PhysicalJoinNode* join_op =
                        dynamic_cast<PhysicalJoinNode*>(in->GetProducers()[0]);

                    if (node::kJoinTypeLeft != join_op->join_type_) {
                        // skip optimized for other join type
                        return false;
                    }

                    if (!CheckExprListFromSchema(
                            group_sort_op->groups_,
                            join_op->GetProducers()[0]->output_schema)) {
                        return false;
                    }

                    if (!CheckExprListFromSchema(
                            group_sort_op->orders_->order_by_,
                            join_op->GetProducers()[0]->output_schema)) {
                        return false;
                    }

                    // 符合优化条件
                    PhysicalGroupAndSortNode* new_group_sort_op =
                        new PhysicalGroupAndSortNode(join_op->GetProducers()[0],
                                                     group_sort_op->groups_,
                                                     group_sort_op->orders_);
                    node_manager_->RegisterNode(new_group_sort_op);
                    PhysicalJoinNode* new_join_op = new PhysicalJoinNode(
                        new_group_sort_op, join_op->GetProducers()[1],
                        join_op->join_type_, join_op->condition_);
                    node_manager_->RegisterNode(new_join_op);
                    *output = new_join_op;
                    return true;
                }
            }

            break;
        }
        default: {
            return false;
        }
    }
    return false;
}  // namespace vm
bool LeftJoinOptimized::ColumnExist(const Schema& schema,
                                    const std::string& column_name) {
    for (int32_t i = 0; i < schema.size(); i++) {
        const type::ColumnDef& column = schema.Get(i);
        if (column_name == column.name()) {
            return true;
        }
    }
    return false;
}
bool LeftJoinOptimized::CheckExprListFromSchema(
    const node::ExprListNode* expr_list, const Schema& schema) {
    if (node::ExprListNullOrEmpty(expr_list)) {
        return true;
    }
    for (auto expr : expr_list->children_) {
        switch (expr->expr_type_) {
            case node::kExprColumnRef: {
                auto column = dynamic_cast<node::ColumnRefNode*>(expr);
                if (!ColumnExist(schema, column->GetColumnName())) {
                    return false;
                }
                break;
            }
            default: {
                // can't optimize when group by other expression
                return false;
            }
        }
    }
    return true;
}

RequestModeransformer::RequestModeransformer(
    node::NodeManager* node_manager, const std::string& db,
    const std::shared_ptr<Catalog>& catalog, ::llvm::Module* module)
    : BatchModeTransformer(node_manager, db, catalog, module) {}

RequestModeransformer::~RequestModeransformer() {}

// transform project plan in request mode
bool RequestModeransformer::TransformProjecPlantOp(
    const node::ProjectPlanNode* node, PhysicalOpNode** output,
    base::Status& status) {
    if (nullptr == node || nullptr == output) {
        status.msg = "input node or output node is null";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }
    PhysicalOpNode* depend = nullptr;
    if (!TransformPlanOp(node->GetChildren()[0], &depend, status)) {
        return false;
    }

    std::vector<PhysicalOpNode*> ops;
    for (auto iter = node->project_list_vec_.cbegin();
         iter != node->project_list_vec_.cend(); iter++) {
        fesql::node::ProjectListNode* project_list =
            dynamic_cast<fesql::node::ProjectListNode*>(*iter);

        PhysicalOpNode* project_op = nullptr;
        if (!TransformProjectOp(project_list, depend, &project_op, status)) {
            return false;
        }
        ops.push_back(project_op);
    }

    if (ops.empty()) {
        status.msg = "fail transform project op: empty projects";
        status.code = common::kPlanError;
        LOG(WARNING) << status.msg;
        return false;
    }

    if (ops.size() == 1) {
        *output = ops[0];
        return true;
    } else {
        auto iter = ops.cbegin();

        PhysicalOpNode* join = new PhysicalJoinNode(
            (*iter), *(++iter), ::fesql::node::kJoinTypeConcat, nullptr);
        node_manager_->RegisterNode(join);
        iter++;
        for (; iter != ops.cend(); iter++) {
            join = new PhysicalJoinNode(
                join, *iter, ::fesql::node::kJoinTypeConcat, nullptr);
            node_manager_->RegisterNode(join);
        }

        auto project_list =
            node_manager_->MakeProjectListPlanNode(nullptr, false);
        uint32_t pos = 0;
        for (auto iter = node->pos_mapping_.cbegin();
             iter != node->pos_mapping_.cend(); iter++) {
            auto sub_project_list = dynamic_cast<node::ProjectListNode*>(
                node->project_list_vec_[iter->first]);

            auto project_node = dynamic_cast<node::ProjectNode*>(
                sub_project_list->GetProjects().at(iter->second));
            if (node::kExprAll == project_node->GetExpression()->expr_type_) {
                auto all_expr =
                    dynamic_cast<node::AllNode*>(project_node->GetExpression());
                if (all_expr->children_.empty()) {
                    // expand all expression if needed
                    for (auto column : depend->output_schema) {
                        all_expr->children_.push_back(
                            node_manager_->MakeColumnRefNode(
                                column.name(), all_expr->GetRelationName()));
                    }
                }
                project_list->AddProject(
                    node_manager_->MakeRowProjectNode(pos, "*", all_expr));
            } else {
                project_list->AddProject(node_manager_->MakeRowProjectNode(
                    pos, project_node->GetName(),
                    node_manager_->MakeColumnRefNode(project_node->GetName(),
                                                     "")));
            }
            pos++;
        }

        return CreatePhysicalProjectNode(kTableProject, join, project_list,
                                         output, status);
    }
}
}  // namespace vm
}  // namespace fesql
