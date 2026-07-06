# Mood Lamp — Firmware CI/CD

Automated build, test, static analysis, and emulation for the Mood Lamp firmware,
driven by GitHub Actions, plus locally-run Doxygen docs and a pre-push hook.

> **Why this replaces the old OtO report.** The prior write-up targeted **ESP-IDF**
> on **ESP32/ESP32-S3 (xtensa)** and leaned on Docker + `idf.py` + `qemu-system-xtensa`.
> This project is **PlatformIO + Arduino** on the **XIAO ESP32-C3 (RISC-V)**. That
> changes almost everything: no Docker/IDF layer (PlatformIO CI is just `pip install
> platformio && pio run`), QEMU is `qemu-system-riscv32 -machine esp32c3` (not xtensa),
> cppcheck must run as C++ (not `--std=c99`), and unit tests run natively on the host
> instead of on-target. The sections below are the corrected, project-specific setup.

---

## Overview

| Stage | Where | Tool | Gate? |
|---|---|---|---|
| Compile firmware | CI + pre-push (build only in CI) | PlatformIO (`pio run`) | Yes |
| Unit tests (logic) | CI | PlatformIO `native` env + Unity, host-compiled | Yes |
| Static analysis | CI + **pre-push** | cppcheck (C++17) | Yes |
| Boot smoke test | CI | QEMU `qemu-system-riscv32 -machine esp32c3` | No (experimental) |
| API docs | local (`doxygen Doxyfile`) | Doxygen | n/a |

Two design decisions carry the whole thing:

1. **Test logic on the host, not the chip.** `mood_frame`, the mood table, and (later)
   the button FSM are pure functions of their inputs. Compiling them with the CI runner's
   own gcc (`pio test -e native`) runs them in seconds with real coverage — no emulator.
   QEMU is reserved for a *boot* smoke test, which is the one thing native tests can't do.
2. **Keep hardware-independent code hardware-independent.** `moods.h` includes `<stdint.h>`
   (not `<Arduino.h>`) and `led_effects.cpp` uses `M_PI` (not Arduino's `PI`), so both
   compile on the host. Anything touching `digitalRead`/NeoPixel stays out of the native build.

---

## GitHub Actions

### `.github/workflows/ci.yml` — on push & PR to `main`

Four jobs, run in parallel except QEMU (which needs the build):

- **build** — installs PlatformIO, regenerates `moods.h` from `moods.yaml`, runs
  `pio run -e seeed_xiao_esp32c3`, uploads the `.bin`s as an artifact.
- **native-test** — `pio test -e native`. Compiles only the portable sources
  (`build_src_filter` in `platformio.ini`) and runs the Unity suite in `Firmware/test/`.
- **cppcheck** — installs cppcheck, runs it as C++17 over `Firmware/src` with
  `--error-exitcode=1`, so any finding fails the job.
- **qemu-smoke** *(experimental, non-blocking)* — merges the build into a 4 MB flash
  image with `esptool merge_bin`, downloads Espressif's RISC-V QEMU, boots the image
  for 30 s, and greps the UART0 serial log for the firmware's boot banner.

Each job regenerates `moods.h` first, so a stale committed header can never hide a
`moods.yaml` change.

Documentation is **not** in CI — it's generated on demand locally (see below).

---

## The QEMU boot smoke test (ESP32-C3 / RISC-V)

Because the C3 is RISC-V, the emulator is `qemu-system-riscv32 -machine esp32c3` from
**Espressif's QEMU fork** — the mainline QEMU esp32 machine (xtensa) in the old report
does not apply. The flow:

1. `pio run` produces `bootloader.bin`, `partitions.bin`, `firmware.bin`.
2. `esptool.py --chip esp32c3 merge_bin` assembles them (plus `boot_app0.bin`) into one
   `flash_image.bin` at the C3 offsets (bootloader at **0x0**, not 0x1000 like the original ESP32).
3. QEMU boots the image; the firmware prints `[boot] mood-lamp firmware up` to **UART0**
   (via `Serial0`, which QEMU emulates — the XIAO's USB-CDC `Serial` is separate).
4. The job greps the serial log for that banner. Present → the app reached `setup()`.

**Status: experimental / non-blocking.** QEMU's esp32c3 machine plus the Arduino core is
not guaranteed to boot cleanly, and Espressif's QEMU release asset names drift over time
(the workflow resolves the latest RISC-V build via the GitHub API to reduce breakage). The
job is `continue-on-error: true` so it can't block merges while being proven. Once it's
reliably green in Actions, remove `continue-on-error` to make it a hard gate.

**Test it locally** (no push needed): `./Utils/qemu/run.sh` builds the firmware and boots it
in the same Espressif QEMU inside Docker. Note `brew install qemu` won't work — upstream QEMU
lacks the `esp32c3` machine; only Espressif's fork has it. QEMU is also dynamically linked
against `libsdl2-2.0-0`, `libglib2.0-0`, `libpixman-1-0`, `libslirp0`, `libnuma1` (even under
`-nographic`), so those must be installed in CI and the local Docker image.

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

- [ ] Prove the QEMU job on real CI, then make it a hard gate (drop `continue-on-error`).
- [ ] Add native tests for the button FSM once `show_mood_button_handle` /
      `select_mood_button_handle` are split from the `digitalRead` HAL.
- [ ] Add gcov/lcov coverage reporting to the native-test job.
- [ ] Consider a `moods.h` drift check (regenerate + `git diff --exit-code`) in CI.
