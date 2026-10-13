/* Host driver for the route code: route_test <root> <METHOD> <path> <query>
 * Runs one request through hub_git_route / hub_gitview_route / hub_gitext_route (query build) with
 * the host stand-ins of furi_stub.c and writes the raw HTTP answer to stdout. HUB_ROOT is <root>
 * ("/ext/apps_data/gitphin/git/x.git" is <root>/git/x.git). */
#include "../../flipper_app/src/http/hub_req.h"
#include "../../flipper_app/src/git/hub_git.h"
#include "../../flipper_app/src/ext/hub_gitext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t* out_buf;
extern size_t out_len;

extern size_t cur_b, peak_b;

int main(int argc, char** argv) {
    if(argc < 5) return 2;
    setenv("HUB_ROOT", argv[1], 1);
    HubReq* r = calloc(1, sizeof(HubReq));
    r->t = (HubTransport*)r;
    r->storage = (Storage*)r;
    r->method = argv[2];
    r->path = argv[3];
    r->query = argv[4];
    size_t base_b = cur_b;
    peak_b = cur_b;
    bool done = hub_gitext_route(r) || hub_gitview_route(r) || hub_git_route(r);
    if(!done) fprintf(stderr, "no route\n");
    fprintf(stderr, "heap_peak=%zu\n", peak_b - base_b);
    fwrite(out_buf, 1, out_len, stdout);
    return done ? 0 : 3;
}
