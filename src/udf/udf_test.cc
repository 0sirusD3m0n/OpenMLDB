/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * udf_test.cc
 *
 * Author: chenjing
 * Date: 2019/11/26
 *--------------------------------------------------------------------------
 **/
#include "udf/udf.h"
#include <dlfcn.h>
#include <gtest/gtest.h>
#include <stdint.h>
#include <algorithm>
#include <vector>

#include "codec/window.h"

namespace fesql {
namespace udf {
using codec::ListV;
using codec::WindowIteratorImpl;

class UDFTest : public ::testing::Test {
 public:
    UDFTest() { InitData(); }
    ~UDFTest() {}

    void InitData() {
        rows.clear();
        // prepare row buf
        {
            int8_t* ptr = reinterpret_cast<int8_t*>(malloc(28));
            *(reinterpret_cast<int32_t*>(ptr + 2)) = 1;
            *(reinterpret_cast<int16_t*>(ptr + 2 + 4)) = 2;
            *(reinterpret_cast<float*>(ptr + 2 + 4 + 2)) = 3.1f;
            *(reinterpret_cast<double*>(ptr + 2 + 4 + 2 + 4)) = 4.1;
            *(reinterpret_cast<int64_t*>(ptr + 2 + 4 + 2 + 4 + 8)) = 5;
            rows.push_back(codec::Row{.buf = ptr});
        }

        {
            int8_t* ptr = static_cast<int8_t*>(malloc(28));
            *(reinterpret_cast<int32_t*>(ptr + 2)) = 11;
            *(reinterpret_cast<int16_t*>(ptr + 2 + 4)) = 22;
            *(reinterpret_cast<float*>(ptr + 2 + 4 + 2)) = 33.1f;
            *(reinterpret_cast<double*>(ptr + 2 + 4 + 2 + 4)) = 44.1;
            *(reinterpret_cast<int64_t*>(ptr + 2 + 4 + 2 + 4 + 8)) = 55;
            rows.push_back(codec::Row{.buf = ptr});
        }

        {
            int8_t* ptr = static_cast<int8_t*>(malloc(28));
            *(reinterpret_cast<int32_t*>(ptr + 2)) = 111;
            *(reinterpret_cast<int16_t*>(ptr + 2 + 4)) = 222;
            *(reinterpret_cast<float*>(ptr + 2 + 4 + 2)) = 333.1f;
            *(reinterpret_cast<double*>(ptr + 2 + 4 + 2 + 4)) = 444.1;
            *(reinterpret_cast<int64_t*>(ptr + 2 + 4 + 2 + 4 + 8)) = 555;
            rows.push_back(codec::Row{.buf = ptr});
        }
    }

