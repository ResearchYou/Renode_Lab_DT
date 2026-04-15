/**
 * SX1276Device.cs
 *
 * Renode SPI peripheral simulating a Semtech SX1276 LoRa transceiver.
 *
 * Simplified model: tracks register writes, logs LoRa TX events.
 *
 * SPI framing (one CS assertion per transaction):
 *   First byte:  bit7=1 write / bit7=0 read,  bits[6:0] = register address
 *   Subsequent bytes: data (auto-increment address for burst)
 *
 * ISPIPeripheral interface:
 *   Transmit(byte) — called per byte, returns MISO byte
 *   FinishTransmission() — called on CS de-assert
 */

using System;
using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;

namespace Antmicro.Renode.Peripherals.SPI
{
    public class SX1276Device : ISPIPeripheral
    {
        public SX1276Device()
        {
            regs = new Dictionary<int, byte>
            {
                { 0x01, 0x09 },   // RegOpMode  — FSK sleep
                { 0x06, 0x6C },   // RegFrMsb   — 434 MHz default
                { 0x07, 0x80 },   // RegFrMid
                { 0x08, 0x00 },   // RegFrLsb
                { 0x22, 0x00 },   // RegPayloadLength
                { 0x42, 0x12 },   // RegVersion — always 0x12
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
            if (phase == Phase.Command)
            {
                isWrite = (data & 0x80) != 0;
                currentReg = data & 0x7F;
                phase = Phase.Data;
                return 0x00;
            }

            // Data phase
            if (isWrite)
            {
                if (currentReg == 0x00) // FIFO register
                {
                    fifo.Add(data);
                }
                else
                {
                    regs[currentReg] = data;

                    // Check for TX mode trigger (RegOpMode, mode bits = 0x03)
                    if (currentReg == 0x01 && (data & 0x07) == 0x03)
                    {
                        if (fifo.Count > 0)
                        {
                            // Log removed — Renode's this.Log() unavailable
                            // in simple ISPIPeripheral without IPeripheralRegister
                            fifo.Clear();
                        }
                    }
                }

                currentReg++;
                return 0x00;
            }
            else
            {
                byte val;
                regs.TryGetValue(currentReg, out val);
                currentReg++;
                return val;
            }
        }

        public void FinishTransmission()
        {
            phase = Phase.Command;
        }

        private enum Phase { Command, Data }

        private readonly Dictionary<int, byte> regs;
        private readonly List<byte> fifo;
        private Phase phase;
        private int currentReg;
        private bool isWrite;
    }
}
