/*
 * router.h
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

#ifndef SRC_VM_ROUTER_H_
#define SRC_VM_ROUTER_H_

#include <string>
#include "vm/physical_op.h"

namespace fesql {
namespace vm {

class Router {
 public:
    void SetMainTable(const std::string& main_table) {
        main_table_ = main_table;
    }

    const std::string& GetMainTable() const { return main_table_; }

    int Parse(const PhysicalOpNode* physical_plan);

    const std::string& GetRouterCol() const { return router_col_; }

 private:
    std::string main_table_;
    std::string router_col_;
};

}  // namespace vm
}  // namespace fesql
#endif  // SRC_VM_ROUTER_H_
