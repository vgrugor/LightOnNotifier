# LightOnNotifier refactoring plan

This plan adopts [TECHNICAL_CONTEXT.md](TECHNICAL_CONTEXT.md), version 1.0.0,
for the existing ESP8266 firmware. It describes proposed work; it does not claim
that the changes or verification below have already been completed.

## Scope and compatibility

- Keep PlatformIO, Arduino, the `nodemcuv2` board, Wi-Fi, Telegram, OTA, serial
  diagnostics, the external LED, buzzer, and cancellation button.
- Keep the configured Ukrainian notification text and multi-recipient delivery.
- Treat `LIGHT_ON` as a startup event: the current firmware has no light sensor
  or separate power-state input. A restart currently triggers another notification.
  Preserve that contract unless an explicitly scoped behavior change replaces it.
- Preserve the ignored local `src/infrastructure/env.cpp`. Never read its values
  into documentation, test fixtures, logs, or CI configuration.
- Do not add a web interface, filesystem assets, persistent delivery queue,
  daily scheduling, or a sensor solely to satisfy optional standard sections.
- Do not change pin assignments, electrical polarity, libraries, or compiler
  language version as incidental cleanup.

## Current-state assessment

| Finding and evidence | Applicable standard requirement | Priority |
| --- | --- | --- |
| `WiFiManager::connect()` loops until connected with no overall timeout; reconnect calls the same loop. | Bounded main-loop work; application-owned retry policy. | P0 |
| `TelegramNotifier::init()` waits up to about ten seconds for NTP; `sendMessage()` performs nested retries and synchronous network calls. | Separate time readiness, scheduling, and transport; bound synchronous operations. | P0 |
| `BuzzerObserver` blocks for the startup signal and pulse sequences; `LedObserver` delays during send indication. All observers execute synchronously. | Callbacks must avoid blocking other work; use elapsed-time state machines. | P0 |
| OTA starts only after startup signaling, Wi-Fi, NTP, and initial Telegram delivery; reconnect can prevent `OTA.handle()` from running. | Lifecycle coordination and responsiveness. | P0 |
| `BUTTON_PIN` is read without explicit input initialization; actuator constructors call `pinMode()` before `setup()`. | Explicit hardware initialization and documented startup sequencing. | P0 |
| Startup behavior lives in `main.cpp`; infrastructure invokes a presentation singleton; Arduino `String` is part of the observer contract. | Composition root, dependency direction, testable device-independent logic. | P1 |
| LED and buzzer observers copy actuator objects; Telegram owns heap objects through raw pointers without an explicit cleanup or reinitialization contract. | Explicit ownership, borrowing, and callback lifetimes. | P1 |
| IP parsing and `WiFi.config()` results are ignored; recipient input and delivery outcomes lack documented contracts. | Configuration validation and external side-effect semantics. | P1 |
| `EventNotifier` permits null or duplicate registration and has no subscription-mutation contract; abstract interfaces lack virtual destructors. | Valid inputs and explicit polymorphic lifetime contracts. | P1 |
| No `AGENTS.md`, formatter configuration, native test environment, behavior tests, or CI is present. README contains only an image. | Project instructions, deterministic formatting, tests, CI, and README structure. | P1 |
| Platform and Git-based dependencies are not pinned to immutable versions. | Reproducible dependency declarations. | P1 |

P0 work removes runtime failure modes. P1 work establishes maintainable boundaries
and repeatable verification. These priorities do not excuse applicable MUST rules
in any new or changed code.

## Target architecture

Use matching declaration and implementation paths under `include/` and `src/`.
Names below are proposed; introduce only boundaries that serve actual behavior
or meaningful host tests.

