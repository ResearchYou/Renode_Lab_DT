<script setup lang="ts">
import type { Check } from '../types'
defineProps<{ checks: Check[] }>()
</script>

<template>
  <div class="card">
    <h2>Validation Checks</h2>
    <table>
      <tr><th>Check</th><th>Result</th></tr>
      <tr v-for="check in checks" :key="check.name">
        <td>{{ check.name }}</td>
        <td>
          <span class="badge" :class="check.passed ? 'badge-pass' : 'badge-fail'">
            {{ check.passed ? 'PASS' : 'FAIL' }}
          </span>
        </td>
      </tr>
    </table>
  </div>
</template>

<style scoped>
.badge { font-weight: 700; font-size: 0.8em; padding: 2px 8px; border-radius: 4px; }
.badge-pass { background: #0d2e1a; color: #2ecc71; }
.badge-fail { background: #2e0d0d; color: #e74c3c; }
</style>
