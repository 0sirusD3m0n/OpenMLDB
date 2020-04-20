/*
 * tablet_server_impl.cc
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

#include "tablet/tablet_server_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "base/strings.h"
#include "gflags/gflags.h"

DECLARE_string(dbms_endpoint);
DECLARE_string(endpoint);
DECLARE_int32(port);
DECLARE_bool(enable_keep_alive);

#define FLAGS_dbms_endpoint std::string("127.0.0.1:8090")
#define FLAGS_endpoint "127.0.0.1:10892"

namespace fesql {
namespace tablet {

TabletServerImpl::TabletServerImpl()
    : slock_(), engine_(), catalog_(), dbms_ch_(NULL) {}

TabletServerImpl::~TabletServerImpl() { delete dbms_ch_; }

bool TabletServerImpl::Init() {
    catalog_ = std::shared_ptr<TabletCatalog>(new TabletCatalog());
    bool ok = catalog_->Init();
    if (!ok) {
        LOG(WARNING) << "fail to init catalog ";
        return false;
    }
    engine_ = std::move(std::unique_ptr<vm::Engine>(new vm::Engine(catalog_)));
    if (FLAGS_enable_keep_alive) {
        dbms_ch_ = new ::brpc::Channel();
        brpc::ChannelOptions options;
        int ret = dbms_ch_->Init(FLAGS_dbms_endpoint.c_str(), &options);
        if (ret != 0) {
            return false;
        }
        KeepAlive();
    }
    LOG(INFO) << "init tablet ok";
    return true;
}

void TabletServerImpl::KeepAlive() {
    dbms::DBMSServer_Stub stub(dbms_ch_);
    std::string endpoint = FLAGS_endpoint;
    dbms::KeepAliveRequest request;
    request.set_endpoint(endpoint);
    dbms::KeepAliveResponse response;
    brpc::Controller cntl;
    stub.KeepAlive(&cntl, &request, &response, NULL);
}

void TabletServerImpl::CreateTable(RpcController* ctrl,
                                   const CreateTableRequest* request,
                                   CreateTableResponse* response,
                                   Closure* done) {
    brpc::ClosureGuard done_guard(done);
    ::fesql::common::Status* status = response->mutable_status();
    if (request->pids_size() == 0) {
        status->set_code(common::kBadRequest);
        status->set_msg("create table without pid");
        return;
    }
    if (request->tid() <= 0) {
        status->set_code(common::kBadRequest);
        status->set_msg("create table with invalid tid " +
                        std::to_string(request->tid()));
        return;
    }

    for (int32_t i = 0; i < request->pids_size(); ++i) {
        std::shared_ptr<storage::Table> table(new storage::Table(
            request->tid(), request->pids(i), request->table()));
        bool ok = table->Init();
        if (!ok) {
            LOG(WARNING) << "fail to init table storage for table "
                         << request->table().name();
            status->set_code(common::kBadRequest);
            status->set_msg("fail to init table storage");
            return;
        }
        ok = AddTableLocked(table);
        if (!ok) {
            LOG(WARNING) << "table with name " << request->table().name()
                         << " exists";
            status->set_code(common::kTableExists);
            status->set_msg("table exist");
            return;
        }
        // TODO(wangtaize) just one partition
        break;
    }
    status->set_code(common::kOk);
    DLOG(INFO) << "create table with name " << request->table().name()
               << " done";
}

void TabletServerImpl::Insert(RpcController* ctrl, const InsertRequest* request,
                              InsertResponse* response, Closure* done) {
    brpc::ClosureGuard done_guard(done);
    ::fesql::common::Status* status = response->mutable_status();
    if (request->db().empty() || request->table().empty()) {
        status->set_code(common::kBadRequest);
        status->set_msg("db or table name is empty");
        return;
    }
    std::shared_ptr<TabletTableHandler> handler =
        GetTableLocked(request->db(), request->table());

    if (!handler) {
        status->set_code(common::kTableNotFound);
        status->set_msg("table is not found");
        return;
    }

    bool ok =
        handler->GetTable()->Put(request->row().c_str(), request->row().size());
    if (!ok) {
        status->set_code(common::kTablePutFailed);
        status->set_msg("fail to put row");
        LOG(WARNING) << "fail to put data to table " << request->table()
                     << " with key " << request->key();
        return;
    }
    status->set_code(common::kOk);
}

void TabletServerImpl::Query(RpcController* ctrl, const QueryRequest* request,
                             QueryResponse* response, Closure* done) {
    brpc::ClosureGuard done_guard(done);
    common::Status* status = response->mutable_status();
    status->set_code(common::kOk);
    status->set_msg("ok");
    vm::BatchRunSession session;

    {
        base::Status base_status;
        auto compile_info =
            engine_->Get(request->sql(), request->db(), session, base_status);
        if (!compile_info) {
            status->set_msg(base_status.msg);
            status->set_code(base_status.code);
            LOG(WARNING) << base_status.msg;
            return;
        }
        //        std::ostringstream oss;
        //        session.GetPhysicalPlan()->Print(oss, "");
        //        std::cout << "physical plan:\n" << oss.str() << std::endl;
    }

    auto table = session.Run();

    if (!table) {
        LOG(WARNING) << "fail to run sql " << request->sql();
        status->set_code(common::kSQLError);
        status->set_msg("fail to run sql");
        return;
    }
    // TODO(wangtaize) opt the result buf
    auto iter = table->GetIterator();
    while (iter->Valid()) {
        int8_t* ptr = iter->GetValue().buf();
        iter->Next();
        response->add_result_set(ptr, *reinterpret_cast<uint32_t*>(ptr + 2));
        free(ptr);
    }
    response->mutable_schema()->CopyFrom(session.GetSchema());
    status->set_code(common::kOk);
}

void TabletServerImpl::GetTableSchema(RpcController* ctrl,
                                      const GetTablesSchemaRequest* request,
                                      GetTableSchemaReponse* response,
                                      Closure* done) {
    brpc::ClosureGuard done_guard(done);
    ::fesql::common::Status* status = response->mutable_status();
    std::shared_ptr<TabletTableHandler> handler =
        GetTableLocked(request->db(), request->name());
    if (!handler) {
        status->set_code(common::kTableNotFound);
        status->set_msg("table is not found");
        return;
    }
    response->mutable_schema()->CopyFrom(handler->GetTable()->GetTableDef());
}

}  // namespace tablet
}  // namespace fesql
