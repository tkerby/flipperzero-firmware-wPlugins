"""Exercise the actual C sync callback with host storage/pipe stubs."""

from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "megaman.c").read_text()
callback = source[
    source.index("static void sync_command(") : source.index(
        "static void format_output("
    )
]
header = r"""
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define XP_PER_LEVEL 10
#define FuriWaitForever 0
#define UNUSED(x) (void)(x)
typedef struct { const char* text; } FuriString;
typedef struct { char response[128]; } PipeSide;
typedef struct { uint32_t total_xp; } Progress;
typedef struct { void* lock; Progress progress; bool save_ok; } App;
static bool storage_works=true;
static unsigned saves=0;
static const char* furi_string_get_cstr(FuriString* s) {return s->text;}
static void furi_mutex_acquire(void* lock,int timeout) {(void)lock;(void)timeout;}
static void furi_mutex_release(void* lock) {(void)lock;}
static size_t pipe_spaces_available(PipeSide* p) {(void)p;return 128;}
static void pipe_send(PipeSide* p,const void* data,size_t len) {memcpy(p->response,data,len);p->response[len]=0;}
static bool save_progress(App* a) {(void)a;saves++;return storage_works;}
"""
tests = r"""
int main(void) {
 App app={0};app.progress.total_xp=12;app.save_ok=true;
 PipeSide pipe={0};FuriString args={"25"};
 sync_command(&pipe,&args,&app);
 assert(app.progress.total_xp==25 && saves==1);
 assert(strcmp(pipe.response,"[MEGAMAN_SYNC] XP=25 LV=3 SAVE=1\r\n")==0);
 args.text="12";sync_command(&pipe,&args,&app);
 assert(app.progress.total_xp==25 && saves==1);
 args.text="28";sync_command(&pipe,&args,&app);
 assert(app.progress.total_xp==28 && strstr(pipe.response,"LV=3"));
 args.text="-1";sync_command(&pipe,&args,&app);
 assert(app.progress.total_xp==28 && strstr(pipe.response,"ERROR"));
 args.text="100000001";sync_command(&pipe,&args,&app);
 assert(app.progress.total_xp==28 && strstr(pipe.response,"ERROR"));
 storage_works=false;args.text="30";sync_command(&pipe,&args,&app);
 assert(app.progress.total_xp==30 && strstr(pipe.response,"SAVE=0"));
 storage_works=true;sync_command(&pipe,&args,&app);
 assert(strstr(pipe.response,"SAVE=1"));
 puts("Actual C sync callback checks passed.");
 return 0;
}
"""
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "sync.c"
    path.write_text(header + callback + tests)
    binary = Path(directory) / "sync"
    subprocess.run(
        ["cc", "-Wall", "-Wextra", "-Werror", str(path), "-o", str(binary)], check=True
    )
    subprocess.run([str(binary)], check=True)
