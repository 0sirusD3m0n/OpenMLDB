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
#include "bm/storage_bm_case.h"
#include "llvm/Transforms/Scalar.h"

namespace fesql {
namespace bm {
using namespace ::llvm;  // NOLINT

static void BM_ArrayListIterate(benchmark::State& state) {  // NOLINT
    ArrayListIterate(&state, BENCHMARK, state.range(0));
}

static void BM_ByteMemPoolAlloc1000(benchmark::State& state) {  // NOLINT
    ByteMemPoolAlloc1000(&state, BENCHMARK, state.range(0));
}
static void BM_NewFree1000(benchmark::State& state) {  // NOLINT
    NewFree1000(&state, BENCHMARK, state.range(0));
}

BENCHMARK(BM_ArrayListIterate)->Args({100})->Args({1000})->Args({10000});
BENCHMARK(BM_ByteMemPoolAlloc1000)
    ->Args({10})
    ->Args({100})
    ->Args({1000})
    ->Args({10000});
BENCHMARK(BM_NewFree1000)->Args({10})->Args({100})->Args({1000})->Args({10000});

}  // namespace bm
}  // namespace fesql

BENCHMARK_MAIN();
