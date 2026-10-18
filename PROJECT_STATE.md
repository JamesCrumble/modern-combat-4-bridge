# MC4 Bridge (Modern Combat 4: Zero Hour -> Android APK для OnePlus 13s): состояние проекта

Цель: повторить для этой игры то, что сделано в `../nova3probe`, `../the-dark-knight-rises-probe` и
`../wild-blood-probe` (APK с qemu-user + мост GLES на GPU телефона). Каркас (portbase, обёртка, qemu,
инструменты) взят из `../wild-blood-probe`, слой игры — из `../the-dark-knight-rises-probe` (тот же движок Glitch),
GDRM — как в N.O.V.A. 3 и Wild Blood.

Обновлено: 2026-10-18. Версия порта 0.4: поддержаны игра 1.2.3e и 1.2.1a (авто-определение по APK, свой патч-OBB и своя папка профиля у каждой, подробности в CHANGELOG.md).

Рабочая папка: `/home/james/probe/modern-combat-4-zero-hour-probe`. Всё исполняется в WSL (Ubuntu-20.04).
Телефон: OnePlus 13s (CPH2723, Snapdragon 8 Elite, только AArch64), виден через Windows `adb.exe` (обёртка `tools/adb`).

## Правила работы
- По-русски, кратко.
- Глобально ничего не ставить: тулчейны и бинарники — локально в папке проекта, копией из соседних проектов.
- Сборка — на основе Ultra APK (решение пользователя 13.10).
- Добавлять только необходимое: заглушки Java-классов «на всякий случай» не писать (социальные классы убраны 13.10).
- Телефон: при заливке данных создавать папки и открывать их приложению (`chmod 2777`) — делает `tools/push_data.sh`.
  13.10 пользователь разрешил ставить APK, заливать данные и запускать игру.
- git: репозитория пока нет.

## Игра
- `gamebin/Modern-Combat-4-Zero-Hour-v1-2-3e-{Ultra1-r2,medium,low}.apk` (пакет `com.gameloft.android.ANMP.GloftM4HM`)
  и `gamebin/2399-...-cache1.zip` (два OBB: main.1100 — 2,05 ГБ, patch.12222 — 15 МБ).
- Три APK отличаются только `classes.dex`, а в нём — одним методом `MC4.getGeneration()`: low отвечает 0, medium 1,
  Ultra 3 для любого устройства. Библиотеки одинаковые. Порт отвечает 3 (`MC4_GENERATION` в `engine_env.txt` меняет).
- Библиотека: `lib/armeabi-v7a/libModernCombat4.so` (24,9 МБ, SHA1 `8cd4ee3566b4289d70d977ef2bde1feb823b5f73`,
  SONAME `libAndroid.so`, экспортированы C++-символы). `libgenerator.so` (HDIDFV) не загружается.
- Данные: OBB — zip без сжатия с `data_1..data_6` и роликами; движок читает OBB сам, пути спрашивает у Java
  (`MC4.getMainFileLocation/getPatchFileLocation`). Распаковывать ничего не нужно.

### Чем отличается от TDKR (тот же движок)
| что | TDKR | MC4 |
|---|---|---|
| обратные вызовы | `GL2JNIActivity`/`GL2JNILib` | glf-методы на классе, переданном в `init`; свои — статические методы `<пакет>/MC4`; ещё `SUtils`, `Device`, `glot/PortingJNI` |
| запуск | `init` -> `initGL` -> ... | перед `init`: `SUtils.nativeInit`, `Device.nativeInit`, `MC4.nativeMC4Init(объект)` |
| показ кадра | порт после `step` | движок сам: `eglGetCurrentDisplay/Surface` + `eglSwapBuffers` (Java-swap пропускается, `GL2JNILib.g`) |
| данные | плоские файлы | два OBB, читаются движком напрямую |
| DRM | нет | GDRM, четыре `lockPointer` (`port/game/patch.cpp`) |
| APK | не нужен после извлечения .so | движок открывает сам APK как zip (`SUtils.GetApkPath`) и читает `assets/` через Java |
| звук | Java `AudioTrack` | то же (`libOpenSLES` в зависимостях есть, но идёт через `AudioTrack`) |

