/*
 * flags.cc
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

#include <gflags/gflags.h>
// cluster config
DEFINE_string(endpoint, "", "config the ip and port that fesql serves for");
DEFINE_int32(port, 0, "config the port that fesql serves for");
DEFINE_int32(thread_pool_size, 8, "config the thread pool for dbms and tablet");
DEFINE_string(tablet_endpoint, "",
              "config the ip and port that fesql tablet for");
// for tablet
DEFINE_string(dbms_endpoint, "", "config the ip and port that fesql dbms for");
DEFINE_bool(enable_keep_alive, true, "config if tablet keep alive with dbms");

// batch config
DEFINE_string(default_db_name, "_fesql",
              "config the default batch catalog db name");
DEFINE_string(native_fesql_libs_prefix,
              "",
              "config the native fesql libs prefix");
DEFINE_string(native_fesql_libs_name,
              "felibs",
              "config the native fesql libs name");
DEFINE_bool(enable_column_sum_opt, true, "enable column sum merge");
DEFINE_bool(enable_window_merge_opt, true, "enable window merge");
