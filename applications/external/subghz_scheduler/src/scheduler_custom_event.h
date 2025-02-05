#pragma once

typedef enum {
    SchedulerStartEventSetInterval,
    SchedulerStartEventSetRepeats,
    SchedulerStartEventSetImmediate,
    SchedulerStartEventSetTxDelay,
    SchedulerStartEventSelectFile,
    SchedulerStartRunEvent,
    SchedulerCustomEventErrorBack,
} SchedulerCustomEvent;
