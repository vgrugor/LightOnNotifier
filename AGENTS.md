# LightOnNotifier engineering instructions

This project adopts [TECHNICAL_CONTEXT.md](TECHNICAL_CONTEXT.md), standard version 1.0.0.
Follow its applicable requirements. The project is ESP8266 Arduino firmware with an embedded
local web interface and LittleFS configuration storage, but no separately deployed filesystem
assets or physical light sensor.

## Purpose and repository map

The device interprets every boot as a `LIGHT_ON` event and sends the configured Ukrainian
Telegram message to multiple recipients. An external LED, buzzer, and cancellation button
provide local feedback. Static-address Wi-Fi, NTP/TLS readiness, the protected web interface,
serial diagnostics, and OTA are the deployed interfaces.

- `include/domain/`: device-independent event and value contracts.
- `include/application/`, `src/application/`: lifecycle, timed signals, connection/time policies,
  atomic static-network validation, settings validation, and per-recipient notification state;
  keep them independent of Arduino and device libraries.
- `include/infrastructure/`, `src/infrastructure/`: board/network/GPIO/Telegram/OTA adapters,
  LittleFS settings storage, and local web server with embedded page assets.
- `include/presentation/`, `src/presentation/`: event dispatch and serial event formatting.
- `src/main.cpp`: static-lifetime dependency wiring and setup/loop coordination.
- `test/`: deterministic host behavior tests for the actual neutral production modules.
- `scripts/`: formatter checks and a placeholder build configuration generator.
- `src/infrastructure/env.cpp.example`: safe configuration example.
- `src/infrastructure/env.cpp`: ignored local credentials; never read, print, overwrite, or commit it.

Public declarations belong under `include/`, with implementations under matching `src/`
paths. Required dependencies are borrowed references. Runtime wiring and callbacks must
outlive every application update and synchronous event dispatch. Avoid mutable singletons.

## Build and verification

Run from the repository root. Install the exact tools in `requirements-dev.txt` in an
isolated Python environment as documented in README. The firmware preserves GNU C++17;
`platformio.ini` declares exact platform/library dependencies and source selection.

```sh
python scripts/check_format.py
pio test -e native
pio run -e nodemcuv2_ci
pio run -e nodemcuv2
pnpm test:e2e
```

The local firmware build requires existing local configuration, including a unique
`SETUP_PASSWORD`. Use `nodemcuv2_ci` to compile
with generated placeholders under `.pio` without accessing local credentials. Native tests
must never depend on live Wi-Fi, Telegram, a physical board, real waits, or test order.
Cover interval boundaries and `uint32_t` wraparound when changing timed behavior.
The native source filter compiles `application/*.cpp`, `presentation/EventNotifier.cpp`,
`infrastructure/telegram/HttpResponse.cpp`, `infrastructure/telegram/TelegramAcknowledgement.cpp`,
`infrastructure/telegram/TelegramConnection.cpp`, `infrastructure/settings/SettingsStore.cpp`,
and `infrastructure/actuators/ExternalLedActuator.cpp` with fake LittleFS/EEPROM/GPIO headers.
HTTP framing and filtered acknowledgment parsing are actual production code, tested with
the firmware's pinned ArduinoJson 7.4.2 revision and Unity 2.6.1.

Formatting uses clang-format 18.1.8 and `.clang-format`; the check script rejects another
formatter version. `python scripts/check_format.py --fix` applies formatting to public C++
headers, firmware/test sources, and the configuration example, excluding local credentials.
CI also installs pinned browser-test dependencies and runs `pnpm test:e2e` with Chromium.
CI configuration is not evidence of a passing remote run; report execution results separately.

Do not run firmware uploads, deployment, or external notifications unless explicitly
authorized for that task. Compile and test changes before completion; report any missing
tools or environmental limits. A build does not establish working hardware.

## Runtime contracts

### Initialization and signals

Initialize GPIO during `setup()`, leave inactive outputs safe, and configure the active-low
button with `INPUT_PULLUP`. The example keeps D1/GPIO4 for the external LED, D7/GPIO13
for the buzzer, and D3/GPIO0 for the button. `BOARD_LED_PIN` is retained but unused.
GPIO0 must be high at reset for normal boot; a pressed D3 button selects the bootloader.
Do not alter pins, polarity, or boot behavior as incidental cleanup.

