# Mood Lamp — Firmware CI/CD

Automated build, test, and static analysis for the Mood Lamp firmware, driven by
GitHub Actions, plus locally-run Doxygen docs and a pre-push hook.

> **Why this replaces the old OtO report.** The prior write-up targeted **ESP-IDF**
> on **ESP32/ESP32-S3 (xtensa)** and leaned on Docker + `idf.py` + `qemu-system-xtensa`.
> This project is **PlatformIO + Arduino** on the **XIAO ESP32-C3 (RISC-V)**. That
> changes almost everything: no Docker/IDF layer (PlatformIO CI is just `pip install
> platformio && pio run`), cppcheck must run as C++ (not `--std=c99`), and unit tests run
> natively on the host instead of on-target. The sections below are the corrected,
> project-specific setup.

---

## Overview

| Stage | Where | Tool | Gate? |
|---|---|---|---|
| Compile firmware | CI + pre-push (build only in CI) | PlatformIO (`pio run`) | Yes |
| Unit tests (logic) | CI | PlatformIO `native` env + Unity, host-compiled | Yes |
| Static analysis | CI + **pre-push** | cppcheck (C++17) | Yes |
| API docs | local (`doxygen Doxyfile`) | Doxygen | n/a |

> On-target emulation (QEMU `esp32c3`) was evaluated and dropped: the Arduino core panics
> at flash init under QEMU (`assert failed: do_core_init … flash_ret == ESP_OK`), a known
> emulator limitation, not a firmware bug. Not worth chasing for a boot smoke test — the
> native tests cover the logic, and real hardware covers the boot.

Two design decisions carry the whole thing:

1. **Test logic on the host, not the chip.** `mood_frame`, the mood table, and (later)
   the button FSM are pure functions of their inputs. Compiling them with the CI runner's
   own gcc (`pio test -e native`) runs them in seconds with real coverage — no emulator.
2. **Keep hardware-independent code hardware-independent.** `moods.h` includes `<stdint.h>`
   (not `<Arduino.h>`) and `led_effects.cpp` uses `M_PI` (not Arduino's `PI`), so both
   compile on the host. Anything touching `digitalRead`/NeoPixel stays out of the native build.

---

## GitHub Actions

### `.github/workflows/ci.yml` — on push & PR to `main`

Three jobs, run in parallel:

- **build** — installs PlatformIO, regenerates `moods.h` from `moods.yaml`, runs
  `pio run -e seeed_xiao_esp32c3`, uploads the `.bin`s as an artifact.
- **native-test** — `pio test -e native`. Compiles only the portable sources
  (`build_src_filter` in `platformio.ini`) and runs the Unity suite in `Firmware/test/`.
- **cppcheck** — installs cppcheck, runs it as C++17 over `Firmware/src` with
  `--error-exitcode=1`, so any finding fails the job.

Each job regenerates `moods.h` first, so a stale committed header can never hide a
`moods.yaml` change.

Documentation is **not** in CI — it's generated on demand locally (see below).

---

## Static analysis (cppcheck)

Runs in CI **and** locally as a pre-push hook, with identical settings so local == CI:

```
cppcheck --enable=warning,style,performance,portability \
  --std=c++17 --language=c++ --error-exitcode=1 --inline-suppr \
  -I include --suppressions-list=.cppcheck-suppressions src
```

- **`--std=c++17 --language=c++`** — this is C++ (Arduino), not the `--std=c99` from the old report.
- **`--error-exitcode=1`** — any finding fails the check.
- **`Firmware/.cppcheck-suppressions`** — suppresses `missingInclude*` (Arduino/library headers
  aren't on cppcheck's path — expected) and `unusedFunction` (cross-TU false positives on HAL
  entry points), plus cppcheck's own nags.

> Difference vs. the compiler: `pio run` catches language errors and (some) `-Wall` warnings
> during codegen; cppcheck does deeper *flow* analysis it can't — null-deref, uninitialised
> reads, out-of-bounds indexing, dead code — **without** compiling, which is why the cppcheck
> job doesn't build the project first (the old report's "build IDF first" step was unnecessary).

---

## Documentation (Doxygen)

Generated **locally on demand** (not in CI): from `Firmware/`, run `doxygen Doxyfile`
and open `Firmware/doxygen/html/index.html`.

- Config: `Firmware/Doxyfile` (`INPUT = include src ../README.md`, `RECURSIVE = YES`,
  `EXTRACT_ALL = YES`, HTML only, `OUTPUT_DIRECTORY = doxygen`).
- Theme: `Firmware/doxygen-custom.css` — a minimalist **black & white** stylesheet loaded via
  `HTML_EXTRA_STYLESHEET`. Doxygen 1.9+ drives colours through CSS variables, so the theme
  overrides those plus flattens the default blue gradient chrome to grayscale.
- Output `Firmware/doxygen/` is git-ignored (generated).

---

## One-time setup

1. **Enable the pre-push hook** (per clone):
   ```bash
   chmod +x .githooks/pre-push
   git config core.hooksPath .githooks
   ```
   Requires `cppcheck` on PATH (`brew install cppcheck`). Bypass once with `git push --no-verify`.
2. **Run the mood generator locally** (needs PyYAML; system Python may need a venv):
   ```bash
   python3 -m venv .venv && source .venv/bin/activate && pip install pyyaml
   python3 Utils/generate_moods.py
   ```

## Running locally

```bash
cd Firmware
pio run -e seeed_xiao_esp32c3     # build
pio test -e native               # host unit tests
doxygen Doxyfile                 # docs -> Firmware/doxygen/html/index.html
cppcheck --enable=warning,style,performance,portability --std=c++17 \
  --language=c++ -I include --suppressions-list=.cppcheck-suppressions src
```

## Open items / next steps

- [ ] Add native tests for the button FSM once `show_mood_button_handle` /
      `select_mood_button_handle` are split from the `digitalRead` HAL.
- [ ] Add gcov/lcov coverage reporting to the native-test job.
- [ ] Consider a `moods.h` drift check (regenerate + `git diff --exit-code`) in CI.
