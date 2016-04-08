# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

file(GLOB_RECURSE sources_Session Session/*.cpp Session/*.h)
file(GLOB_RECURSE sources_Protocol Protocol/*.cpp Protocol/*.h)

file(GLOB sources_localdir *.cpp *.h)

set(game_STAT_SRCS
  ${game_STAT_SRCS}
  ${sources_Session}
  ${sources_Protocol}
  ${sources_localdir}
)

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src/shared
  ${CMAKE_SOURCE_DIR}/src/shared/Packets
  ${CMAKE_SOURCE_DIR}/src/shared/Database
  ${CMAKE_SOURCE_DIR}/src/shared/Network
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Protocol
  ${CMAKE_CURRENT_SOURCE_DIR}/Session
  ${MYSQL_INCLUDE_DIR}
  ${BOOST_INCLUDE_DIR}
)

add_library(game STATIC
  ${game_STAT_SRCS}
)