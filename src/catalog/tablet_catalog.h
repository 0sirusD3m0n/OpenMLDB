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

#ifndef SRC_CATALOG_TABLET_CATALOG_H_
#define SRC_CATALOG_TABLET_CATALOG_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/spinlock.h"
#include "catalog/client_manager.h"
#include "catalog/distribute_iterator.h"
#include "client/tablet_client.h"
#include "codec/row.h"
#include "storage/table.h"
#include "storage/schema.h"
#include "vm/catalog.h"

namespace rtidb {
namespace catalog {

class TabletPartitionHandler;
class TabletTableHandler;
class TabletSegmentHandler;

class TabletSegmentHandler : public ::fesql::vm::TableHandler {
 public:
    TabletSegmentHandler(std::shared_ptr<::fesql::vm::PartitionHandler> partition_handler, const std::string &key)
        : TableHandler(), partition_handler_(partition_handler), key_(key) {}

    ~TabletSegmentHandler() {}

    const ::fesql::vm::Schema *GetSchema() override { return partition_handler_->GetSchema(); }

    const std::string &GetName() override { return partition_handler_->GetName(); }

    const std::string &GetDatabase() override { return partition_handler_->GetDatabase(); }

    const ::fesql::vm::Types &GetTypes() override { return partition_handler_->GetTypes(); }

    const ::fesql::vm::IndexHint &GetIndex() override { return partition_handler_->GetIndex(); }

    const ::fesql::vm::OrderType GetOrderType() const override { return partition_handler_->GetOrderType(); }

    std::unique_ptr<::fesql::vm::RowIterator> GetIterator() const override {
        auto iter = partition_handler_->GetWindowIterator();
        if (iter) {
            DLOG(INFO) << "seek to pk " << key_;
            iter->Seek(key_);
            if (iter->Valid() && 0 == iter->GetKey().compare(fesql::codec::Row(key_))) {
                return std::move(iter->GetValue());
            } else {
                return std::unique_ptr<::fesql::vm::RowIterator>();
            }
        }
        return std::unique_ptr<::fesql::vm::RowIterator>();
    }

    ::fesql::vm::RowIterator *GetRawIterator() const override {
        auto iter = partition_handler_->GetWindowIterator();
        if (iter) {
            DLOG(INFO) << "seek to pk " << key_;
            iter->Seek(key_);
            if (iter->Valid() &&
                0 == iter->GetKey().compare(fesql::codec::Row(key_))) {
                return iter->GetRawValue();
            } else {
                return nullptr;
            }
        }
        return nullptr;
    }

    std::unique_ptr<::fesql::vm::WindowIterator> GetWindowIterator(const std::string &idx_name) override {
        return std::unique_ptr<::fesql::vm::WindowIterator>();
    }

    const uint64_t GetCount() override {
        auto iter = GetIterator();
        if (!iter) return 0;
        uint64_t cnt = 0;
        while (iter->Valid()) {
            cnt++;
            iter->Next();
        }
        return cnt;
    }

    ::fesql::vm::Row At(uint64_t pos) override {
        auto iter = GetIterator();
        if (!iter) return ::fesql::vm::Row();
        while (pos-- > 0 && iter->Valid()) {
            iter->Next();
        }
        return iter->Valid() ? iter->GetValue() : ::fesql::vm::Row();
    }
    const std::string GetHandlerTypeName() override { return "TabletSegmentHandler"; }

 private:
    std::shared_ptr<::fesql::vm::PartitionHandler> partition_handler_;
    std::string key_;
};

class TabletPartitionHandler : public ::fesql::vm::PartitionHandler {
 public:
    TabletPartitionHandler(std::shared_ptr<::fesql::vm::TableHandler> table_hander, const std::string &index_name)
        : PartitionHandler(), table_handler_(table_hander), index_name_(index_name) {}

    ~TabletPartitionHandler() {}

    const ::fesql::vm::OrderType GetOrderType() const override { return ::fesql::vm::OrderType::kDescOrder; }

    const ::fesql::vm::Schema *GetSchema() override { return table_handler_->GetSchema(); }

    const std::string &GetName() override { return table_handler_->GetName(); }

    const std::string &GetDatabase() override { return table_handler_->GetDatabase(); }

    const ::fesql::vm::Types &GetTypes() override { return table_handler_->GetTypes(); }

    const ::fesql::vm::IndexHint &GetIndex() override { return table_handler_->GetIndex(); }

    std::unique_ptr<::fesql::vm::WindowIterator> GetWindowIterator() override {
        DLOG(INFO) << "get window it with name " << index_name_;
        return table_handler_->GetWindowIterator(index_name_);
    }

