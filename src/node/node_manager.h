// Copyright (C) 2019, 4paradigm
// memory_manager.h
//     负责FeSQL的基础元件（SQLNode, PlanNode)的创建和销毁
//     SQL的语法解析树、查询计划里面维护的只是这些节点的指针或者引用
// Author: chenjing
// Date: 2019/10/28

#ifndef SRC_NODE_NODE_MANAGER_H_
#define SRC_NODE_NODE_MANAGER_H_

#include <ctype.h>
#include <list>
#include <string>
#include <utility>
#include <vector>
#include "base/fe_object.h"
#include "node/batch_plan_node.h"
#include "node/plan_node.h"
#include "node/sql_node.h"
#include "node/type_node.h"
#include "vm/physical_op.h"

namespace fesql {
namespace node {

class NodeManager {
 public:
    NodeManager();

    ~NodeManager();

    int GetNodeListSize() {
        int node_size = node_list_.size();
        DLOG(INFO) << "GetNodeListSize: " << node_size;
        return node_size;
    }

    // Make xxxPlanNode
    //    PlanNode *MakePlanNode(const PlanType &type);
    PlanNode *MakeLeafPlanNode(const PlanType &type);
    PlanNode *MakeUnaryPlanNode(const PlanType &type);
    PlanNode *MakeBinaryPlanNode(const PlanType &type);
    PlanNode *MakeMultiPlanNode(const PlanType &type);
    PlanNode *MakeMergeNode(int column_size);
    WindowPlanNode *MakeWindowPlanNode(int w_id);
    ProjectListNode *MakeProjectListPlanNode(const WindowPlanNode *w,
                                             const bool need_agg);
    FilterPlanNode *MakeFilterPlanNode(PlanNode *node,
                                       const ExprNode *condition);

    ProjectNode *MakeRowProjectNode(const int32_t pos, const std::string &name,
                                    node::ExprNode *expression);
    ProjectNode *MakeAggProjectNode(const int32_t pos, const std::string &name,
                                    node::ExprNode *expression,
                                    node::FrameNode *frame);
    PlanNode *MakeTablePlanNode(const std::string &node);
    PlanNode *MakeJoinNode(PlanNode *left, PlanNode *right, JoinType join_type,
                           const OrderByNode *order_by,
                           const ExprNode *condition);
    // Make SQLxxx Node
    QueryNode *MakeSelectQueryNode(
        bool is_distinct, SQLNodeList *select_list_ptr,
        SQLNodeList *tableref_list_ptr, ExprNode *where_expr,
        ExprListNode *group_expr_list, ExprNode *having_expr,
        ExprNode *order_expr_list, SQLNodeList *window_list_ptr,
        SQLNode *limit_ptr);
    QueryNode *MakeUnionQueryNode(QueryNode *left, QueryNode *right,
                                  bool is_all);
    TableRefNode *MakeTableNode(const std::string &name,
                                const std::string &alias);
    TableRefNode *MakeJoinNode(const TableRefNode *left,
                               const TableRefNode *right, const JoinType type,
                               const ExprNode *condition,
                               const std::string alias);
    TableRefNode *MakeLastJoinNode(const TableRefNode *left,
                                   const TableRefNode *right,
                                   const ExprNode *order,
                                   const ExprNode *condition,
                                   const std::string alias);
    TableRefNode *MakeQueryRefNode(const QueryNode *sub_query,
                                   const std::string &alias);
    ExprNode *MakeCastNode(const node::DataType cast_type, ExprNode *expr);
    ExprNode *MakeWhenNode(ExprNode *when_expr, ExprNode *then_expr);
    ExprNode *MakeSimpleCaseWhenNode(ExprNode *case_expr,
                                     ExprListNode *when_list_expr,
                                     ExprNode *else_expr);
    ExprNode *MakeSearchedCaseWhenNode(ExprListNode *when_list_expr,
                                       ExprNode *else_expr);
    ExprNode *MakeTimeFuncNode(const TimeUnit time_unit, ExprListNode *args);
    ExprNode *MakeFuncNode(const std::string &name, ExprListNode *args,
                           const SQLNode *over);
    ExprNode *MakeFuncNode(FnDefNode *fn, ExprListNode *args,
                           const SQLNode *over);
    ExprNode *MakeFuncNode(const std::string &name,
                           const std::vector<ExprNode *> &args,
                           const SQLNode *over);
    ExprNode *MakeFuncNode(FnDefNode *fn, const std::vector<ExprNode *> &args,
                           const SQLNode *over);

