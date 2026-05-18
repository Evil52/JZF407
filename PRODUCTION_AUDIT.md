# Production audit — jtf407

**Дата:** 2026-05-19
**Цель:** дополнительный аудит после исправлений из `memory_ub_analysis_report.md`. Найдены и закрыты ещё 6 проблем, критичных для долговременной работы на проде.

---

## Резюме

| # | Проблема | Серьёзность | Статус |
|---|---|---|---|
| A1 | `configCHECK_FOR_STACK_OVERFLOW = 0` — нет защиты от stack overflow | **Критическая** | ✅ Включено `=2`, добавлен hook |
| A2 | `configUSE_MALLOC_FAILED_HOOK = 0` — OOM проходит молча | **Критическая** | ✅ Включено, добавлен hook |
| A3 | `EthIf` стек 350 байт — мало для tcpip_input chain | **Высокая** | ✅ Увеличен до 768 байт |
| A4 | Гонка `heth` между `low_level_output` (tcpip_thread) и `ethernet_link_thread` | **Высокая** | ✅ Введён `heth_mutex` |
| A5 | `ipaddr_aton` без проверки результата | **Средняя** | ✅ Проверяется, при ошибке bailout |
| A6 | `do_connect` мог запускаться повторно пока предыдущий in-flight | **Средняя** | ✅ Добавлен флаг `s_connecting` |

После правок: `FLASH 22.57%`, `RAM 62.71%`. Заголовок `+512 байт` к flash.

---

## A1 — Stack overflow detection отсутствует (Критическая)

**Файл:** [Core/Inc/FreeRTOSConfig.h](Core/Inc/FreeRTOSConfig.h)

Без `configCHECK_FOR_STACK_OVERFLOW` переполнение стека FreeRTOS-задачи приводит к молчаливому повреждению соседней памяти (включая стек другой задачи, TCB, heap). На проде — невоспроизводимые сбои через дни работы.

**Что включено:**
```c
#define configCHECK_FOR_STACK_OVERFLOW  2  /* pattern-fill check */
```

Метод `2` (pattern fill) проверяет, что весь stack заполнен `0xA5` при создании; при context switch проверяет последние 16 байт — если они изменены, вызывается `vApplicationStackOverflowHook`.

**Hook реализован** в [Core/Src/rtos_hooks.c](Core/Src/rtos_hooks.c):
- Записывает магическое значение `0xDEAD1111` + имя задачи + tick count в фиксированную область CCMRAM (`0x1000FFF0`).
- Отключает прерывания, входит в бесконечный цикл — IWDG (если включён) сбрасывает MCU.
- После сброса bootloader или диагностика может прочитать маркер и понять причину.

---

## A2 — Malloc failed hook отсутствует (Критическая)

`pvPortMalloc()` используется внутри `osMutexNew`, `osSemaphoreNew`, `mqtt_client_new` (через `mem_malloc`/heap_4). При исчерпании FreeRTOS heap (24 KB) функция возвращает NULL; без hook никто об этом не узнает.

**Что включено:**
```c
#define configUSE_MALLOC_FAILED_HOOK  1
```

Hook записывает магическое значение `0xDEAD2222` в ту же область CCMRAM и зависает.

---

## A3 — `INTERFACE_THREAD_STACK_SIZE = 350` слишком мал (Высокая)

