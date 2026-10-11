# Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#
# uve_network: reliable UDP packet framing plus the transport seam, sessions, and the
# replication core. Depends on uve_math (PUBLIC: positions cross the replication header)
# and uve_utilities (PRIVATE: the wire codec stays in the .cpp).

add_library(uve_network STATIC
    Internal/network_conditions_uve.cpp
    Internal/network_replication_uve.cpp
    Internal/network_session_uve.cpp
    Internal/network_transport_uve.cpp
    Internal/reliable_packet_window_uve.cpp
)
target_include_directories(uve_network PUBLIC Expose)
target_compile_features(uve_network PUBLIC cxx_std_23)
target_link_libraries(uve_network PUBLIC uve_math PRIVATE uve_utilities)
uve_set_warnings(uve_network)
