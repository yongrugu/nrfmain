/* Copyright (c) 2019, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Use in source and binary forms, redistribution in binary form only, with
 * or without modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 2. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 3. This software, with or without modification, must only be used with a Nordic
 *    Semiconductor ASA integrated circuit.
 *
 * 4. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Purpose: radio module interface intended for internal usage on library
 *          level
 */

#ifndef PTT_RF_H__
#define PTT_RF_H__

#include <stdint.h>

#include "ptt_config.h"
#include "ctrl/ptt_events.h"
#include "ptt_errors.h"
#include "ptt_types.h"

#define PTT_CHANNEL_MIN           (11u)
#define PTT_CHANNEL_MAX           (26u)

#define PTT_PANID_ADDRESS_SIZE    (2u)
#define PTT_SHORT_ADDRESS_SIZE    (2u)
#define PTT_EXTENDED_ADDRESS_SIZE (8u)

/* received packets statistic */
typedef struct
{
    uint32_t total_pkts; /**< total received packets */
    uint32_t total_lqi;  /**< sum of lqi of received packets */
    uint32_t total_rssi; /**< sum of rssi of received packets */
} ptt_rf_stat_t;

/** payload for `custom ltx` command */
typedef struct
{
    uint8_t arr[PTT_CUSTOM_LTX_PAYLOAD_MAX_SIZE]; /**< raw PHY packet */
    uint8_t len;                                  /**< length of used part of arr */
} ptt_ltx_payload_t;

/* information that came with the packet */
typedef struct
{
    uint8_t lqi;
    int8_t  rssi;
} ptt_rf_packet_info_t;

/** @brief RF module context initialization
 *
 *  @param none
 *
 *  @return none
 */
void ptt_rf_init(void);

/** @brief RF module context unitialization
 *
 *  @param none
 *
 *  @return none
 */
void ptt_rf_uninit(void);

/** @brief RF module reset to default
 *
 *  @param none
 *
 *  @return none
 */
void ptt_rf_reset(void);

/** @brief Send a packet to a transmitter
 *
 *  @param evt_id - event id locking this transmission
 *  @param p_pkt - data to be transmitted
 *  @param len - length of the data
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_send_packet(ptt_evt_id_t    evt_id,
                             const uint8_t * p_pkt,
                             ptt_pkt_len_t   len);

/** @brief Set channel mask to radio driver
 *
 *  @param evt_id - event id locking setting mask
 *  @param mask - channel bit mask, if more than one bit enabled, least significant will be used
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_set_channel_mask(ptt_evt_id_t evt_id, uint32_t mask);

/** @brief Set channel to radio driver
 *
 *  @param evt_id - event id locking setting channel
 *  @param channel - channel number (11-26)
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_set_channel(ptt_evt_id_t evt_id, uint8_t channel);

/** @brief Set short address to radio driver
 *
 *  @param evt_id - event id locking setting address
 *  @param p_short_address  Pointer to the short address (2 bytes, little-endian)
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_set_short_address(ptt_evt_id_t evt_id, const uint8_t * p_short_addr);

/** @brief Set extended address to radio driver
 *
 *  @param evt_id - event id locking setting address
 *  @param p_extended_address  Pointer to the extended address (8 bytes, little-endian)
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_set_extended_address(ptt_evt_id_t evt_id, const uint8_t * p_extended_addr);

/** @brief Set pan id to radio driver
 *
 *  @param evt_id - event id locking setting PAN id
 *  @param  p_pan_id  Pointer to the PAN ID (2 bytes, little-endian)
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_set_pan_id(ptt_evt_id_t evt_id, const uint8_t * p_pan_id);

/** @brief Returns number of current channel
 *
 *  @param none
 *
 *  @return uint8_t - current channel number
 */
uint8_t ptt_rf_get_channel(void);

/** @brief Set power to radio driver
 *
 *  @param evt_id - event id locking setting power
 *  @param power - power in dbm
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_set_power(ptt_evt_id_t evt_id, int8_t power);

/** @brief Returns current power
 *
 *  @param none
 *
 *  @return int8_t - current power
 */
