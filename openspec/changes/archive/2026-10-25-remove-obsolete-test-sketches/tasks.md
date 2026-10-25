## 1. Документация

- [x] 1.1 В таблице скоростей заменить строку «ПК и тесты» на «ПК-пульт» с путём `src/motor_controller/pc_control/controller.py`; убрать `echo_test.py`.
- [x] 1.2 В процедуре отката на 9600 указать, что `BT_BAUD` правится в `bt_config.ino`, и убрать `echo_test.py`.
- [x] 1.3 Обновить упоминание `controller.py` в разделе телеметрии на `pc_control/controller.py`.

## 2. Проверка и архивация

- [x] 2.1 Прогнать `openspec validate remove-obsolete-test-sketches`.
- [x] 2.2 Починить структуру основной спеки `encoder-test-sketch` (`## Purpose` + `## Requirements` вместо `## ADDED Requirements`), иначе CLI не видит её требования и не может удалить.
- [x] 2.3 Удалить каталоги `openspec/specs/bt-echo-test/` и `openspec/specs/encoder-test-sketch/` вручную (CLI отказывается писать спеку без требований) и заархивировать change с `--skip-specs`.
- [x] 2.4 Прогнать `openspec validate --all` — ожидается зелёный результат для всех оставшихся спек.
- [x] 2.5 Проверить `grep` по `echo_test`, `encoder_test`, `bt_echo_test` в активных спеках и docs (архив исключить).
- [x] 2.6 Закоммитить чистку требований и документации.
