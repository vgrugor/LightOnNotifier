
# LightOnNotifier

ESP8266 firmware that sends a Ukrainian Telegram notification when the device starts,
with local LED and buzzer feedback. A startup represents restored power; there is no
separate light sensor.

## Features

- Wi-Fi with static addressing, finite connection attempts, and automatic reconnect.
- A startup notification retained in RAM until Wi-Fi and valid TLS calendar time are available.
- Independent delivery and retries for multiple Telegram recipients.
- Timed LED and buzzer indications; an active-low button cancels the current buzzer signal.
- Serial diagnostics and ArduinoOTA firmware updates over the local network.
- Deterministic host tests for application behavior and CI builds using placeholders.

## Hardware and software

The target is a NodeMCU v2 ESP8266 board, an external active-high LED, an active-high
buzzer, and a momentary button. The example pin mapping is listed below; preserve the
actual mapping in your local configuration.

Development tools are declared in `requirements-dev.txt`: PlatformIO Core 6.2.0 and
clang-format 18.1.8. CI uses Python 3.12. Firmware preserves Arduino's GNU C++17 default;
native tests use C++17. `platformio.ini` pins espressif8266 4.2.1 (Arduino core 3.1.2),
native 1.2.1, and immutable Git revisions of Universal Arduino Telegram Bot and ArduinoJson
7.4.2. Native tests pin Unity 2.6.1 and use the same ArduinoJson revision for response
acknowledgment tests. ArduinoOTA comes from the ESP8266 Arduino framework. Host tests
also require a working native C++ compiler.

## Repository structure

| Path | Purpose |
| --- | --- |
| `include/domain/` | Neutral event and state contracts. |
| `include/application/`, `src/application/` | Static network validation, connection, time, delivery, and signals. |
| `include/infrastructure/`, `src/infrastructure/` | ESP8266, GPIO, TLS/Telegram, configuration, and OTA adapters. |
| `include/presentation/`, `src/presentation/` | Event dispatch and serial formatting. |
| `src/main.cpp` | Hardware initialization, dependency wiring, and lifecycle coordination. |
| `test/` | Native behavior tests using fake adapters and deterministic time. |
| `scripts/` | Formatting checks and placeholder configuration generation for CI. |

Application and domain code do not include Arduino or transport libraries. Hardware
and service adapters implement their contracts. The device has no filesystem assets.

## Initial setup

Run commands from the repository root:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
if [ ! -f src/infrastructure/env.cpp ]; then
    cp src/infrastructure/env.cpp.example src/infrastructure/env.cpp
