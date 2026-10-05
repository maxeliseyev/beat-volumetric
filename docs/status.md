# Текущий статус

Обновлено: 2026-10-02.

## Branch

`feat/faders-and-gain-history`, от `feat/application-window-and-hit-scope`
(`5b212b4`, PR #6). VERSION 0.6.0.

## Now

- C++20/CMake Debug/Release, Makefile, VERSION 0.6.0 и changelog.
- Автономный `beat_leveler_dsp`: потоковые spectral-flux detector, level meter и
  `StreamingLeveler` для mono/stereo с общей 56 ms lookahead latency.
- Синтетические удары с эталонными onset/peak; WAV runner и CSV измеренного выхода.
- Catch2-тесты DSP/стенда и интеграционный CLI-прогон без GUI.
- Потоковый контракт событий, latency-бюджет, кольцевой delay line и базовый gain law
  реализованы без выделений в audio callback.
- Development AU/VST3/Standalone подключены к DSP через JUCE. Strength и Level —
  вертикальные фейдеры. В Auto фейдер Level показывает цель (середину недавних
  ударов) и не двигается вручную. Window задаёт, сколько удар держит коэффициент
  (по умолчанию 120 мс, отдельно от 30 мс измерения). Прибор — два такта
  осциллографа без прокрутки: сверху вход, снизу выход. Рядом число разброса.
- Добавлен `scripts/package-macos.sh`: Developer ID signing, Hardened Runtime,
  notarization/stapling вложенных bundles и подписанный DMG; Makefile получил
  `app` и `release-dmg`.

## Next

На той же записи открыть `dist/Beat Volumetric 0.6.0.dmg` и проверить
осциллограф: два такта, сверху вход, снизу выход, волна не бежит лентой.
В Auto фейдер Level сам встаёт на цель. Затем Strength 50% и 100%, Window
40 мс и 120 мс. Подавление транзиентов отдельным режимом не добавлять.

## Проверено

- Прослушивание инженера на реальной drum-записи до этой правки: в Auto центральная
  ручка Target не влияет на звук, плато 30 мс не слышно, осциллограф входа без
  пары до/после не читается.
- После отделения окна: `rtk proxy make CONFIG=debug test` — 2/2 CTest. Коэффициент
  держится после 30 мс измерения при окне 120 мс, на том же месте равен единице при
  окне 20 мс и возвращается к единице после окна 40 мс. Debug пересобрал AU, VST3
  и Standalone. Повторное прослушивание в DAW ещё не сделано.
- После замены осциллографа: фейдеры Strength/Level, цель Auto на отдельном
  неперетаскиваемом фейдере, история gain change вместо бегущей волны входа.
  Повторное прослушивание в DAW ещё не сделано.
- `rtk proxy make release-dmg` для 0.5.0: Developer ID Application
  Maxim Eliseyev (PK2473XF3P). Notary принял архив бандлов
  `7376927a-2d5b-4f7b-acc1-88f23c1bc63e` и DMG `2d778515-550c-4316-acf1-5c1e4092a8ce`.
  Stapler и Gatekeeper: `source=Notarized Developer ID` для app и DMG.
  Файл `dist/Beat Volumetric 0.5.0.dmg` (9.3 MB). В бандле CFBundleShortVersionString
  0.5.0. В DAW эта сборка ещё не слушалась.
- `rtk proxy make release-dmg` для 0.6.0: осциллограф на два такта. Notary принял
  архив бандлов `e7d46da0-73e0-4395-949d-3209434d7a4e` и DMG
  `58fe452a-0b3d-4527-af15-4a43d5ef3043`. Stapler и Gatekeeper:
  `source=Notarized Developer ID`. Файл `dist/Beat Volumetric 0.6.0.dmg` (9.3 MB).
  В DAW эта сборка ещё не слушалась.

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

1. Инженеру отдать `dist/Beat Volumetric 0.6.0.dmg`, не 0.5.0. Осциллограф
   показывает два такта: сверху вход, снизу выход, без бегущей ленты. В Auto
   фейдер Level сам встаёт на цель. Сравнить Strength 50% и 100%.
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
