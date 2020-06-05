%define api.pure full
%output  "sql_parser.gen.cc"
%defines "sql_parser.gen.h"
%define parse.error verbose
%locations
%lex-param   { yyscan_t scanner }
%parse-param { yyscan_t scanner }
%parse-param { ::fesql::node::NodePointVector &trees}
%parse-param { ::fesql::node::NodeManager *node_manager}
%parse-param { ::fesql::base::Status &status}

%{
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <utility>
#include "node/sql_node.h"
#include "node/node_manager.h"
#include "base/fe_status.h"
#include "parser/sql_parser.gen.h"

extern int yylex(YYSTYPE* yylvalp, 
                 YYLTYPE* yyllocp, 
                 yyscan_t scanner);
void emit(const char *s, ...);
void yyerror(YYLTYPE* yyllocp, yyscan_t unused, ::fesql::node::NodePointVector &trees,
	::fesql::node::NodeManager *node_manager, ::fesql::base::Status &status, const char* msg) {
	status.code=::fesql::common::kSQLError;
	std::ostringstream s;
        s << "line: "<< yyllocp->last_line << ", column: "
       	<< yyllocp->first_column << ": " <<
       	msg;
	status.msg=(s.str());
}
%}

%code requires {
#include "node/sql_node.h"
#include "base/fe_status.h"
#include <sstream>
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif
}

%union {
	int intval;
	int64_t longval;
	float floatval;
	double doubleval;
	char* strval;
	int subtok;
	bool flag;
	::fesql::node::SQLNode* node;
	::fesql::node::QueryNode* query_node;
	::fesql::node::FnNode* fnnode;
	::fesql::node::ExprNode* expr;
	::fesql::node::TableRefNode* table_ref;
	::fesql::node::JoinType join_type;
	::fesql::node::DataType type;
	::fesql::node::TypeNode* typenode;
	::fesql::node::FnNodeList* fnlist;
	::fesql::node::ExprListNode* exprlist;
	::fesql::node::SQLNodeList* list;
}

/* names and literal values */
%token <strval> STRING
%token <intval> INTNUM
%token <longval> LONGNUM
%token <intval> DAYNUM
%token <intval> HOURNUM
%token <intval> MINUTENUM
%token <intval> SECONDNUM
%token <intval> BOOL
%token <floatval> FLOATNUM
%token <doubleval> DOUBLENUM

/* user @abc names */

%token <strval> USERVAR
%token <strval> SQL_IDENTIFIER
%token <strval> FUN_IDENTIFIER

/* operators and precedence levels */

%left ','
%right ASSIGN ADD_ASSIGN SUB_ASSIGN MULTI_ASSIGN MINUS_ASSIGN FDIV_ASSIGN
%nonassoc IN IS LIKE REGEXP
// ? : expr

%left OR
%left ANDOP
// | & ^
%left XOR

%left BETWEEN
%left EQUALS NOT_EQUALS
%left '<' '>' LESS_EQUALS GREATER_EQUALS
%left '|'
%left '&'
%left <subtok> SHIFT /* << >> */
%left '+' '-'
%left '*' '/' '%' MOD
%left '^'
%left '(' ')' '[' ']' LPARENT RPARENT
%left NOT '!'
%nonassoc UMINUS
%nonassoc IDEXPR
%nonassoc CALLEXPR



%token <intval> I32
%token <strval> NEWLINES
%token <intval> INDENT
%token <strval> DEF

