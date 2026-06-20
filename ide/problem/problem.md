# Laboratory: Renode STM32F4 HMAC-SHA1 Validation

## Scenario

An STM32F4 Discovery board runs firmware for a small security-token protocol.
Renode provides a mock USB HID host peripheral, so the firmware sees fixed
64-byte reports without requiring real USB enumeration.

```text
[Mock HID Host]  ->  64-byte request reports  ->  [STM32F4 token]
[Mock HID Host]  <-  64-byte response reports <-  [STM32F4 token]
```

The validation scenario checks:

- `GET_INFO`: protocol/capability response.
- `HMAC_SHA1`: three RFC 2202 HMAC-SHA1 fixed vectors.
- Response framing, CRC, status, and digest values.

## Important Files

- `firmware/main.c` - token firmware, SHA-1/HMAC-SHA1 code, and request handling.
- `firmware/token_protocol.h` - 64-byte report format, commands, statuses, CRC.
- `firmware/stm32f4.h` - STM32F4 and mock-host memory-mapped registers.
- `renode/run_test.resc` - Renode scenario script.
- `renode/peripherals/MockUSBHIDHost.cs` - scripted mock HID host and checks.

## Exercises

Use the OCW handout for the full theory and exercise text. In this workspace,
focus on:

1. Trace `GET_INFO` from `MockUSBHIDHost.cs` to `firmware/main.c`.
2. Inspect the request/response format in `token_protocol.h`.
3. Break one HMAC vector, run the validation, read the report, then restore it.
4. Add a negative mock-host test for an invalid HMAC vector index.

## Running Tests

Use the Run button in the IDE panel, or locally:

```bash
docker compose up digital-twin
```

Expected successful ending:

```text
>>> HMAC-SHA1 FUNCTIONAL VALIDATION PASSED <<<
```
