# Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#
# uve_network: reliable UDP packet framing plus the transport seam (endpoint contract +
# in-memory loopback; socket transports implement the seam later). Fully self-contained -
# no uve_* dependency at all.

add_library(uve_network STATIC
    Internal/network_transport_uve.cpp
    Internal/reliable_packet_window_uve.cpp
)
target_include_directories(uve_network PUBLIC Expose)
target_compile_features(uve_network PUBLIC cxx_std_23)
uve_set_warnings(uve_network)
