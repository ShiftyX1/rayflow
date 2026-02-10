# Rayflow Lighting Systems Overview

## TL;DR

Rayflow использует **две независимые системы освещения**, которые работают вместе:

1. **Minecraft-style Voxel Lighting** (`BlockType::Light`, blocklight/skylight) - Дискретное освещение на основе блоков
2. **Dynamic Shader-based Lighting** (`PointLight`) - Динамическое 3D освещение для игроков и объектов

Эти системы **не заменяют друг друга** и работают параллельно для достижения оптимального баланса между производительностью и визуальным качеством.

---

## 1. Minecraft-style Voxel Lighting System

### Описание

Детерминированная система освещения на основе блоков, аналогичная Minecraft. Каждый блок в мире имеет два значения освещённости (0-15):
- **Skylight** (небесный свет) - свет от неба, проходящий сверху вниз
- **Blocklight** (блочный свет) - свет от излучающих блоков (факелов, лавы и т.д.)

### Ключевые компоненты

#### `BlockType::Light` (block.hpp)
```cpp
enum class BlockType : std::uint8_t {
    // ...
    // LS-1: map/editor light marker block. Must stay stable (append-only).
    Light,
    // ...
};
```

**Назначение:**
- Тип блока, который используется как **маркер источника света** в редакторе карт
- Излучает максимальное значение света (15) в системе blocklight
- Не имеет коллизии (игроки проходят сквозь него)
- Видим как невидимый/прозрачный блок

**Свойства освещения:**
```cpp
// Light (LS-1)
BlockLightProps{ 15u, 0u, 0u, false, false }
//               ^    ^   ^    ^      ^
//               |    |   |    |      |
//         emission |   |    |      skyDimVertical
//              blockAtt |    opaqueForLight
//                   skyAtt
```

#### Применение
- Статическое освещение мира
- Предварительно размещённые источники света в картах
- Детерминированное освещение (одинаковое на всех клиентах)
- **Нулевая нагрузка на сервер** - расчёт происходит локально на клиенте на основе блоков

### Алгоритм

Использует BFS (Breadth-First Search) propagation:
1. Источники света (блоки с `emission > 0`) инициализируют очередь
2. Свет распространяется по соседним блокам, уменьшаясь на 1 каждый шаг
3. Непрозрачные блоки (`opaqueForLight = true`) блокируют распространение
4. Результат сохраняется в `skylight_` и `blocklight_` массивах чанка

### Файлы
- `engine/modules/voxel/shared/block.hpp` - Определение BlockType и BlockLightProps
- `engine/modules/voxel/client/light_volume.cpp` - Алгоритм распространения света
- `engine/modules/voxel/client/chunk.cpp` - Хранение и использование значений освещённости

---

## 2. Dynamic Shader-based Lighting System

### Описание

Динамическая система 3D освещения, основанная на point lights в шейдерах. Используется для подвижных источников света (игроки, снаряды, динамические объекты).

### Ключевые компоненты

#### `PointLight` struct (world.hpp)
```cpp
struct PointLight {
    Vector3 position;   // 3D позиция в мировых координатах
    Vector3 color;      // RGB цвет света (0.0-1.0)
    float radius;       // Радиус действия света
    float intensity;    // Интенсивность
};
```

**Назначение:**
- Динамические источники света, привязанные к игрокам/объектам
- Обновляются каждый кадр
- Передаются в shader для real-time расчёта освещения
- Используют distance attenuation (затухание по расстоянию)

#### Применение
- Освещение вокруг игрока (как факел в руке)
- Освещение других игроков на сервере
- Временные эффекты (взрывы, магия, выстрелы)
- Декоративные источники света, которые не должны быть блоками

### Алгоритм

Рендеринг в fragment shader (`voxel.fs`):
1. Для каждого пикселя вычисляется расстояние до всех PointLight источников
2. Если расстояние < radius, добавляется вклад света
3. Использует smoothstep для плавного затухания
4. Учитывается normal surface для диффузного освещения

### Файлы
- `engine/modules/voxel/client/world.hpp` - Определение PointLight и хранение scene_lights_
- `engine/modules/voxel/client/world.cpp` - Передача lights в shader
- `games/bedwars/resources/shaders/voxel.fs` - Shader код для расчёта освещения
- `games/bedwars/client/bedwars_client.cpp` - Создание и управление динамическими светами

---

## Сравнение систем

| Характеристика | Minecraft Voxel Lighting | Dynamic Point Lights |
|----------------|--------------------------|----------------------|
| **Источник** | `BlockType::Light` и другие блоки | `PointLight` struct |
| **Тип данных** | uint8_t (0-15), дискретный | float (0.0-∞), непрерывный |
| **Расчёт** | BFS propagation, per-chunk | Per-pixel в shader |
| **Частота обновления** | При изменении блоков | Каждый кадр |
| **Стоимость CPU** | Средняя (при изменениях) | Низкая (только передача данных) |
| **Стоимость GPU** | Низкая (vertex colors) | Средняя (per-pixel расчёт) |
| **Детерминизм** | Да (одинаковый на всех клиентах) | Нет (локальный расчёт) |
| **Использование** | Статическое освещение мира | Динамические объекты |
| **Максимум источников** | Неограниченно (блоки) | 32 (MAX_LIGHTS в shader) |
| **Тени** | Неявные (через propagation) | Нет |

