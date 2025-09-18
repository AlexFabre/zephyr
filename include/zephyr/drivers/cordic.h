/*
 * Copyright (c) 2024 Brill Power Ltd.
 * Copyright (c) 2025 Renesas Electronics Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief CRC public API header file.
 * @ingroup crc_interface
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CRC_H
#define ZEPHYR_INCLUDE_DRIVERS_CRC_H

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Interfaces for Cyclic Redundancy Check (CRC) devices.
 * @defgroup crc_interface CRC driver APIs
 * @ingroup io_interfaces
 * @{
 */

/** @} */

/** @endcond */

/**
 * @brief CRC state enumeration
 */

enum cordic_function {
    /*!< Cosine */               
    CORDIC_FUNCTION_COSINE,
    /*!< Sine */                 
    CORDIC_FUNCTION_SINE,
    /*!< Phase */                
    CORDIC_FUNCTION_PHASE,
    /*!< Modulus */              
    CORDIC_FUNCTION_MODULUS,
    /*!< Arctangent */           
    CORDIC_FUNCTION_ARCTANGENT,
    /*!< Hyperbolic Cosine */    
    CORDIC_FUNCTION_HCOSINE,
    /*!< Hyperbolic Sine */      
    CORDIC_FUNCTION_HSINE,
    /*!< Hyperbolic Arctangent */
    CORDIC_FUNCTION_HARCTANGENT,
    /*!< Natural Logarithm */    
    CORDIC_FUNCTION_NATURALLOG,
    /*!< Square Root */          
    CORDIC_FUNCTION_SQUAREROOT,
};

/**
 * @brief Callback API upon CRC calculation begin
 * See @a crc_begin() for argument description
 */
typedef void (*cordic_get_capabilities)(const struct device *dev, struct crc_ctx *ctx);

/**
 * @brief Callback API upon CRC calculation stream update
 * See @a crc_update() for argument description
 */
typedef int (*cordic_configure)(const struct device *dev, struct crc_ctx *ctx, const void *buffer,
			      size_t bufsize);

/**
 * @brief Callback API upon CRC calculation finish
 * See @a crc_finish() for argument description
 */
typedef int (*cordic_api_compute)(const struct device *dev, struct crc_ctx *ctx);

__subsystem struct crc_driver_api {
	crc_api_begin begin;
	crc_api_update update;
	crc_api_finish finish;
};

/**
 * @brief  Configure CRC unit for calculation
 *
 * @param dev Pointer to the device structure
 * @param ctx Pointer to the CRC context structure
 *
 * @retval 0 if successful
 * @retval -ENOSYS if function is not implemented
 * @retval errno code on failure
 */
__syscall int crc_begin(const struct device *dev, struct crc_ctx *ctx);

static inline int z_impl_crc_begin(const struct device *dev, struct crc_ctx *ctx)
{
	const struct crc_driver_api *api = (const struct crc_driver_api *)dev->api;

	if (api->begin == NULL) {
		return -ENOSYS;
	}

	return api->begin(dev, ctx);
}

/**
 * @brief Perform CRC calculation on the provided data buffer and retrieve result
 *
 * @param dev Pointer to the device structure
 * @param ctx Pointer to the CRC context structure
 * @param buffer Pointer to input data buffer
 * @param bufsize Number of bytes in *buffer
 *
 * @retval 0 if successful
 * @retval -ENOSYS if function is not implemented
 * @retval errno code on failure
 */
__syscall int crc_update(const struct device *dev, struct crc_ctx *ctx, const void *buffer,
			 size_t bufsize);

static inline int z_impl_crc_update(const struct device *dev, struct crc_ctx *ctx,
				    const void *buffer, size_t bufsize)
{
	const struct crc_driver_api *api = (const struct crc_driver_api *)dev->api;

	if (api->update == NULL) {
		return -ENOSYS;
	}

	return api->update(dev, ctx, buffer, bufsize);
}

/**
 * @brief  Finalize CRC calculation
 *
 * @param dev Pointer to the device structure
 * @param ctx Pointer to the CRC context structure
 *
 * @retval 0 if successful
 * @retval -ENOSYS if function is not implemented
 * @retval errno code on failure
 */
__syscall int crc_finish(const struct device *dev, struct crc_ctx *ctx);

static inline int z_impl_crc_finish(const struct device *dev, struct crc_ctx *ctx)
{
	const struct crc_driver_api *api = (const struct crc_driver_api *)dev->api;

	if (api->finish == NULL) {
		return -ENOSYS;
	}

	return api->finish(dev, ctx);
}

/**
 * @brief Verify CRC result
 *
 * @param ctx Pointer to the CRC context structure
 * @param expected Expected CRC result
 *
 * @retval 0 if successful
 * @retval -EBUSY if CRC calculation is not completed
 * @retval -EPERM if CRC verification failed
 */
static inline int crc_verify(struct crc_ctx *ctx, crc_result_t expected)
{
	if (ctx == NULL) {
		return -EINVAL;
	}

	if (ctx->state == CRC_STATE_IN_PROGRESS) {
		return -EBUSY;
	}

	if (expected != ctx->result) {
		return -EPERM;
	}

	return 0;
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <zephyr/syscalls/crc.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_CRC_H */
