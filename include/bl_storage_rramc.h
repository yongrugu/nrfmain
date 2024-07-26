/*
 * Copyright (c) 2024 Nordic Semiconductor ASA *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BL_STORAGE_RRAMC_H_
#define BL_STORAGE_RRAMC_H_

#include <nrfx_rramc.h>

#ifdef __cplusplus
extern "C" {
#endif

/** This library implements monotonic counters where each time the counter
 *  is increased, a new slot is written.
 *  This way, the counter can be updated without erase. This is, among other things,
 *  necessary so the counter can be used in the OTP section of the UICR
 *  (available on e.g. nRF91 and nRF53).
 */
struct monotonic_counter {
	/* Counter description. What the counter is used for. See
	 * BL_MONOTONIC_COUNTERS_DESC_x.
	 */
	uint32_t description;
	/* Number of entries in 'counter_slots' list. */
	uint32_t num_counter_slots;
	uint32_t counter_slots[1];
};

/** Storage for the PRoT Security Lifecycle state, that consists of 4 states:
 *  - Device assembly and test
 *  - PRoT Provisioning
 *  - Secured
 *  - Decommissioned
 *  These states are transitioned top down during the life time of a device.
 *  The Device assembly and test state is implicitly defined by checking if
 *  the provisioning state wasn't entered yet.
 *  This works as ASSEMBLY implies the OTP to be erased.
 *
 * Current RRAMC controller only supports word writes in OTP
 */
struct life_cycle_state_data {
	uint32_t provisioning;
	uint32_t secure;
	uint32_t decommissioned;
};

/** @defgroup bl_storage Bootloader storage (protected data).
 * @{
 */

/**
 * @brief Get the current HW monotonic counter.
 *
 * @param[in]  counter_desc  Counter description.
 * @param[out] counter_value The value of the current counter.
 *
 * @retval 0        Success
 * @retval -EINVAL  Cannot find counters with description @p counter_desc or the pointer to
 *                  @p counter_value is NULL.
 */
int get_monotonic_counter(uint16_t counter_desc, uint32_t *counter_value);

/**
 * @brief Set the current HW monotonic counter.
 *
 * @note FYI for users looking at the values directly in flash:
 *       Values are stored with their bits flipped. This is to squeeze one more
 *       value out of the counter.
 *
 * @param[in]  counter_desc Counter description.
 * @param[in]  new_counter  The new counter value. Must be larger than the
 *                          current value.
 *
 * @retval 0        The counter was updated successfully.
 * @retval -EINVAL  @p new_counter is invalid (must be larger than current
 *                  counter, and cannot be 0xFFFF).
 * @retval -ENOMEM  There are no more free counter slots (see
 *                  @kconfig{CONFIG_SB_NUM_VER_COUNTER_SLOTS}).
 */
#ifdef CONFIG_NRFX_NVMC
int set_monotonic_counter(uint16_t counter_desc, uint16_t new_counter);
#else
int set_monotonic_counter(uint16_t counter_desc, uint32_t new_counter);
#endif

static inline uint32_t bl_storage_word_read(uint32_t address)
{
	return nrfx_rramc_word_read(address);
}

static inline uint32_t bl_storage_word_write(uint32_t address, uint32_t value)
{
	nrfx_rramc_word_write(address, value);
	return 0;
}

static inline uint32_t index_from_address(uint32_t address){
	return ((address - (uint32_t)BL_STORAGE)/sizeof(uint32_t));
}

static inline uint16_t bl_storage_otp_halfword_read(uint32_t address){
	uint32_t word;
	uint16_t halfword;

	word = nrfx_rramc_otp_word_read(index_from_address(address));

	if (!(address & 0x3)) {
		halfword = (uint16_t)(word & 0x0000FFFF); // C truncates the upper bits
	} else {
		halfword = (uint16_t)(word >> 16); // Shift the upper half down
	}

	return halfword;
}

/**
 * @brief Update the life cycle state in OTP.
 *
 * @param[in] next_lcs Must be the same or the successor state of the current
 *                     one.
 *
 * @retval 0            Success.
 * @retval -EREADLCS    Reading the current state failed.
 * @retval -EINVALIDLCS Invalid next state.
 */
NRFX_STATIC_INLINE int update_life_cycle_state(enum lcs next_lcs)
{
	int err;
	enum lcs current_lcs = 0;

	if (next_lcs == BL_STORAGE_LCS_UNKNOWN) {
		return -EINVALIDLCS;
	}

	err = read_life_cycle_state(&current_lcs);
	if (err != 0) {
		return err;
	}

	if (next_lcs < current_lcs) {
		/* Is is only possible to transition into a higher state */
		return -EINVALIDLCS;
	}

	if (next_lcs == current_lcs) {
		/* The same LCS is a valid argument, but nothing to do so return success */
		return 0;
	}

	/* As the device starts in ASSEMBLY, it is not possible to write it */
	if (current_lcs == BL_STORAGE_LCS_ASSEMBLY && next_lcs == BL_STORAGE_LCS_PROVISIONING) {
		bl_storage_word_write((uint32_t)&BL_STORAGE->lcs.provisioning, STATE_ENTERED);
		return 0;
	}

	if (current_lcs == BL_STORAGE_LCS_PROVISIONING && next_lcs == BL_STORAGE_LCS_SECURED) {
		bl_storage_word_write((uint32_t)&BL_STORAGE->lcs.secure, STATE_ENTERED);
		return 0;
	}

	if (current_lcs == BL_STORAGE_LCS_SECURED && next_lcs == BL_STORAGE_LCS_DECOMMISSIONED) {
		bl_storage_word_write((uint32_t)&BL_STORAGE->lcs.decommissioned, STATE_ENTERED);
		return 0;
	}

	/* This will be the case if any invalid transition is tried */
	return -EINVALIDLCS;
}

  /** @} */

#ifdef __cplusplus
}
#endif

#endif /* BL_STORAGE_RRAMC_H_ */
