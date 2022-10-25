/*
 * Copyright (c) 2021 - 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#if defined(TFM_PARTITION_CRYPTO)
#include "common.h"
#include <nrf_cc3xx_platform.h>
#include <nrf_cc3xx_platform_ctr_drbg.h>
#endif

#if !defined(PLATFORM_DEFAULT_PROVISIONING) && !defined(TFM_DUMMY_PROVISIONING)
#include "tfm_attest_hal.h"
#endif /* !defined(PLATFORM_DEFAULT_PROVISIONING) && !defined(TFM_DUMMY_PROVISIONING) */

#include "tfm_hal_platform.h"
#include "tfm_hal_platform_common.h"
#include "cmsis.h"
#include "uart_stdout.h"
#include "tfm_spm_log.h"
#include "hw_unique_key.h"

#if defined(TFM_PARTITION_CRYPTO)
static enum tfm_hal_status_t crypto_platform_init(void)
{
	int err;

	/* Initialize the nrf_cc3xx runtime */
#if defined(TFM_CRYPTO_RNG_MODULE_DISABLED)
	err = nrf_cc3xx_platform_init_no_rng();
#else
#if defined(PSA_WANT_ALG_CTR_DRBG)
	err = nrf_cc3xx_platform_init();
#elif defined (PSA_WANT_ALG_HMAC_DRBG)
	err = nrf_cc3xx_platform_init_hmac_drbg();
#else
	#error "Please enable either PSA_WANT_ALG_CTR_DRBG or PSA_WANT_ALG_HMAC_DRBG"
#endif
#endif

	if (err != NRF_CC3XX_PLATFORM_SUCCESS) {
		return TFM_HAL_ERROR_BAD_STATE;
	}

#if !defined(TFM_CRYPTO_KEY_DERIVATION_MODULE_DISABLED) && \
    !defined(PLATFORM_DEFAULT_CRYPTO_KEYS)
	if (!hw_unique_key_are_any_written()) {
		SPMLOG_INFMSG("Writing random Hardware Unique Keys to the KMU.\r\n");
		hw_unique_key_write_random();
		SPMLOG_INFMSG("Success\r\n");
	}
#endif

	return TFM_HAL_SUCCESS;
}
#endif /* defined(TFM_PARTITION_CRYPTO) */

/* To write into AIRCR register, 0x5FA value must be written to the VECTKEY field,
 * otherwise the processor ignores the write.
 */
#define AIRCR_VECTKEY_PERMIT_WRITE ((0x5FAUL << SCB_AIRCR_VECTKEY_Pos))

static void allow_nonsecure_reset(void)
{
    uint32_t reg_value = SCB->AIRCR;

    /* Clear SCB_AIRCR_VECTKEY value */
    reg_value &= ~(uint32_t)(SCB_AIRCR_VECTKEY_Msk);

    /* Clear SCB_AIRC_SYSRESETREQS value */
    reg_value &= ~(uint32_t)(SCB_AIRCR_SYSRESETREQS_Msk);

    /* Add VECTKEY value needed to write the register. */
    reg_value |= (uint32_t)(AIRCR_VECTKEY_PERMIT_WRITE);

    SCB->AIRCR = reg_value;
}

enum tfm_hal_status_t tfm_hal_platform_init(void)
{
	enum tfm_hal_status_t status;

	status = tfm_hal_platform_common_init();
	if (status != TFM_HAL_SUCCESS) {
		return status;
	}

#if defined(TFM_PARTITION_CRYPTO)
	status = crypto_platform_init();
	if (status != TFM_HAL_SUCCESS)
	{
		return status;
	}
#endif /* defined(TFM_PARTITION_CRYPTO) */

#if defined(NRF_ALLOW_NON_SECURE_RESET)
	allow_nonsecure_reset();
#endif

/* When NRF provisioning is enabled we can either be in lifecycle state provisioning or
 * secured, we don't support any other lifecycle state. This ensures that TF-M will not
 * continue booting when a non supported state is present.
 */
#if !defined(PLATFORM_DEFAULT_PROVISIONING) && !defined(TFM_DUMMY_PROVISIONING)
	enum tfm_security_lifecycle_t lcs = tfm_attest_hal_get_security_lifecycle();
	if(lcs != TFM_SLC_PSA_ROT_PROVISIONING && lcs != TFM_SLC_SECURED) {
		return TFM_HAL_ERROR_BAD_STATE;
	}
#endif /* !defined(PLATFORM_DEFAULT_PROVISIONING) && !defined(TFM_DUMMY_PROVISIONING) */

	return TFM_HAL_SUCCESS;
}