fi
```

Copy the example only when the local file does not already exist. Edit the ignored
`src/infrastructure/env.cpp` with your deployment settings. A clean checkout can
also compile the CI environment without creating or accessing a local credential file.

## Configuration and secrets

`include/infrastructure/env.h` declares the configuration; the tracked
`src/infrastructure/env.cpp.example` contains placeholders. The local implementation
`src/infrastructure/env.cpp` is ignored and must never be committed or printed in logs.

| Setting | Purpose |
| --- | --- |
| `WIFI_SSID`, `WIFI_PASSWORD` | Access point credentials. |
| `WIFI_IP`, `WIFI_GATEWAY`, `WIFI_SUBNET` | Valid IPv4 static address, gateway, and subnet. |
| `OTA_HOSTNAME`, `OTA_PASSWORD` | Local OTA hostname and update authentication. |
| `EXTERNAL_LED_PIN`, `BUZZER_PIN`, `BUTTON_PIN` | Local peripheral pin assignments. |
| `BOARD_LED_PIN` | Compatibility setting; the board LED is currently unused. |
| `BOT_TOKEN` | Telegram bot token. |
| `CHAT_IDS`, `CHAT_IDS_COUNT` | One to eight unique chat identifiers, at most 33 bytes each; formats below. |
| `LIGHT_ON_MESSAGE` | Nonempty UTF-8 message, at most 512 bytes, including multibyte characters. |

Invalid notification configuration prevents delivery. Invalid static addressing must
be corrected before Wi-Fi can connect; the device does not silently switch to DHCP.
CI generates a separate placeholder translation unit under `.pio` and excludes the
local `env.cpp` from its firmware sources.

Chat identifiers accept canonical nonzero decimal IDs with an optional leading minus,
without leading zeros, or public channel usernames beginning with `@`. Usernames contain
5–32 ASCII letters, digits, or underscores after `@`; duplicate usernames are rejected
without regard to case. Do not configure the same destination through both its numeric ID
and username; those aliases are not resolved against Telegram during validation.

Static IP and gateway must be distinct unicast host addresses on the same subnet.
The subnet mask must be contiguous and exclude all-zero/all-one masks; network and
broadcast addresses are rejected. Validation is atomic and uses host-testable application code.

## Build and test

Activate the development environment first. Run these commands from the repository root:

```sh
python scripts/check_format.py
pio test -e native
pio run -e nodemcuv2_ci
pio run -e nodemcuv2
```

`nodemcuv2_ci` builds without production credentials. `nodemcuv2` uses the ignored
local configuration. Native tests compile actual application modules, event dispatch,
HTTP response framing, and Telegram acknowledgment parsing. They use no network or
physical board and cover input validation, timing boundaries, and counter wraparound.
To apply the pinned formatter, run `python scripts/check_format.py --fix`; it excludes
the local credential file. CI runs the first three commands on pushes and pull requests.

## Interfaces

Telegram sends the configured text to each configured chat through the bot API over
TLS. Configured recipients with an acknowledged successful send are not sent the event
again during the same boot. Failed recipients receive at most five attempts, with a one-second
retry interval and a minimum 200 ms gap between transport calls. Unavailable Wi-Fi or
calendar time pauses delivery without consuming attempts.

A send is acknowledged only by a complete HTTP 200 response with JSON Boolean `ok: true`.
Responses end at the declared `Content-Length` or at connection close when no length is
provided. Malformed framing, truncated or oversized content, and unexpected transfer encoding
fail the attempt. The request uses HTTP/1.0; chunked responses are not supported.

Serial output uses 115200 baud. Events report connection progress, delivery outcomes,
and configuration errors without printing bot tokens, passwords, or recipient IDs.
There is no HTTP API, MQTT interface, inbound bot command processing, or persistent queue.

Examples are `WiFi connected`, `Waiting for network time`, and
`Message sent; recipient index=0; attempt=1`. Recipient indices are zero-based; attempts
are one-based and count actual transport calls. Failed calls report `Message attempt failed`
or, on the fifth failure, `Message attempts exhausted`.

OTA begins on the first Wi-Fi connection and is handled on each connected loop pass,
including after reconnection. It does not wait for NTP or Telegram delivery. A null or
empty `OTA_PASSWORD` leaves OTA unauthenticated. Configure a strong password and use a
trusted LAN; do not expose the update port to the internet. No firmware upload or live
Telegram send is part of ordinary local tests or CI.

## Hardware connections

| Example pin | ESP8266 GPIO | Peripheral and polarity |
| --- | --- | --- |
| D1 | 4 | External LED, active high; use a current-limiting resistor. |
| D7 | 13 | Buzzer, active high; use a driver if its electrical load requires one. |
| D3 | 0 | Button to ground, active low, initialized with `INPUT_PULLUP`. |
| D4 | 2 | Board LED configuration retained; no runtime use. |

GPIO0 is a boot-strapping pin: holding the D3 button low while powering up or resetting
enters the ESP8266 serial bootloader. Verify the circuit against your board and peripheral
ratings before deployment. GPIO configuration occurs explicitly during startup.

The startup buzzer lasts ten seconds and takes priority over Wi-Fi patterns. A connection
attempt progress event requests 100 ms off, 100 ms on, then 100 ms off, once per second.
Successful connection gives 500 ms off, 500 ms on, 1000 ms off, 500 ms on, then 500 ms off.
The external LED is high while connecting and low when connected. Successful delivery
requests 1000 ms low, then 500 ms high, ending high. A button press
cancels the active and pending buzzer patterns and leaves the buzzer inactive; future
events can request another pattern. It does not cancel Telegram delivery.

## Deployment

For USB flashing, configure the local file, connect the board, and run:

```sh
pio run -e nodemcuv2 -t upload --upload-port /dev/ttyUSB0
pio device monitor -e nodemcuv2
```

Replace the example serial port with the port assigned by your operating system.
For OTA, use a local-network IP and the matching password:

```sh
PLATFORMIO_UPLOAD_FLAGS='--auth=<ota-password>' \
    pio run -e nodemcuv2_ota -t upload --upload-port '<device-ip>'
