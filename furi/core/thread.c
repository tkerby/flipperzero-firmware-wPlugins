#include "thread.h"
#include "thread_i.h"
#include "timer.h"
#include "thread_list.h"
#include "kernel.h"
#include "memmgr.h"
#include "memmgr_heap.h"
#include "check.h"
#include "common_defines.h"
#include "string.h"

#include <timers.h>
#include "log.h"
#include <furi_hal_rtc.h>

#include <FreeRTOS.h>
#include <stdint.h>
#include <task.h>

#include <task_control_block.h>

#define TAG "FuriThread"

#define THREAD_NOTIFY_INDEX (1) // Index 0 is used for stream buffers

#define THREAD_MAX_STACK_SIZE (UINT16_MAX * sizeof(StackType_t))

#define THREAD_STACK_WATERMARK_MIN (256u)

static size_t __furi_thread_stdout_write(FuriThread* thread, const char* data, size_t size);
static int32_t __furi_thread_stdout_flush(FuriThread* thread);

/** Catch threads that are trying to exit wrong way */
__attribute__((__noreturn__)) void furi_thread_catch(void) { //-V1082
    // If you're here it means you're probably doing something wrong
    // with critical sections or with scheduler state
    asm volatile("nop"); // extra magic
    furi_crash("You are doing it wrong"); //-V779
    __builtin_unreachable();
}

static void furi_thread_set_state(FuriThread* thread, FuriThreadState state) {
    furi_assert(thread);
    thread->state = state;
    if(thread->state_callback) {
        thread->state_callback(state, thread->state_context);
    }
}

static void furi_thread_body(void* context) {
    furi_check(context);
    FuriThread* thread = context;

    // store thread instance to thread local storage
    furi_check(pvTaskGetThreadLocalStoragePointer(NULL, 0) == NULL);
    vTaskSetThreadLocalStoragePointer(NULL, 0, thread);

    furi_check(thread->state == FuriThreadStateStarting);
    furi_thread_set_state(thread, FuriThreadStateRunning);

    if(thread->heap_trace_enabled == true) {
        memmgr_heap_enable_thread_trace(thread);
    }

    thread->ret = thread->callback(thread->context);

    furi_check(!thread->is_service, "Service threads MUST NOT return");

    size_t stack_watermark = furi_thread_get_stack_space(thread);
    if(stack_watermark < THREAD_STACK_WATERMARK_MIN) {
#ifdef FURI_DEBUG
        furi_crash("Stack watermark is dangerously low");
#endif
        FURI_LOG_E( //-V779
            thread->name ? thread->name : "Thread",
            "Stack watermark is too low %zu < " STRINGIFY(
                THREAD_STACK_WATERMARK_MIN) ". Increase stack size.",
            stack_watermark);
    }

    if(thread->heap_trace_enabled == true) {
        furi_delay_ms(33);
        thread->heap_size = memmgr_heap_get_thread_memory(thread);
        furi_log_print_format(
            thread->heap_size ? FuriLogLevelError : FuriLogLevelInfo,
            TAG,
            "%s allocation balance: %zu",
            thread->name ? thread->name : "Thread",
            thread->heap_size);
        memmgr_heap_disable_thread_trace(thread);
    }

    furi_check(thread->state == FuriThreadStateRunning);

    // flush stdout
    __furi_thread_stdout_flush(thread);

    furi_thread_set_state(thread, FuriThreadStateStopped);

    vTaskDelete(NULL);
    furi_thread_catch();
}

