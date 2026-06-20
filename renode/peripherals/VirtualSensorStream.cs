using Antmicro.Renode.Core;

namespace Antmicro.Renode.Peripherals.I2C
{
    public class VirtualSensorStream : II2CPeripheral
    {
        public VirtualSensorStream()
        {
            Reset();
        }

        public void Reset()
        {
            selectedRegister = RegisterNextSample;
            sampleIndex = 0;
            readOffset = 0;
            frameActive = false;
        }

        public void Write(byte[] data)
        {
            if(data.Length > 0)
            {
                selectedRegister = data[0];
                readOffset = 0;
                frameActive = false;
            }
        }

        public byte[] Read(int count = 1)
        {
            if(selectedRegister != RegisterNextSample)
            {
                return new byte[count];
            }

            if(!frameActive)
            {
                LoadFrame();
            }

            var result = new byte[count];
            for(var i = 0; i < count; i++)
            {
                result[i] = currentFrame[readOffset];
                readOffset++;
                if(readOffset >= currentFrame.Length)
                {
                    AdvanceSample();
                    if(i + 1 < count)
                    {
                        LoadFrame();
                    }
                }
            }

            return result;
        }

        public void FinishTransmission()
        {
        }

        private void LoadFrame()
        {
            var index = sampleIndex;
            if(index >= Samples.Length)
            {
                index = Samples.Length - 1;
            }

            var value = Samples[index];
            currentFrame[0] = (byte)(index >> 8);
            currentFrame[1] = (byte)index;
            currentFrame[2] = (byte)(value >> 8);
            currentFrame[3] = (byte)value;
            readOffset = 0;
            frameActive = true;
        }

        private void AdvanceSample()
        {
            if(sampleIndex < Samples.Length)
            {
                sampleIndex++;
            }
            readOffset = 0;
            frameActive = false;
        }

        private const byte RegisterNextSample = 0x00;

        private static readonly short[] Samples =
        {
            500, 506, 612, 520, 785, 532, 340, 650, 545, 498,
        };

        private byte selectedRegister;
        private int sampleIndex;
        private int readOffset;
        private bool frameActive;
        private readonly byte[] currentFrame = new byte[4];
    }
}
