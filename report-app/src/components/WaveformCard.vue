<script setup lang="ts">
defineProps<{ waveformB64: string }>()
</script>

<template>
  <div class="card full">
    <h2>GPIO 25 Waveform</h2>
    <img :src="'data:image/png;base64,' + waveformB64" alt="LED waveform">
  </div>
</template>
