/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * catalog_wrapper.h
 *
 * Author: chenjing
 * Date: 2020/5/18
 *--------------------------------------------------------------------------
 **/

#ifndef SRC_VM_CATALOG_WRAPPER_H_
#define SRC_VM_CATALOG_WRAPPER_H_
#include <memory>
#include <string>
#include <utility>
#include "vm/catalog.h"
namespace fesql {
namespace vm {

class ProjectFun {
 public:
    virtual Row operator()(const Row& row) const = 0;
};
class PredicateFun {
 public:
    virtual bool operator()(const Row& row) const = 0;
};
class IteratorProjectWrapper : public RowIterator {
 public:
    IteratorProjectWrapper(std::unique_ptr<RowIterator> iter,
                           const ProjectFun* fun)
        : RowIterator(), iter_(std::move(iter)), fun_(fun), value_() {}
    virtual ~IteratorProjectWrapper() {}
    bool Valid() const override { return iter_->Valid(); }
    void Next() override { iter_->Next(); }
    const uint64_t& GetKey() const override { return iter_->GetKey(); }
    const Row& GetValue() override {
        value_ = fun_->operator()(iter_->GetValue());
        return value_;
    }
    void Seek(const uint64_t& k) override { iter_->Seek(k); }
    void SeekToFirst() override { iter_->SeekToFirst(); }
    bool IsSeekable() const override { return iter_->IsSeekable(); }
    std::unique_ptr<RowIterator> iter_;
    const ProjectFun* fun_;
    Row value_;
};
class IteratorFilterWrapper : public RowIterator {
 public:
    IteratorFilterWrapper(std::unique_ptr<RowIterator> iter,
                          const PredicateFun* fun)
        : RowIterator(), iter_(std::move(iter)), predicate_(fun) {}
    virtual ~IteratorFilterWrapper() {}
    bool Valid() const override {
        return iter_->Valid() && predicate_->operator()(iter_->GetValue());
    }
    void Next() override {
        iter_->Next();
        while (iter_->Valid() && !predicate_->operator()(iter_->GetValue())) {
            iter_->Next();
        }
    }
    const uint64_t& GetKey() const override { return iter_->GetKey(); }
    const Row& GetValue() override { return iter_->GetValue(); }
    void Seek(const uint64_t& k) override {
        iter_->Seek(k);
        while (iter_->Valid() && !predicate_->operator()(iter_->GetValue())) {
            iter_->Next();
        }
    }
    void SeekToFirst() override {
        iter_->SeekToFirst();
        while (iter_->Valid() && !predicate_->operator()(iter_->GetValue())) {
            iter_->Next();
        }
    }
    bool IsSeekable() const override { return iter_->IsSeekable(); }
    std::unique_ptr<RowIterator> iter_;
    const PredicateFun* predicate_;
};

class WindowIteratorProjectWrapper : public WindowIterator {
 public:
    WindowIteratorProjectWrapper(std::unique_ptr<WindowIterator> iter,
                                 const ProjectFun* fun)
        : WindowIterator(), iter_(std::move(iter)), fun_(fun) {}
    virtual ~WindowIteratorProjectWrapper() {}
    std::unique_ptr<RowIterator> GetValue() override {
        auto iter = iter_->GetValue();
        if (!iter) {
            return std::unique_ptr<RowIterator>();
        } else {
            return std::unique_ptr<RowIterator>(
                new IteratorProjectWrapper(std::move(iter), fun_));
        }
    }
    RowIterator* GetRawValue() override {
        auto iter = iter_->GetValue();
        if (!iter) {
            return nullptr;
        } else {
            return new IteratorProjectWrapper(std::move(iter), fun_);
        }
    }
    void Seek(const std::string& key) override { iter_->Seek(key); }
    void SeekToFirst() override { iter_->SeekToFirst(); }
    void Next() override { iter_->Next(); }
    bool Valid() override { return iter_->Valid(); }
    const Row GetKey() override { return iter_->GetKey(); }
    std::unique_ptr<WindowIterator> iter_;
    const ProjectFun* fun_;
};

class WindowIteratorFilterWrapper : public WindowIterator {
 public:
    WindowIteratorFilterWrapper(std::unique_ptr<WindowIterator> iter,
                                const PredicateFun* fun)
        : WindowIterator(), iter_(std::move(iter)), fun_(fun) {}
    virtual ~WindowIteratorFilterWrapper() {}
    std::unique_ptr<RowIterator> GetValue() override {
        auto iter = iter_->GetValue();
        if (!iter) {
            return std::unique_ptr<RowIterator>();
        } else {
            return std::unique_ptr<RowIterator>(
                new IteratorFilterWrapper(std::move(iter), fun_));
        }
    }
    RowIterator* GetRawValue() override {
        auto iter = iter_->GetValue();
        if (!iter) {
            return nullptr;
        } else {
            return new IteratorFilterWrapper(std::move(iter), fun_);
        }
    }
    void Seek(const std::string& key) override { iter_->Seek(key); }
    void SeekToFirst() override { iter_->SeekToFirst(); }
    void Next() override { iter_->Next(); }
    bool Valid() override { return iter_->Valid(); }
    const Row GetKey() override { return iter_->GetKey(); }
    std::unique_ptr<WindowIterator> iter_;
    const PredicateFun* fun_;
};

class TableProjectWrapper;
class TableFilterWrapper;

class PartitionProjectWrapper : public PartitionHandler {
 public:
    PartitionProjectWrapper(std::shared_ptr<PartitionHandler> partition_handler,
                            const ProjectFun* fun)
        : PartitionHandler(),
          partition_handler_(partition_handler),
          value_(),
          fun_(fun) {}
    virtual ~PartitionProjectWrapper() {}
    std::unique_ptr<WindowIterator> GetWindowIterator() override {
        auto iter = partition_handler_->GetWindowIterator();
        if (!iter) {
            return std::unique_ptr<WindowIterator>();
        } else {
            return std::unique_ptr<WindowIterator>(
                new WindowIteratorProjectWrapper(std::move(iter), fun_));
        }
    }
    const Types& GetTypes() override { return partition_handler_->GetTypes(); }
    const IndexHint& GetIndex() override {
        return partition_handler_->GetIndex();
    }

