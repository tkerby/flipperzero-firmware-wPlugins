#include "reinforcement_learning_logic.h"

//If you really desire, you can change this
#define LEARNING_RATE 0.0001f

//These change as time progresses, they represent the current features.
float game_ai_features[4] = {0.0f};

//These values somewhat work
float game_ai_weights[4] = {1.5f, -1.0f, 0, -1.0f};

// Function to calculate softmax probabilities
void softmax(float scores[], float probs[]) {
    float sum = 0.0;
    for(int i = 0; i < 4; i++) {
        scores[i] = expf(scores[i]);
        sum += scores[i];
    }
    for(int i = 0; i < 4; i++) {
        probs[i] = scores[i] / sum;
    }
}

int choose_action(float probs[]) {
    float rand_value = (float)furi_hal_random_get() / FURI_HAL_RANDOM_MAX;
    float cumulative = 0.0;
    for(int i = 0; i < 4; i++) {
        cumulative += probs[i];
        if(rand_value < cumulative) return i;
    }
    return 2; // Fallback is STAY
}

// Main decision function for NPC
int decide_action(
    float player_x,
    float player_y,
    float bullet_x,
    float bullet_y,
    float opponent_x,
    float opponent_y,
    float weights[]) {
    float scores[4];
    float probs[4];
    // Simple distance-based feature calculation for each action
    game_ai_features[0] =
        (player_y > bullet_y ? 1 : -1) + (player_y > opponent_y ? -0.5 : 0.5); // Move up
    game_ai_features[1] =
        (player_x > bullet_x ? 1 : -1) + (player_x > opponent_x ? -0.5 : 0.5); // Move left
    game_ai_features[2] =
        (player_y < bullet_y ? 1 : -1) + (player_y < opponent_y ? -0.5 : 0.5); // Move down
    game_ai_features[3] =
        (player_x < bullet_x ? 1 : -1) + (player_x < opponent_x ? -0.5 : 0.5); // Move right

    // Calculate scores for each action based on weights and features
    for(int i = 0; i < 4; i++) {
        scores[i] = weights[i] * game_ai_features[i];
    }

    // Compute probabilities with softmax
    softmax(scores, probs);

    // Choose an action based on the softmax probabilities
    return choose_action(probs);
}

// Update weights based on reward (1 for success, -1 for failure)
void update_weights(float weights[], float features[], int action, int reward) {
    weights[action] += LEARNING_RATE * reward * features[action];
}
