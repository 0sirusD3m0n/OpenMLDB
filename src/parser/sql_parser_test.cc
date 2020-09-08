/*
 * parser/sql_test.h
 * Copyright (C) 2019 chenjing <chenjing@4paradigm.com>
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
#include <utility>
#include "case/sql_case.h"
#include "gtest/gtest.h"
#include "node/node_manager.h"
#include "node/sql_node.h"
#include "parser/parser.h"

namespace fesql {
namespace parser {
using fesql::node::NodeManager;
using fesql::node::NodePointVector;
using fesql::node::SQLNode;
using fesql::sqlcase::SQLCase;
std::vector<SQLCase> InitCases(std::string yaml_path);
void InitCases(std::string yaml_path, std::vector<SQLCase> &cases);  // NOLINT

void InitCases(std::string yaml_path, std::vector<SQLCase> &cases) {  // NOLINT
    if (!SQLCase::CreateSQLCasesFromYaml(fesql::sqlcase::FindFesqlDirPath(),
                                         yaml_path, cases)) {
        FAIL();
    }
}
std::vector<SQLCase> InitCases(std::string yaml_path) {
    std::vector<SQLCase> cases;
    InitCases(yaml_path, cases);
    return cases;
}
class SqlParserTest : public ::testing::TestWithParam<SQLCase> {
 public:
    SqlParserTest() {
        manager_ = new NodeManager();
        parser_ = new FeSQLParser();
    }

    ~SqlParserTest() {
        delete parser_;
        delete manager_;
    }

    struct PrintToStringParamName {
        template <class ParamType>
        std::string operator()(
            const testing::TestParamInfo<ParamType> &info) const {
            auto sql_case = static_cast<SQLCase>(info.param);
            std::string desc = sql_case.id();
            boost::replace_all(desc, "-", "_");
            return desc;
        }
    };

 protected:
    NodeManager *manager_;
    FeSQLParser *parser_;
};

INSTANTIATE_TEST_SUITE_P(
    SqlSimpleQueryParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/simple_query.yaml")),
    SqlParserTest::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(
    SqlRenameQueryParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/rename_query.yaml")),
    SqlParserTest::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(
    SqlWindowQueryParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/window_query.yaml")));

INSTANTIATE_TEST_SUITE_P(
    SqlDistinctParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/distinct_query.yaml")));

INSTANTIATE_TEST_SUITE_P(
    SqlWhereParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/where_query.yaml")));

INSTANTIATE_TEST_SUITE_P(
    SqlGroupParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/group_query.yaml")));

INSTANTIATE_TEST_SUITE_P(
    SqlHavingParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/having_query.yaml")));

INSTANTIATE_TEST_SUITE_P(
    SqlOrderParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/order_query.yaml")));

INSTANTIATE_TEST_SUITE_P(
    SqlJoinParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/join_query.yaml")));

INSTANTIATE_TEST_SUITE_P(
    SqlUnionParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/union_query.yaml")));

INSTANTIATE_TEST_SUITE_P(
    SqlSubQueryParse, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/sub_query.yaml")));

INSTANTIATE_TEST_SUITE_P(UDFParse, SqlParserTest,
                         testing::ValuesIn(InitCases("cases/plan/udf.yaml")));

INSTANTIATE_TEST_SUITE_P(
    SQLCreate, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/create.yaml")));

INSTANTIATE_TEST_SUITE_P(
    SQLInsert, SqlParserTest,
    testing::ValuesIn(InitCases("cases/plan/insert.yaml")));

INSTANTIATE_TEST_SUITE_P(SQLCmdParserTest, SqlParserTest,
                         testing::ValuesIn(InitCases("cases/plan/cmd.yaml")));

TEST_P(SqlParserTest, Parser_Select_Expr_List) {
    auto sql_case = GetParam();
    std::cout << sql_case.sql_str() << std::endl;

    NodePointVector trees;
    base::Status status;
    int ret =
        parser_->parse(sql_case.sql_str().c_str(), trees, manager_, status);

    if (0 != status.code) {
        std::cout << status.msg << std::endl;
    }
    ASSERT_EQ(0, ret);
    std::cout << *(trees.front()) << std::endl;
}

TEST_F(SqlParserTest, ConstExprTest) {
    std::string sqlstr = "select col1, 1, 1l, 1.0f, 1.0, \"abc\" from t1;";
    NodePointVector trees;
    base::Status status;
    int ret = parser_->parse(sqlstr.c_str(), trees, manager_, status);

    if (0 != status.code) {
        std::cout << status.msg << std::endl;
    }
    ASSERT_EQ(0, ret);
    auto node_ptr = trees.front();
    std::cout << *node_ptr << std::endl;
    node::SelectQueryNode *query_node =
        dynamic_cast<node::SelectQueryNode *>(node_ptr);
    auto iter = query_node->GetSelectList()->GetList().cbegin();
    auto iter_end = query_node->GetSelectList()->GetList().cend();
    {
        ASSERT_EQ("col1", dynamic_cast<node::ColumnRefNode *>(
                              dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                              ->GetColumnName());
    }
    iter++;
    {
        ASSERT_EQ(node::kInt32,
                  dynamic_cast<node::ConstNode *>(
                      dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                      ->GetDataType());
        ASSERT_EQ(1, dynamic_cast<node::ConstNode *>(
                         dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                         ->GetInt());
    }
    iter++;
    {
        ASSERT_EQ(node::kInt64,
                  dynamic_cast<node::ConstNode *>(
                      dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                      ->GetDataType());
        ASSERT_EQ(1L, dynamic_cast<node::ConstNode *>(
                          dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                          ->GetLong());
    }
    iter++;
    {
        ASSERT_EQ(node::kFloat,
                  dynamic_cast<node::ConstNode *>(
                      dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                      ->GetDataType());
        ASSERT_EQ(1.0f, dynamic_cast<node::ConstNode *>(
                            dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                            ->GetFloat());
    }
    iter++;
    {
        ASSERT_EQ(node::kDouble,
                  dynamic_cast<node::ConstNode *>(
                      dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                      ->GetDataType());
        ASSERT_EQ(1.0, dynamic_cast<node::ConstNode *>(
                           dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                           ->GetDouble());
    }
    iter++;
    {
        ASSERT_EQ(node::kVarchar,
                  dynamic_cast<node::ConstNode *>(
                      dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                      ->GetDataType());
        const char *str = dynamic_cast<node::ConstNode *>(
                              dynamic_cast<node::ResTarget *>(*iter)->GetVal())
                              ->GetStr();
        ASSERT_EQ("abc", std::string(str));
    }
    iter++;
    ASSERT_TRUE(iter == iter_end);
}
TEST_F(SqlParserTest, Assign_Op_Test) {
    std::string sqlstr =
        "%%fun\n"
        "def test(a:int, b:int):int\n"
        "\tres= 0\n"
        "\tres += a\n"
        "\tres -= b\n"
        "\tres *= a\n"
        "\tres /= b\n"
        "\treturn res\n"
        "end";
    std::cout << sqlstr << std::endl;

    NodePointVector trees;
    base::Status status;
    int ret = parser_->parse(sqlstr.c_str(), trees, manager_, status);

    if (0 != status.code) {
        std::cout << status.msg << std::endl;
    }
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1u, trees.size());
    std::cout << *(trees.front()) << std::endl;
    ASSERT_EQ(fesql::node::kFnDef, trees[0]->GetType());
    node::FnNodeFnDef *fn_def = dynamic_cast<node::FnNodeFnDef *>(trees[0]);
    ASSERT_EQ(6u, fn_def->block_->children.size());

    {
        ASSERT_EQ(fesql::node::kFnAssignStmt,
                  fn_def->block_->children[0]->GetType());
        fesql::node::FnAssignNode *assign =
            dynamic_cast<fesql::node::FnAssignNode *>(
                fn_def->block_->children[0]);
        ASSERT_EQ(fesql::node::kExprPrimary,
                  assign->expression_->GetExprType());
    }
    {
        ASSERT_EQ(fesql::node::kFnAssignStmt,
                  fn_def->block_->children[1]->GetType());
        fesql::node::FnAssignNode *assign =
            dynamic_cast<fesql::node::FnAssignNode *>(
                fn_def->block_->children[1]);
        ASSERT_EQ(fesql::node::kExprBinary, assign->expression_->GetExprType());
        const fesql::node::BinaryExpr *expr =
            dynamic_cast<const fesql::node::BinaryExpr *>(assign->expression_);
        ASSERT_EQ(fesql::node::kFnOpAdd, expr->GetOp());
    }
    {
        ASSERT_EQ(fesql::node::kFnAssignStmt,
                  fn_def->block_->children[2]->GetType());
        fesql::node::FnAssignNode *assign =
            dynamic_cast<fesql::node::FnAssignNode *>(
                fn_def->block_->children[2]);
        ASSERT_EQ(fesql::node::kExprBinary, assign->expression_->GetExprType());
        const fesql::node::BinaryExpr *expr =
            dynamic_cast<const fesql::node::BinaryExpr *>(assign->expression_);
        ASSERT_EQ(fesql::node::kFnOpMinus, expr->GetOp());
    }
    {
        ASSERT_EQ(fesql::node::kFnAssignStmt,
                  fn_def->block_->children[3]->GetType());
        fesql::node::FnAssignNode *assign =
            dynamic_cast<fesql::node::FnAssignNode *>(
                fn_def->block_->children[3]);
        ASSERT_EQ(fesql::node::kExprBinary, assign->expression_->GetExprType());
        const fesql::node::BinaryExpr *expr =
            dynamic_cast<const fesql::node::BinaryExpr *>(assign->expression_);
        ASSERT_EQ(fesql::node::kFnOpMulti, expr->GetOp());
    }
    {
        ASSERT_EQ(fesql::node::kFnAssignStmt,
                  fn_def->block_->children[4]->GetType());
        fesql::node::FnAssignNode *assign =
            dynamic_cast<fesql::node::FnAssignNode *>(
                fn_def->block_->children[4]);
        ASSERT_EQ(fesql::node::kExprBinary, assign->expression_->GetExprType());
        const fesql::node::BinaryExpr *expr =
            dynamic_cast<const fesql::node::BinaryExpr *>(assign->expression_);
        ASSERT_EQ(fesql::node::kFnOpFDiv, expr->GetOp());
    }
}
TEST_F(SqlParserTest, Parser_Insert_ALL_Stmt) {
    const std::string sqlstr =
        "insert into t1 values(1, 2.3, 3.1, \"string\");";
    NodePointVector trees;
    base::Status status;
    int ret = parser_->parse(sqlstr.c_str(), trees, manager_, status);

    if (0 != ret) {
        std::cout << status.msg << std::endl;
    }
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1u, trees.size());
    std::cout << *(trees.front()) << std::endl;
    ASSERT_EQ(node::kInsertStmt, trees[0]->GetType());
    node::InsertStmt *insert_stmt = dynamic_cast<node::InsertStmt *>(trees[0]);

    ASSERT_EQ(true, insert_stmt->is_all_);
    auto insert_value = insert_stmt->values_[0]->children_;

    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[0])->GetInt(), 1);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[1])->GetDouble(),
              2.3);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[2])->GetDouble(),
              3.1);
    ASSERT_STREQ(dynamic_cast<node::ConstNode *>(insert_value[3])->GetStr(),
                 "string");
}

TEST_F(SqlParserTest, Parser_Insert_All_Placeholder) {
    const std::string sqlstr = "insert into t1 values(?, ?, ?, ?);";
    NodePointVector trees;
    base::Status status;
    int ret = parser_->parse(sqlstr.c_str(), trees, manager_, status);

    if (0 != ret) {
        std::cout << status.msg << std::endl;
    }
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1u, trees.size());
    std::cout << *(trees.front()) << std::endl;
    ASSERT_EQ(node::kInsertStmt, trees[0]->GetType());
    node::InsertStmt *insert_stmt = dynamic_cast<node::InsertStmt *>(trees[0]);

    ASSERT_EQ(true, insert_stmt->is_all_);
    auto insert_value = insert_stmt->values_[0]->children_;

    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[0])->GetDataType(),
              fesql::node::kPlaceholder);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[1])->GetDataType(),
              fesql::node::kPlaceholder);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[2])->GetDataType(),
              fesql::node::kPlaceholder);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[3])->GetDataType(),
              fesql::node::kPlaceholder);
}

TEST_F(SqlParserTest, Parser_Insert_Part_Placeholder) {
    const std::string sqlstr = "insert into t1 values(1, ?, 3.1, ?);";
    NodePointVector trees;
    base::Status status;
    int ret = parser_->parse(sqlstr.c_str(), trees, manager_, status);

    if (0 != ret) {
        std::cout << status.msg << std::endl;
    }
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1u, trees.size());
    std::cout << *(trees.front()) << std::endl;
    ASSERT_EQ(node::kInsertStmt, trees[0]->GetType());
    node::InsertStmt *insert_stmt = dynamic_cast<node::InsertStmt *>(trees[0]);

    ASSERT_EQ(true, insert_stmt->is_all_);
    auto insert_value = insert_stmt->values_[0]->children_;

    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[0])->GetInt(), 1);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[1])->GetDataType(),
              fesql::node::kPlaceholder);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[2])->GetDouble(),
              3.1);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[3])->GetDataType(),
              fesql::node::kPlaceholder);
}

TEST_F(SqlParserTest, Parser_Insert_Stmt) {
    const std::string sqlstr =
        "insert into t1(col1, c2, column3, item4) values(1, 2.3, 3.1, "
        "\"string\");";
    NodePointVector trees;
    base::Status status;
    int ret = parser_->parse(sqlstr.c_str(), trees, manager_, status);

    if (0 != ret) {
        std::cout << status.msg << std::endl;
    }
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1u, trees.size());
    std::cout << *(trees.front()) << std::endl;
    ASSERT_EQ(node::kInsertStmt, trees[0]->GetType());
    node::InsertStmt *insert_stmt = dynamic_cast<node::InsertStmt *>(trees[0]);

    auto insert_value = insert_stmt->values_[0]->children_;
    ASSERT_EQ(1u, insert_stmt->values_.size());
    ASSERT_EQ(false, insert_stmt->is_all_);
    ASSERT_EQ(std::vector<std::string>({"col1", "c2", "column3", "item4"}),
              insert_stmt->columns_);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[0])->GetInt(), 1);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[1])->GetDouble(),
              2.3);
    ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[2])->GetDouble(),
              3.1);
    ASSERT_STREQ(dynamic_cast<node::ConstNode *>(insert_value[3])->GetStr(),
                 "string");
}
TEST_F(SqlParserTest, Parser_Insert_Stmt_values) {
    const std::string sqlstr =
        "insert into t1(col1, c2, column3, item4) values(1, 2.3, 3.1, "
        "\"string\"), (2, 3.3, 4.1, \"hello\");";
    std::cout << sqlstr << std::endl;
    NodePointVector trees;
    base::Status status;
    int ret = parser_->parse(sqlstr.c_str(), trees, manager_, status);

    if (0 != ret) {
        std::cout << status.msg << std::endl;
    }
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1u, trees.size());
    std::cout << *(trees.front()) << std::endl;
    ASSERT_EQ(node::kInsertStmt, trees[0]->GetType());
    node::InsertStmt *insert_stmt = dynamic_cast<node::InsertStmt *>(trees[0]);

    ASSERT_EQ(2u, insert_stmt->values_.size());
    ASSERT_EQ(false, insert_stmt->is_all_);
    {
        auto insert_value = insert_stmt->values_[0]->children_;
        ASSERT_EQ(false, insert_stmt->is_all_);
        ASSERT_EQ(std::vector<std::string>({"col1", "c2", "column3", "item4"}),
                  insert_stmt->columns_);
        ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[0])->GetInt(),
                  1);
        ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[1])->GetDouble(),
                  2.3);
        ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[2])->GetDouble(),
                  3.1);
        ASSERT_STREQ(dynamic_cast<node::ConstNode *>(insert_value[3])->GetStr(),
                     "string");
    }
    {
        auto insert_value = insert_stmt->values_[1]->children_;
        ASSERT_EQ(false, insert_stmt->is_all_);
        ASSERT_EQ(std::vector<std::string>({"col1", "c2", "column3", "item4"}),
                  insert_stmt->columns_);
        ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[0])->GetInt(),
                  2);
        ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[1])->GetDouble(),
                  3.3);
        ASSERT_EQ(dynamic_cast<node::ConstNode *>(insert_value[2])->GetDouble(),
                  4.1);
        ASSERT_STREQ(dynamic_cast<node::ConstNode *>(insert_value[3])->GetStr(),
                     "hello");
    }
}

TEST_F(SqlParserTest, Parser_Create_Stmt) {
    const std::string sqlstr =
        "create table IF NOT EXISTS test(\n"
        "    column1 int NOT NULL,\n"
        "    column2 timestamp NOT NULL,\n"
        "    column3 int NOT NULL,\n"
        "    column4 string NOT NULL,\n"
        "    column5 int,\n"
        "    index(key=(column4, column3), version=(column5, 3), "
        "ts=column2, "
        "ttl=60d)\n"
        ");";
    NodePointVector trees;
    base::Status status;
    int ret = parser_->parse(sqlstr.c_str(), trees, manager_, status);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(1u, trees.size());
    std::cout << *(trees.front()) << std::endl;

    ASSERT_EQ(node::kCreateStmt, trees.front()->GetType());
    node::CreateStmt *createStmt = (node::CreateStmt *)trees.front();

    ASSERT_EQ("test", createStmt->GetTableName());
    ASSERT_EQ(true, createStmt->GetOpIfNotExist());

    ASSERT_EQ(6u, createStmt->GetColumnDefList().size());

    ASSERT_EQ("column1",
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[0]))
                  ->GetColumnName());
    ASSERT_EQ(node::kInt32,
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[0]))
                  ->GetColumnType());
    ASSERT_EQ(true, ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[0]))
                        ->GetIsNotNull());

    ASSERT_EQ("column2",
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[1]))
                  ->GetColumnName());
    ASSERT_EQ(node::kTimestamp,
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[1]))
                  ->GetColumnType());
    ASSERT_EQ(true, ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[1]))
                        ->GetIsNotNull());

    ASSERT_EQ("column3",
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[2]))
                  ->GetColumnName());
    ASSERT_EQ(node::kInt32,
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[2]))
                  ->GetColumnType());
    ASSERT_EQ(true, ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[2]))
                        ->GetIsNotNull());

    ASSERT_EQ("column4",
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[3]))
                  ->GetColumnName());
    ASSERT_EQ(node::kVarchar,
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[3]))
                  ->GetColumnType());
    ASSERT_EQ(true, ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[3]))
                        ->GetIsNotNull());

    ASSERT_EQ("column5",
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[4]))
                  ->GetColumnName());
    ASSERT_EQ(node::kInt32,
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[4]))
                  ->GetColumnType());
    ASSERT_EQ(false,
              ((node::ColumnDefNode *)(createStmt->GetColumnDefList()[4]))
                  ->GetIsNotNull());

    ASSERT_EQ(node::kColumnIndex,
              (createStmt->GetColumnDefList()[5])->GetType());
    node::ColumnIndexNode *index_node =
        (node::ColumnIndexNode *)(createStmt->GetColumnDefList()[5]);
    std::vector<std::string> key;
    key.push_back("column4");
    key.push_back("column3");
    ASSERT_EQ(key, index_node->GetKey());

    ASSERT_EQ("column2", index_node->GetTs());
    ASSERT_EQ("column5", index_node->GetVersion());
    ASSERT_EQ(3, index_node->GetVersionCount());
    ASSERT_EQ(60 * 86400000L, index_node->GetTTL());
}

class SqlParserErrorTest : public ::testing::TestWithParam<
                               std::pair<common::StatusCode, std::string>> {
 public:
    SqlParserErrorTest() {
        manager_ = new NodeManager();
        parser_ = new FeSQLParser();
    }

    ~SqlParserErrorTest() {
        delete parser_;
        delete manager_;
    }

 protected:
    NodeManager *manager_;
    FeSQLParser *parser_;
};

// TODO(chenjing): line and column check
TEST_P(SqlParserErrorTest, ParserErrorStatusTest) {
    auto param = GetParam();
    std::cout << param.second << std::endl;

    NodePointVector trees;
    base::Status status;
    int ret = parser_->parse(param.second.c_str(), trees, manager_, status);
    ASSERT_EQ(1, ret);
    ASSERT_EQ(0u, trees.size());
    ASSERT_EQ(param.first, status.code);
    std::cout << status.msg << std::endl;
}

INSTANTIATE_TEST_SUITE_P(SQLErrorParse, SqlParserErrorTest,
                         testing::Values(std::make_pair(
                             common::kSQLError, "SELECT SUM(*) FROM t1;")));

INSTANTIATE_TEST_SUITE_P(
    UDFErrorParse, SqlParserErrorTest,
    testing::Values(
        std::make_pair(common::kSQLError,
                       "%%fun\ndefine test(x:i32,y:i32):i32\n    c=x+y\n    "
                       "return c\nend"),
        std::make_pair(common::kSQLError,
                       "%%fun\ndef 123test(x:i32,y:i32):i32\n    c=x+y\n    "
                       "return c\nend"),
        std::make_pair(common::kSQLError,
                       "%%fun\ndef test(x:i32,y:i32):i32\n    c=x)(y\n    "
                       "return c\nend")));

}  // namespace parser
}  // namespace fesql

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
