/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/reset.h>
#include <zephyr/sys/byteorder.h>
#include <soc.h>

#define LOG_LEVEL CONFIG_CORDIC_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cordic_stm32);

#if DT_HAS_COMPAT_STATUS_OKAY(st_stm32_cordic)
#define DT_DRV_COMPAT st_stm32_cordic
#else
#error No STM32 CORDIC coprocessor in device tree
#endif

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/* Conversion from/to Q1.31 format */
#define Q31_FACTOR              (0x80000000)
#define Q31_1                   (Q31_FACTOR)
#define FLOAT_TO_Q31(input) 	((uint32_t) ((input) * ((float) Q31_FACTOR)))
#define Q31_TO_FLOAT(input)		((float) (((float) (input)) / ((float) Q31_FACTOR)))

#if CORDIC_SIN_COS
/**
 * @brief Calculates sine and cosine of a floating-point number simultaneously.
 *
 * @param input Input floating-point value in radians.
 * @param sin_val Pointer to store the sine result in the range [-1,1]. Can be NULL if not desired.
 * @param cos_val Pointer to store the cosine result in the range [-1,1]. Can be NULL if not desired.
 * 
 * @note The function automatically handles the input scaling to q1.31.
 * CORDIC engine requires that angles are divided by PI in order
 * to represent them efficiently in fixed point format. */
 */
void cordic_sin_cos(const float input, float *sin_val, float *cos_val)
{
    LL_CORDIC_Config(CORDIC,
		LL_CORDIC_FUNCTION_SINE,        /* Sine function */
		LL_CORDIC_PRECISION_6CYCLES,    /* Max precision for q1.31 sine */
		LL_CORDIC_SCALE_0,              /* No scale */
		LL_CORDIC_NBWRITE_2,            /* Two input data: angle_rad and modulus */
		LL_CORDIC_NBREAD_2,             /* Two output data: sine, then cosine */
        LL_CORDIC_INSIZE_32BITS,        /* q1.31 format for input data */
        LL_CORDIC_OUTSIZE_32BITS);      /* q1.31 format for output data */
    
    q31_t in_q31 = FLOAT_TO_Q31(input / PI);
    
    LL_CORDIC_WriteData(CORDIC, in_q31); /* Angle */
    LL_CORDIC_WriteData(CORDIC, Q31_1); /* Modulus of 1 */

    q31_t sin_q31 = LL_CORDIC_ReadData(CORDIC); /* Sine */
    q31_t cos_q31 = LL_CORDIC_ReadData(CORDIC); /* Cosine */

    if (sin_val != NULL) {
        *sin_val = Q31_TO_FLOAT(sin_q31);
    }
    if (cos_val != NULL) {
        *cos_val = Q31_TO_FLOAT(cos_q31);
    }
}
#endif /* CORDIC_SIN_COS */

#if CORDIC_PHASE_MODULUS
/**
 * @brief Calculates phase and modulus of a vector V[x y] simultaneously.
 *
 * @param x The magnitude of the vector in the direction of the x axis.
 * @param y The magnitude of the vector in the direction of the y axis.
 * @param phase Pointer to store the phase angle of the vector in radians. Can be NULL if not desired.
 * @param modulus Pointer to store the modulus of the vector in the range [0,1]. Can be NULL if not desired.
 * 
 * @note If |x| > 1 or |y| > 1, the function will automatically apply the scaling in software to adapt to the q1.31 range.
 * @note CORDIC phase result must be multiplied by π to obtain the angle in radians. This multiplication is handled by the function.
 * 
 */
void cordic_phase_modulus(const float x, const float y, float *phase, float *modulus)
{
    LL_CORDIC_Config(CORDIC,
		LL_CORDIC_FUNCTION_PHASE,       /* Phase function */
		LL_CORDIC_PRECISION_6CYCLES,    /* Max precision for q1.31 phase */
		LL_CORDIC_SCALE_0,              /* No scale */
		LL_CORDIC_NBWRITE_2,            /* Two input data: x and y */
		LL_CORDIC_NBREAD_2,             /* Two output data: phase, then modulus */
        LL_CORDIC_INSIZE_32BITS,        /* q1.31 format for input data */
        LL_CORDIC_OUTSIZE_32BITS);      /* q1.31 format for output data */
    
    unsigned x_scalar = 1;
    if(x > 1.0)
    {
        x_scalar += (unsigned) x;
    }
    else if(x < -1.0)
    {
        x_scalar += (unsigned) -x;
    }

    unsigned y_scalar = 1;
    if(y > 1.0)
    {
        y_scalar += (unsigned) y;
    }
    else if(y < -1.0)
    {
        y_scalar += (unsigned) -y;
    }

    unsigned scalar = (x_scalar >= y_scalar)?x_scalar:y_scalar;

    q31_t x_q31 = FLOAT_TO_Q31(x/scalar);
    q31_t y_q31 = FLOAT_TO_Q31(y/scalar);

    LL_CORDIC_WriteData(CORDIC, x_q31); /* x magnitude scaled  */
    LL_CORDIC_WriteData(CORDIC, y_q31); /* y magnitude scaled */

    q31_t phase_q31 = LL_CORDIC_ReadData(CORDIC); /* Phase in radian / PI */
    q31_t modulus_q31 = LL_CORDIC_ReadData(CORDIC); /* Modulus */

    if (phase != NULL) {
        *phase = Q31_TO_FLOAT(phase_q31) * PI;
    }
    if (modulus != NULL) {
        *modulus = Q31_TO_FLOAT(modulus_q31) * scalar;
    }
}
#endif /* CORDIC_PHASE_MODULUS */

static DEVICE_API(cordic, cordic_stm32_api) = {
    #ifdef CORDIC_SIN_COS
	.cordic_sin_cos = cordic_sin_cos,
    #endif /* CORDIC_SIN_COS */

    #ifdef CORDIC_PHASE_MODULUS
	.cordic_phase_modulus = cordic_phase_modulus,
    #endif /* CORDIC_PHASE_MODULUS */
};

