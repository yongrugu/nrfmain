.. _nrf_rpc_protocols_serialization_client:

Protocols serialization: client
###############################

.. contents::
   :local:
   :depth: 2

The Protocols serialization client sample demonstrates how to send remote procedure calls (RPC) to a Protocols serialization server device :ref:`_protocols_serialization_server_app`.
The RPCs are used to control Bluetooth LE and OpenThread stacks running on the server device.
The client and server devices use nRF RPC library and the UART interface to communicate with each other.

Requirements
************

The sample supports the following development kit:

.. table-from-sample-yaml::

Overview
********

The protocols serialization client sample is a thin client that neither includes OpenThread nor Bluetooth LE stacks. Instead, it provides
implementations of selected OpenThread and Bluetooth LE functions that forward the function calls over UART to the protocols serialization server.
The client also includes a shell for testing the serialization.

Building and running
********************

Complete the following steps to build and flash the client sample into an nRF52840 DK:

#. Open a terminal and navigate to the nRF Connect SDK installation directory.

#. Navigate to the client sample directory:

   .. code-block:: console

      ~/ncs/nrf$ cd samples/nrf_rpc/ps_client


#. Build the sample:

   .. code-block:: console

      ~/ncs/nrf/samples/nrf_rpc/ps_client$ west build -b nrf52840dk/nrf52840 -S "openthread;ble;debug"


   Note that you can modify the list of enabled features, which by default includes **ble** support as well as **debug** logs.

#. Flash the client sample into an nRF52840 DK:

   .. code-block:: console

      ~/ncs/nrf/samples/nrf_rpc/ps_client$ west flash --erase

Testing
=======

To test the client sample, use the protocol serialization server application test procedure :ref:`_protocols_serialization_server_app_testing`.

Dependencies
************

This sample uses the following `sdk-nrfxlib`_ library:

* :ref:`nrfxlib:nrf_rpc`
