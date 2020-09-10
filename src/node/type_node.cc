/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * type_node.cc
 *
 * Author: chenjing
 * Date: 2019/10/11
 *--------------------------------------------------------------------------
 **/
#include "node/type_node.h"

namespace fesql {
namespace node {

bool TypeNode::IsBaseType() const {
    return IsNumber() || IsString() || IsTimestamp() || IsDate();
}
bool TypeNode::IsDate() const { return base_ == node::kDate; }
bool TypeNode::IsTimestamp() const { return base_ == node::kTimestamp; }
bool TypeNode::IsString() const { return base_ == node::kVarchar; }
bool TypeNode::IsArithmetic() const { return IsInteger() || IsFloating(); }
bool TypeNode::IsNumber() const { return IsInteger() || IsFloating(); }
bool TypeNode::IsNull() const { return base_ == node::kNull; }
bool TypeNode::IsBool() const { return base_ == node::kBool; }

bool TypeNode::IsInteger() const {
    return base_ == node::kBool || base_ == node::kInt16 ||
           base_ == node::kInt32 || base_ == node::kInt64;
}

bool TypeNode::IsFloating() const {
    return base_ == node::kFloat || base_ == node::kDouble;
}
Status TypeNode::CheckTypeNodeNotNull(const TypeNode* left_type) {
    CHECK_TRUE(nullptr != left_type, common::kTypeError, "null type node");
    return Status::OK();
}
}  // namespace node
}  // namespace fesql