Use unsigned elapsed-time subtraction on `uint32_t` ticks. Signal callbacks request
patterns; they never wait for completion. The startup buzzer duration is configurable from
one to 60 seconds and defaults to ten seconds. It takes priority over Wi-Fi patterns.
During startup, coalesce connectivity into one pending
pattern, with connection success taking priority over progress. Progress events occur once
per second and request 100 ms off, 100 ms on, then 100 ms off.
The connected pattern uses 500 ms off, 500 ms on, 1000 ms off, 500 ms on, then 500 ms off.
Button cancellation drops active and pending buzzer patterns and leaves the buzzer inactive;
future events can request new patterns. It does not cancel delivery. By default the LED is
high during connection and low while waiting after connection. Successful delivery gives
1000 ms low, then 500 ms high before the selected state resumes. Repeated delivery events
restart that indication.

Saved sound settings independently enable the startup, Wi-Fi progress, and Wi-Fi success
patterns and set the startup duration. Disabling a category stops its active/pending sound on
the next update. Saved LED settings include a global off switch and off/steady/500 ms blink
modes for startup, connecting, waiting, idle, and error. Startup LED indication lasts ten
seconds independently of sound.
Success indication remains 1000 ms off then 500 ms on when enabled, then returns to the
configured state. Invalid configuration has priority over success; global off has priority
over every indication. Idle means startup has ended and all recipients are acknowledged.
Partial or pending delivery is not idle. Short button presses cancel sound; a five-second
hold after boot opens protected recovery access for ten minutes. D3 held at reset still selects
the hardware bootloader.

Night mode uses the existing daily interval: start hour inclusive, end hour exclusive,
hours 0–23, including intervals across midnight. Enabled intervals must have different
start and end hours. It silences all buzzer patterns and sound previews, drops pending
sounds, and does not replay them when the interval ends. If calendar time is unavailable,
signals use daytime behavior: lit LED phases are 100% and enabled sound categories may play.
When valid time becomes available, the saved schedule applies on the next signal update.
Loss of valid time returns to daytime behavior without replaying suppressed sounds.
`quietLedBrightnessPercent` is a whole percentage
from 0 to 100, defaulting to 10, and applies to automatic lit LED phases and delivery
indication while night mode is active. Daytime brightness is 100%. LED previews with an
explicit percentage temporarily override that level for three seconds; they do not persist
settings and return to the scheduled level after completion or cancellation. Legacy LED
previews without an explicit percentage use the scheduled level. Off phases and
the global LED off switch stay off. The active-high external LED uses software PWM
on its existing pin, with range 0–1023 and the pinned core's default 1 kHz frequency;
percentages represent PWM duty, not calibrated perceived brightness. PWM is updated
only when duty changes. Schedule transitions take effect on the next signal update,
which can be delayed by the documented synchronous Telegram call.
Preview percentages must be integers 0–100; invalid values leave any existing preview
unchanged. LED preview is unavailable during startup or with global LED off. Only the
preview writes LED output while active, preventing intermediate automatic brightness pulses.

### Connection and calendar time

Application code owns Wi-Fi attempts: allow 15 seconds per attempt, wait five seconds
before retry, and publish progress once per second. Reconnect after connection loss.
Validate static IP, gateway, and subnet before starting; invalid settings prevent connection
and must not silently fall back to DHCP. `parseStaticNetwork()` validates four decimal octets,
a contiguous mask other than all-zero/all-one, distinct unicast host addresses on the same
subnet, and exclusion of network/broadcast addresses. It changes its output only after full
validation succeeds. Check the return value of adapter configuration.

Web Wi-Fi changes are provisional for 120 seconds. Confirmation requires authenticated access
through the candidate station address; failure or timeout restores the prior configuration.
No DHCP fallback is introduced. A protected access point is available before first setup and
through a post-boot long button hold, but not merely because station Wi-Fi is unavailable.

Calendar time is separate from monotonic interval timing. Start NTP without a wait loop and
retry acquisition every 60 seconds while unavailable. TLS time is considered valid at Unix
time 1704067200 or later (2024-01-01 UTC). Delivery waits when time is invalid and resumes
after recovery. NTP uses `pool.ntp.org` and `time.nist.gov`; the Unix clock remains UTC,
while daily schedules use Kyiv local time via the pinned core's `TZ_Europe_Kiev` rule.
Calendar corrections must not change retry/signal intervals.

