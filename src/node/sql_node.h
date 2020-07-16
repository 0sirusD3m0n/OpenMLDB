/*
 * parser/node.h
 * Copyright (C) 2019 chenjing <chenjing@4paradigm.com>
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

#ifndef SRC_NODE_SQL_NODE_H_
#define SRC_NODE_SQL_NODE_H_

#include <glog/logging.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "boost/algorithm/string.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/lexical_cast.hpp"
#include "node/expr_node.h"
#include "node/node_enum.h"

// fwd
namespace fesql::udf {
class LLVMUDFGenBase;
}

namespace fesql {
namespace node {

// Global methods
std::string NameOfSQLNodeType(const SQLNodeType &type);

inline const std::string CmdTypeName(const CmdType &type) {
    switch (type) {
        case kCmdShowDatabases:
            return "show databases";
        case kCmdShowTables:
            return "show tables";
        case kCmdUseDatabase:
            return "use database";
        case kCmdCreateDatabase:
            return "create database";
        case kCmdSource:
            return "create table";
        case kCmdCreateGroup:
            return "create group";
        case kCmdDescTable:
            return "desc table";
        case kCmdDropTable:
            return "drop table";
        case kCmdExit:
            return "exit";
        default:
            return "unknown cmd type";
    }
}

inline const std::string ExplainTypeName(const ExplainType &explain_type) {
    switch (explain_type) {
        case kExplainLogical:
            return "logical";
        case kExplainPhysical:
            return "physical";
        default: {
            return "Unknow";
        }
    }
}

inline const std::string JoinTypeName(const JoinType &type) {
    switch (type) {
        case kJoinTypeFull:
            return "FullJoin";
        case kJoinTypeLast:
            return "LastJoin";
        case kJoinTypeLeft:
            return "LeftJoin";
        case kJoinTypeRight:
            return "RightJoin";
        case kJoinTypeInner:
            return "InnerJoin";
        case kJoinTypeConcat:
            return "kJoinTypeConcat";
        default: {
            return "Unknow";
        }
    }
}
inline const std::string ExprOpTypeName(const FnOperator &op) {
    switch (op) {
        case kFnOpAdd:
            return "+";
        case kFnOpMinus:
            return "-";
        case kFnOpMulti:
            return "*";
        case kFnOpDiv:
            return "DIV";
        case kFnOpFDiv:
            return "/";
        case kFnOpMod:
            return "%";
        case kFnOpAnd:
            return "AND";
        case kFnOpOr:
            return "OR";
        case kFnOpXor:
            return "XOR";
        case kFnOpNot:
            return "NOT";
        case kFnOpEq:
            return "=";
        case kFnOpNeq:
            return "!=";
        case kFnOpGt:
            return ">";
        case kFnOpGe:
            return ">=";
        case kFnOpLt:
            return "<";
        case kFnOpLe:
            return "<=";
        case kFnOpAt:
            return "[]";
        case kFnOpDot:
            return ".";
        case kFnOpLike:
            return "LIKE";
        case kFnOpIn:
            return "IN";
        case kFnOpBracket:
            return "()";
        case kFnOpNone:
            return "NONE";
        default:
            return "UNKNOWN";
    }
}
inline const std::string TableRefTypeName(const TableRefType &type) {
    switch (type) {
        case kRefQuery:
            return "kQuery";
        case kRefJoin:
            return "kJoin";
        case kRefTable:
            return "kTable";
        default: {
            return "unknow";
        }
    }
}
inline const std::string QueryTypeName(const QueryType &type) {
    switch (type) {
        case kQuerySelect:
            return "kQuerySelect";
        case kQueryUnion:
            return "kQueryUnion";
        case kQuerySub:
            return "kQuerySub";
        default: {
            return "unknow";
        }
    }
}
inline const std::string ExprTypeName(const ExprType &type) {
    switch (type) {
        case kExprPrimary:
            return "primary";
        case kExprId:
            return "id";
        case kExprBinary:
            return "binary";
        case kExprUnary:
            return "unary";
        case kExprCall:
            return "function";
        case kExprCase:
            return "case";
        case kExprBetween:
            return "between";
        case kExprColumnRef:
            return "column ref";
        case kExprCast:
            return "cast";
        case kExprAll:
            return "all";
        case kExprStruct:
            return "struct";
        case kExprQuery:
            return "query";
        case kExprOrder:
            return "order";
        case kExprUnknow:
            return "unknow";
        default:
            return "unknown expr type";
    }
}

inline const std::string TimeUnitName(const TimeUnit &time_unit) {
    switch (time_unit) {
        case kTimeUnitYear:
            return "year";
        case kTimeUnitMonth:
            return "month";
        case kTimeUnitWeek:
            return "week";
        case kTimeUnitDay:
            return "dayofmonth";
        case kTimeUnitHour:
            return "hour";
        case kTimeUnitMinute:
            return "minute";
        case kTimeUnitSecond:
            return "second";
        case kTimeUnitMilliSecond:
            return "millisecond";
        case kTimeUnitMicroSecond:
            return "microsecond";
        default: {
            return "unknow";
        }
    }
    return "unknow";
}
inline const std::string FrameTypeName(const FrameType &type) {
    switch (type) {
        case fesql::node::kFrameRange:
            return "RANGE";
        case fesql::node::kFrameRows:
            return "ROWS";
        case fesql::node::kFrameRowsRange:
            return "ROWS_RANGE";
    }
}

inline const std::string BoundTypeName(const BoundType &type) {
    switch (type) {
        case fesql::node::kPrecedingUnbound:
            return "PRECEDING UNBOUND";
        case fesql::node::kPreceding:
            return "PRECEDING";
        case fesql::node::kCurrent:
            return "CURRENT";
        case fesql::node::kFollowing:
            return "FOLLOWING";
        case fesql::node::kFollowingUnbound:
            return "FOLLOWING UNBOUND";
        default:
            return "UNKNOW";
    }
    return "";
}
inline const std::string DataTypeName(const DataType &type) {
    switch (type) {
        case fesql::node::kBool:
            return "bool";
        case fesql::node::kInt16:
            return "int16";
        case fesql::node::kInt32:
            return "int32";
        case fesql::node::kInt64:
            return "int64";
        case fesql::node::kFloat:
            return "float";
        case fesql::node::kDouble:
            return "double";
        case fesql::node::kVarchar:
            return "string";
        case fesql::node::kTimestamp:
            return "timestamp";
        case fesql::node::kDate:
            return "date";
        case fesql::node::kList:
            return "list";
        case fesql::node::kMap:
            return "map";
        case fesql::node::kIterator:
            return "iterator";
        case fesql::node::kRow:
            return "row";
        case fesql::node::kSecond:
            return "second";
        case fesql::node::kMinute:
            return "minute";
        case fesql::node::kHour:
            return "hour";
        case fesql::node::kNull:
            return "null";
        case fesql::node::kVoid:
            return "void";
        case fesql::node::kPlaceholder:
            return "placeholder";
        case fesql::node::kOpaque:
            return "opaque";
        default:
            return "unknown";
    }
}

inline const std::string FnNodeName(const SQLNodeType &type) {
    switch (type) {
        case kFnDef:
            return "def";
        case kFnValue:
            return "value";
        case kFnAssignStmt:
            return "=";
        case kFnReturnStmt:
            return "return";
        case kFnPara:
            return "para";
        case kFnParaList:
            return "plist";
        case kFnList:
            return "funlist";
        default:
            return "unknowFn";
    }
}

class NodeBase {
 public:
    virtual ~NodeBase() {}
};

class SQLNode : public NodeBase {
 public:
    SQLNode(const SQLNodeType &type, uint32_t line_num, uint32_t location)
        : type_(type), line_num_(line_num), location_(location) {}

    virtual ~SQLNode() {}

    virtual void Print(std::ostream &output, const std::string &tab) const;

    const SQLNodeType GetType() const { return type_; }

    uint32_t GetLineNum() const { return line_num_; }

    uint32_t GetLocation() const { return location_; }

    virtual bool Equals(const SQLNode *node) const;
    friend std::ostream &operator<<(std::ostream &output, const SQLNode &thiz);
    SQLNodeType type_;

 private:
    uint32_t line_num_;
    uint32_t location_;
};

typedef std::vector<SQLNode *> NodePointVector;

class SQLNodeList : public NodeBase {
 public:
    SQLNodeList() {}
    virtual ~SQLNodeList() {}
    void PushBack(SQLNode *node_ptr) { list_.push_back(node_ptr); }
    const bool IsEmpty() const { return list_.empty(); }
    const int GetSize() const { return list_.size(); }
    const std::vector<SQLNode *> &GetList() const { return list_; }
    void Print(std::ostream &output, const std::string &tab) const;
    virtual bool Equals(const SQLNodeList *that) const;

 private:
    std::vector<SQLNode *> list_;
};

class TypeNode;

class ExprNode : public SQLNode {
 public:
    explicit ExprNode(ExprType expr_type)
        : SQLNode(kExpr, 0, 0), expr_type_(expr_type) {}
    ~ExprNode() {}
    void AddChild(ExprNode *expr) { children_.push_back(expr); }
    void SetChild(size_t idx, ExprNode *expr) { children_[idx] = expr; }
    ExprNode *GetChild(size_t idx) const { return children_[idx]; }
    uint32_t GetChildNum() const { return children_.size(); }

    const ExprType GetExprType() const { return expr_type_; }
    void PushBack(ExprNode *node_ptr) { children_.push_back(node_ptr); }

    std::vector<ExprNode *> children_;
    void Print(std::ostream &output, const std::string &org_tab) const override;
    virtual const std::string GetExprString() const;
    virtual const std::string GenerateExpressionName() const;
    virtual bool Equals(const ExprNode *that) const;

    const ExprType expr_type_;

    const TypeNode *GetOutputType() const { return output_type_; }
    void SetOutputType(const TypeNode *dtype) { output_type_ = dtype; }

    bool nullable() { return nullable_; }
    void SetNullable(bool flag) { nullable_ = flag; }

    /**
     * Infer static attributes of expression; Including
     *
     * - output type: abstract type node of expression output
     * - nullable: whether output can be null
     */
    virtual Status InferAttr(ExprAnalysisContext *ctx) {
        return Status(common::kUnSupport,
                      "Not implemented: " + GetExprString());
    }

 private:
    const TypeNode *output_type_ = nullptr;
    bool nullable_ = true;
};

