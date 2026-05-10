/**
 * SX1276Device.cs
 *
 * Renode SPI peripheral simulating a Semtech SX1276 LoRa transceiver.
 *
 * Simplified model: tracks register writes and returns a stable version
 * register so firmware can detect the device.
 *
 * SPI framing:
 *   First byte: bit7=1 write / bit7=0 read, bits[6:0] = register address.
 *   Subsequent bytes: data, with auto-incremented register address.
 */

using System.Collections.Generic;

namespace Antmicro.Renode.Peripherals.SPI
{
    public class SX1276Device : ISPIPeripheral
    {
        public SX1276Device()
        {
            regs = new Dictionary<int, byte>
            {
                { 0x01, 0x09 },
                { 0x06, 0x6C },
                { 0x07, 0x80 },
                { 0x08, 0x00 },
                { 0x22, 0x00 },
                { 0x42, 0x12 },
            };
            fifo = new List<byte>();
            Reset();
        }

        public void Reset()
        {
            fifo.Clear();
            phase = Phase.Command;
            currentReg = 0;
            isWrite = false;
        }

        public byte Transmit(byte data)
        {
            if(phase == Phase.Command)
            {
                isWrite = (data & 0x80) != 0;
                currentReg = data & 0x7F;
                phase = Phase.Data;
                return 0x00;
            }

            if(isWrite)
            {
                if(currentReg == 0x00)
                {
                    fifo.Add(data);
                }
                else
                {
                    regs[currentReg] = data;

                    if(currentReg == 0x01 && (data & 0x07) == 0x03)
                    {
                        fifo.Clear();
                    }
                }

                currentReg++;
                return 0x00;
            }

            byte val;
            regs.TryGetValue(currentReg, out val);
            currentReg++;
            return val;
        }

        public void FinishTransmission()
        {
            phase = Phase.Command;
        }

        private enum Phase
        {
            Command,
            Data
        }

        private readonly Dictionary<int, byte> regs;
        private readonly List<byte> fifo;
        private Phase phase;
        private int currentReg;
        private bool isWrite;
    }
}
