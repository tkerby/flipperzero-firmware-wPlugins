#pragma once

typedef enum {
    SchedulerStartEventSetInterval,
    SchedulerStartEventSetRepeats,
    SchedulerStartEventSetImmediate,
    SchedulerStartEventSelectFile,
    SchedulerStartRunEvent,
    SchedulerCustomEventErrorBack,
} SchedulerCustomEvent;
