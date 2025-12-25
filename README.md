# Rayflow

[![Build](https://github.com/ShiftyX1/rayflow/actions/workflows/build.yml/badge.svg)](https://github.com/ShiftyX1/rayflow/actions/workflows/build.yml)
[![Tests](https://github.com/ShiftyX1/rayflow/actions/workflows/tests.yml/badge.svg)](https://github.com/ShiftyX1/rayflow/actions/workflows/tests.yml)

Rayflow это внутренний инструмент команды Pulse Studio для разработки игр с использованием Raylib. Он предоставляет удобную среду для создания, тестирования и отладки игр, а также интеграцию с различными инструментами разработки.

## Статус проекта

Проект находится в активной разработке и используется внутри команды Pulse Studio. Идет активная работа над улучшением функциональности и добавлением новых возможностей.

## Требования

- **CMake**: 3.21+
- **C++ компилятор**: GCC 10+, Clang 13+, или MSVC 2019+
- **Ninja** (рекомендуется) или Make

## Зависимости

Проект использует следующие библиотеки:

| Библиотека | Версия | Описание |
|------------|--------|----------|
| [raylib](https://www.raylib.com/) | 5.5 | Графическая библиотека |
| [EnTT](https://github.com/skypjack/entt) | 3.13.2 | Entity Component System |
| [tinyxml2](https://github.com/leethomason/tinyxml2) | 10.0.0 | XML парсер |

### Автоматическая загрузка зависимостей

EnTT и tinyxml2 загружаются автоматически через CMake FetchContent.

raylib ищется в следующем порядке:
1. Через `find_package(raylib)` (если установлен системный пакет)
2. Через поиск в стандартных путях установки
3. **Автоматическая загрузка через FetchContent** (если не найден)

Это означает, что проект соберётся на чистой системе без предустановленного raylib.

### Установка raylib (опционально, ускоряет сборку)

<details>
<summary><b>macOS (Homebrew)</b></summary>

```bash
brew install raylib
```
</details>

<details>
<summary><b>Ubuntu / Debian</b></summary>

```bash
sudo apt install libraylib-dev
```

Если пакет недоступен или устаревший:
```bash
# Сборка произойдёт автоматически через FetchContent
```
</details>

<details>
<summary><b>Arch Linux</b></summary>

```bash
sudo pacman -S raylib
# или из AUR:
yay -S raylib
```
</details>

<details>
<summary><b>Fedora</b></summary>

```bash
sudo dnf install raylib-devel
```
</details>

<details>
<summary><b>Windows (MSYS2)</b></summary>

```bash
# UCRT64 (рекомендуется)
pacman -S mingw-w64-ucrt-x86_64-raylib

# или MinGW64
pacman -S mingw-w64-x86_64-raylib
```
</details>

### Принудительная загрузка raylib

Если хотите использовать FetchContent вместо системного raylib:

```bash
cmake --preset debug -DRAYFLOW_FETCH_RAYLIB=ON
```

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
