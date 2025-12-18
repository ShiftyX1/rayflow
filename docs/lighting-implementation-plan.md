# Minecraft-style Lighting: Implementation Plan

## Executive Summary

Цель: детерминированное Minecraft-style освещение (0..15 block + sky), единое у всех клиентов.

**Принятое решение:** клиентский расчёт по детерминированному алгоритму на основе авторитетных блоков сервера (не серверный расчёт + репликация).

Преимущества:
- 0 CPU нагрузка на сервере
- 0 дополнительного трафика
- При одинаковых блоках — одинаковый результат у всех клиентов

---

## Phase 0: Block Light Properties (shared/)

**Файл:** `shared/voxel/block.hpp`

### Что добавить

```cpp
// Lighting properties per block type
struct BlockLightProps {
    uint8_t emission;        // 0..15, light emitted by this block
    uint8_t blockAttenuation;// extra attenuation for block light (0 = glass-like, 255 = opaque)
    uint8_t skyAttenuation;  // extra attenuation for sky light
    bool opaqueForLight;     // true = blocks all light propagation
    bool skyDimVertical;     // true = leaves/cobweb/water behavior (sky dims 1 per block down)
};

constexpr BlockLightProps BLOCK_LIGHT_PROPS[static_cast<size_t>(BlockType::Count)] = {
    // Air
    { .emission = 0, .blockAttenuation = 0, .skyAttenuation = 0, .opaqueForLight = false, .skyDimVertical = false },
    // Stone
    { .emission = 0, .blockAttenuation = 255, .skyAttenuation = 255, .opaqueForLight = true, .skyDimVertical = false },
    // ... (all block types)
    // Leaves
    { .emission = 0, .blockAttenuation = 0, .skyAttenuation = 0, .opaqueForLight = false, .skyDimVertical = true },
    // Water
    { .emission = 0, .blockAttenuation = 0, .skyAttenuation = 0, .opaqueForLight = false, .skyDimVertical = true },
    // Light
    { .emission = 15, .blockAttenuation = 0, .skyAttenuation = 0, .opaqueForLight = false, .skyDimVertical = false },
};

inline const BlockLightProps& get_light_props(BlockType bt) {
    return BLOCK_LIGHT_PROPS[static_cast<size_t>(bt)];
}
```

### Acceptance
- [x] Все существующие BlockType имеют запись в таблице
- [x] Нет runtime lookup / std::map — constexpr array
- [x] Свойства соответствуют Minecraft-wiki (glass = no penalty, leaves = skyDimVertical)

---

## Phase 1: LightVolume Refactor (client/voxel/)

**Файл:** `client/voxel/light_volume.cpp`, `light_volume.hpp`

### 1.1 Использовать shared BlockLightProps

Заменить `is_opaque_for_light()` на `get_light_props(bt).opaqueForLight`.

### 1.2 Учитывать attenuation в BFS

Текущий код:
```cpp
const std::uint8_t next = static_cast<std::uint8_t>(n.level - 1u);
```

Новый код:
```cpp
const auto& props = shared::voxel::get_light_props(target_block_type);
if (props.opaqueForLight) return; // stop propagation
const uint8_t cost = 1 + props.blockAttenuation; // for blocklight
const uint8_t next = (n.level > cost) ? (n.level - cost) : 0;
```

### 1.3 Sky light vertical dimming (leaves/cobweb/water)

При распространении skylight **вниз** через блок с `skyDimVertical = true`:
- Уровень уменьшается на 1 (как и при горизонтальном spread)

При распространении вниз через обычный transparent:
- Уровень **не** уменьшается (skylight "падает" вниз без потерь)

Текущий код уже делает "downward keeps same level", но надо добавить проверку:
```cpp
uint8_t level_down = n.level;
if (target_props.skyDimVertical) {
    level_down = (n.level > 0) ? (n.level - 1) : 0;
}
```

### 1.4 Emission seeds

Заменить хардкод `bt == BlockType::Light` на:
```cpp
const auto& props = shared::voxel::get_light_props(bt);
if (props.emission > 0) {
    blocklight_[i] = props.emission;
    q_blk_.push_back(...);
}
```

### Acceptance Phase 1
- [ ] Glass/iron bars: не добавляют attenuation
- [ ] Leaves/water: skylight уменьшается вертикально
- [ ] Torch (emission=14) создаёт правильный ромбовидный паттерн
- [ ] Диагональ на плоскости: 14 -> 13 -> 12 (по Манхэттену)

---

## Phase 2: Incremental Relight (client/voxel/)

Текущий код делает **full rebuild** при изменении origin. Для изменений блоков нужен **инкрементальный relight**.

### 2.1 API для block changes

```cpp
class LightVolume {
public:
    // Called when a block changes at world position (wx, wy, wz).
    // Queues incremental relight work.
    void notify_block_changed(int wx, int wy, int wz, BlockType old_type, BlockType new_type);

    // Process queued relight work (call once per frame or per N ms).
    // budget_nodes: max BFS nodes to process this call.
    void process_pending_relight(const World& world, int budget_nodes = 4096);

private:
    struct RelightTask {
        int wx, wy, wz;
        bool increase; // true = light may increase, false = light may decrease
    };
    std::vector<RelightTask> pending_relight_;
};
```

### 2.2 Decrease propagation

Когда источник удалён или блок стал opaque:
1. Сохранить старый уровень света в позиции
2. BFS-decrease: обнулить все клетки, которые получали свет от этого источника
3. После decrease — BFS-increase от соседей, у которых остался свет

Это стандартный Minecraft relight algorithm.

### 2.3 Integration with World::set_block