| Area | Responsibilities and proposed components |
| --- | --- |
| `domain/` | Arduino-independent event identifiers, delivery outcomes, and small state/value types. Remove unused `Device::update()` inheritance if it has no behavioral purpose. |
| `application/` | `NotifierApplication` lifecycle, `ConnectionService` connection scheduling, `NotificationService` per-recipient delivery policy, and `SignalController` timed indication/cancellation. |
| `application/` ports | Explicit interfaces or parameters for monotonic time, connection attempts/status, calendar-time readiness, message sending, button state, and output changes where host tests need substitution. |
| `infrastructure/` | ESP8266 Wi-Fi and time adapters, Telegram transport, GPIO adapters, configuration mapping/validation, and the existing OTA adapter. Move Telegram networking out of `presentation/`. |
| `presentation/` | Serial formatting and event-to-indication translation. Presentation must not own connection or notification retry rules. |
| `src/main.cpp` | Construct dependencies, initialize hardware, invoke application startup/update, and coordinate OTA servicing. No retry loops or notification policy. |
| `test/` | Native behavior suites with fake time, connections, senders, inputs, and outputs. No board, live network, or real delays. |

Dependencies point from concrete adapters toward neutral contracts. Domain and
application headers must compile without Arduino, ESP8266, Telegram, or GPIO
headers. Keep static-lifetime wiring in `main.cpp` where needed for callbacks;
pass required borrowed dependencies by reference.

## Ordered implementation stages

### 1. Establish project contracts and a reproducible baseline

**Changes**

- Add root `AGENTS.md` linking standard version 1.0.0. Describe purpose,
  repository map, dependency boundaries, actual build/test commands, hardware,
  startup behavior, delivery guarantees, limitations, and justified exceptions.
- Rebuild README using the standard heading order. Describe implemented features
  only; document the missing local configuration setup from the placeholder example.
- Confirm the actual board wiring. The example uses external LED D1, buzzer D7,
  button D3, and declares board LED D4, which current runtime code does not use.
  Document button active-low behavior, pull-up expectations, and ESP8266 boot-pin
  constraints before changing initialization.
- Record startup ordering, signal patterns, LED final states, and the current
  maximum of five send attempts per recipient as the baseline. Distinguish observed
  implementation behavior from intended behavior when correcting defects.
- Inventory the locally resolved platform, core/toolchain, and library revisions.
  Pin a verified compatible set in `platformio.ini`; retain existing libraries.
  Determine whether ArduinoOTA is provided by the framework before changing its
  dependency declaration. Record the existing C++ language version.
- Define a placeholder-only configuration fixture for clean builds. Keep local
  credentials ignored; CI must compile without production secrets.

**Acceptance and verification**

- A clean checkout can be configured and built using documented commands.
- Run `pio run -e nodemcuv2` before and after dependency pinning; investigate any
  baseline failure separately. Record firmware size and available memory estimates.
- Review README/AGENTS paths, commands, setup examples, and hardware assumptions.
- Preserve `TECHNICAL_CONTEXT.md` as provided; document adoption without rewriting it.

### 2. Adopt deterministic formatting in a dedicated change

**Changes**

- Add `.editorconfig` and `.clang-format`: UTF-8, LF, final newline, four-space
  C++ indentation, same-line braces, and a practical 100-character line limit.
- Declare the formatter version used by local development and CI. Apply formatting
  separately from behavior changes; translate technical comments to English.
- Standardize project-relative includes, include grouping, and unique path-based
  guards in touched headers. Use `override`, `const`, and supported `constexpr`
  where appropriate; avoid an unrelated compiler-version change.

**Acceptance and verification**

- A second formatter pass produces no changes. Build `nodemcuv2` after broad
  formatting. Review the diff for accidental behavioral changes.

### 3. Introduce testable contracts and explicit lifetimes

**Changes**

- Add `[env:native]` and focused host suites. Filter firmware-only sources and
  dependencies out of native builds; compile the actual neutral production logic.
- Replace `EventNotifier::getInstance()` dependencies with an explicitly wired
  event sink/dispatcher. Move neutral event declarations out of presentation.
- Remove Arduino `String` from application contracts. Define payload ownership:
  synchronous consumers borrow during dispatch; deferred consumers copy into
  bounded owned storage. Document recipient/configuration lifetimes as well.