%token ADD
%token ALL
%token ALTER
%token ANALYZE
%token AND
%token ANY
%token AS
%token ASC
%token AUTO_INCREMENT
%token BEFORE
%token BETWEEN
%token BIGINT
%token BINARY
%token BIT
%token BLOB
%token BOTH
%token BY
%token CALL
%token CASCADE
%token CASE
%token CHANGE
%token CHAR
%token CHECK
%token COLLATE
%token COLUMN
%token COMMENT
%token CONDITION
%token CONSTRAINT
%token CONTINUE
%token CONVERT
%token CREATE
%token CROSS
%token CURRENT
%token CURRENT_DATE
%token CURRENT_TIME
%token CURRENT_TIMESTAMP
%token CURRENT_USER
%token CURSOR
%token DATABASE
%token DATABASES
%token DATE
%token DATETIME
%token DAY_HOUR
%token DAY_MICROSECOND
%token DAY_MINUTE
%token DAY_SECOND
%token DECIMAL
%token DECLARE
%token DEFAULT
%token DELAYED
%token DELETE
%token DESC
%token DESCRIBE
%token DETERMINISTIC
%token DISTINCT
%token DISTINCTROW
%token DIV
%token DOUBLE
%token DROP
%token DUAL
%token EACH
%token ELSE
%token ELSEIF
%token ENCLOSED
%token END
%token FUNDEFEND
%token ENUM
%token ESCAPED
%token EXISTS
%token EXIT
%token EXPLAIN
%token FETCH
%token FLOAT
%token FOR
%token FORCE
%token FOREIGN
%token FOLLOWING
%token FROM
%token FULLTEXT
%token FULL
%token GRANT
%token GROUP
%token HAVING
%token HIGH_PRIORITY
%token HOUR_MICROSECOND
%token HOUR_MINUTE
%token HOUR_SECOND
%token IF
%token IGNORE
%token IN
%token INDEX
%token INFILE
%token INNER
%token INOUT
%token INSENSITIVE
%token INSERT
%token INSTANCE_NOT_IN_WINDOW
%token INT
%token INTEGER
%token INTERVAL
%token INTO
%token ITERATE
%token JOIN
%token KEY
%token KEYS
%token KILL
%token LAST
%token LEADING
%token LEAVE
%token LEFT
%token LIKE
%token LIMIT
%token LINES
%token LIST
%token LOAD
%token LOCALTIME
%token LOCALTIMESTAMP
%token LOCK
%token LOGICAL
%token LONG
%token LONGBLOB
%token LONGTEXT
%token LOOP
%token LOW_PRIORITY
%token MATCH
%token MAP
%token MEDIUMBLOB
%token MEDIUMINT
%token MEDIUMTEXT
%token MINUTE_MICROSECOND
%token MINUTE_SECOND
%token MOD
%token MODIFIES
%token NATURAL
%token NOT
%token NO_WRITE_TO_BINLOG
%token NULLX
%token NUMBER
%token ON
%token ONDUPLICATE
%token OPTIMIZE
%token OPTION
%token OPTIONALLY
%token OR
%token ORDER
%token OUT
%token OUTER
%token OUTFILE
%token OVER
%token PARTITION
%token PRECISION
%token PRIMARY
%token PROCEDURE
%token PURGE
%token QUICK
%token RANGE
%token READ
%token READS
%token REAL
%token REFERENCES
%token REGEXP
%token RELEASE
%token REIDNENTIFIER
%token REPEAT
%token REPLACE
%token REQUIRE
%token RESTRICT
%token RETURN
%token REVOKE
%token PRECEDING
%token RIGHT
%token ROLLUP
%token ROW
%token ROWS
%token SCHEMA
%token SCHEMAS
%token SECOND_MICROSECOND
%token SELECT
%token SENSITIVE
%token SEPARATOR
%token SET
%token SHOW
%token SMALLINT
%token SOME
%token SOIDNENTIFIER
%token SPATIAL
%token SPECIFIC
%token SQL
%token SQLEXCEPTION
%token SQLSTATE
%token SQLWARNING
%token SQL_BIG_RESULT
%token SQL_CALC_FOUND_ROWS
%token SQL_SMALL_RESULT
%token USE_SSL
%token STRINGTYPE
%token STARTING
%token STRAIGHT_JOIN
%token TABLE
%token TABLES
%token TEMPORARY
%token TEXT
%token TERMINATED
%token THEN
%token TIME
%token TIMESTAMP
%token TINYBLOB
%token TINYINT
%token TINYTEXT
%token TO
%token TRAILING
%token TRIGGER
%token TS
%token TTL
%token TTL_TYPE

%token UNDO
%token UNION
%token UNIQUE
%token UNLOCK
%token UNSIGNED
%token UPDATE
%token USAGE
%token USE
%token USING
%token UNBOUNDED
%token UTC_DATE
%token UTC_TIME
%token UTC_TIMESTAMP
%token VALUES
%token VARBINARY
%token VARCHAR
%token VARYING
%token VERSION
%token WINDOW
%token WHEN
%token WHERE
%token WHILE
%token WITH
%token WRITE
%token XOR
%token YEAR
%token YEAR_MONTH
%token ZEROFILL

 /* functions with special syntax */
%token FSUBSTRING
%token FTRIM
%token FDATE_ADD FDATE_SUB
%token FCOUNT

 /* udf */
%type <type> types
%type <join_type> join_type
%type <typenode> complex_types
%type <fnnode> grammar line_list
			   fun_def_block fn_header_indent_op  func_stmt
               fn_header return_stmt assign_stmt para
               if_stmt elif_stmt else_stmt
               for_in_stmt
%type<fnlist> plist stmt_block func_stmts

%type <expr> 	var primary_time  expr_const
				sql_call_expr column_ref frame_expr join_condition
				fun_expr sql_expr
				sort_clause opt_sort_clause
 /* select stmt */
%type <node>  stmt
              projection
              opt_frame_clause frame_bound frame_extent
              window_definition window_specification over_clause
              limit_clause
%type<query_node> sql_stmt union_stmt select_stmt query_clause
%type <table_ref> table_reference join_clause last_join_clause table_factor query_reference
 /* insert table */
%type<node> insert_stmt
%type<exprlist> insert_expr_list column_ref_list opt_partition_clause
				group_expr sql_id_list
				sql_expr_list fun_expr_list
				insert_values insert_value

%type<expr> insert_expr where_expr having_expr

 /* create table */
%type <node>  create_stmt column_desc column_index_item column_index_key
%type <node>  cmd_stmt
%type <flag>  op_not_null op_if_not_exist opt_distinct_clause opt_instance_not_in_window
%type <list>  column_desc_list column_index_item_list

%type <list> opt_target_list
            select_projection_list
            table_references

            window_clause window_definition_list
            opt_union_clause