Start OTA once on the first connected pass, before notification delivery. Service it on
every connected loop pass and resume handling after reconnect; NTP readiness does not gate
OTA. A null or empty OTA password disables authentication. Do not place blocking network
operations in interrupt handlers or observer callbacks.

### Notification delivery

Keep one startup event in RAM. Bound notification input to eight unique identifiers,
33 bytes per identifier, and a nonempty message of at most 512 UTF-8 bytes. Accept canonical
nonzero decimal IDs with an optional leading minus and no leading zeros, or `@` usernames
with 5–32 ASCII letters, digits, or underscores. Username duplicate checks are case-insensitive;
numeric ID/username aliases for the same chat are not resolved. Validate the entire notification
before sending any recipient. The notification service
copies message and recipient data into fixed owned buffers; network/OTA configuration pointers
must remain valid for their borrowing adapters.
Fixed message/recipient buffers consume approximately 1 KiB of static RAM. The transport has
an 8 KiB heap response buffer plus a terminator, allocated per attempt with `nothrow` ownership
and checked failure. A JSON request body is limited to 3200 bytes, the complete request uses
roughly 4 KiB, and JSON/TLS allocations are additional. BearSSL uses 4 KiB receive and
4 KiB transmit buffers; the connection requires successful 4 KiB MFLN negotiation with
Telegram. A 16 KiB receive buffer caused TLS allocation failure (`-1000`) on the deployed
ESP8266; 4 KiB MFLN completed two startup sends on hardware. Peak heap under concurrent
web/TLS use still needs validation. Keep input and response bounds when changing the queue.
Result JSON filtering retains only `ok`.

Each recipient has at most five transport attempts per boot. A failure waits one second
before another attempt; transport calls have at least a 200 ms global interval. Wi-Fi and
TLS-time unavailability do not consume attempts. Preserve acknowledged successes and retry
state across reconnects. Exhaustion remains terminal until reboot. Restart creates a new
event; no persistence or exactly-once delivery exists. A response loss can cause duplicate
delivery on retry. Diagnostics must use recipient indices rather than private IDs.

Settings for the next boot live in versioned, integrity-checked LittleFS records. Alternating
record files preserve the previous complete version while a replacement is written. An EEPROM
marker distinguishes first migration from damaged or reset storage, preventing a silent
reimport of compiled credentials after provisioning. Startup
message/recipient data and the transport token for an in-progress event remain owned copies;
web edits take effect for a later boot or an explicit test. Reset stores an unconfigured
tombstone and must not reimport compiled credentials. Test messages have one attempt per
selected recipient and no automatic retry; they do not affect startup delivery state.
Version-one through version-three settings records migrate to version four in memory and
are rewritten only when settings are saved. Version one gets the default ten-second startup
sound; versions one and two get night mode disabled with hours 22–7. All migrated records
get 10% night LED brightness, preserving version-three night schedules and other settings.

Acknowledgment requires valid, complete HTTP 200 framing and a JSON Boolean `ok: true`.
Content-Length completion ends collection without waiting for disconnect; close-delimited
responses are accepted when no length is declared. Malformed framing, truncated/oversized
content, or any transfer encoding fails the attempt. HTTP/1.0 requests avoid chunked bodies.
The 512-byte accepted message can expand to 3072 escaped JSON bytes. The 8 KiB response bound
fits ordinary echoed metadata, but unusually large Telegram metadata can still reject an
accepted delivery and cause a duplicate retry.

Serial delivery fields are zero-based `recipient index` and one-based `attempt`.
An acknowledged send prints `Message sent`, a failed attempt prints `Message attempt failed`,
and the fifth failure prints `Message attempts exhausted`. Time transitions print
`Waiting for network time` / `Network time ready`; invalid configuration prints
`Invalid WiFi configuration` or `Invalid notification configuration`. Do not log borrowed
event message text or private configuration.

### Event ownership and subscriptions

