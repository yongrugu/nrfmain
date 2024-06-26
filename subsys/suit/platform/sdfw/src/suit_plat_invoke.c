/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <suit_platform_internal.h>
#include <stdbool.h>
#include <suit_platform.h>
#include <suit_plat_decode_util.h>
#include <suit_cpu_run.h>
#include <sdfw/arbiter.h>
#include <sdfw/sdfw_util.h>

LOG_MODULE_REGISTER(suit_plat_invoke, CONFIG_SUIT_LOG_LEVEL);

static bool is_address_range_executable(uint8_t cpu_id, uint8_t *address, size_t size)
{
	nrf_owner_t owner =  0;
	
	if (cpu_id == NRF_PROCESSOR_SYSCTRL)
	{
		/* arbiter_mem_access_check would always fail for sysctrl, as it only
		 * handles local domains. */
		return true;
	}
	
	if (processor_to_owner_id(cpu_id, &owner) != 0)
	{
		return false;
	}

	struct arbiter_mem_params_access mem_params = {
		.allowed_types = ARBITER_MEM_TYPE(RESERVED, FIXED),
		.access = {
				.owner = owner,
				.permissions = ARBITER_MEM_PERM(READ, EXEC, SECURE),
				.address = (uintptr_t)address,
				.size = size,
			},
	};

	return (arbiter_mem_access_check(&mem_params) == ARBITER_STATUS_OK);
}

int suit_plat_check_invoke(suit_component_t image_handle, struct zcbor_string *invoke_args)
{
	struct zcbor_string *component_id;
	suit_component_type_t component_type = SUIT_COMPONENT_TYPE_UNSUPPORTED;
	intptr_t run_address;
	uint8_t cpu_id;
	size_t size;

	if (suit_plat_component_id_get(image_handle, &component_id) != SUIT_SUCCESS) {
		LOG_ERR("suit_plat_component_id_get failed");
		return SUIT_ERR_UNSUPPORTED_COMPONENT_ID;
	}

	if (suit_plat_decode_component_id(component_id, &cpu_id, &run_address, &size)
	    != SUIT_PLAT_SUCCESS) {
		LOG_ERR("suit_plat_decode_component_id failed");
		return SUIT_ERR_UNSUPPORTED_COMPONENT_ID;
	}

	if (suit_plat_decode_component_type(component_id, &component_type) != SUIT_PLAT_SUCCESS) {
		LOG_ERR("suit_plat_decode_component_type failed");
		return SUIT_ERR_UNSUPPORTED_COMPONENT_ID;
	}

	if (!is_address_range_executable(cpu_id, (uint8_t*) run_address, size)) {
		LOG_ERR("Component memory is not executable by CPU");
		return SUIT_ERR_UNSUPPORTED_COMPONENT_ID;
	}

	/* Check if component type supports invocation */
	switch (component_type) {
	case SUIT_COMPONENT_TYPE_MEM:
		/* memory-mapped */
		return SUIT_SUCCESS;
	default:
		LOG_ERR("Unsupported component type");
		break;
	}

	return SUIT_ERR_UNSUPPORTED_COMPONENT_ID;
}

int suit_plat_invoke(suit_component_t image_handle, struct zcbor_string *invoke_args)
{
	struct zcbor_string *component_id;
	suit_component_type_t component_type = SUIT_COMPONENT_TYPE_UNSUPPORTED;
	intptr_t run_address;
	uint8_t cpu_id;
	size_t size;

	if (suit_plat_component_id_get(image_handle, &component_id) != SUIT_SUCCESS) {
		LOG_ERR("suit_plat_component_id_get failed");
		return SUIT_ERR_UNSUPPORTED_COMPONENT_ID;
	}

	if (suit_plat_decode_component_id(component_id, &cpu_id, &run_address, &size)
	    != SUIT_PLAT_SUCCESS) {
		LOG_ERR("suit_plat_decode_component_id failed");
		return SUIT_ERR_UNSUPPORTED_COMPONENT_ID;
	}

	if (suit_plat_decode_component_type(component_id, &component_type) != SUIT_PLAT_SUCCESS) {
		LOG_ERR("suit_plat_decode_component_type failed");
		return SUIT_ERR_UNSUPPORTED_COMPONENT_ID;
	}

	if (!is_address_range_executable(cpu_id, (uint8_t*) run_address, size)) {
		LOG_ERR("Component memory is not executable by CPU");
		return SUIT_ERR_UNSUPPORTED_COMPONENT_ID;
	}

	/* Check if component type supports invocation */
	switch (component_type) {
	case SUIT_COMPONENT_TYPE_MEM:
		/* memory-mapped */
		return suit_plat_cpu_run(cpu_id, run_address);
	default:
		LOG_ERR("Unsupported component type");
		break;
	}

	return SUIT_ERR_UNSUPPORTED_COMPONENT_ID;
}
