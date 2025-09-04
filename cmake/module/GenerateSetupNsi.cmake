# Copyright (c) 2023-present The Bitquantum Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

function(generate_setup_nsi)
  set(abs_top_srcdir ${PROJECT_SOURCE_DIR})
  set(abs_top_builddir ${PROJECT_BINARY_DIR})
  set(CLIENT_URL ${PROJECT_HOMEPAGE_URL})
  set(CLIENT_TARNAME "bitquantum")
  set(BITQUANTUM_WRAPPER_NAME "bitquantum")
  set(BITQUANTUM_GUI_NAME "bitquantum-qt")
  set(BITQUANTUM_DAEMON_NAME "bitquantumd")
  set(BITQUANTUM_CLI_NAME "bitquantum-cli")
  set(BITQUANTUM_TX_NAME "bitquantum-tx")
  set(BITQUANTUM_WALLET_TOOL_NAME "bitquantum-wallet")
  set(BITQUANTUM_TEST_NAME "test_bitquantum")
  set(EXEEXT ${CMAKE_EXECUTABLE_SUFFIX})
  configure_file(${PROJECT_SOURCE_DIR}/share/setup.nsi.in ${PROJECT_BINARY_DIR}/bitquantum-win64-setup.nsi USE_SOURCE_PERMISSIONS @ONLY)
endfunction()
