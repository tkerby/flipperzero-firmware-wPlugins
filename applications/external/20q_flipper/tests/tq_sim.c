/* SPEC 11.2 -- offline simulator. Plays full games against the real engine so
 * the numbers describe the code that ships, not a Python re-implementation.
 *
 *   ./tq_sim files/20q.dat --games 400 --noise 0.10
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../helpers/twentyq_engine.h"
#include "tq_file_reader.h"

static uint32_t rng_state = 12345u;

static uint32_t rnd(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static float rndf(void) {
    return (float)(rnd() >> 8) / 16777216.0f;
}

/** The simulated player: answers question j about entity e by sampling the
 *  dataset's own P(yes), with an optional chance of answering wrongly. */
static Tq_Answer simulated_answer(const Tq_Dataset* ds, uint8_t* col, uint32_t e, float noise) {
    float p = tq_pyes(ds, col, e);
    if(p > 0.45f && p < 0.55f && rndf() < 0.5f) return TqAnswerUnknown;
    bool yes = rndf() < p;
    if(noise > 0.0f && rndf() < noise) yes = !yes;
    if(yes) return (p > 0.8f) ? TqAnswerYes : TqAnswerProbably;
    return (p < 0.2f) ? TqAnswerNo : TqAnswerProbablyNot;
}

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "files/20q.dat";
    int games = 400;
    int limit = 20;
    float noise = 0.0f;
    bool by_prior = true;
    bool verbose = false;
    const char* only_entity = NULL; /* --entity dog: print one full transcript */

    for(int i = 2; i < argc; i++) {
        if(!strcmp(argv[i], "--games") && i + 1 < argc)
            games = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--limit") && i + 1 < argc)
            limit = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--noise") && i + 1 < argc)
            noise = (float)atof(argv[++i]);
        else if(!strcmp(argv[i], "--seed") && i + 1 < argc)
            rng_state = (uint32_t)atoi(argv[++i]);
        else if(!strcmp(argv[i], "--uniform"))
            by_prior = false;
        else if(!strcmp(argv[i], "--verbose"))
            verbose = true;
        else if(!strcmp(argv[i], "--entity") && i + 1 < argc)
            only_entity = argv[++i];
    }

    Tq_Reader reader;
    if(!tq_test_reader_open(&reader, path)) {
        fprintf(stderr, "cannot open %s\n", path);
        return 2;
    }
    Tq_Dataset ds;
    Tq_Error err = tq_dataset_open(&ds, reader, true);
    if(err != TqOk) {
        fprintf(stderr, "%s: %s\n", path, tq_error_str(err));
        return 2;
    }

    uint32_t n = ds.entity_count;
    float* prior = malloc(sizeof(float) * n);
    tq_dataset_read_priors(&ds, prior);
    float prior_total = 0.0f;
    for(uint32_t i = 0; i < n; i++)
        prior_total += prior[i];

    uint8_t* col = malloc(ds.stride);
    Tq_Engine* eng = tq_engine_alloc(&ds, NULL);
    tq_engine_set_question_limit(eng, (uint16_t)limit);

    int wins = 0, first_guess_wins = 0, top3 = 0;
    long total_questions = 0, first_win_questions = 0;
    int best_game = 999;

    for(int g = 0; g < games; g++) {
        /* choose the entity the "player" is thinking of */
        uint32_t target;
        if(only_entity) {
            if(!tq_dataset_find_entity(&ds, only_entity, &target)) {
                fprintf(stderr, "no such entity: %s\n", only_entity);
                return 2;
            }
        } else if(by_prior) {
            float r = rndf() * prior_total, acc = 0.0f;
            target = n - 1;
            for(uint32_t i = 0; i < n; i++) {
                acc += prior[i];
                if(acc >= r) {
                    target = i;
                    break;
                }
            }
        } else {
            target = rnd() % n;
        }

        tq_engine_seed(eng, rnd() | 1u);
        tq_engine_reset(eng, TQ_NO_CATEGORY);

        bool won = false;
        int guesses = 0;
        while(1) {
            if(tq_engine_should_guess(eng)) {
                Tq_Candidate top[3];
                uint32_t got = tq_engine_top(eng, top, 3);
                if(got == 0) break;
                if(guesses == 0) {
                    for(uint32_t k = 0; k < got; k++) {
                        if(top[k].index == target) top3++;
                    }
                }
                if(only_entity) {
                    char gn[TQ_MAX_NAME + 1];
                    tq_dataset_entity_name(&ds, top[0].index, gn, sizeof(gn));
                    printf(
                        "  -> guess %u: %s (belief %.2f)\n",
                        guesses + 1u,
                        gn,
                        (double)top[0].belief);
                }
                if(top[0].index == target) {
                    won = true;
                    if(guesses == 0) {
                        first_guess_wins++;
                        first_win_questions += eng->asked_count;
                        if(eng->asked_count < best_game) best_game = eng->asked_count;
                    }
                    break;
                }
                tq_engine_reject(eng, top[0].index);
                if(++guesses >= TQ_MAX_GUESSES) break;
                if(tq_engine_out_of_questions(eng)) break;
            }
            uint16_t qid;
            if(!tq_engine_select_question(eng, &qid)) break;
            if(!tq_dataset_read_column(&ds, qid, col)) break;
            Tq_Answer a = simulated_answer(&ds, col, target, noise);
            if(only_entity) {
                char qt[TQ_MAX_QTEXT + 1];
                tq_dataset_question_text(&ds, qid, qt, sizeof(qt));
                printf("  %2u. %-46s %s\n", eng->asked_count + 1u, qt, tq_answer_label(a));
            }
            if(!tq_engine_answer(eng, qid, a)) break;
        }
        total_questions += eng->asked_count;
        if(won) wins++;
        if(verbose) {
            char nm[32];
            tq_dataset_entity_name(&ds, target, nm, sizeof(nm));
            printf("  %-22s %s in %u questions\n", nm, won ? "WON " : "lost", eng->asked_count);
        }
    }

    printf(
        "games %d  limit %d  noise %.2f  eps %.3f  candidates %d\n",
        games,
        limit,
        (double)noise,
        (double)TQ_EPS,
        TQ_CANDIDATES);
    printf("  win rate        %.1f%%\n", 100.0 * wins / games);
    printf("  first-guess     %.1f%%\n", 100.0 * first_guess_wins / games);
    printf("  top-3 accuracy  %.1f%%\n", 100.0 * top3 / games);
    printf("  mean questions  %.1f\n", (double)total_questions / games);
    if(first_guess_wins) {
        printf("  mean Qs to win  %.1f\n", (double)first_win_questions / first_guess_wins);
        printf("  best game       %d\n", best_game);
    }
    printf(
        "JSON {\"win_rate\": %.4f, \"first_guess\": %.4f, \"top3\": %.4f, "
        "\"mean_questions\": %.2f, \"eps\": %.3f, \"candidates\": %d, \"limit\": %d, "
        "\"noise\": %.2f}\n",
        (double)wins / games,
        (double)first_guess_wins / games,
        (double)top3 / games,
        (double)total_questions / games,
        (double)TQ_EPS,
        TQ_CANDIDATES,
        limit,
        (double)noise);

    tq_engine_free(eng);
    free(col);
    free(prior);
    tq_dataset_close(&ds);
    tq_test_reader_close(&reader);
    return 0;
}
