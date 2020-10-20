//
// schema.cc
// Copyright (C) 2017 4paradigm.com
// Author denglong
// Date 2020-03-11
//

#include "storage/schema.h"

#include <utility>

namespace rtidb {
namespace storage {

ColumnDef::ColumnDef(const std::string& name, uint32_t id, ::rtidb::type::DataType type, bool not_null)
    : name_(name), id_(id), type_(type), not_null_(not_null) {}

std::shared_ptr<ColumnDef> TableColumn::GetColumn(uint32_t idx) {
    if (idx < columns_.size()) {
        return columns_.at(idx);
    }
    return std::shared_ptr<ColumnDef>();
}

std::shared_ptr<ColumnDef> TableColumn::GetColumn(const std::string& name) {
    auto it = column_map_.find(name);
    if (it != column_map_.end()) {
        return it->second;
    } else {
        return std::shared_ptr<ColumnDef>();
    }
}

const std::vector<std::shared_ptr<ColumnDef>>& TableColumn::GetAllColumn() { return columns_; }

const std::vector<uint32_t>& TableColumn::GetBlobIdxs() { return blob_idxs_; }

void TableColumn::AddColumn(std::shared_ptr<ColumnDef> column_def) {
    columns_.push_back(column_def);
    column_map_.insert(std::make_pair(column_def->GetName(), column_def));
    if (column_def->GetType() == rtidb::type::kBlob) {
        blob_idxs_.push_back(column_def->GetId());
    }
}

IndexDef::IndexDef(const std::string& name, uint32_t id) : name_(name), index_id_(id), status_(IndexStatus::kReady) {}

IndexDef::IndexDef(const std::string& name, uint32_t id, IndexStatus status)
    : name_(name), index_id_(id), status_(status) {}

IndexDef::IndexDef(const std::string& name, uint32_t id, const IndexStatus& status, ::rtidb::type::IndexType type,
                   const std::vector<ColumnDef>& columns)
    : name_(name), index_id_(id), status_(status), type_(type), columns_(columns) {}

bool ColumnDefSortFunc(const ColumnDef& cd_a, const ColumnDef& cd_b) { return (cd_a.GetId() < cd_b.GetId()); }

TableIndex::TableIndex() {
    indexs_ = std::make_shared<std::vector<std::shared_ptr<IndexDef>>>();
    pk_index_ = std::shared_ptr<IndexDef>();
    combine_col_name_map_ = std::make_shared<std::unordered_map<std::string, std::shared_ptr<IndexDef>>>();
    col_name_vec_ = std::make_shared<std::vector<std::string>>();
    unique_col_name_vec_ = std::make_shared<std::vector<std::string>>();
}

void TableIndex::ReSet() {
    auto new_indexs = std::make_shared<std::vector<std::shared_ptr<IndexDef>>>();
    std::atomic_store_explicit(&indexs_, new_indexs, std::memory_order_relaxed);

    pk_index_ = std::shared_ptr<IndexDef>();

    auto new_map = std::make_shared<std::unordered_map<std::string, std::shared_ptr<IndexDef>>>();
    std::atomic_store_explicit(&combine_col_name_map_, new_map, std::memory_order_relaxed);

    auto new_vec = std::make_shared<std::vector<std::string>>();
    std::atomic_store_explicit(&col_name_vec_, new_vec, std::memory_order_relaxed);

    auto new_unique_vec = std::make_shared<std::vector<std::string>>();
    std::atomic_store_explicit(&unique_col_name_vec_, new_unique_vec, std::memory_order_relaxed);
}

std::shared_ptr<IndexDef> TableIndex::GetIndex(uint32_t idx) {
    auto indexs = std::atomic_load_explicit(&indexs_, std::memory_order_relaxed);
    if (idx < indexs->size()) {
        return indexs->at(idx);
    }
    return std::shared_ptr<IndexDef>();
}

std::shared_ptr<IndexDef> TableIndex::GetIndex(const std::string& name) {
    auto indexs = std::atomic_load_explicit(&indexs_, std::memory_order_relaxed);
    for (const auto& index : *indexs) {
        if (index->GetName() == name) {
            return index;
        }
    }
    return std::shared_ptr<IndexDef>();
}

std::vector<std::shared_ptr<IndexDef>> TableIndex::GetAllIndex() {
    return *std::atomic_load_explicit(&indexs_, std::memory_order_relaxed);
}

int TableIndex::AddIndex(std::shared_ptr<IndexDef> index_def) {
    auto old_indexs = std::atomic_load_explicit(&indexs_, std::memory_order_relaxed);
    if (old_indexs->size() >= MAX_INDEX_NUM) {
        return -1;
    }
    auto new_indexs = std::make_shared<std::vector<std::shared_ptr<IndexDef>>>(*old_indexs);
    new_indexs->push_back(index_def);
    std::atomic_store_explicit(&indexs_, new_indexs, std::memory_order_relaxed);
    if (index_def->GetType() == ::rtidb::type::kPrimaryKey || index_def->GetType() == ::rtidb::type::kAutoGen) {
        pk_index_ = index_def;
    }

    auto old_vec = std::atomic_load_explicit(&col_name_vec_, std::memory_order_relaxed);
    auto new_vec = std::make_shared<std::vector<std::string>>(*old_vec);
    auto old_unique_vec = std::atomic_load_explicit(&unique_col_name_vec_, std::memory_order_relaxed);
    auto new_unique_vec = std::make_shared<std::vector<std::string>>(*old_unique_vec);
    std::string combine_name = "";
    for (auto& col_def : index_def->GetColumns()) {
        if (!combine_name.empty()) {
            combine_name.append("_");
        }
        combine_name.append(col_def.GetName());
        new_vec->push_back(col_def.GetName());
        if (index_def->GetType() == ::rtidb::type::kUnique) {
            new_unique_vec->push_back(col_def.GetName());
        }
    }
    std::atomic_store_explicit(&col_name_vec_, new_vec, std::memory_order_relaxed);
    std::atomic_store_explicit(&unique_col_name_vec_, new_unique_vec, std::memory_order_relaxed);

    auto old_map = std::atomic_load_explicit(&combine_col_name_map_, std::memory_order_relaxed);
    auto new_map = std::make_shared<std::unordered_map<std::string, std::shared_ptr<IndexDef>>>(*old_map);
    new_map->insert(std::make_pair(combine_name, index_def));
    std::atomic_store_explicit(&combine_col_name_map_, new_map, std::memory_order_relaxed);
    return 0;
}

bool TableIndex::HasAutoGen() {
    if (pk_index_->GetType() == ::rtidb::type::kAutoGen) {
        return true;
    }
    return false;
}

std::shared_ptr<IndexDef> TableIndex::GetPkIndex() { return pk_index_; }

const std::shared_ptr<IndexDef> TableIndex::GetIndexByCombineStr(const std::string& combine_str) {
    auto map = std::atomic_load_explicit(&combine_col_name_map_, std::memory_order_relaxed);
    auto it = map->find(combine_str);
    if (it != map->end()) {
        return it->second;
    } else {
        return std::shared_ptr<IndexDef>();
    }
}

bool TableIndex::IsColName(const std::string& name) {
    auto vec = std::atomic_load_explicit(&col_name_vec_, std::memory_order_relaxed);
    auto iter = std::find(vec->begin(), vec->end(), name);
    if (iter == vec->end()) {
        return false;
    }
    return true;
}

bool TableIndex::IsUniqueColName(const std::string& name) {
    auto vec = std::atomic_load_explicit(&unique_col_name_vec_, std::memory_order_relaxed);
    auto iter = std::find(vec->begin(), vec->end(), name);
    if (iter == vec->end()) {
        return false;
    }
    return true;
}

PartitionSt::PartitionSt(const ::rtidb::nameserver::TablePartition& partitions) : pid_(partitions.pid()) {
    for (const auto& meta : partitions.partition_meta()) {
        if (!meta.is_alive()) {
            continue;
        }
        if (meta.is_leader()) {
            leader_ = meta.endpoint();
        } else {
            follower_.push_back(meta.endpoint());
        }
    }
}

PartitionSt::PartitionSt(const ::rtidb::common::TablePartition& partitions) : pid_(partitions.pid()) {
    for (const auto& meta : partitions.partition_meta()) {
        if (!meta.is_alive()) {
            continue;
        }
        if (meta.is_leader()) {
            leader_ = meta.endpoint();
        } else {
            follower_.push_back(meta.endpoint());
        }
    }
}

bool PartitionSt::operator==(const PartitionSt& partition_st) const {
    if (pid_ != partition_st.GetPid()) {
        return false;
    }
    if (leader_ != partition_st.GetLeader()) {
        return false;
    }
    const std::vector<std::string>& o_follower = partition_st.GetFollower();
    if (follower_.size() != o_follower.size()) {
        return false;
    }
    for (const auto& endpoint : follower_) {
        if (std::find(o_follower.begin(), o_follower.end(), endpoint) == o_follower.end()) {
            return false;
        }
    }
    return true;
}

TableSt::TableSt(const ::rtidb::nameserver::TableInfo& table_info)
    : name_(table_info.name()),
      db_(table_info.db()),
      tid_(table_info.tid()),
      pid_num_(table_info.table_partition_size()),
      column_desc_(table_info.column_desc_v1()),
      column_key_(table_info.column_key()) {
    partitions_ = std::make_shared<std::vector<PartitionSt>>();
    for (const auto& table_partition : table_info.table_partition()) {
        uint32_t pid = table_partition.pid();
        if (pid > partitions_->size()) {
            continue;
        }
        partitions_->emplace_back(PartitionSt(table_partition));
    }
}

TableSt::TableSt(const ::rtidb::api::TableMeta& meta)
    : name_(meta.name()),
      db_(meta.db()),
      tid_(meta.tid()),
      pid_num_(meta.table_partition_size()),
      column_desc_(meta.column_desc()),
      column_key_(meta.column_key()) {
    partitions_ = std::make_shared<std::vector<PartitionSt>>();
    for (const auto& table_partition : meta.table_partition()) {
        uint32_t pid = table_partition.pid();
        if (pid > partitions_->size()) {
            continue;
        }
        partitions_->emplace_back(PartitionSt(table_partition));
    }
}

bool TableSt::SetPartition(const PartitionSt& partition_st) {
    uint32_t pid = partition_st.GetPid();
    if (pid >= pid_num_) {
        return false;
    }
    auto old_partitions = GetPartitions();
    auto new_partitions = std::make_shared<std::vector<PartitionSt>>(*old_partitions);
    (*new_partitions)[pid] = partition_st;
    std::atomic_store_explicit(&partitions_, new_partitions, std::memory_order_relaxed);
    return true;
}

}  // namespace storage
}  // namespace rtidb