%type <strval> relation_name relation_factor
               column_name
               function_name
               opt_existing_window_name
               database_name table_name group_name file_path
               join_outer

%type <intval> opt_window_exclusion_clause


%start grammar

%%
grammar :
        line_list
        ;

/**** function def ****/


line_list:
		fun_def_block {
            trees.push_back($1);
        }
        | sql_stmt {
            trees.push_back($1);
        }
        | line_list NEWLINES fun_def_block
        {
            trees.push_back($3);
        }
        | line_list NEWLINES sql_stmt
        {
        	trees.push_back($3);
        }
        | line_list NEWLINES {$$ = $1;}
        | NEWLINES line_list {$$ = $2;}
        ;

fun_def_block : fn_header_indent_op NEWLINES stmt_block {
            $$ = node_manager->MakeFnDefNode($1, $3);
        }
        ;


fn_header_indent_op:
        fn_header {
            $$ = $1;
        }
        |INDENT fn_header {$$=$2; $$->indent=$1;}
        ;


stmt_block:
        func_stmts NEWLINES FUNDEFEND {
            emit("enter stmt block");
            $$ = $1;
        }
        ;
func_stmts:
        func_stmt {
			$$ = node_manager->MakeFnListNode();
            $$->AddChild($1);
        }
        |func_stmts NEWLINES func_stmt {
            emit("enter func stmts");
            $$ = $1;
            $$->AddChild($3);
        }
        ;
func_stmt:
         INDENT return_stmt
         {
            emit("INDENT enter return stmt");
            $2->indent = $1;
            $$ = $2;
         }
         |INDENT assign_stmt
         {
            emit("INDENT enter assign stmt");
            $2->indent = $1;
            $$ = $2;
         }
         |INDENT if_stmt
         {
         	emit("INDENT enter if stmt");
            $2->indent = $1;
            $$ = $2;
         }
         |INDENT elif_stmt
         {
         	emit("INDENT enter if stmt");
            $2->indent = $1;
            $$ = $2;
         }
         |INDENT else_stmt
         {
         	emit("INDENT enter else stmt");
            $2->indent = $1;
            $$ = $2;
         }
         |INDENT for_in_stmt
         {
         	emit("INDENT enter for in stmt");
         	$2->indent = $1;
         	$$ = $2;
         }
         ;

fn_header:
   		DEF FUN_IDENTIFIER'(' plist ')' ':' types {
			$$ = node_manager->MakeFnHeaderNode($2, $4, node_manager->MakeTypeNode($7));
   		};
   		|DEF FUN_IDENTIFIER'(' plist ')' ':' complex_types {
			$$ = node_manager->MakeFnHeaderNode($2, $4, $7);
   		};

assign_stmt:
		FUN_IDENTIFIER ASSIGN fun_expr {
            $$ = node_manager->MakeAssignNode($1, $3);
        }
        |FUN_IDENTIFIER ADD_ASSIGN fun_expr {
        	$$ = node_manager->MakeAssignNode($1, $3, ::fesql::node::kFnOpAdd);
        }
        |FUN_IDENTIFIER MINUS_ASSIGN fun_expr {
        	$$ = node_manager->MakeAssignNode($1, $3, ::fesql::node::kFnOpMinus);
        }
        |FUN_IDENTIFIER MULTI_ASSIGN fun_expr {
        	$$ = node_manager->MakeAssignNode($1, $3, ::fesql::node::kFnOpMulti);
        }
        |FUN_IDENTIFIER FDIV_ASSIGN fun_expr {
        	$$ = node_manager->MakeAssignNode($1, $3, ::fesql::node::kFnOpFDiv);
        }
        ;

return_stmt:
		RETURN fun_expr {
            $$ = node_manager->MakeReturnStmtNode($2);
        };

if_stmt:
		IF fun_expr {
			$$ = node_manager->MakeIfStmtNode($2);
		};

elif_stmt:
		ELSEIF fun_expr {
			$$ = node_manager->MakeElifStmtNode($2);
		};

else_stmt:
		ELSE {
			$$ = node_manager->MakeElseStmtNode();
		}
		;

for_in_stmt:
		FOR FUN_IDENTIFIER IN fun_expr {
			$$ = node_manager->MakeForInStmtNode($2, $4);
		}
		;

types:  I32
        {
            $$ = ::fesql::node::kInt32;
        }
        |SMALLINT
        {
        	$$ = ::fesql::node::kInt16;
        }
        |INTEGER
        {
            $$ = ::fesql::node::kInt32;
        }
        |BIGINT
        {
            $$ = ::fesql::node::kInt64;
        }
        |STRINGTYPE
        {
            $$ = ::fesql::node::kVarchar;
        }
        |FLOAT
        {
            $$ = ::fesql::node::kFloat;
        }
        |DOUBLE
        {
            $$ = ::fesql::node::kDouble;
        }
        |TIMESTAMP
        {
            $$ = ::fesql::node::kTimestamp;
        }
        |DATE
        {
        	$$ = ::fesql::node::kDate;
        }
        ;