class ExprListNode : public ExprNode {
 public:
    ExprListNode() : ExprNode(kExprList) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    const bool IsEmpty() const { return children_.empty(); }
    const std::string GetExprString() const;
};
class FnNode : public SQLNode {
 public:
    FnNode() : SQLNode(kFn, 0, 0), indent(0) {}
    explicit FnNode(SQLNodeType type) : SQLNode(type, 0, 0), indent(0) {}

 public:
    int32_t indent;
};
class FnNodeList : public FnNode {
 public:
    FnNodeList() : FnNode(kFnList) {}

    const std::vector<FnNode *> &GetChildren() const { return children; }

    void AddChild(FnNode *child) { children.push_back(child); }
    void Print(std::ostream &output, const std::string &org_tab) const;
    std::vector<FnNode *> children;
};

class OrderByNode : public ExprNode {
 public:
    explicit OrderByNode(const ExprListNode *order, bool is_asc)
        : ExprNode(kExprOrder), is_asc_(is_asc), order_by_(order) {}
    ~OrderByNode() {}

    void Print(std::ostream &output, const std::string &org_tab) const;
    const std::string GetExprString() const;
    virtual bool Equals(const ExprNode *that) const;

    const ExprListNode *order_by() const { return order_by_; }

    bool is_asc() const { return is_asc_; }

    const bool is_asc_;
    const ExprListNode *order_by_;
};
class TableRefNode : public SQLNode {
 public:
    explicit TableRefNode(TableRefType ref_type, std::string alias_table_name)
        : SQLNode(kTableRef, 0, 0),
          ref_type_(ref_type),
          alias_table_name_(alias_table_name) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;

    const TableRefType ref_type_;
    const std::string alias_table_name_;
};

class QueryNode : public SQLNode {
 public:
    explicit QueryNode(QueryType query_type)
        : SQLNode(node::kQuery, 0, 0), query_type_(query_type) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;
    const QueryType query_type_;
};

class TableNode : public TableRefNode {
 public:
    TableNode() : TableRefNode(kRefTable, ""), org_table_name_("") {}

    TableNode(const std::string &name, const std::string &alias)
        : TableRefNode(kRefTable, alias), org_table_name_(name) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;
    const std::string org_table_name_;
};

class QueryRefNode : public TableRefNode {
 public:
    QueryRefNode(const QueryNode *query, const std::string &alias)
        : TableRefNode(kRefQuery, alias), query_(query) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;
    const QueryNode *query_;
};

class JoinNode : public TableRefNode {
 public:
    JoinNode(const TableRefNode *left, const TableRefNode *right,
             const JoinType join_type, const OrderByNode *orders,
             const ExprNode *condition, const std::string &alias_name)
        : TableRefNode(kRefJoin, alias_name),
          left_(left),
          right_(right),
          join_type_(join_type),
          orders_(orders),
          condition_(condition) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;
    const TableRefNode *left_;
    const TableRefNode *right_;
    const JoinType join_type_;
    const node::OrderByNode *orders_;
    const ExprNode *condition_;
};
class SelectQueryNode : public QueryNode {
 public:
    SelectQueryNode(bool is_distinct, SQLNodeList *select_list,
                    SQLNodeList *tableref_list, ExprNode *where_expr,
                    ExprListNode *group_expr_list, ExprNode *having_expr,
                    OrderByNode *order_expr_list, SQLNodeList *window_list,
                    SQLNode *limit_ptr)
        : QueryNode(kQuerySelect),
          distinct_opt_(is_distinct),
          where_clause_ptr_(where_expr),
          group_clause_ptr_(group_expr_list),
          having_clause_ptr_(having_expr),
          order_clause_ptr_(order_expr_list),
          limit_ptr_(limit_ptr),
          select_list_(select_list),
          tableref_list_(tableref_list),
          window_list_(window_list) {}

    ~SelectQueryNode() {}

    // Getter and Setter
    const SQLNodeList *GetSelectList() const { return select_list_; }

    SQLNodeList *GetSelectList() { return select_list_; }

    const SQLNode *GetLimit() const { return limit_ptr_; }

    const SQLNodeList *GetTableRefList() const { return tableref_list_; }

    SQLNodeList *GetTableRefList() { return tableref_list_; }

    const SQLNodeList *GetWindowList() const { return window_list_; }

    SQLNodeList *GetWindowList() { return window_list_; }

    void SetLimit(SQLNode *limit) { limit_ptr_ = limit; }

    int GetDistinctOpt() const { return distinct_opt_; }
    // Print
    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;

    const bool distinct_opt_;
    const ExprNode *where_clause_ptr_;
    const ExprListNode *group_clause_ptr_;
    const ExprNode *having_clause_ptr_;
    const OrderByNode *order_clause_ptr_;
    const SQLNode *limit_ptr_;

 private:
    SQLNodeList *select_list_;
    SQLNodeList *tableref_list_;
    SQLNodeList *window_list_;
    void PrintSQLNodeList(std::ostream &output, const std::string &tab,
                          SQLNodeList *list, const std::string &name,
                          bool last_item) const;
};