    const Schema* GetSchema() override {
        return partition_handler_->GetSchema();
    }
    const std::string& GetName() override {
        return partition_handler_->GetName();
    }
    const std::string& GetDatabase() override {
        return partition_handler_->GetDatabase();
    }
    std::unique_ptr<base::ConstIterator<uint64_t, Row>> GetIterator()
        const override {
        auto iter = partition_handler_->GetIterator();
        if (!iter) {
            return std::unique_ptr<RowIterator>();
        } else {
            return std::unique_ptr<RowIterator>(
                new IteratorProjectWrapper(std::move(iter), fun_));
        }
    }
    base::ConstIterator<uint64_t, Row>* GetRawIterator() const override;
    Row At(uint64_t pos) override {
        value_ = fun_->operator()(partition_handler_->At(pos));
        return value_;
    }
    const uint64_t GetCount() override {
        return partition_handler_->GetCount();
    }
    virtual std::shared_ptr<TableHandler> GetSegment(const std::string& key);
    virtual const OrderType GetOrderType() const {
        return partition_handler_->GetOrderType();
    }
    const std::string GetHandlerTypeName() override {
        return "PartitionHandler";
    }
    std::shared_ptr<PartitionHandler> partition_handler_;
    Row value_;
    const ProjectFun* fun_;
};
class PartitionFilterWrapper : public PartitionHandler {
 public:
    PartitionFilterWrapper(std::shared_ptr<PartitionHandler> partition_handler,
                           const PredicateFun* fun)
        : PartitionHandler(),
          partition_handler_(partition_handler),
          fun_(fun) {}
    virtual ~PartitionFilterWrapper() {}
    std::unique_ptr<WindowIterator> GetWindowIterator() override {
        auto iter = partition_handler_->GetWindowIterator();
        if (!iter) {
            return std::unique_ptr<WindowIterator>();
        } else {
            return std::unique_ptr<WindowIterator>(
                new WindowIteratorFilterWrapper(std::move(iter), fun_));
        }
    }
    const Types& GetTypes() override { return partition_handler_->GetTypes(); }
    const IndexHint& GetIndex() override {
        return partition_handler_->GetIndex();
    }

    const Schema* GetSchema() override {
        return partition_handler_->GetSchema();
    }
    const std::string& GetName() override {
        return partition_handler_->GetName();
    }
    const std::string& GetDatabase() override {
        return partition_handler_->GetDatabase();
    }
    std::unique_ptr<base::ConstIterator<uint64_t, Row>> GetIterator()
        const override {
        auto iter = partition_handler_->GetIterator();
        if (!iter) {
            return std::unique_ptr<base::ConstIterator<uint64_t, Row>>();
        } else {
            return std::unique_ptr<RowIterator>(
                new IteratorFilterWrapper(std::move(iter), fun_));
        }
    }
    base::ConstIterator<uint64_t, Row>* GetRawIterator() const override;
    virtual std::shared_ptr<TableHandler> GetSegment(const std::string& key);
    virtual const OrderType GetOrderType() const {
        return partition_handler_->GetOrderType();
    }
    const std::string GetHandlerTypeName() override {
        return "PartitionHandler";
    }
    std::shared_ptr<PartitionHandler> partition_handler_;
    const PredicateFun* fun_;
};
class TableProjectWrapper : public TableHandler {
 public:
    TableProjectWrapper(std::shared_ptr<TableHandler> table_handler,
                        const ProjectFun* fun)
        : TableHandler(), table_hander_(table_handler), value_(), fun_(fun) {}
    virtual ~TableProjectWrapper() {}

