# Portrait Mode UI Rotation Implementation Plan

## Overview
Transform the submarine game from landscape (128x64) to portrait (64x128) orientation with a 90-degree counter-clockwise rotation. The submarine will continue to point "up" toward the top of the screen, and terrain will rotate around it as before.

## Current State Analysis

### Display Configuration
- **Current**: Landscape 128x64 pixels
- **Target**: Portrait 64x128 pixels (90° CCW rotation)
- **Submarine Position**: Currently at (64, 32) - center of landscape screen
- **New Submarine Position**: Will be at (32, 64) - center of portrait screen

### Current UI Elements
1. **Top HUD** (game.c:288): "V:%.2f H:%.2f" - velocity and heading display
2. **Bottom HUD** (game.c:289-291): "NAV/TORP T:x/6" - mode and torpedo count
3. **Submarine**: Circle with heading indicator line pointing up
4. **Terrain**: Dots rendered based on sonar discovery

### Current Control Scheme
- **Left/Right**: Turn submarine (adjust heading)
- **Up/Down**: Accelerate/decelerate
- **OK**: Fire torpedo or send ping
- **Back**: Toggle mode or exit

## Implementation Changes Required

### 1. Screen Coordinate Transformation (Priority: CRITICAL)

#### File: game.c
**Location**: Lines 19-37 (world_to_screen function)

**Current Implementation**:
```c
// Screen coordinates with submarine at (64, 32)
screen.screen_x = ctx->screen_x + rot_x;  // ctx->screen_x = 64
screen.screen_y = ctx->screen_y + rot_y;  // ctx->screen_y = 32
```

**Required Changes**:
```c
// Step 1: Apply world-to-screen rotation as before
float rot_x = rel_x * cos_h - rel_y * sin_h;
float rot_y = rel_x * sin_h + rel_y * cos_h;

// Step 2: Apply 90° CCW screen rotation
// Original: (x, y) -> Rotated: (-y + 64, x)
screen.screen_x = -rot_y + 32;  // Center at 32 (half of 64)
screen.screen_y = rot_x + 64;   // Center at 64 (half of 128)
```

**Verification**: All rendered points should map correctly to portrait orientation.

### 2. Submarine Position Update (Priority: HIGH)

#### File: game.c
**Locations**: 
- Line 100: `entity_pos_set(self, (Vector){64, 32});`
- Line 212: `entity_pos_set(self, (Vector){game_context->screen_x, game_context->screen_y});`
- Lines 437-438: Initial screen position setup

**Required Changes**:
```c
// In submarine_start (line 100)
entity_pos_set(self, (Vector){32, 64});

// In game_start (lines 437-438)
game_context->screen_x = 32;  // Center of 64px width
game_context->screen_y = 64;  // Center of 128px height
```

### 3. Control Rotation (Priority: HIGH)

#### File: game.c
**Location**: Lines 119-136 (submarine_update input handling)

**Required Mapping** (90° CCW rotation):
- Physical UP button -> Game RIGHT (turn right)
- Physical DOWN button -> Game LEFT (turn left)  
- Physical LEFT button -> Game DOWN (decelerate)
- Physical RIGHT button -> Game UP (accelerate)

**Implementation**:
```c
// Remap controls for portrait orientation
if(input.held & GameKeyUp) {    // Physical up = turn right
    game_context->heading += game_context->turn_rate;
    if(game_context->heading >= 1.0f) game_context->heading -= 1.0f;
}
if(input.held & GameKeyDown) {  // Physical down = turn left
    game_context->heading -= game_context->turn_rate;
    if(game_context->heading < 0) game_context->heading += 1.0f;
}
if(input.held & GameKeyRight) { // Physical right = accelerate
    game_context->velocity += game_context->acceleration;
    if(game_context->velocity > game_context->max_velocity) {
        game_context->velocity = game_context->max_velocity;
    }
}
if(input.held & GameKeyLeft) {  // Physical left = decelerate
    game_context->velocity -= game_context->acceleration;
    if(game_context->velocity < 0) game_context->velocity = 0;
}
```

### 4. Screen Boundary Updates (Priority: HIGH)

#### File: game.c
**Location**: Lines 243-244 (terrain rendering boundary check)