- If retaining observer subscriptions, prohibit null and duplicate registration,
  bound capacity, and define whether registration changes during dispatch are
  rejected or deferred. Use the simplest policy needed for static wiring.
- Make actuator dependencies borrowed references. Give retained polymorphic
  interfaces virtual destructors and remove unused abstractions.
- Replace Telegram's undocumented raw ownership with RAII or stable members as
  supported by the current toolchain. Define one-time or idempotent initialization;
  trust anchors must outlive all TLS operations.
- Separate behavioral defaults from credentials; keep a compatibility mapping
  from the existing `env.h`/local `env.cpp` declarations during migration.

**Acceptance and verification**

- Native tests prove dispatch order, subscription policy, and payload handling.
- Pure application/domain modules compile without device libraries. Run
  `pio test -e native` and `pio run -e nodemcuv2`.

### 4. Make hardware initialization and signaling incremental

**Changes**

- Move GPIO side effects from constructors to explicit initialization in `setup()`.
  Set safe inactive outputs and initialize the button according to verified wiring.
- Replace LED and buzzer delays with timed states advanced by `update()`.
  Cancellation always leaves the buzzer inactive.
- Define how overlapping Wi-Fi, startup, and delivery indications behave: choose
  explicit priority/preemption or a bounded queue and document dropped/coalesced
  indications. Preserve useful signal meanings and record any intentional changes.
- Use `uint32_t` monotonic ticks and unsigned elapsed-time subtraction. Keep pulse
  durations and cancellation/debounce settings as named, documented constants.
- Keep event handlers short; they request a pattern rather than execute it.

**Acceptance and verification**

- Test startup signal completion, press-before-start, cancellation while active,
  pulse boundaries, overlapping events, final output states, and counter overflow.
- Fake-clock tests require no real waiting. Build firmware and run host tests.
- Hardware checks later confirm button polarity, safe startup, and timing; a build
  alone does not validate electrical behavior.

### 5. Coordinate Wi-Fi, time readiness, and OTA without retry loops

**Changes**

- Split Wi-Fi attempt/status access from retry policy. Model disconnected,
  connecting, connected, and retry-wait states with finite attempt windows and
  bounded retry intervals. Emit events on transitions or documented intervals.
- Validate configured IP, gateway, and subnet and check adapter results before
  starting a connection. Choose and document an invalid-configuration policy;
  do not silently replace static addressing with DHCP.
- Start time synchronization without a polling loop. Expose current wall-clock
  validity to TLS readiness; retry time acquisition while other work continues.
  Use monotonic time for retries and document loss/correction of calendar time.
- Start OTA when networking is ready and service it on every eligible loop pass.
  Define lifecycle behavior on disconnect/reconnect using the pinned adapter.
- Have `NotifierApplication` coordinate startup/update. Initialize safe hardware
  before startup events and allow indications, connection progress, and time
  readiness to advance independently.

**Acceptance and verification**

- Test unavailable Wi-Fi, attempt expiry, retry boundaries, connection loss,
  successful recovery, invalid configuration, unavailable time, time recovery,
  wall-clock corrections, and monotonic overflow.
- With fake adapters, verify signaling and other loop work continue while Wi-Fi
  or time is unavailable. Build firmware and run host tests.

### 6. Separate Telegram transport from delivery policy

**Changes**

- Keep TLS and Telegram API calls in an infrastructure adapter. Application logic
  owns the pending startup notification and per-recipient delivery/retry state.
- Retain the startup notification in RAM while Wi-Fi or valid TLS time is absent.
  This intentionally replaces today's permanent skip after failed initial NTP.
- Validate nonempty message and recipient inputs and define empty-recipient-list
  behavior. Bound supported recipient count, message storage, and pending work.
- Preserve five attempts per recipient initially; schedule retries without
  `delay()`. Do not consume send attempts while prerequisites are unavailable.
  Specify what happens to retry state on connection loss and exhaustion.
- Do not resend to recipients already acknowledged within the current boot.
  Distinguish waiting, success, retryable failure, and exhausted delivery outcomes.