static void furi_thread_init_common(FuriThread* thread) {
    thread->output.buffer = furi_string_alloc();

    FuriThread* parent = NULL;
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        // TLS is not available, if we called not from thread context
        parent = pvTaskGetThreadLocalStoragePointer(NULL, 0);

        if(parent && parent->appid) {
            furi_thread_set_appid(thread, parent->appid);
        } else {
            furi_thread_set_appid(thread, "unknown");
        }
    } else {
        // if scheduler is not started, we are starting driver thread
        furi_thread_set_appid(thread, "driver");
    }

    FuriHalRtcHeapTrackMode mode = furi_hal_rtc_get_heap_track_mode();
    if(mode == FuriHalRtcHeapTrackModeAll) {
        thread->heap_trace_enabled = true;
    } else if(mode == FuriHalRtcHeapTrackModeTree && furi_thread_get_current_id()) {
        if(parent) thread->heap_trace_enabled = parent->heap_trace_enabled;
    } else {
        thread->heap_trace_enabled = false;
    }
}

FuriThread* furi_thread_alloc(void) {
    FuriThread* thread = malloc(sizeof(FuriThread));

    furi_thread_init_common(thread);

    return thread;
}

FuriThread* furi_thread_alloc_service(
    const char* name,
    uint32_t stack_size,
    FuriThreadCallback callback,
    void* context) {
    FuriThread* thread = memmgr_alloc_from_pool(sizeof(FuriThread));

    furi_thread_init_common(thread);

    thread->stack_buffer = memmgr_alloc_from_pool(stack_size);
    thread->stack_size = stack_size;
    thread->is_service = true;

    furi_thread_set_name(thread, name);
    furi_thread_set_callback(thread, callback);
    furi_thread_set_context(thread, context);

    return thread;
}

FuriThread* furi_thread_alloc_ex(
    const char* name,
    uint32_t stack_size,
    FuriThreadCallback callback,
    void* context) {
    FuriThread* thread = furi_thread_alloc();
    furi_thread_set_name(thread, name);
    furi_thread_set_stack_size(thread, stack_size);
    furi_thread_set_callback(thread, callback);
    furi_thread_set_context(thread, context);
    return thread;
}

void furi_thread_free(FuriThread* thread) {
    furi_check(thread);
    // Cannot free a service thread
    furi_check(thread->is_service == false);
    // Cannot free a non-joined thread
    furi_check(thread->state == FuriThreadStateStopped);
    furi_check(!thread->is_active);

    furi_thread_set_name(thread, NULL);
    furi_thread_set_appid(thread, NULL);

    if(thread->stack_buffer) {
        free(thread->stack_buffer);
    }

    furi_string_free(thread->output.buffer);
    free(thread);
}

void furi_thread_set_name(FuriThread* thread, const char* name) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);

    if(thread->name) {
        free(thread->name);
    }

    thread->name = name ? strdup(name) : NULL;
}

void furi_thread_set_appid(FuriThread* thread, const char* appid) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);

    if(thread->appid) {
        free(thread->appid);
    }

    thread->appid = appid ? strdup(appid) : NULL;
}

void furi_thread_set_stack_size(FuriThread* thread, size_t stack_size) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);
    furi_check(stack_size);
    furi_check(stack_size <= THREAD_MAX_STACK_SIZE);
    furi_check(stack_size % sizeof(StackType_t) == 0);
    // Stack size cannot be configured for a thread that has been marked as a service
    furi_check(thread->is_service == false);

    if(thread->stack_buffer) {
        free(thread->stack_buffer);
    }

    thread->stack_buffer = malloc(stack_size);
    thread->stack_size = stack_size;
}

void furi_thread_set_callback(FuriThread* thread, FuriThreadCallback callback) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);
    thread->callback = callback;
}

void furi_thread_set_context(FuriThread* thread, void* context) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);
    thread->context = context;
}

void furi_thread_set_priority(FuriThread* thread, FuriThreadPriority priority) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);
    furi_check(priority >= FuriThreadPriorityIdle && priority <= FuriThreadPriorityIsr);
    thread->priority = priority;
}

FuriThreadPriority furi_thread_get_priority(FuriThread* thread) {
    furi_check(thread);
    TaskHandle_t hTask = furi_thread_get_id(thread);
    return (FuriThreadPriority)uxTaskPriorityGet(hTask);
}