Dispatch is synchronous and ordered. Payloads are borrowed only during the callback;
consumers that defer work must copy relevant data into bounded owned storage. Reject null
or duplicate observers, enforce dispatcher capacity, and reject subscription mutation
during dispatch. Nested publishes are dropped. Polymorphic interfaces have virtual destructors.
GPIO adapters and observers borrow their dependencies; do not copy hardware-owning objects.

## Hardware and security assumptions

Verify the deployed board's electrical ratings and use an appropriate LED resistor and
buzzer driver. OTA is a trusted-LAN interface; configure an authentication password and
do not expose it to the public internet. Initial tokens, passwords, static addresses, and
chat IDs belong only in ignored local configuration; web edits are persisted in LittleFS.
Test/CI fixtures contain placeholders only.
Provision a unique printable-ASCII `SETUP_PASSWORD` of 10–63 bytes per device in ignored local
configuration.
The tracked example value is rejected at runtime. The setup access point uses that secret;
the web login accepts it before administrator setup and during physical recovery. The
administrator password is stored as a salted iterative SHA-256 hash. Sessions expire after
30 minutes and state changes require matching origin and CSRF token. The web interface is HTTP
on a trusted LAN: browser traffic and submitted secrets are not encrypted. Never expose it to
the public internet. Only embedded HTML/CSS/JS is served; no filesystem image is uploaded.

## Known limitations and documented exceptions

The pinned Telegram library supplies the trust root. The adapter sends one bounded HTTPS
request itself to avoid the library's internal retries and response accumulation.
The affected shared rule is **Embedded runtime and resource constraints**: required synchronous
operations need bounded timeouts and documented responsiveness effects. This is an explicit
limitation of incremental main-loop scheduling: finite TLS/connect/read timeouts remain
required, and OTA, button polling, and signal updates can stall during a transport call,
extending active buzzer/LED phases.
Do not describe Telegram I/O as nonblocking or promise a loop-response budget without measured
hardware evidence. Replacement requirement: execute at most one transport attempt per eligible
update, schedule all retries in application code, and measure worst-case single-attempt latency
and TLS heap usage in a separately authorized hardware session. DNS and TCP connection
timeouts are one second each, the framework TLS handshake timeout is 15 seconds, request
write has at most two one-second engine waits plus a 300 ms TCP acknowledgment wait; the
bounded request fits one 4 KiB plaintext TLS record. Response collection has a three-second
window plus up to one final second of I/O. Cleanup allows a one-second engine wait and a
1 ms acknowledgment wait. The conservative configured budget is 25 seconds before computation
and scheduling overhead; it is not a measured end-to-end latency guarantee.
The response collection loop calls `yield()` for framework background work. This does
not run application services or establish a measured watchdog or responsiveness budget.
The reduced TLS receive buffer relies on Telegram negotiating 4 KiB MFLN. If it does not,
the attempt fails with a diagnostic instead of accepting potentially oversized TLS records.

The ESP8266 web server's pre-route hook can inspect complete headers only when they fit in
the first TCP receive buffer: the framework's `WiFiClient::peekBytes()` does not copy across
chained buffers. Split headers therefore continue through the framework parser; the parsed
Content-Length, transfer encoding, and form fields are checked before application state
changes, but the framework may allocate the declared body before those checks. The affected
shared rule is **Embedded runtime and resource constraints**. This is an explicit limitation
on a trusted LAN. Replacement requirement: use a bounded streaming HTTP parser or a
framework hook that exposes complete headers before body allocation; verify peak heap use
and malformed split requests on hardware. The pre-route inspection buffer is 3073 heap
bytes per POST and is released before the route runs.

The adopted formatter was introduced with the architectural refactor as one working change
rather than a separate formatting-only commit. The affected **Code style / Formatting tools**
SHOULD rule recommends dedicated adoption before broad formatting. The files are being replaced
as part of this refactor; replacement requirement: use the pinned check for all resulting code
and review semantics independently of formatting. Avoid unrelated format changes in future work.

There is no separately deployed filesystem image. Actual button response, output timing,
Wi-Fi/NTP outage recovery, provisioning and rollback, LittleFS power-loss behavior,
web/Telegram concurrent heap usage, OTA reachability, Telegram delivery, and transport
latency require hardware verification. Preserve the shared standard file; document future
project-specific exceptions here with their reason and replacement requirement.