```

Avoid placing real passwords in shared shell history. There is no filesystem upload.
USB flashing remains the recovery path when Wi-Fi, OTA credentials, or firmware fail.
A flash or restart creates a new startup notification, so use designated test recipients
when validating deployment.

## Diagnostics and troubleshooting

- If Wi-Fi never connects, check static addresses and credentials locally. Attempts
  expire after 15 seconds and are retried after five seconds; connection loss initiates recovery.
- If delivery waits with Wi-Fi connected, check that NTP access is permitted. TLS requires
  a Unix timestamp on or after 2024-01-01; time acquisition is retried every 60 seconds.
- If a recipient exhausts retries, check the bot and chat configuration locally. Exhausted
  work is not automatically reset until reboot.
- If the buzzer or button behaves incorrectly, check polarity, grounding, and the D3 boot constraint.
- If OTA or cancellation briefly stalls during a send, see the synchronous transport limitation below.
- If formatting fails, activate the environment and confirm clang-format 18.1.8 is installed.

## Known limitations

Delivery state exists only in RAM. A restart produces another startup event; a lost API
response can cause a duplicate on retry even if Telegram accepted the earlier request.
There is no exactly-once guarantee, durable queue, or sensor-based power measurement.
The fixed notification message/recipient buffers use approximately 1 KiB of static RAM.
Each transport attempt allocates an 8 KiB response buffer plus a terminator on the heap
with checked allocation failure. The JSON body is limited to 3200 bytes and the complete
request uses roughly 4 KiB; JSON and TLS buffers add separate allocations. BearSSL alone
uses a 16 KiB receive and 4 KiB transmit buffer, with roughly 24 KiB total TLS memory. These are memory assumptions,
not measured peak heap use on the target. Result JSON filtering retains only `ok`.

The 512-byte message limit permits up to 3072 bytes of JSON-escaped text. The 8 KiB response
limit accommodates ordinary echoed-message metadata, but unusually large metadata can
still cause a failed attempt and a duplicate retry after Telegram accepted the message.

Wi-Fi, time readiness, signals, and retry scheduling advance incrementally. The Telegram
adapter makes one synchronous HTTPS request per attempt, using the pinned bot library's
trust root. It avoids that library's internal send retries. DNS and TCP connect allow one
second each, the framework TLS handshake allows 15 seconds, request write allows one
second for each of two engine waits plus a 300 ms TCP acknowledgment wait. The bounded request
fits one 4 KiB plaintext TLS record. The response window allows three seconds plus a final
one-second I/O wait; cleanup allows one engine wait and a 1 ms acknowledgment wait.
Their conservative configured budget is 25 seconds before scheduling and computation overhead.
OTA service, button polling, and signal transitions can stall during the call, extending active
buzzer/LED phases. Actual
worst-case attempt latency and TLS heap usage still require measurement on hardware.
The response loop yields for framework background work; it does not service application
signals, buttons, or OTA while collecting the response.

Host tests and firmware compilation cannot establish electrical safety, physical timing,
Wi-Fi/NTP recovery, real Telegram delivery, or OTA operation. These require an explicitly
authorized hardware session with test recipients.

## Engineering guidance

See [TECHNICAL_CONTEXT.md](TECHNICAL_CONTEXT.md) for the adopted engineering standard,
[AGENTS.md](AGENTS.md) for project contracts and verification rules, and
[REFACTORING_PLAN.md](REFACTORING_PLAN.md) for the original implementation plan.
