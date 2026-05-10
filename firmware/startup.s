.syntax unified
.cpu cortex-m4
.thumb

.section .isr_vector, "a", %progbits
.align 2
.global g_pfnVectors
g_pfnVectors:
    .word   _stack_top
    .word   Reset_Handler
    .word   Default_Handler
    .word   Default_Handler
    .word   Default_Handler
    .word   Default_Handler
    .word   Default_Handler
    .word   0
    .word   0
    .word   0
    .word   0
    .word   Default_Handler
    .word   Default_Handler
    .word   0
    .word   Default_Handler
    .word   Default_Handler
    .rept   82
    .word   Default_Handler
    .endr

.section .text
.align 2

.thumb_func
.global Default_Handler
.type Default_Handler, %function
Default_Handler:
    b .
.size Default_Handler, .-Default_Handler

.thumb_func
.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
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
