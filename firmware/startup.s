.syntax unified
.cpu cortex-m7
.fpu softvfp
.thumb

/* ---- Vector table ---------------------------------------------------- */
.section .isr_vector, "a", %progbits
.align 2
.global g_pfnVectors
g_pfnVectors:
    /* Core vectors */
    .word   _stack_top
    .word   Reset_Handler
    .word   Default_Handler     /* NMI */
    .word   Default_Handler     /* HardFault */
    .word   Default_Handler     /* MemManage */
    .word   Default_Handler     /* BusFault */
    .word   Default_Handler     /* UsageFault */
    .word   0
    .word   0
    .word   0
    .word   0
    .word   Default_Handler     /* SVC */
    .word   Default_Handler     /* DebugMon */
    .word   0
    .word   Default_Handler     /* PendSV */
    .word   Default_Handler     /* SysTick */
    /* Device IRQs 0-57 (positions 16-73) */
    .rept   58
    .word   Default_Handler
    .endr
    /* IRQ58 = DMA2_Stream2 (position 74) */
    .word   DMA2_Stream2_IRQHandler
    /* IRQ59-67 (positions 75-83) */
    .rept   9
    .word   Default_Handler
    .endr

/* ---- Default handler ------------------------------------------------- */
.section .text
.align 2

.thumb_func
.global Default_Handler
.type Default_Handler, %function
Default_Handler:
    b .
.size Default_Handler, .-Default_Handler

/* ---- Reset handler --------------------------------------------------- */
.thumb_func
.global Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    /* Copy .data from flash LMA to RAM VMA */
    ldr     r0, =_sidata
    ldr     r1, =_sdata
    ldr     r2, =_edata
    b       copy_data_check
copy_data:
    ldr     r3, [r0], #4
    str     r3, [r1], #4
copy_data_check:
    cmp     r1, r2
    blo     copy_data

    /* Zero .bss */
    ldr     r0, =_sbss
    ldr     r1, =_ebss
    movs    r2, #0
    b       zero_bss_check
zero_bss:
    str     r2, [r0], #4
zero_bss_check:
    cmp     r0, r1
    blo     zero_bss

    bl      main
    b       .
.size Reset_Handler, .-Reset_Handler

/* ---- Weak alias for DMA2_Stream2_IRQHandler -------------------------- */
.weak   DMA2_Stream2_IRQHandler
.thumb_set DMA2_Stream2_IRQHandler, Default_Handler
