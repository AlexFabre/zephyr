/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <soc.h>
#include <math.h>
#include <zephyr/dsp/types.h>
#include <stm32h5xx_ll_cordic.h>
#include <stm32h5xx_ll_bus.h>
#include "arm_math.h"

/* Use can select/deselect to compute only one of sin or cos, or both */
#define COMPUTE_COSINUS 1
#define COMPUTE_SINUS 1

#define USE_MATH_H 1
#define USE_CMSIS_FAST_MATH 1
#define USE_CORDIC 1

/* Constants and macros */
#if USE_CORDIC
#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define Q31_FACTOR (0x80000000)

#define FLOAT_TO_Q31(input) 	((uint32_t) ((input) * ((float) Q31_FACTOR)))
#define Q31_TO_FLOAT(input)		(((float) (input)) / ((float) Q31_FACTOR))
#endif /* USE_CORDIC */

static void print_result(const char header[10], float angle_rad, float cosOutput, float sinOutput, uint32_t elapsed) {
	printf("%s: ", header);
		#if COMPUTE_COSINUS
		printf("cos(%.2f) = %.4f\t", angle_rad, cosOutput);
		#endif
		#if COMPUTE_SINUS
		printf("sin(%.2f) = %.4f\t", angle_rad, sinOutput);
		#endif
		printf("%4u ticks\n", elapsed);
}

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	for (size_t i = 0; i < 32; i++)
	{
		uint32_t start;
		uint32_t elapsed;
		float cosOutput = 0;
		float sinOutput = 0;

		float angle_rad = 0.0 + (i*0.1);
		
		/* -------------------------------------- */
		/* Math.h */
		/* -------------------------------------- */
		#if USE_MATH_H
		start = k_cycle_get_32();

		#if COMPUTE_COSINUS
		cosOutput = cos(angle_rad);
		#endif
		#if COMPUTE_SINUS
		sinOutput = sin(angle_rad);
		#endif

		/* Calculate number of cycles elapsed */
		elapsed = k_cycle_get_32() - start;
		/* -------------------------------------- */

		print_result("math.h   ", angle_rad, cosOutput, sinOutput, elapsed);
		#endif /* USE_MATH_H */

		/* -------------------------------------- */
		/* ARM CMSIS DSP */
		/* -------------------------------------- */
		#if USE_CMSIS_FAST_MATH
		start = k_cycle_get_32();

		#if COMPUTE_COSINUS
		cosOutput = arm_cos_f32(angle_rad);
		#endif
		#if COMPUTE_SINUS
		sinOutput = arm_sin_f32(angle_rad);
		#endif

		/* Calculate number of cycles elapsed */
		elapsed = k_cycle_get_32() - start;
		/* -------------------------------------- */

		print_result("cmsis DSP", angle_rad, cosOutput, sinOutput, elapsed);
		#endif /* USE_CMSIS_FAST_MATH */
		
		/* -------------------------------------- */
		/* CORDIC */
		/* -------------------------------------- */
		#if USE_CORDIC
		start = k_cycle_get_32();

		/* Configure the CORDIC engine */

		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CORDIC);

		LL_CORDIC_Config(CORDIC,
		LL_CORDIC_FUNCTION_COSINE, /* cosine function */
		LL_CORDIC_PRECISION_6CYCLES, /* max precision for q1.31 cosine */
		LL_CORDIC_SCALE_0, /* no scale */
		LL_CORDIC_NBWRITE_1, /* One input data: angle_rad. Second input data (modulus) is 1 af
		ter cordic reset */
		LL_CORDIC_NBREAD_2, LL_CORDIC_INSIZE_32BITS, LL_CORDIC_OUTSIZE_32BITS); /* Two output data: cosine, then sine */
		/* q1.31 format for input data */
		/* q1.31 format for output data */
		
		/* 	The use of fixed point representation means that input and output values
		 * must be in the range -1 to +1. Input angle_rads in radians must be multiplied
		 * by 1/π. */

		/* Write angle_rad */
		LL_CORDIC_WriteData(CORDIC, FLOAT_TO_Q31(angle_rad / PI));
		/* Read cosine */
		cosOutput = Q31_TO_FLOAT((int32_t)LL_CORDIC_ReadData(CORDIC));
		/* Read sine */
		sinOutput = Q31_TO_FLOAT((int32_t)LL_CORDIC_ReadData(CORDIC));

		/* Calculate number of cycles elapsed */
		elapsed = k_cycle_get_32() - start;
		/* -------------------------------------- */

		print_result("CORDIC   ", angle_rad, cosOutput, sinOutput, elapsed);

		#endif /* USE_CORDIC */
	}

	printf("Done\n");

	return 0;
}
