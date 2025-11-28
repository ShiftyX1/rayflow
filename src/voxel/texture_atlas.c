#include "texture_atlas.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Создание текстурного атласа из файла
TextureAtlas* texture_atlas_create(const char* filepath) {
    TextureAtlas* atlas = (TextureAtlas*)malloc(sizeof(TextureAtlas));
    if (!atlas) {
        fprintf(stderr, "Failed to allocate memory for texture atlas\n");
        return NULL;
    }

    // Загрузка текстуры
    atlas->texture = LoadTexture(filepath);
    if (atlas->texture.id == 0) {
        fprintf(stderr, "Failed to load texture atlas: %s\n", filepath);
        free(atlas);
        return NULL;
    }

    // Устанавливаем фильтрацию для пиксельной графики
    SetTextureFilter(atlas->texture, TEXTURE_FILTER_POINT);
    
    // Вычисляем параметры атласа
    atlas->atlas_width = atlas->texture.width;
    atlas->atlas_height = atlas->texture.height;
    atlas->tiles_per_row = atlas->atlas_width / TEXTURE_SIZE;
    atlas->tiles_per_column = atlas->atlas_height / TEXTURE_SIZE;
    atlas->total_tiles = atlas->tiles_per_row * atlas->tiles_per_column;

    // Выделяем память для UV-координат
    atlas->uvs = (TextureUV*)malloc(sizeof(TextureUV) * atlas->total_tiles);
    if (!atlas->uvs) {
        fprintf(stderr, "Failed to allocate memory for UV coordinates\n");
        UnloadTexture(atlas->texture);
        free(atlas);
        return NULL;
    }

    // Вычисляем UV-координаты для каждого тайла
    for (int y = 0; y < atlas->tiles_per_column; y++) {
        for (int x = 0; x < atlas->tiles_per_row; x++) {
            int index = y * atlas->tiles_per_row + x;
            
            float u_min = (float)(x * TEXTURE_SIZE) / (float)atlas->atlas_width;
            float v_min = (float)(y * TEXTURE_SIZE) / (float)atlas->atlas_height;
            float u_max = (float)((x + 1) * TEXTURE_SIZE) / (float)atlas->atlas_width;
            float v_max = (float)((y + 1) * TEXTURE_SIZE) / (float)atlas->atlas_height;

            atlas->uvs[index].u_min = u_min;
            atlas->uvs[index].v_min = v_min;
            atlas->uvs[index].u_max = u_max;
            atlas->uvs[index].v_max = v_max;
        }
    }

    printf("Texture atlas loaded: %s\n", filepath);
    printf("  Size: %dx%d pixels\n", atlas->atlas_width, atlas->atlas_height);
    printf("  Tiles: %dx%d (%d total)\n", atlas->tiles_per_row, atlas->tiles_per_column, atlas->total_tiles);

    return atlas;
}

// Получить UV-координаты по индексу тайла
TextureUV texture_atlas_get_uv(TextureAtlas* atlas, int tile_index) {
    if (!atlas || tile_index < 0 || tile_index >= atlas->total_tiles) {
        // Возвращаем нулевые UV для невалидных индексов
        TextureUV uv = {0.0f, 0.0f, 0.0f, 0.0f};
        return uv;
    }
    
    return atlas->uvs[tile_index];
}

// Получить UV-координаты по координатам тайла в атласе (x, y)
TextureUV texture_atlas_get_uv_by_coords(TextureAtlas* atlas, int tile_x, int tile_y) {
    if (!atlas || tile_x < 0 || tile_x >= atlas->tiles_per_row || 
        tile_y < 0 || tile_y >= atlas->tiles_per_column) {
        // Возвращаем нулевые UV для невалидных координат
        TextureUV uv = {0.0f, 0.0f, 0.0f, 0.0f};
        return uv;
    }
    
    int index = tile_y * atlas->tiles_per_row + tile_x;
    return atlas->uvs[index];
}

// Освобождение ресурсов атласа
void texture_atlas_destroy(TextureAtlas* atlas) {
    if (!atlas) return;
    
    if (atlas->uvs) {
        free(atlas->uvs);
        atlas->uvs = NULL;
    }
    
    if (atlas->texture.id != 0) {
        UnloadTexture(atlas->texture);
    }
    
    free(atlas);
}
