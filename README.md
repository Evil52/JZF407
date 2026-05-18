JZSTM32F407 MQTT Ethernet Controller

Embedded firmware для STM32F407VETx с управлением через MQTT по Ethernet.

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

### Полная карта пинов JZ-F407VET6 (по официальной схеме)

#### Системное (трогать нельзя)

| Pin | Функция |
|---|---|
| PA1 | ETH RMII_REF_CLK → DP83848 |
| PA2 | ETH RMII_MDIO → DP83848 |
| PA7 | ETH RMII_CRS_DV → DP83848 |
| PB11 | ETH RMII_TX_EN → DP83848 |
| PB12 | ETH RMII_TXD0 → DP83848 |
| PB13 | ETH RMII_TXD1 → DP83848 |
| PC1 | ETH RMII_MDC → DP83848 |
| PC4 | ETH RMII_RXD0 → DP83848 (через 0Ω R47) |
| PC5 | ETH RMII_RXD1 → DP83848 (через 0Ω R48) |
| PA13 | JTMS / SWDIO (отладка через JTAG header или ST-Link) |
| PA14 | JTCK / SWCLK |
| PA15 | JTDI |
| PB3 | JTDO |
| PB4 | NJTRST |
| PH0 | HSE OSC_IN — кварц 25 MHz (X1) |
| PH1 | HSE OSC_OUT |
| PB2 | BOOT1 (jumper-конфигурация) |
| NRST | Reset (кнопка RESET + ST-Link nRST) |
| VBAT | Coin-cell battery (RTC backup, CN5) |

#### Onboard периферия (физически распаяна, прошивка может использовать или нет)

| Pin | Что подключено | Текущий статус в прошивке |
|---|---|---|
| **PE13** | **LED1** | управляется `stm32/led/1` (active-LOW) |
| **PE14** | **LED2** | управляется `stm32/led/2` (active-LOW) |
| **PE15** | **LED3** | управляется `stm32/led/3` (active-LOW) |
| PE10 | **Кнопка S1** (10K pull-up R17, нажатие = LOW) | не используется |
| PE11 | **Кнопка S2** (R18) | не используется |
| PE12 | **Кнопка S3** (R19) | не используется |
| PA9 | USART1_TX → MAX232 → DB9 (CN1) **RS-232** | не используется |
| PA10 | USART1_RX ← MAX232 ← DB9 | не используется |
| PD5 | USART2_TX → MAX3485 (драйвер) | не используется |
| PD6 | USART2_RX ← MAX3485 (приёмник) | не используется |
| PD7 | MAX3485 **RE/DE** (направление RS-485) | не используется |
| PD0 | CAN1_RX ← TJA1050 | не используется |
| PD1 | CAN1_TX → TJA1050 → клемма **P2** (CAN1H/L) | не используется |
| PB5 | CAN2_RX ← TJA1050 | не используется |
| PB6 | CAN2_TX → TJA1050 → клемма **P6** (CAN2H/L) | не используется |
| PA11 | USB OTG_FS_DM → CN3 (USB Type-B) | не используется |
| PA12 | USB OTG_FS_DP → CN3 | не используется |
| PB14 | USB HS DM (если есть PHY) | не используется |
| PB15 | USB HS DP | не используется |
| PC8 | SDIO_D0 → microSD slot **J2** | не используется |
| PC9 | SDIO_D1 | не используется |
| PC10 | SDIO_D2 | не используется |
| PC11 | SDIO_D3 | не используется |
| PC12 | SDIO_CLK | не используется |
| PD2 | SDIO_CMD | не используется |
| PD3 | SD Card Detect | не используется |
| PE3 | **W25Q128 Flash CS** (16 МБ SPI Flash, U1) | не используется |
| PC2 | SPI2_MISO (общий: Flash + NRF24L01) | не используется |
| PC3 | SPI2_MOSI | не используется |
| PB10 | SPI2_SCK | не используется |
| PB8 | I2C1_SCL → **AT24C02 EEPROM** (U5) | не используется |
| PB9 | I2C1_SDA → AT24C02 | не используется |
| PE7 | NRF24L01 CE → разъём P3 | не используется |
| PE8 | NRF24L01 CSN | не используется |
| PE9 | NRF24L01 IRQ | не используется |
| PE2 | DS18B20 1-Wire DQ → разъём P7 (с R29 4.7K pull-up) | не используется |

