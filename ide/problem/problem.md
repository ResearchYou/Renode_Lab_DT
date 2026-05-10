# Challenge: Mock HID Security Token Replay Bug

## Scenario

An STM32F4 Discovery board emulates a small YubiKey-like security token. Renode
provides a mock USB HID host peripheral, so the firmware sees fixed 64-byte
reports without requiring real USB enumeration.

```
[Mock HID Host]  ->  64-byte request reports  ->  [STM32F4 token]
[Mock HID Host]  <-  64-byte response reports <-  [STM32F4 token]
```

The token supports:

- `GET_INFO`: returns protocol/capability information.
- `AUTH`: requires touch presence, increments a monotonic counter, and returns a
  MAC over `challenge || nonce || counter` using a resident secret.

## Your Task

The firmware contains a replay-protection bug. A captured authentication request
with the same nonce and challenge is accepted when it is replayed with a new HID
transport sequence number.

Fix the replay check so repeated `(nonce, challenge)` pairs return `ERR_REPLAY`.
After the fix, the security validation should pass.

## Important Files

- `firmware/main.c` — token application logic and the intentional replay bug.
- `firmware/token_protocol.h` — 64-byte report format, commands, statuses, CRC.
- `renode/peripherals/MockUSBHIDHost.cs` — scripted mock HID host and checks.

## Running Tests

```bash
docker compose up digital-twin
```

Expected initial state: functional checks pass, but replay rejection fails. The
failure is intentional for this bug-fix challenge.
