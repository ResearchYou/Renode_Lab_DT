# HDC1080 Humidity and Temperature Sensor — Renode I2C Python Peripheral
#
# I2C address: 0x40
# Registers:
#   0x00 — Temperature  (trigger write, then read 2 bytes after ~15 ms)
#   0x01 — Humidity     (trigger write, then read 2 bytes after ~15 ms)
#   0x02 — Configuration (16-bit, R/W)
#
# Humidity encoding:   RH%  = (raw / 65536) * 100
# Temperature encoding: T°C = (raw / 65536) * 165 - 40
#
# Renode Python peripheral interface (II2CPeripheral):
#   Write(data)            — master writes bytes (register address / config)
#   Read(count)            — master reads count bytes
#   FinishTransmission()   — end of I2C transaction

import math

_reg = [0x01]       # last written register address
_tick = [0]         # oscillation counter

def _humidity_raw():
    """Oscillate between ~40% and ~80% relative humidity."""
    _tick[0] += 1
    rh = 60.0 + 20.0 * math.sin(_tick[0] * 0.35)
    return int((rh / 100.0) * 65536) & 0xFFFF

def _temperature_raw():
    """Fixed ~25 °C ambient."""
    temp_c = 25.0
    return int(((temp_c + 40.0) / 165.0) * 65536) & 0xFFFF

def Read(count):
    r = _reg[0]
    if r == 0x01:
        raw = _humidity_raw()
    elif r == 0x00:
        raw = _temperature_raw()
    elif r == 0x02:
        return [0x10, 0x00][:count]   # default config
    else:
        return [0x00] * count
    return [(raw >> 8) & 0xFF, raw & 0xFF][:count]

def Write(data):
    if data:
        _reg[0] = data[0]

def FinishTransmission():
    pass
