# jtf407 — STM32F407 MQTT Ethernet Controller

Production-ready embedded firmware для STM32F407VETx с управлением через MQTT по Ethernet.

**Текущая функциональность:** дистанционное управление тремя LED через MQTT-команды. Лёгко расширяется на любые GPIO (см. таблицу пинов ниже).

---

## Содержание

1. [Быстрый старт — ](#быстрый-старт)
2. [Как поменять IP брокера](#как-поменять-ip-брокера)
3. [Таблица пинов — что занято, что свободно](#таблица-пинов)
4. [Архитектура](#архитектура)
5. [Безопасность кода](#безопасность-кода)
6. [Тесты](#тесты)
7. [Troubleshooting](#troubleshooting)
8. [Структура проекта](#структура-проекта)

---

## Быстрый старт

### Что нужно

- Плата **STM32F407VETx** с **DP83848 PHY** (RMII)
- ПК с **mosquitto** брокером и **MQTT Explorer** (UI)
- Ethernet кабель плата ↔ ПК (или плата ↔ роутер)
- ST-Link v2 для прошивки

### Подключение питания и сети

1. Запитать плату (USB или внешний 5V)
2. Подключить Ethernet кабель **плата → ПК** (или **плата → роутер**)
3. Подключить ST-Link к разъёму SWD (PA13/PA14)

### Прошивка

```powershell
# Из корня проекта
cmake --build build
arm-none-eabi-objcopy -O ihex build\jtf407.elf build\jtf407.hex
STM32_Programmer_CLI --connect port=SWD --write build\jtf407.hex --verify --rst
```

### Что должно произойти

1. Через ~3 секунды после подачи питания плата получит свой статический IP
2. Подключится к MQTT брокеру по адресу, прописанному в [Core/Src/mqtt_app.c](Core/Src/mqtt_app.c)
3. Опубликует в брокер:
   - `stm32/status` = `online` (retained)
   - `stm32/diag` = причина последнего reset (retained)

### Проверка из консоли

```powershell
# Включить все три LED
& "C:\Program Files\mosquitto\mosquitto_pub.exe" -h <IP_ПК> -t stm32/led/all -m 1

# Выключить
& "C:\Program Files\mosquitto\mosquitto_pub.exe" -h <IP_ПК> -t stm32/led/all -m 0

# Отдельный LED
& "C:\Program Files\mosquitto\mosquitto_pub.exe" -h <IP_ПК> -t stm32/led/2 -m 1

# Мониторинг статуса
& "C:\Program Files\mosquitto\mosquitto_sub.exe" -h <IP_ПК> -t "stm32/#" -v
```

### MQTT-топики

| Топик | Направление | Payload | Описание |
|---|---|---|---|
| `stm32/status` | плата → брокер | `online`/`offline` | retained, текущий статус (LWT) |
| `stm32/diag` | плата → брокер | строка | retained, причина последнего reset |
| `stm32/led/1` | брокер → плата | `1` или `0` | управление LED1 (PE13) |
| `stm32/led/2` | брокер → плата | `1` или `0` | управление LED2 (PE14) |
| `stm32/led/3` | брокер → плата | `1` или `0` | управление LED3 (PE15) |
| `stm32/led/all` | брокер → плата | `1` или `0` | все три LED одновременно |
| `stm32/ping` | брокер → плата | любой | плата ответит тем же payload в `stm32/pong` |
| `stm32/pong` | плата → брокер | эхо payload | используется для измерения RTT |

---

## Как поменять IP брокера

Откройте [Core/Src/mqtt_app.c](Core/Src/mqtt_app.c), найдите строку ~32:

```c
#define MQTT_BROKER_IP    "192.168.137.1"
#define MQTT_BROKER_PORT  1883
#define MQTT_CLIENT_ID    "stm32f407"
```

Измените `MQTT_BROKER_IP` на IP вашего брокера. Если в цеху несколько плат — также сделайте `MQTT_CLIENT_ID` уникальным для каждой (например `stm32f407_01`, `stm32f407_02`).

**Также:** статический IP самой платы прописан в [LWIP/App/lwip.c](LWIP/App/lwip.c) (~строка 65):

```c
IP4_ADDR(&ipaddr,  192, 168, 137,   2);   // IP платы
IP4_ADDR(&netmask, 255, 255, 255,   0);
IP4_ADDR(&gw,      192, 168, 137,   1);   // gateway = IP роутера/ПК
```

После изменения — пересобрать и перепрошить:

```powershell
cmake --build build
arm-none-eabi-objcopy -O ihex build\jtf407.elf build\jtf407.hex
STM32_Programmer_CLI --connect port=SWD --write build\jtf407.hex --verify --rst
```

### Альтернатива: DHCP

Если в сети есть DHCP-сервер (роутер), можно вернуть DHCP вместо статического IP:

```c
// в lwip.c заменить блок IP4_ADDR на:
ipaddr.addr = 0;
netmask.addr = 0;
gw.addr = 0;

// и раскомментировать в конце MX_LWIP_Init:
dhcp_start(&gnetif);
```

---

## Таблица пинов

### Занятые системой (трогать нельзя)

| Pin | Функция | Группа |
|---|---|---|
| PA1 | ETH_REF_CLK | RMII Ethernet |
| PA2 | ETH_MDIO | RMII Ethernet |
| PA7 | ETH_CRS_DV | RMII Ethernet |
| PA13 | SWDIO | SWD отладка |
| PA14 | SWCLK | SWD отладка |
| PB11 | ETH_TX_EN | RMII Ethernet |
| PB12 | ETH_TXD0 | RMII Ethernet |
| PB13 | ETH_TXD1 | RMII Ethernet |
| PC1 | ETH_MDC | RMII Ethernet |
| PC4 | ETH_RXD0 | RMII Ethernet |
| PC5 | ETH_RXD1 | RMII Ethernet |
| PH0 | OSC_IN | HSE кварц 25 MHz |
| PH1 | OSC_OUT | HSE кварц 25 MHz |
| NRST | Reset | Кнопка/ST-Link |

### Используются прошивкой (можно перенастроить)

| Pin | Текущая роль | Что делает |
|---|---|---|
| PE10 | GPIO Input | свободный вход (не используется в коде) |
| PE11 | GPIO Input | свободный вход (не используется в коде) |
| PE12 | GPIO Input | свободный вход (не используется в коде) |
| **PE13** | **LED1 output, active-LOW** | управляется `stm32/led/1` |
| **PE14** | **LED2 output, active-LOW** | управляется `stm32/led/2` |
| **PE15** | **LED3 output, active-LOW** | управляется `stm32/led/3` |

### Свободные для использования (~75 пинов)

| Порт | Свободные пины |
|---|---|
| **GPIOA** | PA0, PA3, PA4, PA5, PA6, PA8, PA9, PA10, PA11, PA12, PA15 |
| **GPIOB** | PB0–PB10, PB14, PB15 |
| **GPIOC** | PC0, PC2, PC3, PC6–PC15 (PC13/14/15 — анти-tamper, обычно для RTC) |
| **GPIOD** | PD0–PD15 (все 16 пинов) |
| **GPIOE** | PE0–PE9 (10 пинов) |

### Как добавить новый вход/выход

Пример: использовать **PA0** как кнопку.

1. В [Core/Src/main.c](Core/Src/main.c) в `MX_GPIO_Init()` добавить:
   ```c
   __HAL_RCC_GPIOA_CLK_ENABLE();
   GPIO_InitStruct.Pin   = GPIO_PIN_0;
   GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull  = GPIO_PULLUP;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
   ```

2. В коде читать: `HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)` → 0 или 1.

Пример: использовать **PD12** как реле.

1. В `MX_GPIO_Init()`:
   ```c
   __HAL_RCC_GPIOD_CLK_ENABLE();
   GPIO_InitStruct.Pin   = GPIO_PIN_12;
   GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
   ```

2. Управлять: `HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);`

3. Опционально добавить MQTT-топик в [Core/Src/led_dispatch.c](Core/Src/led_dispatch.c) — там сейчас вся маршрутизация команд.

### Доступная периферия

Свободные шины (можно использовать на свободных пинах):
- **USART1, USART2, USART3, UART4, UART5, USART6** — все доступны
- **SPI1, SPI2, SPI3** — все доступны
- **I2C1, I2C2, I2C3** — все доступны
- **CAN1, CAN2** — доступны
- **TIM1..TIM14** — таймеры (TIM6 уже используется как RTOS timebase, остальные свободны)
- **ADC1, ADC2, ADC3** — АЦП, до 16 каналов
- **DAC1, DAC2** — ЦАП на PA4, PA5

---

## Архитектура

### Стек технологий

```
┌─────────────────────────────────────────┐
│  Application (mqtt_app, led_dispatch)   │  ← наш код
├─────────────────────────────────────────┤
│  LwIP MQTT client (apps/mqtt)           │
│  LwIP TCP/IP stack v2.1.2               │
├─────────────────────────────────────────┤
│  CMSIS-RTOS v2 → FreeRTOS v10.3.1       │
├─────────────────────────────────────────┤
│  STM32F4 HAL + DP83848 PHY driver       │
├─────────────────────────────────────────┤
│  STM32F407VETx @ 168 MHz                │
│  128 KB RAM + 64 KB CCMRAM + 512 KB FLASH│
└─────────────────────────────────────────┘
```

### FreeRTOS-задачи

| Задача | Приоритет | Стек | Что делает |
|---|---|---|---|
| `defaultTask` | Normal | 1 KB | Запускает `MX_LWIP_Init()`, потом idle |
| `mqttTask` | BelowNormal | **4 KB** | MQTT connect, reconnect monitor |
| `wdgTask` | Low | 1 KB | Refresh IWDG каждые 1.5 сек |
| `EthIf` | Realtime | 768 B | Приём Ethernet пакетов |
| `EthLink` | BelowNormal | 1 KB | Мониторинг PHY link, переконфигурация MAC |
| `tcpip_thread` | (24) | **2 KB** | Ядро LwIP (TCP/IP, MQTT callbacks) |

### Поток выполнения при старте

```
1. fault_marker_capture()    — захватить причину reset из RCC_CSR + CCMRAM
2. HAL_Init()
3. SystemClock_Config()      — 168 MHz через PLL × 336 / 2
4. watchdog_start()          — IWDG ~20 сек, дальше нельзя выключить
5. MX_GPIO_Init()
6. osKernelInitialize()
7. net_ready_init()          — semaphore для синхронизации задач
8. Create tasks (default, mqtt, wdg)
9. osKernelStart()
   │
   ├── defaultTask:
   │   ├── watchdog_refresh()
   │   ├── MX_LWIP_Init() ← PHY init (медленно, до 5 сек)
   │   ├── watchdog_refresh()
   │   └── net_ready_signal() — освободить mqttTask
   │
   ├── mqttTask:
   │   ├── net_ready_wait()   — ждём, пока LwIP готов
   │   ├── while !link_up: osDelay(200)
   │   ├── mqtt_client_new()
   │   ├── tcpip_callback(do_connect)
   │   └── for(;;) проверять подключение, реконнект через 2 сек
   │
   └── wdgTask:
       └── for(;;) watchdog_refresh(); osDelay(1500)
```

### Что происходит при сбое

| Сбой | Кто ловит | Реакция |
|---|---|---|
| Зависание любой задачи (>20 сек без watchdog refresh) | IWDG | Hardware reset → плата перезагружается |
| `pvPortMalloc` вернул NULL (heap exhausted) | `vApplicationMallocFailedHook` | Записать маркер `0xDEAD2222` в CCMRAM → IWDG reset → опубликовать `stm32/diag = malloc_failed` |
| Потеря Ethernet link | `ethernet_link_thread` | Stop ETH, ждать link up, restart с новой скоростью/duplex |
| Потеря MQTT-соединения | `mqtt_app_task` monitor loop | Реконнект через 2 секунды, LWT → брокер опубликует `offline` |
| Плата вырубилась (питание) | Брокер (LWT) | Через ~90 сек публикует `stm32/status = offline` |

---

## Безопасность кода

Проект прошёл **два аудита** на утечки памяти и неопределённое поведение:

1. **Первый аудит** ([memory_ub_analysis_report.md](memory_ub_analysis_report.md)) — найдены и исправлены 6 проблем синхронизации между потоками FreeRTOS и LwIP.
2. **Второй (production) аудит** ([PRODUCTION_AUDIT.md](PRODUCTION_AUDIT.md)) — найдены и исправлены ещё 6 проблем (stack overflow detection, race на ETH HAL, дублирующий connect, и т.д.).

### Чек-лист статуса

| Класс проблем | Статус |
|---|---|
| Утечки памяти (malloc без free) | ✅ Нет — все аллокации единожды или статические |
| Data race на shared state | ✅ `s_connected`, `s_client` под `volatile` + `__DMB()` |
| Гонка между tcpip_thread и FreeRTOS задачами | ✅ `LOCK_TCPIP_CORE` + tcpip_callback везде |
| Гонка на `heth` (ETH HAL) | ✅ Мьютекс `heth_mutex` с PI |
| Висячие указатели | ✅ `MACAddr` сделан `static` |
| Buffer overflow | ✅ `strncpy` + явный `'\0'`, проверка длин |
| Stack overflow detection | ⚠️ Hooks готовы, но `configCHECK_FOR_STACK_OVERFLOW = 0` (false positives — нужно измерить high water mark и потом включить) |
| Heap exhaustion detection | ✅ `vApplicationMallocFailedHook` записывает в CCMRAM |
| Watchdog | ✅ IWDG ~20 сек, refresh из задачи low priority |
| Reset reason tracking | ✅ Через RCC_CSR + CCMRAM магия, публикуется в `stm32/diag` |
| Last Will Testament | ✅ `offline` retained при разрыве |
| Dangling pointers после free | ✅ Не применимо (нет dynamic free) |

### Что НЕ покрыто (TODO для production)

- [ ] Измерить `uxTaskGetStackHighWaterMark()` для всех задач, поджать стеки, **включить** `configCHECK_FOR_STACK_OVERFLOW = 1`
- [ ] Защита от подмены прошивки (Read-out protection level 1+ через `option bytes`)
- [ ] TLS для MQTT (через mbedTLS) — если плата подключается через интернет
- [ ] Уникальный `MQTT_CLIENT_ID` для каждой платы (сейчас один на всех — две платы будут друг друга кикать)

---

## Тесты

### Нагрузочный тест

Скрипт [scripts/loadtest.ps1](scripts/loadtest.ps1) гоняет MQTT request-response на плату в течение N часов и собирает статистику:
- RTT min/avg/max
- Packet loss
- Reconnect count
- Reset events

Запуск:
```powershell
cd scripts
.\loadtest.ps1 -DurationHours 24
```

### Unit-тесты (host-side)

Чистая логика без HAL/RTOS-зависимостей покрыта Unity-тестами в папке [test/](test/).

**Что покрыто:**
- `fault_marker.c` — 15 тестов на классификацию reset reason
- `led_dispatch.c` — 16 тестов на MQTT-роутинг команд

**Запуск (на ПК, без платы):**
```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cmake -B build_test -S test -G Ninja -DCMAKE_C_COMPILER=cl && cmake --build build_test && ctest --test-dir build_test --output-on-failure'
```

Должно быть **31/31 PASS**.

---

## Troubleshooting

### Плата не подключается к брокеру

| Симптом | Причина | Решение |
|---|---|---|
| `ping` платы не работает | Кабель / link down | Проверить кабель, индикаторы на RJ-45 |
| `ping` ОК, но в брокере нет коннекта | Неверный IP брокера в прошивке | Изменить `MQTT_BROKER_IP` в [mqtt_app.c](Core/Src/mqtt_app.c) |
| `ping` ОК, брокер видит коннект, но сразу disconnect | `MQTT_REQ_MAX_IN_FLIGHT` слишком мал | Сейчас стоит 8 (хватает для 4 sub + 2 pub). Если добавляешь много subscribe — увеличь в [lwipopts.h](LWIP/Target/lwipopts.h) |
| Брокер слушает только на `localhost` | mosquitto конфиг | В `mosquitto.conf` добавить `listener 1883 0.0.0.0` и `allow_anonymous true` |
| Брандмауэр блокирует порт | Windows firewall | `New-NetFirewallRule -DisplayName "Mosquitto" -Direction Inbound -Protocol TCP -LocalPort 1883 -Action Allow` |
| Плата в одной подсети, ПК в другой | Network topology | Использовать ICS (Internet Connection Sharing) или подключить через роутер |

### Плата ребутается циклически

Если в `stm32/diag` появляется `iwdg_timeout` — что-то зависает дольше чем на 20 секунд. Проверь:
- Не добавил ли ты тяжёлые вычисления в IRQ-обработчик
- Не блокирует ли какая-то задача `tcpip_thread`
- Не закончился ли FreeRTOS heap (24 KB) — увеличь `configTOTAL_HEAP_SIZE` в [FreeRTOSConfig.h](Core/Inc/FreeRTOSConfig.h)

Если в `stm32/diag` появляется `stack_overflow` или `malloc_failed` — увеличь стек указанной задачи или общий heap.

### Команды доходят до брокера, но LED не реагируют

Проверь подписку через MQTT Explorer на `stm32/#` — должно быть видно:
- `stm32/status` = `online`
- `stm32/diag` = последний reset reason

Если статус `offline` — плата не подключена. Если статус `online`, но LED не реагируют — пересобери прошивку, возможно ты используешь старую версию.

---

## Структура проекта

```
jtf407/
├── README.md                      ← этот файл
├── memory_ub_analysis_report.md   ← первый аудит
├── PRODUCTION_AUDIT.md            ← второй аудит
├── CMakeLists.txt                 ← корневой CMake (ARM сборка)
├── jtf407.ioc                     ← CubeMX конфиг
├── STM32F407XX_FLASH.ld           ← linker script
│
├── Core/
│   ├── Inc/                       ← публичные заголовки
│   │   ├── main.h
│   │   ├── mqtt_app.h             ← MQTT задача
│   │   ├── net_ready.h            ← синхронизация defaultTask ↔ mqttTask
│   │   ├── watchdog.h             ← IWDG bare-metal driver
│   │   ├── fault_marker.h         ← reset reason capture
│   │   ├── led_dispatch.h         ← чистая логика MQTT-роутинга (тестируемая)
│   │   ├── FreeRTOSConfig.h
│   │   └── stm32f4xx_hal_conf.h
│   └── Src/
│       ├── main.c                 ← entry point, инициализация задач
│       ├── mqtt_app.c             ← MQTT клиент + callbacks
│       ├── net_ready.c            ← готовность сети (semaphore)
│       ├── watchdog.c             ← IWDG (запуск + refresh task)
│       ├── fault_marker.c         ← reset reason из RCC_CSR + CCMRAM
│       ├── led_dispatch.c         ← парсинг топиков → bitmask LED
│       ├── rtos_hooks.c           ← stack overflow / malloc fail hooks
│       ├── freertos.c
│       ├── stm32f4xx_it.c         ← IRQ handlers
│       ├── stm32f4xx_hal_msp.c
│       ├── system_stm32f4xx.c
│       ├── sysmem.c
│       └── syscalls.c
│
├── LWIP/
│   ├── App/
│   │   ├── lwip.c                 ← MX_LWIP_Init (статический IP)
│   │   └── lwip.h
│   └── Target/
│       ├── ethernetif.c           ← интеграция с ETH HAL + heth_mutex
│       ├── ethernetif.h
│       └── lwipopts.h             ← конфигурация LwIP
│
├── Drivers/                       ← STM32 HAL + CMSIS + DP83848 PHY
│
├── Middlewares/
│   └── Third_Party/
│       ├── FreeRTOS/              ← FreeRTOS v10.3.1
│       └── LwIP/                  ← LwIP v2.1.2 + mqtt.c
│
├── cmake/
│   ├── stm32cubemx/CMakeLists.txt
│   └── gcc-arm-none-eabi.cmake    ← toolchain для ARM
│
├── test/                          ← host-side unit tests (MSVC)
│   ├── CMakeLists.txt
│   ├── unity/                     ← Unity test framework
│   ├── mocks/                     ← mock STM32 регистров
│   ├── test_fault_marker.c        ← 15 тестов
│   └── test_led_dispatch.c        ← 16 тестов
│
└── scripts/
    └── loadtest.ps1               ← 24-часовой нагрузочный тест
```

---

## Версии и зависимости

| Компонент | Версия |
|---|---|
| MCU | STM32F407VETx (LQFP100) |
| HAL | STM32F4 HAL Drivers |
| RTOS | FreeRTOS v10.3.1 + CMSIS-RTOS v2 |
| TCP/IP | LwIP v2.1.2 |
| MQTT | LwIP MQTT app (v2.1.2) |
| PHY | DP83848 (RMII mode) |
| Toolchain | arm-none-eabi-gcc 14.3.1 (STM32CubeCLT 1.21.0) |
| Build | CMake 3.22+ + Ninja |
| Flasher | STM32_Programmer_CLI 2.22.0 (ST-Link v2 SWD) |

---

## Лицензия

Сторонние компоненты:
- STM32 HAL — STMicroelectronics, Ultimate Liberty License
- FreeRTOS — MIT License
- LwIP — modified BSD License
- Unity — MIT License

Пользовательский код (`Core/Src/mqtt_app.c`, `net_ready.c`, `watchdog.c`, `fault_marker.c`, `led_dispatch.c`, `rtos_hooks.c`, тесты) — внутреннее использование.
