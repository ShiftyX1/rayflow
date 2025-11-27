#include "raylib.h"
#include "nuklear_raylib.h"
#include <string.h>

#define MAX_TEXT_LENGTH 256

int main() {
    const int screenWidth = 800;
    const int screenHeight = 450;
    
    InitWindow(screenWidth, screenHeight, "PULSE engine TEST");
    SetTargetFPS(60);
    
    nk_raylib_init();
    
    static char text_buffer[MAX_TEXT_LENGTH] = "Hello from raylib!";
    int text_len = strlen(text_buffer);
    
    while (!WindowShouldClose()) {
        nk_raylib_handle_input();
        
        if (nk_begin(&nk_raylib.ctx, "Text Editor", nk_rect(50, 50, 400, 150),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {
            
            nk_layout_row_dynamic(&nk_raylib.ctx, 30, 1);
            nk_label(&nk_raylib.ctx, "Enter text:", NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(&nk_raylib.ctx, 30, 1);
            nk_edit_string(&nk_raylib.ctx, NK_EDIT_FIELD, text_buffer, &text_len, MAX_TEXT_LENGTH, nk_filter_default);
            text_buffer[text_len] = '\0';
        }
        nk_end(&nk_raylib.ctx);
        
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        DrawText(text_buffer, 190, 300, 20, DARKGRAY);
        DrawFPS(10, 10);
        
        nk_raylib_render();
        
        EndDrawing();
    }
    
    nk_raylib_shutdown();
    CloseWindow();
    
    return 0;
}