void furi_thread_set_current_priority(FuriThreadPriority priority) {
    furi_check(priority <= FuriThreadPriorityIsr);

    UBaseType_t new_priority = priority ? priority : FuriThreadPriorityNormal;
    vTaskPrioritySet(NULL, new_priority);
}

FuriThreadPriority furi_thread_get_current_priority(void) {
    return (FuriThreadPriority)uxTaskPriorityGet(NULL);
}

void furi_thread_set_state_callback(FuriThread* thread, FuriThreadStateCallback callback) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);
    thread->state_callback = callback;
}

void furi_thread_set_state_context(FuriThread* thread, void* context) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);
    thread->state_context = context;
}

FuriThreadState furi_thread_get_state(FuriThread* thread) {
    furi_check(thread);
    return thread->state;
}

void furi_thread_set_signal_callback(
    FuriThread* thread,
    FuriThreadSignalCallback callback,
    void* context) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped || thread == furi_thread_get_current());

    thread->signal_callback = callback;
    thread->signal_context = context;
}

bool furi_thread_signal(const FuriThread* thread, uint32_t signal, void* arg) {
    furi_check(thread);

    bool is_consumed = false;

    if(thread->signal_callback) {
        is_consumed = thread->signal_callback(signal, arg, thread->signal_context);
    }

    return is_consumed;
}

void furi_thread_start(FuriThread* thread) {
    furi_check(thread);
    furi_check(thread->callback);
    furi_check(thread->state == FuriThreadStateStopped);
    furi_check(thread->stack_size > 0);

    furi_thread_set_state(thread, FuriThreadStateStarting);

    uint32_t stack_depth = thread->stack_size / sizeof(StackType_t);
    UBaseType_t priority = thread->priority ? thread->priority : FuriThreadPriorityNormal;

    thread->is_active = true;

    furi_check(
        xTaskCreateStatic(
            furi_thread_body,
            thread->name,
            stack_depth,
            thread,
            priority,
            thread->stack_buffer,
            &thread->container) == (TaskHandle_t)thread);
}

void furi_thread_cleanup_tcb_event(TaskHandle_t task) {
    FuriThread* thread = pvTaskGetThreadLocalStoragePointer(task, 0);
    if(thread) {
        // clear thread local storage
        vTaskSetThreadLocalStoragePointer(task, 0, NULL);
        furi_check(thread == (FuriThread*)task);
        thread->is_active = false;
    }
}

bool furi_thread_join(FuriThread* thread) {
    furi_check(thread);
    // Cannot join a service thread
    furi_check(!thread->is_service);
    // Cannot join a thread to itself
    furi_check(furi_thread_get_current() != thread);

    // !!! IMPORTANT NOTICE !!!
    //
    // If your thread exited, but your app stuck here: some other thread uses
    // all cpu time, which delays kernel from releasing task handle
    while(thread->is_active) {
        furi_delay_ms(10);
    }

    return true;
}

FuriThreadId furi_thread_get_id(FuriThread* thread) {
    furi_check(thread);
    return thread;
}

void furi_thread_enable_heap_trace(FuriThread* thread) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);
    thread->heap_trace_enabled = true;
}

void furi_thread_disable_heap_trace(FuriThread* thread) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);
    thread->heap_trace_enabled = false;
}

size_t furi_thread_get_heap_size(FuriThread* thread) {
    furi_check(thread);
    furi_check(thread->heap_trace_enabled == true);
    return thread->heap_size;
}

int32_t furi_thread_get_return_code(FuriThread* thread) {
    furi_check(thread);
    furi_check(thread->state == FuriThreadStateStopped);
    return thread->ret;
}

FuriThreadId furi_thread_get_current_id(void) {
    return xTaskGetCurrentTaskHandle();
}

FuriThread* furi_thread_get_current(void) {
    FuriThread* thread = pvTaskGetThreadLocalStoragePointer(NULL, 0);
    return thread;
}

