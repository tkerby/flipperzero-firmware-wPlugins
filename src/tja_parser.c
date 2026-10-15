#include "tja_parser.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NOTES    8192
#define MAX_SLOTS    2048
#define MAX_COMMANDS 64
#define MAX_TIME_MS  3600000.0f

typedef struct {
    size_t slot;
    float value;
    enum {
        CommandBpm,
        CommandMeasure,
        CommandDelay
    } kind;
} MeasureCommand;

typedef struct {
    char slots[MAX_SLOTS];
    size_t slot_count;
    MeasureCommand commands[MAX_COMMANDS];
    size_t command_count;
    size_t note_capacity;
    float bpm;
    float beats;
    float time_ms;
} Parser;

static char* trim(char* s) {
    while(isspace((unsigned char)*s))
        s++;
    size_t len = strlen(s);
    while(len && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
    return s;
}

static bool number(const char* text, float* value) {
    char* end;
    *value = strtof(text, &end);
    while(isspace((unsigned char)*end))
        end++;
    return end != text && !*end && isfinite(*value);
}

static bool add_note(Parser* p, TjaChart* chart, char kind, const char** error) {
    if(kind == '0' || kind == '8') return true;
    if(chart->note_count == MAX_NOTES) {
        *error = "Chart exceeds 8192 notes";
        return false;
    }
    if(chart->note_count == p->note_capacity) {
        size_t capacity = p->note_capacity ? p->note_capacity * 2 : 256;
        TjaNote* notes = realloc(chart->notes, capacity * sizeof(TjaNote));
        if(!notes) {
            *error = "Not enough memory";
            return false;
        }
        chart->notes = notes;
        p->note_capacity = capacity;
    }
    chart->notes[chart->note_count++] =
        (TjaNote){.time_ms = (int32_t)lroundf(p->time_ms), .kind = (uint8_t)(kind - '0')};
    return true;
}

static bool finish_measure(Parser* p, TjaChart* chart, const char** error) {
    size_t command = 0;
    size_t slots = p->slot_count ? p->slot_count : 1;
    for(size_t i = 0; i <= slots; i++) {
        while(command < p->command_count && p->commands[command].slot == i) {
            MeasureCommand* c = &p->commands[command++];
            if(c->kind == CommandBpm)
                p->bpm = c->value;
            else if(c->kind == CommandMeasure)
                p->beats = c->value;
            else
                p->time_ms += c->value * 1000.0f;
        }
        if(fabsf(p->time_ms) > MAX_TIME_MS) {
            *error = "Chart exceeds one hour";
            return false;
        }
        if(i == slots) break;
        if(p->slot_count && !add_note(p, chart, p->slots[i], error)) return false;
        p->time_ms += 60000.0f / p->bpm * p->beats / slots;
    }
    p->slot_count = 0;
    p->command_count = 0;
    return true;
}

static bool command(Parser* p, char* line, const char** error) {
    char* argument = line;
    while(*argument && !isspace((unsigned char)*argument))
        argument++;
    if(*argument) *argument++ = '\0';
    argument = trim(argument);
    MeasureCommand c = {.slot = p->slot_count};
    if(strcmp(line, "#BPMCHANGE") == 0) {
        c.kind = CommandBpm;
        if(!number(argument, &c.value) || c.value < 1 || c.value > 10000) goto invalid;
    } else if(strcmp(line, "#MEASURE") == 0) {
        c.kind = CommandMeasure;
        char* slash = strchr(argument, '/');
        float numerator, denominator;
        if(!slash) goto invalid;
        *slash++ = '\0';
        if(!number(argument, &numerator) || !number(slash, &denominator) || numerator <= 0 ||
           denominator <= 0)
            goto invalid;
        c.value = 4.0f * numerator / denominator;
        if(!isfinite(c.value) || c.value > 64) goto invalid;
    } else if(strcmp(line, "#DELAY") == 0) {
        c.kind = CommandDelay;
        /* Negative delay would make the event list non-monotonic. */
        if(!number(argument, &c.value) || c.value < 0 || c.value > 3600) goto invalid;
    } else if(strcmp(line, "#BRANCHSTART") == 0 || strcmp(line, "#NEXTSONG") == 0) {
        *error = "Branches / medleys unsupported";
        return false;
    } else {
        /* SCROLL, GOGO, BARLINE, etc. do not affect strike timing. */
        return true;
    }
    if(p->command_count == MAX_COMMANDS) {
        *error = "Too many measure commands";
        return false;
    }
    p->commands[p->command_count++] = c;
    return true;
invalid:
    *error = "Invalid timing command";
    return false;
}

bool tja_parse(TjaReadLine reader, void* context, TjaChart* chart, const char** error) {
    memset(chart, 0, sizeof(*chart));
    *error = "Invalid TJA";
    Parser* p = calloc(1, sizeof(Parser));
    char* buffer = malloc(2048);
    if(!p || !buffer) {
        free(p);
        free(buffer);
        *error = "Not enough memory";
        return false;
    }
    p->bpm = 120;
    p->beats = 4;
    bool started = false, success = false;
    bool first_line = true;
    int result;
    while((result = reader(context, buffer, 2048)) > 0) {
        char* line = buffer;
        if(first_line && strncmp(line, "\xef\xbb\xbf", 3) == 0) line += 3;
        first_line = false;
        char* comment = strstr(line, "//");
        if(comment) *comment = '\0';
        line = trim(line);
        if(!*line) continue;
        if(!started) {
            if(strcmp(line, "#START") == 0 || strncmp(line, "#START ", 7) == 0) {
                if(!*chart->wave) {
                    *error = "Missing WAVE in TJA";
                    break;
                }
                started = true;
            } else {
                char* colon = strchr(line, ':');
                if(!colon) continue;
                *colon++ = '\0';
                char* value = trim(colon);
                line = trim(line);
                if(strcmp(line, "TITLE") == 0)
                    snprintf(chart->title, sizeof(chart->title), "%s", value);
                else if(strcmp(line, "COURSE") == 0)
                    snprintf(chart->course, sizeof(chart->course), "%s", value);
                else if(strcmp(line, "WAVE") == 0) {
                    if(strlen(value) >= sizeof(chart->wave)) {
                        *error = "WAVE path is too long";
                        break;
                    }
                    strcpy(chart->wave, value);
                } else if(strcmp(line, "BPM") == 0) {
                    if(!number(value, &p->bpm) || p->bpm < 1 || p->bpm > 10000) {
                        *error = "Invalid BPM";
                        break;
                    }
                } else if(strcmp(line, "OFFSET") == 0) {
                    float offset;
                    if(!number(value, &offset) || fabsf(offset) > 3600) {
                        *error = "Invalid OFFSET";
                        break;
                    }
                    p->time_ms = -offset * 1000.0f;
                }
            }
        } else if(strcmp(line, "#END") == 0) {
            if(p->slot_count) {
                *error = "Missing final measure comma";
                break;
            }
            chart->end_ms = (int32_t)lroundf(p->time_ms);
            success = true;
            break;
        } else if(*line == '#') {
            if(!command(p, line, error)) break;
        } else {
            bool valid = true;
            for(char* c = line; *c; c++) {
                if(*c == ',') {
                    if(!finish_measure(p, chart, error)) {
                        valid = false;
                        break;
                    }
                } else if(*c >= '0' && *c <= '9') {
                    if(p->slot_count == MAX_SLOTS) {
                        *error = "Measure exceeds 2048 slots";
                        valid = false;
                        break;
                    }
                    p->slots[p->slot_count++] = *c;
                } else if(!isspace((unsigned char)*c)) {
                    *error = "Unsupported note character";
                    valid = false;
                    break;
                }
            }
            if(!valid) break;
        }
    }
    if(result < 0)
        *error = "TJA read error / line too long";
    else if(result == 0)
        *error = "Missing #START or #END";
    free(buffer);
    free(p);
    if(success) *error = NULL;
    return success;
}

void tja_chart_free(TjaChart* chart) {
    free(chart->notes);
    memset(chart, 0, sizeof(*chart));
}