int8_t ptt_rf_get_power(void);

/** @brief Set antenna to radio driver
 *
 *  @param evt_id - event id locking setting antenna
 *  @param antenna - is value in range [0-255]
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_set_antenna(ptt_evt_id_t evt_id, uint8_t antenna);

/** @brief Returns current antenna
 *
 *  @param none
 *
 *  @return uint8_t - current antenna
 */
uint8_t ptt_rf_get_antenna(void);

/** @brief Calls radio driver to verify if the channel is clear
 *
 *  @param evt_id - event id locking radio module
 *  @param mode - CCA mode
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_cca(ptt_evt_id_t evt_id, uint8_t mode);

/** @brief Calls radio driver to detect the maximum energy for a given time
 *
 *  @param evt_id - event id locking radio module
 *  @param time_us - duration of energy detection procedure.
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_ed(ptt_evt_id_t evt_id, uint32_t time_us);

/** @brief Calls radio driver to begin the RSSI measurement
 *
 *  @param evt_id - event id locking static module
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_rssi_measure_begin(ptt_evt_id_t evt_id);

/** @brief Calls radio driver to get the result of the last RSSI measurement
 *
 *  @param[IN]  evt_id - event id locking static module
 *  @param[OUT] rssi   - RSSI measurement result in dBm, unchanged if error code returned
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_rssi_last_get(ptt_evt_id_t evt_id, ptt_rssi_t * rssi);

/** @brief Convert channel bitmask into a channel number
 *
 *  @param mask - channel bit mask, if more than one bit enabled, least significant will be used
 *
 *  @return uint8_t - channel number or 0, if there is no valid channels inside mask
 */
uint8_t ptt_rf_convert_channel_mask_to_num(uint32_t mask);

/** @brief Clear statistic and enable statistic gathering in RF module
 *
 *  @param evt_id - event id locking statistic control
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_start_statistic(ptt_evt_id_t evt_id);

/** @brief Disable statistic gathering in RF module
 *
 *  @param evt_id - event id locking statistic control
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_end_statistic(ptt_evt_id_t evt_id);

/** @brief Get gathered statistic from RF module
 *
 *  @param none
 *
 *  @return ptt_rf_stat_t - structure with gathered statistic
 */
ptt_rf_stat_t ptt_rf_get_stat_report(void);

/** @brief Start modulated stream transmission
 *
 *  @param evt_id - event id locking this transmission
 *  @param p_pkt  - payload to modulate carrier with
 *  @param len    - length of payload to modulate carrier with
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_start_modulated_stream(ptt_evt_id_t    evt_id,
                                        const uint8_t * p_pkt,
                                        ptt_pkt_len_t   len);

/** @brief Stop modulated stream transmission
 *
 *  @param evt_id - event id locking this transmission
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_stop_modulated_stream(ptt_evt_id_t evt_id);

/** @brief Start continuous carrier transmission
 *
 *  @param evt_id - event id locking this transmission
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_start_continuous_carrier(ptt_evt_id_t evt_id);

/** @brief Stop continuous carrier transmission
 *
 *  @param evt_id - event id locking this transmission
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_stop_continuous_carrier(ptt_evt_id_t evt_id);

/** @brief Getter for payload of custom ltx command
 *
 *  @param none
 *
 *  @return ptt_ltx_payload_t * pointer to payload
 */
ptt_ltx_payload_t * ptt_rf_get_custom_ltx_payload();

/** @brief Change radio state to receiving
 *
 *  @param evt_id - event id locking this operation
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_receive(void);

/** @brief Change radio state to sleeping
 *
 *  @param evt_id - event id locking this operation
 *
 *  @return ptt_ret_t - PTT_RET_SUCCESS or error
 */
ptt_ret_t ptt_rf_sleep(void);

#endif /* PTT_RF_H__ */

