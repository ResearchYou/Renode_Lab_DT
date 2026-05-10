/**
 * NodeI2CBridge.cs
 *
 * Renode I2C peripheral simulating the node's I2C slave interface so the
 * master firmware can poll it within the Renode simulation without requiring
 * true inter-machine I2C wiring.
 *
 * Protocol:
 *   Master writes 0x01, selecting the humidity register.
 *   Master reads 2 bytes, a big-endian uint16 holding humidity * 100.
 *
 * Humidity values follow the same oscillation as HDC1080Device so the
 * master receives plausible, varying readings.
 */

using System;

namespace Antmicro.Renode.Peripherals.I2C
{
    public class NodeI2CBridge : II2CPeripheral
    {
        public NodeI2CBridge()
        {
            Reset();
        }

        public void Reset()
        {
            selectedRegister = 0x01;
            tick = 0;
        }

        public void Write(byte[] data)
        {
            if(data.Length > 0)
            {
                selectedRegister = data[0];
            }
        }

        public byte[] Read(int count = 1)
        {
            if(selectedRegister == 0x01)
            {
                var val = HumidityX100();
                return Truncate(new byte[] {
                    (byte)((val >> 8) & 0xFF),
                    (byte)(val & 0xFF)
                }, count);
            }

            return new byte[count];
        }

        public void FinishTransmission()
        {
        }

        private ushort HumidityX100()
        {
            tick++;
            var rh = 60.0 + 20.0 * Math.Sin(tick * 0.15);
            return (ushort)(int)(rh * 100);
        }

        private static byte[] Truncate(byte[] data, int count)
        {
            if(data.Length <= count)
            {
                return data;
            }

            var result = new byte[count];
            Array.Copy(data, result, count);
            return result;
        }

        private byte selectedRegister;
        private int tick;
    }
}