    ExprNode *MakeQueryExprNode(const QueryNode *query);
    SQLNode *MakeWindowDefNode(const std::string &name);
    SQLNode *MakeWindowDefNode(ExprListNode *partitions, ExprNode *orders,
                               SQLNode *frame);
    SQLNode *MakeWindowDefNode(SQLNodeList *union_tables,
                               ExprListNode *partitions, ExprNode *orders,
                               SQLNode *frame, bool instance_not_in_window);
    WindowDefNode *MergeWindow(const WindowDefNode *w1,
                               const WindowDefNode *w2);
    ExprNode *MakeOrderByNode(const ExprListNode *node_ptr, const bool is_asc);
    SQLNode *MakeFrameExtent(SQLNode *start, SQLNode *end);
    SQLNode *MakeFrameBound(BoundType bound_type);
    SQLNode *MakeFrameBound(BoundType bound_type, ExprNode *offset);
    SQLNode *MakeFrameBound(BoundType bound_type, int64_t offset);
    SQLNode *MakeFrameNode(FrameType frame_type, SQLNode *node_ptr,
                           ExprNode *frame_size);
    SQLNode *MakeFrameNode(FrameType frame_type, SQLNode *node_ptr);
    SQLNode *MakeFrameNode(FrameType frame_type, SQLNode *node_ptr,
                           int64_t maxsize);
    SQLNode *MakeFrameNode(FrameType frame_type, FrameExtent *frame_range,
                           FrameExtent *frame_rows, int64_t maxsize);
    FrameNode *MergeFrameNode(const FrameNode *frame1, const FrameNode *frame2);
    SQLNode *MakeLimitNode(int count);

    SQLNode *MakeNameNode(const std::string &name);
    SQLNode *MakeInsertTableNode(const std::string &table_name,
                                 const ExprListNode *column_names,
                                 const ExprListNode *values);
    SQLNode *MakeCreateTableNode(bool op_if_not_exist,
                                 const std::string &table_name,
                                 SQLNodeList *column_desc_list,
                                 SQLNodeList *partition_meta_list);
    SQLNode *MakeColumnDescNode(const std::string &column_name,
                                const DataType data_type, bool op_not_null);
    SQLNode *MakeColumnIndexNode(SQLNodeList *keys, SQLNode *ts, SQLNode *ttl,
                                 SQLNode *version);
    SQLNode *MakeColumnIndexNode(SQLNodeList *index_item_list);
    SQLNode *MakeKeyNode(SQLNodeList *key_list);
    SQLNode *MakeKeyNode(const std::string &key);
    SQLNode *MakeIndexKeyNode(const std::string &key);
    SQLNode *MakeIndexTsNode(const std::string &ts);
    SQLNode *MakeIndexTTLNode(ExprListNode *ttl_expr);
    SQLNode *MakeIndexTTLTypeNode(const std::string &ttl_type);
    SQLNode *MakeIndexVersionNode(const std::string &version);
    SQLNode *MakeIndexVersionNode(const std::string &version, int count);

    SQLNode *MakeResTargetNode(ExprNode *node_ptr, const std::string &name);

    TypeNode *MakeTypeNode(fesql::node::DataType base);
    TypeNode *MakeTypeNode(fesql::node::DataType base,
                           const fesql::node::TypeNode *v1);
    TypeNode *MakeTypeNode(fesql::node::DataType base,
                           fesql::node::DataType v1);
    TypeNode *MakeTypeNode(fesql::node::DataType base, fesql::node::DataType v1,
                           fesql::node::DataType v2);
    OpaqueTypeNode *MakeOpaqueType(size_t bytes);
    RowTypeNode *MakeRowType(const vm::SchemaSourceList &schema_source);
    RowTypeNode *MakeRowType(const std::string &name, codec::Schema *schema);

    ExprNode *MakeColumnRefNode(const std::string &column_name,
                                const std::string &relation_name,
                                const std::string &db_name);
    ExprNode *MakeColumnRefNode(const std::string &column_name,
                                const std::string &relation_name);
    GetFieldExpr *MakeGetFieldExpr(ExprNode *input,
                                   const std::string &column_name,
                                   const std::string &relation_name);
    GetFieldExpr *MakeGetFieldExpr(ExprNode *input, size_t idx);

    CondExpr *MakeCondExpr(ExprNode *condition, ExprNode *left,
                           ExprNode *right);

    ExprNode *MakeBetweenExpr(ExprNode *expr, ExprNode *left, ExprNode *right);
    ExprNode *MakeBinaryExprNode(ExprNode *left, ExprNode *right,
                                 FnOperator op);
    ExprNode *MakeUnaryExprNode(ExprNode *left, FnOperator op);
    ExprNode *MakeExprFrom(const ExprNode *node,
                           const std::string relation_name,
                           const std::string db_name);
    ExprIdNode *MakeExprIdNode(const std::string &name);
    ExprIdNode *MakeUnresolvedExprId(const std::string &name);

