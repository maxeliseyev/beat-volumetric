# Beat Volumetric

Реальновременный плагин для выравнивания громкости отдельных ударов барабана.
Сайд-проект [Beat Equalizer](https://github.com/maxeliseyev/beat-equalizer): общие наработки по
детекции, отдельный продукт и независимый цикл выпуска.

**Статус: начат PR 04.** Есть C++20/CMake-каркас, автономное потоковое DSP-ядро
с детекцией, измерением и базовым gain-law, а также development-плагин AU/VST3/
Standalone. Плагин уже обрабатывает mono/stereo с lookahead 56 мс; прослушивание
на реальных записях и host-проверка ещё впереди.
Beat Volumetric — рабочее название; имя репозитория — `beat-volumetric`.

## Идея

Каждому найденному удару назначается коэффициент усиления. На защищённом участке
удара он постоянен, чтобы сохранить атаку и тело. Сила воздействия позволяет убрать
случайный разброс громкости, оставляя музыкальную динамику и гост-ноты.

Основные элементы интерфейса: **Сила**, **Цель: авто / вручную**, **Dry/Wet**.
Визуализация показывает найденные удары и измеренные уровни до и после обработки.

## План первой версии

- Детекция ударов с адаптивным порогом, низкочастотной веткой и отсевом просачивания.
- Измерение уровня: пик, RMS, частотно-взвешенная энергия.
- Адаптивная или фиксированная цель, отдельная сила подъёма и ослабления.
- Плавная защита гост-нот, пределы усиления, окно возврата к единичному усилению.
- Внешний sidechain для подавления ложных событий.
- Визуализация, показатель разброса уровней, прослушивание разницы, пресеты и MIDI out.
- Mono и stereo с общим коэффициентом для левого и правого каналов.

Целевые форматы — VST3 и AU на macOS. Standalone нужен как стенд разработки.
CLAP — отдельная задача после проверки основных форматов; Windows — следующий
этап поддержки. Связывание микрофонов между инстансами запланировано на 1.1.

## Ограничения

Плагину нужен lookahead и компенсация задержки хостом. Исходный ориентир — 20 мс,
но окончательная задержка зависит от окна измерения и подтверждения события.
Допустимые сочетания параметров будут определены на прототипе.

Подъём усиливает весь сигнал дорожки, включая просачивание и шум. Огибающая
сохраняется на участке постоянного усиления; переходы и возврат к единице меняют
её, а плотные раскаты требуют более мягкого воздействия. Детектор может ошибаться.

Ручное редактирование ударов, ARA, замена сэмплами и спектральная обработка
не входят в первую версию.

## Разработка

Нужны CMake ≥ 3.22, Ninja, Git и компилятор C++20. На macOS — Xcode Command Line
Tools. JUCE **8.0.15** и Catch2 **v3.8.1** загружаются через FetchContent при первой
конфигурации; нужен доступ к GitHub. Версии закреплены в `cmake/Dependencies.cmake`.

```bash
make                 # Release: ядро, стенд и тесты
make debug           # Debug: то же
make test            # Release-тесты с предварительной сборкой
make CONFIG=debug test
make bench           # синтетический WAV и CSV в новой папке build/release/bench/
```

В окружении Codex запускать эти команды через `rtk proxy`, например
`rtk proxy make debug`. Для обычной разработки RTK не является зависимостью.
Makefile — обёртка над `cmake --preset debug|release`, `cmake --build --preset ...`
и `ctest --preset ...`. Артефакты находятся в `build/debug` и `build/release`.
Версия проекта берётся из [VERSION](VERSION), изменения — в [CHANGELOG.md](CHANGELOG.md).

Подпись и раздача macOS описаны в [docs/packaging-macos.md](docs/packaging-macos.md):
`make app` делает подписанный DMG для локальной проверки, а `make release-dmg`
добавляет notarization и stapling для передачи на другой Mac. Для этого нужен
`Developer ID Application`, Team ID и сохранённый профиль `notarytool`.

Только ядро, без JUCE, Catch2 и сетевых загрузок:

```bash
cmake -S . -B build/dsp-only -G Ninja \
  -DBUILD_TESTING=OFF -DBEAT_LEVELER_BUILD_TOOLS=OFF -DBEAT_LEVELER_BUILD_PLUGIN=OFF
cmake --build build/dsp-only
```

Если оставить `BUILD_TESTING=ON`, будут собраны Catch2 и DSP-тесты без JUCE.
DSP-библиотека не зависит от GUI или файловых форматов. При включённом по умолчанию
`BEAT_LEVELER_BUILD_PLUGIN=ON` JUCE также собирает development-артефакты:

```text
build/debug/src/plugin/BeatVolumetric_artefacts/Debug/VST3/BeatVolumetricDev.vst3
build/debug/src/plugin/BeatVolumetric_artefacts/Debug/AU/BeatVolumetricDev.component
build/debug/src/plugin/BeatVolumetric_artefacts/Debug/Standalone/BeatVolumetricDev.app
```

Raw VST3/AU из `build/` остаются ad-hoc development bundles; для передачи на
другой Mac использовать только DMG из `make app` или `make release-dmg`. В Reaper/Logic нужно проверить PDC около 56 мс,
начальный нулевой участок из-за lookahead, Strength, Auto/Manual Target и Dry/Wet.
Защита от клиппинга не добавлена: максимум подъёма сейчас +6 dB, поэтому запас
по уровню на исходной дорожке обязателен.

## Файловый стенд

После `make debug`:

```bash
build/debug/tools/beat_leveler_runner --synthetic --channels 2 --sample-rate 48000 \
  --blocks 127,1,511 --output build/example/synthetic.wav --report build/example/synthetic.csv

build/debug/tools/beat_leveler_runner --input /path/to/drums.wav \
  --blocks 256 --output build/example/processed.wav --report build/example/processed.csv
```

Для размеченного набора реальных записей задайте `BEAT_LEVELER_REAL_KIT_DIR`.
В каталоге должен быть `manifest.csv` с колонками
`file,onset_sample,peak_dbfs,kind`; `kind` — `kick`, `snare`, `tom`, `ghost`,
`flam` или `bleed`. Одна запись manifest может содержать несколько ударов одного
WAV. Запуск пишет только CSV-отчёт и не изменяет исходные записи:

```bash
BEAT_LEVELER_REAL_KIT_DIR=/path/to/real-kit \
  build/debug/tools/beat_leveler_runner --blocks 256 --tolerance-ms 20 \
  --report build/example/real-kit.csv
```

В отчёте есть общие precision/recall, false positives/negatives, timing и
level error, а также строки по категориям разметки. Precision остаётся общей:
детектор пока не классифицирует инструмент, поэтому категория используется для
раздельного recall и ошибок, а не для ложного обещания per-kind precision.

`--blocks` — повторяемая последовательность размеров; последний блок укорачивается
по длине файла. Стенд читает mono/stereo WAV, сохраняет sample rate (8000–192000 Hz)
и пишет float32 WAV. Исходный файл и результаты предыдущих запусков не перезаписывает:
для повторного ручного запуска выбрать новые пути. `make bench` делает это автоматически.
Стенд хранит файл целиком в памяти; это инструмент проверки, не потоковый файловый плеер.

Синтетика содержит четыре затухающих удара на 0.1/0.5/0.9/1.3 секунды с пиками
−24/−12/−30/−6 dBFS. В stereo правый канал равен −0.5 × левый. Это проверочная
фикстура с известными onset, а не модель качества реальной записи.

CSV содержит summary по каналам, а для синтетики — строки `known_hit` с эталонными
позициями и пиками. Строки `detected_hit` содержат timestamp, confidence и
weighted RMS потокового детектора; они появляются и для произвольного файла.
Пики, RMS и максимальная ошибка измеряются по входу и повторно прочитанному выходному
WAV. Silence floor отчёта — −240 dBFS; аудио остаётся нулевым.

Тесты проверяют mono/stereo, in-place и отдельные буферы, пять sample rate,
блоки 1…2048, меняющееся разбиение, противофазное stereo, эталонные onset/levels,
WAV round-trip и защиту существующих файлов. Реальное качество детектора и
левелинга этим ещё не проверяются.

## Документация

| Документ | Назначение |
|---|---|
| [AGENTS.md](AGENTS.md) | Правила разработки и DSP-инварианты |
| [docs/plan.md](docs/plan.md) | Архитектура, этапы, проверки и открытые решения |
| [docs/status.md](docs/status.md) | Где остановились и следующий шаг |
| [docs/packaging-macos.md](docs/packaging-macos.md) | Подпись, notarization и раздача macOS |
| [drum-leveler-project.md](drum-leveler-project.md) | Исходная идея продукта |

Связь с Beat Equalizer не должна требовать соседнего checkout для сборки.
Переиспользование детектора сначала проходит аудит: существующий файловый анализ
нельзя напрямую вызывать из аудиопотока. Подробности — в плане.

Лицензия этого проекта пока не выбрана. Выбор лицензии и проверка условий
зависимостей входят в подготовку первой внешней сборки.