complex_types:
		LIST '<' types '>'
		{
			$$ = node_manager->MakeTypeNode(::fesql::node::kList, $3);
		}
		;
plist:
     para {
        $$ = node_manager->MakeFnListNode();
        $$->AddChild($1);
     } | plist ',' para  {
        $$ = $1;
        $$->AddChild($3);
     };

para:
	FUN_IDENTIFIER ':' types {
        $$ = node_manager->MakeFnParaNode($1, node_manager->MakeTypeNode($3));
    }
    |FUN_IDENTIFIER ':' complex_types {
    	$$ = node_manager->MakeFnParaNode($1, $3);
    }
    ;


primary_time:
    DAYNUM {
        $$ = node_manager->MakeConstNode($1, fesql::node::kDay);
    }
    |HOURNUM {
        $$ = node_manager->MakeConstNode($1, fesql::node::kHour);
    }
    |MINUTENUM {
        $$ = node_manager->MakeConstNode($1, fesql::node::kMinute);
    }
    |SECONDNUM{
        $$ = node_manager->MakeConstNode($1, fesql::node::kSecond);
    }
    |LONGNUM {
        $$ = node_manager->MakeConstNode($1);
    }
    |INTNUM {
        $$ = node_manager->MakeConstNode($1);
    };
var: FUN_IDENTIFIER {
        $$ = node_manager->MakeExprIdNode($1);
     };

sql_stmt: stmt ';' {
                    trees.push_back($1);
                    YYACCEPT;
                    }
                    ;

   /* statements: select statement */

stmt:   query_clause
        {
            $$ = $1;
        }
        |create_stmt
        {
            $$ = $1;
        }
        |insert_stmt
        {
        	$$ = $1;
        }
        |cmd_stmt
        {
        	$$ = $1;
        }
        |EXPLAIN LOGICAL query_clause
        {
        	$$ = node_manager->MakeExplainNode($3, fesql::node::kExplainLogical);
        }
        |EXPLAIN query_clause
        {
        	$$ = node_manager->MakeExplainNode($2, fesql::node::kExplainPhysical);
        }
        ;


query_clause:
		select_stmt {
			$$ = $1;
		}
		| union_stmt {
			$$ = $1;
		}
		;

select_stmt:
			SELECT opt_distinct_clause opt_target_list FROM table_references
			where_expr group_expr having_expr opt_sort_clause window_clause limit_clause
            {
                $$ = node_manager->MakeSelectQueryNode($2, $3, $5, $6, $7, $8, $9, $10, $11);
            }
            | '(' select_stmt ')'
            {
            	$$ = $2;
            }
    		;


create_stmt:    CREATE TABLE op_if_not_exist relation_name '(' column_desc_list ')'
                {
                    $$ = node_manager->MakeCreateTableNode($3, $4, $6);
                }
                |CREATE INDEX column_name ON table_name '(' column_index_item_list ')'
                {
                    $$ = node_manager->MakeCreateIndexNode($3, $5,
                    dynamic_cast<fesql::node::ColumnIndexNode *>(node_manager->MakeColumnIndexNode($7)));
                    free($3);
                    free($5);
                }
                ;


insert_stmt:	INSERT INTO table_name VALUES insert_values
				{
					$$ = node_manager->MakeInsertTableNode($3, NULL, $5);
				}
				|INSERT INTO table_name '(' column_ref_list ')' VALUES insert_values
				{

					$$ = node_manager->MakeInsertTableNode($3, $5, $8);
				}
				;
insert_values:	insert_value
				{
					$$ = node_manager->MakeExprList($1);
				}
				| insert_values ',' insert_value
				{
					$$ = $1;
					$$->PushBack($3);
				}
				;

insert_value:	'(' insert_expr_list ')'
				{
					$$ = $2;
				}
				;
column_ref_list:	column_ref
					{
						$$ = node_manager->MakeExprList($1);
					}
					|column_ref_list',' column_ref
					{
						$$ = $1;
						$$->AddChild($3);
					}
					;

insert_expr_list:	insert_expr
					{
						$$ = node_manager->MakeExprList($1);
					}
					| insert_expr_list ',' insert_expr
					{
						$$ = $1;
						$$->PushBack($3);
					}
					;
insert_expr:	expr_const
				;
cmd_stmt:
			CREATE GROUP group_name
			{
				$$ = node_manager->MakeCmdNode(::fesql::node::kCmdCreateGroup, $3);
				free($3);
			}
			|CREATE DATABASE database_name
			{
				$$ = node_manager->MakeCmdNode(::fesql::node::kCmdCreateDatabase, $3);
				free($3);
			}
			|CREATE TABLE file_path
			{

				$$ = node_manager->MakeCmdNode(::fesql::node::kCmdSource, $3);
				free($3);
			}
			|SHOW DATABASES
			{
				$$ = node_manager->MakeCmdNode(::fesql::node::kCmdShowDatabases);
			}
			|SHOW TABLES
			{
				$$ = node_manager->MakeCmdNode(::fesql::node::kCmdShowTables);
			}
			|DESC table_name
			{
				$$ = node_manager->MakeCmdNode(::fesql::node::kCmdDescTable, $2);
				free($2);
			}
			|USE database_name
			{
				$$ = node_manager->MakeCmdNode(::fesql::node::kCmdUseDatabase, $2);
				free($2);
			}
            |DROP TABLE table_name
            {
                $$ = node_manager->MakeCmdNode(::fesql::node::kCmdDropTable, $3);
                free($3);
            }
            |DROP INDEX column_name ON table_name
            {
                $$ = node_manager->MakeCmdNode(::fesql::node::kCmdDropIndex, $3, $5);
                free($3);
                free($5);
            }
            |EXIT {
                $$ = node_manager->MakeCmdNode(::fesql::node::kCmdExit);
            }
			;