## Что есть в папке
| путь | что |
|---|---|
| `port/game/` | слой игры: `main.cpp`, `io_paths.cpp`, `input_bridge.cpp`, `patch.cpp`, `tweaks.cpp`, `jni/` |
| `port/game/jni/*_gen.inc` | тихие заглушки Java-методов, пишет `tools/gen_stubs.sh` из dex и строк библиотеки |
| `port/portbase/` | база из Wild Blood; добавлено: `AAsset_read/seek`, `eglGetCurrentDisplay`, `gettid`, `android_egl_set_presenter` |
| `port/android/` | APK-обёртка (пакет `com.jamescrumble.mc4`), `build_apk.sh` |
| `jdk/ android-sdk/ sdl-arm/ qemu/out/ port/android/build/sysroot/` | локальные копии из `../wild-blood-probe` |
| `mc4data/` | для запуска на ПК: `apk/<вариант>/` (распаковка), `obb/`, ссылки `lib`, `assets`, `game.apk`, папка `files` |
| `extracted/` | `classes.txt` (dexdump Ultra), `strings.txt`, `imports.txt`, `syms.txt` |
| `tools/` | `build_install.sh`, `run_pc.sh`, `push_data.sh`, `launch.sh`, `pull_log.sh`, `dexmethod.py`, `sym.py` (адрес -> функция), `gen_stubs.sh` |

Раскладка каталога игры для порта (`argv[1]`): `lib/armeabi-v7a/libModernCombat4.so`, `assets/`, `game.apk`,
`obb/` (оба .obb), `files/` («SD-папка» движка). На телефоне обёртка собирает её сама из APK игры и папки с OBB
(или zip-архива кэша рядом с APK: достаёт из него только .obb).

## Сделано (2026-10-13)
- Порт и APK собираются (`tools/build_install.sh noinstall` -> `port/MC4-Bridge-debug.apk`, 58 МБ, debug-ключ).
- **Телефон: главное меню, 60 fps** (`logs/phone_1.png`, `logs/phone.log`). Данные залиты `tools/push_data.sh`
  (APK Ultra + два OBB, 2,1 ГБ, папки `2777`).
- ПК (`tools/run_pc.sh`): 0 неразрешённых импортов, кадры идут и показываются; картинку на ПК не смотрели.

## Как искалось (по порядку)
1. 4 неразрешённых импорта: `AAsset_read`, `AAsset_seek`, `eglGetCurrentDisplay`, `gettid` — добавлены в portbase.
2. `SUtils/Device.nativeInit` получали не свой класс -> «Class MC4 does not have...»; каждому передаётся свой.
3. SIGSEGV +0xb74654 после `getResource('eventParameters.json')`: файл из `assets/` APK отдавался пустым. Теперь
   читается из `<game>/assets`.
4. Пути вида `./` + абсолютный и `<ресурсы>/` + абсолютный (двойной слэш) — правило в `port_fix_path`.
5. SIGSEGV в `_zip_name_locate` (+0x8c4b9c) из `BITrackingManager::LaunchGame`: движок открывает APK по
   `SUtils.GetApkPath()` и не проверяет результат. Отдаётся `<game>/game.apk`.
6. `presented=0` при идущих кадрах: движок показывает кадр через `eglSwapBuffers`, а шим отвечал «нет дисплея».
   `android_egl_set_presenter` (egl_shim.cpp) направляет swap в `mc4_present` (main.cpp).

## Не проверено / не сделано
- Всё после главного меню: кампания, касания, звук на слух, сохранения, пауза/возврат, ролики (mp4 лежат в OBB,
  плеера нет: `playVideo` отвечает заглушкой).
- В логе 53 строки `GetStaticMethodID: NULL class dereference`: это Facebook (28) и GameAPI (25) — классы
  соцсетей, `nativeInit` которых порт не вызывает. Без сети безвредно; оставлено намеренно.
- `Device.d1()` не найден (метода с таким именем нет среди строк библиотеки при генерации).
- Геймпад: коды кнопок и стики в `input_bridge.cpp` остались от TDKR, у MC4 другие точки входа (`nativeSetTouchPad*`).
- ETC1 без декодера уходит драйверу как есть (Adreno принимает).
- Тексты `README.md`-уровня, `tools/release_install.txt`, `port/android/README.md`, `port/NOTICE.md` — ещё от Wild Blood.
- Ограничитель кадров движка и прочие твики TDKR по смещениям убраны из `tweaks.cpp` (остались fast CAS, uclamp, трасса сна).