class UnionQueryNode : public QueryNode {
 public:
    UnionQueryNode(const QueryNode *left, const QueryNode *right, bool is_all)
        : QueryNode(kQueryUnion), left_(left), right_(right), is_all_(is_all) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;
    const QueryNode *left_;
    const QueryNode *right_;
    const bool is_all_;
};

class NameNode : public SQLNode {
 public:
    NameNode() : SQLNode(kName, 0, 0), name_("") {}
    explicit NameNode(const std::string &name)
        : SQLNode(kName, 0, 0), name_(name) {}
    ~NameNode() {}

    std::string GetName() const { return name_; }
    virtual bool Equals(const SQLNode *node) const;

 private:
    std::string name_;
};

class ConstNode : public ExprNode {
    struct FeDate {
        int32_t year = -1;
        int32_t month = -1;
        int32_t day = -1;
    };

 public:
    ConstNode() : ExprNode(kExprPrimary), data_type_(fesql::node::kNull) {}
    explicit ConstNode(DataType data_type)
        : ExprNode(kExprPrimary), data_type_(data_type) {}
    explicit ConstNode(int16_t val)
        : ExprNode(kExprPrimary), data_type_(fesql::node::kInt16) {
        val_.vsmallint = val;
    }
    explicit ConstNode(int val)
        : ExprNode(kExprPrimary), data_type_(fesql::node::kInt32) {
        val_.vint = val;
    }
    explicit ConstNode(int64_t val)
        : ExprNode(kExprPrimary), data_type_(fesql::node::kInt64) {
        val_.vlong = val;
    }
    explicit ConstNode(float val)
        : ExprNode(kExprPrimary), data_type_(fesql::node::kFloat) {
        val_.vfloat = val;
    }

    explicit ConstNode(double val)
        : ExprNode(kExprPrimary), data_type_(fesql::node::kDouble) {
        val_.vdouble = val;
    }

    explicit ConstNode(const char *val)
        : ExprNode(kExprPrimary), data_type_(fesql::node::kVarchar) {
        val_.vstr = strdup(val);
    }

    explicit ConstNode(const std::string &val)
        : ExprNode(kExprPrimary), data_type_(fesql::node::kVarchar) {
        val_.vstr = strdup(val.c_str());
    }

    explicit ConstNode(const ConstNode &that)
        : ExprNode(kExprPrimary), data_type_(that.data_type_) {
        if (kVarchar == that.data_type_) {
            val_.vstr = strdup(that.val_.vstr);
        } else {
            val_ = that.val_;
        }
    }

    ConstNode(int64_t val, DataType time_type)
        : ExprNode(kExprPrimary), data_type_(time_type) {
        val_.vlong = val;
    }

    ~ConstNode() {
        if (data_type_ == fesql::node::kVarchar) {
            delete val_.vstr;
        }
    }
    void Print(std::ostream &output, const std::string &org_tab) const;

    virtual bool Equals(const ExprNode *node) const;

    const bool IsNull() const { return kNull == data_type_; }
    const bool IsPlaceholder() const { return kPlaceholder == data_type_; }
    const std::string GetExprString() const;
    int16_t GetSmallInt() const { return val_.vsmallint; }

    int GetInt() const { return val_.vint; }

    int64_t GetLong() const { return val_.vlong; }

    const char *GetStr() const { return val_.vstr; }

    float GetFloat() const { return val_.vfloat; }

    double GetDouble() const { return val_.vdouble; }

    DataType GetDataType() const { return data_type_; }

    int64_t GetMillis() const {
        switch (data_type_) {
            case fesql::node::kDay:
                return 86400000 * val_.vlong;
            case fesql::node::kHour:
                return 3600000 * val_.vlong;
            case fesql::node::kMinute:
                return 60000 * val_.vlong;
            case fesql::node::kSecond:
                return 1000 * val_.vlong;
            default: {
                LOG(WARNING)
                    << "error occur when get milli second from wrong type "
                    << DataTypeName(data_type_);
                return -1;
            }
        }
    }

    const int32_t GetAsInt32() const {
        switch (data_type_) {
            case kInt32:
                return static_cast<int32_t>(val_.vint);
            case kInt16:
                return static_cast<int32_t>(val_.vsmallint);
            case kInt64:
                return static_cast<int64_t>(val_.vlong);
            case kFloat:
                return static_cast<int64_t>(val_.vfloat);
            case kDouble:
                return static_cast<int64_t>(val_.vdouble);
            default: {
                return 0;
            }
        }
    }

    const int16_t GetAsInt16() const {
        switch (data_type_) {
            case kInt32:
                return static_cast<int16_t>(val_.vint);
            case kInt16:
                return static_cast<int16_t>(val_.vsmallint);
            case kInt64:
                return static_cast<int16_t>(val_.vlong);
            case kFloat:
                return static_cast<int16_t>(val_.vfloat);
            case kDouble:
                return static_cast<int16_t>(val_.vdouble);
            default: {
                return 0;
            }
        }
    }

    const int64_t GetAsInt64() const {
        switch (data_type_) {
            case kInt32:
                return static_cast<int64_t>(val_.vint);
            case kInt16:
                return static_cast<int64_t>(val_.vsmallint);
            case kInt64:
                return static_cast<int64_t>(val_.vlong);
            case kFloat:
                return static_cast<int64_t>(val_.vfloat);
            case kDouble:
                return static_cast<int64_t>(val_.vdouble);
            default: {
                return 0;
            }
        }
    }

    const float GetAsFloat() const {
        switch (data_type_) {
            case kInt32:
                return static_cast<float>(val_.vint);
            case kInt16:
                return static_cast<float>(val_.vsmallint);
            case kInt64:
                return static_cast<float>(val_.vlong);
            case kFloat:
                return static_cast<float>(val_.vfloat);
            case kDouble:
                return static_cast<float>(val_.vdouble);
            default: {
                return 0.0;
            }
        }
    }

    const bool GetAsDate(int32_t *year, int32_t *month, int32_t *day) const {
        switch (data_type_) {
            case kVarchar: {
                std::string date_str(val_.vstr);
                std::vector<std::string> date_vec;
                boost::split(date_vec, date_str, boost::is_any_of("-"),
                             boost::token_compress_on);
                if (date_vec.empty()) {
                    LOG(WARNING) << "Invalid Date Format";
                    return false;
                }
                *year = boost::lexical_cast<int32_t>(date_vec[0]);
                *month = boost::lexical_cast<int32_t>(date_vec[1]);
                *day = boost::lexical_cast<int32_t>(date_vec[2]);
                return true;
            }
            default: {
                LOG(WARNING) << "Invalid data type for date";
                return false;
            }
        }
    }
    const double GetAsDouble() const {
        switch (data_type_) {
            case kInt32:
                return static_cast<double>(val_.vint);
            case kInt16:
                return static_cast<double>(val_.vsmallint);
            case kInt64:
                return static_cast<double>(val_.vlong);
            case kFloat:
                return static_cast<double>(val_.vfloat);
            case kDouble:
                return static_cast<double>(val_.vdouble);
            default: {
                return 0.0;
            }
        }
    }

    Status InferAttr(ExprAnalysisContext *ctx) override;

 private:
    DataType data_type_;
    union {
        int16_t vsmallint;
        int vint;         /* machine integer */
        int64_t vlong;    /* machine integer */
        const char *vstr; /* string */
        float vfloat;
        double vdouble;
    } val_;
};
class LimitNode : public SQLNode {
 public:
    LimitNode() : SQLNode(kLimit, 0, 0), limit_cnt_(0) {}

    explicit LimitNode(int limit_cnt)
        : SQLNode(kLimit, 0, 0), limit_cnt_(limit_cnt) {}

    int GetLimitCount() const { return limit_cnt_; }

    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;

 private:
    int limit_cnt_;
};

class FrameBound : public SQLNode {
 public:
    FrameBound()
        : SQLNode(kFrameBound, 0, 0),
          bound_type_(kPreceding),
          is_time_offset_(false),
          offset_(0) {}

