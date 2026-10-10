
# LightOnNotifier

ESP8266 firmware that sends a Ukrainian Telegram notification when the device starts,
with local LED and buzzer feedback. A startup represents restored power; there is no
separate light sensor.

## Features

- Wi-Fi with static addressing, finite connection attempts, and automatic reconnect.
- A startup notification retained in RAM until Wi-Fi and valid TLS calendar time are available.
- Independent delivery and retries for multiple Telegram recipients.
- Timed LED and buzzer indications; an active-low button cancels the current buzzer signal.
- A mobile-friendly Ukrainian local web interface for Wi-Fi, Telegram, sound, and LED settings.
- Versioned configuration in LittleFS with two alternating records and Wi-Fi change rollback.
- Password-protected setup/recovery access point and authenticated device status.
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
acknowledgment tests. ArduinoOTA, ESP8266WebServer, and LittleFS come from the pinned ESP8266
Arduino framework. Host tests also require a working native C++ compiler. Browser tests use
Node.js 22, pnpm 11.25.0, Playwright 1.62.1, and Chromium or a local Chrome installation.

## Repository structure

| Path | Purpose |
| --- | --- |
| `include/domain/` | Neutral event and state contracts. |
| `include/application/`, `src/application/` | Static network and settings validation, connection, time, delivery, and signals. |
| `include/infrastructure/`, `src/infrastructure/` | ESP8266, GPIO, TLS/Telegram, LittleFS, web, configuration, and OTA adapters. |
| `include/presentation/`, `src/presentation/` | Event dispatch and serial formatting. |
| `src/main.cpp` | Hardware initialization, dependency wiring, and lifecycle coordination. |
| `test/` | Native behavior tests using fake adapters and deterministic time. |
| `tests/browser.cjs` | Browser checks against a local mock controller. |
| `scripts/` | Formatting checks and placeholder configuration generation for CI. |

Application and domain code do not include Arduino or transport libraries. Hardware
and service adapters implement their contracts. HTML, CSS, and JavaScript are embedded in the
firmware; LittleFS stores configuration only. No separate filesystem image is deployed.

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
`src/infrastructure/env.cpp` with your deployment settings. Add a unique `SETUP_PASSWORD` to
an older local configuration file; do not print or commit it. Record the per-device setup
password in a secure place before installing the device. The example password is rejected
by the firmware. A clean checkout can compile the CI environment without a local credential
file. The CI fixture intentionally cannot open a usable setup access point.

For browser tests, install Node.js 22, enable Corepack, and select the pinned pnpm version:

```sh
corepack enable
corepack prepare pnpm@11.25.0 --activate
```

## Configuration and secrets

`include/infrastructure/env.h` declares the configuration; the tracked
`src/infrastructure/env.cpp.example` contains placeholders. The local implementation
`src/infrastructure/env.cpp` is ignored and must never be committed or printed in logs.

| Setting | Purpose |
| --- | --- |
| `WIFI_SSID`, `WIFI_PASSWORD` | Access point credentials. |
| `WIFI_IP`, `WIFI_GATEWAY`, `WIFI_SUBNET` | Valid IPv4 static address, gateway, and subnet. |
| `OTA_HOSTNAME`, `OTA_PASSWORD` | Local OTA hostname and update authentication. |
| `SETUP_PASSWORD` | Unique 10–63-byte printable ASCII initial and recovery secret for this device; also protects its setup access point. |
| `EXTERNAL_LED_PIN`, `BUZZER_PIN`, `BUTTON_PIN` | Local peripheral pin assignments. |
| `BOARD_LED_PIN` | Compatibility setting; the board LED is currently unused. |
| `BOT_TOKEN` | Telegram bot token. |
| `CHAT_IDS`, `CHAT_IDS_COUNT` | One to eight unique chat identifiers, at most 33 bytes each; formats below. |
| `LIGHT_ON_MESSAGE` | Nonempty UTF-8 message, at most 512 bytes, including multibyte characters. |

