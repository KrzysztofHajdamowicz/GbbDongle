# Host tests

C++ units of `components/gbb_dongle/` built and run on the development
machine instead of the ESP32: `test_<unit>.cpp` is linked with
`components/gbb_dongle/<unit>.cpp`. The ESP-IDF, FreeRTOS and ESPHome APIs a
unit uses come from the stand-in headers in `stubs/`, and each test defines
what they do. Built with AddressSanitizer and UBSan.

```sh
tests/host/run.sh                  # all tests
tests/host/run.sh cloud_pairing    # one unit
```

ArduinoJson comes from a firmware build, so the tests use the version
ESPHome pins. Compile any config once before the first run:

```sh
uv run esphome -s external_components_source ../components compile firmware/gbbdongle-factory.yaml
```

or set `ARDUINOJSON_INCLUDE` to a directory that holds `ArduinoJson.h`.
`CXX` picks the compiler (default `c++`; GCC and Clang both work, Clang on
Linux needs its sanitizer runtime, e.g. Ubuntu's `libclang-rt-<n>-dev`).
Binaries go to `tests/host/build/` (git-ignored). CI does not run these yet.

| Test | Covers |
|---|---|
| `test_cloud_pairing.cpp` | Pairing code normalization (prefix of 0–2 letters, pasted spaces and dashes), validation of GbbOptimizer's answer, and the exchange's state machine (refusals, timeouts, cancel, queued code, expiry) against a fake HTTP client on a `std::thread`. It never contacts GbbOptimizer: redeeming a real code rotates the plant's token. |
