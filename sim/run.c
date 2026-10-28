// Entry point for the host simulator. The scenario itself lives in sim/shim.c, next to the
// View/ViewDispatcher internals it has to drive.
#include <shim.h>

void sim_trace_begin(const char* path);
void sim_trace_end(void);
void sim_heap_report(void);
void sim_dump_files(void);

extern int32_t rf_survey_app(void* p);

int main(int argc, char** argv) {
    const char* out = (argc > 1) ? argv[1] : "sim/out"; // output DIRECTORY
    sim_trace_begin(out);
    int32_t rc = rf_survey_app(NULL);
    sim_trace_end();
    sim_heap_report();
    sim_dump_files();
    fprintf(stderr, "[i] app returned %d\n", (int)rc);
    return rc == 0 ? 0 : 1;
}
