<script setup lang="ts">
import type { UartEvent } from '../types'
defineProps<{ events: UartEvent[] }>()
</script>

<template>
  <div class="card">
    <h2>UART Event Timeline</h2>
    <table>
      <tr><th>Time</th><th>State</th><th>Event</th></tr>
      <tr v-for="e in events" :key="e.time_us + '-' + e.cycle">
        <td>{{ (e.time_us / 1000).toFixed(1) }} ms</td>
        <td :class="e.state === 'LED ON' ? 'ev-on' : 'ev-off'">{{ e.state }}</td>
        <td>Cycle {{ e.cycle }}</td>
      </tr>
    </table>
  </div>
</template>

<style scoped>
.ev-on  { color: #2ecc71; font-weight: 600; }
.ev-off { color: #e74c3c; font-weight: 600; }
</style>
