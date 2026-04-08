# Настройка GitHub Actions для Jacal

## 1. Создание репозитория

```bash
cd /path/to/Jacal
git init
git add .
git commit -m "Initial commit"
```

На GitHub: **New repository** → имя `Jacal` → **Create**.

```bash
git remote add origin https://github.com/YOUR_USERNAME/Jacal.git
git branch -M main
git push -u origin main
```

## 2. Что делает pipeline

Файл `.github/workflows/build.yml` запускается автоматически при каждом `push` и `pull request`.

### Этапы:

| Этап | Что делает | Зависит от |
|------|-----------|------------|
| **test-core** | Компилирует и запускает `tests/test_core.cpp` — ~200 проверок ядра игры | — |
| **build-linux-gui** | Собирает Qt6 QML версию для Linux | test-core |
| **build-linux-console** | Собирает консольную Linux версию + smoke test | test-core |
| **build-windows-console** | Кросс-компилирует Windows .exe через MinGW | test-core |

Сборки запускаются **только если тесты прошли**. Если тест упал — сборки не начинаются.

## 3. Где смотреть результаты

1. Откройте репозиторий на GitHub
2. Вкладка **Actions**
3. Последний workflow run → зелёная галочка = всё ок, красный крест = есть ошибки
4. Клик на упавший тест → логи с описанием что именно сломалось

## 4. Скачивание артефактов

После успешной сборки:
1. Actions → выберите run → **Artifacts** внизу страницы
2. Скачайте: `jackal-linux-gui`, `jackal-linux-console`, `jackal-win64`

## 5. Бейдж в README

Добавьте в начало `README.md`:

```markdown
![Build](https://github.com/YOUR_USERNAME/Jacal/actions/workflows/build.yml/badge.svg)
```

Бейдж автоматически показывает статус: зелёный (passing) или красный (failing).

## 6. Локальный запуск тестов

Без GitHub, на любой машине с g++:

```bash
g++ -std=c++17 -O2 -I core core/*.cpp tests/test_core.cpp -o test_core
./test_core
```

Ожидаемый результат:
```
Jacal Core Test Suite
=====================

=== Maps ===
=== Deck ===
=== Init ===
...
=====================
PASSED: XXX  FAILED: 0
=====================
```

## 7. Утилита dump_board

Для создания новых детерминированных тестов:

```bash
g++ -std=c++17 -O2 -I core core/*.cpp tools/dump_board.cpp -o dump_board
./dump_board --seed 42 --map classic
./dump_board --seed 1 --map duel --teams 2
```

Показывает полную раскладку поля: какой тайл на какой позиции.

## 8. Добавление новых тестов

Откройте `tests/test_core.cpp`. Добавьте новую функцию:

```cpp
static void testMyNewFeature() {
    SECTION("My New Feature");
    auto g = makeGame("classic", 2, 42);
    // ... настройка ...
    CHECK(condition, "описание проверки");
    CHECK_EQ(actual, expected, "что сравниваем");
}
```

Вызовите её из `main()`:
```cpp
testMyNewFeature();
```

Макросы:
- `CHECK(условие, сообщение)` — проверяет что условие истинно
- `CHECK_EQ(a, b, сообщение)` — проверяет a == b, при ошибке показывает оба значения
- `SECTION("имя")` — задаёт имя секции для вывода ошибок
