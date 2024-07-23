.. _protocols_serialization_server_app:

Protocols serialization application
###################################

.. contents::
   :local:
   :depth: 2

The protocols serialization server runs full Bluetooth LE and OpenThread stacks and it exposes selected functions from these stacks over the UART.

Requirements
************

The sample supports the following development kits for testing the network status:

.. table-from-sample-yaml::

Overview
********

Protocols serialization application performs server part of the OpenThread and Bluetooth LE API calls serialization between two devices
that communicate with each other using the UART interface. The application uses nRF RPC library and CBOR encoding to serialize function calls.

Building and flashing application
*********************************

Complete the following steps to build and flash the server application into another nRF52840 DK:

#. Open a terminal and navigate to the nRF Connect SDK installation directory.

#. Navigate to the server application's directory:

   .. code-block:: console

      ~/ncs/nrf$ cd applications/protocols_serialization


#. Build the server application:

   .. code-block:: console

      ~/ncs/nrf/applications/protocols_serialization$ west build -b nrf52840dk/nrf52840 -S "openthread;ble;debug"


   Note that you can modify the list of enabled features, which by default includes **ble** support as well as **debug** logs.

#. Flash the server application into an nRF52840 DK:

   .. code-block:: console

      ~/ncs/nrf/applications/protocols_serialization$ west flash --erase

Testing
*******

For testing the application, use the protocol serialization client sample :ref:`nrf_rpc_protocols_serialization_client`.

.. _protocols_serialization_server_app_connection:

Connecting client and server
============================

The nRF52840 system-on-chip includes two UART peripherals: `uart0` and `uart1`. In the protocols serialization application, the `uart0` peripheral is used for shell and logging purposes, similarly to other applications and samples, while the `uart1` peripheral is used for sending OpenThread and Bluetooth LE remote procedure calls (RPCs).

The `uart1` peripheral is configured to use the following pins:

- **P1.1**: RX
- **P1.2**: TX
- **P1.3**: RTS (hardware flow control)
- **P1.4**: CTS (hardware flow control)

Therefore, in order to enable the communication between the client and the server devices, connect the two nRF 52840 DKs using jumper wires according to the following diagram:

.. figure:: /images/ps_nrf52_connections.png
   :alt: nRF52 connections

The nRF54l15 system-on-chip uses by default `uart21` for RPC and `uart20` is used for shell.

The `uart21` peripheral is configured to use the following pins:

- **P1.10**: RX
- **P1.11**: TX
- **P1.12**: RTS (hardware flow control)
- **P1.13**: CTS (hardware flow control)

.. figure:: /images/ps_nrf54l_connections.png
   :alt: nRF54l connections

.. _protocols_serialization_server_app_testing:

Testing BLE
===========

Complete the following steps to test Bluetooth LE API serialization:

#. Install `nRF Connect for Mobile` application on your phone:

   - IOS: https://apps.apple.com/pl/app/nrf-connect-for-mobile/id1054362403
   - Android: https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp&pcampaignid=web_share

#. Connect to the UART console of the client device using a terminal emulator of your choice.

#. Connect to the UART console of the server device using a terminal emulator of your choice.

#. Reboot **both** devices at the same time using the **IF BOOT/RESET** button on the DK.

#. Wait a few seconds until similar message on both UART consoles:

   .. code-block:: console

      [00:00:00.842,862] <inf> nrf_rpc_host: RPC client ready


   This indicates that the communication between the devices has been initialized properly.

#. Run the following command on the client's console to start Bluetooth LE advertising:

   .. code-block:: console

      uart:~$ bt init
      uart:~$ bt advertise on


#. On mobile phone, open `nRF Connect` app and scan for `Nordic_UART_Service` as in the image below:

   .. figure:: /images/ps_nrf_scan.png
   :alt: BLE scanning

#. Tap **CONNECT** button to connect to the client device.
   Observe the following message on the client's console:

   .. code-block:: console

      LE conn param updated: int 0x0027 lat 0 to 42