    explicit FrameBound(BoundType bound_type)
        : SQLNode(kFrameBound, 0, 0),
          bound_type_(bound_type),
          is_time_offset_(false),
          offset_(0) {}

    FrameBound(BoundType bound_type, int64_t offset, bool is_time_offet)
        : SQLNode(kFrameBound, 0, 0),
          bound_type_(bound_type),
          is_time_offset_(is_time_offet),
          offset_(offset) {}

    ~FrameBound() {}

    void Print(std::ostream &output, const std::string &org_tab) const {
        SQLNode::Print(output, org_tab);
        const std::string tab = org_tab + INDENT + SPACE_ED;
        std::string space = org_tab + INDENT + INDENT;
        output << "\n";
        output << tab << SPACE_ST << "bound: " << BoundTypeName(bound_type_)
               << "\n";

        if (kFollowing == bound_type_ || kPreceding == bound_type_) {
            output << space << offset_;
        }
    }
    const std::string GetExprString() const {
        switch (bound_type_) {
            case node::kCurrent:
                return "0";
            case node::kFollowing:
            case node::kPreceding:
                return std::to_string(GetSignedOffset());
            case node::kPrecedingUnbound:
            case node::kFollowingUnbound:
                return "UNBOUND";
        }
    }

    BoundType bound_type() const { return bound_type_; }
    const bool is_time_offset() const { return is_time_offset_; }
    int64_t GetOffset() const { return offset_; }
    int64_t GetSignedOffset() const {
        switch (bound_type_) {
            case node::kCurrent:
                return 0;
            case node::kFollowing:
                return offset_;
            case node::kPreceding:
                return -1 * offset_;
            case node::kPrecedingUnbound:
                return INT64_MIN;
            case node::kFollowingUnbound:
                return INT64_MAX;
        }
        return 0;
    }
    virtual bool Equals(const SQLNode *node) const;
    static int Compare(const FrameBound *bound1, const FrameBound *bound2);

 private:
    BoundType bound_type_;
    bool is_time_offset_;
    int64_t offset_;
};

class FrameExtent : public SQLNode {
 public:
    explicit FrameExtent(FrameBound *start)
        : SQLNode(kFrameExtent, 0, 0), start_(start) {}

    FrameExtent(FrameBound *start, FrameBound *end)
        : SQLNode(kFrameExtent, 0, 0), start_(start), end_(end) {}

    ~FrameExtent() {}

    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;
    FrameBound *start() const { return start_; }
    FrameBound *end() const { return end_; }
    const std::string GetExprString() const {
        std::string str = "[";
        if (nullptr == start_) {
            str.append("UNBOUND");
        } else {
            str.append(start_->GetExprString());
        }
        str.append(",");
        if (nullptr == end_) {
            str.append("UNBOUND");
        } else {
            str.append(end_->GetExprString());
        }

        str.append("]");
        return str;
    }

 private:
    FrameBound *start_;
    FrameBound *end_;
};
class FrameNode : public SQLNode {
 public:
    FrameNode(FrameType frame_type, FrameExtent *frame_range,
              FrameExtent *frame_rows, int64_t frame_maxsize)
        : SQLNode(kFrames, 0, 0),
          frame_type_(frame_type),
          frame_range_(frame_range),
          frame_rows_(frame_rows),
          frame_maxsize_(frame_maxsize) {}
    ~FrameNode() {}
    FrameType frame_type() const { return frame_type_; }
    void set_frame_type(FrameType frame_type) { frame_type_ = frame_type; }
    FrameExtent *frame_range() const { return frame_range_; }
    FrameExtent *frame_rows() const { return frame_rows_; }
    int64_t frame_maxsize() const { return frame_maxsize_; }
    int64_t GetHistoryRangeStart() const {
        if (nullptr == frame_rows_ && nullptr == frame_range_) {
            return INT64_MIN;
        }
        if (nullptr == frame_rows_) {
            return nullptr == frame_range_ || nullptr == frame_range_->start()
                       ? INT64_MIN
                       : frame_range_->start()->GetSignedOffset() > 0
                             ? 0
                             : frame_range_->start()->GetSignedOffset();
        } else {
            return nullptr == frame_range_ || nullptr == frame_range_->start()
                       ? 0
                       : frame_range_->start()->GetSignedOffset() > 0
                             ? 0
                             : frame_range_->start()->GetSignedOffset();
        }
    }
    int64_t GetHistoryRangeEnd() const {
        return nullptr == frame_range_ || nullptr == frame_range_->end()
                   ? 0
                   : frame_range_->end()->GetSignedOffset() > 0
                         ? 0
                         : frame_range_->end()->GetSignedOffset();
    }

    int64_t GetHistoryRowsStart() const {
        if (nullptr == frame_rows_ && nullptr == frame_range_) {
            return INT64_MIN;
        }
        if (nullptr == frame_range_) {
            return nullptr == frame_rows_ || nullptr == frame_rows_->start()
                       ? INT64_MIN
                       : frame_rows_->start()->GetSignedOffset() > 0
                             ? 0
                             : frame_rows_->start()->GetSignedOffset();
        } else {
            return nullptr == frame_rows_ || nullptr == frame_rows_->start()
                       ? 0
                       : frame_rows_->start()->GetSignedOffset() > 0
                             ? 0
                             : frame_rows_->start()->GetSignedOffset();
        }
    }
    int64_t GetHistoryRowsEnd() const {
        if (nullptr == frame_rows_ && nullptr == frame_range_) {
            return INT64_MIN;
        }
        if (nullptr == frame_range_) {
            return nullptr == frame_rows_ || nullptr == frame_rows_->start()
                       ? INT64_MIN
                       : frame_rows_->end()->GetSignedOffset();
        } else {
            return nullptr == frame_rows_ || nullptr == frame_rows_->start()
                       ? 0
                       : frame_rows_->end()->GetSignedOffset() > 0
                             ? 0
                             : frame_rows_->end()->GetSignedOffset();
        }
    }
    inline const bool IsHistoryFrame() const {
        switch (frame_type_) {
            case kFrameRows:
                return GetHistoryRowsEnd() < 0;
            case kFrameRange: {
                return GetHistoryRangeEnd() < 0;
            }
            case kFrameRowsRange: {
                return GetHistoryRangeEnd() < 0;
            }
        }
    }
    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;
    const std::string GetExprString() const;
    bool CanMergeWith(const FrameNode *that) const;

 private:
    FrameType frame_type_;
    FrameExtent *frame_range_;
    FrameExtent *frame_rows_;
    int64_t frame_maxsize_;
};
class WindowDefNode : public SQLNode {
 public:
    WindowDefNode()
        : SQLNode(kWindowDef, 0, 0),
          instance_not_in_window_(false),
          window_name_(""),
          frame_ptr_(NULL),
          union_tables_(nullptr),
          partitions_(nullptr),
          orders_(nullptr) {}

    ~WindowDefNode() {}

    const std::string &GetName() const { return window_name_; }

    void SetName(const std::string &name) { window_name_ = name; }

    ExprListNode *GetPartitions() const { return partitions_; }

    OrderByNode *GetOrders() const { return orders_; }

    FrameNode *GetFrame() const { return frame_ptr_; }

    void SetPartitions(ExprListNode *partitions) { partitions_ = partitions; }
    void SetOrders(OrderByNode *orders) { orders_ = orders; }
    void SetFrame(FrameNode *frame) { frame_ptr_ = frame; }

    SQLNodeList *union_tables() const { return union_tables_; }
    void set_union_tables(SQLNodeList *union_table) {
        union_tables_ = union_table;
    }

