# Copyright (c) 2023-present The Bitquantum Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

add_library(bitquantum_zmq STATIC EXCLUDE_FROM_ALL
  zmqabstractnotifier.cpp
  zmqnotificationinterface.cpp
  zmqpublishnotifier.cpp
  zmqrpc.cpp
  zmqutil.cpp
)
target_compile_definitions(bitquantum_zmq
  INTERFACE
    ENABLE_ZMQ=1
)
target_link_libraries(bitquantum_zmq
  PRIVATE
    core_interface
    univalue
    zeromq
)
