/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * udf_library_test.cc
 *
 * Author: chenjing
 * Date: 2019/11/26
 *--------------------------------------------------------------------------
 **/
#include "udf/udf_library.h"
#include <gtest/gtest.h>
#include <unordered_set>
#include <vector>
#include "udf/udf_registry.h"

namespace fesql {
namespace udf {

class UDFLibraryTest : public ::testing::Test {
 public:
    UDFLibrary library;
    node::NodeManager nm;
};

TEST_F(UDFLibraryTest, test_check_is_udaf_by_name) {
    library.RegisterUDAF("sum")
        .templates<int32_t, int32_t, int32_t>()
        .const_init(0)
        .update("add", reinterpret_cast<void*>(0))
        .output("identity", reinterpret_cast<void*>(1));

    ASSERT_TRUE(library.IsUDAF("sum", 1));
    ASSERT_TRUE(!library.IsUDAF("sum", 2));
    ASSERT_TRUE(!library.IsUDAF("sum2", 1));
}

TEST_F(UDFLibraryTest, test_check_list_arg) {
    library.RegisterExternal("f1")
        .args<codec::ListRef<int32_t>, int32_t>(reinterpret_cast<void*>(0))
        .returns<codec::ListRef<int32_t>>();

    library.RegisterExternal("f2")
        .args<int32_t, codec::ListRef<int32_t>>(reinterpret_cast<void*>(0))
        .returns<codec::ListRef<int32_t>>();
    library.RegisterExternal("f2")
        .args<int32_t, int32_t>(reinterpret_cast<void*>(0))
        .returns<int32_t>();

    ASSERT_TRUE(library.RequireListAt("f1", 0));
    ASSERT_TRUE(!library.RequireListAt("f1", 1));
    ASSERT_TRUE(library.IsListReturn("f1"));

    ASSERT_TRUE(!library.RequireListAt("f2", 0));
    ASSERT_TRUE(!library.IsListReturn("f2"));
}

}  // namespace udf
}  // namespace fesql

int main(int argc, char** argv) {
    ::testing::GTEST_FLAG(color) = "yes";
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
