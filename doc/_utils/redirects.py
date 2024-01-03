"""
Copyright (c) 2022 Nordic Semiconductor
SPDX-License-Identifier: Apache-2.0

This module contains per-docset variables with a list of tuples
(old_url, new_url) for pages that need a redirect. This list allows redirecting
old URLs (caused by, e.g., reorganizing doc directories)

Notes:
    - Please keep lists sorted alphabetically.
    - URLs must be relative to document root (with NO leading slash), and
      without the html extension).

Examples:

    ("old/README", "new/index")
"""

NRF = [
    ("introduction", "index"),
    ("app_boards", "config_and_build/board_support"),
    ("app_dev/board_support/index", "config_and_build/board_support"),
    ("ug_bootloader", "config_and_build/bootloaders_and_dfu/bootloader"),
    ("app_dev/bootloaders_and_dfu/bootloader", "config_and_build/bootloaders_and_dfu/bootloader"),
    ("app_dev/bootloaders_and_dfu/bootloader", "config_and_build/bootloaders/bootloader"),
    ("ug_bootloader_adding", "config_and_build/bootloaders_and_dfu/bootloader_adding"),
    ("app_dev/bootloaders_and_dfu/bootloader_adding", "config_and_build/bootloaders_and_dfu/bootloader_adding"),
    ("app_dev/bootloaders_and_dfu/bootloader_adding", "config_and_build/bootloaders/bootloader_adding"),
    ("ug_bootloader_config", "config_and_build/bootloaders_and_dfu/bootloader_config"),
    ("app_dev/bootloaders_and_dfu/bootloader_config", "config_and_build/bootloaders_and_dfu/bootloader_config"),
    ("app_dev/bootloaders_and_dfu/bootloader_config", "config_and_build/bootloaders/bootloader_config"),
    ("ug_bootloader_external_flash", "config_and_build/bootloaders_and_dfu/bootloader_external_flash"),
    ("app_dev/bootloaders_and_dfu/bootloader_external_flash", "config_and_build/bootloaders_and_dfu/bootloader_external_flash"),
    ("app_dev/bootloaders_and_dfu/bootloader_external_flash", "config_and_build/bootloaders/bootloader_external_flash"),
    ("ug_bootloader_testing", "config_and_build/bootloaders_and_dfu/bootloader_testing"),
    ("app_dev/bootloaders_and_dfu/bootloader_testing", "config_and_build/bootloaders_and_dfu/bootloader_testing"),
    ("app_dev/bootloaders_and_dfu/bootloader_testing", "config_and_build/bootloaders/bootloader_testing"),
    ("ug_fw_update", "config_and_build/bootloaders_and_dfu/fw_update"),
    ("app_dev/bootloaders_and_dfu/fw_update", "config_and_build/bootloaders_and_dfu/fw_update"),
    ("config_and_build/bootloaders_and_dfu/fw_update", "config_and_build/dfu/index"),
    ("app_bootloaders", "config_and_build/bootloaders_and_dfu/index"),
    ("app_dev/bootloaders_and_dfu/index", "config_and_build/bootloaders_and_dfu/index"),
    ("ug_logging", "test_and_optimize/logging"),
    ("app_dev/logging/index", "test_and_optimize/logging"),
    ("ug_multi_image", "config_and_build/multi_image"),
    ("app_dev/multi_image/index", "config_and_build/multi_image"),
    ("app_opt", "test_and_optimize/optimizing/index"),
    ("app_dev/optimizing/index", "test_and_optimize/optimizing/index"),
    ("app_memory", "test_and_optimize/optimizing/memory"),
    ("app_dev/optimizing/memory", "test_and_optimize/optimizing/memory"),
    ("app_power_opt", "test_and_optimize/optimizing/power"),
    ("app_dev/optimizing/power", "test_and_optimize/optimizing/power"),
    ("ug_pinctrl", "device_guides/pin_control"),
    ("app_dev/pin_control/index", "device_guides/pin_control"),
    ("ug_unity_testing", "test_and_optimize/testing_unity_cmock"),
    ("app_dev/testing_unity_cmock/index", "test_and_optimize/testing_unity_cmock"),
    ("ug_tfm", "security/tfm"),
    ("app_dev/tfm/index", "security/tfm"),
    ("app_build_system", "config_and_build/config_and_build_system"),
    ("app_dev/build_and_config_system/index", "config_and_build/config_and_build_system"),
    ("ug_radio_coex", "device_guides/wifi_coex"),
    ("app_dev/wifi_coex/index", "device_guides/wifi_coex"),
    ("ug_radio_fem", "device_guides/working_with_fem"),
    ("app_dev/working_with_fem/index", "device_guides/working_with_fem"),
    ("ug_dev_model", "releases_and_maturity/dev_model"),
    ("dev_model", "releases_and_maturity/dev_model"),
    ("dm_adding_code", "releases_and_maturity/developing/adding_code"),
    ("developing/adding_code", "releases_and_maturity/developing/adding_code"),
    ("dm_code_base", "releases_and_maturity/developing/code_base"),
    ("developing/code_base", "releases_and_maturity/developing/code_base"),
    ("dm_managing_code", "releases_and_maturity/developing/managing_code"),
    ("developing/managing_code", "releases_and_maturity/developing/managing_code"),
    ("dm_ncs_distro", "releases_and_maturity/developing/ncs_distro"),
    ("developing/ncs_distro", "releases_and_maturity/developing/ncs_distro"),
    ("doc_build", "documentation/build"),
    ("doc_build_process", "documentation/build_process"),
    ("doc_structure", "documentation/structure"),
    ("doc_styleguide", "documentation/styleguide"),
    ("doc_templates", "documentation/templates"),
    ("documentation/build", "dev_model_and_contributions/documentation/build"),
    ("documentation/build_process", "dev_model_and_contributions/documentation/doc_build_process"),
    ("documentation/structure", "dev_model_and_contributions/documentation/structure"),
    ("documentation/styleguide", "dev_model_and_contributions/documentation/styleguide"),
    ("documentation/templates", "dev_model_and_contributions/documentation/templates"),
    ("ug_bt_fast_pair", "external_comp/bt_fast_pair"),
    ("ug_edge_impulse", "external_comp/edge_impulse"),
    ("ug_memfault", "external_comp/memfault"),
    ("ug_nrf_cloud", "external_comp/nrf_cloud"),
    ("gs_assistant", "installation/assistant"),
    ("getting_started", "installation"),
    ("getting_started/assistant", "installation/install_ncs"),
    ("installation/assistant", "installation/install_ncs"),
    ("gs_installing", "installation/installing"),
    ("getting_started/installing", "installation/install_ncs"),
    ("installation/installing", "installation/install_ncs"),
    ("gs_modifying", "config_and_build/modifying"),
    ("getting_started/modifying", "config_and_build/modifying"),
    ("gs_programming", "config_and_build/programming"),
    ("getting_started/programming", "config_and_build/programming"),
    ("gs_recommended_versions", "installation/recommended_versions"),
    ("getting_started/recommended_versions", "installation/recommended_versions"),
    ("gs_testing", "test_and_optimize/testing"),
    ("getting_started/testing", "test_and_optimize/testing"),
    ("gs_updating", "installation/updating"),
    ("getting_started/updating", "installation/updating"),
    ("ug_nrf52", "device_guides/nrf52"),
    ("nrf52", "device_guides/nrf52"),
    ("ug_nrf53", "device_guides/nrf53"),
    ("nrf53", "device_guides/nrf53"),
    ("ug_nrf70", "device_guides/nrf70"),
    ("nrf70", "device_guides/nrf70"),
    ("ug_nrf91", "device_guides/nrf91"),
    ("nrf91", "device_guides/nrf91"),
    ("ug_ble_controller", "protocols/ble/index"),
    ("ug_bt_mesh_architecture", "protocols/bt_mesh/architecture"),
    ("ug_bt_mesh_concepts", "protocols/bt_mesh/concepts"),
    ("ug_bt_mesh_configuring", "protocols/bt_mesh/configuring"),
    ("ug_bt_mesh_fota", "protocols/bt_mesh/fota"),
    ("ug_bt_mesh", "protocols/bt_mesh/index"),
    ("ug_bt_mesh_model_config_app", "protocols/bt_mesh/model_config_app"),
    ("ug_bt_mesh_node_removal", "protocols/bt_mesh/node_removal"),
    ("ug_bt_mesh_reserved_ids", "protocols/bt_mesh/reserved_ids"),
    ("ug_bt_mesh_supported_features", "protocols/bt_mesh/supported_features"),
    ("ug_bt_mesh_vendor_model", "protocols/bt_mesh/vendor_model/index"),
    ("ug_bt_mesh_vendor_model_chat_sample_walk_through", "protocols/bt_mesh/vendor_model/chat_sample_walk_through"),
    ("ug_bt_mesh_vendor_model_dev_overview", "protocols/bt_mesh/vendor_model/dev_overview"),
    ("ug_esb","protocols/esb/index"),
    ("ug_gzll","protocols/gazell/gzll"),
    ("ug_gzp","protocols/gazell/gzp"),
    ("ug_gz","protocols/gazell/index"),
    ("ug_matter_device_attestation","protocols/matter/end_product/attestation"),
    ("ug_matter_device_bootloader","protocols/matter/end_product/bootloader"),
    ("ug_matter_device_certification","protocols/matter/end_product/certification"),
    ("ug_matter_ecosystems_certification","protocols/matter/end_product/ecosystems_certification"),
    ("ug_matter_device_dcl","protocols/matter/end_product/dcl"),
    ("ug_matter_device_factory_provisioning","protocols/matter/end_product/factory_provisioning"),
    ("ug_matter_intro_device","protocols/matter/end_product/index"),
    ("ug_matter_device_prerequisites","protocols/matter/end_product/prerequisites"),
    ("ug_matter_gs_adding_clusters","protocols/matter/getting_started/adding_clusters"),
    ("ug_matter_gs_advanced_kconfigs","protocols/matter/getting_started/advanced_kconfigs"),
    ("ug_matter_hw_requirements","protocols/matter/getting_started/hw_requirements"),
    ("ug_matter_intro_gs","protocols/matter/getting_started/index"),
    ("ug_matter_gs_kconfig","protocols/matter/getting_started/kconfig"),
    ("ug_matter_gs_testing","protocols/matter/getting_started/testing/index"),
    ("ug_matter_gs_testing_thread_one_otbr","protocols/matter/getting_started/testing/thread_one_otbr"),
    ("ug_matter_gs_testing_thread_separate_otbr_android","protocols/matter/getting_started/testing/thread_separate_otbr_android"),
    ("ug_matter_gs_testing_thread_separate_linux_macos","protocols/matter/getting_started/testing/thread_separate_otbr_linux_macos"),
    ("ug_matter_gs_testing_wifi_mobile","protocols/matter/getting_started/testing/wifi_mobile"),
    ("ug_matter_gs_testing_wifi_pc","protocols/matter/getting_started/testing/wifi_pc"),
    ("ug_matter_gs_tools","protocols/matter/getting_started/tools"),
    ("ug_matter","protocols/matter/index"),
    ("ug_matter_overview_architecture","protocols/matter/overview/architecture"),
    ("ug_matter_overview_commissioning","protocols/matter/overview/commisioning"),
    ("ug_matter_overview_data_model","protocols/matter/overview/data_model"),
    ("ug_matter_overview_dev_model","protocols/matter/overview/dev_model"),
    ("ug_matter_overview_dfu","protocols/matter/overview/dfu"),
    ("ug_matter_intro_overview","protocols/matter/overview/index"),
    ("ug_matter_overview_int_model","protocols/matter/overview/int_model"),
    ("ug_matter_overview_architecture_integration","protocols/matter/overview/integration"),
    ("ug_matter_overview_multi_fabrics","protocols/matter/overview/multi_fabrics"),
    ("ug_matter_overview_network_topologies","protocols/matter/overview/network_topologies"),
    ("ug_matter_overview_security","protocols/matter/overview/security"),
    ("ug_multiprotocol_support","protocols/multiprotocol/index"),
    ("ug_nfc","protocols/nfc/index"),
    ("ug_thread_certification","protocols/thread/certification"),
    ("ug_thread_configuring","protocols/thread/configuring"),
    ("ug_thread","protocols/thread/index"),
    ("ug_thread_architectures","protocols/thread/overview/architectures"),
    ("ug_thread_commissioning","protocols/thread/overview/commissioning"),
    ("ug_thread_communication","protocols/thread/overview/communication"),
    ("ug_thread_overview","protocols/thread/overview/index"),
    ("ug_thread_ot_integration","protocols/thread/overview/ot_integration"),
    ("ug_thread_ot_memory","protocols/thread/overview/ot_memory"),
    ("ug_thread_supported_features","protocols/thread/overview/supported_features"),
    ("ug_thread_prebuilt_libs","protocols/thread/prebuilt_libs"),
    ("ug_thread_tools","protocols/thread/tools"),
    ("ug_wifi","protocols/wifi/index"),
    ("ug_zigbee_adding_clusters","protocols/zigbee/adding_clusters"),
    ("ug_zigbee_architectures","protocols/zigbee/architectures"),
    ("ug_zigbee_commissioning","protocols/zigbee/commissioning"),
    ("ug_zigbee_configuring","protocols/zigbee/configuring"),
    ("ug_zigbee_configuring_libraries","protocols/zigbee/configuring_libraries"),
    ("ug_zigbee_configuring_zboss_traces","protocols/zigbee/configuring_zboss_traces"),
    ("ug_zigbee","protocols/zigbee/index"),
    ("ug_zigbee_memory","protocols/zigbee/memory"),
    ("ug_zigbee_other_ecosystems","protocols/zigbee/other_ecosystems"),
    ("ug_zigbee_qsg","protocols/zigbee/qsg"),
    ("ug_zigbee_supported_features","protocols/zigbee/supported_features"),
    ("ug_zigbee_tools","protocols/zigbee/tools"),
    ("samples/samples_bl","samples/bl"),
    ("samples/samples_crypto","samples/crypto"),
    ("samples/samples_edge","samples/edge"),
    ("samples/samples_gazell","samples/gazell"),
    ("samples/samples_matter","samples/matter"),
    ("samples/samples_nfc","samples/nfc"),
    ("samples/samples_nrf5340","samples/nrf5340"),
    ("samples/samples_nrf9160","samples/cellular"),
    ("samples/samples_other","samples/other"),
    ("samples/samples_tfm","samples/tfm"),
    ("samples/samples_thread","samples/thread"),
    ("samples/samples_wifi","samples/wifi"),
    ("samples/samples_zigbee","samples/zigbee"),
    ("security_chapter","security/security"),
    ("security","security/security"),
    ("ug_nrf52_developing","device_guides/working_with_nrf/nrf52/developing"),
    ("working_with_nrf/nrf52/developing","device_guides/working_with_nrf/nrf52/developing"),
    ("ug_nrf52_features","device_guides/working_with_nrf/nrf52/features"),
    ("working_with_nrf/nrf52/features","device_guides/working_with_nrf/nrf52/features"),
    ("ug_nrf52_gs","device_guides/working_with_nrf/nrf52/gs"),
    ("working_with_nrf/nrf52/gs","device_guides/working_with_nrf/nrf52/gs"),
    ("ug_nrf5340","device_guides/working_with_nrf/nrf53/nrf5340"),
    ("working_with_nrf/nrf53/nrf5340","device_guides/working_with_nrf/nrf53/nrf5340"),
    ("ug_thingy53","device_guides/working_with_nrf/nrf53/thingy53"),
    ("working_with_nrf/nrf53/thingy53","device_guides/working_with_nrf/nrf53/thingy53"),
    ("ug_thingy53_gs","device_guides/working_with_nrf/nrf53/thingy53_gs"),
    ("working_with_nrf/nrf53/thingy53_gs","device_guides/working_with_nrf/nrf53/thingy53_gs"),
    ("ug_nrf7002_constrained","device_guides/working_with_nrf/nrf70/developing/constrained"),
    ("working_with_nrf/nrf70/developing/constrained","device_guides/working_with_nrf/nrf70/developing/constrained"),
    ("ug_nrf70_developing","device_guides/working_with_nrf/nrf70/developing/index"),
    ("working_with_nrf/nrf70/developing/index","device_guides/working_with_nrf/nrf70/developing/index"),
    ("ug_nrf70_developing_powersave","device_guides/working_with_nrf/nrf70/developing/powersave"),
    ("working_with_nrf/nrf70/developing/powersave","device_guides/working_with_nrf/nrf70/developing/powersave"),
    ("ug_nrf70_features","device_guides/working_with_nrf/nrf70/features"),
    ("working_with_nrf/nrf70/features","device_guides/working_with_nrf/nrf70/features"),
    ("ug_nrf7002_gs","device_guides/working_with_nrf/nrf70/gs"),
    ("working_with_nrf/nrf70/gs","device_guides/working_with_nrf/nrf70/gs"),
    ("ug_nrf9160","device_guides/working_with_nrf/nrf91/nrf9160"),
    ("working_with_nrf/nrf91/nrf9160","device_guides/working_with_nrf/nrf91/nrf9160"),
    ("ug_nrf9160_gs","device_guides/working_with_nrf/nrf91/nrf9160_gs"),
    ("working_with_nrf/nrf91/nrf9160_gs","device_guides/working_with_nrf/nrf91/nrf9160_gs"),
    ("ug_nrf91_features","device_guides/working_with_nrf/nrf91/nrf91_features"),
    ("working_with_nrf/nrf91/nrf91_features","device_guides/working_with_nrf/nrf91/nrf91_features"),
    ("ug_thingy91","device_guides/working_with_nrf/nrf91/thingy91"),
    ("working_with_nrf/nrf91/thingy91","device_guides/working_with_nrf/nrf91/thingy91"),
    ("ug_thingy91_gsg","device_guides/working_with_nrf/nrf91/thingy91_gsg"),
    ("working_with_nrf/nrf91/thingy91_gsg","device_guides/working_with_nrf/nrf91/thingy91_gsg"),
    ("known_issues","releases_and_maturity/known_issues"),
    ("libraries/networking/nrf_cloud_agps","libraries/networking/nrf_cloud_agnss"),
]
