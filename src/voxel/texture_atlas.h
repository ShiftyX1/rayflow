#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <raylib.h>
#include <stdint.h>
#include <stdbool.h>

// Размеры текстуры отдельного блока (пиксели)
#define TEXTURE_SIZE 16

// Структура для UV-координат одной текстуры в атласе
typedef struct {
    float u_min;  // Минимальная U-координата (0.0 - 1.0)
    float v_min;  // Минимальная V-координата (0.0 - 1.0)
    float u_max;  // Максимальная U-координата (0.0 - 1.0)
    float v_max;  // Максимальная V-координата (0.0 - 1.0)
} TextureUV;

// Структура текстурного атласа
typedef struct {
    Texture2D texture;      // Загруженная текстура атласа
    int atlas_width;        // Ширина атласа в пикселях
    int atlas_height;       // Высота атласа в пикселях
    int tiles_per_row;      // Количество тайлов в строке
    int tiles_per_column;   // Количество тайлов в столбце
    int total_tiles;        // Общее количество тайлов
    TextureUV* uvs;         // Массив UV-координат для каждого тайла
} TextureAtlas;

// Создание текстурного атласа из файла
TextureAtlas* texture_atlas_create(const char* filepath);

// Получить UV-координаты по индексу тайла
TextureUV texture_atlas_get_uv(TextureAtlas* atlas, int tile_index);

// Получить UV-координаты по координатам тайла в атласе (x, y)
TextureUV texture_atlas_get_uv_by_coords(TextureAtlas* atlas, int tile_x, int tile_y);

// Освобождение ресурсов атласа
void texture_atlas_destroy(TextureAtlas* atlas);

#endif // TEXTURE_ATLAS_H
