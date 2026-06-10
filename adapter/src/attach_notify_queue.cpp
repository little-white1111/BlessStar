#include "bs/kernel/common/bs_wait_trace.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>

#include <atomic>
#include <deque>
#include <functional>
#include <future>
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
    std::thread::id            worker_tid{};
    std::atomic<bool>          stop{false};
    std::atomic<bool>          drain_mode{false};
    std::atomic<int>           in_flight{0};
};

AttachNotifyQueue* queue_of(AttachContext* ctx)
{
    return ctx ? static_cast<AttachNotifyQueue*>(ctx->notify_queue) : nullptr;
}

#if defined(BS_TESTING)
static int watch_callback_timeout_ms(void)
{
    const char* v = std::getenv("BS_ATTACH_WATCH_CALLBACK_TIMEOUT_MS");
    if (!v || !*v)
        return 30000;
    return std::atoi(v);
}

static void invoke_watch_notify_with_timeout(WatchManager* wm, const char* path,
                                             ConfigEventType type, const void* snap)
{
    const int timeout_ms = watch_callback_timeout_ms();
    if (timeout_ms <= 0)
    {
        (void)bs_watch_manager_notify(wm, path, type, snap);
        return;
    }

    std::promise<void> completed;
    std::thread        watchdog(
        [timeout_ms, fut = completed.get_future().share()]
        {
            if (fut.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout)
            {
                std::fprintf(
                    stderr,
                    "BS_TESTING: watch callback exceeded BS_ATTACH_WATCH_CALLBACK_TIMEOUT_MS=%d\n",
                    timeout_ms);
                std::abort();
            }
        });

    (void)bs_watch_manager_notify(wm, path, type, snap);
    completed.set_value();
    if (watchdog.joinable())
        watchdog.join();
}
#endif

static void dispatch_watch_notify_job(const WatchNotifyJob& job)
{
    if (!job.wm || job.path.empty())
        return;
    const void* snap      = job.snapshot_bytes.empty() ? nullptr : job.snapshot_bytes.data();
    const int   notify_t0 = bs_wait_trace_hang_begin("notify_queue:watch_notify");
#if defined(BS_TESTING)
    invoke_watch_notify_with_timeout(job.wm, job.path.c_str(), job.type, snap);
#else
    (void)bs_watch_manager_notify(job.wm, job.path.c_str(), job.type, snap);
#endif
    if (notify_t0 >= 0)
        bs_wait_trace_hang_end("notify_queue:watch_notify", notify_t0);
}

void run_worker(AttachNotifyQueue* q)
{
    q->worker_tid = std::this_thread::get_id();
    for (;;)
    {
        WatchNotifyJob job{};
        {
            std::unique_lock<std::mutex> lock(q->mu);
            q->cv.wait(lock,
                       [&]
                       {
                           if (q->stop.load())
                               return true;
                           return !q->drain_mode.load() && !q->jobs.empty();
                       });
            if (q->stop.load() && q->jobs.empty())
                return;
            if (q->drain_mode.load() || q->jobs.empty())
                continue;
            job = std::move(q->jobs.front());
            q->jobs.pop_front();
            q->in_flight.fetch_add(1);
        }

        dispatch_watch_notify_job(job);

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

    /* Avoid deadlock: worker cannot flush while in_flight counts this thread. */
    if (q->worker_tid == std::this_thread::get_id())
        return;

    q->drain_mode.store(true);
    q->cv.notify_all();

    const int hang_t0 = bs_wait_trace_hang_begin("notify_queue:flush_wait");
    {
        std::unique_lock<std::mutex> lock(q->mu);
        while (q->in_flight.load() > 0)
        {
            bs_wait_trace_hang_tick_u64("notify_queue:flush_wait_in_flight", hang_t0,
                                        (unsigned long long)q->in_flight.load());
            q->cv.wait_for(lock, std::chrono::milliseconds(500),
                           [&] { return q->in_flight.load() == 0; });
        }

        while (!q->jobs.empty())
        {
            bs_wait_trace_hang_tick_u64("notify_queue:flush_wait_jobs", hang_t0,
                                        (unsigned long long)q->jobs.size());
            WatchNotifyJob job = std::move(q->jobs.front());
            q->jobs.pop_front();
            lock.unlock();
            dispatch_watch_notify_job(job);
            lock.lock();
        }
    }

    q->drain_mode.store(false);
    q->cv.notify_all();
    if (hang_t0 >= 0)
        bs_wait_trace_hang_end("notify_queue:flush_wait", hang_t0);
}

void bs_adapter_attach_notify_queue_shutdown(AttachContext* ctx)
{
    if (!ctx || !ctx->notify_queue)
        return;

    auto* q           = static_cast<AttachNotifyQueue*>(ctx->notify_queue);
    ctx->notify_queue = nullptr;

    q->drain_mode.store(false);
    {
        std::lock_guard<std::mutex> lock(q->mu);
        q->stop.store(true);
        q->jobs.clear();
    }
    q->cv.notify_all();

    const int hang_t0 = bs_wait_trace_hang_begin("notify_queue:shutdown_wait");
    {
        std::unique_lock<std::mutex> lock(q->mu);
        while (q->in_flight.load() > 0)
        {
            bs_wait_trace_hang_tick_u64("notify_queue:shutdown_wait_in_flight", hang_t0,
                                        (unsigned long long)q->in_flight.load());
            q->cv.wait_for(lock, std::chrono::milliseconds(500),
                           [&] { return q->in_flight.load() == 0; });
        }
    }
    if (hang_t0 >= 0)
        bs_wait_trace_hang_end("notify_queue:shutdown_wait", hang_t0);

    if (q->worker.joinable())
        q->worker.join();
    delete q;
}