    // Make Fn Node
    ExprNode *MakeConstNode(int16_t value);
    ExprNode *MakeConstNode(int value);
    ExprNode *MakeConstNode(int value, TTLType ttl_type);
    ExprNode *MakeConstNode(int64_t value, DataType unit);
    ExprNode *MakeConstNode(int64_t value);
    ExprNode *MakeConstNode(int64_t value, TTLType ttl_type);
    ExprNode *MakeConstNode(float value);
    ExprNode *MakeConstNode(double value);
    ExprNode *MakeConstNode(const std::string &value);
    ExprNode *MakeConstNode(const char *value);
    ExprNode *MakeConstNode();
    ExprNode *MakeConstNode(DataType type);
    ExprNode *MakeConstNodeINT16MAX();
    ExprNode *MakeConstNodeINT32MAX();
    ExprNode *MakeConstNodeINT64MAX();
    ExprNode *MakeConstNodeFLOATMAX();
    ExprNode *MakeConstNodeDOUBLEMAX();
    ExprNode *MakeConstNodeINT16MIN();
    ExprNode *MakeConstNodeINT32MIN();
    ExprNode *MakeConstNodeINT64MIN();
    ExprNode *MakeConstNodeFLOATMIN();
    ExprNode *MakeConstNodeDOUBLEMIN();
    ExprNode *MakeConstNodePlaceHolder();

    ExprNode *MakeAllNode(const std::string &relation_name);
    ExprNode *MakeAllNode(const std::string &relation_name,
                          const std::string &db_name);

    FnNode *MakeFnNode(const SQLNodeType &type);
    FnNodeList *MakeFnListNode();
    FnNode *MakeFnDefNode(const FnNode *header, FnNodeList *block);
    FnNode *MakeFnHeaderNode(const std::string &name, FnNodeList *plist,
                             const TypeNode *return_type);

    FnParaNode *MakeFnParaNode(const std::string &name,
                               const TypeNode *para_type);
    FnNode *MakeAssignNode(const std::string &name, ExprNode *expression);
    FnNode *MakeAssignNode(const std::string &name, ExprNode *expression,
                           const FnOperator op);
    FnNode *MakeReturnStmtNode(ExprNode *value);
    FnIfBlock *MakeFnIfBlock(FnIfNode *if_node, FnNodeList *block);
    FnElifBlock *MakeFnElifBlock(FnElifNode *elif_node, FnNodeList *block);
    FnIfElseBlock *MakeFnIfElseBlock(FnIfBlock *if_block,
                                     FnElseBlock *else_block);
    FnElseBlock *MakeFnElseBlock(FnNodeList *block);
    FnNode *MakeIfStmtNode(ExprNode *value);
    FnNode *MakeElifStmtNode(ExprNode *value);
    FnNode *MakeElseStmtNode();
    FnNode *MakeForInStmtNode(const std::string &var_name, ExprNode *value);

    SQLNode *MakeCmdNode(node::CmdType cmd_type);
    SQLNode *MakeCmdNode(node::CmdType cmd_type, const std::string &arg);
    SQLNode *MakeCmdNode(node::CmdType cmd_type, const std::string &index_name,
                         const std::string &table_name);
    SQLNode *MakeCreateIndexNode(const std::string &index_name,
                                 const std::string &table_name,
                                 ColumnIndexNode *index);
    // Make NodeList
    SQLNode *MakeExplainNode(const QueryNode *query,
                             node::ExplainType explain_type);
    SQLNodeList *MakeNodeList(SQLNode *node_ptr);
    SQLNodeList *MakeNodeList();

    ExprListNode *MakeExprList(ExprNode *node_ptr);
    ExprListNode *MakeExprList(ExprNode *node_ptr_1, ExprNode *node_ptr_2);
    ExprListNode *MakeExprList();

    DatasetNode *MakeDataset(const std::string &table);
    MapNode *MakeMapNode(const NodePointVector &nodes);
    node::FnForInBlock *MakeForInBlock(FnForInNode *for_in_node,
                                       FnNodeList *block);

    PlanNode *MakeSelectPlanNode(PlanNode *node);

    PlanNode *MakeGroupPlanNode(PlanNode *node, const ExprListNode *by_list);

    PlanNode *MakeProjectPlanNode(
        PlanNode *node, const std::string &table,
        const PlanNodeList &project_list,
        const std::vector<std::pair<uint32_t, uint32_t>> &pos_mapping);

