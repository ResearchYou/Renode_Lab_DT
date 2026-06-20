using System;
using System.Text;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public class MockUSBHIDHost : IDoubleWordPeripheral, IKnownSize, IPeripheral
    {
        private const int ReportSize = 64;

        private const long StatusOffset = 0x00;
        private const long ControlOffset = 0x04;
        private const long OutBase = 0x100;
        private const long InBase = 0x200;

        private const uint StatusOutReady = 1u << 0;
        private const uint StatusInReady = 1u << 1;
        private const uint StatusTouchPresent = 1u << 2;
        private const uint StatusDone = 1u << 3;

        private const uint ControlAckOut = 1u << 0;
        private const uint ControlSendIn = 1u << 1;

        private const byte Magic0 = 0x59;
        private const byte Magic1 = 0x4B;
        private const byte Version = 0x01;
        private const byte Guard = 0x7E;

        private const byte CmdGetInfo = 0x01;
        private const byte CmdHmacSha1 = 0x03;

        private const byte StatusOk = 0x00;

        private readonly byte[] outReport = new byte[ReportSize];
        private readonly byte[] inReport = new byte[ReportSize];
        private readonly ScriptedRequest[] script;

        private int scriptIndex;
        private bool outReady;
        private bool inReady;
        private bool done;

        public long Size => 0x300;

        public MockUSBHIDHost()
        {
            script = new[]
            {
                new ScriptedRequest(1, CmdGetInfo,  0x00000000u, 0, false, "GET_INFO", StatusOk,
                    null),
                new ScriptedRequest(2, CmdHmacSha1, 0x00000000u, 0, false, "RFC2202_TC1", StatusOk,
                    "B617318655057264E28BC0B6FB378C8EF146BE00"),
                new ScriptedRequest(3, CmdHmacSha1, 0x00000000u, 1, false, "RFC2202_TC2", StatusOk,
                    "EFFCDF6AE5EB2FA2D27416D5F184DF9C259A7C79"),
                new ScriptedRequest(4, CmdHmacSha1, 0x00000000u, 2, false, "RFC2202_TC3", StatusOk,
                    "125D7342B9AC11CD91A39AF48AA17B4F63F175D3"),
            };
            LoadCurrentRequest();
        }

        public uint ReadDoubleWord(long offset)
        {
            if (offset == StatusOffset)
            {
                var status = 0u;
                if (outReady) status |= StatusOutReady;
                if (inReady) status |= StatusInReady;
                if (done) status |= StatusDone;
                if (!done && scriptIndex < script.Length && script[scriptIndex].TouchPresent)
                {
                    status |= StatusTouchPresent;
                }
                return status;
            }

            if (offset >= OutBase && offset < OutBase + ReportSize)
            {
                return ReadWord(outReport, offset - OutBase);
            }

            if (offset >= InBase && offset < InBase + ReportSize)
            {
                return ReadWord(inReport, offset - InBase);
            }

            return 0;
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            if (offset == ControlOffset)
            {
                if ((value & ControlAckOut) != 0)
                {
                    outReady = false;
                    this.Log(LogLevel.Info, "MOCK_USB_HOST: OUT_ACK");
                }

                if ((value & ControlSendIn) != 0)
                {
                    inReady = true;
                    HandleTokenResponse();
                    AdvanceScript();
                }
                return;
            }

            if (offset >= InBase && offset < InBase + ReportSize)
            {
                WriteWord(inReport, offset - InBase, value);
            }
        }

        public void Reset()
        {
            Array.Clear(outReport, 0, outReport.Length);
            Array.Clear(inReport, 0, inReport.Length);
            scriptIndex = 0;
            outReady = false;
            inReady = false;
            done = false;
            LoadCurrentRequest();
        }

        private void AdvanceScript()
        {
            scriptIndex++;
            inReady = false;

            if (scriptIndex >= script.Length)
            {
                done = true;
                this.Log(LogLevel.Info, "MOCK_USB_HOST: SCRIPT_DONE");
                return;
            }

            LoadCurrentRequest();
        }

        private void LoadCurrentRequest()
        {
            if (scriptIndex >= script.Length)
            {
                done = true;
                return;
            }

            Array.Clear(outReport, 0, outReport.Length);
            Array.Clear(inReport, 0, inReport.Length);

            var req = script[scriptIndex];
            outReport[0] = Magic0;
            outReport[1] = Magic1;
            outReport[2] = Version;
            outReport[3] = req.Sequence;
            outReport[4] = req.Command;
            WriteUInt32(outReport, 6, req.Nonce);

            for (var i = 0; i < 32; i++)
            {
                outReport[10 + i] = (byte)(req.ChallengeSeed + i);
            }
            if (req.Command == CmdHmacSha1)
            {
                outReport[10] = req.ChallengeSeed;
            }

            outReport[62] = Crc8(outReport, ReportSize - 2);
            outReport[63] = Guard;
            outReady = true;

            this.Log(LogLevel.Info,
                "MOCK_USB_HOST: OUT seq={0} label={1} cmd={2} touch={3} nonce=0x{4:X8} challenge={5}",
                req.Sequence, req.Label, CommandName(req.Command), req.TouchPresent ? 1 : 0,
                req.Nonce, Hex(outReport, 10, 32));
        }

        private void HandleTokenResponse()
        {
            if (scriptIndex >= script.Length)
            {
                return;
            }

            var req = script[scriptIndex];
            var seq = inReport[3];
            var status = inReport[4];
            var counter = ReadUInt32(inReport, 6);
            var crcOk = inReport[0] == Magic0 &&
                        inReport[1] == Magic1 &&
                        inReport[2] == Version &&
                        inReport[63] == Guard &&
                        inReport[62] == Crc8(inReport, ReportSize - 2);

            this.Log(LogLevel.Info,
                "MOCK_USB_HOST: IN seq={0} label={1} status={2} counter={3} crc={4} mac={5}",
                seq, req.Label, StatusName(status), counter, crcOk ? "OK" : "BAD",
                Hex(inReport, 10, 32));

            var digestOk = req.ExpectedDigest == null ||
                           Hex(inReport, 10, 20).Equals(req.ExpectedDigest, StringComparison.Ordinal);

            if (status == req.ExpectedStatus && crcOk && digestOk)
            {
                if (req.ExpectedDigest != null)
                {
                    this.Log(LogLevel.Info,
                        "MOCK_USB_HOST: CHECK {0} OK expected={1} actual={2}",
                        req.Label, req.ExpectedDigest, Hex(inReport, 10, 20));
                    return;
                }
                this.Log(LogLevel.Info, "MOCK_USB_HOST: CHECK {0} OK", req.Label);
                return;
            }

            if (req.ExpectedDigest != null && status == req.ExpectedStatus && crcOk)
            {
                this.Log(LogLevel.Error,
                    "MOCK_USB_HOST: CHECK {0} FAIL expected_digest={1} actual_digest={2}",
                    req.Label, req.ExpectedDigest, Hex(inReport, 10, 20));
                return;
            }

            this.Log(LogLevel.Error,
                "MOCK_USB_HOST: CHECK {0} FAIL expected={1} actual={2} crc={3}",
                req.Label, StatusName(req.ExpectedStatus), StatusName(status), crcOk ? "OK" : "BAD");
        }

        private static uint ReadWord(byte[] buffer, long offset)
        {
            var index = (int)offset;
            return (uint)(buffer[index] |
                         (buffer[index + 1] << 8) |
                         (buffer[index + 2] << 16) |
                         (buffer[index + 3] << 24));
        }

        private static void WriteWord(byte[] buffer, long offset, uint value)
        {
            var index = (int)offset;
            buffer[index + 0] = (byte)value;
            buffer[index + 1] = (byte)(value >> 8);
            buffer[index + 2] = (byte)(value >> 16);
            buffer[index + 3] = (byte)(value >> 24);
        }

        private static void WriteUInt32(byte[] buffer, int index, uint value)
        {
            buffer[index + 0] = (byte)value;
            buffer[index + 1] = (byte)(value >> 8);
            buffer[index + 2] = (byte)(value >> 16);
            buffer[index + 3] = (byte)(value >> 24);
        }

        private static uint ReadUInt32(byte[] buffer, int index)
        {
            return (uint)(buffer[index] |
                         (buffer[index + 1] << 8) |
                         (buffer[index + 2] << 16) |
                         (buffer[index + 3] << 24));
        }

        private static string Hex(byte[] buffer, int start, int length)
        {
            var builder = new StringBuilder(length * 2);
            for (var i = 0; i < length; i++)
            {
                builder.Append(buffer[start + i].ToString("X2"));
            }
            return builder.ToString();
        }

        private static byte Crc8(byte[] data, int length)
        {
            byte crc = 0;
            for (var i = 0; i < length; i++)
            {
                crc ^= data[i];
                for (var bit = 0; bit < 8; bit++)
                {
                    crc = (crc & 0x80) != 0 ? (byte)((crc << 1) ^ 0x07)
                                            : (byte)(crc << 1);
                }
            }
            return crc;
        }

        private static string CommandName(byte command)
        {
            switch (command)
            {
                case CmdGetInfo: return "GET_INFO";
                case CmdHmacSha1: return "HMAC_SHA1";
                default: return "UNKNOWN";
            }
        }

        private static string StatusName(byte status)
        {
            switch (status)
            {
                case 0x00: return "OK";
                case 0x01: return "ERR_CRC";
                case 0x02: return "ERR_CMD";
                case 0x03: return "ERR_TOUCH";
                case 0x04: return "ERR_REPLAY";
                default: return "ERR_UNKNOWN";
            }
        }

        private struct ScriptedRequest
        {
            public ScriptedRequest(byte sequence, byte command, uint nonce,
                byte challengeSeed, bool touchPresent, string label, byte expectedStatus,
                string expectedDigest)
            {
                Sequence = sequence;
                Command = command;
                Nonce = nonce;
                ChallengeSeed = challengeSeed;
                TouchPresent = touchPresent;
                Label = label;
                ExpectedStatus = expectedStatus;
                ExpectedDigest = expectedDigest;
            }

            public readonly byte Sequence;
            public readonly byte Command;
            public readonly uint Nonce;
            public readonly byte ChallengeSeed;
            public readonly bool TouchPresent;
            public readonly string Label;
            public readonly byte ExpectedStatus;
            public readonly string ExpectedDigest;
        }
    }
}
