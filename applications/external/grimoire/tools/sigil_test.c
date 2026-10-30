// Prints a sigil's strokes so they can be compared with the site's compose(); see tools/sigil_check.js.
#include "../sigil.h"
#include <stdio.h>
int main(int argc, char** argv) {
    static Sigil s;
    const char* wish = argc > 1 ? argv[1] : "this is my wish";
    if(!sigil_make(wish, &s)) {
        printf("no letters\n");
        return 1;
    }
    printf("%s\n%u strokes %u points\n", s.letters, s.stroke_count, s.point_count);
    for(unsigned i = 0; i < s.stroke_count; i++) {
        printf("%u:", s.strokes[i].count);
        for(unsigned k = 0; k < s.strokes[i].count; k++) {
            const SigilPoint* p = &s.points[s.strokes[i].first + k];
            printf(" %.4f,%.4f", p->x, p->y);
        }
        printf("\n");
    }
    return 0;
}