> **Все эти периферийные устройства физически распаяны на плате**, но прошивка их не инициализирует. Можно использовать в любой момент — добавив `MX_xxx_Init()` и драйвер.

### Доступные через колодки платы (JZ-F407VET6)

На плате две гребёнки **P4** и **P5** (Header 8×2). Это всё что доступно для подключения внешних датчиков/исполнителей. Большинство пинов имеют альтернативную функцию **DCMI** (камера) — если камера не используется, можно работать как обычным GPIO.

#### Колодка P4 (левая)

| Pin | Сигнал | STM32 пин | Альт. функция |
|:---:|---|:---:|---|
| 1 | +3.3V | — | питание |
| 2 | DGND | — | земля |
| 3 | PA5 | PA5 | SPI1_SCK, DAC2_OUT |
| 4 | PA6 | PA6 | **DCMI_PIXCK**, SPI1_MISO |
| 5 | PA3 | PA3 | USART2_RX, TIM2_CH4 |
| 6 | PA4 | PA4 | **DCMI_HSYNC**, SPI1_NSS, DAC1_OUT |
| 7 | PC0 | PC0 | ADC123_IN10 |
| 8 | PA0 | PA0 | ADC123_IN0, USART2_CTS, TIM2_CH1 |
| 9 | PE6 | PE6 | **DCMI_D7**, TIM9_CH2 |
| 10 | PC13 | PC13 | RTC_OUT, RTC_TAMP1 (без сильной нагрузки) |
| 11 | PE4 | PE4 | **DCMI_D4**, TIM4_BKIN |
| 12 | PE5 | PE5 | **DCMI_D6**, TIM9_CH1 |
| 13 | PE1 | PE1 | **DCMI_D3** |
| 14 | PE0 | PE0 | **DCMI_D2**, TIM4_ETR |
| 15 | PB7 | PB7 | **DCMI_VSYNC**, I2C1_SDA |
| 16 | PD4 | PD4 | FSMC_NOE, USART2_RTS |

#### Колодка P5 (правая)

| Pin | Сигнал | STM32 пин | Альт. функция |
|:---:|---|:---:|---|
| 1 | +3.3V | — | питание |
| 2 | DGND | — | земля |
| 3 | PC7 | PC7 | **DCMI_D1**, USART6_RX, I2S3_MCK |
| 4 | PA8 | PA8 | TIM1_CH1, MCO1, USART1_CK |
| 5 | PD15 | PD15 | TIM4_CH4, FSMC_D1 |
| 6 | PC6 | PC6 | **DCMI_D0**, USART6_TX, I2S2_MCK |
| 7 | PD13 | PD13 | TIM4_CH2, FSMC_A18 |
| 8 | PD14 | PD14 | TIM4_CH3, FSMC_D0 |
| 9 | PD11 | PD11 | **DCMI_RESET** (если камера), FSMC_A16 |
| 10 | PD12 | PD12 | TIM4_CH1, FSMC_A17 |
| 11 | PD9 | PD9 | USART3_RX, FSMC_D14 |
| 12 | PD10 | PD10 | **DCMI_PWDN** (если камера), FSMC_D15 |
| 13 | PB1 | PB1 | ADC12_IN9, TIM3_CH4 |
| 14 | PD8 | PD8 | USART3_TX, FSMC_D13 |
| 15 | PB0 | PB0 | ADC12_IN8, TIM3_CH3 |
| 16 | — | — | свободный |

#### Винтовая клемма (синяя, 5-pin)

По схеме это **объединённый блок RS-485 + CAN**. Слева направо:

| Pin | Сигнал | Назначение |
|:---:|---|---|
| 1 | **485A** | RS-485 линия A (J1, через MAX3485, R3=120Ω терминатор) |
| 2 | **485B** | RS-485 линия B |
| 3 | **CAN1H** | CAN1 High (P2, через TJA1050) |
| 4 | **CAN1L** | CAN1 Low |
| 5 | **GND** | общая земля |

Для работы соответствующей шины нужно инициализировать USART2 (PD5/PD6/PD7) или CAN1 (PD0/PD1) в прошивке.