    const uint64_t GetCount() override {
        auto iter = GetWindowIterator();
        if (!iter) return 0;
        uint64_t cnt = 0;
        iter->SeekToFirst();
        while (iter->Valid()) {
            cnt++;
            iter->Next();
        }
        return cnt;
    }

    std::shared_ptr<::fesql::vm::TableHandler> GetSegment(
        std::shared_ptr<::fesql::vm::PartitionHandler> partition_hander, const std::string &key) override {
        return std::make_shared<TabletSegmentHandler>(partition_hander, key);
    }
    const std::string GetHandlerTypeName() override { return "TabletPartitionHandler"; }

 private:
    std::shared_ptr<::fesql::vm::TableHandler> table_handler_;
    std::string index_name_;
};

class TabletTableHandler : public ::fesql::vm::TableHandler {
 public:
    explicit TabletTableHandler(const ::rtidb::api::TableMeta &meta);

    explicit TabletTableHandler(const ::rtidb::nameserver::TableInfo &meta);

    bool Init();

    const ::fesql::vm::Schema *GetSchema() override { return &schema_; }

    const std::string &GetName() override { return table_st_.GetName(); }

    const std::string &GetDatabase() override { return table_st_.GetDB(); }

    const ::fesql::vm::Types &GetTypes() override { return types_; }

    const ::fesql::vm::IndexHint &GetIndex() override { return index_hint_; }

    const ::fesql::codec::Row Get(int32_t pos);

    std::unique_ptr<::fesql::codec::RowIterator> GetIterator() const override;

    ::fesql::codec::RowIterator *GetRawIterator() const override;

    std::unique_ptr<::fesql::codec::WindowIterator> GetWindowIterator(const std::string &idx_name) override;

    const uint64_t GetCount() override;

    ::fesql::codec::Row At(uint64_t pos) override;

    std::shared_ptr<::fesql::vm::PartitionHandler> GetPartition(std::shared_ptr<::fesql::vm::TableHandler> table_hander,
                                                                const std::string &index_name) const override;

    const std::string GetHandlerTypeName() override { return "TabletTableHandler"; }

    inline int32_t GetTid() { return table_st_.GetTid(); }

    void AddTable(std::shared_ptr<::rtidb::storage::Table> table);

    int DeleteTable(uint32_t pid);

    void Update(const ::rtidb::nameserver::TableInfo &meta, const ClientManager &client_manager);

    bool GetTablets(const std::string &index_name, const std::string &pk,
                    std::vector<std::shared_ptr<::rtidb::client::TabletClient>> *clients);

 private:
    inline int32_t GetColumnIndex(const std::string &column) {
        auto it = types_.find(column);
        if (it != types_.end()) {
            return it->second.idx;
        }
        return -1;
    }

 private:
    ::fesql::vm::Schema schema_;
    ::rtidb::storage::TableSt table_st_;
    std::shared_ptr<Tables> tables_;
    ::fesql::vm::Types types_;
    ::fesql::vm::IndexList index_list_;
    ::fesql::vm::IndexHint index_hint_;
    std::shared_ptr<TableClientManager> table_client_manager_;
};

typedef std::map<std::string, std::map<std::string, std::shared_ptr<TabletTableHandler>>> TabletTables;
typedef std::map<std::string, std::shared_ptr<::fesql::type::Database>> TabletDB;

class TabletCatalog : public ::fesql::vm::Catalog {
 public:
    TabletCatalog();

    ~TabletCatalog();

    bool Init();

    bool AddDB(const ::fesql::type::Database &db);

    bool AddTable(const ::rtidb::api::TableMeta &meta, std::shared_ptr<::rtidb::storage::Table> table);

    std::shared_ptr<::fesql::type::Database> GetDatabase(const std::string &db) override;

    std::shared_ptr<::fesql::vm::TableHandler> GetTable(const std::string &db, const std::string &table_name) override;

    bool IndexSupport() override;

    bool DeleteTable(const std::string &db, const std::string &table_name, uint32_t pid);

    bool DeleteDB(const std::string &db);

    void RefreshTable(const std::vector<std::shared_ptr<::rtidb::nameserver::TableInfo>> &table_info_vec);

    bool UpdateClient(const std::map<std::string, std::string> &real_ep_map);

 private:
    ::rtidb::base::SpinMutex mu_;
    TabletTables tables_;
    TabletDB db_;
    ClientManager client_manager_;
};

}  // namespace catalog
}  // namespace rtidb
#endif  // SRC_CATALOG_TABLET_CATALOG_H_
