/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * window.h
 *
 * Author: chenjing
 * Date: 2019/11/25
 *--------------------------------------------------------------------------
 **/

#ifndef SRC_CODEC_LIST_ITERATOR_CODEC_H_
#define SRC_CODEC_LIST_ITERATOR_CODEC_H_
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "base/fe_slice.h"
#include "base/iterator.h"
#include "codec/row.h"
#include "codec/type_codec.h"
#include "glog/logging.h"
namespace fesql {
namespace codec {

using fesql::base::ConstIterator;
template <class V>
class ArrayListIterator;

template <class V>
class ColumnImpl;

template <class V>
class ColumnIterator;

typedef ConstIterator<uint64_t, Row> RowIterator;
class WindowIterator {
 public:
    WindowIterator() {}
    virtual ~WindowIterator() {}
    virtual void Seek(const std::string &key) = 0;
    virtual void SeekToFirst() = 0;
    virtual void Next() = 0;
    virtual bool Valid() = 0;
    virtual std::unique_ptr<RowIterator> GetValue() = 0;
    virtual RowIterator *GetValue(int8_t *addr) = 0;
    virtual const Row GetKey() = 0;
};

template <class V>
class ListV {
 public:
    ListV() {}
    virtual ~ListV() {}
    // TODO(chenjing): at 数组越界处理
    virtual std::unique_ptr<ConstIterator<uint64_t, V>> GetIterator() const = 0;
    virtual ConstIterator<uint64_t, V> *GetIterator(int8_t *addr) const = 0;
    virtual const uint64_t GetCount() {
        auto iter = GetIterator();
        uint64_t cnt = 0;
        while (iter->Valid()) {
            iter->Next();
            cnt++;
        }
        return cnt;
    }
    virtual V At(uint64_t pos) {
        auto iter = GetIterator();
        while (pos-- > 0 && iter->Valid()) {
            iter->Next();
        }
        return iter->Valid() ? iter->GetValue() : V();
    }
};

template <class V, class R>
class WrapListImpl : public ListV<V> {
 public:
    WrapListImpl() : ListV<V>() {}
    ~WrapListImpl() {}
    virtual const V GetFieldUnsafe(const R &row) const = 0;
    virtual void GetField(const R &row, V *, int8_t *) const = 0;
    virtual const bool IsNull(const R &row) const = 0;
};

template <class V>
class ColumnImpl : public WrapListImpl<V, Row> {
 public:
    ColumnImpl(ListV<Row> *impl, int32_t row_idx, uint32_t col_idx,
               uint32_t offset)
        : WrapListImpl<V, Row>(),
          root_(impl),
          row_idx_(row_idx),
          col_idx_(col_idx),
          offset_(offset) {}

    ~ColumnImpl() {}

    const V GetFieldUnsafe(const Row &row) const override {
        V value;
        const int8_t *ptr = row.buf(row_idx_) + offset_;
        value = *((const V *)ptr);
        return value;
    }

    void GetField(const Row &row, V *res, int8_t *is_null) const override {
        const int8_t *buf = row.buf(row_idx_);
        if (buf == nullptr || v1::IsNullAt(buf, col_idx_)) {
            *is_null = true;
        } else {
            *is_null = false;
            const int8_t *ptr = buf + offset_;
            *res = *((const V *)ptr);
        }
    }

    const bool IsNull(const Row &row) const override {
        const int8_t *buf = row.buf(row_idx_);
        return buf == nullptr || v1::IsNullAt(buf, col_idx_);
    }

    // TODO(xxx): iterator of nullable V
    std::unique_ptr<ConstIterator<uint64_t, V>> GetIterator() const override {
        auto iter = std::unique_ptr<ConstIterator<uint64_t, V>>(
            new ColumnIterator<V>(root_, this));
        return std::move(iter);
    }
    ConstIterator<uint64_t, V> *GetIterator(int8_t *addr) const override {
        if (nullptr == addr) {
            return new ColumnIterator<V>(root_, this);
        } else {
            return new (addr) ColumnIterator<V>(root_, this);
        }
    }
    const uint64_t GetCount() override { return root_->GetCount(); }
    V At(uint64_t pos) override { return GetFieldUnsafe(root_->At(pos)); }

 protected:
    ListV<Row> *root_;
    const uint32_t row_idx_;
    const uint32_t col_idx_;
    const uint32_t offset_;
};

class StringColumnImpl : public ColumnImpl<StringRef> {
 public:
    StringColumnImpl(ListV<Row> *impl, int32_t row_idx, uint32_t col_idx,
                     int32_t str_field_offset, int32_t next_str_field_offset,
                     int32_t str_start_offset)
        : ColumnImpl<StringRef>(impl, row_idx, col_idx, 0u),
          str_field_offset_(str_field_offset),
          next_str_field_offset_(next_str_field_offset),
          str_start_offset_(str_start_offset) {}