**Файл:** [LWIP/Target/ethernetif.c:46](LWIP/Target/ethernetif.c#L46)

Задача `ethernetif_input` принимает Ethernet пакеты и вызывает `netif->input` = `tcpip_input`. Цепочка вызовов при приёме TCP-пакета:

```
ethernetif_input → tcpip_input → tcpip_thread mbox push  (другой поток)
```

Но lwIP в режиме NO_SYS=0 ставит пакет в очередь, так что глубина в этом потоке невелика. Однако коллбеки `HAL_ETH_RxAllocateCallback` и `HAL_ETH_RxLinkCallback` тоже исполняются в этом потоке через `HAL_ETH_ReadData`, и они дёргают `LWIP_MEMPOOL_ALLOC` (lwIP внутренние вызовы).

**350 байт = 87 32-битных слов** — критически мало, особенно для GCC `-O0` (Debug). Любой всплеск трафика → stack overflow.

**Исправлено:** увеличено до 768 байт.

---

## A4 — Гонка на `heth` между TX и link thread (Высокая)

**Файлы:** [LWIP/Target/ethernetif.c](LWIP/Target/ethernetif.c) (строки 372-441 и 776-832)

Два потока обращаются к `&heth` без синхронизации:

| Поток | Вызов | Что делает |
|---|---|---|
| `tcpip_thread` | `low_level_output` | `HAL_ETH_Transmit_IT(&heth, ...)` — настраивает TX DMA дескрипторы |
| `EthLink` (`BelowNormal`) | `ethernet_link_thread` | `HAL_ETH_Stop_IT/SetMACConfig/Start_IT(&heth)` |

При смене состояния PHY (link up/down) во время активной передачи могут быть повреждены DMA дескрипторы, регистры MAC, состояние очереди TX. На практике — зависание TX или HardFault.

**Решение:** введён `osMutexId_t heth_mutex`, оборачивает оба пути:

```c
static osMutexId_t heth_mutex = NULL;
static void heth_lock(void)   { if (heth_mutex) osMutexAcquire(heth_mutex, osWaitForever); }
static void heth_unlock(void) { if (heth_mutex) osMutexRelease(heth_mutex); }
```

Мьютекс создаётся в `low_level_init` (если ещё не создан). FreeRTOS-мьютексы поддерживают priority inheritance — приоритетная инверсия (tcpip_thread = 24 высокий, EthLink = BelowNormal) корректно обрабатывается.

**Что НЕ защищено мьютексом и почему:**
- `HAL_ETH_RxAllocateCallback` / `HAL_ETH_RxLinkCallback` — вызываются ИЗ `HAL_ETH_ReadData`, который зовётся из `low_level_input` (`ethernetif_input` поток). Они не трогают TX-состояние heth, только RX, и сам ETH IRQ disabled в этой точке.
- `HAL_ETH_ErrorCallback` / `RxCpltCallback` / `TxCpltCallback` — вызываются из ETH_IRQHandler, который НЕ может вытесняться задачами FreeRTOS (priority выше syscall priority). Они только дёргают семафоры — это безопасно.

---

## A5 — `ipaddr_aton` без проверки (Средняя)

**Файл:** [Core/Src/mqtt_app.c](Core/Src/mqtt_app.c)

Раньше:
```c
ipaddr_aton(MQTT_BROKER_IP, &broker);  // ignore result
mqtt_client_connect(s_client, &broker, ...);  // possibly garbage
```

Сейчас `MQTT_BROKER_IP` — литерал, и `ipaddr_aton` всегда успешен. Но если в будущем IP начнут брать из конфигурации (EEPROM, NVS) — невалидная строка приведёт к использованию неинициализированной `ip_addr_t broker` (стек) → подключение к рандомному адресу → возможный fault внутри LwIP.

**Исправлено:**
```c
if (!ipaddr_aton(MQTT_BROKER_IP, &broker)) {
    set_connecting(0);
    return;
}
```

---

## A6 — Дублирующий `do_connect` (Средняя)

**Файл:** [Core/Src/mqtt_app.c](Core/Src/mqtt_app.c)

Старый цикл:
```c
for (;;) {
    osDelay(2000);
    if (!mqtt_is_connected_safe() && !get_connected()) {
        tcpip_callback(do_connect, NULL);  // может стрельнуть второй раз
    }
}
```

`mqtt_client_connect` асинхронный: он возвращает ERR_OK и инициирует TCP handshake. Колбек `on_connection` вызывается **позже** (может занять секунды, особенно при потере пакетов). За эти 2 секунды цикл может снова увидеть `s_connected = 0` и поставить второй `do_connect` в очередь tcpip_thread. Тогда lwIP получит **два** `mqtt_client_connect` подряд на одном клиенте — поведение не определено (внутреннее состояние клиента может оказаться невалидным).

**Решение:** добавлен флаг `s_connecting`:
- Устанавливается перед `tcpip_callback(do_connect, ...)`.
- Сбрасывается в `on_connection` (любой статус: accepted, refused, timeout) и в `do_connect` если `mqtt_client_connect` вернул ошибку дисптача.
- Цикл мониторинга пропускает итерацию пока `s_connecting == 1`.

Также проверяется возврат `tcpip_callback` — если mbox tcpip_thread переполнен, флаг сбрасывается для повторной попытки.

---

## Что осталось вне аудита (приемлемо для текущего usecase)

| Проблема | Почему оставлено |
|---|---|
| `s_topic_buf` глобальный без сброса | Оба колбека (`on_publish`, `on_data`) последовательны в одном потоке. Невозможна гонка по дизайну lwIP. |
| Нет LWT (Last Will Testament) | По требованию задачи — простое управление LED. При желании можно вернуть `will_topic = "stm32/status"`, `will_msg = "offline"`, `will_retain = 1`. |
| `configASSERT` зависает без output | Для production желательно сохранять PC/file/line в CCMRAM (как hooks из A1/A2). Сейчас просто `for(;;)`. |
| Нет IWDG (Independent Watchdog) | Не настроен в .ioc. Для прода **обязательно** включить IWDG с таймаутом ~5-10 сек и обновлять его в самой низкоприоритетной задаче. |
| Нет реконнекта при долгом link-down | Сейчас reconnect только при разрыве MQTT, не при пропаже link. Можно добавить проверку `netif_is_link_up` в монитор. |
| `mqtt_disconnect` не вызывается | При reconnect через тот же `s_client` lwIP внутренне переустанавливает соединение, явный disconnect не требуется. |

---

## Что НЕ нужно делать (популярные ложные тревоги)

- ❌ **Не делайте** `s_client = NULL` при разрыве — lwIP сам управляет lifecycle через колбеки.
- ❌ **Не вызывайте** `mqtt_client_free()` — клиент переиспользуется при reconnect.
- ❌ **Не оборачивайте** `HAL_GPIO_WritePin` в мьютекс — GPIO BSRR атомарен по архитектуре Cortex-M4.
- ❌ **Не используйте** `printf` в IRQ или критических секциях — newlib `_write` не reentrant.

---

## Изменённые файлы

| Файл | Назначение |
|---|---|
| [Core/Inc/FreeRTOSConfig.h](Core/Inc/FreeRTOSConfig.h) | +2 опции защиты |
| [Core/Src/rtos_hooks.c](Core/Src/rtos_hooks.c) | (новый) hooks для stack overflow / malloc fail |
| [Core/Src/mqtt_app.c](Core/Src/mqtt_app.c) | флаг `s_connecting`, проверка `ipaddr_aton` |
| [LWIP/Target/ethernetif.c](LWIP/Target/ethernetif.c) | `heth_mutex`, стек 768, защита TX/link |
| [CMakeLists.txt](CMakeLists.txt) | +`rtos_hooks.c` в сборку |

---

## Чек-лист для production deployment

- [x] Stack overflow detection включён
- [x] Malloc failed hook включён
- [x] EthIf stack ≥ 512 байт
- [x] heth защищён мьютексом
- [x] MQTT reconnect не дублируется
- [x] Synchronisation `gnetif` (через `net_ready`)
- [x] `s_connected`/`s_client` с memory barriers
- [x] `mqtt_client_is_connected` под `LOCK_TCPIP_CORE`
- [x] MAC-адрес в static памяти
- [x] Проверка результата `osSemaphoreAcquire`
- [ ] **TODO:** включить IWDG в .ioc (через CubeMX)
- [ ] **TODO:** реализовать оповещение о сбоях из CCMRAM маркера (UART/LED-код)
- [ ] **TODO:** настроить LWT для статуса плата online/offline в брокере
- [ ] **TODO:** прогон 24+ часа под нагрузкой (continuous publish 10 Hz)

---

## Память

| Регион | Использовано | Размер | % |
|---|---|---|---|
| FLASH | 118344 B | 512 KB | 22.6% |
| RAM (heap+bss) | 82200 B | 128 KB | 62.7% |
| CCMRAM | 0 B | 64 KB | 0.0% |

CCMRAM полностью свободен — отличный кандидат для FreeRTOS heap (через linker script) или для крупных буферов (LwIP MEM_SIZE), что снизит давление на основной SRAM. Это уже отдельная задача оптимизации.
