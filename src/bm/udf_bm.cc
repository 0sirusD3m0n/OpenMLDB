/*
 * udf_bm.cc
 * Copyright (C) 4paradigm.com 2019 wangtaize <wangtaize@4paradigm.com>
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

#include "benchmark/benchmark.h"
#include "bm/udf_bm_case.h"
#include "llvm/Transforms/Scalar.h"

namespace fesql {
namespace bm {
using namespace ::llvm;                                 // NOLINT
static void BM_MemSumColInt(benchmark::State& state) {  // NOLINT
    SumMemTableCol(&state, BENCHMARK, state.range(0), "col1");
}

static void BM_MemSumColDouble(benchmark::State& state) {  // NOLINT
    SumMemTableCol(&state, BENCHMARK, state.range(0), "col4");
}

static void BM_ArraySumColInt(benchmark::State& state) {  // NOLINT
    SumArrayListCol(&state, BENCHMARK, state.range(0), "col1");
}

static void BM_ArraySumColDouble(benchmark::State& state) {  // NOLINT
    SumArrayListCol(&state, BENCHMARK, state.range(0), "col4");
}

static void BM_CopyMemSegment(benchmark::State& state) {  // NOLINT
    CopyMemSegment(&state, BENCHMARK, state.range(0));
}
static void BM_CopyMemTable(benchmark::State& state) {  // NOLINT
    CopyMemTable(&state, BENCHMARK, state.range(0));
}
static void BM_CopyArrayList(benchmark::State& state) {  // NOLINT
    CopyArrayList(&state, BENCHMARK, state.range(0));
}

static void BM_TabletFullIterate(benchmark::State& state) {  // NOLINT
    TabletFullIterate(&state, BENCHMARK, state.range(0));
}

static void BM_TabletWindowIterate(benchmark::State& state) {  // NOLINT
    TabletWindowIterate(&state, BENCHMARK, state.range(0));
}
static void BM_MemTableIterate(benchmark::State& state) {  // NOLINT
    MemTableIterate(&state, BENCHMARK, state.range(0));
}

static void BM_MemSegmentIterate(benchmark::State& state) {  // NOLINT
    MemSegmentIterate(&state, BENCHMARK, state.range(0));
}

static void BM_Day(benchmark::State& state) {  // NOLINT
    CTimeDay(&state, BENCHMARK, state.range(0));
}

static void BM_Month(benchmark::State& state) {  // NOLINT
    CTimeMonth(&state, BENCHMARK, state.range(0));
}
static void BM_Year(benchmark::State& state) {  // NOLINT
    CTimeYear(&state, BENCHMARK, state.range(0));
}
static void BM_TimestampToString(benchmark::State& state) {  // NOLINT
    TimestampToString(&state, BENCHMARK);
}
static void BM_TimestampFormat(benchmark::State& state) {  // NOLINT
    TimestampFormat(&state, BENCHMARK);
}

static void BM_DateToString(benchmark::State& state) {  // NOLINT
    DateToString(&state, BENCHMARK);
}
static void BM_DateFormat(benchmark::State& state) {  // NOLINT
    DateFormat(&state, BENCHMARK);
}

static void BM_AllocFromByteMemPool1000(benchmark::State& state) {  // NOLINT
    ByteMemPoolAlloc1000(&state, BENCHMARK, state.range(0));
}
static void BM_AllocFromNewFree1000(benchmark::State& state) {  // NOLINT
    NewFree1000(&state, BENCHMARK, state.range(0));
}
BENCHMARK(BM_TabletFullIterate)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});

BENCHMARK(BM_TabletWindowIterate)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});

BENCHMARK(BM_MemTableIterate)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});

BENCHMARK(BM_MemSegmentIterate)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});

BENCHMARK(BM_CopyArrayList)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});
BENCHMARK(BM_CopyMemSegment)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});

BENCHMARK(BM_CopyMemTable)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});

BENCHMARK(BM_ArraySumColInt)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});
BENCHMARK(BM_ArraySumColDouble)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});

BENCHMARK(BM_MemSumColInt)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});

BENCHMARK(BM_MemSumColDouble)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});
BENCHMARK(BM_Day)->Args({1})->Args({10})->Args({100})->Args({1000})->Args(
    {10000});
BENCHMARK(BM_Month)->Args({1})->Args({10})->Args({100})->Args({1000})->Args(
    {10000});
BENCHMARK(BM_Year)->Args({1})->Args({10})->Args({100})->Args({1000})->Args(
    {10000});
BENCHMARK(BM_AllocFromByteMemPool1000)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});
BENCHMARK(BM_AllocFromNewFree1000)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});

BENCHMARK(BM_TimestampFormat);
BENCHMARK(BM_TimestampToString);
BENCHMARK(BM_DateFormat);
BENCHMARK(BM_DateToString);
}  // namespace bm
}  // namespace fesql

BENCHMARK_MAIN();
