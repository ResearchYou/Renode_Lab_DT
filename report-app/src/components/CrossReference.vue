<script setup lang="ts">
import type { XRefRow } from '../types'

defineProps<{
  gpioLogFound: boolean
  xrefRows: XRefRow[]
}>()

function formatTime(us: number | null): string {
  return us !== null ? (us / 1000).toFixed(1) + ' ms' : '-'
}
</script>

<template>
  <div v-if="gpioLogFound && xrefRows.length" class="card full">
    <h2>Cross-Reference: Firmware Claims vs Emulator GPIO</h2>
    <p class="card-note">Compares UART printf output against actual SIO register writes observed by Renode.</p>
    <table>
      <tr>
        <th>#</th><th>UART Time</th><th>UART State</th>
        <th>GPIO Time</th><th>GPIO State</th><th>Register</th><th>Verdict</th>
      </tr>
      <tr v-for="row in xrefRows" :key="row.index">
        <td>{{ row.index }}</td>
        <td>{{ formatTime(row.uart_time_us) }}</td>
        <td :class="row.uart_state === 'HIGH' ? 'ev-on' : 'ev-off'">{{ row.uart_state }}</td>
        <td>{{ formatTime(row.gpio_time_us) }}</td>
        <td :class="row.gpio_state === 'HIGH' ? 'ev-on' : 'ev-off'">{{ row.gpio_state }}</td>
        <td>{{ row.gpio_reg }}</td>
        <td :class="row.match ? 'ev-on' : 'ev-off'">{{ row.match ? 'MATCH' : 'MISMATCH' }}</td>
      </tr>
    </table>
  </div>

  <div v-else-if="!gpioLogFound" class="card full">
    <h2>Cross-Reference: Firmware Claims vs Emulator GPIO</h2>
    <p class="card-note warn">
      Renode log (renode.log) not found. Enable <code>logFile</code> and
      <code>LogPeripheralAccess sysbus.sio</code> in the .resc script to verify GPIO state independently.
    </p>
  </div>
</template>

<style scoped>
.ev-on  { color: #2ecc71; font-weight: 600; }
.ev-off { color: #e74c3c; font-weight: 600; }
</style>
