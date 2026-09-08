# TagBuilder

## Русская версия

TagBuilder — настольное приложение для создания и массового редактирования тегов объектов, в основном для клапанов и подобных устройств.

### Возможности
- Добавление объектов по типу, номеру и индексу
- Поиск и фильтрация списка объектов
- Массовое редактирование тегов
- Вставка списков из буфера обмена
- Работа только с выделенными строками
- Экспорт в CSV
- Поддержка горячих клавиш
- Интерфейс на русском языке

### Требования
- Windows 10+
- Qt 6.8.3
- MinGW 64-bit
- CMake 3.16+

### Сборка
Из корня проекта выполните:

```bat
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64"
cmake --build build-mingw --config Release
```

Или используйте готовый запускатель:

```bat
TagBuilder.cmd
```

### Запуск
Готовый исполняемый файл:

```text
build-mingw\TagBuilder.exe
```

### GitHub Release
В релизе также доступен готовый архив:

```text
TagBuilder-release.zip
```

---

## English version

TagBuilder is a desktop utility for creating and bulk-editing object tags, mainly for valves and similar devices.

### Features
- Add objects by type, number, and index
- Search and filter object lists
- Bulk tag editing
- Paste tag lists from the clipboard
- Work with selected rows only
- Export to CSV
- Keyboard shortcut support
- Russian-language interface

### Requirements
- Windows 10+
- Qt 6.8.3
- MinGW 64-bit
- CMake 3.16+

### Build
From the project root run:

```bat
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64"
cmake --build build-mingw --config Release
```

Or simply use:

```bat
TagBuilder.cmd
```

### Run
Built application:

```text
build-mingw\TagBuilder.exe
```

### GitHub Release
A ready-to-use release archive is also provided:

```text
TagBuilder-release.zip
```

---

## GitHub Release text

### Русский
TagBuilder v1.0.0

TagBuilder — приложение для создания и массового редактирования тегов объектов, в основном для клапанов и аналогичных устройств.

Что умеет:
- добавлять объекты по типу, номеру и индексу
- искать и фильтровать данные по столбцам
- вставлять списки тегов из буфера обмена
- массово изменять значения
- менять только выделенные строки
- экспортировать данные в CSV
- работать с горячими клавишами

В архиве находится готовая Windows-сборка вместе с необходимыми библиотеками Qt.

### English
TagBuilder v1.0.0

TagBuilder is a desktop utility for creating and bulk-editing object tags, mainly for valves and similar devices.

Features:
- add objects by type, number, and index
- search and filter data by columns
- paste tag lists from the clipboard
- apply mass edits
- update only selected rows
- export data to CSV
- keyboard shortcut support

This archive includes the ready-to-use Windows build with the required Qt runtime files.

---

## Repository description

### Русский
TagBuilder — приложение для создания и массового редактирования тегов объектов.

### English
TagBuilder — desktop utility for creating and bulk editing industrial object tags.
