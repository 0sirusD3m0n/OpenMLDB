//
// kv_iterator_test.cc
// Copyright 2017 4paradigm.com

#include "base/kv_iterator.h"
#include <iostream>
#include "base/codec.h"
#include "base/strings.h"
#include "gtest/gtest.h"
#include "proto/tablet.pb.h"

namespace rtidb {
namespace base {

class KvIteratorTest : public ::testing::Test {
 public:
    KvIteratorTest() {}
    ~KvIteratorTest() {}
};

TEST_F(KvIteratorTest, Iterator_NULL) {
    ::rtidb::api::ScanResponse* response = new ::rtidb::api::ScanResponse();
    KvIterator kv_it(response);
    ASSERT_FALSE(kv_it.Valid());
}

TEST_F(KvIteratorTest, Iterator_ONE) {
    ::rtidb::api::ScanResponse* response = new ::rtidb::api::ScanResponse();
    std::string* pairs = response->mutable_pairs();
    pairs->resize(17);
    char* data = reinterpret_cast<char*>(&((*pairs)[0]));
    DataBlock* db1 = new DataBlock(1, "hello", 5);
    Encode(9527, db1, data, 0);
    KvIterator kv_it(response);
    ASSERT_TRUE(kv_it.Valid());
    ASSERT_EQ(9527, kv_it.GetKey());
    ASSERT_EQ("hello", kv_it.GetValue().ToString());
    kv_it.Next();
    ASSERT_FALSE(kv_it.Valid());
}

TEST_F(KvIteratorTest, Iterator) {
    ::rtidb::api::ScanResponse* response = new ::rtidb::api::ScanResponse();

    std::string* pairs = response->mutable_pairs();
    pairs->resize(34);
    char* data = reinterpret_cast<char*>(&((*pairs)[0]));
    DataBlock* db1 = new DataBlock(1, "hello", 5);
    DataBlock* db2 = new DataBlock(1, "hell1", 5);
    Encode(9527, db1, data, 0);
    Encode(9528, db2, data, 17);
    KvIterator kv_it(response);
    ASSERT_TRUE(kv_it.Valid());
    ASSERT_EQ(9527, kv_it.GetKey());
    ASSERT_EQ("hello", kv_it.GetValue().ToString());
    kv_it.Next();
    ASSERT_TRUE(kv_it.Valid());
    ASSERT_EQ(9528, kv_it.GetKey());
    ASSERT_EQ("hell1", kv_it.GetValue().ToString());
    kv_it.Next();
    ASSERT_FALSE(kv_it.Valid());
}

TEST_F(KvIteratorTest, HasPK) {
    ::rtidb::api::TraverseResponse* response =
        new ::rtidb::api::TraverseResponse();

    std::string* pairs = response->mutable_pairs();
    pairs->resize(52);
    char* data = reinterpret_cast<char*>(&((*pairs)[0]));
    DataBlock* db1 = new DataBlock(1, "hello", 5);
    DataBlock* db2 = new DataBlock(1, "hell1", 5);
    EncodeFull("test1", 9527, db1, data, 0);
    EncodeFull("test2", 9528, db2, data, 26);
    KvIterator kv_it(response);
    ASSERT_TRUE(kv_it.Valid());
    ASSERT_STREQ("test1", kv_it.GetPK().c_str());
    ASSERT_EQ(9527, kv_it.GetKey());
    ASSERT_STREQ("hello", kv_it.GetValue().ToString().c_str());
    kv_it.Next();
    ASSERT_TRUE(kv_it.Valid());
    ASSERT_STREQ("test2", kv_it.GetPK().c_str());
    ASSERT_EQ(9528, kv_it.GetKey());
    ASSERT_STREQ("hell1", kv_it.GetValue().ToString().c_str());
    kv_it.Next();
    ASSERT_FALSE(kv_it.Valid());
}

}  // namespace base
}  // namespace rtidb

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
