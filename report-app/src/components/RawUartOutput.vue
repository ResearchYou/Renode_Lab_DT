<script setup lang="ts">
defineProps<{ rawUart: string }>()
</script>

<template>
  <div class="card">
    <h2>Raw UART Output</h2>
    <pre>{{ rawUart }}</pre>
  </div>
</template>