    ~StringColumnImpl() {}
    const StringRef GetFieldUnsafe(const Row &row) const override {
        int32_t addr_space = v1::GetAddrSpace(row.size(row_idx_));
        StringRef value;
        int8_t *buffer;
        v1::GetStrFieldUnsafe(row.buf(row_idx_), str_field_offset_,
                              next_str_field_offset_, str_start_offset_,
                              addr_space, &buffer, &(value.size_));
        value.data_ = reinterpret_cast<char *>(buffer);
        return value;
    }

    void GetField(const Row &row, StringRef *res,
                  int8_t *is_null) const override {
        const int8_t *buf = row.buf(row_idx_);
        if (buf == nullptr || v1::IsNullAt(buf, col_idx_)) {
            *is_null = true;
        } else {
            int32_t addr_space = v1::GetAddrSpace(row.size(row_idx_));
            StringRef value;
            int8_t *buffer;
            v1::GetStrFieldUnsafe(buf, str_field_offset_,
                                  next_str_field_offset_, str_start_offset_,
                                  addr_space, &buffer, &(value.size_));
            value.data_ = reinterpret_cast<char *>(buffer);
            *res = value;
        }
    }

 private:
    uint32_t str_field_offset_;
    uint32_t next_str_field_offset_;
    uint32_t str_start_offset_;
};

template <class V>
class ArrayListV : public ListV<V> {
 public:
    ArrayListV() : start_(0), end_(0), buffer_(nullptr) {}
    explicit ArrayListV(std::vector<V> *buffer)
        : start_(0), end_(buffer->size()), buffer_(buffer) {}

    ArrayListV(std::vector<V> *buffer, uint32_t start, uint32_t end)
        : start_(start), end_(end), buffer_(buffer) {}

    ~ArrayListV() {}
    // TODO(chenjing): at 数组越界处理

    std::unique_ptr<ConstIterator<uint64_t, V>> GetIterator() const override {
        return std::unique_ptr<ArrayListIterator<V>>(
            new ArrayListIterator<V>(buffer_, start_, end_));
    }
    ConstIterator<uint64_t, V> *GetIterator(int8_t *addr) const override {
        if (nullptr == addr) {
            return new ArrayListIterator<V>(buffer_, start_, end_);
        } else {
            return new (addr) ArrayListIterator<V>(buffer_, start_, end_);
        }
    }
    virtual const uint64_t GetCount() { return end_ - start_; }
    virtual V At(uint64_t pos) const { return buffer_->at(start_ + pos); }

 protected:
    uint64_t start_;
    uint64_t end_;
    std::vector<V> *buffer_;
};

template <class V>
class ArrayListIterator : public ConstIterator<uint64_t, V> {
 public:
    explicit ArrayListIterator(const std::vector<V> *buffer,
                               const uint64_t start, const uint64_t end)
        : buffer_(buffer),
          iter_start_(buffer->cbegin() + start),
          iter_end_(buffer->cbegin() + end),
          iter_(iter_start_),
          key_(0) {}

    explicit ArrayListIterator(const ArrayListIterator<V> &impl)
        : buffer_(impl.buffer_),
          iter_start_(impl.iter_start_),
          iter_end_(impl.iter_end_),
          iter_(impl.iter_start_),
          key_(0) {}
    explicit ArrayListIterator(const ArrayListIterator<V> &impl, uint64_t start,
                               uint64_t end)
        : buffer_(impl.buffer_),
          iter_start_(impl.iter_start_ + start),
          iter_end_(impl.iter_start_ + end),
          iter_(iter_start_),
          key_(0) {}

    ~ArrayListIterator() {}
    void Seek(const uint64_t &key) override {
        iter_ =
            (iter_start_ + key) >= iter_end_ ? iter_end_ : iter_start_ + key;
        key_ = iter_end_ - iter_start_;
    }

    bool Valid() const override { return iter_end_ != iter_; }

    void Next() override { ++iter_; }

    const V &GetValue() override { return *iter_; }

    const uint64_t &GetKey() const override { return key_; }

    void SeekToFirst() { iter_ = iter_start_; }

    bool IsSeekable() const override { return true; }
    ArrayListIterator<V> *range(int start, int end) {
        if (start > end || end < iter_start_ || start > iter_end_) {
            return new ArrayListIterator(buffer_, iter_start_, iter_start_);
        }
        start = start < iter_start_ ? iter_start_ : start;
        end = end > iter_end_ ? iter_end_ : end;
        return new ArrayListIterator(buffer_, start, end);
    }

