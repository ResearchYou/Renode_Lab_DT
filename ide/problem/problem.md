# RP2040 Hello World Challenge

Implement and iterate firmware in `firmware/main.c`.

## Goal
- Boot the RP2040 firmware in Renode.
- Print a clear hello-world boot message and basic LED cycle logs over UART.
- Produce a passing HTML report in `output/report.html`.

## Files
- Firmware code: `firmware/main.c`
- Harness output: `output/*.txt`

## Workflow
1. Edit code in the left editor pane.
2. The results panel on the right updates when you refresh it after a test run.
3. Run tests:
   - `docker-compose up digital-twin` (the test runner)

## Optional PDF
Place a PDF statement in this folder (e.g. `problem.pdf`) and open it in a side tab.
