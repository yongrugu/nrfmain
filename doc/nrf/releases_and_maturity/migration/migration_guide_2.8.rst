.. _migration_2.8:

Migration guide for |NCS| v2.8.0 (Working draft)
################################################

.. contents::
   :local:
   :depth: 3

This document describes the changes required or recommended when migrating your application from |NCS| v2.7.0 to |NCS| v2.8.0.

.. HOWTO

   Add changes in the following format:

   Component (for example, application, sample or libraries)
   *********************************************************

   .. toggle::

      * Change1 and description
      * Change2 and description

.. _migration_2.8_required:

Required changes
****************

The following changes are mandatory to make your application work in the same way as in previous releases.

Samples and applications
========================

This section describes the changes related to samples and applications.

Serial LTE Modem (SLM)
----------------------

.. toggle::

  * The handling of Release Assistance Indication (RAI) socket options has been updated in the ``#XSOCKETOPT`` command.
    The individual RAI-related socket options have been consolidated into a single ``SO_RAI`` option.
    You must modify your application to use the new ``SO_RAI`` option with the corresponding value to specify the RAI behavior.
    The changes are as follows:

    The ``SO_RAI_NO_DATA``, ``SO_RAI_LAST``, ``SO_RAI_ONE_RESP``, ``SO_RAI_ONGOING``, and ``SO_RAI_WAIT_MORE`` options have been replaced by the ``SO_RAI`` option with values from ``1`` to ``5``.

    Here are the changes you need to make in your application code:

    * If you previously used ``AT#XSOCKETOPT=1,50,`` replace it with ``AT#XSOCKETOPT=1,61,1`` to indicate ``RAI_NO_DATA``.
    * If you previously used ``AT#XSOCKETOPT=1,51,`` replace it with ``AT#XSOCKETOPT=1,61,2`` to indicate ``RAI_LAST``.
    * If you previously used ``AT#XSOCKETOPT=1,52,`` replace it with ``AT#XSOCKETOPT=1,61,3`` to indicate ``RAI_ONE_RESP``.
    * If you previously used ``AT#XSOCKETOPT=1,53,`` replace it with ``AT#XSOCKETOPT=1,61,4`` to indicate ``RAI_ONGOING``.
    * If you previously used ``AT#XSOCKETOPT=1,54,`` replace it with ``AT#XSOCKETOPT=1,61,5`` to indicate ``RAI_WAIT_MORE``.

Libraries
=========

This section describes the changes related to libraries.

|no_changes_yet_note|

.. _migration_2.8_recommended:

Recommended changes
*******************

The following changes are recommended for your application to work optimally after the migration.

Samples and applications
========================

This section describes the changes related to samples and applications.

|no_changes_yet_note|

Libraries
=========

This section describes the changes related to libraries.

|no_changes_yet_note|