 protected:
    const std::vector<V> *buffer_;
    const typename std::vector<V>::const_iterator iter_start_;
    const typename std::vector<V>::const_iterator iter_end_;
    typename std::vector<V>::const_iterator iter_;
    uint64_t key_;
};

class BoolArrayListIterator : public ConstIterator<uint64_t, bool> {
 public:
    explicit BoolArrayListIterator(const std::vector<int> *buffer,
                                   const uint64_t start, const uint64_t end)
        : buffer_(buffer),
          iter_start_(buffer->cbegin() + start),
          iter_end_(buffer->cbegin() + end),
          iter_(iter_start_),
          key_(0) {
        if (Valid()) {
            tmp_ = *iter_;
        }
    }

    explicit BoolArrayListIterator(const BoolArrayListIterator &impl)
        : buffer_(impl.buffer_),
          iter_start_(impl.iter_start_),
          iter_end_(impl.iter_end_),
          iter_(impl.iter_start_),
          key_(0) {
        if (Valid()) {
            tmp_ = *iter_;
        }
    }
    explicit BoolArrayListIterator(const BoolArrayListIterator &impl,
                                   uint64_t start, uint64_t end)
        : buffer_(impl.buffer_),
          iter_start_(impl.iter_start_ + start),
          iter_end_(impl.iter_start_ + end),
          iter_(iter_start_),
          key_(0) {
        if (Valid()) {
            tmp_ = *iter_;
        }
    }

    ~BoolArrayListIterator() {}
    void Seek(const uint64_t &key) override {
        iter_ =
            (iter_start_ + key) >= iter_end_ ? iter_end_ : iter_start_ + key;
        key_ = iter_end_ - iter_start_;
    }

    bool Valid() const override { return iter_end_ != iter_; }

    void Next() override { tmp_ = *(++iter_); }

    const bool &GetValue() override { return tmp_; }

    const uint64_t &GetKey() const override { return key_; }

    void SeekToFirst() { iter_ = iter_start_; }

    bool IsSeekable() const override { return true; }

 protected:
    const std::vector<int> *buffer_;
    const typename std::vector<int>::const_iterator iter_start_;
    const typename std::vector<int>::const_iterator iter_end_;
    typename std::vector<int>::const_iterator iter_;
    uint64_t key_;
    bool tmp_;
};

class BoolArrayListV : public ListV<bool> {
 public:
    BoolArrayListV() : start_(0), end_(0), buffer_(nullptr) {}
    explicit BoolArrayListV(std::vector<int> *buffer)
        : start_(0), end_(buffer->size()), buffer_(buffer) {}

    BoolArrayListV(std::vector<int> *buffer, uint32_t start, uint32_t end)
        : start_(start), end_(end), buffer_(buffer) {}

    ~BoolArrayListV() {}

    std::unique_ptr<ConstIterator<uint64_t, bool>> GetIterator()
        const override {
        return std::unique_ptr<BoolArrayListIterator>(
            new BoolArrayListIterator(buffer_, start_, end_));
    }
    ConstIterator<uint64_t, bool> *GetIterator(int8_t *addr) const override {
        if (nullptr == addr) {
            return new BoolArrayListIterator(buffer_, start_, end_);
        } else {
            return new (addr) BoolArrayListIterator(buffer_, start_, end_);
        }
    }
    virtual const uint64_t GetCount() { return end_ - start_; }
    virtual bool At(uint64_t pos) const { return buffer_->at(start_ + pos); }

