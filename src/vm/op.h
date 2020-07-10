/*
 * op.h
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

#ifndef SRC_VM_OP_H_
#define SRC_VM_OP_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "proto/fe_type.pb.h"
#include "vm/catalog.h"

namespace fesql {
namespace vm {

enum OpType { kOpProject = 1, kOpScan, kOpLimit, kOpMerge };

struct OpNode {
    virtual ~OpNode() {}
    OpType type;
    uint32_t idx;
    vm::Schema output_schema;
    std::vector<OpNode*> children;
};

struct ScanOp : public OpNode {
    ~ScanOp() {}
    std::string db;
    std::shared_ptr<TableHandler> table_handler;
    uint32_t limit;
};

// TODO(chenjing): WindowOp
struct ScanInfo {
    std::vector<ColInfo> keys;
    ColInfo order;
    std::string index_name;
    // todo(chenjing): start and end parse
    int64_t start_offset;
    int64_t end_offset;
    bool has_order;
    bool is_range_between;

    bool FindKey(const type::Type& type, uint32_t pos) {
        for (uint32_t i = 0; i < keys.size(); i++) {
            const ColInfo& col = keys.at(i);
            if (col.pos == pos && type == col.type) {
                return true;
            }
        }
        return false;
    }
};

struct ProjectOp : public OpNode {
    ~ProjectOp() {}
    std::string db;
    std::shared_ptr<TableHandler> table_handler;
    uint32_t scan_limit;
    int8_t* fn;
    std::string fn_name;
    bool window_agg;
    ScanInfo w;
};

struct LimitOp : public OpNode {
    ~LimitOp() {}
    uint32_t limit;
};

struct MergeOp : public OpNode {
    int8_t* fn;
    std::vector<std::pair<uint32_t, uint32_t>> pos_mapping;
};

}  // namespace vm
}  // namespace fesql
#endif  // SRC_VM_OP_H_
