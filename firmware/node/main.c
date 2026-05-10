/*
 * Node firmware — RP2040
 *
 * Peripherals:
 *   I2C0 (GP4=SDA, GP5=SCL) — HDC1080 humidity sensor at address 0x40
 *   SPI0 (GP16=MISO, GP17=CS, GP18=SCK, GP19=MOSI) — SX1276 LoRa module
 *   I2C1 (GP2=SDA, GP3=SCL) — I2C slave at address 0x08 (polled by master)
 *
 * Behaviour:
 *   Every SAMPLE_INTERVAL_MS:
 *     1. Read humidity and temperature from HDC1080 over I2C0
 *     2. Broadcast both readings over LoRa (SX1276 SPI)
 *     3. Update internal registers so I2C1 slave can serve master polls
 */

#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/structs/i2c.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include <stdio.h>


/* ── HDC1080 ──────────────────────────────────────────────────── */
#define HDC1080_ADDR 0x40
#define HDC1080_REG_TEMP 0x00
#define HDC1080_REG_HUM 0x01
#define HDC1080_REG_CONFIG 0x02

/* ── SX1276 ───────────────────────────────────────────────────── */
#define LORA_CS_PIN 17
#define LORA_MISO_PIN 16
#define LORA_SCK_PIN 18
#define LORA_MOSI_PIN 19

#define SX1276_REG_FIFO 0x00
#define SX1276_REG_OPMODE 0x01
#define SX1276_REG_PAYLEN 0x22
#define SX1276_LORA_STANDBY 0x81 /* LoRa mode + standby */
#define SX1276_LORA_TX 0x83      /* LoRa mode + TX */

/* ── I2C1 slave ───────────────────────────────────────────────── */
#define NODE_SLAVE_ADDR 0x08
#define NODE_REG_HUMIDITY 0x01
#define NODE_REG_TEMPERATURE 0x02

#define LORA_PAYLOAD_TYPE_HUM_TEMP 'A'

#define SAMPLE_INTERVAL_MS 2000

/* Shared values in units of 0.01 (humidity %RH, temperature C). */
static volatile uint16_t g_humidity_x100 = 0;
static volatile uint16_t g_temperature_x100 = 0;

/* ── HDC1080 ──────────────────────────────────────────────────── */
static float hdc1080_read_humidity(void) {
  uint8_t reg = HDC1080_REG_HUM;
  uint8_t raw[2] = {0};

  i2c_write_blocking(i2c0, HDC1080_ADDR, &reg, 1, false);
  sleep_ms(15); /* 14-bit conversion ~14.85 ms */
  i2c_read_blocking(i2c0, HDC1080_ADDR, raw, 2, false);

  uint16_t val = (uint16_t)((raw[0] << 8) | raw[1]);
  return (val / 65536.0f) * 100.0f;
}

static float hdc1080_read_temperature(void) {
  uint8_t reg = HDC1080_REG_TEMP;
  uint8_t raw[2] = {0};

  i2c_write_blocking(i2c0, HDC1080_ADDR, &reg, 1, false);
  sleep_ms(15); /* 14-bit conversion ~14.85 ms */
  i2c_read_blocking(i2c0, HDC1080_ADDR, raw, 2, false);

  uint16_t val = (uint16_t)((raw[0] << 8) | raw[1]);
  return ((val / 65536.0f) * 165.0f) - 40.0f;
}

/* ── SX1276 ───────────────────────────────────────────────────── */
static void lora_write_reg(uint8_t reg, uint8_t value) {
  uint8_t buf[2] = {(uint8_t)(0x80u | reg), value};
  gpio_put(LORA_CS_PIN, 0);
  spi_write_blocking(spi0, buf, 2);
  gpio_put(LORA_CS_PIN, 1);
}

static uint8_t lora_read_reg(uint8_t reg) {
  uint8_t tx[2] = {reg & 0x7Fu, 0x00};
  uint8_t rx[2] = {0};
  gpio_put(LORA_CS_PIN, 0);
  spi_write_read_blocking(spi0, tx, rx, 2);
  gpio_put(LORA_CS_PIN, 1);
  return rx[1];
}

/*
 * Minimal LoRa frame: 1 byte type + humidity_x100 + temperature_x100
 * in big-endian order. In a real deployment this would carry a proper
 * LoRaWAN MAC frame.
 */
