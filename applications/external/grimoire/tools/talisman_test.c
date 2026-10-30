// Every square is checked: rows, columns, both diagonals, and 1..n*n once each; the names sum as Agrippa says.
#include "../talisman.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    int fails = 0;
    for(int p = 0; p < PlanetCount; p++) {
        uint8_t n;
        const uint8_t* sq = talisman_square((Planet)p, &n);
        int want = talisman_row_sum((Planet)p), d1 = 0, d2 = 0, seen[82] = {0}, ok = 1;
        for(int i = 0; i < n; i++) {
            int row = 0, col = 0;
            for(int j = 0; j < n; j++) {
                row += sq[i * n + j];
                col += sq[j * n + i];
                seen[sq[i * n + j]]++;
            }
            if(row != want || col != want) ok = 0;
            d1 += sq[i * n + i];
            d2 += sq[i * n + n - 1 - i];
        }
        if(d1 != want || d2 != want) ok = 0;
        for(int v = 1; v <= n * n; v++)
            if(seen[v] != 1) ok = 0;
        const TalismanName* in = talisman_intelligence((Planet)p);
        const TalismanName* sp = talisman_spirit((Planet)p);
        int isum = in ? talisman_sum(in->values, in->count) : -1,
            ssum = talisman_sum(sp->values, sp->count);
        // each name sums to one of the square's numbers: a row, the whole, or the side squared
        int tot = talisman_total((Planet)p);
        int iok = !in || isum == want || isum == tot || isum == n * n;
        int sok = ssum == want || ssum == tot || ssum == n * n;
        printf(
            "%s %-8s %dx%d rows %3d all %4d  %s %s=%d %s  %s=%d %s\n",
            ok && iok && sok ? "ok  " : "FAIL",
            planet_names[p],
            n,
            n,
            want,
            talisman_total((Planet)p),
            ok ? "magic" : "BROKEN",
            in ? in->name : "-",
            isum,
            iok ? "" : "(?)",
            sp->name,
            ssum,
            sok ? "" : "(?)");
        if(!(ok && iok && sok)) fails++;
    }
    uint16_t v[16];
    size_t k = talisman_latin("Rick Seufert", v, 16);
    printf("Rick Seufert ->");
    for(size_t i = 0; i < k; i++)
        printf(" %u", v[i]);
    printf(" = %u\n", talisman_sum(v, k));
    uint8_t c, r;
    talisman_cell(PlanetMars, 400, &c, &r);
    printf("400 on Mars reads as 4 -> col %u row %u (want 0,1)\n", c, r);
    if(c != 0 || r != 1) fails++;
    printf("%s\n", fails ? "SOME CHECKS FAILED" : "all checks passed");
    return fails != 0;
}