Invalid notification configuration prevents delivery. Invalid static addressing must
be corrected before Wi-Fi can connect; the device does not silently switch to DHCP.
CI generates a separate placeholder translation unit under `.pio` and excludes the
local `env.cpp` from its firmware sources.

On first boot after upgrading, valid build-time Wi-Fi and Telegram values initialize the
saved configuration once. Later web edits use LittleFS. A small EEPROM marker records that
the device was provisioned so a damaged filesystem does not reimport compiled credentials.
Version-one through version-three records migrate in memory to version four; the next
settings save writes the new format while retaining other values. Version one receives a
ten-second startup sound, versions one and two receive disabled night mode with hours 22–7,
and all migrated records receive 10% night LED brightness. Version-three night schedules
are preserved.
A deliberate web reset clears the saved configuration and does not reimport build-time credentials.
If no usable configuration
exists, the device opens a protected setup access point named `LightOn-XXXXXX` with address
`192.168.4.1`; the suffix comes from the ESP8266 chip ID. Connect using `SETUP_PASSWORD`,
open `http://192.168.4.1/`, and sign in with the same password. Save Telegram settings, then
apply Wi-Fi settings. Open the proposed station IP address from that network and confirm it
within 120 seconds. Set an administrator password of 10–64 bytes in the Device section.
Unconfirmed network settings revert automatically. If the stored configuration is unreadable,
the device enters protected setup rather than silently importing compiled credentials.
The Wi-Fi change endpoint acknowledges the provisional settings before disconnecting from the
old network. If the browser loses that response, check the proposed IP address and confirm
there; do not immediately submit the change again. The previous network returns after 120
seconds without confirmation.

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
pnpm install --frozen-lockfile
pnpm exec playwright install chromium
pnpm test:e2e
```

`nodemcuv2_ci` builds without production credentials. `nodemcuv2` uses the ignored
local configuration. Native tests compile actual application modules, event dispatch,
HTTP response framing, Telegram acknowledgment parsing, settings storage, and LED PWM with
fake LittleFS/EEPROM/GPIO adapters. They use no network or
physical board and cover input validation, timing boundaries, and counter wraparound.
To apply the pinned formatter, run `python scripts/check_format.py --fix`; it excludes
the local credential file. On a Mac with Chrome already installed, set `PW_CHANNEL=chrome`
for `pnpm test:e2e` instead of installing Chromium. CI runs formatting, host tests, the
placeholder firmware build, and browser checks on pushes and pull requests.

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
There is no MQTT interface, inbound bot command processing, or persistent notification queue.

Examples are `WiFi connected`, `Waiting for network time`, and
`Message sent; recipient index=0; attempt=1`. Recipient indices are zero-based; attempts
are one-based and count actual transport calls. Failed calls report `Message attempt failed`
or, on the fifth failure, `Message attempts exhausted`.

OTA begins on the first Wi-Fi connection and is handled on each connected loop pass,
including after reconnection. It does not wait for NTP or Telegram delivery. A null or
empty `OTA_PASSWORD` leaves OTA unauthenticated. Configure a strong password and use a
trusted LAN; do not expose the update port to the internet. No firmware upload or live
Telegram send is part of ordinary local tests or CI.

The embedded web page is available at `http://<device-ip>/` on the local network. Open it
from a phone or computer on the same network. Pages are in Ukrainian and work without an
internet connection. The five sections are Status, Sound and LED, Wi-Fi, Telegram, and Device.
The page stores nothing merely because it was opened. Save actions write changed settings;
network settings require confirmation from the candidate station IP address. The physical
button cancels sound on a short press and opens the protected setup access point after a
five-second hold following boot. Recovery access closes after ten minutes unless a Wi-Fi
change is being confirmed.

The HTTP API uses JSON responses and form-encoded POST bodies. `POST /api/login` accepts a
`password` and establishes a 30-minute session cookie; it returns a CSRF token. Authenticated
GET routes are `/api/session`, `/api/status`, and `/api/settings`. Authenticated POST routes are
`/api/signals`, `/api/telegram`, `/api/wifi`, `/api/wifi/confirm`, `/api/password`,
`/api/preview`, `/api/test`, `/api/reset`, and `/api/restart`. Mutations require the session
cookie, matching `Origin` and `X-CSRF` header, and a form body no larger than 2048 bytes.
Settings mutations include the current numeric `revision`; stale revisions return HTTP 409.
Errors return JSON with an `error` string, usually with HTTP 400, 401, 403, 409, 413, 415, or
500. Neither status nor settings responses return saved passwords or bot tokens.

