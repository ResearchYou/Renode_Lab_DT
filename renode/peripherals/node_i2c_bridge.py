# Node I2C Bridge — Renode I2C Python Peripheral
#
# Placed on the master machine's I2C0 bus at address 0x08.
# Simulates the node's I2C slave interface so the master firmware can poll it
# in the Renode simulation without requiring true inter-machine I2C wiring.
#
# Protocol (mirrors node firmware):
#   Master writes 0x01  → humidity register selected
#   Master reads  2 bytes → big-endian uint16 = humidity * 100
#
# The humidity values follow the same oscillation as hdc1080.py so the
# master receives plausible, varying readings.
#
# Renode Python peripheral interface (II2CPeripheral):
#   Write(data)
#   Read(count)
#   FinishTransmission()

import math

_reg  = [0x01]
_tick = [0]

def _humidity_x100():
    """Returns humidity * 100 as uint16 (range ~4000–8000 → 40.00–80.00%)."""
    _tick[0] += 1
    rh = 60.0 + 20.0 * math.sin(_tick[0] * 0.15)
    return int(rh * 100) & 0xFFFF

def Read(count):
    if _reg[0] == 0x01:
        val = _humidity_x100()
        return [(val >> 8) & 0xFF, val & 0xFF][:count]
    return [0x00] * count

def Write(data):
    if data:
        _reg[0] = data[0]

def FinishTransmission():
    pass
