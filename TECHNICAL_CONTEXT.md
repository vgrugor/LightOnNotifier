# Shared Technical Context

Standard version: 1.0.0

## Purpose and scope

This file defines the shared engineering requirements for related embedded and IoT projects. It is intended for developers, reviewers, and coding agents. Projects adopting it should follow the same code style, architecture principles, testing expectations, and documentation structure.

The standard covers firmware and, where present, local web interfaces, network protocols, and persistent storage. Apply a requirement only when the corresponding capability exists. Do not introduce a web application, abstraction layer, dependency, or service solely to satisfy an optional section.

Use these requirement levels:

- **MUST**: required for applicable work.
- **SHOULD**: expected by default; deviations need a documented reason.
- **MAY**: optional, based on project needs.

## Relationship with AGENTS.md

`TECHNICAL_CONTEXT.md` contains reusable requirements. Each project's root `AGENTS.md` MUST describe its own purpose, repository map, build and test commands, architecture details, runtime contracts, hardware constraints, and known limitations. Nested `AGENTS.md` files MAY provide directory-specific instructions.

The root `AGENTS.md` MUST link to this file, identify the adopted standard version, and document any project-specific exceptions. Exceptions MUST identify the affected rule, explain the reason, and state the replacement requirement. Directory-specific instructions MUST remain consistent with the project's declared exceptions.

Explicit task instructions and applicable execution-environment rules take precedence. When project documentation conflicts with this standard and no exception is documented, resolve the conflict before making a dependent change. Do not silently weaken a requirement.

Projects SHOULD keep the same filename and Markdown structure. Maintain a canonical version in a shared source and distribute updates through reviewable changes. Do not allow copied versions to drift through undocumented local edits. Repositories SHOULD contain a readable local copy so the standard is available offline.

Adoption does not require an unrelated rewrite of existing code. New and changed code MUST follow the standard; existing deviations SHOULD be documented and corrected in focused changes.

## Architecture and repository organization

- Separate domain state and rules, application behavior, infrastructure adapters, and presentation concerns where the project's complexity warrants it.
- Domain and application logic MUST remain independent of board-specific networking, GPIO, filesystem, display, and transport libraries.
- Infrastructure adapters MUST implement hardware and external-service access. Application logic SHOULD own retry policies, scheduling, and state transitions.
- Presentation code MUST validate and translate input before invoking application behavior. It MUST NOT duplicate business rules.
- The composition root, such as `main.cpp`, MUST focus on dependency wiring and lifecycle coordination. Keep business logic out of `setup()` and `loop()`.
- Inject dependencies such as clocks, network connections, senders, storage, and views through explicit interfaces or parameters when this enables meaningful host tests. Do not create interfaces with no practical boundary or testing purpose.
- In PlatformIO C++ projects, place public declarations under `include/` and implementations under matching paths in `src/`. Keep static web assets under `data/` when deployed through an embedded filesystem.
- Use repository-relative project includes, such as `"application/SensorUpdateService.h"`. Do not use machine-specific absolute paths or chains of parent-directory traversal.
- Avoid dependency cycles and shared mutable globals. Objects with static lifetime MAY be wired in the composition root when required by embedded callbacks.

## Code style

### Common conventions

- Use UTF-8, LF line endings, a final newline, and no trailing whitespace.
- Use four spaces for indentation in C++, JavaScript, HTML, and CSS. Use two spaces in JSON and YAML. Use tabs only when required by the file format.
- Keep lines within 100 characters where practical. Split long signatures and expressions consistently; do not break URLs or generated content solely for line length.
- Use English for identifiers, comments, technical documentation, commit messages, and PR descriptions. User-facing language is a project requirement and MAY differ.
- Prefer explicit names and short functions with one clear responsibility. Avoid unexplained abbreviations and magic numbers.
- Comments SHOULD explain intent, constraints, or a non-obvious decision. Do not restate the code. Document public contracts and ownership rules where they are not obvious.
- Keep formatting-only changes separate from behavior changes unless formatting is necessary in the touched code.

