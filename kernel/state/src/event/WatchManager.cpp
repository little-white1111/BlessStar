#include "bs/kernel/state/WatchManager.h"

#include <cstdlib>
#include <cstring>

#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct WatcherEntry
{
    WatchCallback callback;
    WatchMode     mode;
    void*         userData;
    bool          isOnce;
};

struct WatchManager
{
    std::unordered_map<std::string, std::vector<WatcherEntry>> watchers;
    std::shared_mutex                                          mutex;
};

WatchManager* WatchManager_Create()
{
    return new WatchManager();
}

void WatchManager_Destroy(WatchManager* wm)
{
    if (!wm)
        return;
    delete wm;
}

int WatchManager_AddWatch(WatchManager* wm, const char* path, WatchCallback callback,
                          WatchMode mode, void* userData)
{
    if (!wm || !path || !callback)
        return -1;

    std::unique_lock<std::shared_mutex> lock(wm->mutex);

    WatcherEntry entry;
    entry.callback = callback;
    entry.mode     = mode;
    entry.userData = userData;
    entry.isOnce   = (mode == WATCH_MODE_ONCE);

    wm->watchers[path].push_back(entry);

    return 0;
}

int WatchManager_RemoveWatch(WatchManager* wm, const char* path, WatchCallback callback)
{
    if (!wm || !path || !callback)
        return -1;

    std::unique_lock<std::shared_mutex> lock(wm->mutex);

    auto it = wm->watchers.find(path);
    if (it == wm->watchers.end())
        return -2;

    auto& entries = it->second;
    for (auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
        if (entryIt->callback == callback)
        {
            entries.erase(entryIt);
            break;
        }
    }

    return 0;
}

int WatchManager_Notify(WatchManager* wm, const char* path, ConfigEventType type, const void* data)
{
    if (!wm || !path)
        return -1;

    std::shared_lock<std::shared_mutex> lock(wm->mutex);

    auto it = wm->watchers.find(path);
    if (it == wm->watchers.end())
    {
        return 0;
    }

    std::vector<WatcherEntry> tempEntries;
    for (auto& entry : it->second)
    {
        tempEntries.push_back(entry);
    }

    lock.unlock();

    std::vector<WatchCallback> onceCallbacks;

    for (auto& entry : tempEntries)
    {
        entry.callback(path, type, data, entry.userData);

        if (entry.isOnce)
        {
            onceCallbacks.push_back(entry.callback);
        }
    }

    if (!onceCallbacks.empty())
    {
        std::unique_lock<std::shared_mutex> lock2(wm->mutex);
        auto                                it2 = wm->watchers.find(path);
        if (it2 != wm->watchers.end())
        {
            auto& entries = it2->second;
            for (auto callback : onceCallbacks)
            {
                for (auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
                {
                    if (entryIt->callback == callback)
                    {
                        entries.erase(entryIt);
                        break;
                    }
                }
            }
        }
    }

    return 0;
}
