/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * node_manager.cc
 *
 * Author: chenjing
 * Date: 2019/10/28
 *--------------------------------------------------------------------------
 **/

#include "node/node_manager.h"
#include <string>
#include <utility>
#include <vector>

namespace fesql {
namespace node {

QueryNode *NodeManager::MakeSelectQueryNode(
    bool is_distinct, SQLNodeList *select_list_ptr,
    SQLNodeList *tableref_list_ptr, ExprNode *where_expr,
    ExprListNode *group_expr_list, ExprNode *having_expr,
    ExprNode *order_expr_list, SQLNodeList *window_list_ptr,
    SQLNode *limit_ptr) {
    SelectQueryNode *node_ptr =
        new SelectQueryNode(is_distinct, select_list_ptr, tableref_list_ptr,
                            where_expr, group_expr_list, having_expr,
                            dynamic_cast<OrderByNode *>(order_expr_list),
                            window_list_ptr, limit_ptr);
    RegisterNode(node_ptr);
    return node_ptr;
}

QueryNode *NodeManager::MakeUnionQueryNode(QueryNode *left, QueryNode *right,
                                           bool is_all) {
    UnionQueryNode *node_ptr = new UnionQueryNode(left, right, is_all);
    RegisterNode(node_ptr);
    return node_ptr;
}

TableRefNode *NodeManager::MakeTableNode(const std::string &name,
                                         const std::string &alias) {
    TableRefNode *node_ptr = new TableNode(name, alias);
    RegisterNode(node_ptr);
    return node_ptr;
}

TableRefNode *NodeManager::MakeJoinNode(const TableRefNode *left,
                                        const TableRefNode *right,
                                        const JoinType type,
                                        const ExprNode *condition,
                                        const std::string alias) {
    TableRefNode *node_ptr =
        new JoinNode(left, right, type, nullptr, condition, alias);
    RegisterNode(node_ptr);
    return node_ptr;
}

TableRefNode *NodeManager::MakeLastJoinNode(const TableRefNode *left,
                                            const TableRefNode *right,
                                            const ExprNode *orders,
                                            const ExprNode *condition,
                                            const std::string alias) {
    if (nullptr == orders || node::kExprOrder != orders->GetExprType()) {
        LOG(WARNING)
            << "fail to create last join node with invalid order type " +
                   NameOfSQLNodeType(orders->GetType());
        return nullptr;
    }
    TableRefNode *node_ptr = new JoinNode(
        left, right, node::kJoinTypeLast,
        dynamic_cast<const OrderByNode *>(orders), condition, alias);
    RegisterNode(node_ptr);
    return node_ptr;
}

TableRefNode *NodeManager::MakeQueryRefNode(const QueryNode *sub_query,
                                            const std::string &alias) {
    TableRefNode *node_ptr = new QueryRefNode(sub_query, alias);
    RegisterNode(node_ptr);
    return node_ptr;
}
SQLNode *NodeManager::MakeResTargetNode(ExprNode *node,
                                        const std::string &name) {
    ResTarget *node_ptr = new ResTarget(name, node);
    return RegisterNode(node_ptr);
}

SQLNode *NodeManager::MakeLimitNode(int count) {
    LimitNode *node_ptr = new LimitNode(count);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeWindowDefNode(ExprListNode *partitions,
                                        ExprNode *orders, SQLNode *frame) {
    return MakeWindowDefNode(nullptr, partitions, orders, frame, true);
}

SQLNode *NodeManager::MakeWindowDefNode(SQLNodeList *union_tables,
                                        ExprListNode *partitions,
                                        ExprNode *orders, SQLNode *frame,
                                        bool instance_not_in_window) {
    WindowDefNode *node_ptr = new WindowDefNode();
    if (nullptr != orders) {
        if (node::kExprOrder != orders->GetExprType()) {
            LOG(WARNING)
                << "fail to create window node with invalid order type " +
                       NameOfSQLNodeType(orders->GetType());
            delete node_ptr;
            return nullptr;
        }
        node_ptr->SetOrders(dynamic_cast<OrderByNode *>(orders));
    }
    node_ptr->set_instance_not_in_window(instance_not_in_window);
    node_ptr->set_union_tables(union_tables);
    node_ptr->SetPartitions(partitions);
    node_ptr->SetFrame(dynamic_cast<FrameNode *>(frame));
    return RegisterNode(node_ptr);
}

SQLNode *NodeManager::MakeWindowDefNode(const std::string &name) {
    WindowDefNode *node_ptr = new WindowDefNode();
    node_ptr->SetName(name);
    return RegisterNode(node_ptr);
}

SQLNode *NodeManager::MakeFrameBound(SQLNodeType bound_type) {
    FrameBound *node_ptr = new FrameBound(bound_type);
    return RegisterNode(node_ptr);
}

SQLNode *NodeManager::MakeFrameBound(SQLNodeType bound_type, ExprNode *offset) {
    FrameBound *node_ptr = new FrameBound(bound_type, offset);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeFrameExtent(SQLNode *start, SQLNode *end) {
    FrameExtent *node_ptr = new FrameExtent(dynamic_cast<FrameBound *>(start),
                                            dynamic_cast<FrameBound *>(end));
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeFrameNode(FrameType frame_type, SQLNode *frame_extent,
                                    ExprNode *frame_size) {
    if (nullptr != frame_extent && node::kFrameExtent != frame_extent->type_) {
        LOG(WARNING) << "Fail Make Frame Node: 2nd arg isn't frame extent";
        return nullptr;
    }

    if (nullptr != frame_size && node::kExprPrimary != frame_size->expr_type_) {
        LOG(WARNING) << "Fail Make Frame Node: 3nd arg isn't const expression";
        return nullptr;
    }

    FrameNode *node_ptr =
        new FrameNode(frame_type, dynamic_cast<FrameExtent *>(frame_extent),
                      dynamic_cast<ConstNode *>(frame_size));
    return RegisterNode(node_ptr);
}
ExprNode *NodeManager::MakeOrderByNode(const ExprListNode *order,
                                       const bool is_asc) {
    OrderByNode *node_ptr = new OrderByNode(order, is_asc);
    return RegisterNode(node_ptr);
}

ExprNode *NodeManager::MakeColumnRefNode(const std::string &column_name,
                                         const std::string &relation_name,
                                         const std::string &db_name) {
    ColumnRefNode *node_ptr =
        new ColumnRefNode(column_name, relation_name, db_name);

    return RegisterNode(node_ptr);
}

ExprNode *NodeManager::MakeColumnRefNode(const std::string &column_name,
                                         const std::string &relation_name) {
    return MakeColumnRefNode(column_name, relation_name, "");
}

ExprNode *NodeManager::MakeFuncNode(const std::string &name,
                                    const ExprListNode *list_ptr,
                                    const SQLNode *over) {
    CallExprNode *node_ptr = new CallExprNode(
        name, list_ptr, dynamic_cast<const WindowDefNode *>(over));
    return RegisterNode(node_ptr);
}
ExprNode *NodeManager::MakeConstNode(int value) {
    ExprNode *node_ptr = new ConstNode(value);
    return RegisterNode(node_ptr);
}

ExprNode *NodeManager::MakeConstNode(int64_t value) {
    ExprNode *node_ptr = new ConstNode(value);
    return RegisterNode(node_ptr);
}

ExprNode *NodeManager::MakeConstNode(int64_t value, DataType time_type) {
    ExprNode *node_ptr = new ConstNode(value, time_type);
    return RegisterNode(node_ptr);
}
ExprNode *NodeManager::MakeConstNode(float value) {
    ExprNode *node_ptr = new ConstNode(value);
    return RegisterNode(node_ptr);
}

ExprNode *NodeManager::MakeConstNode(double value) {
    ExprNode *node_ptr = new ConstNode(value);
    return RegisterNode(node_ptr);
}

ExprNode *NodeManager::MakeConstNode(const char *value) {
    ExprNode *node_ptr = new ConstNode(value);
    return RegisterNode(node_ptr);
}
ExprNode *NodeManager::MakeConstNode(const std::string &value) {
    ExprNode *node_ptr = new ConstNode(value);
    return RegisterNode(node_ptr);
}
ExprNode *NodeManager::MakeConstNode() {
    ExprNode *node_ptr = new ConstNode();
    return RegisterNode(node_ptr);
}

ExprNode *NodeManager::MakeExprIdNode(const std::string &name) {
    ::fesql::node::ExprNode *id_node = new ::fesql::node::ExprIdNode(name);
    return RegisterNode(id_node);
}

ExprNode *NodeManager::MakeBinaryExprNode(ExprNode *left, ExprNode *right,
                                          FnOperator op) {
    ::fesql::node::BinaryExpr *bexpr = new ::fesql::node::BinaryExpr(op);
    bexpr->AddChild(left);
    bexpr->AddChild(right);
    return RegisterNode(bexpr);
}

ExprNode *NodeManager::MakeUnaryExprNode(ExprNode *left, FnOperator op) {
    ::fesql::node::UnaryExpr *uexpr = new ::fesql::node::UnaryExpr(op);
    uexpr->AddChild(left);
    return RegisterNode(uexpr);
}

SQLNode *NodeManager::MakeNameNode(const std::string &name) {
    SQLNode *node_ptr = new NameNode(name);
    return RegisterNode(node_ptr);
}

SQLNode *NodeManager::MakeCreateTableNode(bool op_if_not_exist,
                                          const std::string &table_name,
                                          SQLNodeList *column_desc_list) {
    CreateStmt *node_ptr = new CreateStmt(table_name, op_if_not_exist);
    FillSQLNodeList2NodeVector(column_desc_list, node_ptr->GetColumnDefList());
    return RegisterNode(node_ptr);
}

SQLNode *NodeManager::MakeColumnIndexNode(SQLNodeList *index_item_list) {
    ColumnIndexNode *index_ptr = new ColumnIndexNode();
    if (nullptr != index_item_list && 0 != index_item_list->GetSize()) {
        for (auto node_ptr : index_item_list->GetList()) {
            switch (node_ptr->GetType()) {
                case kIndexKey:
                    index_ptr->SetKey(
                        dynamic_cast<IndexKeyNode *>(node_ptr)->GetKey());
                    break;
                case kIndexTs:
                    index_ptr->SetTs(
                        dynamic_cast<IndexTsNode *>(node_ptr)->GetColumnName());
                    break;
                case kIndexVersion:
                    index_ptr->SetVersion(
                        dynamic_cast<IndexVersionNode *>(node_ptr)
                            ->GetColumnName());

                    index_ptr->SetVersionCount(
                        dynamic_cast<IndexVersionNode *>(node_ptr)->GetCount());
                    break;
                case kIndexTTL: {
                    IndexTTLNode *ttl_node =
                        dynamic_cast<IndexTTLNode *>(node_ptr);
                    index_ptr->SetTTL(ttl_node->GetTTLExpr());
                    break;
                }
                case kIndexTTLType: {
                    IndexTTLTypeNode *ttl_type_node =
                        dynamic_cast<IndexTTLTypeNode *>(node_ptr);
                    index_ptr->set_ttl_type(ttl_type_node->ttl_type());
                    break;
                }
                default: {
                    LOG(WARNING) << "can not handle type "
                                 << NameOfSQLNodeType(node_ptr->GetType())
                                 << " for column index";
                }
            }
        }
    }
    return RegisterNode(index_ptr);
}
SQLNode *NodeManager::MakeColumnIndexNode(SQLNodeList *keys, SQLNode *ts,
                                          SQLNode *ttl, SQLNode *version) {
    SQLNode *node_ptr = new SQLNode(kColumnIndex, 0, 0);
    return RegisterNode(node_ptr);
}

SQLNode *NodeManager::MakeColumnDescNode(const std::string &column_name,
                                         const DataType data_type,
                                         bool op_not_null) {
    SQLNode *node_ptr = new ColumnDefNode(column_name, data_type, op_not_null);
    return RegisterNode(node_ptr);
}

SQLNodeList *NodeManager::MakeNodeList() {
    SQLNodeList *new_list_ptr = new SQLNodeList();
    RegisterNode(new_list_ptr);
    return new_list_ptr;
}

SQLNodeList *NodeManager::MakeNodeList(SQLNode *node) {
    SQLNodeList *new_list_ptr = new SQLNodeList();
    new_list_ptr->PushBack(node);
    RegisterNode(new_list_ptr);
    return new_list_ptr;
}

ExprListNode *NodeManager::MakeExprList() {
    ExprListNode *new_list_ptr = new ExprListNode();
    RegisterNode(new_list_ptr);
    return new_list_ptr;
}
ExprListNode *NodeManager::MakeExprList(ExprNode *expr_node) {
    ExprListNode *new_list_ptr = new ExprListNode();
    new_list_ptr->AddChild(expr_node);
    RegisterNode(new_list_ptr);
    return new_list_ptr;
}

PlanNode *NodeManager::MakeLeafPlanNode(const PlanType &type) {
    PlanNode *node_ptr = new LeafPlanNode(type);
    RegisterNode(node_ptr);
    return node_ptr;
}

PlanNode *NodeManager::MakeUnaryPlanNode(const PlanType &type) {
    PlanNode *node_ptr = new UnaryPlanNode(type);
    RegisterNode(node_ptr);
    return node_ptr;
}

PlanNode *NodeManager::MakeBinaryPlanNode(const PlanType &type) {
    PlanNode *node_ptr = new BinaryPlanNode(type);
    RegisterNode(node_ptr);
    return node_ptr;
}

PlanNode *NodeManager::MakeMultiPlanNode(const PlanType &type) {
    PlanNode *node_ptr = new MultiChildPlanNode(type);
    RegisterNode(node_ptr);
    return node_ptr;
}

PlanNode *NodeManager::MakeTablePlanNode(const std::string &table_name) {
    PlanNode *node_ptr = new TablePlanNode("", table_name);
    return RegisterNode(node_ptr);
}

PlanNode *NodeManager::MakeRenamePlanNode(PlanNode *node,
                                          std::string alias_name) {
    PlanNode *node_ptr = new RenamePlanNode(node, alias_name);
    return RegisterNode(node_ptr);
}

FilterPlanNode *NodeManager::MakeFilterPlanNode(PlanNode *node,
                                                const ExprNode *condition) {
    node::FilterPlanNode *node_ptr = new FilterPlanNode(node, condition);
    RegisterNode(node_ptr);
    return node_ptr;
}

WindowPlanNode *NodeManager::MakeWindowPlanNode(int w_id) {
    WindowPlanNode *node_ptr = new WindowPlanNode(w_id);
    RegisterNode(node_ptr);
    return node_ptr;
}

ProjectListNode *NodeManager::MakeProjectListPlanNode(
    const WindowPlanNode *w_ptr, const bool need_agg) {
    ProjectListNode *node_ptr = new ProjectListNode(w_ptr, need_agg);
    RegisterNode(node_ptr);
    return node_ptr;
}

FnNode *NodeManager::MakeFnHeaderNode(const std::string &name,
                                      FnNodeList *plist,
                                      const TypeNode *return_type) {
    ::fesql::node::FnNodeFnHeander *fn_header =
        new FnNodeFnHeander(name, plist, return_type);
    return RegisterNode(fn_header);
}

FnNode *NodeManager::MakeFnDefNode(const FnNode *header,
                                   const FnNodeList *block) {
    ::fesql::node::FnNodeFnDef *fn_def =
        new FnNodeFnDef(dynamic_cast<const FnNodeFnHeander *>(header), block);
    return RegisterNode(fn_def);
}
FnNode *NodeManager::MakeAssignNode(const std::string &name,
                                    ExprNode *expression) {
    ::fesql::node::FnAssignNode *fn_assign =
        new fesql::node::FnAssignNode(name, expression);
    return RegisterNode(fn_assign);
}

FnNode *NodeManager::MakeAssignNode(const std::string &name,
                                    ExprNode *expression, const FnOperator op) {
    ::fesql::node::FnAssignNode *fn_assign = new fesql::node::FnAssignNode(
        name, MakeBinaryExprNode(MakeExprIdNode(name), expression, op));

    return RegisterNode(fn_assign);
}
FnNode *NodeManager::MakeReturnStmtNode(ExprNode *value) {
    FnNode *fn_node = new FnReturnStmt(value);
    return RegisterNode(fn_node);
}

FnNode *NodeManager::MakeIfStmtNode(const ExprNode *value) {
    FnNode *fn_node = new FnIfNode(value);
    return RegisterNode(fn_node);
}
FnNode *NodeManager::MakeElseStmtNode() {
    FnNode *fn_node = new FnElseNode();
    return RegisterNode(fn_node);
}
FnNode *NodeManager::MakeElifStmtNode(ExprNode *value) {
    FnNode *fn_node = new FnElifNode(value);
    return RegisterNode(fn_node);
}
FnNode *NodeManager::MakeFnNode(const SQLNodeType &type) {
    return RegisterNode(new FnNode(type));
}

FnNodeList *NodeManager::MakeFnListNode() {
    FnNodeList *fn_list = new FnNodeList();
    RegisterNode(fn_list);
    return fn_list;
}

FnIfBlock *NodeManager::MakeFnIfBlock(const FnIfNode *if_node,
                                      const FnNodeList *block) {
    ::fesql::node::FnIfBlock *if_block =
        new ::fesql::node::FnIfBlock(if_node, block);
    RegisterNode(if_block);
    return if_block;
}

FnElifBlock *NodeManager::MakeFnElifBlock(const FnElifNode *elif_node,
                                          const FnNodeList *block) {
    ::fesql::node::FnElifBlock *elif_block =
        new ::fesql::node::FnElifBlock(elif_node, block);
    RegisterNode(elif_block);
    return elif_block;
}
FnIfElseBlock *NodeManager::MakeFnIfElseBlock(const FnIfBlock *if_block,
                                              const FnElseBlock *else_block) {
    ::fesql::node::FnIfElseBlock *if_else_block =
        new ::fesql::node::FnIfElseBlock(if_block, else_block);
    RegisterNode(if_else_block);
    return if_else_block;
}
FnElseBlock *NodeManager::MakeFnElseBlock(const FnNodeList *block) {
    ::fesql::node::FnElseBlock *else_block =
        new ::fesql::node::FnElseBlock(block);
    RegisterNode(else_block);
    return else_block;
}

FnNode *NodeManager::MakeFnParaNode(const std::string &name,
                                    const TypeNode *para_type) {
    ::fesql::node::FnParaNode *para_node =
        new ::fesql::node::FnParaNode(name, para_type);
    return RegisterNode(para_node);
}

SQLNode *NodeManager::MakeKeyNode(SQLNodeList *key_list) {
    SQLNode *node_ptr = new SQLNode(kIndexKey, 0, 0);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeKeyNode(const std::string &key) {
    SQLNode *node_ptr = new SQLNode(kIndexKey, 0, 0);
    return RegisterNode(node_ptr);
}

SQLNode *NodeManager::MakeIndexKeyNode(const std::string &key) {
    SQLNode *node_ptr = new IndexKeyNode(key);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeIndexTsNode(const std::string &ts) {
    SQLNode *node_ptr = new IndexTsNode(ts);
    return RegisterNode(node_ptr);
}

SQLNode *NodeManager::MakeIndexTTLNode(ExprNode *ttl_expr) {
    SQLNode *node_ptr = new IndexTTLNode(ttl_expr);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeIndexTTLTypeNode(const std::string &ttl_type) {
    SQLNode *node_ptr = new IndexTTLTypeNode(ttl_type);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeIndexVersionNode(const std::string &version) {
    SQLNode *node_ptr = new IndexVersionNode(version);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeIndexVersionNode(const std::string &version,
                                           int count) {
    SQLNode *node_ptr = new IndexVersionNode(version, count);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeCmdNode(node::CmdType cmd_type) {
    SQLNode *node_ptr = new CmdNode(cmd_type);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeCmdNode(node::CmdType cmd_type,
                                  const std::string &arg) {
    CmdNode *node_ptr = new CmdNode(cmd_type);
    node_ptr->AddArg(arg);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeCmdNode(node::CmdType cmd_type,
                                  const std::string &index_name,
                                  const std::string &table_name) {
    CmdNode *node_ptr = new CmdNode(cmd_type);
    node_ptr->AddArg(index_name);
    node_ptr->AddArg(table_name);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeCreateIndexNode(const std::string &index_name,
                                          const std::string &table_name,
                                          ColumnIndexNode *index) {
    CreateIndexNode *node_ptr =
        new CreateIndexNode(index_name, table_name, index);
    return RegisterNode(node_ptr);
}
ExprNode *NodeManager::MakeAllNode(const std::string &relation_name) {
    return MakeAllNode(relation_name, "");
}

ExprNode *NodeManager::MakeAllNode(const std::string &relation_name,
                                   const std::string &db_name) {
    ExprNode *node_ptr = new AllNode(relation_name, db_name);
    return RegisterNode(node_ptr);
}

SQLNode *NodeManager::MakeInsertTableNode(const std::string &table_name,
                                          const ExprListNode *columns_expr,
                                          const ExprListNode *values) {
    if (nullptr == columns_expr) {
        InsertStmt *node_ptr = new InsertStmt(table_name, values->children_);
        return RegisterNode(node_ptr);
    } else {
        std::vector<std::string> column_names;
        for (auto expr : columns_expr->children_) {
            switch (expr->GetExprType()) {
                case kExprColumnRef: {
                    ColumnRefNode *column_ref =
                        dynamic_cast<ColumnRefNode *>(expr);
                    column_names.push_back(column_ref->GetColumnName());
                    break;
                }
                default: {
                    LOG(WARNING)
                        << "Can't not handle insert column name with type"
                        << ExprTypeName(expr->GetExprType());
                }
            }
        }
        InsertStmt *node_ptr =
            new InsertStmt(table_name, column_names, values->children_);
        return RegisterNode(node_ptr);
    }
}

DatasetNode *NodeManager::MakeDataset(const std::string &table) {
    DatasetNode *db = new DatasetNode(table);
    batch_plan_node_list_.push_back(db);
    return db;
}

MapNode *NodeManager::MakeMapNode(const NodePointVector &nodes) {
    MapNode *mn = new MapNode(nodes);
    batch_plan_node_list_.push_back(mn);
    return mn;
}

TypeNode *NodeManager::MakeTypeNode(fesql::node::DataType base) {
    TypeNode *node_ptr = new TypeNode(base);
    RegisterNode(node_ptr);
    return node_ptr;
}

TypeNode *NodeManager::MakeTypeNode(fesql::node::DataType base,
                                    fesql::node::DataType v1) {
    TypeNode *node_ptr = new TypeNode(base, v1);
    RegisterNode(node_ptr);
    return node_ptr;
}
TypeNode *NodeManager::MakeTypeNode(fesql::node::DataType base,
                                    fesql::node::DataType v1,
                                    fesql::node::DataType v2) {
    TypeNode *node_ptr = new TypeNode(base, v1, v2);
    RegisterNode(node_ptr);
    return node_ptr;
}

FnNode *NodeManager::MakeForInStmtNode(const std::string &var_name,
                                       const ExprNode *expression) {
    FnForInNode *node_ptr = new FnForInNode(var_name, expression);
    return RegisterNode(node_ptr);
}

FnForInBlock *NodeManager::MakeForInBlock(FnForInNode *for_in_node,
                                          FnNodeList *block) {
    FnForInBlock *node_ptr = new FnForInBlock(for_in_node, block);
    RegisterNode(node_ptr);
    return node_ptr;
}
PlanNode *NodeManager::MakeJoinNode(PlanNode *left, PlanNode *right,
                                    JoinType join_type,
                                    const OrderByNode *order_by,
                                    const ExprNode *condition) {
    node::JoinPlanNode *node_ptr =
        new JoinPlanNode(left, right, join_type, order_by, condition);
    return RegisterNode(node_ptr);
}
PlanNode *NodeManager::MakeSelectPlanNode(PlanNode *node) {
    node::QueryPlanNode *select_plan_ptr = new QueryPlanNode(node);
    return RegisterNode(select_plan_ptr);
}
PlanNode *NodeManager::MakeGroupPlanNode(PlanNode *node,
                                         const ExprListNode *by_list) {
    node::GroupPlanNode *node_ptr = new GroupPlanNode(node, by_list);
    return RegisterNode(node_ptr);
}
PlanNode *NodeManager::MakeProjectPlanNode(
    PlanNode *node, const std::string &table,
    const PlanNodeList &projection_list,
    const std::vector<std::pair<uint32_t, uint32_t>> &pos_mapping) {
    node::ProjectPlanNode *node_ptr =
        new ProjectPlanNode(node, table, projection_list, pos_mapping);
    return RegisterNode(node_ptr);
}
PlanNode *NodeManager::MakeLimitPlanNode(PlanNode *node, int limit_cnt) {
    node::LimitPlanNode *node_ptr = new LimitPlanNode(node, limit_cnt);
    return RegisterNode(node_ptr);
}
ProjectNode *NodeManager::MakeProjectNode(const int32_t pos,
                                          const std::string &name,
                                          const bool is_aggregation,
                                          node::ExprNode *expression) {
    node::ProjectNode *node_ptr =
        new ProjectNode(pos, name, is_aggregation, expression);
    RegisterNode(node_ptr);
    return node_ptr;
}
CreatePlanNode *NodeManager::MakeCreateTablePlanNode(
    std::string table_name, const NodePointVector &column_list) {
    node::CreatePlanNode *node_ptr =
        new CreatePlanNode(table_name, column_list);
    RegisterNode(node_ptr);
    return node_ptr;
}
CmdPlanNode *NodeManager::MakeCmdPlanNode(const CmdNode *node) {
    node::CmdPlanNode *node_ptr =
        new CmdPlanNode(node->GetCmdType(), node->GetArgs());
    RegisterNode(node_ptr);
    return node_ptr;
}
InsertPlanNode *NodeManager::MakeInsertPlanNode(const InsertStmt *node) {
    node::InsertPlanNode *node_ptr = new InsertPlanNode(node);
    RegisterNode(node_ptr);
    return node_ptr;
}
FuncDefPlanNode *NodeManager::MakeFuncPlanNode(const FnNodeFnDef *node) {
    node::FuncDefPlanNode *node_ptr = new FuncDefPlanNode(node);
    RegisterNode(node_ptr);
    return node_ptr;
}
ExprNode *NodeManager::MakeQueryExprNode(const QueryNode *query) {
    node::ExprNode *node_ptr = new QueryExpr(query);
    RegisterNode(node_ptr);
    return node_ptr;
}
PlanNode *NodeManager::MakeSortPlanNode(PlanNode *node,
                                        const OrderByNode *order_list) {
    node::SortPlanNode *node_ptr = new SortPlanNode(node, order_list);
    return RegisterNode(node_ptr);
}
PlanNode *NodeManager::MakeUnionPlanNode(PlanNode *left, PlanNode *right,
                                         const bool is_all) {
    node::UnionPlanNode *node_ptr = new UnionPlanNode(left, right, is_all);
    return RegisterNode(node_ptr);
}
PlanNode *NodeManager::MakeDistinctPlanNode(PlanNode *node) {
    node::DistinctPlanNode *node_ptr = new DistinctPlanNode(node);
    return RegisterNode(node_ptr);
}
SQLNode *NodeManager::MakeExplainNode(const QueryNode *query,
                                      ExplainType explain_type) {
    node::ExplainNode *node_ptr = new ExplainNode(query, explain_type);
    return RegisterNode(node_ptr);
}
ProjectNode *NodeManager::MakeAggProjectNode(const int32_t pos,
                                             const std::string &name,
                                             node::ExprNode *expression) {
    return MakeProjectNode(pos, name, true, expression);
}
ProjectNode *NodeManager::MakeRowProjectNode(const int32_t pos,
                                             const std::string &name,
                                             node::ExprNode *expression) {
    return MakeProjectNode(pos, name, false, expression);
}
ExprNode *NodeManager::MakeEqualCondition(const std::string &db1,
                                          const std::string &table1,
                                          const std::string &db2,
                                          const std::string &table2,
                                          const node::ExprListNode *expr_list) {
    if (nullptr == expr_list || expr_list->children_.empty()) {
        return nullptr;
    }
    auto iter = expr_list->children_.cbegin();
    auto condition =
        MakeBinaryExprNode(MakeExprFrom(*iter, table1, db1),
                           MakeExprFrom(*iter, table2, db2), node::kFnOpEq);
    iter++;
    for (; iter != expr_list->children_.cend(); iter++) {
        auto eq =
            MakeBinaryExprNode(MakeExprFrom(*iter, table1, db1),
                               MakeExprFrom(*iter, table2, db2), node::kFnOpEq);
        condition = MakeBinaryExprNode(condition, eq, kFnOpAnd);
    }

    return condition;
}

// TODO(chenjing): WindoNode, FrameNode 支持expr
ExprNode *NodeManager::MakeExprFrom(const node::ExprNode *expr,
                                    const std::string relation_name,
                                    const std::string db_name) {
    if (nullptr == expr) {
        return nullptr;
    }

    switch (expr->expr_type_) {
        case kExprList: {
            auto expr_list = dynamic_cast<const ExprListNode *>(expr);
            auto new_expr_list = MakeExprList();
            for (auto each : expr_list->children_) {
                new_expr_list->AddChild(
                    MakeExprFrom(each, relation_name, db_name));
            }
            return new_expr_list;
        }
        case kExprAll: {
            return MakeAllNode(relation_name, db_name);
        }
        case kExprColumnRef: {
            auto expr_column = dynamic_cast<const ColumnRefNode *>(expr);
            return MakeColumnRefNode(expr_column->GetColumnName(),
                                     relation_name, db_name);
        }
        case kExprBinary: {
            auto expr_binary_op = dynamic_cast<const BinaryExpr *>(expr);
            return MakeBinaryExprNode(MakeExprFrom(expr_binary_op->children_[0],
                                                   relation_name, db_name),
                                      MakeExprFrom(expr_binary_op->children_[1],
                                                   relation_name, db_name),
                                      expr_binary_op->GetOp());
        }
        case kExprUnary: {
            auto expr_unary_op = dynamic_cast<const UnaryExpr *>(expr);
            return MakeUnaryExprNode(MakeExprFrom(expr_unary_op->children_[0],
                                                  relation_name, db_name),
                                     expr_unary_op->GetOp());
        }
        case kExprOrder: {
            auto expr_order = dynamic_cast<const OrderByNode *>(expr);
            return MakeOrderByNode(
                dynamic_cast<node::ExprListNode *>(MakeExprFrom(
                    expr_order->order_by_, relation_name, db_name)),
                expr_order->is_asc_);
        }
        case kExprCall: {
            auto expr_call = dynamic_cast<const CallExprNode *>(expr);
            return MakeFuncNode(
                expr_call->GetFunctionName(),
                dynamic_cast<node::ExprListNode *>(
                    MakeExprFrom(expr_call->GetArgs(), relation_name, db_name)),
                expr_call->GetOver());
        }

        case kExprPrimary: {
            auto expr_primary = dynamic_cast<const ConstNode *>(expr);
            switch (expr_primary->GetDataType()) {
                case node::kInt16:
                    return MakeConstNode(expr_primary->GetSmallInt());
                case node::kInt32:
                    return MakeConstNode(expr_primary->GetInt());
                case node::kInt64:
                    return MakeConstNode(expr_primary->GetLong());
                case node::kFloat:
                    return MakeConstNode(expr_primary->GetFloat());
                case node::kDouble:
                    return MakeConstNode(expr_primary->GetDouble());
                case node::kVarchar:
                    return MakeConstNode(expr_primary->GetStr());
                case node::kTimestamp:
                    return MakeConstNode(expr_primary->GetLong(),
                                         expr_primary->GetDataType());
                default: {
                    LOG(WARNING)
                        << "fail to copy primary expr " +
                               node::DataTypeName(expr_primary->GetDataType());
                    return nullptr;
                }
            }
        }
        default: {
            LOG(WARNING) << "fail to copy expr " +
                                node::ExprTypeName(expr->expr_type_);
            return nullptr;
        }
    }
}

ExprNode *NodeManager::MakeBetweenExpr(ExprNode *expr, ExprNode *left,
                                       ExprNode *right) {
    ExprNode *node_ptr = new BetweenExpr(expr, left, right);
    return RegisterNode(node_ptr);
}
ExprNode *NodeManager::MakeAndExpr(ExprListNode *expr_list) {
    if (node::ExprListNullOrEmpty(expr_list)) {
        return nullptr;
    }
    ExprNode *left_node = expr_list->children_[0];
    if (1 == expr_list->children_.size()) {
        return left_node;
    }
    for (size_t i = 1; i < expr_list->children_.size(); i++) {
        left_node = MakeBinaryExprNode(left_node, expr_list->children_[i],
                                       node::kFnOpAnd);
    }
    return left_node;
}

// Build expression list from column sources
node::ExprListNode *NodeManager::BuildExprListFromSchemaSource(
    const vm::ColumnSourceList column_sources,
    const vm::SchemaSourceList &schema_souces) {
    node::ExprListNode *output = MakeExprList();
    if (nullptr == output) {
        LOG(WARNING)
            << "Fail Build Expr List from column sources: output is nullptr";
        return nullptr;
    }
    for (auto iter = column_sources.cbegin(); iter != column_sources.cend();
         iter++) {
        switch (iter->type()) {
            case vm::kSourceColumn: {
                auto schema_souce =
                    schema_souces.schema_source_list().at(iter->schema_idx());
                auto column = schema_souce.schema_->Get(iter->column_idx());
                output->AddChild(
                    MakeColumnRefNode(column.name(), schema_souce.table_name_));
                break;
            }
            case vm::kSourceConst: {
                output->AddChild(
                    const_cast<node::ConstNode *>(iter->const_value()));
                break;
            }
            default: {
                LOG(WARNING) << "Fail Gen Simple Project, invalid source type";
                return nullptr;
            }
        }
    }
    return output;
}
}  // namespace node
}  // namespace fesql