`/api/status` reports `wifi`, `ip`, `uptimeMs`, `firmware`, `timeReady`, `delivery`,
`ledActive`, `ledBrightnessPercent`, `buzzerActive`, `quietHoursEnabled`, `quietHoursActive`,
`wifiPending`, `recovery`, `storageReady`, `unsupportedSchema`,
`configured`, numbered
`recipients` with `outcome` and `attempts`, and per-recipient test results. Delivery is one of
`empty`, `invalid`, `waiting`, `partial`, `delivered`, or `exhausted`. A test send is an explicit
action available only after the startup event is terminal and Wi-Fi/time are ready; it makes
one attempt per selected recipient. A failed response can still follow an accepted message,
so manual repetition may produce a duplicate. Sound previews and LED previews do not send
Telegram messages.

Web authentication protects local access but HTTP does not encrypt browser traffic or
submitted secrets. Use a trusted LAN; do not expose the web port to the internet. ArduinoOTA
uses its separate configured password and does not wait for NTP or Telegram delivery.

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

When enabled, the startup buzzer lasts the configured 1–60 seconds (ten seconds by default)
and takes priority over Wi-Fi
patterns. A connection
attempt progress event requests 100 ms off, 100 ms on, then 100 ms off, once per second.
The manual startup-sound preview remains limited to three seconds.
Successful connection gives 500 ms off, 500 ms on, 1000 ms off, 500 ms on, then 500 ms off.
The external LED defaults to steady on during startup and connection, off while waiting for
delivery, and steady on after all recipients are acknowledged. It can be turned off globally,
or each state can be off, steady, or blink with 500 ms on/500 ms off phases. Successful delivery
requests 1000 ms off, then 500 ms on, before returning to the selected state. A button press
cancels the active and pending buzzer patterns and leaves the buzzer inactive; future
events can request another pattern. It does not cancel Telegram delivery.

Night mode is configured in the Sound and LED section. It silences the buzzer and sets the
LED's lit phases to the chosen night brightness (whole percentages, 0–100; default 10%).
0% turns the LED off and 100% keeps full brightness. `POST /api/signals` and
`GET /api/settings` use `quietHoursEnabled` (Boolean; POST uses `0`/`1`), `quietStartHour`
and `quietEndHour` (integers 0–23), and `quietLedBrightnessPercent` (integer 0–100, required
on save). Invalid or missing brightness returns HTTP 400 before settings change.
The interval includes its start and excludes its end; 22–7 means 22:00–07:00, using Kyiv
local time with the pinned seasonal rule. Enabled intervals cannot have identical hours.
Until time is available, the device uses daytime behavior: lit LED phases use 100% brightness
and enabled buzzer categories may play. Valid time activates the saved night schedule on the
next signal update; losing valid time restores daytime behavior without replaying old sounds.
It also applies to delivery flashes; daytime lit phases use 100%.
Global LED off and off phases remain off. `ledBrightnessPercent` reports the current output
level, including zero during an off phase. Buzzer signals suppressed at night are not replayed
afterward. Telegram delivery continues independently of night mode.

The Preview block has a separate LED brightness field (whole percentages 0–100, initially
100%). Select a mode and press LED to preview that exact percentage for three seconds,
including during night mode. The saved settings and revision do not change. Completion,
Wi-Fi transitions, or disabling the LED restore the regular output state. Global LED off
and the startup window prevent LED preview. The authenticated `POST /api/preview` accepts
`kind=led`, `mode=0|1|2` (off/steady/blink), and optional `brightnessPercent=0..100`.
Malformed or out-of-range percentages return HTTP 400 without changing an active preview.
For older clients that omit `brightnessPercent`, the current scheduled brightness is used.

