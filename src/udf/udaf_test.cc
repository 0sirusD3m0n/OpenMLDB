/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * udaf_test.cc
 *
 * Author: chenjing
 * Date: 2020/6/17
 *--------------------------------------------------------------------------
 **/
#include "udf/udf_test.h"

namespace fesql {
namespace udf {

using udf::Nullable;
using codec::Date;
using codec::Timestamp;
using codec::StringRef;
using codec::ListRef;

class UDAFTest : public ::testing::Test {
 public:
    UDAFTest() {}
    ~UDAFTest() {}
};

template <class Ret, class... Args>
void CheckUDF(const std::string &name, Ret expect, Args... args) {
    auto function = udf::UDFFunctionBuilder(name)
                        .args<Args...>()
                        .template returns<Ret>()
                        .library(udf::DefaultUDFLibrary::get())
                        .build();
    ASSERT_TRUE(function.valid());
    auto result = function(args...);
    udf::EqualValChecker<Ret>::check(expect, result);
}

template <class T, class... Args>
void CheckUDFFail(const std::string &name, T expect, Args... args) {
    auto function = udf::UDFFunctionBuilder(name)
                        .args<Args...>()
                        .template returns<T>()
                        .build();
    ASSERT_FALSE(function.valid());
}

TEST_F(UDAFTest, count_where_test) {
    CheckUDF<int64_t, ListRef<int32_t>, ListRef<bool>>(
        "count_where", 2, MakeList<int32_t>({4, 5, 6}),
        MakeBoolList({true, false, true}));

    CheckUDF<int64_t, ListRef<StringRef>, ListRef<bool>>(
        "count_where", 2,
        MakeList({StringRef("1"), StringRef("2"),
                  StringRef("3")}),
        MakeBoolList({true, false, true}));

    CheckUDF<int64_t, ListRef<Nullable<int32_t>>, ListRef<Nullable<bool>>>(
        "count_where", 2, MakeList<Nullable<int32_t>>({4, 5, 6, nullptr}),
        MakeList<Nullable<bool>>({true, false, nullptr, true}));

    CheckUDF<int64_t, ListRef<int32_t>, ListRef<bool>>(
        "count_where", 0, MakeList<int32_t>({}), MakeBoolList({}));
}

TEST_F(UDAFTest, avg_where_test) {
    CheckUDF<double, ListRef<int32_t>, ListRef<bool>>(
        "avg_where", 5.0, MakeList<int32_t>({4, 5, 6}),
        MakeBoolList({true, false, true}));

    // TODO(someone): Timestamp arithmetic
    // CheckUDF<double, ListRef<Timestamp>, ListRef<bool>>(
    //    "avg_where", 5.0, MakeList<Timestamp>({Timestamp(4), Timestamp(5), Timestamp(6)}),
    //    MakeBoolList({true, false, true}));

    CheckUDF<double, ListRef<Nullable<int32_t>>, ListRef<Nullable<bool>>>(
        "avg_where", 5.5, MakeList<Nullable<int32_t>>({4, 5, 6, 7}),
        MakeList<Nullable<bool>>({true, false, nullptr, true}));

    CheckUDF<double, ListRef<int32_t>, ListRef<bool>>(
        "avg_where", 0.0 / 0, MakeList<int32_t>({}), MakeBoolList({}));
}

TEST_F(UDAFTest, avg_test) {
    CheckUDF<double, ListRef<int16_t>>("avg", 2.5,
                                              MakeList<int16_t>({1, 2, 3, 4}));
    CheckUDF<double, ListRef<int32_t>>("avg", 2.5,
                                              MakeList<int32_t>({1, 2, 3, 4}));
    CheckUDF<double, ListRef<int64_t>>("avg", 2.5,
                                              MakeList<int64_t>({1, 2, 3, 4}));
    CheckUDF<double, ListRef<float>>("avg", 2.5,
                                            MakeList<float>({1, 2, 3, 4}));
    CheckUDF<double, ListRef<double>>("avg", 2.5,
                                             MakeList<double>({1, 2, 3, 4}));
    // empty list
    CheckUDF<double, ListRef<double>>("avg", 0.0 / 0,
                                             MakeList<double>({}));
}

TEST_F(UDAFTest, topk_test) {
    CheckUDF<StringRef, ListRef<int32_t>, ListRef<int32_t>>(
        "top", StringRef("6,6,5,4"),
        MakeList<int32_t>({1, 6, 3, 4, 5, 2, 6}),
        MakeList<int32_t>({4, 4, 4, 4, 4, 4, 4}));

    CheckUDF<StringRef, ListRef<float>, ListRef<int32_t>>(
        "top", StringRef("6.600000,6.600000,5.500000,4.400000"),
        MakeList<float>({1.1, 6.6, 3.3, 4.4, 5.5, 2.2, 6.6}),
        MakeList<int32_t>({4, 4, 4, 4, 4, 4, 4}));

    CheckUDF<StringRef, ListRef<Date>, ListRef<int32_t>>(
        "top", StringRef("1900-01-06,1900-01-06,1900-01-05,1900-01-04"),
        MakeList<Date>({Date(1), Date(6), Date(3), Date(4), Date(5),
                           Date(2), Date(6)}),
        MakeList<int32_t>({4, 4, 4, 4, 4, 4, 4}));

    CheckUDF<StringRef, ListRef<Timestamp>, ListRef<int32_t>>(
        "top", StringRef("1970-01-01 08:00:06,1970-01-01 08:00:06,1970-01-01 08:00:05,1970-01-01 08:00:04"),
        MakeList<Timestamp>({Timestamp(1000), Timestamp(6000),
            Timestamp(3000), Timestamp(4000), Timestamp(5000),
            Timestamp(2000), Timestamp(6000)}),
        MakeList<int32_t>({4, 4, 4, 4, 4, 4, 4}));

    CheckUDF<StringRef, ListRef<StringRef>, ListRef<int32_t>>(
        "top", StringRef("6,6,5,4"),
        MakeList<StringRef>({StringRef("1"), StringRef("6"), 
                           StringRef("3"), StringRef("4"),
                           StringRef("5"), StringRef("2"),
                           StringRef("6")}),
        MakeList<int32_t>({4, 4, 4, 4, 4, 4, 4}));

    // null and not enough inputs
    CheckUDF<StringRef, ListRef<Nullable<int32_t>>, ListRef<int32_t>>(
        "top", StringRef("5,3,1"),
        MakeList<Nullable<int32_t>>({1, nullptr, 3, nullptr, 5}),
        MakeList<int32_t>({4, 4, 4, 4, 4}));

    // empty
    CheckUDF<StringRef, ListRef<int32_t>, ListRef<int32_t>>(
        "top", StringRef(""),
        MakeList<int32_t>({}),
        MakeList<int32_t>({}));
}

TEST_F(UDAFTest, avg_cate_test) {
    CheckUDF<StringRef, ListRef<int32_t>, ListRef<int32_t>>(
        "avg_cate", StringRef("1:2.000000,2:3.000000"),
        MakeList<int32_t>({1, 2, 1, 2}),
        MakeList<int32_t>({1, 2, 3, 4}));

    CheckUDF<StringRef, ListRef<Date>, ListRef<int32_t>>(
        "avg_cate", StringRef("1900-01-01:2.000000,1900-01-02:3.000000"),
        MakeList<Date>({Date(1), Date(2), Date(1), Date(2)}),
        MakeList<int32_t>({1, 2, 3, 4}));

    CheckUDF<StringRef, ListRef<StringRef>, ListRef<int32_t>>(
        "avg_cate", StringRef("x:2.000000,y:3.000000"),
        MakeList<StringRef>({StringRef("x"), StringRef("y"), StringRef("x"), StringRef("y")}),
        MakeList<int32_t>({1, 2, 3, 4}));

    // null key and values
    CheckUDF<StringRef, ListRef<Nullable<StringRef>>, ListRef<Nullable<int32_t>>>(
        "avg_cate", StringRef("x:2.000000,y:3.000000"),
        MakeList<Nullable<StringRef>>({StringRef("x"), StringRef("y"), StringRef("x"), StringRef("y"), nullptr, StringRef("x")}),
        MakeList<Nullable<int32_t>>({1, 2, 3, 4, 5, nullptr}));

    // empty
    CheckUDF<StringRef, ListRef<int32_t>, ListRef<int32_t>>(
        "avg_cate", StringRef(""), MakeList<int32_t>({}), MakeList<int32_t>({}));
}

TEST_F(UDAFTest, avg_cate_where_test) {
    CheckUDF<StringRef, ListRef<int32_t>, ListRef<int32_t>, ListRef<bool>>(
        "avg_cate_where", StringRef("1:2.000000,2:3.000000"),
        MakeList<int32_t>({1, 2, 1, 2, 1, 2}),
        MakeList<int32_t>({1, 2, 3, 4, 5, 6}),
        MakeBoolList({true, true, true, true, false, false}));

    CheckUDF<StringRef, ListRef<Date>, ListRef<int32_t>, ListRef<bool>>(
        "avg_cate_where", StringRef("1900-01-01:2.000000,1900-01-02:3.000000"),
        MakeList<Date>({Date(1), Date(2), Date(1), Date(2), Date(1), Date(2)}),
        MakeList<int32_t>({1, 2, 3, 4, 5, 6}),
        MakeBoolList({true, true, true, true, false, false}));

    CheckUDF<StringRef, ListRef<StringRef>, ListRef<int32_t>, ListRef<bool>>(
        "avg_cate_where", StringRef("x:2.000000,y:3.000000"),
        MakeList<StringRef>({StringRef("x"), StringRef("y"), StringRef("x"), StringRef("y"), StringRef("x"), StringRef("y")}),
        MakeList<int32_t>({1, 2, 3, 4, 5, 6}),
        MakeBoolList({true, true, true, true, false, false}));

    // null key and values
    CheckUDF<StringRef, ListRef<Nullable<StringRef>>, ListRef<Nullable<int32_t>>, ListRef<Nullable<bool>>>(
        "avg_cate_where", StringRef("x:3.000000,y:4.000000"),
        MakeList<Nullable<StringRef>>({StringRef("x"), StringRef("y"), StringRef("x"), StringRef("y"), nullptr, StringRef("x")}),
        MakeList<Nullable<int32_t>>({1, 2, 3, 4, 5, nullptr}),
        MakeList<Nullable<bool>>({false, nullptr, true, true, true, true}));

    // empty
    CheckUDF<StringRef, ListRef<int32_t>, ListRef<int32_t>, ListRef<bool>>(
        "avg_cate_where", StringRef(""), MakeList<int32_t>({}), MakeList<int32_t>({}), MakeBoolList({}));
}

TEST_F(UDAFTest, top_n_avg_cate_where_test) {
    CheckUDF<StringRef, ListRef<int32_t>, ListRef<int32_t>, ListRef<bool>, ListRef<int32_t>>(
        "top_n_avg_cate_where", StringRef("2:4.500000,1:3.500000"),
        MakeList<int32_t>({0, 1, 2, 0, 1, 2, 0, 1, 2}),
        MakeList<int32_t>({1, 2, 3, 4, 5, 6, 7, 8, 9}),
        MakeBoolList({true, true, true, true, true, true, false, false, false}),
        MakeList<int32_t>({2, 2, 2, 2, 2, 2, 2, 2, 2}));

    CheckUDF<StringRef, ListRef<Date>, ListRef<int32_t>, ListRef<bool>, ListRef<int32_t>>(
        "top_n_avg_cate_where", StringRef("1900-01-02:4.500000,1900-01-01:3.500000"),
        MakeList<Date>({Date(0), Date(1), Date(2), Date(0), Date(1), Date(2), Date(0), Date(1), Date(2)}),
        MakeList<int32_t>({1, 2, 3, 4, 5, 6, 7, 8, 9}),
        MakeBoolList({true, true, true, true, true, true, false, false, false}),
        MakeList<int32_t>({2, 2, 2, 2, 2, 2, 2, 2, 2}));

    CheckUDF<StringRef, ListRef<StringRef>, ListRef<int32_t>, ListRef<bool>, ListRef<int32_t>>(
        "top_n_avg_cate_where", StringRef("z:4.500000,y:3.500000"),
        MakeList<StringRef>({StringRef("x"), StringRef("y"), StringRef("z"),
                             StringRef("x"), StringRef("y"), StringRef("z"),
                             StringRef("x"), StringRef("y"), StringRef("z")}),
        MakeList<int32_t>({1, 2, 3, 4, 5, 6, 7, 8, 9}),
        MakeBoolList({true, true, true, true, true, true, false, false, false}),
        MakeList<int32_t>({2, 2, 2, 2, 2, 2, 2, 2, 2}));

    // null key and values
    CheckUDF<StringRef, ListRef<Nullable<StringRef>>, ListRef<Nullable<int32_t>>, ListRef<Nullable<bool>>, ListRef<int32_t>>(
        "top_n_avg_cate_where", StringRef("z:3.000000,y:5.000000"),
        MakeList<Nullable<StringRef>>({StringRef("x"), StringRef("y"), StringRef("z"),
                                       StringRef("x"), StringRef("y"), nullptr, StringRef("x")}),
        MakeList<Nullable<int32_t>>({1, 2, 3, 4, 5, 6, nullptr}),
        MakeList<Nullable<bool>>({false, nullptr, true, true, true, true, true}),
        MakeList<int32_t>({2, 2, 2, 2, 2, 2, 2}));

    // empty
    CheckUDF<StringRef, ListRef<int32_t>, ListRef<int32_t>, ListRef<bool>, ListRef<int32_t>>(
        "top_n_avg_cate_where", StringRef(""), MakeList<int32_t>({}), MakeList<int32_t>({}), MakeBoolList({}), MakeList<int32_t>({}));
}

}  // namespace udf
}  // namespace fesql

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::llvm::InitializeNativeTarget();
    ::llvm::InitializeNativeTargetAsmPrinter();
    return RUN_ALL_TESTS();
}
