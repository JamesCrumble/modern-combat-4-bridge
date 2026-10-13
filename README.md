# MC4 Bridge

Modern Combat 4: Zero Hour 1.2.3e (Gameloft, только 32-бит) на 64-битном Android-телефоне: игра исполняется в
qemu-user внутри APK, её вызовы OpenGL ES уходят на GPU телефона. Альфа: главное меню, 60 кадров/с на OnePlus 13s.

Файлы игры в комплект не входят. Состояние, устройство и список недоделанного — в [PROJECT_STATE.md](PROJECT_STATE.md).

```sh
tools/build_install.sh noinstall   # порт + port/MC4-Bridge-debug.apk
tools/push_data.sh                 # APK игры (Ultra) и два OBB на телефон, папки открыты приложению
tools/launch.sh install            # установить и запустить
tools/pull_log.sh                  # лог -> logs/phone.log
```