### C++ conventions

- Use `PascalCase` for types and class-based filenames, `lowerCamelCase` for functions, variables, parameters, and members, and `UPPER_SNAKE_CASE` for named constants and enum values.
- Use opening braces on the same line as declarations and control statements. Use braces for all control-flow bodies, including single statements.
- Use `enum class` for new enumerations, `nullptr` for null pointers, and `override` for overridden virtual methods.
- Use `const` for values and member functions that do not modify state. Prefer `constexpr` for compile-time constants when supported by the configured toolchain.
- Prefer references for required non-owning dependencies. Use pointers when absence or another pointer-specific behavior is part of the contract.
- Use fixed-width integers when width matters to timing, storage, hardware, or protocols. Use types appropriate to container sizes and check narrowing conversions.
- Make ownership and lifetime explicit. Do not transfer ownership through undocumented raw pointers. Polymorphic interfaces used for deletion MUST have virtual destructors.
- Group includes as the implementation's own header, standard headers, external-library headers, and project headers, with blank lines between groups.
- New headers SHOULD use unique include guards derived from their repository path.
- Use the C++ language version declared by the project. Do not introduce unsupported language features or change compiler options incidentally.

### JavaScript and static web conventions

- Use `lowerCamelCase` for functions and variables, `PascalCase` for classes, and `UPPER_SNAKE_CASE` for named constants with fixed semantic values.
- Prefer `const`; use `let` when reassignment is required. Do not introduce `var` in new code.
- Use single-quoted strings, semicolons, strict equality, and explicit error handling.
- Keep DOM updates and transport handling understandable and independently testable where practical.
- Preserve mobile usability, connection-state feedback, and reconnect behavior.
- Embedded web assets SHOULD remain small and self-contained. Additional build tooling or CDN dependencies need a documented benefit and deployment plan.

### Formatting tools

Projects SHOULD provide an `.editorconfig` and a formatter configuration appropriate to their languages, such as `.clang-format` for C++. Formatting MUST be deterministic and use the shared conventions above. Adopt formatter settings in a dedicated change before applying them broadly to existing files.

## Embedded runtime and resource constraints

- Main-loop work and callbacks MUST avoid unbounded blocking. Use elapsed-time checks and state machines for recurring work and retries.
- Required synchronous operations MUST have bounded timeouts. Document material effects on responsiveness, watchdog servicing, and other work.
- Use wrap-safe unsigned elapsed-time calculations for monotonic counters. Do not compare future absolute deadlines in a way that fails when the counter wraps.
- Separate monotonic time used for intervals from calendar time used for dates and schedules. Define behavior before time synchronization and after wall-clock corrections.
- Be conservative with heap allocation, repeated dynamic-string construction, and large buffers. Document memory assumptions for significant allocations.
- Keep callback references valid for the callback's lifetime. Document borrowed payloads and whether a receiver must copy them.
- Do not perform blocking network or storage operations in interrupt handlers. Defer work to an appropriate execution context.
- Changes to pin assignments, active-high/active-low behavior, board configuration, or startup sequencing MUST document the hardware impact.

## Configuration, secrets, and dependencies

- Separate tracked behavior defaults from local credentials and deployment-specific settings.
- Secret-bearing local files MUST remain untracked and covered by ignore rules. Provide tracked examples containing placeholders only.
- Never print, commit, overwrite, or copy real secrets into tests, logs, generated documentation, or CI configuration.
- Document the required configuration values, their purpose, and the setup procedure. Preserve an existing local configuration when performing unrelated work.
- Declare build environments, toolchain requirements, and dependencies in project configuration. Avoid assumptions about undeclared global packages.
- Use lock files where the dependency manager supports them. Platform packages and dependencies without lock files SHOULD use exact versions or immutable revisions when practical.
- Dependency updates MUST include relevant build and test verification. Do not switch libraries, frameworks, or build systems as incidental cleanup.

