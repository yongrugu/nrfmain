#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic

import hid
import struct
import time
import logging
from enum import IntEnum
import codecs

REPORT_ID = 6
REPORT_SIZE = 30
EVENT_DATA_LEN_MAX = REPORT_SIZE - 6

MOD_FIELD_POS = 4
OPT_FIELD_POS = 0
OPT_FIELD_MAX_OPT_CNT = 0xf

OPT_MODULE_DESCR = 0x0

POLL_INTERVAL_DEFAULT = 0.02
POLL_RETRY_COUNT = 200

END_OF_TRANSFER_CHAR = '\n'


class ConfigStatus(IntEnum):
    PENDING            = 0
    GET_MAX_MOD_ID     = 1
    GET_HWID           = 2
    GET_BOARD_NAME     = 3
    SET                = 4
    FETCH              = 5
    SUCCESS            = 6
    TIMEOUT            = 7
    REJECT             = 8
    WRITE_ERROR        = 9
    DISCONNECTED_ERROR = 10
    FAULT              = 99

class NrfHidTransport():
    @staticmethod
    def _create_feature_report(recipient, event_id, status, event_data):
        assert isinstance(recipient, int)
        assert isinstance(event_id, int)
        assert isinstance(status, ConfigStatus)

        if event_data:
            assert isinstance(event_data, bytes)
            event_data_len = len(event_data)
        else:
            event_data_len = 0

        if status == ConfigStatus.SET:
            # No addtional checks
            pass
        elif status in (ConfigStatus.GET_MAX_MOD_ID,
                        ConfigStatus.GET_HWID,
                        ConfigStatus.GET_BOARD_NAME):
            assert event_id == 0
            assert event_data_len == 0
        elif status == ConfigStatus.FETCH:
            assert event_data_len == 0
        else:
            # Unsupported operation
            assert False

        report = struct.pack('<BHBBB', REPORT_ID, recipient, event_id, status,
                             event_data_len)

        if event_data:
            report += event_data

        assert len(report) <= REPORT_SIZE
        report += b'\0' * (REPORT_SIZE - len(report))

        return report

    @staticmethod
    def _parse_response(response_raw):
        data_field_len = len(response_raw) - struct.calcsize('<BHBBB')

        if data_field_len < 0:
            logging.error('Response too short')
            return None

        # Report ID is not included in the feature report from device
        fmt = '<BHBBB{}s'.format(data_field_len)

        (report_id, rcpt, event_id, status, data_len, data) = struct.unpack(fmt, response_raw)

        if report_id != REPORT_ID:
            logging.error('Improper report ID')
            return None

        if data_len > len(data):
            logging.error('Required data not present')
            return None

        if data_len == 0:
            event_data = None
        else:
            event_data = data[:data_len]

        return (rcpt, event_id, status, event_data)

    @staticmethod
    def exchange_feature_report(dev, recipient, event_id, status, event_data,
                                poll_interval=POLL_INTERVAL_DEFAULT):
        data = NrfHidTransport._create_feature_report(recipient, event_id, status, event_data)

        try:
            dev.send_feature_report(data)
        except Exception as e:
            logging.debug('Send feature report problem: {}'.format(e))
            return False, None

        for _ in range(POLL_RETRY_COUNT):
            time.sleep(poll_interval)

            try:
                response_raw = dev.get_feature_report(REPORT_ID, REPORT_SIZE)
            except Exception as e:
                logging.error('Get feature report problem: {}'.format(e))
                rsp_status = ConfigStatus.FAULT
                break

            rsp = NrfHidTransport._parse_response(response_raw)
            if rsp is None:
                logging.error('Parsing response failed')
                rsp_status = ConfigStatus.FAULT
                break

            (rsp_recipient, rsp_event_id, rsp_status, rsp_event_data) = rsp
            rsp_status = ConfigStatus(rsp_status)

            if rsp_status == ConfigStatus.PENDING:
                # Response was not ready
                continue

            if (rsp_recipient != recipient) or (rsp_event_id != event_id):
                logging.error('Response does not match the request:\n'
                              '\trequest: recipient {} event_id {}\n'
                              '\tresponse: recipient {}, event_id {}'.format(recipient,
                                                                             event_id,
                                                                             rsp_recipient,
                                                                             rsp_event_id))
                rsp_status = ConfigStatus.FAULT
            break

        fetched_data = None
        success = False

        if rsp_status == ConfigStatus.SUCCESS:
            success = True
            if rsp_event_data is not None:
                fetched_data = rsp_event_data
        else:
            logging.error('Error: {}'.format(rsp_status.name))

        return success, fetched_data


