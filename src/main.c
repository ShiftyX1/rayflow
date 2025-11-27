#include "raylib.h"
#include "nuklear_raylib.h"
#include <string.h>
#include <stdlib.h>

#define MAX_TEXT_LENGTH 256

int main() {
    const int screenWidth = 800;
    const int screenHeight = 450;
    
    InitWindow(screenWidth, screenHeight, "PULSE engine TEST");
    SetTargetFPS(60);
    
    // Change working directory to project root
    ChangeDirectory("/Users/shifty/projects/raylib-test");
    
    // Load range of characters including Cyrillic
    // 0x0020-0x007F - basic Latin
    // 0x0400-0x04FF - Cyrillic
    int codepointCount = 0;
    int *codepoints = LoadCodepoints("АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~", &codepointCount);
    
    Font font = LoadFontEx("src/static/fonts/Inter_18pt-Regular.ttf", 20, codepoints, codepointCount);
    UnloadCodepoints(codepoints);
    
    if (font.texture.id == 0) {
        font = GetFontDefault();
    }
    
    nk_raylib_init();
    
    static char text_buffer[MAX_TEXT_LENGTH] = "Привет! Hello!";
    int text_len = strlen(text_buffer);
    static struct nk_colorf text_color = {0.2f, 0.2f, 0.2f, 1.0f};
    
    while (!WindowShouldClose()) {
        nk_raylib_handle_input();
        
        if (nk_begin(&nk_raylib.ctx, "Text Editor", nk_rect(50, 50, 400, 250),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {
            
            nk_layout_row_dynamic(&nk_raylib.ctx, 30, 1);
            nk_label(&nk_raylib.ctx, "Enter text:", NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(&nk_raylib.ctx, 30, 1);
            nk_edit_string(&nk_raylib.ctx, NK_EDIT_FIELD, text_buffer, &text_len, MAX_TEXT_LENGTH, nk_filter_default);
            text_buffer[text_len] = '\0';
            
            nk_layout_row_dynamic(&nk_raylib.ctx, 30, 1);
            nk_label(&nk_raylib.ctx, "Text Color:", NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(&nk_raylib.ctx, 120, 1);
            text_color = nk_color_picker(&nk_raylib.ctx, text_color, NK_RGBA);
        }
        nk_end(&nk_raylib.ctx);
        
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        Color raylib_color = {
            (unsigned char)(text_color.r * 255),
            (unsigned char)(text_color.g * 255),
            (unsigned char)(text_color.b * 255),
            (unsigned char)(text_color.a * 255)
        };
        DrawTextEx(font, text_buffer, (Vector2){190, 300}, 20, 1, raylib_color);
        DrawFPS(10, 10);
        
        nk_raylib_render();
        
        EndDrawing();
    }
    
    nk_raylib_shutdown();
    UnloadFont(font);
    CloseWindow();
    
    return 0;
}
