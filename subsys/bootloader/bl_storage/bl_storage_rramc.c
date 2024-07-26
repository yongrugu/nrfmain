#include <nrfx_rramc.h>

int set_monotonic_counter(uint16_t counter_desc, uint32_t new_counter)
{
	const uint32_t *next_counter_addr;
	uint32_t current_cnt_value;
	int err;

	err = get_counter(counter_desc, &current_cnt_value, &next_counter_addr);
	if (err != 0) {
		return err;
	}

	if (new_counter <= current_cnt_value) {
		/* Counter value must increase. */
		return -EINVAL;
	}

	if (next_counter_addr == NULL) {
		/* No more room. */
		return -ENOMEM;
	}

	nrfx_rramc_otp_word_write(index_from_address((uint32_t)next_counter_addr), ~new_counter);
	return 0;
}

/** Function for getting the current value and the first free slot.
 *
 * @param[in]   counter_desc  Counter description
 * @param[out]  counter_value The current value of the requested counter.
 * @param[out]  free_slot     Pointer to the first free slot. Can be NULL.
 *
 * @retval 0        Success
 * @retval -EINVAL  Cannot find counters with description @p counter_desc or the pointer to
 *                  @p counter_value is NULL.
 */
static int get_counter(uint16_t counter_desc, uint32_t *counter_value, const uint32_t **free_slot)
{
	uint32_t highest_counter = 0;
	const uint32_t *slots;
	const uint32_t *addr = NULL;

	const struct monotonic_counter *counter_obj = get_counter_struct(counter_desc);
	uint16_t num_counter_slots;

	if (counter_obj == NULL || counter_value == NULL) {
		return -EINVAL;
	}

	slots = counter_obj->counter_slots;
	num_counter_slots = bl_storage_otp_halfword_read((uint32_t)&counter_obj->num_counter_slots);

	for (uint32_t i = 0; i < num_counter_slots; i++) {
		uint32_t counter = ~nrfx_rramc_otp_word_read(index_from_address((uint32_t)&slots[i]));

		if (counter == 0) {
			addr = &slots[i];
			break;
		}
		if (highest_counter < counter) {
			highest_counter = counter;
		}
	}

	if (free_slot != NULL) {
		*free_slot = addr;
	}

	*counter_value = highest_counter;
	return 0;
}

int get_monotonic_counter(uint16_t counter_desc, uint32_t *counter_value)
{
	return get_counter(counter_desc, counter_value, NULL);
}
