# Mood Lamp - Firmware CI/CD

Automated checks are centered on the ESP32 firmware: compile the real target, run host-side unit tests
for portable logic, and run cppcheck before code reaches `main`.

## Overview

| Stage | Where | Tool | Gate? |
|---|---|---|---|
| Firmware compile | GitHub Actions | PlatformIO `pio run -e seeed_xiao_esp32c3` | Yes |
| Native unit tests | GitHub Actions | PlatformIO `native` env + Unity | Yes |
| Static analysis | GitHub Actions + local pre-push | cppcheck as C++17 | Yes |
| API docs | Local only | Doxygen | No |

This project is PlatformIO + Arduino on a Seeed XIAO ESP32-C3. It does not use ESP-IDF CI, Docker,
or QEMU boot tests. The useful CI signal today is:

- compile the actual firmware against the Arduino/PlatformIO toolchain;
- unit-test hardware-independent logic on the host;
- run cppcheck with the same settings locally and in CI.

## GitHub Actions

`.github/workflows/ci.yml` runs on pushes and pull requests to `main`.

### `build`

The build job:

1. checks out the repo;
2. installs Python 3.11;
3. caches PlatformIO and pip downloads;
4. installs PlatformIO and PyYAML;
5. regenerates `Firmware/include/moods.h` from `Utils/moods.yaml`;
6. copies `Firmware/include/secrets.example.h` to `Firmware/include/secrets.h`;
7. runs `pio run -e seeed_xiao_esp32c3` from `Firmware/`;
8. uploads the generated `.bin` files as a workflow artifact.

The artifact is a compile artifact, not a ready-to-deploy private firmware image. It is built with the
placeholder values from `secrets.example.h`, so server communication will not work until a local build
uses a real `Firmware/include/secrets.h`.

### `native-test`

The native test job:

1. installs PlatformIO and PyYAML;
2. regenerates `moods.h`;
3. creates the placeholder `secrets.h`;
4. runs `pio test -e native`.

The `native` environment in `Firmware/platformio.ini` intentionally compiles only portable source:

```ini
build_src_filter = -<*> +<hal/led_effects.cpp>
```

This keeps host tests away from Arduino-only hardware code such as `digitalRead`, Wi-Fi, NimBLE, and
`Adafruit_NeoPixel`. The current native test surface is the mood animation logic in `mood_frame()`.

### `cppcheck`

The static-analysis job:

1. installs PyYAML and cppcheck;
2. regenerates `moods.h`;
3. creates the placeholder `secrets.h`;
4. runs cppcheck inside `Firmware/`.

Current command:

```bash
cppcheck --enable=warning,style,performance,portability \
  --std=c++17 --language=c++ --error-exitcode=1 --inline-suppr \
  -I include \
  --suppressions-list=.cppcheck-suppressions \
  src
```

`--error-exitcode=1` makes any reported finding fail the job. `--inline-suppr` allows deliberate
source-level suppressions, and `.cppcheck-suppressions` covers expected false positives such as
missing Arduino/library include paths.

## Secrets in CI

`Firmware/include/secrets.h` is intentionally ignored by git because it contains:

```cpp
#define SERVER_URL "https://..."
#define DEVICE_TOKEN "..."
#define ROOT_CA "..."
```

Ignoring that file is correct, but the firmware includes `secrets.h`, so CI still needs a file with
that name. The repository solves this with `Firmware/include/secrets.example.h`, which contains dummy
values. Each CI job copies the example into place before building, testing, or running cppcheck:

```bash
cp Firmware/include/secrets.example.h Firmware/include/secrets.h
```

Rules:

- commit `secrets.example.h`;
- never commit `secrets.h`;
- do not flash CI artifacts to real devices unless you intentionally provide real secrets during that
  build;
- do not put raw device tokens into workflow logs.

## Local Setup

Enable the pre-push hook once per clone:

```bash
chmod +x .githooks/pre-push
git config core.hooksPath .githooks
```

Install local tools:

```bash
brew install cppcheck doxygen
python3 -m venv .venv
source .venv/bin/activate
pip install platformio pyyaml
```

Generate the mood table after changing `Utils/moods.yaml`:

```bash
python Utils/generate_moods.py
```

Create `Firmware/include/secrets.h` before local firmware builds. For compile-only work, the example
is enough:

```bash
cp Firmware/include/secrets.example.h Firmware/include/secrets.h
```

For flashing a real lamp, edit `Firmware/include/secrets.h` with that device's real server URL, token,
and trusted root CA.

## Running Locally

From the repo root:

```bash
python Utils/generate_moods.py
```

From `Firmware/`:

```bash
pio run -e seeed_xiao_esp32c3
pio test -e native
doxygen Doxyfile
cppcheck --enable=warning,style,performance,portability \
  --std=c++17 --language=c++ --error-exitcode=1 --inline-suppr \
  -I include \
  --suppressions-list=.cppcheck-suppressions \
  src
```

The pre-push hook currently runs cppcheck only. It does not run `pio run` or `pio test`, so run those
manually before pushing larger firmware changes.

## Generated Files and Ignored Files

Expected generated or local-only files:

| Path/pattern | Reason |
|---|---|
| `Firmware/include/secrets.h` | Real server URL, token, and trusted root CA. |
| `.venv/`, `Server/.venv/` | Python virtual environments. |
| `__pycache__/`, `*.pyc` | Python bytecode cache. |
| `*.db`, `*.sqlite`, `*.sqlite3` | Local server databases with token hashes and device state. |
| `*.pem`, `*.key` | Certificate/private-key material. |
| `Firmware/.pio/` | PlatformIO build/cache output. |
| `Docs/doxygen/` | Generated Doxygen documentation. |
| `*.bin`, `*.elf`, `*.map` | Firmware build artifacts. |

## Open Items

1. Add native tests for button gesture handling once the state-machine logic is isolated from the
   Arduino GPIO calls.
2. Add server tests for auth failure, pair isolation, mood validation, and `ETag`/`304` polling.
3. Consider a CI drift check that regenerates `moods.h` and fails if `git diff --exit-code` is not
   clean.
4. Decide whether release firmware should ever be built in CI with encrypted GitHub secrets, or only
   built locally on trusted machines.
