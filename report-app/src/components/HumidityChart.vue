<script setup lang="ts">
import type { HumidityReading } from "../types"

defineProps<{
  chartB64: string
  readings: HumidityReading[]
}>()
</script>

<template>
  <div class="card full">
    <h2>Humidity Readings</h2>
    <div v-if="chartB64">
      <img :src="'data:image/png;base64,' + chartB64" alt="Humidity chart" />
    </div>
    <p v-else class="card-note warn">No humidity data available to chart.</p>
    <table v-if="readings.length" style="margin-top: 14px;">
      <tr><th>Poll #</th><th>Humidity (%)</th></tr>
      <tr v-for="r in readings" :key="r.poll">
        <td>{{ r.poll }}</td>
        <td>{{ r.humidity.toFixed(1) }}%</td>
      </tr>
    </table>
  </div>
</template>

<style scoped>
.card-note.warn { color: #e67e22; font-size: 0.82em; }
</style>
