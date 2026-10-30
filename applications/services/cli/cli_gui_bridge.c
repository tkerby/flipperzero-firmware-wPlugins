#include "cli_gui_bridge.h"
#include "cli_main_shell.h"
#include <toolbox/api_lock.h>
#include <toolbox/cli/cli_registry_i.h>
#include <toolbox/cli/shell/cli_shell.h>
#include <toolbox/cli/shell/cli_shell_i.h>
#include <toolbox/pipe.h>
#include <string.h>

typedef struct {
    PipeSide* own_pipe;
    PipeSide* shell_pipe;
    CliRegistry* registry;
    CliShell* shell;
    FuriTimer* drain_timer;
    size_t output_capacity;
    size_t input_capacity;
} CliGuiSession;

static struct {
    FuriMutex* mutex;
    CliRegistry* main_registry;
    CliGuiSession* session;
    bool attached;
} cli_gui_state;

static CliRegistry* cli_gui_clone_registry(void) {
    CliRegistry* registry = cli_registry_alloc();
    cli_registry_lock(cli_gui_state.main_registry);
    cli_registry_lock(registry);
    CliCommandDict_t* source = cli_registry_get_commands(cli_gui_state.main_registry);
    CliCommandDict_t* target = cli_registry_get_commands(registry);
    for
        M_EACH(item, *source, CliCommandDict_t) {
            const char* name = furi_string_get_cstr(item->key);
            // Shell built-ins carry a shell context and are installed for this shell on start.
            if(!strcmp(name, "help") || !strcmp(name, "?") || !strcmp(name, "exit") ||
               !strcmp(name, "reload_ext_cmds"))
                continue;
            CliCommandDict_set_at(*target, item->key, item->value);
        }
    cli_registry_unlock(registry);
    cli_registry_unlock(cli_gui_state.main_registry);
    return registry;
}

static void cli_gui_start_shell(CliGuiSession* session) {
    PipeSideBundle pipes = pipe_alloc_ex(
        (PipeSideReceiveSettings){.capacity = session->output_capacity, .trigger_level = 1},
        (PipeSideReceiveSettings){.capacity = session->input_capacity, .trigger_level = 1});
    session->own_pipe = pipes.alices_side;
    session->shell_pipe = pipes.bobs_side;
    session->registry = cli_gui_clone_registry();
    session->shell = cli_shell_alloc(
        cli_main_motd, NULL, session->shell_pipe, session->registry, &cli_main_ext_config);
    cli_shell_set_gui_mode(session->shell);
    cli_shell_start(session->shell);
}

static void cli_gui_stop_shell(CliGuiSession* session) {
    // Broken-pipe handling interrupts cooperative commands and releases blocked pipe I/O.
    pipe_free(session->own_pipe);
    cli_shell_join(session->shell);
    cli_shell_free(session->shell);
    pipe_free(session->shell_pipe);
    cli_registry_free(session->registry);
}

static void cli_gui_discard_output(CliGuiSession* session) {
    uint8_t data[64];
    size_t remaining = pipe_bytes_available(session->own_pipe);
    while(remaining) {
        size_t count = MIN(remaining, sizeof(data));
        size_t received = pipe_receive(session->own_pipe, data, count);
        if(!received) break;
        remaining -= received;
    }
}

static void cli_gui_drain_background(void* context) {
    CliGuiSession* session = context;
    // Timer callbacks must not wait behind registry/file I/O while a GUI attaches.
    if(furi_mutex_acquire(cli_gui_state.mutex, 0) != FuriStatusOk) return;
    if(cli_gui_state.session == session && !cli_gui_state.attached)
        cli_gui_discard_output(session);
    furi_mutex_release(cli_gui_state.mutex);
}

typedef struct {
    CliGuiSession* session;
    FuriApiLock ready;
} CliGuiTimerInit;

static void cli_gui_create_drain_timer(void* context, uint32_t arg) {
    UNUSED(arg);
    CliGuiTimerInit* init = context;
    // Timers retain their creator's appid pointer. Use the resident timer service,
    // so this name remains valid after the GUI thread and its FAP are released.
    init->session->drain_timer =
        furi_timer_alloc(cli_gui_drain_background, FuriTimerTypePeriodic, init->session);
    api_lock_unlock(init->ready);
}

