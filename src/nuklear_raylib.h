#ifndef NUKLEAR_RAYLIB_H
#define NUKLEAR_RAYLIB_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_RAYLIB_IMPLEMENTATION
#define STBRP_STATIC
#define STBTT_STATIC
#define STB_RECT_PACK_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION

#include "include/nuklear.h"
#include "raylib.h"
#include <string.h>

typedef struct {
    struct nk_context ctx;
    struct nk_font_atlas atlas;
    struct nk_buffer cmds;
    struct nk_draw_null_texture null;
    Texture2D font_texture;
} NuklearRaylibContext;

static NuklearRaylibContext nk_raylib;

static void nk_raylib_init(void) {
    nk_buffer_init_default(&nk_raylib.cmds);
    nk_font_atlas_init_default(&nk_raylib.atlas);
    nk_font_atlas_begin(&nk_raylib.atlas);
    
    const void *image;
    int w, h;
    image = nk_font_atlas_bake(&nk_raylib.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    
    Image img = {
        .data = (void*)image,
        .width = w,
        .height = h,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    nk_raylib.font_texture = LoadTextureFromImage(img);
    
    nk_font_atlas_end(&nk_raylib.atlas, nk_handle_id(nk_raylib.font_texture.id), &nk_raylib.null);
    
    nk_init_default(&nk_raylib.ctx, &nk_raylib.atlas.default_font->handle);
}

static void nk_raylib_handle_input(void) {
    struct nk_context *ctx = &nk_raylib.ctx;
    nk_input_begin(ctx);
    
    Vector2 mouse_pos = GetMousePosition();
    nk_input_motion(ctx, (int)mouse_pos.x, (int)mouse_pos.y);
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)mouse_pos.x, (int)mouse_pos.y, IsMouseButtonDown(MOUSE_LEFT_BUTTON));
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)mouse_pos.x, (int)mouse_pos.y, IsMouseButtonDown(MOUSE_RIGHT_BUTTON));
    
    int key = GetCharPressed();
    while (key > 0) {
        nk_input_unicode(ctx, key);
        key = GetCharPressed();
    }
    
    if (IsKeyPressed(KEY_BACKSPACE)) nk_input_key(ctx, NK_KEY_BACKSPACE, 1);
    if (IsKeyPressed(KEY_ENTER)) nk_input_key(ctx, NK_KEY_ENTER, 1);
    if (IsKeyPressed(KEY_DELETE)) nk_input_key(ctx, NK_KEY_DEL, 1);
    
    nk_input_end(ctx);
}

static void nk_raylib_render(void) {
    const struct nk_command *cmd;
    struct nk_context *ctx = &nk_raylib.ctx;
    
    nk_foreach(cmd, ctx) {
        switch (cmd->type) {
            case NK_COMMAND_NOP: break;
            case NK_COMMAND_SCISSOR: {
                const struct nk_command_scissor *s = (const struct nk_command_scissor*)cmd;
                BeginScissorMode(s->x, s->y, s->w, s->h);
            } break;
            case NK_COMMAND_RECT: {
                const struct nk_command_rect *r = (const struct nk_command_rect*)cmd;
                DrawRectangleLines(r->x, r->y, r->w, r->h, 
                    (Color){r->color.r, r->color.g, r->color.b, r->color.a});
            } break;
            case NK_COMMAND_RECT_FILLED: {
                const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled*)cmd;
                DrawRectangle(r->x, r->y, r->w, r->h, 
                    (Color){r->color.r, r->color.g, r->color.b, r->color.a});
            } break;
            case NK_COMMAND_TEXT: {
                const struct nk_command_text *t = (const struct nk_command_text*)cmd;
                DrawText((const char*)t->string, t->x, t->y, t->height, 
                    (Color){t->foreground.r, t->foreground.g, t->foreground.b, t->foreground.a});
            } break;
            case NK_COMMAND_LINE: {
                const struct nk_command_line *l = (const struct nk_command_line*)cmd;
                DrawLine(l->begin.x, l->begin.y, l->end.x, l->end.y,
                    (Color){l->color.r, l->color.g, l->color.b, l->color.a});
            } break;
            default: break;
        }
    }
    EndScissorMode();
    nk_clear(ctx);
}

static void nk_raylib_shutdown(void) {
    UnloadTexture(nk_raylib.font_texture);
    nk_font_atlas_clear(&nk_raylib.atlas);
    nk_buffer_free(&nk_raylib.cmds);
    nk_free(&nk_raylib.ctx);
}

#endif
