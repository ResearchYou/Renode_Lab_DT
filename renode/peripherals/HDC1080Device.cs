/**
 * HDC1080Device.cs
 *
 * Renode I2C peripheral implementing the TI HDC1080 humidity/temperature sensor.
 *
 * Registers:
 *   0x00 — Temperature  (read 2 bytes, big-endian uint16)
 *   0x01 — Humidity     (read 2 bytes, big-endian uint16)
 *   0x02 — Configuration (16-bit, returns default 0x1000)
 *
 * Humidity encoding:   RH%  = (raw / 65536) * 100
 * Temperature encoding: T°C = (raw / 65536) * 165 − 40
 *
 * Humidity oscillates between ~40% and ~80% via a sine wave so the
 * firmware receives plausible, varying readings.
 */

using System;
using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;

namespace Antmicro.Renode.Peripherals.I2C
{
    public class HDC1080Device : II2CPeripheral
    {
        public HDC1080Device()
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
            if (data.Length > 0)
            {
                selectedRegister = data[0];
            }
        }

        public byte[] Read(int count = 1)
        {
            ushort raw;

            switch (selectedRegister)
            {
                case 0x01: // Humidity
                    raw = HumidityRaw();
                    break;
                case 0x00: // Temperature
                    raw = TemperatureRaw();
                    break;
                case 0x02: // Configuration
                    return Truncate(new byte[] { 0x10, 0x00 }, count);
                default:
                    return new byte[count];
            }

            return Truncate(new byte[] {
                (byte)((raw >> 8) & 0xFF),
                (byte)(raw & 0xFF)
            }, count);
        }

        public void FinishTransmission()
        {
        }

        private ushort HumidityRaw()
        {
            tick++;
            double rh = 60.0 + 20.0 * Math.Sin(tick * 0.35);
            return (ushort)((rh / 100.0) * 65536);
        }

        private ushort TemperatureRaw()
        {
            double tempC = 25.0;
            return (ushort)(((tempC + 40.0) / 165.0) * 65536);
        }

        private static byte[] Truncate(byte[] data, int count)
        {
            if (data.Length <= count)
                return data;
            var result = new byte[count];
            Array.Copy(data, result, count);
            return result;
        }

        private byte selectedRegister;
        private int tick;
    }
}