The active-high LED on D1/GPIO4 now uses software PWM at the core's default 1 kHz, with
range 0–1023. The percentage sets electrical duty; perceived brightness is not calibrated.
Pin assignment, resistor requirements, and output polarity are unchanged. Actual brightness
and PWM behavior on the connected LED require hardware verification after firmware upload.

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
Before flashing an older local deployment, add a unique `SETUP_PASSWORD` to its ignored
`env.cpp`; the new firmware requires that symbol. Keep a recovery copy outside the device.
Normal OTA updates retain compatible LittleFS settings. Back up deployment settings before
changing firmware versions, because an older firmware may not understand the stored schema.

## Diagnostics and troubleshooting

- If Wi-Fi never connects, check static addresses and credentials locally. Attempts
  expire after 15 seconds and are retried after five seconds; connection loss initiates recovery.
- If delivery waits with Wi-Fi connected, check that NTP access is permitted. TLS requires
  a Unix timestamp on or after 2024-01-01; time acquisition is retried every 60 seconds.
- If a recipient exhausts retries, check the bot and chat configuration locally. Exhausted
  work is not automatically reset until reboot.
- If the buzzer or button behaves incorrectly, check polarity, grounding, and the D3 boot constraint.
- If web setup is unavailable, check `SETUP_PASSWORD`: the example value and values outside
  10–63 bytes cannot open the access point. Hold the button for five seconds only after boot
  to enter recovery; its SSID is `LightOn-XXXXXX` at `192.168.4.1`.
- If Wi-Fi changes are not confirmed from the new station address within 120 seconds, the
  previous network settings return automatically. Refresh the browser at the old address.
- If a save reports a storage error, do not assume it persisted; check LittleFS availability
  in the authenticated Status section.
- If OTA or cancellation briefly stalls during a send, see the synchronous transport limitation below.
- If formatting fails, activate the environment and confirm clang-format 18.1.8 is installed.

## Known limitations

Delivery state exists only in RAM. A restart produces another startup event; a lost API
response can cause a duplicate on retry even if Telegram accepted the earlier request.
There is no exactly-once guarantee, durable queue, or sensor-based power measurement.
Configuration is stored in LittleFS, but the delivery queue is still RAM-only. The web page
and its assets are embedded in firmware; there is no separately flashed filesystem image.
The fixed notification message/recipient buffers use approximately 1 KiB of static RAM.
Each transport attempt allocates an 8 KiB response buffer plus a terminator on the heap
with checked allocation failure. The JSON body is limited to 3200 bytes and the complete
request uses roughly 4 KiB; JSON and TLS buffers add separate allocations. BearSSL uses
4 KiB receive and 4 KiB transmit buffers. Telegram must negotiate 4 KiB MFLN; otherwise
the attempt fails and the serial log identifies the negotiation failure. A 16 KiB receive
buffer exhausted TLS heap on the deployed ESP8266, while the 4 KiB configuration completed
both startup sends. Peak heap under concurrent web/TLS use remains unmeasured. Result JSON
filtering retains only `ok`.

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

Web request headers are checked before body parsing. POST bodies are capped at 2048 bytes;
one form at a time is handled by the ESP8266 web server. The browser may appear disconnected
during a synchronous Telegram send and must fetch current state before retrying a mutation.
Web sessions and test-send progress are RAM-only. A restart invalidates them. Configuration
records are versioned and alternating, but their power-loss behavior and compatibility across
OTA changes still require hardware verification. The browser API is HTTP only.

Host tests and firmware compilation cannot establish electrical safety, physical timing,
Wi-Fi/NTP recovery, LittleFS resilience, actual web provisioning, real Telegram delivery,
heap use under concurrent web/TLS traffic, or OTA operation. These require an explicitly
authorized hardware session with test recipients.

## Engineering guidance

See [TECHNICAL_CONTEXT.md](TECHNICAL_CONTEXT.md) for the adopted engineering standard,
[AGENTS.md](AGENTS.md) for project contracts and verification rules, and
[PRD.md](PRD.md) for the Ukrainian web configuration requirements.