    const bool instance_not_in_window() const {
        return instance_not_in_window_;
    }
    void set_instance_not_in_window(bool instance_not_in_window) {
        instance_not_in_window_ = instance_not_in_window;
    }
    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *that) const;
    bool CanMergeWith(const WindowDefNode *that) const;

 private:
    bool instance_not_in_window_;
    std::string window_name_;   /* window's own name */
    FrameNode *frame_ptr_;      /* expression for starting bound, if any */
    SQLNodeList *union_tables_; /* union other table in window */
    ExprListNode *partitions_;  /* PARTITION BY expression list */
    OrderByNode *orders_;       /* ORDER BY (list of SortBy) */
};

class AllNode : public ExprNode {
 public:
    AllNode() : ExprNode(kExprAll), relation_name_("") {}

    explicit AllNode(const std::string &relation_name)
        : ExprNode(kExprAll), relation_name_(relation_name) {}
    explicit AllNode(const std::string &relation_name,
                     const std::string &db_name)
        : ExprNode(kExprAll),
          relation_name_(relation_name),
          db_name_(db_name) {}

    std::string GetRelationName() const { return relation_name_; }
    std::string GetDBName() const { return db_name_; }

    void SetRelationName(const std::string &relation_name) {
        relation_name_ = relation_name;
    }
    const std::string GetExprString() const;
    virtual bool Equals(const ExprNode *that) const;

 private:
    std::string relation_name_;
    std::string db_name_;
};

class FnDefNode : public SQLNode {
 public:
    explicit FnDefNode(const SQLNodeType &type) : SQLNode(type, 0, 0) {}
    virtual const TypeNode *GetReturnType() const { return nullptr; }
    virtual size_t GetArgSize() const { return 0; }
    virtual const TypeNode *GetArgType(size_t i) const { return nullptr; }
    virtual const std::string GetSimpleName() const = 0;
    virtual bool Validate(
        const std::vector<const TypeNode *> &arg_types) const = 0;
};
class CastExprNode : public ExprNode {
 public:
    explicit CastExprNode(const node::DataType cast_type, node::ExprNode *expr)
        : ExprNode(kExprCast), cast_type_(cast_type) {
        this->AddChild(expr);
    }

    ~CastExprNode() {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    const std::string GetExprString() const;
    virtual bool Equals(const ExprNode *that) const;
    ExprNode *expr() const { return GetChild(0); }
    DataType cast_type_;

    Status InferAttr(ExprAnalysisContext *ctx) override;
};
class CallExprNode : public ExprNode {
 public:
    explicit CallExprNode(const FnDefNode *fn_def, ExprListNode *args,
                          const WindowDefNode *over)
        : ExprNode(kExprCall), fn_def_(fn_def), over_(over) {
        if (args != nullptr) {
            for (size_t i = 0; i < args->GetChildNum(); ++i) {
                this->AddChild(args->GetChild(i));
            }
        }
    }

    ~CallExprNode() {}

    void Print(std::ostream &output, const std::string &org_tab) const;
    const std::string GetExprString() const;
    virtual bool Equals(const ExprNode *that) const;

    const WindowDefNode *GetOver() const { return over_; }

    void SetOver(WindowDefNode *over) { over_ = over; }

    bool IsRowWise() const { return is_row_wise_; }

    void SetRowWise(bool flag) { is_row_wise_ = flag; }

    const FnDefNode *GetFnDef() const { return fn_def_; }

    Status InferAttr(ExprAnalysisContext *ctx) override;

 private:
    // bool is_agg_;
    // const std::string function_name_;
    const FnDefNode *fn_def_;
    const WindowDefNode *over_;

    // TODO(xxx): maybe remove this if high order expression supported
    bool is_row_wise_ = false;
};

class QueryExpr : public ExprNode {
 public:
    explicit QueryExpr(const QueryNode *query)
        : ExprNode(kExprQuery), query_(query) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const ExprNode *node) const;
    const std::string GetExprString() const;

    const QueryNode *query_;
};

class BinaryExpr : public ExprNode {
 public:
    BinaryExpr() : ExprNode(kExprBinary) {}
    explicit BinaryExpr(FnOperator op) : ExprNode(kExprBinary), op_(op) {}
    FnOperator GetOp() const { return op_; }
    void Print(std::ostream &output, const std::string &org_tab) const;
    const std::string GetExprString() const;
    virtual bool Equals(const ExprNode *node) const;

 private:
    FnOperator op_;
};
class UnaryExpr : public ExprNode {
 public:
    UnaryExpr() : ExprNode(kExprUnary) {}
    explicit UnaryExpr(FnOperator op) : ExprNode(kExprUnary), op_(op) {}
    FnOperator GetOp() const { return op_; }
    void Print(std::ostream &output, const std::string &org_tab) const override;
    const std::string GetExprString() const;
    virtual bool Equals(const ExprNode *node) const;

 private:
    FnOperator op_;
};
class ExprIdNode : public ExprNode {
 public:
    ExprIdNode() : ExprNode(kExprId) {}
    explicit ExprIdNode(const std::string &name, size_t id)
        : ExprNode(kExprId), name_(name), id_(id) {}
    const std::string &GetName() const { return name_; }
    int64_t GetId() const { return id_; }
    void SetId(int64_t id) { id_ = id; }
    void Print(std::ostream &output, const std::string &org_tab) const override;
    const std::string GetExprString() const override;
    bool Equals(const ExprNode *node) const override;

    Status InferAttr(ExprAnalysisContext *ctx) override;

    // Since lambda argument should be unique identified,
    // a static count value is maintained here. Currently
    // we can not put it in node_manager because there is
    // no ensurement of unique node_manager instance.
    // TODO(xxx): are all exprs unique identified neccesary?
    static int64_t expr_id_cnt_;
    static int64_t GetNewId() { return expr_id_cnt_++; }

    bool IsResolved() const { return id_ >= 0; }

 private:
    std::string name_;
    int64_t id_;
};

class ColumnRefNode : public ExprNode {
 public:
    ColumnRefNode()
        : ExprNode(kExprColumnRef), column_name_(""), relation_name_("") {}

    ColumnRefNode(const std::string &column_name,
                  const std::string &relation_name)
        : ExprNode(kExprColumnRef),
          column_name_(column_name),
          relation_name_(relation_name),
          db_name_("") {}

    ColumnRefNode(const std::string &column_name,
                  const std::string &relation_name, const std::string &db_name)
        : ExprNode(kExprColumnRef),
          column_name_(column_name),
          relation_name_(relation_name),
          db_name_(db_name) {}

    std::string GetDBName() const { return db_name_; }

    void SetDBName(const std::string &db_name) { db_name_ = db_name; }

    std::string GetRelationName() const { return relation_name_; }

    void SetRelationName(const std::string &relation_name) {
        relation_name_ = relation_name;
    }

    std::string GetColumnName() const { return column_name_; }

    void SetColumnName(const std::string &column_name) {
        column_name_ = column_name;
    }

    static ColumnRefNode *CastFrom(ExprNode *node);
    void Print(std::ostream &output, const std::string &org_tab) const;
    const std::string GetExprString() const;
    const std::string GenerateExpressionName() const;
    virtual bool Equals(const ExprNode *node) const;

    Status InferAttr(ExprAnalysisContext *ctx) override;

 private:
    std::string column_name_;
    std::string relation_name_;
    std::string db_name_;
};

class GetFieldExpr : public ExprNode {
 public:
    GetFieldExpr(ExprNode *input, const std::string &column_name,
                 const std::string &relation_name)
        : ExprNode(kExprGetField),
          column_name_(column_name),
          relation_name_(relation_name) {
        this->AddChild(input);
    }

    std::string GetRelationName() const { return relation_name_; }
    std::string GetColumnName() const { return column_name_; }
    ExprNode *GetRow() const { return GetChild(0); }

    void Print(std::ostream &output, const std::string &org_tab) const;
    const std::string GetExprString() const;
    const std::string GenerateExpressionName() const;
    virtual bool Equals(const ExprNode *node) const;