void furi_thread_yield(void) {
    furi_check(!FURI_IS_IRQ_MODE());
    taskYIELD();
}

/* Limits */
#define MAX_BITS_TASK_NOTIFY  31U
#define MAX_BITS_EVENT_GROUPS 24U

#define THREAD_FLAGS_INVALID_BITS (~((1UL << MAX_BITS_TASK_NOTIFY) - 1U))
#define EVENT_FLAGS_INVALID_BITS  (~((1UL << MAX_BITS_EVENT_GROUPS) - 1U))

uint32_t furi_thread_flags_set(FuriThreadId thread_id, uint32_t flags) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    uint32_t rflags;
    BaseType_t yield;

    if((hTask == NULL) || ((flags & THREAD_FLAGS_INVALID_BITS) != 0U)) {
        rflags = (uint32_t)FuriStatusErrorParameter;
    } else {
        rflags = (uint32_t)FuriStatusError;

        if(FURI_IS_IRQ_MODE()) {
            yield = pdFALSE;

            (void)xTaskNotifyIndexedFromISR(hTask, THREAD_NOTIFY_INDEX, flags, eSetBits, &yield);
            (void)xTaskNotifyAndQueryIndexedFromISR(
                hTask, THREAD_NOTIFY_INDEX, 0, eNoAction, &rflags, NULL);

            portYIELD_FROM_ISR(yield);
        } else {
            (void)xTaskNotifyIndexed(hTask, THREAD_NOTIFY_INDEX, flags, eSetBits);
            (void)xTaskNotifyAndQueryIndexed(hTask, THREAD_NOTIFY_INDEX, 0, eNoAction, &rflags);
        }
    }
    /* Return flags after setting */
    return rflags;
}

uint32_t furi_thread_flags_clear(uint32_t flags) {
    TaskHandle_t hTask;
    uint32_t rflags, cflags;

    if(FURI_IS_IRQ_MODE()) {
        rflags = (uint32_t)FuriStatusErrorISR;
    } else if((flags & THREAD_FLAGS_INVALID_BITS) != 0U) {
        rflags = (uint32_t)FuriStatusErrorParameter;
    } else {
        hTask = xTaskGetCurrentTaskHandle();

        if(xTaskNotifyAndQueryIndexed(hTask, THREAD_NOTIFY_INDEX, 0, eNoAction, &cflags) ==
           pdPASS) {
            rflags = cflags;
            cflags &= ~flags;

            if(xTaskNotifyIndexed(hTask, THREAD_NOTIFY_INDEX, cflags, eSetValueWithOverwrite) !=
               pdPASS) {
                rflags = (uint32_t)FuriStatusError;
            }
        } else {
            rflags = (uint32_t)FuriStatusError;
        }
    }

    /* Return flags before clearing */
    return rflags;
}

uint32_t furi_thread_flags_get(void) {
    TaskHandle_t hTask;
    uint32_t rflags;

    if(FURI_IS_IRQ_MODE()) {
        rflags = (uint32_t)FuriStatusErrorISR;
    } else {
        hTask = xTaskGetCurrentTaskHandle();

        if(xTaskNotifyAndQueryIndexed(hTask, THREAD_NOTIFY_INDEX, 0, eNoAction, &rflags) !=
           pdPASS) {
            rflags = (uint32_t)FuriStatusError;
        }
    }

    return rflags;
}

