# Текущий статус

Обновлено: 2026-09-14.

## Branch

`feat/streaming-detection-and-level-meter`, ответвлена от актуального `main`
(`b95bda6`, PR #2 уже squash-слит). Remote — `origin`. Рабочее дерево содержит
незакоммиченную реализацию начальной части PR 03; новый PR пока не открыт.

## Now

- C++20/CMake Debug/Release, Makefile, VERSION 0.1.0 и changelog.
- Автономный `beat_leveler_dsp`: passthrough-аудиотракт, потоковые spectral-flux
  detector и level meter для mono/stereo без выходной latency.
- Синтетические удары с эталонными onset/peak; WAV runner и CSV измеренного выхода.
- Catch2-тесты DSP/стенда и интеграционный CLI-прогон без GUI.
- Потоковый контракт событий, latency-бюджет и кольцевой delay line реализованы.
- Потоковая детекция/измерение начаты; gain law и plugin-адаптер ещё не реализованы.

## Next

Довести PR 03 из [плана](plan.md#порядок-реализации): подключить CSV-manifest
размеченного внешнего набора через `BEAT_LEVELER_REAL_KIT_DIR` к готовому matcher
и отчёту precision/recall, ошибок времени/уровней. Затем зафиксировать численные
baseline-пороги и решить по сравнению на наборе, остаётся ли weighted high-pass
120 Hz подходящей мерой.

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

## Resume

1. Проверить `rtk git status -sb`, прочитать [AGENTS.md](../AGENTS.md) и текущий
   diff PR 03.
2. `rtk proxy make debug`; `rtk proxy make CONFIG=debug bench` создаёт пример WAV/CSV
   с `detected_hit`.
3. Спроектировать file-side manifest и matcher для размеченного материала, не добавляя
   аудио в git; пропускать прогон явно при отсутствии `BEAT_LEVELER_REAL_KIT_DIR`.
4. На замороженном наборе отделить precision, recall, ложные подъёмы, timing и level
   error; только затем менять порог или weighted-фильтр.

## Открыто

Рабочее имя и идентификаторы плагина, лицензия проекта, поддерживаемые версии ОС,
совместное распространение DSP, допустимые комбинации latency/measurement window.
Они распределены по этапам в плане и не блокируют создание каркаса.