    Status InferAttr(ExprAnalysisContext *ctx) override;

 private:
    std::string column_name_;
    std::string relation_name_;
};

class BetweenExpr : public ExprNode {
 public:
    BetweenExpr(ExprNode *expr, ExprNode *left, ExprNode *right)
        : ExprNode(kExprBetween), expr_(expr), left_(left), right_(right) {}
    ~BetweenExpr() {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    const std::string GetExprString() const;
    virtual bool Equals(const ExprNode *node) const;

    ExprNode *expr_;
    ExprNode *left_;
    ExprNode *right_;
};
class ResTarget : public SQLNode {
 public:
    ResTarget() : SQLNode(kResTarget, 0, 0), name_(""), val_(nullptr) {}

    ResTarget(const std::string &name, ExprNode *val)
        : SQLNode(kResTarget, 0, 0), name_(name), val_(val) {}

    ~ResTarget() {}

    std::string GetName() const { return name_; }

    ExprNode *GetVal() const { return val_; }

    void Print(std::ostream &output, const std::string &org_tab) const;
    virtual bool Equals(const SQLNode *node) const;

 private:
    std::string name_; /* column name or NULL */
    ExprNode *val_;    /* the value expression to compute or assign */
    NodePointVector indirection_; /* subscripts, field names, and '*', or NIL */
};

class ColumnDefNode : public SQLNode {
 public:
    ColumnDefNode()
        : SQLNode(kColumnDesc, 0, 0), column_name_(""), column_type_() {}
    ColumnDefNode(const std::string &name, const DataType &data_type,
                  bool op_not_null)
        : SQLNode(kColumnDesc, 0, 0),
          column_name_(name),
          column_type_(data_type),
          op_not_null_(op_not_null) {}
    ~ColumnDefNode() {}

    std::string GetColumnName() const { return column_name_; }

    DataType GetColumnType() const { return column_type_; }

    bool GetIsNotNull() const { return op_not_null_; }
    void Print(std::ostream &output, const std::string &org_tab) const;

 private:
    std::string column_name_;
    DataType column_type_;
    bool op_not_null_;
};

class InsertStmt : public SQLNode {
 public:
    InsertStmt(const std::string &table_name,
               const std::vector<std::string> &columns,
               const std::vector<ExprNode *> &values)
        : SQLNode(kInsertStmt, 0, 0),
          table_name_(table_name),
          columns_(columns),
          values_(values),
          is_all_(false) {}

    InsertStmt(const std::string &table_name,
               const std::vector<ExprNode *> &values)
        : SQLNode(kInsertStmt, 0, 0),
          table_name_(table_name),
          values_(values),
          is_all_(true) {}
    void Print(std::ostream &output, const std::string &org_tab) const;

    const std::string table_name_;
    const std::vector<std::string> columns_;
    const std::vector<ExprNode *> values_;
    const bool is_all_;
};
class CreateStmt : public SQLNode {
 public:
    CreateStmt()
        : SQLNode(kCreateStmt, 0, 0),
          table_name_(""),
          op_if_not_exist_(false) {}

    CreateStmt(const std::string &table_name, bool op_if_not_exist)
        : SQLNode(kCreateStmt, 0, 0),
          table_name_(table_name),
          op_if_not_exist_(op_if_not_exist) {}

    ~CreateStmt() {}

    NodePointVector &GetColumnDefList() { return column_desc_list_; }
    const NodePointVector &GetColumnDefList() const {
        return column_desc_list_;
    }

    std::string GetTableName() const { return table_name_; }

    bool GetOpIfNotExist() const { return op_if_not_exist_; }

    void Print(std::ostream &output, const std::string &org_tab) const;

 private:
    std::string table_name_;
    bool op_if_not_exist_;
    NodePointVector column_desc_list_;
};
class IndexKeyNode : public SQLNode {
 public:
    IndexKeyNode() : SQLNode(kIndexKey, 0, 0) {}
    explicit IndexKeyNode(const std::string &key) : SQLNode(kIndexKey, 0, 0) {
        key_.push_back(key);
    }
    ~IndexKeyNode() {}
    void AddKey(const std::string &key) { key_.push_back(key); }
    std::vector<std::string> &GetKey() { return key_; }

 private:
    std::vector<std::string> key_;
};
class IndexVersionNode : public SQLNode {
 public:
    IndexVersionNode() : SQLNode(kIndexVersion, 0, 0) {}
    explicit IndexVersionNode(const std::string &column_name)
        : SQLNode(kIndexVersion, 0, 0), column_name_(column_name), count_(1) {}
    IndexVersionNode(const std::string &column_name, int count)
        : SQLNode(kIndexVersion, 0, 0),
          column_name_(column_name),
          count_(count) {}

    std::string &GetColumnName() { return column_name_; }

    int GetCount() const { return count_; }

 private:
    std::string column_name_;
    int count_;
};
class IndexTsNode : public SQLNode {
 public:
    IndexTsNode() : SQLNode(kIndexTs, 0, 0) {}
    explicit IndexTsNode(const std::string &column_name)
        : SQLNode(kIndexTs, 0, 0), column_name_(column_name) {}

    std::string &GetColumnName() { return column_name_; }

 private:
    std::string column_name_;
};
class IndexTTLNode : public SQLNode {
 public:
    IndexTTLNode() : SQLNode(kIndexTTL, 0, 0) {}
    explicit IndexTTLNode(ExprNode *expr)
        : SQLNode(kIndexTTL, 0, 0), ttl_expr_(expr) {}

    ExprNode *GetTTLExpr() const { return ttl_expr_; }

 private:
    ExprNode *ttl_expr_;
};
class IndexTTLTypeNode : public SQLNode {
 public:
    IndexTTLTypeNode() : SQLNode(kIndexTTLType, 0, 0) {}
    explicit IndexTTLTypeNode(const std::string &ttl_type)
        : SQLNode(kIndexTTLType, 0, 0), ttl_type_(ttl_type) {}

    void set_ttl_type(const std::string &ttl_type) {
        this->ttl_type_ = ttl_type;
    }
    const std::string &ttl_type() const { return ttl_type_; }

 private:
    std::string ttl_type_;
};

class ColumnIndexNode : public SQLNode {
 public:
    ColumnIndexNode()
        : SQLNode(kColumnIndex, 0, 0),
          ts_(""),
          version_(""),
          version_count_(0),
          ttl_(-1L),
          ttl_type_(""),
          name_("") {}

    std::vector<std::string> &GetKey() { return key_; }
    void SetKey(const std::vector<std::string> &key) { key_ = key; }

    std::string GetTs() const { return ts_; }

    void SetTs(const std::string &ts) { ts_ = ts; }

    std::string GetVersion() const { return version_; }

    void SetVersion(const std::string &version) { version_ = version; }

    std::string GetName() const { return name_; }

    void SetName(const std::string &name) { name_ = name; }
    int GetVersionCount() const { return version_count_; }

    void SetVersionCount(int count) { version_count_ = count; }

    const std::string &ttl_type() const { return ttl_type_; }
    void set_ttl_type(const std::string &ttl_type) { ttl_type_ = ttl_type; }
    int64_t GetTTL() const { return ttl_; }
    void SetTTL(ExprNode *ttl_node) {
        if (nullptr == ttl_node) {
            ttl_ = -1l;
        } else {
            switch (ttl_node->GetExprType()) {
                case kExprPrimary: {
                    const ConstNode *ttl = dynamic_cast<ConstNode *>(ttl_node);
                    switch (ttl->GetDataType()) {
                        case fesql::node::kInt32:
                            ttl_ = ttl->GetInt();
                            break;
                        case fesql::node::kInt64:
                            ttl_ = ttl->GetLong();
                            break;
                        case fesql::node::kDay:
                        case fesql::node::kHour:
                        case fesql::node::kMinute:
                        case fesql::node::kSecond:
                            ttl_ = ttl->GetMillis();
                            break;
                        default: {
                            ttl_ = -1;
                        }
                    }
                    break;
                }
                default: {
                    LOG(WARNING) << "can't set ttl with expr type "
                                 << ExprTypeName(ttl_node->GetExprType());
                }
            }
        }
    }

