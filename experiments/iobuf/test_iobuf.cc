/*
 * test_iobuf.cc
 * Copyright (C) 4paradigm.com 2020 wangtaize <wangtaize@4paradigm.com>
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

#include <stdio.h>
#include "butil/iobuf.h"

int main(int argc, char* argv[]) {
    butil::IOBuf buf;
    uint32_t i = 9;
    int size = buf.append(reinterpret_cast<const void*>(&i), 4);
    printf("append size %d \n", size);

    uint32_t j = 0;
    size_t len = buf.copy_to(reinterpret_cast<void*>(&j), 4, 0);
    printf("read len %lu , j %d \n", len, j);
    return 0;
}

