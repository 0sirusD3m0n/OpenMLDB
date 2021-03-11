// file_sender.h
// Copyright (C) 2017 4paradigm.com
// Author denglong
// Date 2019-08-14
//

#pragma once

#include <brpc/channel.h>
#include <brpc/controller.h>
#include <string>
#include "proto/tablet.pb.h"

namespace rtidb {
namespace tablet {

class FileSender {
 public:
    FileSender(uint32_t tid, uint32_t pid,
               const std::string& endpoint);
    ~FileSender();
    bool Init();
    int SendFile(const std::string& file_name, const std::string& dir_name,
                 const std::string& full_path);
    int SendFile(const std::string& file_name, const std::string& full_path);
    int SendFileInternal(const std::string& file_name,
                         const std::string& dir_name,
                         const std::string& full_path, uint64_t file_size);
    int SendDir(const std::string& dir_name, const std::string& full_path);
    int WriteData(const std::string& file_name, const std::string& dir_name,
                  const char* buffer, size_t len, uint64_t block_id);
    int CheckFile(const std::string& file_name, const std::string& dir_name,
                  uint64_t file_size);

 private:
    uint32_t tid_;
    uint32_t pid_;
    std::string endpoint_;
    uint32_t cur_try_time_;
    uint32_t max_try_time_;
    uint64_t limit_time_;
    brpc::Channel* channel_;
    ::rtidb::api::TabletServer_Stub* stub_;
};

}  // namespace tablet
}  // namespace rtidb
