#include <furi.h>
#include <gui/gui.h>
#include <stdlib.h>
#include <input/input.h>

// Display constants
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define CHAR_WIDTH 6
#define CHAR_HEIGHT 8
#define MAX_COLUMNS (SCREEN_WIDTH / CHAR_WIDTH)
#define MAX_ROWS (SCREEN_HEIGHT / CHAR_HEIGHT)

// Structure to track each falling column of characters
typedef struct {
    int x;       // Column position (x-coordinate)
    int y;       // Current y-coordinate of the column head
    int length;  // Length of the "rain" in this column
    int speed;   // Speed of the falling characters
} MatrixColumn;

// Context to maintain the state of all matrix columns
typedef struct {
    MatrixColumn columns[MAX_COLUMNS];
    int num_columns;
} MatrixContext;

// Main application context structure
typedef struct {
    Gui* gui;
    ViewPort* viewport;
    FuriTimer* timer;
    bool exit;
    MatrixContext matrix_ctx;
} MatrixApp;

// Initialize a new matrix column with random properties
static void init_matrix_column(MatrixColumn* column, int x_pos) {
    column->x = x_pos * CHAR_WIDTH;
    column->y = -(rand() % SCREEN_HEIGHT);
    column->length = 3 + rand() % 10;  // Random length between 3 and 12
    column->speed = 1 + rand() % 3;    // Random speed between 1 and 3
}

// Initialize the entire matrix effect
static void init_matrix_context(MatrixContext* ctx) {
    ctx->num_columns = MAX_COLUMNS;
    
    // Initialize each column with different starting positions
    for(int i = 0; i < ctx->num_columns; i++) {
        init_matrix_column(&ctx->columns[i], i);
        // Stagger the starting positions
        ctx->columns[i].y -= (rand() % SCREEN_HEIGHT);
    }
}

// Render the matrix rain effect
static void matrix_render(Canvas* canvas, void* context) {
    MatrixContext* ctx = (MatrixContext*)context;
    canvas_clear(canvas);
    
    // Draw and update each column
    for(int i = 0; i < ctx->num_columns; i++) {
        // Draw each character in the column
        for(int j = 0; j < ctx->columns[i].length; j++) {
            int char_y = ctx->columns[i].y - (j * CHAR_HEIGHT);
            
            // Only draw if the character is within screen bounds
            if(char_y >= 0 && char_y < SCREEN_HEIGHT) {
                // Generate random character (A-Z)
                char random_char = 'A' + (rand() % 26);
                canvas_draw_str(canvas, ctx->columns[i].x, char_y, &random_char);
            }
        }
        
        // Update column position
        ctx->columns[i].y += ctx->columns[i].speed;
        
        // Reset column if it moves off screen
        if(ctx->columns[i].y - (ctx->columns[i].length * CHAR_HEIGHT) > SCREEN_HEIGHT) {
            init_matrix_column(&ctx->columns[i], ctx->columns[i].x / CHAR_WIDTH);
        }
    }
}

// Input callback to handle button presses
static void matrix_input_callback(InputEvent* input_event, void* context) {
    MatrixApp* app = context;
    furi_assert(app != NULL);
    
    if(input_event->key == InputKeyBack && input_event->type == InputTypePress) {
        app->exit = true;
    }
}

// Timer callback to trigger screen updates
static void matrix_timer_callback(void* context) {
    MatrixApp* app = context;
    furi_assert(app != NULL);
    
    view_port_update(app->viewport);
}

// Clean up and free all resources
static void matrix_app_free(MatrixApp* app) {
    furi_assert(app != NULL);
    
    // Stop and free the timer
    if(app->timer != NULL) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }
    
    // Clean up GUI resources
    if(app->viewport != NULL) {
        view_port_enabled_set(app->viewport, false);
        gui_remove_view_port(app->gui, app->viewport);
        view_port_free(app->viewport);
    }
    
    // Close GUI record
    if(app->gui != NULL) {
        furi_record_close("gui");
    }
    
    // Free the app context
    free(app);
}

// Main application entry point
int32_t matrix_app(void* p) {
    UNUSED(p);
    
    // Allocate and initialize application context
    MatrixApp* app = malloc(sizeof(MatrixApp));
    app->exit = false;
    
    // Initialize the matrix effect
    init_matrix_context(&app->matrix_ctx);
    
    // Initialize GUI
    app->gui = furi_record_open("gui");
    
    // Create and configure ViewPort
    app->viewport = view_port_alloc();
    view_port_input_callback_set(app->viewport, matrix_input_callback, app);
    view_port_draw_callback_set(app->viewport, matrix_render, &app->matrix_ctx);
    gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);
    
    // Initialize timer for animation updates
    app->timer = furi_timer_alloc(matrix_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 50);  // 50ms for smooth animation
    
    // Main application loop
    while(!app->exit) {
        furi_delay_ms(10);
    }
    
    // Cleanup all resources
    matrix_app_free(app);
    
    return 0;
}