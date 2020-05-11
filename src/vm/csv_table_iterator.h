/*
 * csv_table_iterator.h
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

#ifndef SRC_VM_CSV_TABLE_ITERATOR_H_
#define SRC_VM_CSV_TABLE_ITERATOR_H_

#include <map>
#include <memory>
#include <string>
#include "arrow/table.h"
#include "codec/fe_row_codec.h"
#include "vm/catalog.h"
#include "vm/csv_catalog.h"

namespace fesql {
namespace vm {

uint32_t GetRowSize(const Schema& schema, uint64_t chunk_offset,
                    uint64_t array_offset,
                    const std::shared_ptr<arrow::Table>& table,
                    codec::RowBuilder* rb);

bool GetRow(const Schema& schema, const std::shared_ptr<arrow::Table>& table,
            uint64_t chunk_offset, uint64_t array_offset,
            codec::RowBuilder* rb);

class CSVSegmentIterator : public RowIterator {
 public:
    CSVSegmentIterator(const std::shared_ptr<arrow::Table>& table,
                       const IndexDatas* index_datas,
                       const std::string& index_name, const std::string& pk,
                       const Schema& schema);
    ~CSVSegmentIterator();

    void Seek(uint64_t ts);

    void SeekToFirst();

    const uint64_t GetKey();

    const Row& GetValue();

    void Next();

    bool Valid();

 private:
    const std::shared_ptr<arrow::Table> table_;
    const IndexDatas* index_datas_;
    const std::string index_name_;
    const std::string pk_;
    const Schema& schema_;
    int8_t* buf_;
    codec::RowBuilder rb_;
    uint32_t buf_size_;
    std::map<uint64_t, RowLocation>::const_reverse_iterator it_;
    std::map<uint64_t, RowLocation>::const_reverse_iterator rend_;
    Row value_;
};

class CSVTableIterator : public RowIterator {
 public:
    CSVTableIterator(const std::shared_ptr<arrow::Table>& table,
                     const Schema& schema);
    ~CSVTableIterator();

    void Seek(uint64_t ts);

    void SeekToFirst();

    const uint64_t GetKey();

    const Row& GetValue();

    void Next();

    bool Valid();

 private:
    void BuildRow();

 private:
    const std::shared_ptr<arrow::Table> table_;
    const Schema schema_;
    int64_t chunk_offset_;
    int64_t array_offset_;
    int8_t* buf_;
    codec::RowBuilder rb_;
    uint32_t buf_size_;
    Row value_;
};

}  // namespace vm
}  // namespace fesql

#endif  // SRC_VM_CSV_TABLE_ITERATOR_H_
