#include "bs/db/mgmt/db_watcher.h"

namespace bs::db::mgmt {

DbWatcher::DbWatcher() = default;
DbWatcher::~DbWatcher() { Stop(); }

void DbWatcher::Watch(const std::string& file_path, int interval_ms, Callback cb)
{
    if (running_) return;
    file_path_ = file_path;
    interval_ms_ = interval_ms;
    callback_ = std::move(cb);
    running_ = true;
    if (std::filesystem::exists(file_path_))
        last_write_time_ = std::filesystem::last_write_time(file_path_);
    thread_ = std::make_unique<std::thread>(&DbWatcher::loop, this);
}

void DbWatcher::Stop()
{
    running_ = false;
    if (thread_ && thread_->joinable()) thread_->join();
    thread_.reset();
}

void DbWatcher::loop()
{
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
        if (!running_) break;
        if (std::filesystem::exists(file_path_)) {
            auto current = std::filesystem::last_write_time(file_path_);
            if (current != last_write_time_) {
                last_write_time_ = current;
                if (callback_) callback_();
            }
        }
    }
}

} // namespace bs::db::mgmt
