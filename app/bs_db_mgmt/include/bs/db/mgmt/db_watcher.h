#ifndef BS_DB_MGMT_DB_WATCHER_H
#define BS_DB_MGMT_DB_WATCHER_H

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace bs::db::mgmt
{

class DbWatcher
{
public:
    using Callback = std::function<void()>;

    DbWatcher();
    ~DbWatcher();

    DbWatcher(const DbWatcher&) = delete;
    DbWatcher& operator=(const DbWatcher&) = delete;

    void Watch(const std::string& file_path, int interval_ms, Callback cb);
    void Stop();

    bool watching() const { return running_; }

private:
    void loop();

    std::atomic<bool>           running_{false};
    std::unique_ptr<std::thread> thread_;
    std::string                  file_path_;
    int                          interval_ms_ = 5000;
    Callback                     callback_;
    std::filesystem::file_time_type last_write_time_;
};

} // namespace bs::db::mgmt

#endif
