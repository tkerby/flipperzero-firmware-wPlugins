#include "run/sprites.hpp"
#include <math.h>

Sprite::Sprite(const char *name, EntityType type, Vector position, Vector endPosition, Vector size, float move_timer, float speed, float attack_timer, float strength, float health)
    : Entity(name, type, position, size, nullptr, nullptr, nullptr)
{
    this->start_position = position;
    this->end_position = endPosition == Vector(-1, -1) ? position : endPosition;
    this->state = ENTITY_MOVING_TO_END;
    this->move_timer = move_timer;
    this->speed = speed;
    this->attack_timer = attack_timer;
    this->strength = strength;
    this->health = health;
}

void Sprite::collision(Entity *other, Game *game)
{
    UNUSED(game);
    if (other->type != ENTITY_PLAYER)
    {
        return;
    }
    // I'll come back to this...
    // FURI_LOG_I("Sprite", "Collision with player detected: %s", other->name);
    // this->position_set(this->old_position);
}

void Sprite::update(Game *game)
{
    // Update the sprite's state
    UNUSED(game);

    // Only move if start and end positions are different
    if (start_position.x == end_position.x && start_position.y == end_position.y)
    {
        return; // No movement needed
    }

    const float DISTANCE_THRESHOLD = 0.2f; // Minimum distance to consider "reached"
    const float MOVE_SPEED = 0.1f;         // Even faster movement speed

    // move towards the end position if the state is MOVING_TO_END
    if (state == ENTITY_MOVING_TO_END)
    {
        float dx = end_position.x - position.x;
        float dy = end_position.y - position.y;
        float distance = sqrtf(dx * dx + dy * dy);

        if (distance > DISTANCE_THRESHOLD)
        {
            // Normalize direction and apply speed
            float move_x = (dx / distance) * MOVE_SPEED;
            float move_y = (dy / distance) * MOVE_SPEED;

            // Don't overshoot the target
            if (distance < MOVE_SPEED)
            {
                move_x = dx;
                move_y = dy;
            }

            position.x += move_x;
            position.y += move_y;
        }
        else
        {
            position = end_position; // Snap to end position when close enough
            state = ENTITY_IDLE;     // Set to idle state when reached
        }
    }
}