#.  Tap the arrow next to the RX characteristic to send data:

   .. figure:: /images/ps_nrf_send.png
   :alt: BLE send data

   Observe the following message on the client's console:

   .. code-block:: console

      bt_nus: on_receive: Received data, handle 0, conn 0x200023c4

Testing OpenThread
==================

Complete the following steps to test OpenThread API serialization:

#. Connect to the UART console of the client device using a terminal emulator of your choice.

#. Connect to the UART console of the server device using a terminal emulator of your choice.

#. Reboot **both** devices at the same time using the **IF BOOT/RESET** button on the DK.

#. Wait a few seconds until the following message on both UART consoles:

   .. code-block:: console

      uart:~$ [00:00:03.392,517] <dbg> NRF_RPC: nrf_rpc_init: Done initializing nRF RPC module


   This indicates that the communication between the devices has been initialized properly.

#. Run the following command on the client's console to bring the OpenThread interface on the server device up:

   .. code-block:: console

      uart:~$ ot ifconfig up
      Done
      [00:02:28.980,041] <dbg> NRF_RPC: cmd_ctx_alloc: Command context 0 allocated
      [00:02:28.980,102] <dbg> NRF_RPC: nrf_rpc_cmd_common: Sending command 0x00 from group 0x01
      [00:02:28.980,133] <dbg> nrf_rpc_uart: send: Sending frame
                                             80 00 ff 01 01 f6
      ...


#. Bring the corresponding Zephyr network interface on the client device:

   .. code-block:: console

      uart:~$ net iface up 1
      Interface 1 is up

#. Verify that the Zephyr network interface has been automatically configured with OpenThread's link-local address:

   .. code-block:: console

      uart:~$ ot ipaddr
      fe80:0:0:0:6c26:956a:813:1e33
      Done
      ...

      uart:~$ net iface  show

      Interface net0 (0x200012c8) (<unknown type>) [1]
      =========================================
      MTU       : 1280
      Flags     : NO_AUTO_START,IPv6,NO_ND,NO_MLD
      Device    : ot_rpc (0x2b748)
      IPv6 unicast addresses (max 5):
            fe80::6c26:956a:813:1e33 autoconf preferred infinite

#. Start Thread and become a leader:

   .. code-block:: console
      uart:~$ ot thread start
      Done
      ...

      uart:~$ ot state leader
      Done

#. Verify that the Zephyr network interface has automatically received OpenThread's mesh-local addresses:

   .. code-block:: console

      uart:~$ net iface  show

      Interface net0 (0x200012c8) (<unknown type>) [1]
      =========================================
      MTU       : 1280
      Flags     : NO_AUTO_START,IPv6,NO_ND,NO_MLD
      Device    : ot_rpc (0x2b748)
      IPv6 unicast addresses (max 5):
            fe80::6c26:956a:813:1e33 autoconf preferred infinite
            fdde:ad00:beef:0:e503:abfd:1c8d:2664 autoconf preferred infinite meshlocal
            fdde:ad00:beef::ff:fe00:fc00 autoconf preferred infinite meshlocal


   This happens because the client registers a notification callback for OpenThread state changes at the server device and it continuously refreshes the client's IPv6 address list when that changes on the server side.

#. Retrieve the operational dataset of the Thread network:

    .. code-block:: console

      uart:~$ ot dataset active -x
      0e080000000000000000000300000b35060004001fffe00208dead00beef00cafe0708fddead00beef00000510f7893f15a55d8adeacad288c38bf32cc030a4f70656e546872656164010240120410d48d777a474f80e61aa5680de764bd6d0c0402a0f7f8
      Done

#. To send a UDP packet to a peer device and UDP port 5555, run the following command:

   .. code-block:: console

      uart:~$ net udp send fe80:0:0:0:6c26:956a:813:1e34 5555 AAAA
      Message sent

#. To open a UDP port 5555 and listen for incoming UDP datagrams from peer devices, run the following command:

   .. code-block:: console

      uart:~$ net udp bind fe80:0:0:0:6c26:956a:813:1e33 5555
