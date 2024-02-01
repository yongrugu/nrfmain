/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef __ENCRYPTED_STORAGE_AUTH_CRYPT_NONCE_H_
#define __ENCRYPTED_STORAGE_AUTH_CRYPT_NONCE_H_

#include <psa/error.h>
#include <psa/storage_common.h>

psa_status_t encrypted_storage_get_nonce(uint8_t *nonce, size_t nonce_len);

#endif /* __ENCRYPTED_STORAGE_AUTH_CRYPT_NONCE_H_ */
