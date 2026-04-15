<script setup lang="ts">
defineProps<{ generatedAt: string }>()
</script>

<template>
  <h1>RP2040 Digital Twin &mdash; Dual I2C Humidity / LoRaWAN Test Report</h1>
  <p class="meta">
    Generated: {{ generatedAt }}
    &nbsp;|&nbsp; Firmware: Node (HDC1080 + SX1276) + Master (I2C poll)
    &nbsp;|&nbsp; Platform: Renode 1.15.3 (dual emulated RP2040)
  </p>
</template>
