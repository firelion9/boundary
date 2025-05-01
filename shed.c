#define _GNU_SOURCE

#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <malloc.h>
#include <threads.h>
#include <stdarg.h>

#define max(a, b)               \
   ({ __typeof__ (a) _a = (a);  \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

enum Mode {
    IO,
    MEMORY,
    CPU,
};

struct ModeStack {
    enum Mode *data;
    size_t size;
    size_t capacity;
};

void grow(struct ModeStack *stack, size_t min_size) {
    enum Mode *old_data = stack->data;
    size_t new_size = max(min_size, stack->size * 2);
    enum Mode *new_data = malloc(sizeof(enum Mode) * new_size);
    stack->data = new_data;
    stack->capacity = new_size;
    free(old_data);
}

void push(struct ModeStack *stack, enum Mode m) {
    if (stack->size == stack->capacity) {
        grow(stack, stack->size + 1);
    }
}

void pop(struct ModeStack *stack) {
    assert(stack->size > 0 && "pop from empty stack");
    stack->size -= 1;
}

enum Mode top(struct ModeStack *stack) {
    assert(stack->size > 0 && "top of empty stack");
    return stack->data[stack->size - 1];
}

_Thread_local static struct ModeStack mode_stack = {NULL, 0, 0};

struct CpuSet {
    cpu_set_t *set;
    size_t count;
};

size_t cpu_set_size(struct CpuSet set) {
    return CPU_ALLOC_SIZE(set.count);
}

struct CpuSet cpu_set_alloc(size_t count) {
    struct CpuSet res = {CPU_ALLOC(count), count};
    CPU_ZERO_S(cpu_set_size(res), res.set);
    return res;
}

void cpu_set_add(struct CpuSet set, size_t x) {
    CPU_SET_S(x, cpu_set_size(set), set.set);
}

void cpu_set_add_all(struct CpuSet set, int n, ...) {
    va_list args;
    va_start(args, n);
    for (int i = 0; i < n; i++)
        cpu_set_add(set, va_arg(args, int));
    va_end(args);
}

#define DECLARE_THREAD_LOCAL_CPU_SET(name, max_cpu, n_cpus, ...)       \
    static struct CpuSet name() {                                      \
        _Thread_local static struct CpuSet *store = NULL;              \
        struct CpuSet *val = store;                                    \
        if (val == NULL) {                                             \
            val = malloc(sizeof(struct CpuSet));                       \
            *val = cpu_set_alloc(max_cpu + 1);                         \
            cpu_set_add_all(*val, n_cpus, __VA_ARGS__);                \
            if (!__sync_val_compare_and_swap(&store, NULL, val)) {     \
                free(val);                                             \
                return *store;                                         \
            } else {                                                   \
                return *val;                                           \
            }                                                          \
        }                                                              \
        return *val;                                                   \
    }

DECLARE_THREAD_LOCAL_CPU_SET(io_cpu_set, 8, 4, 0, 1, 2, 3)
DECLARE_THREAD_LOCAL_CPU_SET(memory_cpu_set, 8, 4, 0, 1, 2, 3)
DECLARE_THREAD_LOCAL_CPU_SET(cpu_cpu_set, 8, 4, 4, 5, 6, 7)

static void pin_to(pid_t pid, const struct CpuSet set) {
    sched_setaffinity(pid, cpu_set_size(set), set.set);
}

static void pin_self_to(const struct CpuSet set) {
    pin_to(0 /* or getpid() */, set);
}

static void on_mode_changed(enum Mode prev, enum Mode cur) {
    if (prev == cur) return;

    switch (cur) {
        case IO:
            pin_self_to(io_cpu_set());
            break;
        case MEMORY:
            pin_self_to(memory_cpu_set());
            break;
        case CPU:
            pin_self_to(cpu_cpu_set());
            break;
    }
}

static void push_mode(enum Mode next) {
    enum Mode prev = top(&mode_stack);
    push(&mode_stack, next);
    on_mode_changed(prev, next);
}

static void pop_mode(enum Mode expected) {
    enum Mode prev = top(&mode_stack);
    assert(prev == expected && "unexpected mode");
    pop(&mode_stack);
    on_mode_changed(prev, top(&mode_stack));
}

void io_enter() {
    push_mode(IO);
}

void io_exit() {
    pop_mode(IO);
}

void memory_enter() {
    push_mode(MEMORY);
}

void memory_exit() {
    pop_mode(MEMORY);
}

void cpu_enter() {
    push_mode(CPU);
}

void cpu_exit() {
    pop_mode(CPU);
}
