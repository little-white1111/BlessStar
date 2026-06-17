#ifndef BS_APP_SDK_COMMON_BS_FACTORY_H
#define BS_APP_SDK_COMMON_BS_FACTORY_H

#include <functional>
#include <map>
#include <mutex>

namespace bs::app
{

template<typename TKey, typename TFn>
class BsFactory
{
public:
    bool Register(TKey key, TFn fn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return registry_.emplace(key, std::move(fn)).second;
    }

    TFn* Get(TKey key) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(key);
        return it != registry_.end() ? &it->second : nullptr;
    }

    bool Has(TKey key) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return registry_.find(key) != registry_.end();
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_.clear();
    }

private:
    mutable std::mutex            mutex_;
    std::map<TKey, TFn> registry_;
};

} // namespace bs::app

#endif // BS_APP_SDK_COMMON_BS_FACTORY_H
