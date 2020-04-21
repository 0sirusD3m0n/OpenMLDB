//
// snapshot.h
// Copyright (C) 2017 4paradigm.com
// Author vagrant
// Date 2017-07-24
//
#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "base/schema_codec.h"
#include "log/log_reader.h"
#include "log/log_writer.h"
#include "log/sequential_file.h"
#include "proto/tablet.pb.h"
#include "storage/snapshot.h"

using ::rtidb::api::LogEntry;
namespace rtidb {
namespace base {
class Status;
}
namespace storage {

using ::rtidb::log::WriteHandle;

typedef ::rtidb::base::Skiplist<uint32_t, uint64_t,
                                ::rtidb::base::DefaultComparator>
    LogParts;

// table snapshot
class MemTableSnapshot : public Snapshot {
 public:
    MemTableSnapshot(uint32_t tid, uint32_t pid, LogParts* log_part,
                     const std::string& db_root_path);

    virtual ~MemTableSnapshot() = default;

    bool Init() override;

    bool Recover(std::shared_ptr<Table> table,
                 uint64_t& latest_offset) override;

    void RecoverFromSnapshot(const std::string& snapshot_name,
                             uint64_t expect_cnt, std::shared_ptr<Table> table);

    int MakeSnapshot(std::shared_ptr<Table> table,
                     uint64_t& out_offset,  // NOLINT
                     uint64_t end_offset) override;

    int TTLSnapshot(std::shared_ptr<Table> table,
                    const ::rtidb::api::Manifest& manifest, WriteHandle* wh,
                    uint64_t& count, uint64_t& expired_key_num,  // NOLINT
                    uint64_t& deleted_key_num);                  // NOLINT

    void Put(std::string& path, std::shared_ptr<Table>& table,  // NOLINT
             std::vector<std::string*> recordPtr,
             std::atomic<uint64_t>* succ_cnt,
             std::atomic<uint64_t>* failed_cnt);

    int ExtractIndexFromSnapshot(
        std::shared_ptr<Table> table, const ::rtidb::api::Manifest& manifest,
        WriteHandle* wh, ::rtidb::common::ColumnKey& column_key,  // NOLINT
        uint32_t idx, uint32_t partition_num,
        std::vector<::rtidb::base::ColumnDesc> columns,  // NOLINT
        uint32_t max_idx, std::vector<uint32_t> index_cols,
        uint64_t& count,                                        // NOLINT
        uint64_t& expired_key_num, uint64_t& deleted_key_num);  // NOLINT

    bool DumpSnapshotIndexData(std::shared_ptr<Table> table,
                               const ::rtidb::common::ColumnKey& column_key,
                               std::vector<uint32_t>& index_cols,  // NOLINT
                               std::vector<::rtidb::base::ColumnDesc>& columns,  // NOLINT
                               std::set<uint32_t>& deleted_index,  // NOLINT
                               uint32_t max_idx, uint32_t idx,
                               std::vector<::rtidb::log::WriteHandle*>& whs, // NOLINT
                               uint64_t& latest_offset); // NOLINT

    int ExtractIndexData(std::shared_ptr<Table> table,
                         ::rtidb::common::ColumnKey& column_key,  // NOLINT
                         uint32_t idx, uint32_t partition_num,
                         uint64_t& out_offset);  // NOLINT

    bool DumpIndexData(std::shared_ptr<Table>& table,
                       const ::rtidb::common::ColumnKey& column_key,
                       uint32_t idx,
                       std::vector<::rtidb::log::WriteHandle*>& whs);

 private:
    // load single snapshot to table
    void RecoverSingleSnapshot(const std::string& path,
                               std::shared_ptr<Table> table,
                               std::atomic<uint64_t>* g_succ_cnt,
                               std::atomic<uint64_t>* g_failed_cnt);

    uint64_t CollectDeletedKey(uint64_t end_offset);

 private:
    LogParts* log_part_;
    std::string log_path_;
    std::map<std::string, uint64_t> deleted_keys_;
    std::string db_root_path_;
};

}  // namespace storage
}  // namespace rtidb