uint32_t furi_thread_flags_wait(uint32_t flags, uint32_t options, uint32_t timeout) {
    uint32_t rflags, nval;
    uint32_t clear;
    TickType_t t0, td, tout;
    BaseType_t rval;

    if(FURI_IS_IRQ_MODE()) {
        rflags = (uint32_t)FuriStatusErrorISR;
    } else if((flags & THREAD_FLAGS_INVALID_BITS) != 0U) {
        rflags = (uint32_t)FuriStatusErrorParameter;
    } else {
        if((options & FuriFlagNoClear) == FuriFlagNoClear) {
            clear = 0U;
        } else {
            clear = flags;
        }

        rflags = 0U;
        tout = timeout;

        t0 = xTaskGetTickCount();
        do {
            rval = xTaskNotifyWaitIndexed(THREAD_NOTIFY_INDEX, 0, clear, &nval, tout);

            if(rval == pdPASS) {
                rflags &= flags;
                rflags |= nval;

                if((options & FuriFlagWaitAll) == FuriFlagWaitAll) {
                    if((flags & rflags) == flags) {
                        break;
                    } else {
                        if(timeout == 0U) {
                            rflags = (uint32_t)FuriStatusErrorResource;
                            break;
                        }
                    }
                } else {
                    if((flags & rflags) != 0) {
                        break;
                    } else {
                        if(timeout == 0U) {
                            rflags = (uint32_t)FuriStatusErrorResource;
                            break;
                        }
                    }
                }

                /* Update timeout */
                td = xTaskGetTickCount() - t0;

                if(td > tout) {
                    tout = 0;
                } else {
                    tout -= td;
                }
            } else {
                if(timeout == 0) {
                    rflags = (uint32_t)FuriStatusErrorResource;
                } else {
                    rflags = (uint32_t)FuriStatusErrorTimeout;
                }
            }
        } while(rval != pdFAIL);
    }

    /* Return flags before clearing */
    return rflags;
}

static const char* furi_thread_state_name(eTaskState state) {
    switch(state) {
    case eRunning:
        return "Running";
    case eReady:
        return "Ready";
    case eBlocked:
        return "Blocked";
    case eSuspended:
        return "Suspended";
    case eDeleted:
        return "Deleted";
    case eInvalid:
        return "Invalid";
    default:
        return "?";
    }
}

bool furi_thread_enumerate(FuriThreadList* thread_list) {
    furi_check(thread_list);
    furi_check(!FURI_IS_IRQ_MODE());

    bool result = false;

    vTaskSuspendAll();
    do {
        uint32_t tick = furi_get_tick();
        uint32_t count = uxTaskGetNumberOfTasks();

        TaskStatus_t* task = pvPortMalloc(count * sizeof(TaskStatus_t));

        if(!task) break;

        configRUN_TIME_COUNTER_TYPE total_run_time;
        count = uxTaskGetSystemState(task, count, &total_run_time);
        for(uint32_t i = 0U; i < count; i++) {
            TaskControlBlock* tcb = (TaskControlBlock*)task[i].xHandle;

            FuriThreadListItem* item =
                furi_thread_list_get_or_insert(thread_list, (FuriThread*)task[i].xHandle);

            item->thread = (FuriThreadId)task[i].xHandle;
            item->app_id = furi_thread_get_appid(item->thread);
            item->name = task[i].pcTaskName;
            item->priority = task[i].uxCurrentPriority;
            item->stack_address = (uint32_t)tcb->pxStack;
            size_t thread_heap = memmgr_heap_get_thread_memory(item->thread);
            item->heap = thread_heap == MEMMGR_HEAP_UNKNOWN ? 0u : thread_heap;
            item->stack_size = (tcb->pxEndOfStack - tcb->pxStack + 1) * sizeof(StackType_t);
            item->stack_min_free = furi_thread_get_stack_space(item->thread);
            item->state = furi_thread_state_name(task[i].eCurrentState);
            item->counter_previous = item->counter_current;
            item->counter_current = task[i].ulRunTimeCounter;
            item->tick = tick;
        }

        vPortFree(task);
        furi_thread_list_process(thread_list, total_run_time, tick);

        result = true;
    } while(false);
    (void)xTaskResumeAll();

    return result;
}

