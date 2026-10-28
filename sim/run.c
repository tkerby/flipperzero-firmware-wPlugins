// Entry point for the host simulator. The scenario itself lives in sim/shim.c, next to the
// View/ViewDispatcher internals it has to drive.
#include <shim.h>

void sim_trace_begin(const char* path);
void sim_trace_end(void);
void sim_heap_report(void);
void sim_dump_files(void);

// Which app the driver starts. Defaults to this repo's; point it at another FAP with
//   SIM_ENTRY=my_app ./sim/build.sh
// A missing symbol then fails at link time, which is a clearer error than a dlsym miss.
#ifndef SIM_ENTRY
#define SIM_ENTRY rf_survey_app
#endif
extern int32_t SIM_ENTRY(void* p);

int main(int argc, char** argv) {
    const char* out = (argc > 1) ? argv[1] : "sim/out"; // output DIRECTORY

    sim_trace_begin(out);
    int32_t rc = SIM_ENTRY(NULL);
    sim_trace_end();
    sim_heap_report();
    sim_dump_files();
    fprintf(stderr, "[i] app returned %d\n", (int)rc);
    return rc == 0 ? 0 : 1;
}
