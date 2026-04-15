export interface Check {
  name: string
  passed: boolean
}

export interface MachineEvent {
  type: string
  machine: string
  message: string
}

export interface HumidityReading {
  poll: number
  humidity: number
}

export interface ReportData {
  generated_at: string
  all_pass: boolean
  checks: Check[]
  chart_b64: string
  node_events: MachineEvent[]
  master_events: MachineEvent[]
  humidity_readings: HumidityReading[]
  node_raw_uart: string
  master_raw_uart: string
}

declare global {
  interface Window {
    __REPORT_DATA__: ReportData
  }
}
