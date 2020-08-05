//
// server_name_test.cc
// Copyright 2017 4paradigm.com

#include "base/server_name.h"
#include "gtest/gtest.h"

DECLARE_string(data_dir);

namespace rtidb {
namespace base {

class ServerNameTest : public ::testing::Test {
 public:
    ServerNameTest() {}
    ~ServerNameTest() {}
};

TEST_F(ServerNameTest, GetName) {
    std::string restore_dir = "/tmp/data";
    std::string server_name;
    ASSERT_TRUE(GetNameFromTxt(restore_dir, &server_name));
    std::string server_name_2;
    ASSERT_TRUE(GetNameFromTxt(restore_dir, &server_name_2));
    ASSERT_EQ(server_name, server_name_2);
}

}  // namespace base
}  // namespace rtidb

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::rtidb::base::SetLogLevel(INFO);
    ::rtidb::base::RemoveDirRecursive("/tmp/data");
    int ret = RUN_ALL_TESTS();
    return ret;
}