**Required Changes**:
```c
// Update screen boundaries for portrait mode
if(screen.screen_x >= 0 && screen.screen_x < 64 &&   // Was 128
   screen.screen_y >= 0 && screen.screen_y < 128) {  // Was 64
    canvas_draw_dot(canvas, screen.screen_x, screen.screen_y);
}
```

### 5. UI Element Redesign (Priority: MEDIUM)

#### A. Velocity Indicator (3 bars)
**Location**: game.c:288 (current text display)

**New Implementation**:
```c
// Draw velocity bars instead of text (bottom left in portrait)
int vel_bars = 0;
if(game_context->velocity > 0.066f) vel_bars = 3;  // Fast
else if(game_context->velocity > 0.033f) vel_bars = 2;  // Medium  
else if(game_context->velocity > 0.001f) vel_bars = 1;  // Slow

for(int i = 0; i < 3; i++) {
    int bar_x = 2 + (i * 4);
    int bar_y = 120;
    if(i < vel_bars) {
        canvas_draw_box(canvas, bar_x, bar_y, 3, 6);  // Filled bar
    } else {
        canvas_draw_frame(canvas, bar_x, bar_y, 3, 6);  // Empty frame
    }
}
```

#### B. Torpedo Indicators (8 tiny torpedoes)
**Location**: game.c:289-291 (current text display)

**New Implementation**:
```c
// Draw torpedo icons instead of text (bottom right in portrait)
for(int i = 0; i < game_context->max_torpedoes; i++) {
    int torp_x = 40 + (i % 4) * 6;  // 4 columns
    int torp_y = 118 + (i / 4) * 8;  // 2 rows
    
    if(i < game_context->torpedo_count) {
        // Draw filled torpedo (fired)
        canvas_draw_disc(canvas, torp_x, torp_y, 1);
        canvas_draw_line(canvas, torp_x + 2, torp_y, torp_x + 3, torp_y);
    } else {
        // Draw outline torpedo (available)
        canvas_draw_circle(canvas, torp_x, torp_y, 1);
        canvas_draw_dot(canvas, torp_x + 2, torp_y);
        canvas_draw_dot(canvas, torp_x + 3, torp_y);
    }
}
```

#### C. Mode Indicator
**Location**: Near velocity bars

**New Implementation**:
```c
// Small mode indicator near velocity bars
canvas_draw_str(canvas, 2, 110, game_context->mode == GAME_MODE_NAV ? "N" : "T");
```

### 6. Velocity Control Simplification (Priority: LOW)

#### File: game.c
**Location**: Lines 515-517 (game settings)

**Required Changes**:
```c
// Simplify to 4 discrete velocity levels
game_context->max_velocity = 0.1f;     // Keep same
game_context->acceleration = 0.034f;   // Jump to next level (0.1/3)

// In submarine_update, implement discrete velocity levels:
if(input.pressed & GameKeyRight) {  // Pressed, not held
    if(game_context->velocity < 0.001f) {
        game_context->velocity = 0.0f;    // Stop
    } else if(game_context->velocity < 0.034f) {
        game_context->velocity = 0.033f;  // Slow
    } else if(game_context->velocity < 0.067f) {
        game_context->velocity = 0.066f;  // Medium
    } else {
        game_context->velocity = 0.1f;    // Fast
    }
}
if(input.pressed & GameKeyLeft) {   // Decrease velocity
    if(game_context->velocity > 0.066f) {
        game_context->velocity = 0.066f;  // Medium
    } else if(game_context->velocity > 0.033f) {
        game_context->velocity = 0.033f;  // Slow
    } else {
        game_context->velocity = 0.0f;    // Stop
    }
}
```

### 7. Terrain System Updates (Priority: LOW)

#### File: game.c
**Location**: Lines 428-429 (chart dimensions)

**Required Changes**:
```c
// Update chart dimensions for portrait
game_context->chart_width = 64;   // Was 128
game_context->chart_height = 128; // Was 64
```

## Implementation Order

1. **Phase 1: Core Rotation**
   - [ ] Update submarine screen position (32, 64)
   - [ ] Implement coordinate transformation in world_to_screen
   - [ ] Update screen boundary checks
   - [ ] Test basic rendering

2. **Phase 2: Control Remapping**
   - [ ] Implement control rotation in submarine_update
   - [ ] Test submarine movement and turning
   - [ ] Verify heading indicator points correctly

3. **Phase 3: UI Replacement**
   - [ ] Remove text-based HUD elements
   - [ ] Implement velocity bars
   - [ ] Implement torpedo indicators
   - [ ] Add minimal mode indicator

