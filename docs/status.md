# Текущий статус

Обновлено: 2026-09-19.

## Branch

`feat/realtime-gain-leveler`, ответвлена от актуального `main` (`a843297`, PR #4
уже squash-слит). Remote — `origin`. Ветка содержит незакоммиченную реализацию
базового PR 04; новый PR пока не открыт.

## Now

- C++20/CMake Debug/Release, Makefile, VERSION 0.1.0 и changelog.
- Автономный `beat_leveler_dsp`: потоковые spectral-flux detector, level meter и
  `StreamingLeveler` для mono/stereo с общей 56 ms lookahead latency.
- Синтетические удары с эталонными onset/peak; WAV runner и CSV измеренного выхода.
- Catch2-тесты DSP/стенда и интеграционный CLI-прогон без GUI.
- Потоковый контракт событий, latency-бюджет, кольцевой delay line и базовый gain law
  реализованы без выделений в audio callback.
- Development AU/VST3/Standalone подключены к DSP через JUCE; UI показывает hits,
  peak, confidence и применённый gain. Раскладка UI разделяет controls, input
  waveform и telemetry; scope получает входной сигнал до leveler.
- Добавлен `scripts/package-macos.sh`: Developer ID signing, Hardened Runtime,
  notarization/stapling вложенных bundles и подписанный DMG; Makefile получил
  `app` и `release-dmg`.

## Next

Проверить development VST3/AU на реальной записи в Reaper/Logic: PDC, слышимый
эффект Strength/Target/Dry-Wet, атаки, хвосты, плотные раскаты и перегрузка. Затем
доделать PR 03 file-side manifest/real-kit отчёт и зафиксировать baseline
precision/recall, false positives, timing и level error до расширения gain-поведения.

## Проверено

- macOS, AppleClang 21.0.0, CMake 4.1.2, Ninja 1.13.1.
- `rtk proxy make debug`: DSP/Catch2 и интеграционный CLI — 2/2 CTest-прогона.
- `rtk proxy make all bench`: Release, те же 2/2 прогона и пример WAV/CSV.
- Отдельная конфигурация с `BUILD_TESTING=OFF`, `BEAT_LEVELER_BUILD_TOOLS=OFF`
  и `FETCHCONTENT_FULLY_DISCONNECTED=ON` собрала ядро без внешних зависимостей.
- Повторное чтение float32 WAV сохраняет сэмплы точно; отчёт даёт max_abs_error=0.
  Результаты одинаковы при фиксированных и меняющихся размерах блока.
- Проверены ссылки документации, JSON presets и whitespace; аудио и зависимости
  остаются в игнорируемом `build/`.
- После PR 02: Debug CTest 2/2; delay line даёт импульс ровно через заявленные
  отсчёты, одинаковый результат при блоках 1 и 127/511/64, latency 56 мс округляется
  в 2688 отсчётов при 48 kHz и 2470 при 44.1 kHz.
- Реальный материал, DSP левелинга и DAW пока не проверялись: их реализация впереди.
- На ветке PR 03: `rtk proxy make debug` — 2/2 CTest. Синтетический runner при
  48 kHz фиксирует четыре события и четыре завершённых измерения и даёт одинаковый
  результат для блоков `1` и `127,1,511`; противофазный stereo не отменяет события.
- На этой же ветке `rtk proxy make all bench` проходит Release CTest 2/2 и создаёт
  WAV/CSV с четырьмя synthetic `detected_hit`; отдельная `BUILD_TESTING=OFF`,
  `BEAT_LEVELER_BUILD_TOOLS=OFF`, `FETCHCONTENT_FULLY_DISCONNECTED=ON` конфигурация
  собирает новое DSP-ядро без внешних зависимостей.
- После `126f114`: matcher `DetectionMetrics` покрыт synthetic-тестом на matching,
  false positive/negative, timing и level error; `BEAT_LEVELER_REAL_KIT_DIR` в
  текущем окружении не задан, поэтому реальный прогон не запускался.
- На `feat/realtime-gain-leveler`: `rtk proxy make debug` — 2/2 CTest; Debug
  собирает AU, VST3 и Standalone. DSP-тесты проверяют unity с lookahead, общий
  stereo gain и одинаковый результат при блоках `1` и `127,1,511`.
- После исправления UI: `rtk proxy make debug` — 2/2 CTest; Debug-сборка
  пересобирает Editor/Processor и повторно подписывает AU, VST3 и Standalone.
  Визуально необходимо подтвердить новую раскладку в Reaper после повторного
  сканирования/загрузки bundle.
- Release CTest 2/2 и `make all bench` также проходят; `BUILD_TESTING=OFF`,
  `BEAT_LEVELER_BUILD_TOOLS=OFF`, `BEAT_LEVELER_BUILD_PLUGIN=OFF` собирает DSP
  без JUCE. Финальные Debug/Release AU, VST3 и Standalone bundles проходят
  `codesign --verify --deep --strict` после post-build ad-hoc подписи.
- Реальная запись и целевые DAW в этом окружении не проверялись; development IDs
  и code signing не являются релизными.
- Packaging-путь проверен синтаксически и на отсутствии credentials: текущий
  Keychain не содержит `Developer ID Application`, поэтому реальный signing,
  notarization и DMG не запускались.

## Resume

1. В DAW повторно загрузить свежий `build/debug/src/plugin/BeatVolumetric_artefacts/Debug/VST3/
   BeatVolumetricDev.vst3` (или AU), подтвердить layout/scope и записать
   наблюдения инженера по реальному drum track.
2. Получить Developer ID Application, Team ID и app-specific password; сохранить
   credentials профилем `beat-volumetric`, затем проверить `make app` и
   `make release-dmg` на отдельном Mac.
3. Проверить отдельными проходами `mix=0`, `strength=0`, ручную цель и Auto; сохранить
   исходник/обработанный bounce вне git и отметить PDC/первые 56 мс.
4. Подключить CSV-manifest размеченного внешнего набора через
   `BEAT_LEVELER_REAL_KIT_DIR` к matcher и отчёту precision/recall.
5. Только после baseline решать по weighted high-pass, плотным пассажам и переходить
   к полноценному PR 05.

## Открыто

Рабочее имя и идентификаторы плагина, лицензия проекта, поддерживаемые версии ОС,
совместное распространение DSP, допустимые комбинации latency/measurement window.
Они распределены по этапам в плане и не блокируют создание каркаса.