static bool cli_gui_open(size_t output_capacity, size_t input_capacity) {
    furi_check(output_capacity && input_capacity);
    furi_mutex_acquire(cli_gui_state.mutex, FuriWaitForever);
    if(cli_gui_state.attached) {
        furi_mutex_release(cli_gui_state.mutex);
        return false;
    }

    CliGuiSession* session = cli_gui_state.session;
    if(!session) {
        session = malloc(sizeof(CliGuiSession));
        *session = (CliGuiSession){
            .output_capacity = output_capacity,
            .input_capacity = input_capacity,
        };
        cli_gui_start_shell(session);
        CliGuiTimerInit timer_init = {.session = session, .ready = api_lock_alloc_locked()};
        furi_timer_pending_callback(cli_gui_create_drain_timer, &timer_init, 0);
        api_lock_wait_unlock_and_free(timer_init.ready);
        cli_gui_state.session = session;
        furi_timer_start(session->drain_timer, furi_ms_to_ticks(20));
    } else if(!cli_shell_is_running(session->shell)) {
        cli_gui_stop_shell(session);
        cli_gui_start_shell(session);
    }

    // As before, reattaching clears old output while keeping a live command/session.
    cli_gui_discard_output(session);
    cli_gui_state.attached = true;
    furi_mutex_release(cli_gui_state.mutex);
    return true;
}

static size_t cli_gui_available(void) {
    furi_mutex_acquire(cli_gui_state.mutex, FuriWaitForever);
    size_t size = cli_gui_state.attached ? pipe_bytes_available(cli_gui_state.session->own_pipe) :
                                           0;
    furi_mutex_release(cli_gui_state.mutex);
    return size;
}

static size_t cli_gui_receive(void* data, size_t size) {
    furi_mutex_acquire(cli_gui_state.mutex, FuriWaitForever);
    if(cli_gui_state.attached) {
        size = MIN(size, pipe_bytes_available(cli_gui_state.session->own_pipe));
        if(size) size = pipe_receive(cli_gui_state.session->own_pipe, data, size);
    } else {
        size = 0;
    }
    furi_mutex_release(cli_gui_state.mutex);
    return size;
}

static size_t cli_gui_send(const void* data, size_t size) {
    furi_mutex_acquire(cli_gui_state.mutex, FuriWaitForever);
    if(cli_gui_state.attached) {
        CliGuiSession* session = cli_gui_state.session;
        if(!cli_shell_is_running(session->shell)) {
            cli_gui_stop_shell(session);
            cli_gui_start_shell(session);
        }
        size = MIN(size, pipe_spaces_available(session->own_pipe));
        if(size) size = pipe_send(session->own_pipe, data, size);
    } else {
        size = 0;
    }
    furi_mutex_release(cli_gui_state.mutex);
    return size;
}

static void cli_gui_close(bool persist) {
    furi_mutex_acquire(cli_gui_state.mutex, FuriWaitForever);
    CliGuiSession* session = cli_gui_state.session;
    cli_gui_state.attached = false;
    if(persist || !session) {
        furi_mutex_release(cli_gui_state.mutex);
        return;
    }
    cli_gui_state.session = NULL;
    furi_mutex_release(cli_gui_state.mutex);

    // Freeing the timer synchronizes its callback before any session memory is released.
    furi_timer_free(session->drain_timer);
    const uint8_t etx[] = {0x03, 0x03, 0x03};
    size_t count = MIN(sizeof(etx), pipe_spaces_available(session->own_pipe));
    if(count) pipe_send(session->own_pipe, etx, count);
    cli_gui_stop_shell(session);
    free(session);
}

static const CliGuiBridge cli_gui_bridge = {
    .version = CLI_GUI_BRIDGE_API_VERSION,
    .open = cli_gui_open,
    .available = cli_gui_available,
    .receive = cli_gui_receive,
    .send = cli_gui_send,
    .close = cli_gui_close,
};

void cli_gui_bridge_init(CliRegistry* main_registry) {
    cli_gui_state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    cli_gui_state.main_registry = main_registry;
    furi_record_create(RECORD_CLI_GUI_BRIDGE, (void*)&cli_gui_bridge);
}
