#include <stdint.h>
#include <stdbool.h>

typedef enum FileTxType {
    SchedulerFileTypePlaylist,
    SchedulerFileTypeSingle
} FileTxType;

typedef struct Scheduler Scheduler;

Scheduler* scheduler_alloc();
void scheduler_free(Scheduler* scheduler);
void scheduler_reset(Scheduler* scheduler);

bool scheduler_time_to_trigger(Scheduler* scheduler);

void scheduler_set_interval(Scheduler* scheduler, uint8_t interval);
void scheduler_set_tx_repeats(Scheduler* scheduler, uint8_t tx_repeats);
void scheduler_set_file_name(Scheduler* scheduler, const char* file_name);
void scheduler_set_file_type(Scheduler* scheduler, FileTxType file_type);

void scheduler_reset_previous_time(Scheduler* scheduler);
uint32_t scheduler_get_previous_time(Scheduler* scheduler);
uint8_t scheduler_get_interval(Scheduler* scheduler);
uint8_t scheduler_get_tx_repeats(Scheduler* scheduler);
const char* scheduler_get_file_name(Scheduler* scheduler);
FileTxType scheduler_get_file_type(Scheduler* scheduler);

void scheduler_set_immediate_mode(Scheduler* scheduler, bool mode);
bool scheduler_get_immediate_mode(Scheduler* scheduler);

void scheduler_get_countdown_fmt(Scheduler* scheduler, char* buffer, uint8_t size);