#### Красные элементы (DIP-switch / jumpers)

По схеме на плате есть:
- **3-pin jumper P8** — выбор BOOT0 (нормально = pull-down к GND через R54=3.3K, замыкаем = к +3.3V для DFU-загрузки)
- **Перемычки 0Ω R47, R48** на PC4/PC5 (для ETH RMII RX), пользователю **не трогать**
- **Перемычка J5** на USB +5V — выбор питания (USB vs внешнее)
- **Терминаторы R24, R26 (120Ω)** для CAN1/CAN2 — переключаются jumper'ами J3, J4 (если плата на конце CAN-шины)



#### Питание

- **CN3 (USB Type-B)** — 5V с USB-порта
- **CN2 (DC-10B, 3-pin)** — внешнее питание (обычно 5–7V)
- **Jumper J5** — выбор источника (USB / внешний)
- Питание идёт через **AMS1117-3.3** → +3.3V на всю плату
- **Светодиод LED3** (отдельный, не RGB) — индикатор питания через R1

#### Прочие разъёмы

| Разъём | Что |
|---|---|
| **CN1** (DB9-male) | RS-232 через MAX232 (USART1) |
| **CN2** (DC-10B) | внешнее питание |
| **CN3** (USB) | USB OTG_FS |
| **CN4** (HR911105A) | RJ-45 Ethernet |
| **CN5** | Coin-cell для RTC backup |
| **J1** | 2-pin RS-485 (дубль клеммы) |
| **J2** | microSD card slot |
| **JTAG** | 10×2 разъём для отладки (полный JTAG, не только SWD) |
| **P1** | 2-pin (вероятно USB +5V на выход) |
| **P2** | 2-pin CAN1 (H/L, дубль клеммы) |
| **P3** | 8-pin для модуля **NRF24L01** |
| **P6** | 2-pin CAN2 (H/L) |
| **P7** | 3-pin для **DS18B20** (термодатчик 1-wire) |
| **P4, P5** | Header 8×2 — пользовательские GPIO (см. ниже) |

### Как добавить новый GPIO (используя реальный пин платы)

### Как добавить новый вход/выход

**Пример 1:** кнопка на колодке P4, пин 8 (= **PA0**).

1. В [Core/Src/main.c](Core/Src/main.c) в `MX_GPIO_Init()` добавить:
   ```c
   __HAL_RCC_GPIOA_CLK_ENABLE();
   GPIO_InitStruct.Pin   = GPIO_PIN_0;
   GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull  = GPIO_PULLUP;   /* +3.3V → кнопка → пин → GND */
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
   ```

2. В коде читать: `HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)` → `GPIO_PIN_RESET` когда нажата.

**Пример 2:** реле на колодке P5, пин 10 (= **PD12**).

1. В `MX_GPIO_Init()`:
   ```c
   __HAL_RCC_GPIOD_CLK_ENABLE();
   GPIO_InitStruct.Pin   = GPIO_PIN_12;
   GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
   ```

2. Управлять: `HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);`

3. Добавить новый MQTT-топик `stm32/relay/1` в [Core/Src/led_dispatch.c](Core/Src/led_dispatch.c):
   ```c
   /* В led_dispatch_parse(), перед "else if stm32/led/..." */
   if (strcmp(topic, "stm32/relay/1") == 0) {
       r.mask  = 0x10;        /* свободный бит */
       r.state = (payload[0] == '1') ? 1 : 0;
       return r;
   }
   ```
   И в [Core/Src/mqtt_app.c](Core/Src/mqtt_app.c) `on_data()`:
   ```c
   if (d.mask & 0x10) HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12,
                                         d.state ? GPIO_PIN_SET : GPIO_PIN_RESET);
   ```
   И подписаться в `on_connection()`:
   ```c
   mqtt_subscribe(client, "stm32/relay/1", 0, on_sub_done, NULL);
   ```

**Пример 3:** аналоговый датчик (термистор) на колодке P5, пин 13 (= **PB1** = ADC12_IN9).

Это требует инициализации ADC1 — выходит за рамки простого GPIO. Используй CubeMX чтобы сгенерировать `MX_ADC1_Init()`, потом читай `HAL_ADC_GetValue()`.

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


