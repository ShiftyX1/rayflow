# Rayflow

[![Build](https://github.com/ShiftyX1/rayflow/actions/workflows/build.yml/badge.svg)](https://github.com/ShiftyX1/rayflow/actions/workflows/build.yml)
[![Tests](https://github.com/ShiftyX1/rayflow/actions/workflows/tests.yml/badge.svg)](https://github.com/ShiftyX1/rayflow/actions/workflows/tests.yml)

Rayflow — внутренний игровой движок команды Pulse Studio. Построен на GLFW (окно/ввод) + OpenGL 4.x (рендеринг) + glm (математика). Предоставляет ECS-архитектуру, воксельный рендерер, сетевой мультиплеер (клиент-сервер), UI-фреймворк и редактор карт.

## Статус проекта

Проект находится в активной разработке и используется внутри команды Pulse Studio. Идет активная работа над улучшением функциональности и добавлением новых возможностей.

## Требования

- **CMake**: 3.21+
- **C++ компилятор**: GCC 10+, Clang 13+, или MSVC 2019+
- **Ninja** (рекомендуется) или Make

## Зависимости

Все зависимости загружаются автоматически через CMake FetchContent — проект собирается на чистой системе без предустановки.

| Библиотека | Версия | Описание |
|------------|--------|----------|
| [GLFW](https://www.glfw.org/) | 3.4 | Окно, ввод, контекст OpenGL |
| [glad](https://github.com/Dav1dde/glad) | — | OpenGL 4.1 core loader |
| [glm](https://github.com/g-truc/glm) | 1.0.1 | Математика (vec, mat, quat) |
| [stb](https://github.com/nothings/stb) | — | Загрузка изображений и шрифтов |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.8 | Debug UI |
| [EnTT](https://github.com/skypjack/entt) | 3.13.2 | Entity Component System |
| [tinyxml2](https://github.com/leethomason/tinyxml2) | 10.0.0 | XML парсер |
| [ENet](https://github.com/lsalzman/enet) | 1.3.18 | Сетевой транспорт (UDP) |
| [LuaJIT](https://luajit.org/) | 2.1 | Скриптинг |
| [sol2](https://github.com/ThePhD/sol2) | — | C++ биндинги для Lua |

## Сборка

### Быстрый старт

```bash
# Конфигурация (Debug)
cmake --preset debug

# Сборка
cmake --build --preset debug

# Запуск тестов
ctest --preset debug
```

### Пресеты сборки

| Пресет | Описание |
|--------|----------|
| `debug` | Debug сборка с тестами |
| `release` | Release сборка с оптимизациями |

### Сборка отдельных компонентов

```bash
# Только основной клиент
cmake --build --preset debug --target rayflow

# Только редактор карт
cmake --build --preset debug --target map_builder

# Только выделенный сервер
cmake --build --preset debug --target rfds
```

## Структура проекта

```
rayflow/
├── app/           # Точки входа (main.cpp, etc.)
├── client/        # Клиентский код (рендеринг, ввод, UI)
├── server/        # Серверный код (симуляция, правила)
├── shared/        # Общий код (протокол, типы)
├── ui/            # UI фреймворк (XML + CSS-lite)
├── tests/         # Тесты
├── cmake/         # CMake модули
└── build/         # Директория сборки
    ├── debug/
    └── release/
```

## Документация

Подробная документация доступна в директории `documentation/docs/`.