## Data, protocol, and persistence contracts

- Validate external input before reading beyond buffer boundaries or changing state. Handle missing, malformed, out-of-range, and non-finite numeric values explicitly.
- Document public routes or topics, request formats, response formats, errors, field types, units, precision, and missing-value semantics.
- Distinguish an actual zero reading from an unavailable reading when introducing or changing a data contract. Define stale-data behavior separately.
- Multi-value operations SHOULD validate all inputs before modifying state. Partial updates MUST be an explicit, documented behavior.
- Treat names, casing, units, and serialized types as compatibility contracts. Update producers, consumers, tests, and documentation together when changing them.
- Avoid duplicate notifications or external actions. Specify event ordering and delivery semantics where consumers depend on them.
- Define behavior after power loss, restart, connection loss, and invalid storage. Persisted formats MUST have validation and a migration or reset policy when their layout changes.
- Document retry and duplicate-delivery behavior for external side effects. Claim exactly-once behavior only when the implementation supports it across relevant failure scenarios.
- Logs, status endpoints, and error responses MUST NOT expose credentials or unnecessary private configuration. Document the intended network exposure and access-control assumptions.

## Testing requirements

### Test design

- New device-independent behavior MUST have focused host-side tests. Bug fixes SHOULD include a regression test that fails for the original faulty behavior.
- Test observable behavior and contracts rather than private implementation details. Pure formatting or documentation changes do not need new behavior tests.
- Cover relevant success paths, validation failures, boundary values, state transitions, and recovery behavior.
- For timing logic, cover interval boundaries and counter overflow. For daily schedules, cover unavailable time, date changes, and restart behavior where applicable.
- Use deterministic clocks, fake adapters, and controlled inputs. Ordinary unit tests MUST NOT depend on a physical board, live Wi-Fi, external services, or real waiting.
- Test suites MUST be independent and repeatable. A test MUST NOT rely on another test's order or retained state.
- Name tests after the expected behavior, such as `test_connection_loss_retries_immediately`. Structure them as setup, action, and assertions.
- For web interfaces, verify startup, incoming data, malformed payloads, disconnect/reconnect, and visible error states when affected. Mock controller traffic locally.
- Do not add arbitrary coverage targets or tests that merely duplicate the implementation. Choose tests based on behavior and failure risk.

### Verification by change type

| Change | Required verification |
| --- | --- |
| Firmware source, headers, board settings, or firmware dependencies | Compile the affected firmware environment. |
| Device-independent state, validation, timing, or application behavior | Run host-side tests and add or update focused behavior tests. |
| Web scripts, HTML, or browser-visible contracts | Run browser tests; add or update checks for changed behavior. |
| Styles or visual assets | Inspect the affected layout at relevant viewport sizes; run browser checks when behavior is affected. |
| Shared protocol or stored-data format | Verify affected producers and consumers, compatibility behavior, and persistence handling as applicable. |
| Build scripts or CI configuration | Exercise the affected commands where available and review environment assumptions. |
| Documentation only | Verify paths, commands, examples, links, and consistency with the implementation. |

Projects MUST provide their actual commands in `AGENTS.md` and README. For PlatformIO projects, the baseline commands are typically `pio run` and `pio test -e native`; browser projects SHOULD expose a documented package script.

If verification cannot run, report the missing tool or environmental limitation and identify what remains unverified. Do not claim a passing check without executing it. A successful firmware build does not establish that the physical device works.

## CI requirements