---

## Как они работают вместе

### В Fragment Shader (voxel.fs)

```glsl
// 1. Базовое освещение от skylight (Minecraft система)
float skylightFactor = max(fragSkyLight, minAmbient);
vec3 lighting = (ambientColor + sunColor * sunDiff) * skylightFactor;

// 2. Добавление динамических point lights
for (int i = 0; i < lightCount; i++) {
    PointLight light = lights[i];
    // ... расчёт attenuation и добавление к lighting ...
    lighting += light.color * light.intensity * diff * att;
}

// 3. Применение Ambient Occlusion (Minecraft система)
vec3 finalRGB = tintedColor * lighting * ao;
```

### Визуально

**Minecraft Lighting** обеспечивает:
- Базовое освещение всего мира
- Плавные переходы между освещёнными и тёмными областями
- Тени от блоков
- Ambient Occlusion на углах

**Point Lights** добавляют:
- "Ореол" света вокруг игроков
- Динамические световые эффекты
- Цветное освещение (например, синий свет от магии)
- Локальное выделение объектов

---

## Примеры использования

### Пример 1: Игрок с факелом

```cpp
// Minecraft система: факел как блок в мире
world.set_block(x, y, z, BlockType::Light);
// → Распространяет blocklight 15 → 14 → 13... → 0

// Dynamic система: свет вокруг игрока
PointLight player_light{
    .position = player.position,
    .color = {1.0f, 0.9f, 0.7f},  // тёплый жёлтый
    .radius = 12.0f,
    .intensity = 1.0f
};
world.set_lights({player_light});
// → Shader добавляет динамический "ореол" вокруг игрока
```

**Результат:**
- Если игрок стоит в тёмной пещере с факелом в руке:
  - `BlockType::Light` блоки в стенах освещают статичное окружение
  - `PointLight` от игрока добавляет дополнительное освещение, которое двигается с ним

### Пример 2: Подземелье

```cpp
// Статическое освещение: факелы на стенах (Minecraft)
for (auto torch_pos : torch_positions) {
    world.set_block(torch_pos.x, torch_pos.y, torch_pos.z, BlockType::Light);
}

// Динамическое освещение: игроки и мобы (PointLight)
std::vector<PointLight> dynamic_lights;
for (auto& player : players) {
    dynamic_lights.push_back({
        .position = player.position,
        .color = {1.0f, 0.9f, 0.7f},
        .radius = 10.0f,
        .intensity = 0.8f
    });
}
world.set_lights(dynamic_lights);
```

---

## Планы на будущее

### Интеграция систем (Potential)

В будущем возможна более глубокая интеграция:

1. **BlockType::Light → PointLight автоконверсия**
   - Автоматически создавать PointLight для всех `BlockType::Light` блоков
   - Даст плавное real-time освещение от статичных источников

2. **Гибридный подход**
   - Использовать Minecraft lighting для базы
   - PointLights для "hot spots" и динамики
   - Смешивание в shader для бесшовного перехода

3. **Унификация данных**
   - Единый формат для описания световых свойств
   - Переиспользование BlockLightProps для настройки PointLight

---

## FAQ

### Почему две системы, а не одна?

**Производительность и качество:**
- Minecraft система очень эффективна для статического мира (расчёт только при изменениях)
- PointLight система даёт высокое визуальное качество для динамических объектов
- Комбинация даёт лучший результат, чем любая система в отдельности

### Можно ли использовать только PointLight?

Технически да, но:
- Нужен PointLight для каждого источника света → сотни/тысячи объектов
- Shader ограничен 32 lights максимум
- Огромная нагрузка на GPU для расчёта всех источников каждый кадр
- Нет теней и ambient occlusion

### Можно ли использовать только Minecraft lighting?

Да, и многие игры так и делают. Но:
- Динамическое освещение игроков потребует обновления блоков каждый кадр
- Нагрузка на сервер (синхронизация света)
- Менее плавное и естественное освещение для движущихся объектов

### Как BlockType::Light отличается от обычного блока?

```cpp
// Обычный непрозрачный блок (камень)
BlockLightProps{ 0u, 0u, 0u, true, false }
//               ^^emission = 0 (не светится)
//                           ^^^^opaqueForLight = true (блокирует свет)

// BlockType::Light
BlockLightProps{ 15u, 0u, 0u, false, false }
//               ^^^emission = 15 (максимальный свет)
//                            ^^^^^opaqueForLight = false (пропускает свет)
```

### Где используется BlockType::Light?

1. **Map Editor** - размещение статических источников света в картах
2. **Decorative Lighting** - невидимые источники света для подсветки областей
3. **Predictable Lighting** - гарантия одинакового освещения на всех клиентах

---

## Дополнительные ресурсы

- `docs/lighting-implementation-plan.md` - План реализации Minecraft lighting
- `engine/modules/voxel/shared/block.hpp` - BlockType и BlockLightProps
- `engine/modules/voxel/client/world.hpp` - PointLight и управление динамическим освещением
- `games/bedwars/resources/shaders/voxel.fs` - Shader код для комбинированного освещения
