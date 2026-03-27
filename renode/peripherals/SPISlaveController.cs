//
// SPISlaveController.cs
//
// Registered on the SLAVE board's sysbus at 0x50000000.
// Receives packets from SPIBridgeSlave (called by the master's Transmit path),
// writes them directly into slave SRAM via SystemBus, and pulses an IRQ
// (connected to NVIC@58 = DMA2_Stream2_IRQn in slave firmware).
//
// Register map:
//   0x00  STATUS  R/W  bit0=rx_ready  (write 1 to bit3 = ack_rx)
//   0x04  RX_COUNT R   number of bytes written to DMA_DST
//   0x08  DMA_DST R/W  slave writes destination buffer address
//   0x0C  DMA_SRC R/W  slave writes response buffer address
//   0x10  CTRL    R/W  bit0=enable  bit1=use_dma  bit2=irq_en  bit3=ack_rx
//

using System;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.SPI
{
    public class SPISlaveController : IDoubleWordPeripheral, IKnownSize, IPeripheral
    {
        private const int PacketSize = 21;

        private readonly IMachine machine;

        public GPIO IRQ { get; } = new GPIO();

        private uint statusReg;
        private uint rxCount;
        private ulong dmaDstAddr;
        private ulong dmaSrcAddr;
        private uint ctrlReg;

        public long Size => 0x100;

        public SPISlaveController(IMachine machine)
        {
            this.machine = machine;
        }

        public SPISlaveController(IMachine machine, long size)
            : this(machine)
        {
        }

        public SPISlaveController(IMachine machine, ulong size)
            : this(machine)
        {
        }

        // Called by SPIBridgeSlave when master finishes sending a TX packet
        public void DeliverPacket(byte[] data)
        {
            int len = Math.Min(data.Length, PacketSize);
            bool useDma   = (ctrlReg & 0x2) != 0;
            bool irqEn    = (ctrlReg & 0x4) != 0;
            bool enabled  = (ctrlReg & 0x1) != 0;

            if (!enabled) return;

            if (useDma && dmaDstAddr != 0)
            {
                for (int i = 0; i < len; i++)
                    machine.SystemBus.WriteByte(dmaDstAddr + (ulong)i, data[i]);
            }

            rxCount   = (uint)len;
            statusReg |= 0x1;  // rx_ready

            if (irqEn)
            {
                IRQ.Set();
                IRQ.Unset();
            }
        }

        // Called by SPIBridgeSlave at the start of the master's RX transfer,
        // after the slave ISR has had time to write a response to DMA_SRC.
        public byte[] ReadResponse()
        {
            var response = new byte[PacketSize];
            if (dmaSrcAddr != 0)
            {
                for (int i = 0; i < PacketSize; i++)
                    response[i] = machine.SystemBus.ReadByte(dmaSrcAddr + (ulong)i);
                this.Log(LogLevel.Info,
                    "SPISlaveCtrl: ReadResponse dmaSrcAddr=0x{0:X8} bytes[0..2]={1:X2} {2:X2} {3:X2}",
                    dmaSrcAddr, response[0], response[1], response[2]);
            }
            else
            {
                this.Log(LogLevel.Warning, "SPISlaveCtrl: ReadResponse called with dmaSrcAddr=0");
                for (int i = 0; i < PacketSize; i++) response[i] = 0xFF;
            }
            return response;
        }

        public uint ReadDoubleWord(long offset)
        {
            switch (offset)
            {
                case 0x00: return statusReg;
                case 0x04: return rxCount;
                case 0x08: return (uint)dmaDstAddr;
                case 0x0C: return (uint)dmaSrcAddr;
                case 0x10: return ctrlReg;
                default:
                    this.Log(LogLevel.Warning,
                        "SPISlaveController: unhandled read at offset 0x{0:X}", offset);
                    return 0;
            }
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            switch (offset)
            {
                case 0x00:
                    if ((value & 0x8) != 0) statusReg &= ~0x1u; // ack clears rx_ready
                    break;
                case 0x08:
                    dmaDstAddr = value;
                    break;
                case 0x0C:
                    dmaSrcAddr = value;
                    break;
                case 0x10:
                    if ((value & 0x8) != 0) statusReg &= ~0x1u; // ack_rx
                    ctrlReg = value & ~0x8u;  // ack bit is self-clearing
                    break;
                default:
                    this.Log(LogLevel.Warning,
                        "SPISlaveController: unhandled write at offset 0x{0:X} val=0x{1:X}",
                        offset, value);
                    break;
            }
        }

        public void Reset()
        {
            statusReg  = 0;
            rxCount    = 0;
            dmaDstAddr = 0;
            dmaSrcAddr = 0;
            ctrlReg    = 0;
            IRQ.Unset();
        }
    }
}