4. **Phase 4: Refinements**
   - [ ] Implement discrete velocity levels
   - [ ] Update terrain chart dimensions
   - [ ] Optimize rendering for portrait layout

## Testing Strategy

### Unit Tests

1. **Coordinate Transformation Test**
```c
void test_rotation() {
    // Test that (64, 32) -> (32, 64) after rotation
    GameContext ctx = {.screen_x = 32, .screen_y = 64};
    ScreenPoint p = world_to_screen(&ctx, 32, 64);
    assert(p.screen_x == 32 && p.screen_y == 64);
    
    // Test corner cases
    // Top-left landscape (0,0) -> bottom-left portrait (0, 64)
    // Top-right landscape (128,0) -> top-left portrait (0, 0)
    // Bottom-right landscape (128,64) -> top-right portrait (64, 0)
}
```

2. **Control Mapping Test**
```c
void test_controls() {
    // Verify each physical button maps to correct action
    // UP -> increase heading
    // DOWN -> decrease heading
    // RIGHT -> increase velocity
    // LEFT -> decrease velocity
}
```

3. **Boundary Test**
```c
void test_boundaries() {
    // Ensure all rendered points fall within 64x128
    // Test terrain rendering boundaries
    // Test torpedo range calculations
}
```

### Integration Tests

1. **Visual Verification**
   - Submarine appears at center of portrait screen
   - Heading indicator points to top of screen
   - Terrain rotates correctly around submarine
   - UI elements appear in correct positions

2. **Gameplay Test**
   - Controls feel natural in portrait orientation
   - Velocity changes work with discrete levels
   - Torpedo firing works correctly
   - Mode switching functions properly

## Success Criteria

### Functional Requirements
1. ✓ Screen displays in 64x128 portrait orientation
2. ✓ Submarine centered at (32, 64) pointing up
3. ✓ Controls rotated 90° CCW to match portrait hold
4. ✓ Terrain rotates around submarine correctly
5. ✓ All UI elements visible and functional

### Visual Requirements
1. ✓ 8 torpedo indicators show fired/available status
2. ✓ 3 velocity bars indicate speed levels
3. ✓ No text-based UI elements (except mode indicator)
4. ✓ All graphics fit within 64x128 boundaries

### Performance Requirements
1. ✓ Maintains 30 FPS target
2. ✓ No visual artifacts or clipping
3. ✓ Smooth rotation and movement

## Validation Checklist for LLM Implementation

### Pre-Implementation
- [ ] Read game.c completely
- [ ] Understand current coordinate system
- [ ] Map all UI element positions
- [ ] Identify all screen dimension dependencies

### During Implementation
- [ ] Create backup of original game.c
- [ ] Implement changes in order specified
- [ ] Test after each phase
- [ ] Document any deviations from plan

### Post-Implementation
- [ ] Run `make build` successfully
- [ ] Verify all screen elements render in portrait
- [ ] Test all controls work as specified
- [ ] Confirm velocity bars show correct states
- [ ] Verify torpedo indicators update properly
- [ ] Test mode switching still works
- [ ] Ensure terrain rotation is smooth
- [ ] Check boundary conditions
- [ ] Run on actual device for final validation

## Common Pitfalls to Avoid

1. **Don't forget to rotate ALL coordinates** - Every canvas_draw call needs updated coordinates
2. **Remember screen_to_world needs inverse rotation** - Though currently unused, keep it correct
3. **Velocity bar calculation** - Ensure thresholds match discrete levels exactly
4. **Torpedo count logic** - Available torpedoes = max - count (unfired), not fired count
5. **Chart dimensions** - Update both width and height throughout
6. **Ping radius** - May need adjustment for portrait aspect ratio
7. **Heading calculation** - Ensure submarine still points "up" after rotation

## Debug Helpers

Add these temporary debug displays during development:
```c
// Show screen dimensions
canvas_printf(canvas, 2, 8, "%dx%d", 64, 128);

// Show submarine position
canvas_printf(canvas, 2, 16, "S:%d,%d", (int)game_context->screen_x, (int)game_context->screen_y);

// Show control state
canvas_printf(canvas, 2, 24, "H:%.2f V:%.2f", game_context->heading, game_context->velocity);
```

Remove these once implementation is verified correct.