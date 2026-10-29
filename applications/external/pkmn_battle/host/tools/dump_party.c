/*
 * dump_party OUTDIR [seed [level_min level_max]]
 *
 * Generates a team, writes the party block and patch list we would send
 * (OUTDIR/party_block.bin, OUTDIR/patch.bin) and a canonical text dump of the
 * team taken from the PkbTeam fields (OUTDIR/expected.txt), for
 * tools/gen/decode_party.py --expect to cross-check.
 */
#include <stdio.h>
#include <stdlib.h>
#include <pkb/party_block.h>
#include "../tests/host_io.h"

static bool write_file(const char* dir, const char* name, const void* data, size_t len) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE* f = fopen(path, "wb");
    if(!f) return false;
    bool ok = fwrite(data, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

int main(int argc, char** argv) {
    if(argc < 2) {
        fprintf(stderr, "usage: dump_party OUTDIR [seed [level_min level_max]]\n");
        return 2;
    }
    uint64_t seed = argc > 2 ? strtoull(argv[2], NULL, 0) : 7;
    uint8_t lo = argc > 3 ? (uint8_t)atoi(argv[3]) : 50,
            hi = argc > 4 ? (uint8_t)atoi(argv[4]) : lo;

    HostBlob storage = {0};
    PkbBlobReader pools = host_movepools(&storage);
    PkbPolicyRng rng;
    PkbTeam team;
    pkb_policy_rng_seed(&rng, seed);
    if(pkb_team_generate(&team, PKB_PARTY_LENGTH, lo, hi, &pools, NULL, NULL, &rng) != PkbTeamOk) {
        fprintf(stderr, "team generation failed\n");
        return 1;
    }
    team.trainer_id = 0xFEFE; /* exercise the patch list */

    uint8_t block[PKB_PARTY_BLOCK_LEN], patch[PKB_PATCH_BLOCK_LEN];
    if(!pkb_party_block_build(&team, team.name, block, patch)) return 1;

    char text[8192];
    int n = snprintf(
        text,
        sizeof(text),
        "trainer %s\nid %u\ncount %u\n",
        team.name,
        team.trainer_id,
        team.count);
    for(int i = 0; i < team.count; i++) {
        const PkbTeamMon* m = &team.mon[i];
        char nick[16];
        pkb_species_name(m->species, nick, sizeof(nick));
        n += snprintf(
            text + n,
            sizeof(text) - n,
            "mon %d species %u listed %u nick %s ot %s otid %u level %u exp %lu item %u"
            " hp %u maxhp %u atk %u def %u spd %u sat %u sdf %u"
            " moves %u %u %u %u pp %u %u %u %u dvs %u"
            " statexp %u %u %u %u %u happiness %u status 0\n",
            i + 1,
            m->species,
            m->species,
            nick,
            team.name,
            team.trainer_id,
            m->level,
            (unsigned long)m->exp,
            m->item,
            m->max_hp,
            m->max_hp,
            m->stats[0],
            m->stats[1],
            m->stats[2],
            m->stats[3],
            m->stats[4],
            m->moves[0],
            m->moves[1],
            m->moves[2],
            m->moves[3],
            m->pp[0],
            m->pp[1],
            m->pp[2],
            m->pp[3],
            (unsigned)(m->dvs[0] << 8 | m->dvs[1]),
            m->stat_exp,
            m->stat_exp,
            m->stat_exp,
            m->stat_exp,
            m->stat_exp,
            m->happiness);
    }

    if(!write_file(argv[1], "party_block.bin", block, sizeof(block)) ||
       !write_file(argv[1], "patch.bin", patch, sizeof(patch)) ||
       !write_file(argv[1], "expected.txt", text, (size_t)n)) {
        fprintf(stderr, "write failed\n");
        return 1;
    }
    return 0;
}