    void Print(std::ostream &output, const std::string &org_tab) const;

 private:
    std::vector<std::string> key_;
    std::string ts_;
    std::string version_;
    int version_count_;
    int64_t ttl_;
    std::string ttl_type_;
    std::string name_;
};
class CmdNode : public SQLNode {
 public:
    explicit CmdNode(node::CmdType cmd_type)
        : SQLNode(kCmdStmt, 0, 0), cmd_type_(cmd_type) {}

    ~CmdNode() {}

    void AddArg(const std::string &arg) { args_.push_back(arg); }
    const std::vector<std::string> &GetArgs() const { return args_; }
    void Print(std::ostream &output, const std::string &org_tab) const;

    const node::CmdType GetCmdType() const { return cmd_type_; }

 private:
    node::CmdType cmd_type_;
    std::vector<std::string> args_;
};

class CreateIndexNode : public SQLNode {
 public:
    explicit CreateIndexNode(const std::string &index_name,
                             const std::string &table_name,
                             ColumnIndexNode *index)
        : SQLNode(kCreateIndexStmt, 0, 0),
          index_name_(index_name),
          table_name_(table_name),
          index_(index) {}
    void Print(std::ostream &output, const std::string &org_tab) const;

    const std::string index_name_;
    const std::string table_name_;
    node::ColumnIndexNode *index_;
};

class ExplainNode : public SQLNode {
 public:
    explicit ExplainNode(const QueryNode *query, node::ExplainType explain_type)
        : SQLNode(kExplainStmt, 0, 0),
          explain_type_(explain_type),
          query_(query) {}
    void Print(std::ostream &output, const std::string &org_tab) const;

    const node::ExplainType explain_type_;
    const node::QueryNode *query_;
};

class FnParaNode : public FnNode {
 public:
    explicit FnParaNode(ExprIdNode *para_id)
        : FnNode(kFnPara), para_id_(para_id) {}
    const std::string &GetName() const { return para_id_->GetName(); }

    const TypeNode *GetParaType() const { return para_id_->GetOutputType(); }

    ExprIdNode *GetExprId() const { return para_id_; }

    void Print(std::ostream &output, const std::string &org_tab) const;

 private:
    ExprIdNode *para_id_;
};
class FnNodeFnHeander : public FnNode {
 public:
    FnNodeFnHeander(const std::string &name, FnNodeList *parameters,
                    const TypeNode *ret_type)
        : FnNode(kFnHeader),
          name_(name),
          parameters_(parameters),
          ret_type_(ret_type) {}

    void Print(std::ostream &output, const std::string &org_tab) const;
    const std::string GeIRFunctionName() const;
    const std::string name_;
    const FnNodeList *parameters_;
    const TypeNode *ret_type_;
};
class FnNodeFnDef : public FnNode {
 public:
    FnNodeFnDef(const FnNodeFnHeander *header, FnNodeList *block)
        : FnNode(kFnDef), header_(header), block_(block) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    const FnNodeFnHeander *header_;
    FnNodeList *block_;
};

class FnAssignNode : public FnNode {
 public:
    explicit FnAssignNode(ExprIdNode *var, ExprNode *expression)
        : FnNode(kFnAssignStmt),
          var_(var),
          expression_(expression),
          is_ssa_(false) {}
    std::string GetName() const { return var_->GetName(); }
    const bool IsSSA() const { return is_ssa_; }
    void EnableSSA() { is_ssa_ = true; }
    void DisableSSA() { is_ssa_ = false; }
    void Print(std::ostream &output, const std::string &org_tab) const;
    node::ExprIdNode *var_;
    ExprNode *expression_;

 private:
    bool is_ssa_;
};

class FnIfNode : public FnNode {
 public:
    explicit FnIfNode(ExprNode *expression)
        : FnNode(kFnIfStmt), expression_(expression) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    ExprNode *expression_;
};
class FnElifNode : public FnNode {
 public:
    explicit FnElifNode(ExprNode *expression)
        : FnNode(kFnElifStmt), expression_(expression) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    ExprNode *expression_;
};
class FnElseNode : public FnNode {
 public:
    FnElseNode() : FnNode(kFnElseStmt) {}
    void Print(std::ostream &output, const std::string &org_tab) const override;
};

class FnForInNode : public FnNode {
 public:
    FnForInNode(ExprIdNode *var, ExprNode *in_expression)
        : FnNode(kFnForInStmt), var_(var), in_expression_(in_expression) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    ExprIdNode *var_;
    ExprNode *in_expression_;
};

class FnIfBlock : public FnNode {
 public:
    FnIfBlock(FnIfNode *node, FnNodeList *block)
        : FnNode(kFnIfBlock), if_node(node), block_(block) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    FnIfNode *if_node;
    FnNodeList *block_;
};

class FnElifBlock : public FnNode {
 public:
    FnElifBlock(FnElifNode *node, FnNodeList *block)
        : FnNode(kFnElifBlock), elif_node_(node), block_(block) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    FnElifNode *elif_node_;
    FnNodeList *block_;
};
class FnElseBlock : public FnNode {
 public:
    explicit FnElseBlock(FnNodeList *block)
        : FnNode(kFnElseBlock), block_(block) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    FnNodeList *block_;
};
class FnIfElseBlock : public FnNode {
 public:
    FnIfElseBlock(FnIfBlock *if_block, FnElseBlock *else_block)
        : FnNode(kFnIfElseBlock),
          if_block_(if_block),
          else_block_(else_block) {}
    void Print(std::ostream &output, const std::string &org_tab) const;
    FnIfBlock *if_block_;
    std::vector<FnNode *> elif_blocks_;
    FnElseBlock *else_block_;
};

class FnForInBlock : public FnNode {
 public:
    FnForInBlock(FnForInNode *for_in_node, FnNodeList *block)
        : FnNode(kFnForInBlock), for_in_node_(for_in_node), block_(block) {}

    void Print(std::ostream &output, const std::string &org_tab) const;
    FnForInNode *for_in_node_;
    FnNodeList *block_;
};
class FnReturnStmt : public FnNode {
 public:
    explicit FnReturnStmt(ExprNode *return_expr)
        : FnNode(kFnReturnStmt), return_expr_(return_expr) {}
    void Print(std::ostream &output, const std::string &org_tab) const override;
    ExprNode *return_expr_;
};
class StructExpr : public ExprNode {
 public:
    explicit StructExpr(const std::string &name)
        : ExprNode(kExprStruct), class_name_(name) {}
    void SetFileds(FnNodeList *fileds) { fileds_ = fileds; }
    void SetMethod(FnNodeList *methods) { methods_ = methods; }

    const FnNodeList *GetMethods() const { return methods_; }

    const FnNodeList *GetFileds() const { return fileds_; }

    const std::string &GetName() const { return class_name_; }
    void Print(std::ostream &output, const std::string &org_tab) const override;

 private:
    const std::string class_name_;
    FnNodeList *fileds_;
    FnNodeList *methods_;
};

class ExternalFnDefNode : public FnDefNode {
 public:
    explicit ExternalFnDefNode(
        const std::string &name, void *fn_ptr, const node::TypeNode *ret_type,
        const std::vector<const node::TypeNode *> &arg_types,
        int variadic_pos = -1, bool return_by_arg = false)
        : FnDefNode(kExternalFnDef),
          function_name_(name),
          function_ptr_(fn_ptr),
          ret_type_(ret_type),
          arg_types_(arg_types),
          variadic_pos_(variadic_pos),
          return_by_arg_(return_by_arg) {}

