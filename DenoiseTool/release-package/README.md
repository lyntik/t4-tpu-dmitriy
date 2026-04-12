# Пакет релиза AutoDenoiseTool

## Состав
- `Plugins/AutoDenoiseTool.dll`
- `Scripts/analytics/denoise_volumeop.py`
- `Scripts/analytics/autodenoise/denoise.py`
- `Scripts/analytics/autodenoise/models/network_swinir.py`
- `Scripts/analytics/autodenoise/utils/patchify.py`
- `Scripts/analytics/autodenoise/filtration_autodenoise_hook.py` (опционально, для fallback)
- `Scripts/analytics/autodenoise/FILTRATION_HOOK_LINE.txt` (подсказка для включения fallback)

## Внешний обязательный файл модели
- `ai-models/denoise/swinir.pth` (или выбрать другое имя модели в UI)

## Установка в каталог Tomograph4 (пример: `C:\t4vis`)
1. Скопировать `Plugins/AutoDenoiseTool.dll` -> `<root>/Plugins/AutoDenoiseTool.dll`
2. Скопировать `Scripts/analytics/*` -> `<root>/Scripts/analytics/*`
3. Убедиться, что PTH "Путь к скриптам" указывает на `<root>/Scripts`

## Примечания по запуску
- Основной entrypoint: `Scripts/analytics/denoise_volumeop.py`.
- Если runtime хоста не исполняет этот entrypoint, плагин может перейти в fallback через `filtration.py`.
- Условие fallback: в `Scripts/analytics/filtration.py` должна быть hook-строка загрузки `Scripts/analytics/autodenoise/filtration_autodenoise_hook.py`.
- Авто-fallback также можно принудительно включить переменной окружения: `T4_DENOISE_ALLOW_FILTRATION_FALLBACK=1`.
- Рекомендуемый режим для чистой сборки: без hook в `filtration.py`.
- Опция CUDA требует CUDA-enabled `torch` в целевом runtime.