    PlanNode *MakeLimitPlanNode(PlanNode *node, int limit_cnt);

    CreatePlanNode *MakeCreateTablePlanNode(
        const std::string &table_name, int replica_num,
        const NodePointVector &column_list,
        const NodePointVector &partition_meta_list);

    CreateProcedurePlanNode *MakeCreateProcedurePlanNode(
        const std::string &sp_name, const NodePointVector &input_parameter_list,
        const PlanNodeList &inner_plan_node_list);

    CmdPlanNode *MakeCmdPlanNode(const CmdNode *node);

    InsertPlanNode *MakeInsertPlanNode(const InsertStmt *node);

    FuncDefPlanNode *MakeFuncPlanNode(FnNodeFnDef *node);

    PlanNode *MakeRenamePlanNode(PlanNode *node, const std::string alias_name);

    PlanNode *MakeSortPlanNode(PlanNode *node, const OrderByNode *order_list);

    PlanNode *MakeUnionPlanNode(PlanNode *left, PlanNode *right,
                                const bool is_all);

    PlanNode *MakeDistinctPlanNode(PlanNode *node);

    node::ExprNode *MakeEqualCondition(const std::string &db1,
                                       const std::string &table1,
                                       const std::string &db2,
                                       const std::string &table2,
                                       const node::ExprListNode *expr_list);

    node::ExprNode *MakeAndExpr(ExprListNode *expr_list);
    node ::ExprListNode *BuildExprListFromSchemaSource(
        const vm::ColumnSourceList column_sources,
        const vm::SchemaSourceList &schema_souces);

    node::FrameNode *MergeFrameNodeWithCurrentHistoryFrame(FrameNode *frame1);

    ExternalFnDefNode *MakeExternalFnDefNode(
        const std::string &function_name, void *function_ptr,
        node::TypeNode *ret_type, bool ret_nullable,
        const std::vector<const node::TypeNode *> &arg_types,
        const std::vector<int> &arg_nullable, int variadic_pos,
        bool return_by_arg);

    ExternalFnDefNode *MakeUnresolvedFnDefNode(
        const std::string &function_name);

    UDFDefNode *MakeUDFDefNode(FnNodeFnDef *def);

    UDFByCodeGenDefNode *MakeUDFByCodeGenDefNode(
        const std::vector<const node::TypeNode *> &arg_types,
        const std::vector<int> &arg_nullable, const node::TypeNode *ret_type,
        bool ret_nullable);

    UDAFDefNode *MakeUDAFDefNode(const std::string &name,
                                 const std::vector<const TypeNode *> &arg_types,
                                 ExprNode *init, FnDefNode *update_func,
                                 FnDefNode *merge_func, FnDefNode *output_func);
    LambdaNode *MakeLambdaNode(const std::vector<ExprIdNode *> &args,
                               ExprNode *body);

    SQLNode *MakePartitionMetaNode(RoleType role_type,
                                   const std::string &endpoint);

    SQLNode *MakeReplicaNumNode(int num);

    SQLNode *MakeDistributionsNode(SQLNodeList *distribution_list);

    SQLNode *MakeCreateProcedureNode(const std::string &sp_name,
                                     SQLNodeList *input_parameter_list,
                                     SQLNode *inner_node);

    SQLNode *MakeInputParameterNode(bool is_constant,
                                    const std::string &column_name,
                                    DataType data_type);

    template <typename T>
    T *RegisterNode(T *node_ptr) {
        node_list_.push_back(node_ptr);
        SetNodeUniqueId(node_ptr);
        return node_ptr;
    }

 private:
    ProjectNode *MakeProjectNode(const int32_t pos, const std::string &name,
                                 const bool is_aggregation,
                                 node::ExprNode *expression,
                                 node::FrameNode *frame);

    void SetNodeUniqueId(ExprNode *node);
    void SetNodeUniqueId(TypeNode *node);
    void SetNodeUniqueId(PlanNode *node);
    void SetNodeUniqueId(vm::PhysicalOpNode *node);

    template <typename T>
    void SetNodeUniqueId(T *node) {
        node->SetNodeId(other_node_idx_counter_++);
    }

    std::list<base::FeBaseObject *> node_list_;

    // unique id counter for various types of node
    size_t expr_idx_counter_ = 1;
    size_t type_idx_counter_ = 1;
    size_t plan_idx_counter_ = 1;
    size_t physical_plan_idx_counter_ = 1;
    size_t other_node_idx_counter_ = 1;
    size_t exprid_idx_counter_ = 0;
};

}  // namespace node
}  // namespace fesql
#endif  // SRC_NODE_NODE_MANAGER_H_
