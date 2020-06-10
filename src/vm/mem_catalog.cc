/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * mem_catalog.cc
 *
 * Author: chenjing
 * Date: 2020/3/25
 *--------------------------------------------------------------------------
 **/
#include "vm/mem_catalog.h"
#include <algorithm>
namespace fesql {
namespace vm {
MemTimeTableIterator::MemTimeTableIterator(const MemTimeTable* table,
                                           const vm::Schema* schema)
    : table_(table),
      schema_(schema),
      start_iter_(table->cbegin()),
      end_iter_(table->cend()),
      iter_(table->cbegin()) {}
MemTimeTableIterator::MemTimeTableIterator(const MemTimeTable* table,
                                           const vm::Schema* schema,
                                           int32_t start, int32_t end)
    : table_(table),
      schema_(schema),
      start_iter_(table_->begin() + start),
      end_iter_(table_->begin() + end),
      iter_(start_iter_) {}
MemTimeTableIterator::~MemTimeTableIterator() {}

// TODO(chenjing): speed up seek for memory iterator
void MemTimeTableIterator::Seek(const uint64_t& ts) {
    iter_ = start_iter_;
    while (iter_ != end_iter_) {
        if (iter_->first <= ts) {
            return;
        }
        iter_++;
    }
}
void MemTimeTableIterator::SeekToFirst() { iter_ = start_iter_; }
const uint64_t& MemTimeTableIterator::GetKey() const { return iter_->first; }
const Row& fesql::vm::MemTimeTableIterator::GetValue() { return iter_->second; }
void MemTimeTableIterator::Next() { iter_++; }
bool MemTimeTableIterator::Valid() const { return end_iter_ > iter_; }
bool MemTimeTableIterator::IsSeekable() const { return true; }
MemWindowIterator::MemWindowIterator(const MemSegmentMap* partitions,
                                     const Schema* schema)
    : WindowIterator(),
      partitions_(partitions),
      schema_(schema),
      iter_(partitions->cbegin()),
      start_iter_(partitions->cbegin()),
      end_iter_(partitions->cend()) {}

MemWindowIterator::~MemWindowIterator() {}

void MemWindowIterator::Seek(const std::string& key) {
    iter_ = partitions_->find(key);
}
void MemWindowIterator::SeekToFirst() { iter_ = start_iter_; }
void MemWindowIterator::Next() { iter_++; }
bool MemWindowIterator::Valid() { return end_iter_ != iter_; }
std::unique_ptr<RowIterator> MemWindowIterator::GetValue() {
    return std::unique_ptr<RowIterator>(
        new MemTimeTableIterator(&(iter_->second), schema_));
}
const Row MemWindowIterator::GetKey() { return Row(iter_->first); }

MemTimeTableHandler::MemTimeTableHandler()
    : TableHandler(),
      table_name_(""),
      db_(""),
      schema_(nullptr),
      types_(),
      index_hint_(),
      table_(),
      order_type_(kNoneOrder) {}
MemTimeTableHandler::MemTimeTableHandler(const Schema* schema)
    : TableHandler(),
      table_name_(""),
      db_(""),
      schema_(schema),
      types_(),
      index_hint_(),
      table_(),
      order_type_(kNoneOrder) {}
MemTimeTableHandler::MemTimeTableHandler(const std::string& table_name,
                                         const std::string& db,
                                         const Schema* schema)
    : TableHandler(),
      table_name_(table_name),
      db_(db),
      schema_(schema),
      types_(),
      index_hint_(),
      table_(),
      order_type_(kNoneOrder) {}

MemTimeTableHandler::~MemTimeTableHandler() {}
std::unique_ptr<RowIterator> MemTimeTableHandler::GetIterator() const {
    std::unique_ptr<MemTimeTableIterator> it(
        new MemTimeTableIterator(&table_, schema_));
    return std::move(it);
}
std::unique_ptr<WindowIterator> MemTimeTableHandler::GetWindowIterator(
    const std::string& idx_name) {
    return std::unique_ptr<WindowIterator>();
}

void MemTimeTableHandler::AddRow(const uint64_t key, const Row& row) {
    table_.emplace_back(std::make_pair(key, row));
}

void MemTimeTableHandler::AddFrontRow(const uint64_t key, const Row& row) {
    table_.emplace_front(std::make_pair(key, row));
}
void MemTimeTableHandler::PopBackRow() { table_.pop_back(); }

void MemTimeTableHandler::PopFrontRow() { table_.pop_front(); }

void MemTimeTableHandler::AddRow(const Row& row) {
    table_.emplace_back(std::make_pair(0, row));
}
void MemTimeTableHandler::AddFrontRow(const Row& row) {
    table_.emplace_front(std::make_pair(0, row));
}
const Types& MemTimeTableHandler::GetTypes() { return types_; }

void MemTimeTableHandler::Sort(const bool is_asc) {
    if (is_asc) {
        AscComparor comparor;
        std::sort(table_.begin(), table_.end(), comparor);
        order_type_ = kAscOrder;
    } else {
        DescComparor comparor;
        std::sort(table_.begin(), table_.end(), comparor);
        order_type_ = kDescOrder;
    }
}
void MemTimeTableHandler::Reverse() {
    std::reverse(table_.begin(), table_.end());
    order_type_ = kAscOrder == order_type_
                      ? kDescOrder
                      : kDescOrder == order_type_ ? kAscOrder : kNoneOrder;
}
RowIterator* MemTimeTableHandler::GetIterator(int8_t* addr) const {
    if (nullptr == addr) {
        return new MemTimeTableIterator(&table_, schema_);
    } else {
        return new (addr) MemTimeTableIterator(&table_, schema_);
    }
}

MemPartitionHandler::MemPartitionHandler()
    : PartitionHandler(),
      table_name_(""),
      db_(""),
      schema_(nullptr),
      order_type_(kNoneOrder) {}

MemPartitionHandler::MemPartitionHandler(const Schema* schema)
    : PartitionHandler(),
      table_name_(""),
      db_(""),
      schema_(schema),
      order_type_(kNoneOrder) {}
MemPartitionHandler::MemPartitionHandler(const std::string& table_name,
                                         const std::string& db,
                                         const Schema* schema)
    : PartitionHandler(),
      table_name_(table_name),
      db_(db),
      schema_(schema),
      order_type_(kNoneOrder) {}
MemPartitionHandler::~MemPartitionHandler() {}
const Schema* MemPartitionHandler::GetSchema() { return schema_; }
const std::string& MemPartitionHandler::GetName() { return table_name_; }
const std::string& MemPartitionHandler::GetDatabase() { return db_; }
const Types& MemPartitionHandler::GetTypes() { return types_; }
const IndexHint& MemPartitionHandler::GetIndex() { return index_hint_; }
bool MemPartitionHandler::AddRow(const std::string& key, uint64_t ts,
                                 const Row& row) {
    auto iter = partitions_.find(key);
    if (iter == partitions_.cend()) {
        partitions_.insert(std::pair<std::string, MemTimeTable>(
            key, {std::make_pair(ts, row)}));
    } else {
        iter->second.push_back(std::make_pair(ts, row));
    }
    return true;
}
std::unique_ptr<WindowIterator> MemPartitionHandler::GetWindowIterator() {
    return std::unique_ptr<WindowIterator>(
        new MemWindowIterator(&partitions_, schema_));
}
void MemPartitionHandler::Sort(const bool is_asc) {
    if (is_asc) {
        AscComparor comparor;
        for (auto& segment : partitions_) {
            std::sort(segment.second.begin(), segment.second.end(), comparor);
        }
        order_type_ = kAscOrder;
    } else {
        DescComparor comparor;
        for (auto& segment : partitions_) {
            std::sort(segment.second.begin(), segment.second.end(), comparor);
        }
        order_type_ = kDescOrder;
    }
}
void MemPartitionHandler::Reverse() {
    for (auto& segment : partitions_) {
        std::reverse(segment.second.begin(), segment.second.end());
    }
    order_type_ = kAscOrder == order_type_
                      ? kDescOrder
                      : kDescOrder == order_type_ ? kAscOrder : kNoneOrder;
}
void MemPartitionHandler::Print() {
    for (auto iter = partitions_.cbegin(); iter != partitions_.cend(); iter++) {
        std::cout << iter->first << ":";
        for (auto segment_iter = iter->second.cbegin();
             segment_iter != iter->second.cend(); segment_iter++) {
            std::cout << segment_iter->first << ",";
        }
        std::cout << std::endl;
    }
}

std::unique_ptr<WindowIterator> MemTableHandler::GetWindowIterator(
    const std::string& idx_name) {
    return std::unique_ptr<WindowIterator>();
}
std::unique_ptr<RowIterator> MemTableHandler::GetIterator() const {
    std::unique_ptr<MemTableIterator> it(
        new MemTableIterator(&table_, schema_));
    return std::move(it);
}
RowIterator* MemTableHandler::GetIterator(int8_t* addr) const {
    return new (addr) MemTableIterator(&table_, schema_);
}

MemTableHandler::MemTableHandler()
    : TableHandler(),
      table_name_(""),
      db_(""),
      schema_(nullptr),
      types_(),
      index_hint_(),
      table_(),
      order_type_(kNoneOrder) {}
MemTableHandler::MemTableHandler(const Schema* schema)
    : TableHandler(),
      table_name_(""),
      db_(""),
      schema_(schema),
      types_(),
      index_hint_(),
      table_(),
      order_type_(kNoneOrder) {}
MemTableHandler::MemTableHandler(const std::string& table_name,
                                 const std::string& db, const Schema* schema)
    : TableHandler(),
      table_name_(table_name),
      db_(db),
      schema_(schema),
      types_(),
      index_hint_(),
      table_(),
      order_type_(kNoneOrder) {}
void MemTableHandler::AddRow(const Row& row) { table_.push_back(row); }
void MemTableHandler::Reverse() {
    std::reverse(table_.begin(), table_.end());
    order_type_ = kAscOrder == order_type_
                      ? kDescOrder
                      : kDescOrder == order_type_ ? kAscOrder : kNoneOrder;
}
MemTableHandler::~MemTableHandler() {}
MemTableIterator::MemTableIterator(const MemTable* table,
                                   const vm::Schema* schema)
    : table_(table),
      schema_(schema),
      start_iter_(table->cbegin()),
      end_iter_(table->cend()),
      iter_(table->cbegin()),
      key_(0) {}
MemTableIterator::MemTableIterator(const MemTable* table,
                                   const vm::Schema* schema, int32_t start,
                                   int32_t end)
    : table_(table),
      schema_(schema),
      start_iter_(table_->begin() + start),
      end_iter_(table_->begin() + end),
      iter_(start_iter_),
      key_(0) {}
MemTableIterator::~MemTableIterator() {}
void MemTableIterator::Seek(const uint64_t& ts) { iter_ = start_iter_ + ts; }
void MemTableIterator::SeekToFirst() { iter_ = start_iter_; }
const uint64_t& MemTableIterator::GetKey() const { return key_; }

bool MemTableIterator::Valid() const { return end_iter_ > iter_; }
void MemTableIterator::Next() {
    iter_++;
    key_ = Valid() ? iter_ - start_iter_ : 0;
}
const Row& MemTableIterator::GetValue() { return *iter_; }
bool MemTableIterator::IsSeekable() const { return true; }

// row iter interfaces for llvm
void GetRowIter(int8_t* input, int8_t* iter_addr) {
    auto handler = reinterpret_cast<ListV<Row>*>(input);
    auto local_iter =
        new (iter_addr) std::unique_ptr<RowIterator>(handler->GetIterator());
    (*local_iter)->SeekToFirst();
}
bool RowIterHasNext(int8_t* iter_ptr) {
    auto& local_iter =
        *reinterpret_cast<std::unique_ptr<RowIterator>*>(iter_ptr);
    return local_iter->Valid();
}
void RowIterNext(int8_t* iter_ptr) {
    auto& local_iter =
        *reinterpret_cast<std::unique_ptr<RowIterator>*>(iter_ptr);
    local_iter->Next();
}
int8_t* RowIterGetCurSlice(int8_t* iter_ptr, size_t idx) {
    auto& local_iter =
        *reinterpret_cast<std::unique_ptr<RowIterator>*>(iter_ptr);
    const Row& row = local_iter->GetValue();
    return row.buf(idx);
}
size_t RowIterGetCurSliceSize(int8_t* iter_ptr, size_t idx) {
    auto& local_iter =
        *reinterpret_cast<std::unique_ptr<RowIterator>*>(iter_ptr);
    const Row& row = local_iter->GetValue();
    return row.size(idx);
}
void RowIterDelete(int8_t* iter_ptr) {
    auto& local_iter =
        *reinterpret_cast<std::unique_ptr<RowIterator>*>(iter_ptr);
    local_iter = nullptr;
}

}  // namespace vm
}  // namespace fesql
