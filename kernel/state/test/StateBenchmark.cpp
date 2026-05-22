#include "bs/kernel/state/StateBus.h"
#include "bs/kernel/state/StateMachine.h"

#include <cstdio>
#include <ctime>

// State Machine benchmark
static void benchmark_StateMachineCreateDestroy(size_t N)
{
    clock_t start = clock();

    for (size_t i = 0; i < N; i++)
    {
        StateMachine* sm = StateMachine_Create("test");
        StateMachine_Destroy(sm);
    }

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    printf("benchmark_StateMachineCreateDestroy (N=%zu): %.3f ms\n", N, ms);
}

static void benchmark_StateMachineTransitions(size_t N)
{
    StateMachine* sm = StateMachine_Create("test");

    clock_t start = clock();

    for (size_t i = 0; i < N; i++)
    {
        StateMachine_Transition(sm, CONFIG_STATE_LOADING);
        StateMachine_Transition(sm, CONFIG_STATE_ACTIVE);
        StateMachine_Transition(sm, CONFIG_STATE_UPDATING);
        StateMachine_Transition(sm, CONFIG_STATE_ACTIVE);
    }

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    StateMachine_Destroy(sm);

    printf("benchmark_StateMachineTransitions (N=%zu cycles): %.3f ms\n", N, ms);
}

// State Bus benchmark
static void benchmark_StateBusCreateDestroy(size_t N)
{
    clock_t start = clock();

    for (size_t i = 0; i < N; i++)
    {
        StateBus* bus = StateBus_Create();
        StateBus_Destroy(bus);
    }

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    printf("benchmark_StateBusCreateDestroy (N=%zu): %.3f ms\n", N, ms);
}

static void benchmark_StateBusSetGet(size_t N)
{
    StateBus* bus = StateBus_Create();

    clock_t start = clock();

    for (size_t i = 0; i < N; i++)
    {
        StateBus_SetState(bus, "path", CONFIG_STATE_ACTIVE, nullptr, 0);

        StateEntry* entry = nullptr;
        StateBus_GetState(bus, "path", &entry);
    }

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    StateBus_Destroy(bus);

    printf("benchmark_StateBusSetGet (N=%zu): %.3f ms\n", N, ms);
}

int main()
{
    printf("=== State Performance Benchmarks ===\n");

    // State Machine
    benchmark_StateMachineCreateDestroy(10000);
    benchmark_StateMachineCreateDestroy(100000);
    benchmark_StateMachineTransitions(10000);

    // State Bus
    benchmark_StateBusCreateDestroy(10000);
    benchmark_StateBusCreateDestroy(100000);
    benchmark_StateBusSetGet(10000);
    benchmark_StateBusSetGet(100000);

    printf("=== State Benchmark Complete ===\n");
    return 0;
}