file_path:
			STRING
			{}
			;

column_desc_list:   column_desc
                    {
                        $$ = node_manager->MakeNodeList($1);
                    }
                    | column_desc_list ',' column_desc
                    {
                    	$$ = $1;
                        $$->PushBack($3);
                    }
                    ;

column_desc:    column_name types op_not_null
                {
                    $$ = node_manager->MakeColumnDescNode($1, $2, $3);
                }
                | INDEX '(' column_index_item_list ')'
                {
                    $$ = node_manager->MakeColumnIndexNode($3);
                }
                ;

column_index_item_list:    column_index_item
                    {
                        $$ = node_manager->MakeNodeList($1);
                    }
                    |  column_index_item_list ',' column_index_item
                    {
                        $$ = $1;
                        $$->PushBack($3);
                    }
                    ;

column_index_item:  KEY EQUALS column_name
                    {
                        $$ = node_manager->MakeIndexKeyNode($3);
                    }
                    | KEY EQUALS '(' column_index_key ')'
                    {
                        $$ = $4;
                    }
                    | TS EQUALS column_name
                    {
                        $$ = node_manager->MakeIndexTsNode($3);
                    }
                    | TTL EQUALS primary_time
                    {
                        $$ = node_manager->MakeIndexTTLNode($3);
                    }
                    | TTL_TYPE EQUALS SQL_IDENTIFIER
                    {
                        $$ = node_manager->MakeIndexTTLTypeNode($3);
                    }
                    | VERSION EQUALS column_name
                    {
                        $$ = node_manager->MakeIndexVersionNode($3);
                    }
                    | VERSION EQUALS '(' column_name ',' INTNUM ')'
                    {
                        $$ = node_manager->MakeIndexVersionNode($4, $6);
                    }
                    | VERSION EQUALS '(' column_name ',' LONGNUM ')'
                    {
                        $$ = node_manager->MakeIndexVersionNode($4, $6);
                    }
                    ;

column_index_key:   column_name
            {
                $$ = node_manager->MakeIndexKeyNode($1);
            }
            | column_index_key ',' column_name
            {
                $$ = $1;
                ((::fesql::node::IndexKeyNode*)$$)->AddKey($3);
            }
            ;


op_if_not_exist:    IF NOT EXISTS
                    {
                        $$ = true;
                    }
                    |/*EMPTY*/
                    {
                        $$ = false;
                    }
                    ;

op_not_null:    NOT NULLX
                {
                    $$ = true;
                }
                |/*EMPTY*/
                {
                    $$ = false;
                }
                ;

opt_distinct_clause:
        DISTINCT
        {
        	$$ = true;
        }
        | /*EMPTY*/
        {
        	$$ = false;
        }
    ;


/*****************************************************************************
 *
 *	target list for SELECT
 *
 *****************************************************************************/

opt_target_list: select_projection_list						{ $$ = $1; }
			| /* EMPTY */							{ $$ = NULL; }
		;
select_projection_list: projection {
                            $$ = node_manager->MakeNodeList($1);
                       }
    | select_projection_list ',' projection
    {
        $$ = $1;
        $$->PushBack($3);
    }
    ;

projection:	sql_expr
			{
				$$ = node_manager->MakeResTargetNode($1, "");
			}
			|sql_expr SQL_IDENTIFIER
			{
        		$$ = node_manager->MakeResTargetNode($1, $2);
				free($2);
    		}
    		| sql_expr AS SQL_IDENTIFIER
    		{
    			$$ = node_manager->MakeResTargetNode($1, $3);
    			free($3);
    		}
    		| '*'
        	{
            	::fesql::node::ExprNode* pNode = node_manager->MakeAllNode("");
            	$$ = node_manager->MakeResTargetNode(pNode, "");
        	}
    		;

over_clause: OVER window_specification
				{ $$ = $2; }
			| OVER SQL_IDENTIFIER
				{
				    $$ = node_manager->MakeWindowDefNode($2);
				    free($2);
				}
			| /*EMPTY*/
				{ $$ = NULL; }
		;

table_references:
		table_reference
		{
			$$ = node_manager->MakeNodeList($1);
		}
    	|  table_references ',' table_reference
    	{
     		$$ = $1;
        	$$->PushBack($3);
    	}
    	;

table_reference:
			table_factor
			{
				$$ = $1;
			}
			|join_clause
			{
				$$ = $1;
			}
			|last_join_clause
			{
				$$ = $1;
			}
			|query_reference
			{
				$$ = $1;
			}
			;