 protected:
    uint64_t start_;
    uint64_t end_;
    std::vector<int> *buffer_;
};

template <class V>
class InnerRowsIterator : public ConstIterator<uint64_t, V> {
 public:
    InnerRowsIterator(ListV<V> *list, uint64_t start, uint64_t end)
        : ConstIterator<uint64_t, V>(),
          root_(list->GetIterator()),
          pos_(0),
          start_(start),
          end_(end) {
        if (nullptr != root_) {
            SeekToFirst();
        }
    }
    ~InnerRowsIterator() {}
    virtual bool Valid() const {
        return root_->Valid() && pos_ <= end_ && pos_ >= start_;
    }
    virtual void Next() {
        pos_++;
        return root_->Next();
    }
    virtual const uint64_t &GetKey() const { return root_->GetKey(); }
    virtual const V &GetValue() { return root_->GetValue(); }
    virtual void Seek(const uint64_t &k) { root_->Seek(k); }
    virtual void SeekToFirst() {
        root_->SeekToFirst();
        pos_ = 0;
        while (root_->Valid() && pos_ < start_) {
            root_->Next();
            pos_++;
        }
    }
    bool IsSeekable() const { return root_->IsSeekable(); }
    std::unique_ptr<ConstIterator<uint64_t, V>> root_;
    uint64_t pos_;
    const uint64_t start_;
    const uint64_t end_;
};

template <class V>
class InnerRangeIterator : public ConstIterator<uint64_t, V> {
 public:
    InnerRangeIterator(ListV<V> *list, uint64_t start, uint64_t end)
        : ConstIterator<uint64_t, V>(),
          root_(list->GetIterator()),
          start_key_(0),
          start_(start),
          end_(end) {
        if (nullptr != root_) {
            root_->SeekToFirst();
            start_key_ = root_->Valid() ? root_->GetKey() : 0;
        }
    }
    ~InnerRangeIterator() {}
    virtual bool Valid() const {
        return root_->Valid() && root_->GetKey() <= start_ &&
               root_->GetKey() >= end_;
    }
    virtual void Next() { return root_->Next(); }
    virtual const uint64_t &GetKey() const { return root_->GetKey(); }
    virtual const V &GetValue() { return root_->GetValue(); }
    virtual void Seek(const uint64_t &k) { root_->Seek(k); }
    virtual void SeekToFirst() {
        root_->SeekToFirst();
        root_->Seek(start_);
    }
    virtual bool IsSeekable() const { return root_->IsSeekable(); }
    std::unique_ptr<ConstIterator<uint64_t, V>> root_;
    uint64_t start_key_;
    const uint64_t start_;
    const uint64_t end_;
};

template <class V>
class InnerRangeList : public ListV<V> {
 public:
    InnerRangeList(ListV<Row> *root, uint64_t start, uint64_t end)
        : ListV<V>(), root_(root), start_(start), end_(end) {}
    virtual ~InnerRangeList() {}
    // TODO(chenjing): at 数组越界处理
    virtual std::unique_ptr<ConstIterator<uint64_t, V>> GetIterator() const {
        return std::unique_ptr<InnerRangeIterator<V>>(
            new InnerRangeIterator<V>(root_, start_, end_));
    }
    virtual ConstIterator<uint64_t, V> *GetIterator(int8_t *addr) const {
        if (nullptr == addr) {
            return new InnerRangeIterator<V>(root_, start_, end_);
        } else {
            return new (addr) InnerRangeIterator<V>(root_, start_, end_);
        }
    }

    ListV<Row> *root_;
    uint64_t start_;
    uint64_t end_;
};

template <class V>
class InnerRowsList : public ListV<V> {
 public:
    InnerRowsList(ListV<Row> *root, uint64_t start, uint64_t end)
        : ListV<V>(), root_(root), start_(start), end_(end) {}
    virtual ~InnerRowsList() {}
    // TODO(chenjing): at 数组越界处理
    virtual std::unique_ptr<ConstIterator<uint64_t, V>> GetIterator() const {
        return std::unique_ptr<InnerRowsIterator<V>>(
            new InnerRowsIterator<V>(root_, start_, end_));
    }
    virtual ConstIterator<uint64_t, V> *GetIterator(int8_t *addr) const {
        if (nullptr == addr) {
            return new InnerRowsIterator<V>(root_, start_, end_);
        } else {
            return new (addr) InnerRowsIterator<V>(root_, start_, end_);
        }
    }

    ListV<Row> *root_;
    uint64_t start_;
    uint64_t end_;
};
template <class V>
class ColumnIterator : public ConstIterator<uint64_t, V> {
 public:
    ColumnIterator(ListV<Row> *list, const ColumnImpl<V> *column_impl)
        : ConstIterator<uint64_t, V>(), column_impl_(column_impl) {
        row_iter_ = list->GetIterator();
        if (!row_iter_) {
            row_iter_->SeekToFirst();
        }
    }
    ~ColumnIterator() {}
    void Seek(const uint64_t &key) override { row_iter_->Seek(key); }
    void SeekToFirst() override { row_iter_->SeekToFirst(); }
    bool Valid() const override { return row_iter_->Valid(); }
    void Next() override { row_iter_->Next(); }
    const V &GetValue() override {
        value_ = column_impl_->GetFieldUnsafe(row_iter_->GetValue());
        return value_;
    }
    const uint64_t &GetKey() const override { return row_iter_->GetKey(); }
    bool IsSeekable() const override { return row_iter_->IsSeekable(); }

 private:
    const ColumnImpl<V> *column_impl_;
    std::unique_ptr<RowIterator> row_iter_;
    V value_;
};

}  // namespace codec
}  // namespace fesql

#endif  // SRC_CODEC_LIST_ITERATOR_CODEC_H_