- Bound connect/TLS/read operations using the APIs actually available in pinned
  dependencies. Measure the worst-case blocking duration of one attempt. If it is
  incompatible with the agreed responsiveness budget, address transport design
  explicitly rather than claiming a state machine makes synchronous I/O nonblocking.
- Remove recipient identifiers from ordinary error logs; use recipient indices
  or redacted diagnostic context. Never log tokens or private configuration.
- Document delivery uncertainty: a lost response may cause a duplicate retry;
  restart creates a new startup event; there is no durable queue or exactly-once
  guarantee. Keep OTA exposure and authentication assumptions documented.

**Acceptance and verification**

- Test success, partial delivery, failure/exhaustion, retry timing, Wi-Fi loss,
  late NTP readiness, no resend after acknowledgment, empty/invalid inputs,
  overflow, and restart resetting in-memory state.
- Verify transport result mapping and timeout configuration against the pinned
  implementation. Run host tests and the firmware build.
- Measure TLS heap use and single-attempt latency on hardware during the final
  authorized validation; document remaining OTA/button responsiveness limits.

### 7. Add CI and complete documentation

**Changes**

- Add pull-request and relevant-push CI jobs for formatting, native tests, and
  the `nodemcuv2` build using the same documented local commands.
- Install declared tool versions and dependencies, use minimum workflow
  permissions, and cancel superseded runs. Generate placeholder configuration
  only in clean CI workspaces; never overwrite a developer's local configuration.
- Keep firmware upload and live Telegram calls outside ordinary checks.
- Update README and AGENTS alongside each behavior change, then reconcile their
  final configuration, serial/event contracts, signal patterns, retry semantics,
  setup, OTA/USB deployment, and limitations. No filesystem deployment is currently
  needed because the project has no deployed static assets.

**Acceptance and verification**

- Exercise documented build/test/format commands locally and verify the CI workflow
  on a PR when available. Report CI as unverified until a run has completed.
- Confirm a placeholder-only clean checkout passes ordinary verification.

## Change sequencing and verification gates

Implement stages as focused reviewable changes in order. Stages 4 and 5 depend on
the neutral contracts/tests from stage 3; stage 6 depends on stage 5's readiness
contracts. Stage 7's initial CI can be introduced once native tests exist and
extended as subsequent changes land. Keep firmware compiling at each boundary;
move declarations, callers, tests, and documentation together.

For every applicable change, run from the repository root:

```sh
pio run -e nodemcuv2
pio test -e native
```

The native command becomes valid only after stage 3. Document the actual local
PlatformIO executable/setup in stage 1; do not assume a global installation.
Add the exact formatter command once its version and file scope are selected.
Documentation-only work requires path, command, and implementation consistency
review rather than new behavior tests.

After automated checks, an explicitly authorized hardware session should confirm
safe boot, button cancellation, Wi-Fi outage/recovery, NTP outage/recovery,
multi-recipient delivery, TLS memory/latency, and OTA availability during retries.
Agree and record measurable loop-service and button-response budgets before these
checks; separate normal-loop timing from bounded synchronous transport stalls.
Use designated test recipients only with explicit authorization. Firmware upload,
production deployment, and external notifications are separate authorized actions.

## Completion criteria

- Applicable standard requirements are met or have specific justified exceptions
  in `AGENTS.md`, including replacement requirements.
- `setup()` and `loop()` coordinate dependencies and lifecycle; application policy
  is board-independent and covered by deterministic host tests.
- No unbounded Wi-Fi waits or delay-driven indication/retry loops remain. Required
  synchronous operations have documented finite timeouts and responsiveness limits.
- Local secrets remain ignored and untouched; builds and CI use safe examples.
- Dependency revisions, language version, formatter, and verification commands
  are declared and reproducible.
- README and AGENTS describe the implemented result, hardware assumptions,
  delivery guarantees, restart behavior, deployment, and known limitations.
- Completion reports distinguish executed checks from proposed checks and record
  any remaining hardware validation or deployment work.
