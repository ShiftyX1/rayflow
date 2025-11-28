#ifndef BLOCK_REGISTRY_H
#define BLOCK_REGISTRY_H

#include "texture_atlas.h"
#include <stdint.h>
#include <stdbool.h>

// Типы блоков (ID)
typedef enum {
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_SAND,
    BLOCK_WATER,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_COBBLESTONE,
    BLOCK_PLANKS,
    BLOCK_GLASS,
    BLOCK_BRICK,
    BLOCK_COUNT
} BlockType;

// Индексы текстур для разных сторон блока
typedef struct {
    int top;      // Верхняя грань
    int bottom;   // Нижняя грань
    int side;     // Боковые грани (north, south, east, west)
    int north;    // Северная грань (опционально, если отличается от side)
    int south;    // Южная грань
    int east;     // Восточная грань
    int west;     // Западная грань
    bool use_individual_sides; // Использовать индивидуальные текстуры для сторон
} BlockTextures;

// Свойства блока
typedef struct {
    BlockType type;
    const char* name;
    bool is_solid;        // Твердый блок (коллизия)
    bool is_transparent;  // Прозрачный блок
    bool is_opaque;       // Непрозрачный (блокирует свет)
    bool is_liquid;       // Жидкость
    float light_emission; // Излучение света (0.0 - 1.0)
    BlockTextures textures; // Индексы текстур в атласе
} BlockProperties;

// Реестр блоков
typedef struct {
    BlockProperties blocks[BLOCK_COUNT];
    TextureAtlas* atlas;
    bool is_initialized;
} BlockRegistry;

// Глобальный реестр блоков
extern BlockRegistry g_block_registry;

// Инициализация реестра блоков
bool block_registry_init(const char* atlas_path);

// Регистрация блока
void block_registry_register(BlockType type, const char* name, 
                             bool is_solid, bool is_transparent, bool is_opaque,
                             bool is_liquid, float light_emission,
                             BlockTextures textures);

// Получить свойства блока по типу
BlockProperties* block_registry_get(BlockType type);

// Получить UV-координаты для конкретной стороны блока
TextureUV block_registry_get_texture_uv(BlockType type, int face_index);

// Освобождение ресурсов реестра
void block_registry_destroy(void);

// Вспомогательная функция для создания текстур с одной текстурой для всех сторон
BlockTextures block_textures_all(int texture_index);

// Вспомогательная функция для создания текстур с разными top/bottom/side
BlockTextures block_textures_top_bottom_side(int top, int bottom, int side);

// Вспомогательная функция для создания текстур со всеми индивидуальными сторонами
BlockTextures block_textures_individual(int top, int bottom, int north, int south, int east, int west);

#endif // BLOCK_REGISTRY_H