table_factor:
  relation_factor
    {
        $$ = node_manager->MakeTableNode($1, "");
    }
  | relation_factor AS relation_name
    {
        $$ = node_manager->MakeTableNode($1, $3);
    }
  | relation_factor relation_name
    {
        $$ = node_manager->MakeTableNode($1, $2);
    }

  ;
relation_factor:
    	relation_name
    	{
    		$$ = $1;
    	}
    	;

query_reference:
		'(' query_clause ')' {
			$$ = node_manager->MakeQueryRefNode($2, "");
		}
		| '(' query_clause ')' relation_name {
			$$ = node_manager->MakeQueryRefNode($2, $4);
		}
		| '(' query_clause ')' AS relation_name {
			$$ = node_manager->MakeQueryRefNode($2, $5);
		}
		;

join_clause:
		table_reference join_type JOIN table_reference join_condition
		{
			$$ = node_manager->MakeJoinNode($1, $4, $2, $5, "");
		}
		| table_reference join_type JOIN table_reference join_condition relation_name
		{
			$$ = node_manager->MakeJoinNode($1, $4, $2, $5, $6);
		}
		| table_reference join_type JOIN table_reference join_condition AS relation_name
		{
			$$ = node_manager->MakeJoinNode($1, $4, $2, $5, $7);
		}
		;
last_join_clause:
		table_reference LAST JOIN table_reference sort_clause join_condition
		{
			$$ = node_manager->MakeLastJoinNode($1, $4, $5, $6, "");
		}
		| table_reference LAST JOIN table_reference sort_clause join_condition relation_name
		{
			$$ = node_manager->MakeLastJoinNode($1, $4, $5, $6, $7);
		}
		| table_reference LAST JOIN table_reference sort_clause join_condition AS relation_name
		{
			$$ = node_manager->MakeLastJoinNode($1, $4, $5, $6, $8);
		}
		;
union_stmt:
		query_clause UNION query_clause
		{
			$$ = node_manager->MakeUnionQueryNode($1, $3, false);
		}
		|query_clause UNION DISTINCT query_clause
		{
			$$ = node_manager->MakeUnionQueryNode($1, $4, false);
		}
		| query_clause UNION ALL query_clause
		{
			$$ = node_manager->MakeUnionQueryNode($1, $4, true);
		}
		;
join_type:
		FULL join_outer
		{
			$$ = fesql::node::kJoinTypeFull;
		}
		|LAST join_outer
		{
			$$ = fesql::node::kJoinTypeLast;
		}
		|LEFT join_outer
		{
			$$ = fesql::node::kJoinTypeLeft;
		}
		|RIGHT join_outer
		{
			$$ = fesql::node::kJoinTypeRight;
		}
		|INNER
		{
			$$ = fesql::node::kJoinTypeInner;
		}
		;

join_outer:
		OUTER {
			$$ = NULL;
		}
		|/*EMPTY*/
		{
			$$ = NULL;
		}
		;
join_condition:
		ON sql_expr
		{
			$$ = $2;
		}
		| USING '(' sql_id_list ')'
		{
			$$ = $3;
		}
		;
/**** expressions ****/
fun_expr_list:
    fun_expr
    {
      $$ = node_manager->MakeExprList($1);
    }
  	| fun_expr_list ',' fun_expr
    {
        $$ = $1;
        $$->AddChild($3);
    }
  	;

/**** expressions ****/
sql_expr_list:
	sql_expr
	{
	  $$ = node_manager->MakeExprList($1);
	}
	| sql_expr_list ',' sql_expr
	{
		$$ = $1;
		$$->AddChild($3);
	}
	;
sql_id_list:
	SQL_IDENTIFIER
	{
		$$ = node_manager->MakeExprList(node_manager->MakeExprIdNode($1));
	}
	| sql_id_list ',' SQL_IDENTIFIER
	{
		$$ = $1;
		$$->AddChild(node_manager->MakeExprIdNode($3));
	}
	;


fun_expr:
	 var   	{ $$ = $1; }
     | expr_const 	{ $$ = $1; }
     | function_name '(' ')'  	{
     	$$ = node_manager->MakeFuncNode($1, NULL, NULL);
     }
     | function_name '(' fun_expr_list ')'
     {
     	$$ = node_manager->MakeFuncNode($1, $3, NULL);
     }
     | fun_expr '+' fun_expr
     {
     	$$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpAdd);
     }
     | fun_expr '-' fun_expr
     {
     	$$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpMinus);
     }
     | fun_expr '*' fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpMulti);
     }
     | fun_expr '/' fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpFDiv);
     }
     | fun_expr '%' fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpMod);
     }
     | fun_expr '>' fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpGt);
     }
     | fun_expr '<' fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpLt);
     }
     | fun_expr LESS_EQUALS fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpLe);
     }
     | fun_expr EQUALS fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpEq);
     }
     | fun_expr NOT_EQUALS fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpNeq);
     }
     | fun_expr GREATER_EQUALS fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpGe);
     }
     | fun_expr ANDOP fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpAnd);
     }
     | fun_expr OR fun_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpOr);
     }
     | fun_expr '[' fun_expr ']'
	 {
	 	$$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpAt);
	 }
     | '!' fun_expr
     {
        $$ = node_manager->MakeUnaryExprNode($2, ::fesql::node::kFnOpNot);
     }
     | NOT fun_expr
     {
        $$ = node_manager->MakeUnaryExprNode($2, ::fesql::node::kFnOpNot);
     }
     | '(' fun_expr ')'
     {
        $$ = node_manager->MakeUnaryExprNode($2, ::fesql::node::kFnOpBracket);
     }
     ;

