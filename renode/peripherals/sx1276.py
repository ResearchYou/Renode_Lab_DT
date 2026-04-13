# SX1276 LoRa Transceiver — Renode SPI Python Peripheral
#
# Simplified model: tracks register writes, logs LoRa TX events.
#
# SPI framing (one CS assertion per transaction):
#   First byte:  bit7=1 write / bit7=0 read,  bits[6:0] = register address
#   Subsequent bytes: data (auto-increment address for burst)
#
# Renode Python peripheral interface (ISPIPeripheral):
#   Transfer(byte)       — called per byte, returns MISO byte
#   FinishTransmission() — called on CS de-assert

_regs = {
    0x01: 0x09,   # RegOpMode  — FSK sleep
    0x06: 0x6C,   # RegFrMsb   — 434 MHz default
    0x07: 0x80,   # RegFrMid
    0x08: 0x00,   # RegFrLsb
    0x22: 0x00,   # RegPayloadLength
    0x42: 0x12,   # RegVersion — always 0x12
}
_fifo   = []
_phase  = ['cmd']   # 'cmd' | 'data'
_reg    = [0]
_write  = [False]

def Transfer(data):
    byte = int(data) & 0xFF

    if _phase[0] == 'cmd':
        _write[0] = bool(byte & 0x80)
        _reg[0]   = byte & 0x7F
        _phase[0] = 'data'
        return 0x00

    reg = _reg[0]

    if _write[0]:
        if reg == 0x00:                  # FIFO register
            _fifo.append(byte)
        else:
            _regs[reg] = byte
            if reg == 0x01 and (byte & 0x07) == 0x03:   # TX mode triggered
                if _fifo:
                    self.Log(LogLevel.Info,
                        "[SX1276] LoRa TX: {} byte payload = {}".format(
                            len(_fifo), list(_fifo)))
                    _fifo.clear()
        _reg[0] += 1
        return 0x00
    else:
        val = _regs.get(reg, 0x00)
        _reg[0] += 1
        return val

def FinishTransmission():
    _phase[0] = 'cmd'
