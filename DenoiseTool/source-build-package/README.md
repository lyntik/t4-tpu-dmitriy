# Пакет для самостоятельной сборки AutoDenoiseTool

## Содержимое
- Исходники C++ плагина (`autodenoisetool.*`, `denoisewidget.*`)
- UI файлы (`DenoiseControlPanel.ui`, `DenoiseWidget.ui`)
- CMake/Qt project файлы (`CMakeLists.txt`, `AutoDenoiseTool.pro`, `resource.qrc`)
- Python-скрипты аналитики (`denoise_volumeop.py`, `autodenoise/*`)
- Опциональные fallback-файлы (`filtration_autodenoise_hook.py`, `FILTRATION_HOOK_LINE.txt`)
- `build_example.cmd`

## Требования для сборки (Windows)
- Visual Studio 2022 (x64)
- CMake >= 3.16
- Qt 6 (например `C:\Qt\6.8.2\msvc2022_64`)
- SDK t4Lib: `T4LIB_ROOT` (`include/`, `lib/`, `depends/`)
- `OPENCV_ROOT`, `VTK_ROOT`, `VTK_VERSION`, `LIBTIFF_ROOT`

## Быстрый старт
1. Открыть `cmd` в корне пакета.
2. Настроить переменные окружения (или отредактировать `build_example.cmd`).
3. Запустить `build_example.cmd`.
4. Готовый DLL: `build\bin\Plugins\Release\AutoDenoiseTool.dll`.

## Деплой в Tomograph4
1. Скопировать DLL -> `<t4root>\Plugins\AutoDenoiseTool.dll`
2. Скопировать `Scripts\analytics\*` -> `<t4root>\Scripts\analytics\*`
3. Убедиться, что PTH "Путь к скриптам" указывает на `<t4root>\Scripts`

## Примечания
- Для CUDA в runtime нужен CUDA-enabled `torch` в целевой установке Tomograph4.
- Файл модели `swinir.pth` в пакет не включён; положить его в `ai-models/denoise`.
- Основной entrypoint плагина: `Scripts/analytics/denoise_volumeop.py`.
- Если runtime хоста не исполняет этот entrypoint, в плагине есть fallback на `filtration.py`.
- Для fallback в `Scripts/analytics/filtration.py` должна быть hook-строка загрузки `Scripts/analytics/autodenoise/filtration_autodenoise_hook.py`.
- Авто-fallback можно принудительно включить переменной окружения `T4_DENOISE_ALLOW_FILTRATION_FALLBACK=1`.
