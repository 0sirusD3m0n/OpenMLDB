/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * fesql_client_bm_case.h
 *
 * Author: chenjing
 * Date: 2019/12/25
 *--------------------------------------------------------------------------
 **/

#ifndef SRC_BM_FESQL_CLIENT_BM_CASE_H_
#define SRC_BM_FESQL_CLIENT_BM_CASE_H_
#include <stdio.h>
#include <stdlib.h>
#include "benchmark/benchmark.h"
namespace fesql {
namespace bm {
enum MODE { BENCHMARK, TEST };
void SIMPLE_CASE1_QUERY(benchmark::State *state_ptr, MODE mode,
                        bool is_batch_mode, int64_t group_size,
                        int64_t max_window_size);
void WINDOW_CASE1_QUERY(benchmark::State *state_ptr, MODE mode,
                        bool is_batch_mode, int64_t group_size,
                        int64_t max_window_size);
void WINDOW_CASE2_QUERY(benchmark::State *state_ptr, MODE mode,
                        bool is_batch_mode, int64_t group_size,
                        int64_t max_window_size);
void WINDOW_CASE3_QUERY(benchmark::State *state_ptr, MODE mode,
                        bool is_batch_mode, int64_t group_size,
                        int64_t max_window_size);
void WINDOW_CASE0_QUERY(benchmark::State *state_ptr, MODE mode,
                        bool is_batch_mode, int64_t group_size,
                        int64_t max_window_size);

void GROUPBY_CASE0_QUERY(benchmark::State *state_ptr, MODE mode,
                         bool is_batch_mode, int64_t group_size,
                         int64_t max_window_size);

}  // namespace bm
}  // namespace fesql
#endif  // SRC_BM_FESQL_CLIENT_BM_CASE_H_
