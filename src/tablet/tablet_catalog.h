/*
 * tablet_catalog.h
 * Copyright (C) 4paradigm.com 2020 wangtaize <wangtaize@4paradigm.com>
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

#ifndef SRC_TABLET_TABLET_CATALOG_H_
#define SRC_TABLET_TABLET_CATALOG_H_

#include <map>
#include <memory>
#include <string>
#include "base/spin_lock.h"
#include "storage/fe_table.h"
#include "vm/catalog.h"

namespace fesql {
namespace tablet {

using codec::Row;
using vm::OrderType;
using vm::PartitionHandler;
using vm::RowIterator;
using vm::TableHandler;
using vm::WindowIterator;

class TabletPartitionHandler;
class TabletTableHandler;
class TabletSegmentHandler;

class TabletSegmentHandler : public TableHandler {
 public:
    TabletSegmentHandler(std::shared_ptr<PartitionHandler> partition_hander,
                         const std::string& key);

    ~TabletSegmentHandler();

    inline const vm::Schema* GetSchema() {
        return partition_hander_->GetSchema();
    }

    inline const std::string& GetName() { return partition_hander_->GetName(); }

    inline const std::string& GetDatabase() {
        return partition_hander_->GetDatabase();
    }

    inline const vm::Types& GetTypes() { return partition_hander_->GetTypes(); }

    inline const vm::IndexHint& GetIndex() {
        return partition_hander_->GetIndex();
    }

    const OrderType GetOrderType() const override {
        return partition_hander_->GetOrderType();
    }

    std::unique_ptr<vm::RowIterator> GetIterator() const;
    RowIterator* GetIterator(int8_t* addr) const override;
    std::unique_ptr<vm::WindowIterator> GetWindowIterator(
        const std::string& idx_name);
    virtual const uint64_t GetCount();
    Row At(uint64_t pos) override;
    const std::string GetHandlerTypeName() override {
        return "TabletSegmentHandler";
    }

 private:
    std::shared_ptr<vm::PartitionHandler> partition_hander_;
    std::string key_;
};

class TabletPartitionHandler : public PartitionHandler {
 public:
    TabletPartitionHandler(std::shared_ptr<TableHandler> table_hander,
                           const std::string& index_name)
        : PartitionHandler(),
          table_handler_(table_hander),
          index_name_(index_name) {}

    ~TabletPartitionHandler() {}

    const OrderType GetOrderType() const { return OrderType::kDescOrder; }

    inline const vm::Schema* GetSchema() { return table_handler_->GetSchema(); }

    inline const std::string& GetName() { return table_handler_->GetName(); }

    inline const std::string& GetDatabase() {
        return table_handler_->GetDatabase();
    }

    inline const vm::Types& GetTypes() { return table_handler_->GetTypes(); }

    inline const vm::IndexHint& GetIndex() { return index_hint_; }
    std::unique_ptr<vm::WindowIterator> GetWindowIterator() override {
        return table_handler_->GetWindowIterator(index_name_);
    }
    const uint64_t GetCount() override;

    virtual std::shared_ptr<TableHandler> GetSegment(
        std::shared_ptr<PartitionHandler> partition_hander,
        const std::string& key) {
        return std::shared_ptr<TabletSegmentHandler>(
            new TabletSegmentHandler(partition_hander, key));
    }
    const std::string GetHandlerTypeName() override {
        return "TabletPartitionHandler";
    }

 private:
    std::shared_ptr<TableHandler> table_handler_;
    std::string index_name_;
    vm::IndexHint index_hint_;
};

class TabletTableHandler : public vm::TableHandler {
 public:
    TabletTableHandler(const vm::Schema schema, const std::string& name,
                       const std::string& db, const vm::IndexList& index_list,
                       std::shared_ptr<storage::Table> table);

    ~TabletTableHandler();

    bool Init();

    inline const vm::Schema* GetSchema() { return &schema_; }

    inline const std::string& GetName() { return name_; }

    inline const std::string& GetDatabase() { return db_; }

    inline const vm::Types& GetTypes() { return types_; }

    inline const vm::IndexHint& GetIndex() { return index_hint_; }

    const Row Get(int32_t pos);

    inline std::shared_ptr<storage::Table> GetTable() { return table_; }

    std::unique_ptr<RowIterator> GetIterator() const;
    RowIterator* GetIterator(int8_t* addr) const override;
    std::unique_ptr<WindowIterator> GetWindowIterator(
        const std::string& idx_name);
    virtual const uint64_t GetCount();
    Row At(uint64_t pos) override;

    virtual std::shared_ptr<PartitionHandler> GetPartition(
        std::shared_ptr<TableHandler> table_hander,
        const std::string& index_name) const {
        if (!table_hander) {
            LOG(WARNING) << "fail to get partition for tablet table handler: "
                            "table handler is null";
            return std::shared_ptr<PartitionHandler>();
        }
        if (table_hander->GetIndex().find(index_name) ==
            table_hander->GetIndex().cend()) {
            LOG(WARNING)
                << "fail to get partition for tablet table handler, index name "
                << index_name;
            return std::shared_ptr<PartitionHandler>();
        }
        return std::shared_ptr<TabletPartitionHandler>(
            new TabletPartitionHandler(table_hander, index_name));
    }
    const std::string GetHandlerTypeName() override {
        return "TabletTableHandler";
    }

 private:
    inline int32_t GetColumnIndex(const std::string& column) {
        auto it = types_.find(column);
        if (it != types_.end()) {
            return it->second.pos;
        }
        return -1;
    }

 private:
    vm::Schema schema_;
    std::string name_;
    std::string db_;
    std::shared_ptr<storage::Table> table_;
    vm::Types types_;
    vm::IndexList index_list_;
    vm::IndexHint index_hint_;
};

typedef std::map<std::string,
                 std::map<std::string, std::shared_ptr<TabletTableHandler>>>
    TabletTables;
typedef std::map<std::string, std::shared_ptr<type::Database>> TabletDB;

class TabletCatalog : public vm::Catalog {
 public:
    TabletCatalog();

    ~TabletCatalog();

    bool Init();

    bool AddDB(const type::Database& db);

    bool AddTable(std::shared_ptr<TabletTableHandler> table);

    std::shared_ptr<type::Database> GetDatabase(const std::string& db);

    std::shared_ptr<vm::TableHandler> GetTable(const std::string& db,
                                               const std::string& table_name);
    bool IndexSupport() override;

 private:
    TabletTables tables_;
    TabletDB db_;
};

}  // namespace tablet
}  // namespace fesql
#endif  // SRC_TABLET_TABLET_CATALOG_H_
