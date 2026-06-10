#include "bs/kernel/common/bs_wait_trace.h"

#include <condition_variable>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "attach_notify_queue_internal.h"

namespace
{
struct WatchNotifyJob
{
    WatchManager*        wm = nullptr;
    std::string          path;
    ConfigEventType      type = CONFIG_EVENT_ENTER_INITIAL;
    std::vector<uint8_t> snapshot_bytes;
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

        if (job.wm && !job.path.empty())
        {
            const void* snap = job.snapshot_bytes.empty() ? nullptr : job.snapshot_bytes.data();
            const int   notify_t0 = bs_wait_trace_hang_begin("notify_queue:watch_notify");
            (void)bs_watch_manager_notify(job.wm, job.path.c_str(), job.type, snap);
            if (notify_t0 >= 0)
                bs_wait_trace_hang_end("notify_queue:watch_notify", notify_t0);
        }

        q->in_flight.fetch_sub(1);
        q->cv.notify_all();
    }
}

void phase2_watch_hook(ConfigManager* cm, WatchManager* wm, const char* path, ConfigEventType type,
                       const void* snapshot, size_t snapshot_size, void* user_data)
{
    (void)cm;
    auto* ctx = static_cast<AttachContext*>(user_data);
    if (!ctx)
        return;
    bs_adapter_attach_notify_queue_enqueue_watch(ctx, wm, path, type, snapshot, snapshot_size);
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
                                                  const void* snapshot, size_t snapshot_size)
{
    auto* q = queue_of(ctx);
    if (!q || !wm || !path)
        return;

    WatchNotifyJob job{};
    job.wm   = wm;
    job.path = path;
    job.type = type;
    if (snapshot && snapshot_size > 0)
    {
        const auto* bytes = static_cast<const uint8_t*>(snapshot);
        job.snapshot_bytes.assign(bytes, bytes + snapshot_size);
    }

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
    const int                    hang_t0 = bs_wait_trace_hang_begin("notify_queue:flush_wait");
    while (!(q->jobs.empty() && q->in_flight.load() == 0))
    {
        if (!q->jobs.empty())
            bs_wait_trace_hang_tick_u64("notify_queue:flush_wait_jobs", hang_t0,
                                        (unsigned long long)q->jobs.size());
        else
            bs_wait_trace_hang_tick_u64("notify_queue:flush_wait_in_flight", hang_t0,
                                        (unsigned long long)q->in_flight.load());
        q->cv.wait_for(lock, std::chrono::milliseconds(500),
                       [&] { return q->jobs.empty() && q->in_flight.load() == 0; });
    }
    if (hang_t0 >= 0)
        bs_wait_trace_hang_end("notify_queue:flush_wait", hang_t0);
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
    bs_wait_trace("notify_queue:worker_join_begin");
    if (q->worker.joinable())
        q->worker.join();
    bs_wait_trace("notify_queue:worker_join_done");

    delete q;
    ctx->notify_queue = nullptr;
}
