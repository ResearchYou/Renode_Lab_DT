<script setup lang="ts">
defineProps<{ allPass: boolean }>()
</script>

<template>
  <div class="card">
    <h2>Overall Result</h2>
    <div class="status" :class="allPass ? 'pass' : 'fail'">
      {{ allPass ? '\u2713 ALL CHECKS PASSED' : '\u2717 SOME CHECKS FAILED' }}
    </div>
  </div>
</template>

<style scoped>
.status { font-size: 1.25em; font-weight: 700; padding: 10px 18px; border-radius: 6px; display: inline-block; margin-top: 4px; }
.status.pass { background: #0d2e1a; color: #2ecc71; border: 1px solid #2ecc7155; }
.status.fail { background: #2e0d0d; color: #e74c3c; border: 1px solid #e74c3c55; }
</style>