class NrfHidDevice():
    def __init__(self, name, vid, pid, dongle_pid, req_hwid=None):
        self.name = name
        self.vid = vid
        self.pid = pid
        self.dev_ptr = None
        self.dev_config = None
        self.board_name = None

        direct_devs = NrfHidDevice._open_devices(vid, pid)
        dongle_devs = []

        if dongle_pid is not None:
            dongle_devs = NrfHidDevice._open_devices(vid, dongle_pid)

        devs = direct_devs + dongle_devs

        for d in devs:
            if self.dev_ptr is None:
                board_name, hwid = NrfHidDevice._read_device_info(d, pid)

                # HW ID missmatch
                if (req_hwid is not None) and (req_hwid != hwid):
                    d.close()
                    continue

                if (board_name is not None) and (hwid is not None):
                    config = NrfHidDevice._discover_device_config(d, pid)
                else:
                    config = None

                if (config is not None) and (board_name is not None) and \
                   (hwid is not None):
                    self.dev_config = config
                    self.dev_ptr = d
                    self.board_name = board_name
                    self.hwid = hwid
                    print("Device board name is {} (HW ID: {})".format(board_name, hwid))
                else:
                    d.close()
            else:
                d.close()

    @staticmethod
    def _open_devices(vid, pid):
        devs = []
        try:
            devlist = hid.enumerate(vid=vid, pid=pid)
            for d in devlist:
                dev = hid.Device(path=d['path'])
                devs.append(dev)

        except hid.HIDException:
            pass
        except Exception as e:
            logging.error('Unknown exception: {}'.format(e))

        return devs

    @staticmethod
    def _read_max_mod_id(dev, recipient):
        success, fetched_data = NrfHidTransport.exchange_feature_report(dev,
                                                                        recipient,
                                                                        0,
                                                                        ConfigStatus.GET_MAX_MOD_ID,
                                                                        None)
        if not success or not fetched_data:
            return None

        max_mod_id = ord(fetched_data.decode('utf-8'))

        return max_mod_id

    @staticmethod
    def _fetch_next_option(dev, recipient, module_id):
        event_id = (module_id << MOD_FIELD_POS) | (OPT_MODULE_DESCR << OPT_FIELD_POS)

        success, fetched_data = NrfHidTransport.exchange_feature_report(dev, recipient,
                                                                        event_id, ConfigStatus.FETCH,
                                                                        None)
        if not success or not fetched_data:
            return False, None

        opt_name = fetched_data.decode('utf-8').replace(chr(0x00), '')

        return success, opt_name

    @staticmethod
    def _get_event_id(module_name, option_name, device_config):
        module_id = device_config[module_name]['id']
        option_id = device_config[module_name]['options'][option_name]['id']

        return (module_id << MOD_FIELD_POS) | (option_id << OPT_FIELD_POS)

    @staticmethod
    def _discover_module_config(dev, recipient, module_id):
        fetched_options = []
        fetch_idx = 0
        end_of_transfer_idx = None

        while True:
            success, opt = NrfHidDevice._fetch_next_option(dev, recipient,
                                                           module_id)
            if not success:
                return None, None

            if opt in fetched_options:
                break

            if opt[0] == END_OF_TRANSFER_CHAR:
                end_of_transfer_idx = len(fetched_options)

            fetched_options.append(opt)

            if fetch_idx > OPT_FIELD_MAX_OPT_CNT + 1:
                print("Improper module description")
                return None, None

            fetch_idx += 1

        if end_of_transfer_idx is None:
            print("Improper module description")
            return None, None

        fetched_options = fetched_options[end_of_transfer_idx:] + \
                          fetched_options[:end_of_transfer_idx]
        fetched_options = fetched_options[1:]

        module_name = fetched_options[0]
        fetched_options = fetched_options[1:]

        module_config = {}
        module_config['id'] = module_id
        module_config['options'] = {}

        # Option with index 0 is module description
        opt_idx = 1
        for o in fetched_options:
            module_config['options'][o] = {
                'id' : opt_idx,
            }
            opt_idx += 1

        return module_name, module_config

    @staticmethod
    def _discover_device_config(dev, recipient):
        device_config = {}

        max_mod_id = NrfHidDevice._read_max_mod_id(dev, recipient)
        if max_mod_id is None:
            return None

        for i in range(0, max_mod_id + 1):
            module_name, module_config = NrfHidDevice._discover_module_config(dev, recipient, i)
            if (module_name is None) or (module_config is None):
                return None

            device_config[module_name] = module_config

        return device_config

    @staticmethod
    def _read_device_info(dev, recipient):
        success, fetched_data = NrfHidTransport.exchange_feature_report(dev,
                                                                        recipient,
                                                                        0,
                                                                        ConfigStatus.GET_BOARD_NAME,
                                                                        None)
        if success:
            board_name = fetched_data.decode('utf-8').replace(chr(0x00), '')
        else:
            return None, None

        success, fetched_data = NrfHidTransport.exchange_feature_report(dev,
                                                                        recipient,
                                                                        0,
                                                                        ConfigStatus.GET_HWID,
                                                                        None)
        if success:
            hwid = codecs.encode(fetched_data, 'hex')
            hwid = hwid.decode('utf-8')
        else:
            return None, None

        return board_name, hwid

    def _config_operation(self, module_name, option_name, is_get, value, poll_interval):
        if not self.initialized():
            print("Device not found")

            if is_get:
                return False, None
            else:
                return False

        try:
            event_id = NrfHidDevice._get_event_id(module_name, option_name, self.dev_config)
        except KeyError:
            print("No module: {} or option: {}".format(module_name, option_name))
            if is_get:
                return False, None
            else:
                return False

        if is_get:
            status = ConfigStatus.FETCH
        else:
            status = ConfigStatus.SET

        success, fetched_data = NrfHidTransport.exchange_feature_report(self.dev_ptr, self.pid,
                                                                        event_id, status,
                                                                        value, poll_interval)
        if is_get:
            return success, fetched_data
        else:
            return success

    def close_device(self):
        self.dev_ptr.close()
        self.dev_ptr = None
        self.dev_config = None

    def initialized(self):
        if (self.dev_ptr is None) or (self.dev_config is None):
            return False
        else:
            return True

    def get_board_name(self):
        return self.board_name

    def get_device_config(self):
        if not self.initialized():
            print("Device is not initialized")
            return None

        res = {}

        for module in self.dev_config:
            res[module] = list(self.dev_config[module]['options'].keys())

        return res

    def config_get(self, module_name, option_name, poll_interval=POLL_INTERVAL_DEFAULT):
        return self._config_operation(module_name, option_name, True, None, poll_interval)

    def config_set(self, module_name, option_name, value, poll_interval=POLL_INTERVAL_DEFAULT):
        return self._config_operation(module_name, option_name, False, value, poll_interval)
