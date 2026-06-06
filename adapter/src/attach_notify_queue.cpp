#include <condition_variable>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "attach_notify_queue_internal.h"

namespace
{
struct WatchNotifyJob
{
    WatchManager*   wm = nullptr;
    std::string     path;
    ConfigEventType type     = CONFIG_EVENT_ENTER_INITIAL;
    const void*     snapshot = nullptr;
};

struct AttachNotifyQueue
{
    std::mutex                 mu;
    std::condition_variable    cv;
    std::deque<WatchNotifyJob> jobs;
    std::thread                worker;
    std::atomic<bool>          stop{false};
    std::atomic<int>           in_flight{0};
};

AttachNotifyQueue* queue_of(AttachContext* ctx)
{
    return ctx ? static_cast<AttachNotifyQueue*>(ctx->notify_queue) : nullptr;
}

void run_worker(AttachNotifyQueue* q)
{
    for (;;)
    {
        WatchNotifyJob job{};
        {
            std::unique_lock<std::mutex> lock(q->mu);
            q->cv.wait(lock, [&] { return q->stop.load() || !q->jobs.empty(); });
            if (q->stop.load() && q->jobs.empty())
                return;
            job = std::move(q->jobs.front());
            q->jobs.pop_front();
            q->in_flight.fetch_add(1);
        }

        if (job.wm && job.path.c_str())
            (void)bs_watch_manager_notify(job.wm, job.path.c_str(), job.type, job.snapshot);

        q->in_flight.fetch_sub(1);
        q->cv.notify_all();
    }
}

void phase2_watch_hook(ConfigManager* /*cm*/, WatchManager* wm, const char* path,
                       ConfigEventType type, const void* snapshot, void* user_data)
{
    auto* ctx = static_cast<AttachContext*>(user_data);
    if (!ctx)
        return;
    bs_adapter_attach_notify_queue_enqueue_watch(ctx, wm, path, type, snapshot);
}

} // namespace

void bs_adapter_attach_notify_queue_bind(AttachContext* ctx)
{
    if (!ctx || ctx->notify_queue)
        return;

    auto* q           = new AttachNotifyQueue();
    q->worker         = std::thread(run_worker, q);
    ctx->notify_queue = q;
    bs_adapter_attach_config_register_phase2_notify(ctx, phase2_watch_hook);
}

void bs_adapter_attach_notify_queue_enqueue_watch(AttachContext* ctx, WatchManager* wm,
                                                  const char* path, ConfigEventType type,
                                                  const void* snapshot)
{
    auto* q = queue_of(ctx);
    if (!q || !wm || !path)
        return;

    WatchNotifyJob job{};
    job.wm       = wm;
    job.path     = path;
    job.type     = type;
    job.snapshot = snapshot;

    {
        std::lock_guard<std::mutex> lock(q->mu);
        q->jobs.push_back(std::move(job));
    }
    q->cv.notify_one();
}

void bs_adapter_attach_notify_queue_flush(AttachContext* ctx)
{
    auto* q = queue_of(ctx);
    if (!q)
        return;

    std::unique_lock<std::mutex> lock(q->mu);
    q->cv.wait(lock, [&] { return q->jobs.empty() && q->in_flight.load() == 0; });
}

void bs_adapter_attach_notify_queue_shutdown(AttachContext* ctx)
{
    if (!ctx || !ctx->notify_queue)
        return;

    auto* q = static_cast<AttachNotifyQueue*>(ctx->notify_queue);
    bs_adapter_attach_notify_queue_flush(ctx);

    {
        std::lock_guard<std::mutex> lock(q->mu);
        q->stop.store(true);
    }
    q->cv.notify_all();
    if (q->worker.joinable())
        q->worker.join();

    delete q;
    ctx->notify_queue = nullptr;
}
