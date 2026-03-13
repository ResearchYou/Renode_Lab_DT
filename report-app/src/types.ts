export interface Check {
  name: string
  passed: boolean
}

export interface XRefRow {
  index: number
  uart_time_us: number | null
  uart_state: string
  gpio_time_us: number | null
  gpio_state: string
  gpio_reg: string
  match: boolean
}

export interface UartEvent {
  time_us: number
  state: string
  cycle: number
}

export interface ReportData {
  generated_at: string
  all_pass: boolean
  checks: Check[]
  waveform_b64: string
  gpio_log_found: boolean
  xref_rows: XRefRow[]
  uart_events: UartEvent[]
  raw_uart: string
}

declare global {
  interface Window {
    __REPORT_DATA__: ReportData
  }
}
