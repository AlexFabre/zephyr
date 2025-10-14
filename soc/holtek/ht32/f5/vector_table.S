/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief HT32F5xxxx Cortex-M0+ vector table
 */

#include <zephyr/toolchain.h>

_ASM_FILE_PROLOGUE

GDATA(z_main_stack)
GDATA(_vector_table)

SECTION_SUBSEC_FUNC(exc_vector_table,_vector_table_section,_vector_table)

    /*
     * Cortex-M0+ vector table
     */
    .word z_main_stack + CONFIG_MAIN_STACK_SIZE  /* Top of Stack */
    .word z_arm_reset                             /* Reset Handler */
    .word z_arm_nmi                               /* NMI Handler */
    .word z_arm_hard_fault                        /* Hard Fault Handler */
    .word 0                                       /* Reserved */
    .word 0                                       /* Reserved */
    .word 0                                       /* Reserved */
    .word 0                                       /* Reserved */
    .word 0                                       /* Reserved */
    .word 0                                       /* Reserved */
    .word 0                                       /* Reserved */
    .word z_arm_svc                               /* SVCall Handler */
    .word 0                                       /* Reserved */
    .word 0                                       /* Reserved */
    .word z_arm_pendsv                            /* PendSV Handler */
    .word z_arm_int_systick                       /* SysTick Handler */

    /* External interrupts */
    .word z_arm_int_spurious    /* 0:  LVD/BOD */
    .word z_arm_int_spurious    /* 1:  RTC */
    .word z_arm_int_spurious    /* 2:  FLASH */
    .word z_arm_int_spurious    /* 3:  EVWUP */
    .word z_arm_int_spurious    /* 4:  EXTI0_1 */
    .word z_arm_int_spurious    /* 5:  EXTI2_3 */
    .word z_arm_int_spurious    /* 6:  EXTI4_15 */
    .word z_arm_int_spurious    /* 7:  Reserved/CAN0 */
    .word z_arm_int_spurious    /* 8:  ADC */
    .word z_arm_int_spurious    /* 9:  Reserved */
    .word z_arm_int_spurious    /* 10: MCTM0_BRK */
    .word z_arm_int_spurious    /* 11: MCTM0_UP */
    .word z_arm_int_spurious    /* 12: MCTM0_TR_UP2 */
    .word z_arm_int_spurious    /* 13: MCTM0_CC */
    .word z_arm_int_spurious    /* 14: GPTM0_G */
    .word z_arm_int_spurious    /* 15: GPTM0_VCLK */
    .word z_arm_int_spurious    /* 16: BFTM0 */
    .word z_arm_int_spurious    /* 17: BFTM1 */
    .word z_arm_int_spurious    /* 18: CMP0 */
    .word z_arm_int_spurious    /* 19: CMP1 */
    .word z_arm_int_spurious    /* 20: Reserved */
    .word z_arm_int_spurious    /* 21: I2C0 */
    .word z_arm_int_spurious    /* 22: SPI0 */
    .word z_arm_int_spurious    /* 23: USART0 */
    .word z_arm_int_spurious    /* 24: UART0 */
    .word z_arm_int_spurious    /* 25: PDMA_CH0_1 */
    .word z_arm_int_spurious    /* 26: PDMA_CH2_3 */
    .word z_arm_int_spurious    /* 27: PDMA_CH4_5/SCI */
    .word z_arm_int_spurious    /* 28: SCTM0 */
    .word z_arm_int_spurious    /* 29: SCTM1/USB */
    .word z_arm_int_spurious    /* 30: SCTM2 */
    .word z_arm_int_spurious    /* 31: SCTM3 */
