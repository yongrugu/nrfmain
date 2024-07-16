Solution areas
##############

Bluetooth technology provides full stack support for a wide and expanding range of use cases.

Audio streaming
***************

|NCS| supports the Bluetooth LE Audio standard, including Auracast broadcast audio.
See :ref:`nrf53_audio_app` to get started.

Note that Nordic Semiconductor SoCs do not support Bluetooth Classic Audio, which is based on the
Bluetooth BR/EDR radio.

Data transfer
*************

Data transfer is the main use case area for Bluetooth LE and covers use cases like wearables, PC peripherals, and
health related devices. Supported use cases for Bluetooth LE are implemented as Services and Profiles on top of the
Generic Attribute Profile (GATT).

Support in |NCS|:

* :ref:`nrf_desktop`: Reference design for a Bluetooth Human Interface Device (HID) solution. Includes desktop mouse, gaming
  mouse, keyboard, and dongle.
* :ref:`ble_samples` (|NCS|): Bluetooth samples, most of which are data transfer related. The following
  samples are recommended to get started with Bluetooth:

  * :ref:`peripheral_lbs`: This sample implements controls for buttons and LEDs on various hardware platforms
    from Nordic Semiconductor. The GATT Service is not a Bluetooth standard, so the sample is a good template for
    implementing vendor specific profiles and services.

  * :ref:`peripheral_uart` and :ref:`central_uart` implements a Nordic defined GATT service and profile that give a simple TX/RX
    generic data pipe, providing UART communication over Bluetooth LE.

* :ref:`bluetooth-samples` (Zephyr Project): The Zephyr Project offers additional Bluetooth samples. These samples are not
  guaranteed to work as part of |NCS|, but are helpful as starting point for a relvant use case.

Device networks
***************

Bluetooth Mesh is a technology that enables large-scale device networks in a many-to-many (m:m) topology, with focus
on reliability and security. The support in |NCS| includes the Mesh Profile, Mesh Models, Bluetooth Networked
Lighting Control (NLC) profiles, and related application examples. See the :ref:`ug_bt_mesh` main page for further details.

Periodic Advertising with Response (PAwR) allows a central device to communicate with 1000s of devices in a star
topology. The feature is support in SoftDevice Controller, and the related Encrypted Advertising (EAD) feature is
supported in Zephyr Host. The main use case, Electronic Shelf-Labels (ESL), is implemented as an application here
(experimental state): https://github.com/NordicPlayground/nrf-esl-bluetooth

Location services
*****************

In addition to the various forms of data transfer, Bluetooth technology includes methods for device positioning.

Presence
========

Bluetooth advertising is an easy way to indicate presence of a device to a nearby scanner. Such a solution is known as
Bluetooth beacons and works as a foundation for many solutions for tracking and item finding.

Nordic Semiconductor offers full implementation of two solutions for item finding:

* Google Find My Device Network: See the :ref:`fast_pair_locator_tag` sample for an implementation of a tag device.
* Apple Find My Network: See :ref:`integrations` page on how to apply for access.

Direction
=========

|NCS| includes 4 samples related to Bluetooth Angle-of-Arrival (AoA) technology, which allows to find the
direction from which a Bluetooth signal is transmitted:

* :ref:`direction_finding_connectionless_rx`
* :ref:`direction_finding_connectionless_tx`
* :ref:`bluetooth_direction_finding_central`
* :ref:`direction_finding_peripheral`

Distance
========

Channel sounding is an upcoming feature in the Bluetooth specification that allows distance measurement between two devices,
based on round-trip timing (RTT) and phase-based ranging (PBR).