- CI MUST run the applicable firmware build, host-side tests, and browser tests for pull requests and relevant pushes.
- Local and CI checks MUST exercise the same project commands. Document any environment-specific setup.
- Use placeholder configuration to compile when local secrets are required. CI MUST NOT need production credentials for ordinary verification.
- CI SHOULD install dependencies from declared versions and lock files, use minimum necessary permissions, and cancel superseded runs where appropriate.
- Hardware upload and production deployment MUST remain separate from ordinary build and test jobs unless explicitly configured and authorized.

## README standard

Each README MUST describe the implemented project rather than planned features. Use the following section order. Omit capability-specific sections only when they do not apply; keep the remaining headings consistent.

```markdown
# ProjectName

Short description of the device, its purpose, and its main users.

## Features
Implemented capabilities.

## Hardware and software
Supported boards, peripherals, toolchain requirements, and required versions.

## Repository structure
Key directories and a short explanation of architecture and dependency boundaries.

## Initial setup
Steps from a clean checkout to a configured development environment.

## Configuration and secrets
Required settings, local configuration files, safe examples, and ignored secrets.

## Build and test
Exact commands for firmware builds and each applicable test suite.

## Interfaces
HTTP, WebSocket, MQTT, serial, or other contracts; units, examples, and errors.

## Hardware connections
Pin mapping, electrical assumptions, and relevant peripheral configuration.

## Deployment
Firmware and asset upload procedures, required connection details, and recovery.

## Diagnostics and troubleshooting
Logs, status endpoints, common failures, and actionable checks.

## Known limitations
Current limitations, persistence behavior, and verification boundaries.

## Engineering guidance
Links to TECHNICAL_CONTEXT.md and the project's AGENTS.md.
```

- Commands MUST state the working directory or assume the documented repository root. Examples MUST use placeholders for private deployment values.
- Setup steps MUST account for files intentionally missing from a clean checkout.
- Interface examples MUST match actual field names, types, units, and formatting. Separate interface subsections MAY be used for different protocols.
- Deployment instructions MUST distinguish firmware from filesystem or other separately deployed assets.
- Update README in the same change when setup, commands, interfaces, hardware assumptions, or deployment behavior changes.
- Keep detailed project-specific runtime contracts in `AGENTS.md` or linked documentation. Do not duplicate the entire shared standard in README.

## Change, review, and deployment workflow

- Keep each change focused on its stated outcome. Avoid unrelated refactoring, reformatting, dependency updates, or hardware changes.
- Preserve existing user changes and local configuration. Do not discard work as part of cleanup.
- Use concise English commit subjects in the imperative form. Projects MAY adopt Conventional Commits if documented consistently.
- A PR description MUST explain the problem, resulting behavior, relevant verification, and material limitations. Document migration, compatibility, and hardware impacts when applicable.
- Update affected code, tests, and documentation together. Record public breaking changes in release notes or a changelog when the project maintains one.
- Run required checks before reporting completion. Hardware access, uploads, production deployment, and external notifications require explicit authorization for the task.
- Report deployment requirements even when no upload is requested. Do not treat a firmware upload as deployment of separately stored web assets.

## Definition of done

An applicable change is complete when:

- It fulfills the requested behavior and respects architecture boundaries.
- New and changed code follows the shared style or a documented exception.
- Meaningful tests cover introduced behavior and relevant failure cases.
- Required checks have passed, or their execution limitations are explicitly reported.
- Interfaces, configuration examples, README, and project instructions remain consistent.
- Local secrets and unrelated user work are preserved.
- Compatibility, hardware, persistence, and deployment impacts are documented where relevant.
- The completion report states what changed, what was verified, and any remaining action required to deploy or validate on hardware.

## Maintaining this standard

Change the standard version when shared requirements change. Review updates centrally and describe substantive differences in the distribution change or PR.

- Increment the major version for incompatible requirements.
- Increment the minor version for new compatible requirements or sections.
- Increment the patch version for clarifications and corrections that do not change requirements.

When adopting an update, review its applicability, update the version recorded in `AGENTS.md`, and document any necessary exceptions. Keep project-specific facts out of the shared file.