```cpp
void World::set_block(int x, int y, int z, Block type) {
    Block old_type = get_block(x, y, z);
    // ... actual set ...
    light_volume_.notify_block_changed(x, y, z, old_type, type);
}
```

### Acceptance Phase 2
- [ ] Установка факела: свет появляется за 1-2 кадра
- [ ] Удаление факела: свет исчезает корректно
- [ ] Постановка opaque блока: тень появляется
- [ ] Удаление opaque блока: свет "заливает" через дыру
- [ ] Нет full rebuild при каждом изменении блока

---

## Phase 3: Chunk-local Light Storage (optimization)

Текущий `LightVolume` хранит один bounded volume вокруг камеры. Для больших миров лучше хранить свет per-chunk.

### 3.1 Light data in Chunk

```cpp
class Chunk {
    // ...
    std::array<uint8_t, CHUNK_SIZE> skylight_{};   // 4 bits would suffice, but uint8 is faster
    std::array<uint8_t, CHUNK_SIZE> blocklight_{};
    bool light_dirty_{true};
};
```

### 3.2 Cross-chunk propagation

При relight нужно учитывать соседние чанки:
- Читать свет из соседей при BFS на границе
- Пушить relight tasks в соседние чанки при overflow

### 3.3 Initial chunk light

При генерации/загрузке чанка:
1. Seeding skylight сверху (y=255 -> down)
2. Seeding blocklight от emissive блоков
3. BFS propagation

### Acceptance Phase 3
- [ ] Свет корректен на границах чанков
- [ ] Нет "тёмных швов" между чанками
- [ ] Память: ~8 KB на чанк (2 × 4096 bytes)

---

## Phase 4: Mesh Generation Integration

**Файл:** `client/voxel/chunk.cpp` — `generate_mesh()`

### 4.1 Vertex lighting

Текущий код вызывает `world.sample_light01(x, y, z)`. После Phase 1-3 это должно работать автоматически.

Проверить, что:
- Каждая вершина получает свет от соседнего блока (не от блока внутри solid)
- Используется face-aware sampling (свет берётся из воздуха перед гранью)

### 4.2 Smooth lighting (optional, Phase 4b)

Minecraft-style smooth lighting: усреднение 4 углов грани.
- Требует sampling 4 соседних light values
- Интерполяция в vertex color

### Acceptance Phase 4
- [ ] Блоки затенены корректно
- [ ] Нет "плоского" освещения (видны градиенты)
- [ ] Optional: smooth lighting без резких границ

---

## Phase 5: Brightness Curve & Time-of-Day

### 5.1 Brightness function

```cpp
// In client/voxel/ or client/renderer/
float compute_brightness(uint8_t combined_light, float time_of_day, float gamma) {
    // Minecraft-style curve (approximation)
    // time_of_day: 0.0 = midnight, 0.5 = noon
    float base = static_cast<float>(combined_light) / 15.0f;
    
    // Nonlinear curve (Minecraft uses pow-like)
    float curved = std::pow(base, gamma);
    
    // Time-of-day modulates the floor (minimum brightness)
    float min_brightness = lerp(0.05f, 0.0f, std::abs(time_of_day - 0.5f) * 2.0f);
    
    return std::max(curved, min_brightness);
}
```

### 5.2 Server-synced time

Сервер отправляет `worldTime` (или `tickCount`). Клиент вычисляет `time_of_day` из этого.

### Acceptance Phase 5
- [ ] Ночью мир темнее, но skylight values не меняются
- [ ] Факелы "оранжевее" при низком свете (optional tint)
- [ ] time_of_day синхронизирован между клиентами

---

## File Change Summary

| Phase | File | Change |
|-------|------|--------|
| 0 | `shared/voxel/block.hpp` | Add `BlockLightProps`, `get_light_props()` |
| 1 | `client/voxel/light_volume.cpp` | Use `BlockLightProps` in BFS, skyDimVertical |
| 2 | `client/voxel/light_volume.hpp/cpp` | Add incremental relight API |
| 2 | `client/voxel/world.cpp` | Call `notify_block_changed()` |
| 3 | `client/voxel/chunk.hpp/cpp` | Add per-chunk light storage (optional) |
| 4 | `client/voxel/chunk.cpp` | Verify vertex light sampling |
| 5 | `client/renderer/` or `shared/` | Add brightness curve |
| 5 | `shared/protocol/messages.hpp` | Add `worldTime` to snapshots (if not present) |

---

## Testing Checklist

### Determinism test
1. Два клиента подключаются к одному серверу
2. Оба видят одинаковый мир
3. Один ставит/ломает блоки
4. Проверить: освещение идентично на обоих клиентах (screenshot diff)

### Performance test
1. Rapid block changes (10+ per second)
2. CPU usage не должна spike-ить
3. FPS не падает ниже 60

### Edge cases
- [ ] Блок на границе loaded/unloaded чанка
- [ ] Torch под водой (water attenuates blocklight?)
- [ ] Vertical shaft of leaves (skylight dims correctly)
- [ ] Light source at world height limit

---

## Estimated Effort

| Phase | Complexity | Time |
|-------|------------|------|
| 0 | Low | 1-2 hours |
| 1 | Medium | 3-4 hours |
| 2 | High | 6-8 hours |
| 3 | Medium | 4-6 hours |
| 4 | Low | 1-2 hours |
| 5 | Low | 2-3 hours |

**Total: ~17-25 hours** of focused implementation.

Phase 0-1 можно сделать первыми — они сразу улучшат визуал и не требуют много рефакторинга.

---

## Dependencies

```
Phase 0 ──┬──> Phase 1 ──> Phase 4
          │
          └──> Phase 2 ──> Phase 3
                              │
Phase 5 (independent) ────────┘
```

Phase 5 (brightness curve) можно делать параллельно с остальными.