    const std::string function_name() const { return function_name_; }

    const std::string GetSimpleName() const override { return function_name_; }

    void *function_ptr() const { return function_ptr_; }
    const node::TypeNode *ret_type() const { return ret_type_; }
    const std::vector<const node::TypeNode *> &arg_types() const {
        return arg_types_;
    }
    int variadic_pos() const { return variadic_pos_; }
    bool return_by_arg() const { return return_by_arg_; }

    void Print(std::ostream &output, const std::string &tab) const override;
    bool Equals(const SQLNode *node) const override;

    void SetRetType(const node::TypeNode *dtype) { this->ret_type_ = dtype; }

    void SetReturnByArg(bool flag) { this->return_by_arg_ = flag; }

    bool IsResolved() const { return ret_type_ != nullptr; }

    bool Validate(
        const std::vector<const TypeNode *> &arg_types) const override;

    const TypeNode *GetReturnType() const override { return ret_type_; }
    size_t GetArgSize() const override { return arg_types_.size(); }
    const TypeNode *GetArgType(size_t i) const { return arg_types_[i]; }

 private:
    std::string function_name_;
    void *function_ptr_;

    const node::TypeNode *ret_type_;
    std::vector<const node::TypeNode *> arg_types_;

    // eg, variadic_pos_=1 for fn(x, ...);
    // -1 denotes non-variadic
    int variadic_pos_;

    bool return_by_arg_;
};

class UDFDefNode : public FnDefNode {
 public:
    explicit UDFDefNode(FnNodeFnDef *def) : FnDefNode(kUDFDef), def_(def) {}
    FnNodeFnDef *def() const { return def_; }

    const std::string GetSimpleName() const override { return "UDF"; }

    void Print(std::ostream &output, const std::string &tab) const override;
    bool Equals(const SQLNode *node) const override;

    const TypeNode *GetReturnType() const override {
        return def()->header_->ret_type_;
    }
    size_t GetArgSize() const override {
        return def()->header_->parameters_->GetChildren().size();
    }
    const TypeNode *GetArgType(size_t i) const {
        auto node = def()->header_->parameters_->GetChildren()[i];
        return dynamic_cast<FnParaNode *>(node)->GetParaType();
    }

    bool Validate(
        const std::vector<const TypeNode *> &arg_types) const override {
        return true;
    }

 private:
    FnNodeFnDef *def_;
};

class UDFByCodeGenDefNode : public FnDefNode {
 public:
    UDFByCodeGenDefNode(const std::vector<const node::TypeNode *> &arg_types,
                        const node::TypeNode *ret_type)
        : FnDefNode(kUDFByCodeGenDef),
          arg_types_(arg_types),
          ret_type_(ret_type) {}

    const std::string GetSimpleName() const override { return "CODEGEN_UDF"; }

    void SetGenImpl(std::shared_ptr<udf::LLVMUDFGenBase> gen_impl) {
        this->gen_impl_ = gen_impl;
    }

    std::shared_ptr<udf::LLVMUDFGenBase> GetGenImpl() const {
        return this->gen_impl_;
    }

    const TypeNode *GetReturnType() const override { return ret_type_; }
    size_t GetArgSize() const override { return arg_types_.size(); }
    const TypeNode *GetArgType(size_t i) const { return arg_types_[i]; }

    bool Validate(
        const std::vector<const TypeNode *> &arg_types) const override {
        return true;
    }

 private:
    std::shared_ptr<udf::LLVMUDFGenBase> gen_impl_;
    std::vector<const node::TypeNode *> arg_types_;
    const node::TypeNode *ret_type_;
};

class LambdaNode : public FnDefNode {
 public:
    LambdaNode(const std::vector<node::ExprIdNode *> &args,
               node::ExprNode *body)
        : FnDefNode(kLambdaDef), args_(args), body_(body) {}

    const std::string GetSimpleName() const override { return "Lambda"; }

    const TypeNode *GetReturnType() const override {
        return body_->GetOutputType();
    }
    size_t GetArgSize() const override { return args_.size(); }
    const TypeNode *GetArgType(size_t i) const override {
        return args_[i]->GetOutputType();
    }

    node::ExprIdNode *GetArg(size_t i) const { return args_[i]; }
    node::ExprNode *body() const { return body_; }

    void Print(std::ostream &output, const std::string &tab) const override;
    bool Equals(const SQLNode *node) const override;

    bool Validate(
        const std::vector<const TypeNode *> &arg_types) const override;

 private:
    std::vector<node::ExprIdNode *> args_;
    node::ExprNode *body_;
};

class UDAFDefNode : public FnDefNode {
 public:
    UDAFDefNode(const std::string &name, const TypeNode *input_type,
                const ExprNode *init, const FnDefNode *update_func,
                const FnDefNode *merge_func, const FnDefNode *output_func)
        : FnDefNode(kUDAFDef),
          name_(name),
          input_type_(input_type),
          init_(init),
          update_(update_func),
          merge_(merge_func),
          output_(output_func) {}

    const std::string GetSimpleName() const override { return name_; }

    bool Equals(const SQLNode *node) const override;
    void Print(std::ostream &output, const std::string &tab) const override;

    const ExprNode *init_expr() const { return init_; }
    const FnDefNode *update_func() const { return update_; }
    const FnDefNode *merge_func() const { return merge_; }
    const FnDefNode *output_func() const { return output_; }

    bool AllowMerge() const { return merge_ != nullptr; }
    bool Validate(
        const std::vector<const TypeNode *> &arg_types) const override {
        return true;
    }

    const TypeNode *GetInputElementType() const {
        return update_->GetArgType(1);
    }

    const TypeNode *GetStateType() const { return update_->GetReturnType(); }

    const TypeNode *GetReturnType() const override {
        if (output_ != nullptr) {
            return output_->GetReturnType();
        } else {
            return GetStateType();
        }
    }

    size_t GetArgSize() const override { return 1; }

    const TypeNode *GetArgType(size_t i) const {
        if (i == 0) {
            return input_type_;
        } else {
            return nullptr;
        }
    }

 private:
    std::string name_;
    const TypeNode *input_type_;
    const ExprNode *init_;
    const FnDefNode *update_;
    const FnDefNode *merge_;
    const FnDefNode *output_;
};

std::string ExprString(const ExprNode *expr);
std::string MakeExprWithTable(const ExprNode *expr, const std::string db);
bool ExprListNullOrEmpty(const ExprListNode *expr);
bool SQLEquals(const SQLNode *left, const SQLNode *right);
bool SQLListEquals(const SQLNodeList *left, const SQLNodeList *right);
bool ExprEquals(const ExprNode *left, const ExprNode *right);
bool FnDefEquals(const FnDefNode *left, const FnDefNode *right);
bool WindowOfExpression(std::map<std::string, const WindowDefNode *> windows,
                        ExprNode *node_ptr, const WindowDefNode **output);
void FillSQLNodeList2NodeVector(
    SQLNodeList *node_list_ptr,
    std::vector<SQLNode *> &node_list);  // NOLINT (runtime/references)
void PrintSQLNode(std::ostream &output, const std::string &org_tab,
                  const SQLNode *node_ptr, const std::string &item_name,
                  bool last_child);
void PrintSQLVector(std::ostream &output, const std::string &tab,
                    const NodePointVector &vec, const std::string &vector_name,
                    bool last_item);
void PrintSQLVector(std::ostream &output, const std::string &tab,
                    const std::vector<ExprNode *> &vec,
                    const std::string &vector_name, bool last_item);
void PrintValue(std::ostream &output, const std::string &org_tab,
                const std::string &value, const std::string &item_name,
                bool last_child);
}  // namespace node
}  // namespace fesql
#endif  // SRC_NODE_SQL_NODE_H_
