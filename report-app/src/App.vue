<script setup lang="ts">
import type { ReportData } from './types'
import ReportHeader from './components/ReportHeader.vue'
import OverallResult from './components/OverallResult.vue'
import ValidationChecks from './components/ValidationChecks.vue'
import WaveformCard from './components/WaveformCard.vue'
import CrossReference from './components/CrossReference.vue'
import UartTimeline from './components/UartTimeline.vue'
import RawUartOutput from './components/RawUartOutput.vue'

const data: ReportData = window.__REPORT_DATA__
</script>

<template>
  <ReportHeader :generated-at="data.generated_at" />
  <div class="grid">
    <OverallResult :all-pass="data.all_pass" />
    <ValidationChecks :checks="data.checks" />
    <WaveformCard :waveform-b64="data.waveform_b64" />
    <CrossReference
      :gpio-log-found="data.gpio_log_found"
      :xref-rows="data.xref_rows"
    />
    <UartTimeline :events="data.uart_events" />
    <RawUartOutput :raw-uart="data.raw_uart" />
  </div>
</template>

<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: 'Segoe UI', system-ui, sans-serif; background: #0d0d1a; color: #dde1f0; padding: 28px; }
h1 { color: #00d4ff; font-size: 1.6em; border-bottom: 2px solid #00d4ff22; padding-bottom: 10px; margin-bottom: 6px; }
h2 { color: #7eb8f7; font-size: 1.05em; margin-bottom: 12px; text-transform: uppercase; letter-spacing: .06em; }
.meta { color: #666; font-size: 0.82em; margin-bottom: 24px; }
.grid { display: grid; grid-template-columns: 1fr 1fr; gap: 18px; margin-bottom: 18px; }
.card { background: #141428; border: 1px solid #252545; border-radius: 8px; padding: 18px 20px; }
.card.full { grid-column: 1 / -1; }
.card-note { font-size: 0.82em; color: #888; margin-bottom: 10px; }
.card-note.warn { color: #e67e22; }
table { border-collapse: collapse; width: 100%; font-size: 0.9em; }
th, td { padding: 7px 12px; border: 1px solid #252545; text-align: left; }
th { background: #1a1a35; color: #7eb8f7; }
pre { background: #090917; border: 1px solid #252545; padding: 14px; border-radius: 6px;
      font-size: 0.82em; color: #90d090; overflow-x: auto; white-space: pre-wrap; }
img { width: 100%; border-radius: 6px; border: 1px solid #252545; }
</style>
