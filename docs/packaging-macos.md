# Сборка и раздача под macOS

Для локальной проверки достаточно development-сборки. Для передачи плагина
звукорежиссёру нужен `Developer ID Application`, Hardened Runtime и notarization.
Весь путь автоматизирован в `scripts/package-macos.sh`.

```sh
make app                 # Release: собрать, подписать и сделать DMG
make CONFIG=debug app    # Debug-вариант, тоже с подписью
make release-dmg         # то же + notarization и stapling
```

Результат находится в `dist/Beat Volumetric 0.1.0.dmg`. Внутри:

```text
BeatVolumetricDev.app
Plug-Ins/
├── BeatVolumetricDev.component
└── BeatVolumetricDev.vst3
```

## Что получить у Apple

Нужны следующие данные и объекты:

1. Активное членство в Apple Developer Program. Для выпуска Developer ID
   certificate обычно нужны права Account Holder.
2. `Team ID` — десятисимвольный идентификатор команды из Apple Developer
   Account. Он нужен для notarization.
3. Сертификат **Developer ID Application** вместе с приватным ключом в Keychain.
   `Apple Development` и `Apple Distribution` для внешней macOS-раздачи не
   подходят.
4. App-specific password для Apple ID. Это не обычный пароль Apple ID и не
   пароль от Developer Portal.
5. Финальные bundle/manufacturer/plugin IDs, если development-идентификаторы
   `com.beatvolumetric.development`, `Bvlt` и `Lvld` нужно заменить перед
   первой внешней сборкой.

`Developer ID Installer` нужен только если позже появится подписанный `.pkg`.
Для текущего DMG он не требуется.

Проверить наличие сертификата и приватного ключа можно так:

```sh
security find-identity -v -p codesigning
```

В выводе должна быть строка вида:

```text
Developer ID Application: Имя или компания (TEAMID)
```

Если есть только `.cer`, этого недостаточно: приватный ключ создаётся вместе
с CSR и должен находиться на этой машине. В противном случае нужно импортировать
`.p12` с приватным ключом или выпустить новый сертификат по CSR на текущем Mac.

## Один раз сохранить credentials notarization

Создать app-specific password на `appleid.apple.com`, затем выполнить:

```sh
xcrun notarytool store-credentials beat-volumetric \
    --apple-id your-apple-id@example.com \
    --team-id TEAMID \
    --password APP_SPECIFIC_PASSWORD
```

Команда сохранит секрет в Keychain. Пароль и `.p8`-ключи не добавляются в git.
Другой вариант — Team App Store Connect API key, но для ручной первой настройки
app-specific password проще.

После этого:

```sh
make app
make release-dmg
```

Скрипт автоматически:

- собирает AU, VST3 и Standalone;
- подписывает вложенный код и bundles через `Developer ID Application`;
- включает Hardened Runtime и secure timestamp;
- нотариализирует архив всех bundles;
- stapling’ом прикрепляет ticket к `.app`, `.component` и `.vst3`;
- создаёт, подписывает, нотариализирует и stapling’ом прошивает DMG;
- запускает `codesign`, `stapler validate` и `spctl` проверки.

Сертификат можно выбрать явно:

```sh
CODESIGN_IDENTITY=SHA1_OR_FULL_IDENTITY_NAME make app
NOTARY_PROFILE=beat-volumetric make release-dmg
```

Для теста подписи без обращения к Apple можно использовать только `make app`,
но такой DMG ещё может быть отклонён Gatekeeper на другом Mac. Для инженера
нужно передавать результат `make release-dmg`, а не ad-hoc build из `build/`.

AU и VST3 устанавливаются в стандартные папки пользователя:

```text
~/Library/Audio/Plug-Ins/Components/
~/Library/Audio/Plug-Ins/VST3/
```

Стенд `BeatVolumetricDev.app` можно запустить отдельно для проверки доступа к
аудиоустройству; entitlement `com.apple.security.device.audio-input` применяется
только к Standalone. Плагин внутри Reaper или Logic использует разрешения host.

## Проверка на другой машине

Проверять нужно именно DMG после `make release-dmg`, желательно на Mac, где этот
проект раньше не собирался. Локальная машина разработчика может иметь кэш
notarization и ранее выданные разрешения Gatekeeper.
