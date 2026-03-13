<script setup lang="ts">
defineProps<{ generatedAt: string }>()
</script>

<template>
  <h1>RP2040 Digital Twin &mdash; Test Report</h1>
  <p class="meta">
    Generated: {{ generatedAt }}
    &nbsp;|&nbsp; Firmware: LED blink &times;5
    &nbsp;|&nbsp; Platform: Renode 1.15.3 (emulated RP2040)
  </p>
</template>
