<script setup lang="ts">
import type { MachineEvent } from "../types"

defineProps<{
  title: string
  events: MachineEvent[]
}>()

function badgeClass(type: string): string {
  switch (type) {
    case "boot": return "badge-boot"
    case "sensor": return "badge-sensor"
    case "lora_tx": return "badge-lora"
    case "poll": return "badge-poll"
    default: return ""
  }
}

function typeLabel(type: string): string {
  switch (type) {
    case "boot": return "BOOT"
    case "sensor": return "SENSOR"
    case "lora_tx": return "LoRa TX"
    case "poll": return "POLL"
    default: return type.toUpperCase()
  }
}
</script>

<template>
  <div class="card">
    <h2>{{ title }}</h2>
    <p v-if="!events.length" class="card-note">No events recorded.</p>
    <table v-else>
      <tr><th>Type</th><th>Message</th></tr>
      <tr v-for="(e, i) in events" :key="i">
        <td><span class="badge" :class="badgeClass(e.type)">{{ typeLabel(e.type) }}</span></td>
        <td class="msg">{{ e.message }}</td>
      </tr>
    </table>
  </div>
</template>

<style scoped>
.badge { font-weight: 700; font-size: 0.75em; padding: 2px 8px; border-radius: 4px; white-space: nowrap; }
.badge-boot { background: #1a2e3d; color: #00d4ff; }
.badge-sensor { background: #1a2e1a; color: #2ecc71; }
.badge-lora { background: #2e2e1a; color: #f1c40f; }
.badge-poll { background: #1a1a3d; color: #9b59b6; }
.msg { font-family: monospace; font-size: 0.85em; color: #a0a0c0; }
.card-note { font-size: 0.82em; color: #888; }
</style>
