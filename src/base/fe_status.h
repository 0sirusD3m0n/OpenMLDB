/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * status.h
 *
 * Author: chenjing
 * Date: 2019/11/21
 *--------------------------------------------------------------------------
 **/

#ifndef SRC_BASE_FE_STATUS_H_
#define SRC_BASE_FE_STATUS_H_
#include <string>
#include "glog/logging.h"
#include "proto/fe_common.pb.h"
#include "proto/fe_type.pb.h"

namespace fesql {
namespace base {

template <typename STREAM, typename... Args>
static inline std::initializer_list<int> __output_literal_args(
    STREAM& stream,  // NOLINT
    Args... args) {  // NOLINT
    return std::initializer_list<int>{(stream << args, 0)...};
}

#define MAX_STATUS_TRACE_SIZE 4096

#define CHECK_STATUS(call, ...)                                              \
    while (true) {                                                           \
        auto _status = (call);                                               \
        if (!_status.isOK()) {                                               \
            std::stringstream _msg;                                          \
            fesql::base::__output_literal_args(_msg, ##__VA_ARGS__);         \
            std::stringstream _trace;                                        \
            fesql::base::__output_literal_args(_trace, "    (At ", __FILE__, \
                                               ":", __LINE__, ")");          \
            if (_status.trace.size() >= MAX_STATUS_TRACE_SIZE) {             \
                LOG(WARNING) << "Internal error: " << _status.msg << "\n"    \
                             << _status.trace;                               \
            } else {                                                         \
                if (!_status.msg.empty()) {                                  \
                    _trace << "\n"                                           \
                           << "(Caused by) " << _status.msg;                 \
                }                                                            \
                _trace << "\n" << _status.trace;                             \
            }                                                                \
            return fesql::base::Status(_status.code, _msg.str(),             \
                                       _trace.str());                        \
        }                                                                    \
        break;                                                               \
    }

#define CHECK_TRUE(call, errcode, ...)                                       \
    while (true) {                                                           \
        if (!(call)) {                                                       \
            std::stringstream _msg;                                          \
            fesql::base::__output_literal_args(_msg, ##__VA_ARGS__);         \
            std::stringstream _trace;                                        \
            fesql::base::__output_literal_args(_trace, "    (At ", __FILE__, \
                                               ":", __LINE__, ")");          \
            fesql::base::Status _status(errcode, _msg.str(), _trace.str());  \
            return _status;                                                  \
        }                                                                    \
        break;                                                               \
    }

struct Status {
    Status() : code(common::kOk), msg("ok") {}

    Status(common::StatusCode status_code, const std::string& msg_str)
        : code(status_code), msg(msg_str) {}

    Status(common::StatusCode status_code, const std::string& msg_str,
           const std::string& trace_str)
        : code(status_code), msg(msg_str), trace(trace_str) {}

    static Status OK() { return Status(); }

    inline bool isOK() const { return code == common::kOk; }
    inline bool isRunning() const { return code == common::kRunning; }

    const std::string str() const { return msg + "\n" + trace; }

    common::StatusCode code;

    std::string msg;
    std::string trace;
};

std::ostream& operator<<(std::ostream& os, const Status& status);  // NOLINT

}  // namespace base
}  // namespace fesql
#endif  // SRC_BASE_FE_STATUS_H_