static void lora_transmit(float humidity, float temperature) {
  uint16_t hum_x100 = (uint16_t)(humidity * 100.0f);
  uint16_t temp_x100 = (uint16_t)(temperature * 100.0f);
  uint8_t payload[5] = {
      LORA_PAYLOAD_TYPE_HUM_TEMP,
      (uint8_t)(hum_x100 >> 8),
      (uint8_t)(hum_x100 & 0xFFu),
      (uint8_t)(temp_x100 >> 8),
      (uint8_t)(temp_x100 & 0xFFu),
  };

  lora_write_reg(SX1276_REG_OPMODE, SX1276_LORA_STANDBY);
  sleep_ms(10);

  /* Burst-write payload into FIFO */
  gpio_put(LORA_CS_PIN, 0);
  uint8_t fifo_cmd = 0x80u | SX1276_REG_FIFO;
  spi_write_blocking(spi0, &fifo_cmd, 1);
  spi_write_blocking(spi0, payload, sizeof(payload));
  gpio_put(LORA_CS_PIN, 1);

  lora_write_reg(SX1276_REG_PAYLEN, sizeof(payload));
  lora_write_reg(SX1276_REG_OPMODE, SX1276_LORA_TX);

  printf("[NODE] LoRa TX: type=%c hum=%.1f%% temp=%.2fC\n",
         LORA_PAYLOAD_TYPE_HUM_TEMP, humidity, temperature);
}

/* ── I2C1 slave ───────────────────────────────────────────────── */
/*
 * Minimal polling I2C slave using raw RP2040 I2C hardware registers.
 * Protocol: master writes register address byte:
 *   0x01 humidity (%RH * 100)
 *   0x02 temperature (°C * 100)
 * then reads 2 bytes back (uint16 big-endian).
 */
static void i2c1_slave_init(void) {
  i2c_hw_t *hw = i2c1_hw;

  hw->enable = 0;
  /* Clear MASTER_MODE (bit 0) and IC_SLAVE_DISABLE (bit 6) */
  hw->con = (hw->con & ~(uint32_t)I2C_IC_CON_MASTER_MODE_BITS &
             ~(uint32_t)I2C_IC_CON_IC_SLAVE_DISABLE_BITS);
  hw->sar = NODE_SLAVE_ADDR;
  hw->enable = 1;
}

static void i2c1_slave_poll(void) {
  i2c_hw_t *hw = i2c1_hw;

  /* RX data available — master sent a register address */
  if (hw->status & I2C_IC_STATUS_RFNE_BITS) {
    uint8_t reg = (uint8_t)(hw->data_cmd & 0xFFu);
    uint16_t val = 0;
    if (reg == NODE_REG_HUMIDITY) {
      val = g_humidity_x100;
    } else if (reg == NODE_REG_TEMPERATURE) {
      val = g_temperature_x100;
    }

    if (reg == NODE_REG_HUMIDITY || reg == NODE_REG_TEMPERATURE) {
      /* Queue 2 bytes for master read */
      hw->data_cmd = (val >> 8) & 0xFFu;
      hw->data_cmd = val & 0xFFu;
    }
  }
}

/* ── main ─────────────────────────────────────────────────────── */
int main(void) {
  stdio_init_all();
  printf("[NODE] Boot: humidity node (HDC1080 + SX1276 LoRaWAN)\n");

  /* I2C0 — master mode, HDC1080 */
  i2c_init(i2c0, 100 * 1000);
  gpio_set_function(4, GPIO_FUNC_I2C);
  gpio_set_function(5, GPIO_FUNC_I2C);
  gpio_pull_up(4);
  gpio_pull_up(5);

  /* SPI0 — SX1276 */
  spi_init(spi0, 1 * 1000 * 1000);
  gpio_set_function(LORA_MISO_PIN, GPIO_FUNC_SPI);
  gpio_set_function(LORA_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(LORA_MOSI_PIN, GPIO_FUNC_SPI);
  gpio_init(LORA_CS_PIN);
  gpio_set_dir(LORA_CS_PIN, GPIO_OUT);
  gpio_put(LORA_CS_PIN, 1);

  uint8_t ver = lora_read_reg(0x42);
  printf("[NODE] SX1276 version: 0x%02X%s\n", ver,
         ver == 0x12 ? " (ok)" : " (unexpected)");

  /* I2C1 — slave mode, responds to master polls */
  i2c_init(i2c1, 100 * 1000);
  gpio_set_function(2, GPIO_FUNC_I2C);
  gpio_set_function(3, GPIO_FUNC_I2C);
  gpio_pull_up(2);
  gpio_pull_up(3);
  i2c1_slave_init();

  printf("[NODE] Initialized. Sampling every %d ms\n", SAMPLE_INTERVAL_MS);

  uint32_t last_sample_ms = 0;

  while (1) {
    uint32_t now_ms = time_us_32() / 1000;

    if (now_ms - last_sample_ms >= SAMPLE_INTERVAL_MS) {
      float humidity = hdc1080_read_humidity();
      float temperature = hdc1080_read_temperature();
      g_humidity_x100 = (uint16_t)(humidity * 100.0f);
      g_temperature_x100 = (uint16_t)(temperature * 100.0f);
      printf("[NODE] HDC1080: humidity=%.1f%% temp=%.2fC\n", humidity,
             temperature);
      lora_transmit(humidity, temperature);
      last_sample_ms = now_ms;
    }

    i2c1_slave_poll();
    sleep_ms(5);
  }

  return 0;
}
