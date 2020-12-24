/*
 * client_manager.cc
 * Copyright (C) 4paradigm.com
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

#include "catalog/client_manager.h"

#include <utility>

#include "codec/fe_schema_codec.h"
#include "codec/sql_rpc_row_codec.h"

DECLARE_int32(request_timeout_ms);

namespace rtidb {
namespace catalog {

TabletRowHandler::TabletRowHandler(const std::string& db, rtidb::RpcCallback<rtidb::api::QueryResponse>* callback)
    : db_(db), name_(), status_(::fesql::base::Status::Running()), row_(), callback_(callback) {
    callback_->Ref();
}

TabletRowHandler::TabletRowHandler(::fesql::base::Status status)
    : db_(), name_(), status_(status), row_(), callback_(nullptr) {}

TabletRowHandler::~TabletRowHandler() {
    if (callback_ != nullptr) {
        callback_->UnRef();
    }
}

const ::fesql::codec::Row& TabletRowHandler::GetValue() {
    if (!status_.isRunning() || !callback_) {
        return row_;
    }
    auto cntl = callback_->GetController();
    auto response = callback_->GetResponse();
    if (!cntl || !response) {
        status_.code = fesql::common::kRpcError;
        return row_;
    }
    DLOG(INFO) << "TabletRowHandler get value by brpc join";
    brpc::Join(cntl->call_id());
    if (cntl->Failed()) {
        status_ = ::fesql::base::Status(::fesql::common::kRpcError, "request error. " + cntl->ErrorText());
        return row_;
    }
    if (cntl->response_attachment().size() <= codec::HEADER_LENGTH) {
        status_.code = fesql::common::kSchemaCodecError;
        status_.msg = "response content decode fail";
        return row_;
    }
    uint32_t tmp_size = 0;
    row_ = fesql::codec::Row();
    if (!codec::DecodeRpcRow(cntl->response_attachment(), 0, response->byte_size(),
                             response->row_slices(), &row_)) {
        status_.code = fesql::common::kRpcError;
        status_.msg = "response content decode fail";
        return row_;
    }
    status_.code = ::fesql::common::kOk;
    return row_;
}

AsyncTableHandler::AsyncTableHandler(rtidb::RpcCallback<rtidb::api::SQLBatchRequestQueryResponse>* callback)
    : fesql::vm::MemTableHandler("", "", nullptr),
      status_(::fesql::base::Status::Running()),
      callback_(callback) {
    callback_->Ref();
}

std::unique_ptr<fesql::vm::RowIterator> AsyncTableHandler::GetIterator() {
    if (status_.isRunning()) {
        SyncRpcResponse();
    }
    if (status_.isOK()) {
        return fesql::vm::MemTableHandler::GetIterator();
    }

    return std::unique_ptr<fesql::vm::RowIterator>();
}
fesql::vm::RowIterator* AsyncTableHandler::GetRawIterator() {
    if (status_.isRunning()) {
        SyncRpcResponse();
    }
    if (status_.isOK()) {
        return fesql::vm::MemTableHandler::GetRawIterator();
    }
    return nullptr;
}
void AsyncTableHandler::SyncRpcResponse() {
    auto cntl = callback_->GetController();
    auto response = callback_->GetResponse();
    if (!cntl || !response) {
        status_.code = fesql::common::kRpcError;
        status_.msg = "rpc controller or response is null";
        LOG(WARNING) << status_.msg;
        return;
    }
    DLOG(INFO) << "AsyncTableHandler sync data brpc join";
    brpc::Join(cntl->call_id());
    if (cntl->Failed()) {
        status_ = ::fesql::base::Status(::fesql::common::kRpcError, "request error. " + cntl->ErrorText());
        LOG(WARNING) << status_.msg;
        return;
    }
    if (response->code() != 0) {
        status_ = ::fesql::base::Status(::fesql::common::kResponseError, "request error. " + response->msg());
        LOG(WARNING) << status_.msg;
        return;
    }
    int32_t position = 0;
    for (auto row_size : response->row_sizes()) {
        if (row_size < 0) {
            LOG(WARNING) << "illegal row size field";
            status_ = ::fesql::base::Status(::fesql::common::kResponseError, "illegal row size field.");
            LOG(WARNING) << status_.msg;
            return;
        }
        int8_t* out_buf = reinterpret_cast<int8_t*>(malloc(row_size));
        cntl->response_attachment().copy_to(out_buf, row_size, position);
        AddRow(fesql::codec::Row(fesql::base::RefCountedSlice::CreateManaged(out_buf, row_size)));
        position += row_size;
    }
    status_ = fesql::base::Status::OK();
    return;
}

AsyncTablesHandler::AsyncTablesHandler()
    : fesql::vm::MemTableHandler("", "", nullptr),
      status_(::fesql::base::Status::Running()),
      rows_cnt_(0),
      posinfos_(),
      handlers_() {}


std::unique_ptr<fesql::vm::RowIterator> AsyncTablesHandler::GetIterator() {
    if (status_.isRunning()) {
        SyncAllTableHandlers();
    }
    if (status_.isOK()) {
        return fesql::vm::MemTableHandler::GetIterator();
    }
    return std::unique_ptr<fesql::vm::RowIterator>();
}
fesql::vm::RowIterator* AsyncTablesHandler::GetRawIterator() {
    if (status_.isRunning()) {
        SyncAllTableHandlers();
    }
    if (status_.isOK()) {
        return fesql::vm::MemTableHandler::GetRawIterator();
    }
    return nullptr;
}
bool AsyncTablesHandler::SyncAllTableHandlers() {
    DLOG(INFO) << "SyncAllTableHandlers rows_cnt_ " << rows_cnt_;
    Resize(rows_cnt_);
    for (size_t handler_idx = 0; handler_idx < handlers_.size(); handler_idx++) {
        auto& handler = handlers_[handler_idx];
        auto iter = handler->GetIterator();
        if (!handler->GetStatus().isOK()) {
            status_.msg = "fail to sync table handler " + std::to_string(handler_idx) + ": " + handler->GetStatus().msg;
            status_.code = handler->GetStatus().code;
            return false;
        }
        if (!iter) {
            status_.msg = "fail to sync table hander: iter is null";
            status_.code = fesql::common::kResponseError;
            return false;
        }
        auto& posinfo = posinfos_[handler_idx];
        if (handler->GetCount() != posinfos_[handler_idx].size()) {
            status_.msg = "fail to sync table hander: unexpected rows cnt";
            status_.code = fesql::common::kResponseError;
            return false;
        }
        size_t pos_idx = 0;
        iter->SeekToFirst();
        while (iter->Valid()) {
            SetRow(posinfo[pos_idx], iter->GetValue());
            iter->Next();
            pos_idx++;
        }
    }
    status_ = fesql::base::Status::OK();
    DLOG(INFO) << "SyncAllTableHandlers OK";
    return true;
}

std::shared_ptr<::fesql::vm::RowHandler> TabletAccessor::SubQuery(uint32_t task_id, const std::string& db,
                                                                  const std::string& sql,
                                                                  const ::fesql::codec::Row& row,
                                                                  const bool is_procedure, const bool is_debug) {
    DLOG(INFO) << "SubQuery taskid: " << task_id << " is_procedure=" << is_procedure;
    auto client = GetClient();
    if (!client) {
        return std::make_shared<TabletRowHandler>(
            ::fesql::base::Status(::fesql::common::kRpcError, "get client failed"));
    }
    ::rtidb::api::QueryRequest request;
    if (is_procedure) {
        request.set_sp_name(sql);
    } else {
        request.set_sql(sql);
    }
    request.set_db(db);
    request.set_is_batch(false);
    request.set_task_id(task_id);
    request.set_is_debug(is_debug);
    request.set_is_procedure(is_procedure);
    auto cntl = std::make_shared<brpc::Controller>();
    if (!row.empty()) {
        auto& io_buf = cntl->request_attachment();
        size_t row_size;
        if (!codec::EncodeRpcRow(row, &io_buf, &row_size)) {
            return std::make_shared<TabletRowHandler>(
                ::fesql::base::Status(::fesql::common::kRpcError, "encode row failed"));
        }
        request.set_row_size(row_size);
        request.set_row_slices(row.GetRowPtrCnt());
    }
    auto response = std::make_shared<::rtidb::api::QueryResponse>();
    cntl->set_timeout_ms(FLAGS_request_timeout_ms);
    auto callback = new rtidb::RpcCallback<rtidb::api::QueryResponse>(response, cntl);
    auto row_handler = std::make_shared<TabletRowHandler>(db, callback);
    if (!client->SubQuery(request, callback)) {
        return std::make_shared<TabletRowHandler>(
            ::fesql::base::Status(::fesql::common::kRpcError, "send request failed"));
    }
    return row_handler;
}

std::shared_ptr<::fesql::vm::TableHandler> TabletAccessor::SubQuery(uint32_t task_id, const std::string& db,
                                                                    const std::string& sql,
                                                                    const std::vector<::fesql::codec::Row>& rows,
                                                                    const bool is_procedure, const bool is_debug) {
    DLOG(INFO) << "SubQuery batch request, taskid=" << task_id << ", is_procedure=" << is_procedure;
    auto client = GetClient();
    if (!client) {
        return std::make_shared<fesql::vm::ErrorTableHandler>(::fesql::common::kRpcError, "get client failed");
    }
    auto cntl = std::make_shared<brpc::Controller>();
    auto& io_buf = cntl->request_attachment();

    ::rtidb::api::SQLBatchRequestQueryRequest request;
    if (is_procedure) {
        request.set_sp_name(sql);
    } else {
        request.set_sql(sql);
    }
    request.set_is_procedure(is_procedure);
    request.set_db(db);
    request.set_task_id(task_id);
    request.set_is_debug(is_debug);
    bool common_slice_empty = false;

    if (common_slice_empty) {
        if (rows.empty()) {
            request.set_common_slices(0);
        } else {
            size_t common_slice_size = 0;
            if (!codec::EncodeRpcRow(rows[0], &io_buf, &common_slice_size)) {
                return std::make_shared<::fesql::vm::ErrorTableHandler>(::fesql::common::kBadRequest,
                                                                        "encode common row buf failed");
            }
            request.add_row_sizes(common_slice_size);
            request.set_common_slices(rows[0].GetRowPtrCnt());
        }
    } else {
        for (const auto& row : rows) {
            size_t uncommon_slice_size = 0;
            if (!codec::EncodeRpcRow(row, &io_buf, &uncommon_slice_size)) {
                return std::make_shared<::fesql::vm::ErrorTableHandler>(::fesql::common::kBadRequest,
                                                                        "encode uncommon row buf failed");
            }
            request.add_row_sizes(uncommon_slice_size);
            request.set_non_common_slices(row.GetRowPtrCnt());
        }
    }
    auto response = std::make_shared<::rtidb::api::SQLBatchRequestQueryResponse>();
    cntl->set_timeout_ms(FLAGS_request_timeout_ms);
    auto callback = new rtidb::RpcCallback<rtidb::api::SQLBatchRequestQueryResponse>(response, cntl);
    auto async_table_handler = std::make_shared<AsyncTableHandler>(callback);
    if (!client->SubBatchRequestQuery(request, callback)) {
        LOG(WARNING) << "fail to query tablet";
        return std::make_shared<::fesql::vm::ErrorTableHandler>(::fesql::common::kRpcError,
                                                                "fail to batch request query");
    }
    return async_table_handler;
}
std::shared_ptr<fesql::vm::RowHandler> TabletsAccessor::SubQuery(uint32_t task_id, const std::string& db,
                                                                 const std::string& sql, const fesql::codec::Row& row,
                                                                 const bool is_procedure, const bool is_debug) {
    return std::make_shared<::fesql::vm::ErrorRowHandler>(::fesql::common::kRpcError,
                                                          "TabletsAccessor Unsupport SubQuery with request");
}
std::shared_ptr<fesql::vm::TableHandler> TabletsAccessor::SubQuery(uint32_t task_id, const std::string& db,
                                                                   const std::string& sql,
                                                                   const std::vector<fesql::codec::Row>& rows,
                                                                   const bool is_procedure, const bool is_debug) {
    auto tables_handler = std::make_shared<AsyncTablesHandler>();
    std::vector<std::vector<fesql::vm::Row>> accessors_rows(accessors_.size());
    for (size_t idx = 0; idx < rows.size(); idx++) {
        accessors_rows[assign_accessor_idxs_[idx]].push_back(rows[idx]);
    }
    for (size_t idx = 0; idx < accessors_.size(); idx++) {
        tables_handler->AddAsyncRpcHandler(
            accessors_[idx]->SubQuery(task_id, db, sql, accessors_rows[idx], is_procedure, is_debug), posinfos_[idx]);
    }
    return tables_handler;
}
PartitionClientManager::PartitionClientManager(uint32_t pid, const std::shared_ptr<TabletAccessor>& leader,
                                               const std::vector<std::shared_ptr<TabletAccessor>>& followers)
    : pid_(pid), leader_(leader), followers_(followers), rand_(0xdeadbeef) {}

std::shared_ptr<TabletAccessor> PartitionClientManager::GetFollower() {
    if (!followers_.empty()) {
        uint32_t it = rand_.Next() % followers_.size();
        return followers_[it];
    }
    return std::shared_ptr<TabletAccessor>();
}

TableClientManager::TableClientManager(const TablePartitions& partitions, const ClientManager& client_manager) {
    for (const auto& table_partition : partitions) {
        uint32_t pid = table_partition.pid();
        if (pid > partition_managers_.size()) {
            continue;
        }
        std::shared_ptr<TabletAccessor> leader;
        std::vector<std::shared_ptr<TabletAccessor>> follower;
        for (const auto& meta : table_partition.partition_meta()) {
            if (meta.is_alive()) {
                auto client = client_manager.GetTablet(meta.endpoint());
                if (!client) {
                    continue;
                }
                if (meta.is_leader()) {
                    leader = client;
                } else {
                    follower.push_back(client);
                }
            }
        }
        partition_managers_.push_back(std::make_shared<PartitionClientManager>(pid, leader, follower));
    }
}

TableClientManager::TableClientManager(const ::rtidb::storage::TableSt& table_st, const ClientManager& client_manager) {
    for (const auto& partition_st : *(table_st.GetPartitions())) {
        uint32_t pid = partition_st.GetPid();
        if (pid > partition_managers_.size()) {
            continue;
        }
        std::shared_ptr<TabletAccessor> leader = client_manager.GetTablet(partition_st.GetLeader());
        std::vector<std::shared_ptr<TabletAccessor>> follower;
        for (const auto& endpoint : partition_st.GetFollower()) {
            auto client = client_manager.GetTablet(endpoint);
            if (client) {
                follower.push_back(client);
            }
        }
        partition_managers_.push_back(std::make_shared<PartitionClientManager>(pid, leader, follower));
    }
}

bool TableClientManager::UpdatePartitionClientManager(const ::rtidb::storage::PartitionSt& partition,
                                                      const ClientManager& client_manager) {
    uint32_t pid = partition.GetPid();
    if (pid > partition_managers_.size()) {
        return false;
    }
    auto leader = client_manager.GetTablet(partition.GetLeader());
    if (!leader) {
        return false;
    }
    std::vector<std::shared_ptr<TabletAccessor>> followers;
    for (const auto& endpoint : partition.GetFollower()) {
        auto client = client_manager.GetTablet(endpoint);
        if (!client) {
            return false;
        }
        followers.push_back(client);
    }
    auto partition_manager = std::make_shared<PartitionClientManager>(pid, leader, followers);
    std::atomic_store_explicit(&partition_managers_[pid], partition_manager, std::memory_order_relaxed);
    return true;
}

std::shared_ptr<TabletAccessor> ClientManager::GetTablet(const std::string& name) const {
    std::lock_guard<::rtidb::base::SpinMutex> lock(mu_);
    auto it = clients_.find(name);
    if (it == clients_.end()) {
        return std::shared_ptr<TabletAccessor>();
    }
    return it->second;
}

std::shared_ptr<TabletAccessor> ClientManager::GetTablet() const {
    std::lock_guard<::rtidb::base::SpinMutex> lock(mu_);
    if (clients_.empty()) {
        return std::shared_ptr<TabletAccessor>();
    }
    uint32_t seq = rand_.Uniform(clients_.size());
    uint32_t cnt = 0;
    for (const auto& kv : clients_) {
        if (cnt == seq) {
            return kv.second;
        }
        cnt++;
    }
    return std::shared_ptr<TabletAccessor>();
}

bool ClientManager::UpdateClient(const std::map<std::string, std::string>& endpoint_map) {
    if (endpoint_map.empty()) {
        DLOG(INFO) << "endpoint_map is empty";
        return true;
    }
    std::lock_guard<::rtidb::base::SpinMutex> lock(mu_);
    for (const auto& kv : endpoint_map) {
        auto it = real_endpoint_map_.find(kv.first);
        if (it == real_endpoint_map_.end()) {
            auto wrapper = std::make_shared<TabletAccessor>(kv.first);
            if (!wrapper->UpdateClient(kv.second)) {
                LOG(WARNING) << "add client failed. name " << kv.first << ", endpoint " << kv.second;
                continue;
            }
            LOG(INFO) << "add client. name " << kv.first << ", endpoint " << kv.second;
            clients_.emplace(kv.first, wrapper);
            real_endpoint_map_.emplace(kv.first, kv.second);
            continue;
        }
        if (it->second != kv.second) {
            auto client_it = clients_.find(kv.first);
            LOG(INFO) << "update client " << kv.first << "from " << it->second << " to " << kv.second;
            if (!client_it->second->UpdateClient(kv.second)) {
                LOG(WARNING) << "update client failed. name " << kv.first << ", endpoint " << kv.second;
                continue;
            }
            it->second = kv.second;
        }
    }
    return true;
}

bool ClientManager::UpdateClient(
    const std::map<std::string, std::shared_ptr<::rtidb::client::TabletClient>>& tablet_clients) {
    std::lock_guard<::rtidb::base::SpinMutex> lock(mu_);
    for (const auto& kv : tablet_clients) {
        auto it = real_endpoint_map_.find(kv.first);
        if (it == real_endpoint_map_.end()) {
            auto wrapper = std::make_shared<TabletAccessor>(kv.first, kv.second);
            DLOG(INFO) << "add client. name " << kv.first << ", endpoint " << kv.second->GetRealEndpoint();
            clients_.emplace(kv.first, wrapper);
            real_endpoint_map_.emplace(kv.first, kv.second->GetRealEndpoint());
            continue;
        }
        if (it->second != kv.second->GetRealEndpoint()) {
            auto client_it = clients_.find(kv.first);
            LOG(INFO) << "update client " << kv.first << " from " << it->second << " to "
                      << kv.second->GetRealEndpoint();
            if (!client_it->second->UpdateClient(kv.second)) {
                LOG(WARNING) << "update client failed. name " << kv.first << ", endpoint "
                             << kv.second->GetRealEndpoint();
                continue;
            }
            it->second = kv.second->GetRealEndpoint();
        }
    }
    return true;
}

}  // namespace catalog
}  // namespace rtidb
