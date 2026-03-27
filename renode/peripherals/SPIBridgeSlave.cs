//
// SPIBridgeSlave.cs
//
// Registered on the MASTER board's SPI1 at CS index 0.
// Forwards 21-byte packets to the slave board's SPISlaveController
// and returns the slave's previously prepared response bytes.
//
// Two-transfer protocol per exchange:
//   TX transfer  (first byte == 0xAA): accumulate 21 bytes → auto-deliver to slave
//   RX transfer  (first byte == 0x00): return slave's response for that packet
//
// IMPORTANT: delivery is done inside Transmit() on the 21st byte so it never
// depends on FinishTransmission() / GPIO CS-line wiring (which Renode's
// STM32SPI model does not auto-connect).
//

using System;
using System.Text;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.SPI;

namespace Antmicro.Renode.Peripherals.SPI
{
    public class SPIBridgeSlave : ISPIPeripheral, IGPIOReceiver
    {
        private const int PacketSize = 21;
        private const byte ProtoSync = 0xA5;  // STMP v1 MAGIC

        private string targetMachineName;

        private SPISlaveController target;
        private bool targetResolved;

        // Counters for Renode log output
        private uint csAssertions;
        private uint csDeassertions;
        private uint mosiBytes;
        private uint misoBytes;
        private uint txPackets;
        private uint rxTransfers;

        // Incoming bytes from master
        private readonly byte[] txBuf = new byte[PacketSize];
        private int txCount;
        private bool isRealPacket;

        // Pending response from slave (read at start of RX transfer)
        private readonly byte[] pendingResponse = new byte[PacketSize];
        private int rxIndex;
        private bool hasPacketToDeliver;

        public SPIBridgeSlave(IMachine machine)
        {
            this.targetMachineName = "slave";
            FillDefault(pendingResponse);
        }

        // Settable from .repl / Monitor: targetMachineName: "slave"
        public string TargetMachineName
        {
            get => targetMachineName;
            set
            {
                targetMachineName = value;
                target = null;
                targetResolved = false;
            }
        }

        public byte Transmit(byte data)
        {
            mosiBytes++;

            if (txCount == 0)
            {
                // First byte of transfer determines type
                isRealPacket = (data == ProtoSync);

                if (!isRealPacket)
                {
                    // Start of RX transfer: read slave's response NOW
                    // (slave ISR had time to run during the master's delay loop)
                    EnsureTarget();
                    if (target != null && hasPacketToDeliver)
                    {
                        var resp = target.ReadResponse();
                        if (resp != null)
                            Array.Copy(resp, pendingResponse,
                                       Math.Min(resp.Length, PacketSize));
                        hasPacketToDeliver = false;
                    }
                    rxIndex = 0;
                }
            }

            if (txCount < PacketSize)
                txBuf[txCount++] = data;

            // ----------------------------------------------------------------
            // Auto-deliver on the 21st byte.  Do NOT wait for FinishTransmission()
            // because Renode's STM32SPI does not automatically call it when
            // software-NSS is used without an explicit GPIO→IGPIOReceiver wiring.
            // ----------------------------------------------------------------
            if (txCount == PacketSize)
            {
                if (isRealPacket)
                {
                    EnsureTarget();
                    if (target != null)
                    {
                        target.DeliverPacket(txBuf);
                        hasPacketToDeliver = true;
                        txPackets++;
                        this.Log(LogLevel.Info,
                            "SPI_BUS: MOSI_REQ seq={0} frame={1}", txBuf[1], BytesToHex(txBuf));
                        this.Log(LogLevel.Info,
                            "SPI_LINE: TX_PACKET seq={0} delivered to slave", txBuf[1]);
                    }
                }
                else
                {
                    rxTransfers++;
                    this.Log(LogLevel.Info,
                        "SPI_BUS: MISO_RSP seq={0} frame={1}", pendingResponse[1], BytesToHex(pendingResponse));
                    this.Log(LogLevel.Info,
                        "SPI_LINE: RX_TRANSFER complete ({0} bytes returned)", PacketSize);
                }
                txCount = 0;  // Ready for next transfer
            }

            if (!isRealPacket)
            {
                misoBytes++;
                return (rxIndex < PacketSize) ? pendingResponse[rxIndex++] : (byte)0xFF;
            }

            return 0xFF;
        }

        public void FinishTransmission()
        {
            // Auto-delivery is handled inside Transmit() above.
            // This may be called if CS deasserts mid-packet (partial cleanup).
            if (txCount > 0)
                this.Log(LogLevel.Warning,
                    "SPIBridgeSlave: partial packet discarded ({0} bytes)", txCount);

            csDeassertions++;
            this.Log(LogLevel.Info,
                "SPI_LINE: CS_DEASSERT count={0} mosiBytes={1} misoBytes={2} txPackets={3} rxTransfers={4}",
                csDeassertions, mosiBytes, misoBytes, txPackets, rxTransfers);

            txCount = 0;
            rxIndex = 0;
        }

        public void Reset()
        {
            txCount = 0;
            rxIndex = 0;
            hasPacketToDeliver = false;
            csAssertions = 0;
            csDeassertions = 0;
            mosiBytes = 0;
            misoBytes = 0;
            txPackets = 0;
            rxTransfers = 0;
            FillDefault(pendingResponse);
        }

        // IGPIOReceiver: called if GPIOA pin 4 is connected to this peripheral
        public void OnGPIO(int number, bool value)
        {
            if (number != 0) return;

            if (!value)
            {
                // CS asserted (low): reset counters for clean start
                csAssertions++;
                this.Log(LogLevel.Info, "SPI_LINE: CS_ASSERT count={0}", csAssertions);
                txCount = 0;
                rxIndex = 0;
                return;
            }

            // CS deasserted (high)
            FinishTransmission();
        }

        private void EnsureTarget()
        {
            if (targetResolved) return;
            targetResolved = true;

            var emulation = EmulationManager.Instance.CurrentEmulation;
            IMachine slaveMachine;
            if (!emulation.TryGetMachineByName(targetMachineName, out slaveMachine))
            {
                this.Log(LogLevel.Warning,
                    "SPIBridgeSlave: machine '{0}' not found", targetMachineName);
                return;
            }

            IPeripheral p;
            if (!slaveMachine.TryGetByName("sysbus.spiSlaveCtrl", out p))
            {
                this.Log(LogLevel.Warning,
                    "SPIBridgeSlave: spiSlaveCtrl not found in machine '{0}'",
                    targetMachineName);
                return;
            }

            target = p as SPISlaveController;
            if (target == null)
                this.Log(LogLevel.Warning,
                    "SPIBridgeSlave: spiSlaveCtrl is not a SPISlaveController");
        }

        private static void FillDefault(byte[] buf)
        {
            for (int i = 0; i < buf.Length; i++) buf[i] = 0xFF;
        }

        private static string BytesToHex(byte[] buffer)
        {
            var builder = new StringBuilder(buffer.Length * 3);
            for (var index = 0; index < buffer.Length; index++)
            {
                if(index > 0)
                {
                    builder.Append(' ');
                }
                builder.Append(buffer[index].ToString("X2"));
            }
            return builder.ToString();
        }
    }
}
