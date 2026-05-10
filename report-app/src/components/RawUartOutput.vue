<script setup lang="ts">
defineProps<{
  title: string
  rawUart: string
}>()
</script>

<template>
  <div class="card">
    <h2>{{ title }}</h2>
    <pre>{{ rawUart || "(no output)" }}</pre>
  </div>
</template>
