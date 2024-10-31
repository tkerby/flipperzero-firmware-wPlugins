#pragma once
#include <math.h>
#include <furi_hal_random.h>
#ifdef _cplusplus
extern "C" {
#endif

extern float game_ai_features[4];
extern float game_ai_weights[4];


void softmax(float scores[], float probs[]);
int choose_action(float probs[]);
int decide_action(
    float player_x,
    float player_y,
    float bullet_x,
    float bullet_y,
    float opponent_x,
    float opponent_y,
    float weights[]);
void update_weights(float weights[], float features[], int action, int reward);

#ifdef __cplusplus
}
#endif
