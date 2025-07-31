#include <furi.h>
#include <furi_hal.h>
#include <stdio.h>
#include "engine.h"

void find_ai_jumps(JumpingPawnsModel* model, int board[11][6], int from_x, int from_y, int x, int y, int player_value, AIMoveStructure* move_list, size_t* move_count, size_t max_moves, bool visited[11][6]) {
    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};

    visited[y][x] = true;

    for (int dir = 0; dir < 4; dir++) {
        int cx = x;
        int cy = y;

        bool found_clump = false;

        int mid_x = cx + dx[dir];
        int mid_y = cy + dy[dir];

        // Traverse clump (any non-zero tile, avoid infinite loop on marked moves)
        while (mid_x >= 0 && mid_x < 6 && mid_y >= 0 && mid_y < 11 &&
               board[mid_y][mid_x] != 0 && board[mid_y][mid_x] != 3) {
            cx = mid_x;
            cy = mid_y;
            mid_x = cx + dx[dir];
            mid_y = cy + dy[dir];
            found_clump = true;
        }

        // Landing square
        if (found_clump &&
            mid_x >= 0 && mid_x < 6 && mid_y >= 0 && mid_y < 11 &&
            board[mid_y][mid_x] == 0 && !visited[mid_y][mid_x]) {

            if (*move_count < max_moves) {
                move_list[(*move_count)++] = (AIMoveStructure){
                    .from_x = from_x,
                    .from_y = from_y,
                    .to_x = mid_x,
                    .to_y = mid_y,
                };
            }

            find_ai_jumps(model, board, from_x, from_y, mid_x, mid_y, player_value, move_list, move_count, max_moves, visited);
        }
    }

    visited[y][x] = false;
}

size_t generate_legal_ai_moves(JumpingPawnsModel* model, int player_value, AIMoveStructure* move_list, size_t max_moves) {
    size_t move_count = 0;

    for (int sy = 0; sy < 11; sy++) {
        for (int sx = 0; sx < 6; sx++) {
            if (model->board_state[sy][sx] != player_value) continue;

            bool visited[11][6] = {false};
            find_ai_jumps(model, model->board_state, sx, sy, sx, sy, player_value, move_list, &move_count, max_moves, visited);
        }
    }

    return move_count;
}

int evaluate_board(JumpingPawnsModel* model) {
    int score = 0;
    // add points for how close AI pieces are to the bottom
    for(int y = 0; y < 11; y++) {
        for(int x = 0; x < 6; x++) {
            if (model->board_state[y][x] == 2) {
                score += y;
            }
        }
    }

    // subtract points for how close opponent pieces are to AI's side
    for(int y = 0; y < 11; y++) {
        for(int x = 0; x < 6; x++) {
            if (model->board_state[y][x] == 1) {
                score += y - 10;
            }
        }
    }

    return score;
}

void copy_model(JumpingPawnsModel* dest, JumpingPawnsModel* src) {
    for(int y = 0; y < 11; y++) {
        for(int x = 0; x < 6; x++) {
            dest->board_state[y][x] = src->board_state[y][x];
        }
    }
}

int minimax(JumpingPawnsModel* model, int depth, bool maximizingPlayer) {
    if(depth == 0) {
        return evaluate_board(model);
    }

    AIMoveStructure moves[64];
    size_t move_count = generate_legal_ai_moves(model, maximizingPlayer ? 2 : 1, moves, 64);

    if(move_count == 0) {
        return evaluate_board(model);
    }

    int best_score = maximizingPlayer ? -10000 : 10000;

    for(size_t i = 0; i < move_count; i++) {
        JumpingPawnsModel temp;
        copy_model(&temp, model);
        apply_ai_move(&temp, moves[i], maximizingPlayer ? 2 : 1);

        int score = minimax(&temp, depth - 1, !maximizingPlayer);

        if(maximizingPlayer) {
            if(score > best_score) best_score = score;
        } else {
            if(score < best_score) best_score = score;
        }
    }

    return best_score;
}

void apply_ai_move(JumpingPawnsModel* model, AIMoveStructure move, int player_value) {
    model->board_state[move.from_y][move.from_x] = 0;
    model->board_state[move.to_y][move.to_x] = player_value;
}

void ai_move(JumpingPawnsModel* model) {
    AIMoveStructure moves[64];
    size_t count = generate_legal_ai_moves(model, 2, moves, 64);
    int depth = 3;

    if(count == 0) return;

    int best_score = -10000;
    AIMoveStructure best_move = moves[0];

    for(size_t i = 0; i < count; i++) {
        JumpingPawnsModel temp;
        copy_model(&temp, model);
        apply_ai_move(&temp, moves[i], 2);
        int score = minimax(&temp, depth, false);

        if(score > best_score) {
            best_score = score;
            best_move = moves[i];
        }
    }

    apply_ai_move(model, best_move, 2);
}