sql_expr:
     column_ref			{ $$ = $1; }
     | expr_const 	{ $$ = $1; }
     | primary_time	{ $$ = $1; }
     | sql_call_expr  	{ $$ = $1; }
     | sql_expr '+' sql_expr
     {
     	$$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpAdd);
     }
     | sql_expr '-' sql_expr
     {
     	$$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpMinus);
     }
     | sql_expr '*' sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpMulti);
     }
     | sql_expr '/' sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpFDiv);
     }
     | sql_expr '%' sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpMod);
     }
     | sql_expr '>' sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpGt);
     }
     | sql_expr '<' sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpLt);
     }
     | sql_expr LESS_EQUALS sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpLe);
     }
     | sql_expr EQUALS sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpEq);
     }
     | sql_expr NOT_EQUALS sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpNeq);
     }
     | sql_expr GREATER_EQUALS sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpGe);
     }
     | sql_expr ANDOP sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpAnd);
     }
     | sql_expr OR sql_expr
     {
        $$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpOr);
     }
     | sql_expr '[' sql_expr ']'
	 {
	 	$$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpAt);
	 }
     | '!' sql_expr
     {
        $$ = node_manager->MakeUnaryExprNode($2, ::fesql::node::kFnOpNot);
     }
     | NOT sql_expr
     {
        $$ = node_manager->MakeUnaryExprNode($2, ::fesql::node::kFnOpNot);
     }
     | sql_expr LIKE sql_expr
     {
     	$$ = node_manager->MakeBinaryExprNode($1, $3, ::fesql::node::kFnOpLike);
     }
     | sql_expr NOT LIKE sql_expr
     {
     	$$ = node_manager->MakeUnaryExprNode(
     		node_manager->MakeBinaryExprNode($1, $4, ::fesql::node::kFnOpLike),
     		::fesql::node::kFnOpNot);

     }
     | sql_expr IN '(' sql_expr_list ')'
     {
     	$$ = node_manager->MakeBinaryExprNode($1, $4, ::fesql::node::kFnOpIn);
     }
     | sql_expr NOT IN '(' sql_expr_list ')'
     {
     	$$ = node_manager->MakeUnaryExprNode(
     		node_manager->MakeBinaryExprNode($1, $5, ::fesql::node::kFnOpIn),
     		::fesql::node::kFnOpNot);
     }
     | sql_expr BETWEEN sql_expr AND sql_expr
     {
     	$$ = node_manager->MakeBetweenExpr($1, $3, $5);
     }
     | '(' sql_expr ')'
     {
        $$ = node_manager->MakeUnaryExprNode($2, ::fesql::node::kFnOpBracket);
     }
     | '(' query_clause ')' {
     	$$ = node_manager->MakeQueryExprNode($2);
     }
     ;

expr_const:
    STRING
        {
        	$$ = node_manager->MakeConstNode($1);
			free($1);
        }
  	| INTNUM
        { $$ = (node_manager->MakeConstNode($1)); }
  	| LONGNUM
        { $$ = (node_manager->MakeConstNode($1)); }
  	| DOUBLENUM
        { $$ = (node_manager->MakeConstNode($1)); }
  	| FLOATNUM
        { $$ = (node_manager->MakeConstNode($1)); }
  	| BOOL
        { $$ = (node_manager->MakeConstNode($1)); }
  	| NULLX
        { $$ = (node_manager->MakeConstNode()); }
  	;

sql_call_expr:
    function_name '(' '*' ')' over_clause
    {
          if (strcasecmp($1, "count") != 0)
          {
            yyerror(&(@3), scanner, trees, node_manager, status, "Only COUNT function can be with '*' parameter!");
            YYABORT;
          }
          else
          {
            $$ = node_manager->MakeFuncNode($1, NULL, $5);
          }
    }
    | function_name '(' sql_expr_list ')' over_clause
    {
        $$ = node_manager->MakeFuncNode($1, $3, $5);
    }
    ;

where_expr:
			WHERE sql_expr
			{
				$$ = $2;
			}
			| /*EMPTY*/
			{
				$$ = NULL;
			}
			;
group_expr:
			GROUP BY sql_expr_list
			{
				$$ = $3;
			}
			|/*EMPTY*/
			{
				$$ = NULL;
			}

having_expr:
			HAVING sql_expr
			{
				$$ = $2;
			}
			| /*EMPTY*/
            {
            	$$ = NULL;
            }