 protected:
    std::vector<codec::Row> rows;
};

TEST_F(UDFTest, UDF_sum_test) {
    ListV<codec::Row> window(&rows);
    const uint32_t size = sizeof(::fesql::codec::ColumnImpl<int16_t>);
    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(
            0, ::fesql::codec::v1::GetCol(reinterpret_cast<int8_t*>(&window),
                                            2, fesql::type::kInt32, buf));
        ASSERT_EQ(1 + 11 + 111, fesql::udf::v1::sum_list<int32_t>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(
            0, ::fesql::codec::v1::GetCol(reinterpret_cast<int8_t*>(&window),
                                            2 + 4, fesql::type::kInt16, buf));
        ASSERT_EQ(2 + 22 + 222, fesql::udf::v1::sum_list<int16_t>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(0, ::fesql::codec::v1::GetCol(
                         reinterpret_cast<int8_t*>(&window), 2 + 4 + 2,
                         fesql::type::kFloat, buf));
        ASSERT_EQ(3.1f + 33.1f + 333.1f, fesql::udf::v1::sum_list<float>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(0, ::fesql::codec::v1::GetCol(
                         reinterpret_cast<int8_t*>(&window), 2 + 4 + 2 + 4,
                         fesql::type::kDouble, buf));
        ASSERT_EQ(4.1 + 44.1 + 444.1, fesql::udf::v1::sum_list<double>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(0, ::fesql::codec::v1::GetCol(
                         reinterpret_cast<int8_t*>(&window), 2 + 4 + 2 + 4 + 8,
                         fesql::type::kInt64, buf));
        ASSERT_EQ(5L + 55L + 555L, fesql::udf::v1::sum_list<int64_t>(col));
    }
}

TEST_F(UDFTest, UDF_max_test) {
    ListV<codec::Row> impl(&rows);
    const uint32_t size = sizeof(::fesql::codec::ColumnImpl<int16_t>);
    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(
            0, ::fesql::codec::v1::GetCol(reinterpret_cast<int8_t*>(&impl), 2,
                                            fesql::type::kInt32, buf));
        ASSERT_EQ(111, fesql::udf::v1::max_list<int32_t>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(
            0, ::fesql::codec::v1::GetCol(reinterpret_cast<int8_t*>(&impl),
                                            2 + 4, fesql::type::kInt16, buf));
        ASSERT_EQ(222, fesql::udf::v1::max_list<int16_t>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(0, ::fesql::codec::v1::GetCol(
                         reinterpret_cast<int8_t*>(&impl), 2 + 4 + 2,
                         fesql::type::kFloat, buf));
        ASSERT_EQ(333.1f, fesql::udf::v1::max_list<float>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(0, ::fesql::codec::v1::GetCol(
                         reinterpret_cast<int8_t*>(&impl), 2 + 4 + 2 + 4,
                         fesql::type::kDouble, buf));
        ASSERT_EQ(444.1, fesql::udf::v1::max_list<double>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(0, ::fesql::codec::v1::GetCol(
                         reinterpret_cast<int8_t*>(&impl), 2 + 4 + 2 + 4 + 8,
                         fesql::type::kInt64, buf));
        ASSERT_EQ(555L, fesql::udf::v1::max_list<int64_t>(col));
    }
}

TEST_F(UDFTest, UDF_min_test) {
    ListV<fesql::codec::Row> impl(&rows);
    const uint32_t size = sizeof(::fesql::codec::ColumnImpl<int16_t>);
    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(
            0, ::fesql::codec::v1::GetCol(reinterpret_cast<int8_t*>(&impl), 2,
                                            fesql::type::kInt32, buf));
        ASSERT_EQ(1, fesql::udf::v1::min_list<int32_t>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(
            0, ::fesql::codec::v1::GetCol(reinterpret_cast<int8_t*>(&impl),
                                            2 + 4, fesql::type::kInt16, buf));
        ASSERT_EQ(2, fesql::udf::v1::min_list<int16_t>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(0, ::fesql::codec::v1::GetCol(
                         reinterpret_cast<int8_t*>(&impl), 2 + 4 + 2,
                         fesql::type::kFloat, buf));
        ASSERT_EQ(3.1f, fesql::udf::v1::min_list<float>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(0, ::fesql::codec::v1::GetCol(
                         reinterpret_cast<int8_t*>(&impl), 2 + 4 + 2 + 4,
                         fesql::type::kDouble, buf));
        ASSERT_EQ(4.1, fesql::udf::v1::min_list<double>(col));
    }

    {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        int8_t* col = reinterpret_cast<int8_t*>(&list_ref);

        ASSERT_EQ(0, ::fesql::codec::v1::GetCol(
                         reinterpret_cast<int8_t*>(&impl), 2 + 4 + 2 + 4 + 8,
                         fesql::type::kInt64, buf));
        ASSERT_EQ(5L, fesql::udf::v1::min_list<int64_t>(col));
    }
}
TEST_F(UDFTest, GetColTest) {
    ListV<fesql::codec::Row> impl(&rows);
    const uint32_t size = sizeof(::fesql::codec::ColumnImpl<int16_t>);
    for (int i = 0; i < 10; ++i) {
        int8_t* buf = reinterpret_cast<int8_t*>(alloca(size));
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        ASSERT_EQ(
            0, ::fesql::codec::v1::GetCol(reinterpret_cast<int8_t*>(&impl), 2,
                                            fesql::type::kInt32, buf));
        ::fesql::codec::ColumnImpl<int16_t>* col =
            reinterpret_cast< ::fesql::codec::ColumnImpl<int16_t>*>(
                list_ref.list);
        ::fesql::codec::IteratorImpl<int16_t> col_iterator(*col);
        ASSERT_TRUE(col_iterator.Valid());
        ASSERT_EQ(1, col_iterator.Next());
        ASSERT_TRUE(col_iterator.Valid());
        ASSERT_EQ(11, col_iterator.Next());
        ASSERT_TRUE(col_iterator.Valid());
        ASSERT_EQ(111, col_iterator.Next());
        ASSERT_FALSE(col_iterator.Valid());
    }
}

TEST_F(UDFTest, GetColHeapTest) {
    ListV<fesql::codec::Row> impl(&rows);
    const uint32_t size = sizeof(::fesql::codec::ColumnImpl<int16_t>);
    for (int i = 0; i < 10000; ++i) {
        int8_t buf[size];  // NOLINT
        ::fesql::codec::ListRef list_ref;
        list_ref.list = buf;
        ASSERT_EQ(
            0, ::fesql::codec::v1::GetCol(reinterpret_cast<int8_t*>(&impl), 2,
                                            fesql::type::kInt32, buf));
        ::fesql::codec::ColumnImpl<int16_t>* impl =
            reinterpret_cast<::fesql::codec::ColumnImpl<int16_t>*>(
                list_ref.list);
        ASSERT_EQ(3u, impl->Count());
    }
}

}  // namespace udf
}  // namespace fesql
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