    std::unique_ptr<RowIterator> GetIterator() const {
        auto iter = table_hander_->GetIterator();
        if (!iter) {
            return std::unique_ptr<RowIterator>();
        } else {
            return std::unique_ptr<RowIterator>(
                new IteratorProjectWrapper(std::move(iter), fun_));
        }
    }
    const Types& GetTypes() override { return table_hander_->GetTypes(); }
    const IndexHint& GetIndex() override { return table_hander_->GetIndex(); }
    std::unique_ptr<WindowIterator> GetWindowIterator(
        const std::string& idx_name) override {
        auto iter = table_hander_->GetWindowIterator(idx_name);
        if (!iter) {
            return std::unique_ptr<WindowIterator>();
        } else {
            return std::unique_ptr<WindowIterator>(
                new WindowIteratorProjectWrapper(std::move(iter), fun_));
        }
    }
    const Schema* GetSchema() override { return table_hander_->GetSchema(); }
    const std::string& GetName() override { return table_hander_->GetName(); }
    const std::string& GetDatabase() override {
        return table_hander_->GetDatabase();
    }
    base::ConstIterator<uint64_t, Row>* GetRawIterator() const override {
        auto iter = table_hander_->GetIterator();
        if (!iter) {
            return nullptr;
        } else {
            return new IteratorProjectWrapper(std::move(iter), fun_);
        }
    }
    Row At(uint64_t pos) override {
        value_ = fun_->operator()(table_hander_->At(pos));
        return value_;
    }
    const uint64_t GetCount() override { return table_hander_->GetCount(); }
    virtual std::shared_ptr<PartitionHandler> GetPartition(
        const std::string& index_name);
    virtual const OrderType GetOrderType() const {
        return table_hander_->GetOrderType();
    }
    std::shared_ptr<TableHandler> table_hander_;
    Row value_;
    const ProjectFun* fun_;
};

class TableFilterWrapper : public TableHandler {
 public:
    TableFilterWrapper(std::shared_ptr<TableHandler> table_handler,
                       const PredicateFun* fun)
        : TableHandler(), table_hander_(table_handler), fun_(fun) {}
    virtual ~TableFilterWrapper() {}

    std::unique_ptr<RowIterator> GetIterator() const {
        auto iter = table_hander_->GetIterator();
        if (!iter) {
            return std::unique_ptr<RowIterator>();
        } else {
            return std::unique_ptr<RowIterator>(
                new IteratorFilterWrapper(std::move(iter), fun_));
        }
    }
    const Types& GetTypes() override { return table_hander_->GetTypes(); }
    const IndexHint& GetIndex() override { return table_hander_->GetIndex(); }
    std::unique_ptr<WindowIterator> GetWindowIterator(
        const std::string& idx_name) override {
        auto iter = table_hander_->GetWindowIterator(idx_name);
        if (!iter) {
            return std::unique_ptr<WindowIterator>();
        } else {
            return std::unique_ptr<WindowIterator>(
                new WindowIteratorFilterWrapper(std::move(iter), fun_));
        }
    }
    const Schema* GetSchema() override { return table_hander_->GetSchema(); }
    const std::string& GetName() override { return table_hander_->GetName(); }
    const std::string& GetDatabase() override {
        return table_hander_->GetDatabase();
    }
    base::ConstIterator<uint64_t, Row>* GetRawIterator() const override {
        return new IteratorFilterWrapper(
            static_cast<std::unique_ptr<RowIterator>>(
                table_hander_->GetRawIterator()),
            fun_);
    }
    virtual std::shared_ptr<PartitionHandler> GetPartition(
        const std::string& index_name);
    virtual const OrderType GetOrderType() const {
        return table_hander_->GetOrderType();
    }
    std::shared_ptr<TableHandler> table_hander_;
    Row value_;
    const PredicateFun* fun_;
};

class RowProjectWrapper : public RowHandler {
 public:
    RowProjectWrapper(std::shared_ptr<RowHandler> row_handler,
                      const ProjectFun* fun)
        : RowHandler(), row_handler_(row_handler), value_(), fun_(fun) {}
    virtual ~RowProjectWrapper() {}
    const Row& GetValue() override {
        auto row = row_handler_->GetValue();
        if (row.empty()) {
            value_ = row;
            return value_;
        }
        value_ = fun_->operator()(row_handler_->GetValue());
        return value_;
    }
    const Schema* GetSchema() override { return row_handler_->GetSchema(); }
    const std::string& GetName() override { return row_handler_->GetName(); }
    const std::string& GetDatabase() override {
        return row_handler_->GetDatabase();
    }
    std::shared_ptr<RowHandler> row_handler_;
    Row value_;
    const ProjectFun* fun_;
};
}  // namespace vm
}  // namespace fesql

#endif  // SRC_VM_CATALOG_WRAPPER_H_
