#include "block_registry.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Глобальный реестр блоков
BlockRegistry g_block_registry = {0};

// Вспомогательная функция для создания текстур с одной текстурой для всех сторон
BlockTextures block_textures_all(int texture_index) {
    BlockTextures tex = {0};
    tex.top = texture_index;
    tex.bottom = texture_index;
    tex.side = texture_index;
    tex.north = texture_index;
    tex.south = texture_index;
    tex.east = texture_index;
    tex.west = texture_index;
    tex.use_individual_sides = false;
    return tex;
}

// Вспомогательная функция для создания текстур с разными top/bottom/side
BlockTextures block_textures_top_bottom_side(int top, int bottom, int side) {
    BlockTextures tex = {0};
    tex.top = top;
    tex.bottom = bottom;
    tex.side = side;
    tex.north = side;
    tex.south = side;
    tex.east = side;
    tex.west = side;
    tex.use_individual_sides = false;
    return tex;
}

// Вспомогательная функция для создания текстур со всеми индивидуальными сторонами
BlockTextures block_textures_individual(int top, int bottom, int north, int south, int east, int west) {
    BlockTextures tex = {0};
    tex.top = top;
    tex.bottom = bottom;
    tex.north = north;
    tex.south = south;
    tex.east = east;
    tex.west = west;
    tex.side = north; // Запасное значение
    tex.use_individual_sides = true;
    return tex;
}

// Регистрация блока
void block_registry_register(BlockType type, const char* name, 
                             bool is_solid, bool is_transparent, bool is_opaque,
                             bool is_liquid, float light_emission,
                             BlockTextures textures) {
    if (type >= BLOCK_COUNT) {
        fprintf(stderr, "Error: Invalid block type %d\n", type);
        return;
    }

    BlockProperties* props = &g_block_registry.blocks[type];
    props->type = type;
    props->name = name;
    props->is_solid = is_solid;
    props->is_transparent = is_transparent;
    props->is_opaque = is_opaque;
    props->is_liquid = is_liquid;
    props->light_emission = light_emission;
    props->textures = textures;
}

// Инициализация реестра блоков
bool block_registry_init(const char* atlas_path) {
    if (g_block_registry.is_initialized) {
        fprintf(stderr, "Warning: Block registry already initialized\n");
        return true;
    }

    // Загружаем текстурный атлас
    g_block_registry.atlas = texture_atlas_create(atlas_path);
    if (!g_block_registry.atlas) {
        fprintf(stderr, "Error: Failed to load texture atlas\n");
        return false;
    }

    // Инициализируем все блоки как воздух
    memset(g_block_registry.blocks, 0, sizeof(g_block_registry.blocks));

    // Регистрируем блоки (примерная раскладка атласа - нужно будет скорректировать по реальному атласу)
    // Индексы рассчитываются как: y * tiles_per_row + x
    
    // Воздух - невидимый блок
    block_registry_register(BLOCK_AIR, "Air", false, true, false, false, 0.0f, 
                           block_textures_all(0));
    
    // Трава - верх, низ и бока разные
    block_registry_register(BLOCK_GRASS, "Grass", true, false, true, false, 0.0f,
                           block_textures_top_bottom_side(0, 2, 3));
    
    // Земля - одинаковая со всех сторон
    block_registry_register(BLOCK_DIRT, "Dirt", true, false, true, false, 0.0f,
                           block_textures_all(2));
    
    // Камень
    block_registry_register(BLOCK_STONE, "Stone", true, false, true, false, 0.0f,
                           block_textures_all(1));
    
    // Песок
    block_registry_register(BLOCK_SAND, "Sand", true, false, true, false, 0.0f,
                           block_textures_all(18));
    
    // Вода - прозрачная
    block_registry_register(BLOCK_WATER, "Water", false, true, false, true, 0.0f,
                           block_textures_all(5));
    
    // Дерево - верх/низ и бока разные
    block_registry_register(BLOCK_WOOD, "Wood", true, false, true, false, 0.0f,
                           block_textures_top_bottom_side(6, 6, 7));
    
    // Листва - полупрозрачная
    block_registry_register(BLOCK_LEAVES, "Leaves", true, true, false, false, 0.0f,
                           block_textures_all(8));
    
    // Булыжник
    block_registry_register(BLOCK_COBBLESTONE, "Cobblestone", true, false, true, false, 0.0f,
                           block_textures_all(9));
    
    // Доски
    block_registry_register(BLOCK_PLANKS, "Planks", true, false, true, false, 0.0f,
                           block_textures_all(4));
    
    // Стекло - прозрачное
    block_registry_register(BLOCK_GLASS, "Glass", true, true, false, false, 0.0f,
                           block_textures_all(11));
    
    // Кирпич
    block_registry_register(BLOCK_BRICK, "Brick", true, false, true, false, 0.0f,
                           block_textures_all(12));

    g_block_registry.is_initialized = true;
    
    printf("Block registry initialized with %d blocks\n", BLOCK_COUNT);
    return true;
}

// Получить свойства блока по типу
BlockProperties* block_registry_get(BlockType type) {
    if (type >= BLOCK_COUNT) {
        return &g_block_registry.blocks[BLOCK_AIR];
    }
    return &g_block_registry.blocks[type];
}

// Получить UV-координаты для конкретной стороны блока
// face_index: 0=top, 1=bottom, 2=north, 3=south, 4=east, 5=west
TextureUV block_registry_get_texture_uv(BlockType type, int face_index) {
    if (!g_block_registry.is_initialized || !g_block_registry.atlas) {
        TextureUV empty = {0};
        return empty;
    }

    BlockProperties* props = block_registry_get(type);
    int texture_index = 0;

    // Выбираем нужную текстуру в зависимости от стороны
    if (props->textures.use_individual_sides) {
        switch (face_index) {
            case 0: texture_index = props->textures.top; break;
            case 1: texture_index = props->textures.bottom; break;
            case 2: texture_index = props->textures.north; break;
            case 3: texture_index = props->textures.south; break;
            case 4: texture_index = props->textures.east; break;
            case 5: texture_index = props->textures.west; break;
            default: texture_index = props->textures.side; break;
        }
    } else {
        switch (face_index) {
            case 0: texture_index = props->textures.top; break;
            case 1: texture_index = props->textures.bottom; break;
            default: texture_index = props->textures.side; break;
        }
    }

    return texture_atlas_get_uv(g_block_registry.atlas, texture_index);
}

// Освобождение ресурсов реестра
void block_registry_destroy(void) {
    if (!g_block_registry.is_initialized) {
        return;
    }

    if (g_block_registry.atlas) {
        texture_atlas_destroy(g_block_registry.atlas);
        g_block_registry.atlas = NULL;
    }

    g_block_registry.is_initialized = false;
    printf("Block registry destroyed\n");
}
