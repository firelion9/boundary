#include <iostream>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <stack>
#include <algorithm>
#include <initializer_list>
#include <cassert>

enum Mode {
    IO,
    MEMORY,
    CPU,
};

static thread_local std::stack<Mode> mode_stack{{CPU}};

struct CpuSet {
    cpu_set_t *set;
    size_t count;

    CpuSet() : set(nullptr), count(0) {}

    explicit CpuSet(size_t count) : set(CPU_ALLOC(count)), count(count) {
        CPU_ZERO_S(alloc_size(), set);
    }

    CpuSet(CpuSet &&s) noexcept: set(s.set), count(s.count) {
        s.set = nullptr;
    }

    CpuSet &operator=(CpuSet &&s) noexcept {
        swap(s);
        return *this;
    }

    void swap(CpuSet &s) {
        std::swap(set, s.set);
        std::swap(count, s.count);
    }

    [[nodiscard]] size_t alloc_size() const {
        return set ? CPU_ALLOC_SIZE(count) : 0;
    }

    void add(size_t cpu) {
        CPU_SET_S(cpu, alloc_size(), set);
    }

    ~CpuSet() {
        if (set) {
            CPU_FREE(set);
        }
    }
};

template<class Range = std::initializer_list<size_t>>
static CpuSet cpu_set(Range &&r) {
    auto cpu_count = *std::max_element(r.begin(), r.end()) + 1;
    CpuSet res(cpu_count);
    for (auto Cpu: r) {
        res.add(Cpu);
    }
    return res;
}

static CpuSet io_cpu_set = cpu_set({0, 1, 2, 3});
static CpuSet memory_cpu_set = cpu_set({0, 1, 2, 3});
static CpuSet cpu_cpu_set = cpu_set({4, 5, 6, 7});

static void pin_to(pid_t pid, const CpuSet &set) {
    sched_setaffinity(pid, set.alloc_size(), set.set);
}

static void pin_self_to(const CpuSet &set) {
    pin_to(0 /* or getpid() */, set);
}

static void on_mode_changed(Mode prev, Mode cur) {
    if (prev == cur) return;

    switch (cur) {
        case IO:
            pin_self_to(io_cpu_set);
            break;
        case MEMORY:
            pin_self_to(memory_cpu_set);
            break;
        case CPU:
            pin_self_to(cpu_cpu_set);
            break;
    }
}

static void push_mode(Mode next) {
    auto prev = mode_stack.top();
    mode_stack.push(next);
    on_mode_changed(prev, next);
}

static void pop_mode(Mode expected) {
    auto prev = mode_stack.top();
    assert(prev == expected && "unexpected mode");
    mode_stack.pop();
    on_mode_changed(prev,  mode_stack.top());
}

extern "C" void io_enter() {
    push_mode(IO);
}

extern "C" void io_exit() {
    pop_mode(IO);
}

extern "C" void memory_enter() {
    push_mode(MEMORY);
}

extern "C" void memory_exit() {
    pop_mode(MEMORY);
}
