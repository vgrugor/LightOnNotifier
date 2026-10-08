# LightOnNotifier engineering instructions

This project adopts [TECHNICAL_CONTEXT.md](TECHNICAL_CONTEXT.md), standard version 1.0.0.
Follow its applicable requirements. The project is ESP8266 Arduino firmware, with no
web interface, persistent storage, filesystem assets, or physical light sensor.

## Purpose and repository map

The device interprets every boot as a `LIGHT_ON` event and sends the configured Ukrainian
Telegram message to multiple recipients. An external LED, buzzer, and cancellation button
provide local feedback. Static-address Wi-Fi, NTP/TLS readiness, serial diagnostics, and OTA
are the deployed interfaces.

- `include/domain/`: device-independent event and value contracts.
- `include/application/`, `src/application/`: lifecycle, timed signals, connection/time policies,
  atomic static-network validation, and per-recipient notification state; keep them independent
  of Arduino and device libraries.
- `include/infrastructure/`, `src/infrastructure/`: board/network/GPIO/Telegram/OTA adapters.
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
```

The final command requires existing local configuration. Use `nodemcuv2_ci` to compile
with generated placeholders under `.pio` without accessing local credentials. Native tests
must never depend on live Wi-Fi, Telegram, a physical board, real waits, or test order.
Cover interval boundaries and `uint32_t` wraparound when changing timed behavior.
The native source filter compiles `application/*.cpp`, `presentation/EventNotifier.cpp`,
`infrastructure/telegram/HttpResponse.cpp`, and `infrastructure/telegram/TelegramAcknowledgement.cpp`.
HTTP framing and filtered acknowledgment parsing are actual production code, tested with
the firmware's pinned ArduinoJson 7.4.2 revision and Unity 2.6.1.

Formatting uses clang-format 18.1.8 and `.clang-format`; the check script rejects another
formatter version. `python scripts/check_format.py --fix` applies formatting to public C++
headers, firmware/test sources, and the configuration example, excluding local credentials.
CI uses the same format, host test, and placeholder firmware build commands. CI configuration
is not evidence of a passing remote run; report execution results separately.

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
patterns; they never wait for completion. The startup buzzer is active for ten seconds and
takes priority over Wi-Fi patterns. During startup, coalesce connectivity into one pending
pattern, with connection success taking priority over progress. Progress events occur once
per second and request 100 ms off, 100 ms on, then 100 ms off.
The connected pattern uses 500 ms off, 500 ms on, 1000 ms off, 500 ms on, then 500 ms off.
Button cancellation drops active and pending buzzer patterns and leaves the buzzer inactive;
future events can request new patterns. It does not cancel delivery. The LED is high during
connection and low on connection success. Successful delivery gives 1000 ms low, then
500 ms high, and ends high. Repeated delivery events restart that indication.

### Connection and calendar time

Application code owns Wi-Fi attempts: allow 15 seconds per attempt, wait five seconds
before retry, and publish progress once per second. Reconnect after connection loss.
Validate static IP, gateway, and subnet before starting; invalid settings prevent connection
and must not silently fall back to DHCP. `parseStaticNetwork()` validates four decimal octets,
a contiguous mask other than all-zero/all-one, distinct unicast host addresses on the same
subnet, and exclusion of network/broadcast addresses. It changes its output only after full
validation succeeds. Check the return value of adapter configuration.

Calendar time is separate from monotonic interval timing. Start NTP without a wait loop and
retry acquisition every 60 seconds while unavailable. TLS time is considered valid at Unix
time 1704067200 or later (2024-01-01 UTC). Delivery waits when time is invalid and resumes
after recovery. The network time adapter uses UTC with `pool.ntp.org` and `time.nist.gov`.
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
roughly 4 KiB, and JSON/TLS allocations are additional. BearSSL has a 16 KiB receive and
4 KiB transmit buffer and needs roughly 24 KiB total. These estimates require peak-heap validation on the target; keep input
and response bounds when changing the queue. Result JSON filtering retains only `ok`.

Each recipient has at most five transport attempts per boot. A failure waits one second
before another attempt; transport calls have at least a 200 ms global interval. Wi-Fi and
TLS-time unavailability do not consume attempts. Preserve acknowledged successes and retry
state across reconnects. Exhaustion remains terminal until reboot. Restart creates a new
event; no persistence or exactly-once delivery exists. A response loss can cause duplicate
delivery on retry. Diagnostics must use recipient indices rather than private IDs.

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
do not expose it to the public internet. Tokens, passwords, static addresses, and chat IDs
belong only in ignored local configuration. Test/CI fixtures contain placeholders only.

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

The adopted formatter was introduced with the architectural refactor as one working change
rather than a separate formatting-only commit. The affected **Code style / Formatting tools**
SHOULD rule recommends dedicated adoption before broad formatting. The files are being replaced
as part of this refactor; replacement requirement: use the pinned check for all resulting code
and review semantics independently of formatting. Avoid unrelated format changes in future work.

There is no separately deployed filesystem image. Actual button response, output timing,
Wi-Fi/NTP outage recovery, OTA reachability, Telegram delivery, and transport latency/heap
usage require hardware verification. Preserve the shared standard file; document future
project-specific exceptions here with their reason and replacement requirement.
