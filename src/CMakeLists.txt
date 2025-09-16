# Copyright (c) 2023-present The Bitquantum Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include(AddWindowsResources)

configure_file(${PROJECT_SOURCE_DIR}/cmake/bitquantum-build-config.h.in bitquantum-build-config.h USE_SOURCE_PERMISSIONS @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

#=============================
# Subprojects
#=============================
# Subprojects include subdirectories that do or could have tests
# and/or benchmark binaries, such as all subtrees and univalue.
# These need to be included before CMAKE_*_OUTPUT_DIRECTORY variables
# are set, so output locations of subproject tests and libraries are
# not overridden.
include(../cmake/crc32c.cmake)
include(../cmake/leveldb.cmake)
include(../cmake/minisketch.cmake)
add_subdirectory(univalue)
if (ENABLE_IPC AND NOT WITH_EXTERNAL_LIBMULTIPROCESS)
  include(../cmake/libmultiprocess.cmake)
  add_libmultiprocess(ipc/libmultiprocess)
endif()
include(../cmake/secp256k1.cmake)
add_secp256k1(secp256k1)

# Set top-level target output locations.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()

add_custom_target(generate_build_info
  BYPRODUCTS ${PROJECT_BINARY_DIR}/src/bitquantum-build-info.h
  COMMAND ${CMAKE_COMMAND} -DBUILD_INFO_HEADER_PATH=${PROJECT_BINARY_DIR}/src/bitquantum-build-info.h -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P ${PROJECT_SOURCE_DIR}/cmake/script/GenerateBuildInfo.cmake
  COMMENT "Generating bitquantum-build-info.h"
  VERBATIM
)
add_library(bitquantum_clientversion STATIC EXCLUDE_FROM_ALL
  clientversion.cpp
)
target_link_libraries(bitquantum_clientversion
  PRIVATE
    core_interface
)
add_dependencies(bitquantum_clientversion generate_build_info)

add_subdirectory(crypto)
add_subdirectory(util)
if(ENABLE_IPC)
  add_subdirectory(ipc)
endif()

add_library(bitquantum_consensus STATIC EXCLUDE_FROM_ALL
  arith_uint256.cpp
  consensus/merkle.cpp
  consensus/tx_check.cpp
  hash.cpp
  primitives/block.cpp
  primitives/transaction.cpp
  pubkey.cpp
  script/interpreter.cpp
  script/script.cpp
  script/script_error.cpp
  uint256.cpp
)
target_link_libraries(bitquantum_consensus
  PRIVATE
    core_interface
    bitquantum_crypto
    secp256k1
)

if(WITH_ZMQ)
  add_subdirectory(zmq)
endif()

# Home for common functionality shared by different executables and libraries.
# Similar to `bitquantum_util` library, but higher-level.
add_library(bitquantum_common STATIC EXCLUDE_FROM_ALL
  addresstype.cpp
  base58.cpp
  bech32.cpp
  chain.cpp
  crypto/randomq_mining.cpp
  chainparams.cpp
  chainparamsbase.cpp
  coins.cpp
  common/args.cpp
  common/bloom.cpp
  common/config.cpp
  common/init.cpp
  common/interfaces.cpp
  common/messages.cpp
  common/netif.cpp
  common/pcp.cpp
  common/run_command.cpp
  common/settings.cpp
  common/signmessage.cpp
  common/system.cpp
  common/url.cpp
  compressor.cpp
  core_read.cpp
  core_write.cpp
  deploymentinfo.cpp
  external_signer.cpp
  init/common.cpp
  kernel/chainparams.cpp
  key.cpp
  key_io.cpp
  merkleblock.cpp
  musig.cpp
  net_permissions.cpp
  net_types.cpp
  netaddress.cpp
  netbase.cpp
  outputtype.cpp
  policy/feerate.cpp
  policy/policy.cpp
  pow.cpp
  protocol.cpp
  psbt.cpp
  rpc/rawtransaction_util.cpp
  rpc/request.cpp
  rpc/util.cpp
  scheduler.cpp
  script/descriptor.cpp
  script/miniscript.cpp
  script/parsing.cpp
  script/sign.cpp
  script/signingprovider.cpp
  script/solver.cpp
)
target_link_libraries(bitquantum_common
  PRIVATE
    core_interface
    bitquantum_consensus
    bitquantum_util
    univalue
    secp256k1
    Boost::headers
    $<TARGET_NAME_IF_EXISTS:USDT::headers>
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

include(InstallBinaryComponent)

if(ENABLE_WALLET)
  add_subdirectory(wallet)

  if(BUILD_WALLET_TOOL)
    add_executable(bitquantum-wallet
      bitquantum-wallet.cpp
      init/bitquantum-wallet.cpp
      wallet/wallettool.cpp
    )
    add_windows_resources(bitquantum-wallet bitquantum-wallet-res.rc)
    add_windows_application_manifest(bitquantum-wallet)
    target_link_libraries(bitquantum-wallet
      core_interface
      bitquantum_wallet
      bitquantum_common
      bitquantum_util
      Boost::headers
    )
    install_binary_component(bitquantum-wallet HAS_MANPAGE)
  endif()
endif()


# P2P and RPC server functionality used by `bitquantumd` and `bitquantum-qt` executables.
add_library(bitquantum_node STATIC EXCLUDE_FROM_ALL
  addrdb.cpp
  addrman.cpp
  banman.cpp
  bip324.cpp
  blockencodings.cpp
  blockfilter.cpp
  consensus/tx_verify.cpp
  dbwrapper.cpp
  deploymentstatus.cpp
  flatfile.cpp
  headerssync.cpp
  httprpc.cpp
  httpserver.cpp
  i2p.cpp
  index/base.cpp
  index/blockfilterindex.cpp
  index/coinstatsindex.cpp
  index/txindex.cpp
  init.cpp
  kernel/chain.cpp
  kernel/checks.cpp
  kernel/coinstats.cpp
  kernel/context.cpp
  kernel/cs_main.cpp
  kernel/disconnected_transactions.cpp
  kernel/mempool_removal_reason.cpp
  mapport.cpp
  net.cpp
  net_processing.cpp
  netgroup.cpp
  node/abort.cpp
  node/blockmanager_args.cpp
  node/blockstorage.cpp
  node/caches.cpp
  node/chainstate.cpp
  node/chainstatemanager_args.cpp
  node/coin.cpp
  node/coins_view_args.cpp
  node/connection_types.cpp
  node/context.cpp
  node/database_args.cpp
  node/eviction.cpp
  node/interface_ui.cpp
  node/interfaces.cpp
  node/kernel_notifications.cpp
  node/mempool_args.cpp
  node/mempool_persist.cpp
  node/mempool_persist_args.cpp
  node/miner.cpp
  node/mini_miner.cpp
  node/minisketchwrapper.cpp
  node/peerman_args.cpp
  node/psbt.cpp
  node/timeoffsets.cpp
  node/transaction.cpp
  node/txdownloadman_impl.cpp
  node/txorphanage.cpp
  node/txreconciliation.cpp
  node/utxo_snapshot.cpp
  node/warnings.cpp
  noui.cpp
  policy/ephemeral_policy.cpp
  policy/fees.cpp
  policy/fees_args.cpp
  policy/packages.cpp
  policy/rbf.cpp
  policy/settings.cpp
  policy/truc_policy.cpp
  rest.cpp
  rpc/blockchain.cpp
  rpc/external_signer.cpp
  rpc/fees.cpp
  rpc/mempool.cpp
  rpc/mining.cpp
  rpc/net.cpp
  rpc/node.cpp
  rpc/output_script.cpp
  rpc/rawtransaction.cpp
  rpc/server.cpp
  rpc/server_util.cpp
  rpc/signmessage.cpp
  rpc/txoutproof.cpp
  script/sigcache.cpp
  signet.cpp
  torcontrol.cpp
  txdb.cpp
  txgraph.cpp
  txmempool.cpp
  txrequest.cpp
  validation.cpp
  validationinterface.cpp
  versionbits.cpp
  $<$<TARGET_EXISTS:bitquantum_wallet>:wallet/init.cpp>
  $<$<NOT:$<TARGET_EXISTS:bitquantum_wallet>>:dummywallet.cpp>
)
target_link_libraries(bitquantum_node
  PRIVATE
    core_interface
    bitquantum_common
    bitquantum_util
    $<TARGET_NAME_IF_EXISTS:bitquantum_zmq>
    leveldb
    minisketch
    univalue
    Boost::headers
    $<TARGET_NAME_IF_EXISTS:libevent::core>
    $<TARGET_NAME_IF_EXISTS:libevent::extra>
    $<TARGET_NAME_IF_EXISTS:libevent::pthreads>
    $<TARGET_NAME_IF_EXISTS:USDT::headers>
)

# Bitquantum wrapper executable that can call other executables.
if(BUILD_BITQUANTUM_BIN)
  add_executable(bitquantum bitquantum.cpp)
  add_windows_resources(bitquantum bitquantum-res.rc)
  add_windows_application_manifest(bitquantum)
  target_link_libraries(bitquantum core_interface bitquantum_util)
  install_binary_component(bitquantum)
endif()

# Bitquantum Core bitquantumd.
if(BUILD_DAEMON)
  add_executable(bitquantumd
    bitquantumd.cpp
    init/bitquantumd.cpp
  )
  add_windows_resources(bitquantumd bitquantumd-res.rc)
  add_windows_application_manifest(bitquantumd)
  target_link_libraries(bitquantumd
    core_interface
    bitquantum_node
    $<TARGET_NAME_IF_EXISTS:bitquantum_wallet>
  )
  install_binary_component(bitquantumd HAS_MANPAGE)
endif()
if(ENABLE_IPC AND BUILD_DAEMON)
  add_executable(bitquantum-node
    bitquantumd.cpp
    init/bitquantum-node.cpp
  )
  target_link_libraries(bitquantum-node
    core_interface
    bitquantum_node
    bitquantum_ipc
    $<TARGET_NAME_IF_EXISTS:bitquantum_wallet>
  )
  install_binary_component(bitquantum-node INTERNAL)
endif()

if(ENABLE_IPC AND BUILD_TESTS)
    # bitquantum_ipc_test library target is defined here in src/CMakeLists.txt
    # instead of src/test/CMakeLists.txt so capnp files in src/test/ are able to
    # reference capnp files in src/ipc/capnp/ by relative path. The Cap'n Proto
    # compiler only allows importing by relative path when the importing and
    # imported files are underneath the same compilation source prefix, so the
    # source prefix must be src/, not src/test/
    add_library(bitquantum_ipc_test STATIC EXCLUDE_FROM_ALL
      test/ipc_test.cpp
    )
    target_capnp_sources(bitquantum_ipc_test ${PROJECT_SOURCE_DIR}
      test/ipc_test.capnp
    )
    add_dependencies(bitquantum_ipc_test bitquantum_ipc_headers)
endif()


add_library(bitquantum_cli STATIC EXCLUDE_FROM_ALL
  compat/stdin.cpp
  rpc/client.cpp
)
target_link_libraries(bitquantum_cli
  PUBLIC
    core_interface
    univalue
)


# Bitquantum Core RPC client
if(BUILD_CLI)
  add_executable(bitquantum-cli bitquantum-cli.cpp)
  add_windows_resources(bitquantum-cli bitquantum-cli-res.rc)
  add_windows_application_manifest(bitquantum-cli)
  target_link_libraries(bitquantum-cli
    core_interface
    bitquantum_cli
    bitquantum_common
    bitquantum_util
    libevent::core
    libevent::extra
  )
  install_binary_component(bitquantum-cli HAS_MANPAGE)
endif()

# Standalone RandomQ CPU miner (client-side via RPC)
add_executable(cpuminer-randomq
  tools/cpuminer-randomq.cpp
)
target_link_libraries(cpuminer-randomq
  PRIVATE
    core_interface
    bitquantum_cli
    bitquantum_common
    bitquantum_util
    libevent::core
    libevent::extra
)
install_binary_component(cpuminer-randomq)

# Standalone RandomQ GPU miner (OpenCL)
find_package(OpenCL QUIET)
if(OpenCL_FOUND)
  add_executable(gpuminer-randomq
    tools/gpuminer-randomq.cpp
  )
  target_link_libraries(gpuminer-randomq
    PRIVATE
      core_interface
      bitquantum_cli
      bitquantum_common
      bitquantum_util
      libevent::core
      libevent::extra
      OpenCL::OpenCL
  )
  target_compile_definitions(gpuminer-randomq PRIVATE OPENCL_FOUND)
  install_binary_component(gpuminer-randomq)
else()
  message(STATUS "OpenCL not found, skipping gpuminer-randomq")
endif()


if(BUILD_TX)
  add_executable(bitquantum-tx bitquantum-tx.cpp)
  add_windows_resources(bitquantum-tx bitquantum-tx-res.rc)
  add_windows_application_manifest(bitquantum-tx)
  target_link_libraries(bitquantum-tx
    core_interface
    bitquantum_common
    bitquantum_util
    univalue
  )
  install_binary_component(bitquantum-tx HAS_MANPAGE)
endif()


if(BUILD_UTIL)
  add_executable(bitquantum-util bitquantum-util.cpp)
  add_windows_resources(bitquantum-util bitquantum-util-res.rc)
  add_windows_application_manifest(bitquantum-util)
  target_link_libraries(bitquantum-util
    core_interface
    bitquantum_common
    bitquantum_util
  )
  install_binary_component(bitquantum-util HAS_MANPAGE)
endif()


if(BUILD_GUI)
  add_subdirectory(qt)
endif()


if(BUILD_KERNEL_LIB)
  add_subdirectory(kernel)
endif()

if(BUILD_UTIL_CHAINSTATE)
  add_executable(bitquantum-chainstate
    bitquantum-chainstate.cpp
  )
  # TODO: The `SKIP_BUILD_RPATH` property setting can be deleted
  #       in the future after reordering Guix script commands to
  #       perform binary checks after the installation step.
  # Relevant discussions:
  # - https://github.com/hebasto/bitquantum/pull/236#issuecomment-2183120953
  # - https://github.com/bitquantumcore/bitquantum /pull/30312#issuecomment-2191235833
  set_target_properties(bitquantum-chainstate PROPERTIES
    SKIP_BUILD_RPATH OFF
  )
  target_link_libraries(bitquantum-chainstate
    PRIVATE
      core_interface
      bitquantumkernel
  )
  install_binary_component(bitquantum-chainstate INTERNAL)
endif()


add_subdirectory(test/util)
if(BUILD_BENCH)
  add_subdirectory(bench)
endif()

if(BUILD_TESTS)
  add_subdirectory(test)
endif()

if(BUILD_FUZZ_BINARY)
  add_subdirectory(test/fuzz)
endif()
