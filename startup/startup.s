.syntax unified
.cpu cortex-m3
.thumb

.global Reset_Handler
.global Default_Handler

.extern main

.section .isr_vector, "a", %progbits
.type g_pfnVectors, %object

g_pfnVectors:
    .word _estack
    .word Reset_Handler

    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word 0
    .word 0
    .word 0
    .word Default_Handler
    .word Default_Handler
    .word 0
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler

.size g_pfnVectors, . - g_pfnVectors


.section .text.Reset_Handler
.type Reset_Handler, %function

Reset_Handler:

    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata

copy_data:
    cmp r1, r2
    bge zero_bss

    ldr r3, [r0]
    str r3, [r1]

    adds r0, r0, #4
    adds r1, r1, #4

    b copy_data


zero_bss:

    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0

clear_bss:
    cmp r1, r2
    bge call_main

    str r3, [r1]
    adds r1, r1, #4

    b clear_bss


call_main:

    bl main


hang:
    b hang


.size Reset_Handler, . - Reset_Handler


.section .text.Default_Handler
.type Default_Handler, %function

Default_Handler:
    b Default_Handler

.size Default_Handler, . - Default_Handler