/***** Window Definitions */
window_clause:
			WINDOW window_definition_list			{ $$ = $2; }
			| /*EMPTY*/								{ $$ = NULL; }
		;

window_definition_list:
    window_definition
    {
        $$ = node_manager->MakeNodeList($1);
    }
	| window_definition_list ',' window_definition
	{
	 	$$ = $1;
        $$->PushBack($3);
	}
	;

window_definition:
		SQL_IDENTIFIER AS window_specification
		{
		    ((::fesql::node::WindowDefNode*)$3)->SetName($1);
			free($1);
		    $$ = $3;
		}
		;

window_specification:
				'(' opt_existing_window_name opt_union_clause opt_partition_clause
					opt_sort_clause opt_frame_clause opt_instance_not_in_window ')'
					{
                 		$$ = node_manager->MakeWindowDefNode($3, $4, $5, $6, $7);
                 	}
		;

opt_existing_window_name:
						SQL_IDENTIFIER { $$ = $1; }
						| /*EMPTY*/		{ $$ = NULL; }

                        ;
opt_union_clause:
				UNION table_references
				{
					$$ = $2;
				}
				| /*EMPTY*/
				{
					$$ = NULL;
				}
opt_partition_clause: PARTITION BY column_ref_list		{ $$ = $3; }
			            | /*EMPTY*/					{ $$ = NULL; }


opt_instance_not_in_window:
			INSTANCE_NOT_IN_WINDOW { $$ = true; }
			| /*EMPTY*/ {$$ = false;}
limit_clause:
            LIMIT INTNUM
            {
                $$ = node_manager->MakeLimitNode($2);
            }
            | LIMIT LONGNUM
            {
                $$ = node_manager->MakeLimitNode($2);
            }
            | /*EMPTY*/ {$$ = NULL;}
            ;
/*===========================================================
 *
 *	Sort By Clasuse
 *
 *===========================================================*/

opt_sort_clause:
			sort_clause								{ $$ = $1; }
			| /*EMPTY*/								{ $$ = NULL; }
		    ;

sort_clause:
			ORDER BY sql_expr_list
			{
				$$ = node_manager->MakeOrderByNode($3, true);
			}
			|ORDER BY sql_expr_list ASC
			{
				$$ = node_manager->MakeOrderByNode($3, true);
			}
			|ORDER BY sql_expr_list  DESC
			{
				$$ = node_manager->MakeOrderByNode($3, false);
			}
		    ;
/*===========================================================
 *
 *	Frame Clasuse
 *
 *===========================================================*/
opt_frame_clause:
	        RANGE frame_extent opt_window_exclusion_clause
				{
				    $$ = node_manager->MakeRangeFrameNode($2);

				}
			| ROWS frame_extent opt_window_exclusion_clause
				{
				    $$ = node_manager->MakeRowsFrameNode($2);
				}
			|
			/*EMPTY*/
			{
			    $$ = NULL;
		    }
		    ;

opt_window_exclusion_clause:
             /*EMPTY*/				{ $$ = 0; }
            ;
frame_extent: BETWEEN frame_bound AND frame_bound
				{
				    $$ = node_manager->MakeFrameNode($2, $4);
				}
		;


frame_bound:
			UNBOUNDED PRECEDING
				{
				    $$ = (fesql::node::SQLNode*)(node_manager->MakeFrameBound(fesql::node::kPreceding));
				}
			| UNBOUNDED FOLLOWING
				{
				    $$ = (fesql::node::SQLNode*)(node_manager->MakeFrameBound(fesql::node::kFollowing));
				}
			| CURRENT ROW
				{
				    $$ = (fesql::node::SQLNode*)(node_manager->MakeFrameBound(fesql::node::kCurrent));
				}
			| frame_expr PRECEDING
				{
				    $$ = (fesql::node::SQLNode*)(node_manager->MakeFrameBound(fesql::node::kPreceding, $1));
				}
			| frame_expr FOLLOWING
				{
				    $$ = (fesql::node::SQLNode*)(node_manager->MakeFrameBound(fesql::node::kFollowing, $1));
				}
		;

frame_expr: expr_const
			|primary_time
			;
column_ref:
    column_name
    {
        $$ = node_manager->MakeColumnRefNode($1, "");
    }
  | relation_name '.' column_name
    {
        $$ = node_manager->MakeColumnRefNode($3, $1);
    }
  |
    relation_name '.' '*'
    {
        $$ = node_manager->MakeColumnRefNode("*", $1);
    }
  ;

/*===========================================================
 *
 *	Name classification
 *
 *===========================================================*/

database_name:
	SQL_IDENTIFIER
	;

group_name:
	SQL_IDENTIFIER
	;

table_name:
	SQL_IDENTIFIER
	;
column_name:
    SQL_IDENTIFIER
  ;

relation_name:
    SQL_IDENTIFIER
  ;

function_name:
    SQL_IDENTIFIER
    |FUN_IDENTIFIER
  ;

%%


void emit(const char *s, ...)
{
//  	va_list ap;
//  	va_start(ap, s);
//  	printf("rpn: ");
//  	vfprintf(stdout, s, ap);
//  	printf("\n");
}