const char* furi_thread_get_name(FuriThreadId thread_id) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    const char* name;

    if(FURI_IS_IRQ_MODE() || (hTask == NULL)) {
        name = NULL;
    } else {
        name = pcTaskGetName(hTask);
    }

    return name;
}

const char* furi_thread_get_appid(FuriThreadId thread_id) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    const char* appid = "system";

    if(!FURI_IS_IRQ_MODE() && (hTask != NULL)) {
        FuriThread* thread = (FuriThread*)pvTaskGetThreadLocalStoragePointer(hTask, 0);
        if(thread) {
            appid = thread->appid;
        } else if(hTask == xTimerGetTimerDaemonTaskHandle()) {
            const char* timer = furi_timer_get_current_name();
            if(timer) {
                appid = timer;
            }
        }
    }

    return appid;
}

uint32_t furi_thread_get_stack_space(FuriThreadId thread_id) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    uint32_t sz;

    if(FURI_IS_IRQ_MODE() || (hTask == NULL)) {
        sz = 0U;
    } else {
        sz = (uint32_t)(uxTaskGetStackHighWaterMark(hTask) * sizeof(StackType_t));
    }

    return sz;
}

static size_t __furi_thread_stdout_write(FuriThread* thread, const char* data, size_t size) {
    if(thread->output.write_callback != NULL) {
        thread->output.write_callback(data, size);
    } else {
        furi_log_tx((const uint8_t*)data, size);
    }
    return size;
}

static int32_t __furi_thread_stdout_flush(FuriThread* thread) {
    FuriString* buffer = thread->output.buffer;
    size_t size = furi_string_size(buffer);
    if(size > 0) {
        __furi_thread_stdout_write(thread, furi_string_get_cstr(buffer), size);
        furi_string_reset(buffer);
    }
    return 0;
}

void furi_thread_set_stdout_callback(FuriThreadStdoutWriteCallback callback) {
    FuriThread* thread = furi_thread_get_current();
    furi_check(thread);
    __furi_thread_stdout_flush(thread);
    thread->output.write_callback = callback;
}

FuriThreadStdoutWriteCallback furi_thread_get_stdout_callback(void) {
    FuriThread* thread = furi_thread_get_current();
    furi_check(thread);
    return thread->output.write_callback;
}

size_t furi_thread_stdout_write(const char* data, size_t size) {
    FuriThread* thread = furi_thread_get_current();
    furi_check(thread);

    if(size == 0 || data == NULL) {
        return __furi_thread_stdout_flush(thread);
    } else {
        if(data[size - 1] == '\n') {
            // if the last character is a newline, we can flush buffer and write data as is, wo buffers
            __furi_thread_stdout_flush(thread);
            __furi_thread_stdout_write(thread, data, size);
        } else {
            // string_cat doesn't work here because we need to write the exact size data
            for(size_t i = 0; i < size; i++) {
                furi_string_push_back(thread->output.buffer, data[i]);
                if(data[i] == '\n') {
                    __furi_thread_stdout_flush(thread);
                }
            }
        }
    }

    return size;
}

int32_t furi_thread_stdout_flush(void) {
    FuriThread* thread = furi_thread_get_current();
    furi_check(thread);

    return __furi_thread_stdout_flush(thread);
}

void furi_thread_suspend(FuriThreadId thread_id) {
    furi_check(thread_id);

    TaskHandle_t hTask = (TaskHandle_t)thread_id;

    vTaskSuspend(hTask);
}

void furi_thread_resume(FuriThreadId thread_id) {
    furi_check(thread_id);

    TaskHandle_t hTask = (TaskHandle_t)thread_id;

    if(FURI_IS_IRQ_MODE()) {
        xTaskResumeFromISR(hTask);
    } else {
        vTaskResume(hTask);
    }
}

bool furi_thread_is_suspended(FuriThreadId thread_id) {
    furi_check(thread_id);

    TaskHandle_t hTask = (TaskHandle_t)thread_id;

    return eTaskGetState(hTask) == eSuspended;
}
