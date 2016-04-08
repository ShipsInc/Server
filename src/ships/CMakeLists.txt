# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

file(GLOB sources_localdir *.cpp *.h)

set(ships_STAT_SRCS
  ${ships_STAT_SRCS}
  ${sources_localdir}
)

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src/shared
  ${CMAKE_SOURCE_DIR}/src/shared/Packets
  ${CMAKE_SOURCE_DIR}/src/shared/Database
  ${CMAKE_SOURCE_DIR}/src/shared/Network
  ${CMAKE_SOURCE_DIR}/src/game
  ${CMAKE_SOURCE_DIR}/src/game/Protocol
  ${CMAKE_SOURCE_DIR}/src/game/Session
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${MYSQL_INCLUDE_DIR}
)

add_executable(ships
  ${ships_STAT_SRCS}
)

if( UNIX AND NOT NOJEM AND NOT APPLE )
  set(ships_LINK_FLAGS "-pthread -lncurses ${ships_LINK_FLAGS}")
endif()

set_target_properties(ships PROPERTIES LINK_FLAGS "${ships_LINK_FLAGS}")

target_link_libraries(ships
  game
  shared
  ${MYSQL_LIBRARY}
  ${Boost_LIBRARIES}
)

if( UNIX )
  install(TARGETS ships DESTINATION bin)
elseif( WIN32 )
  install(TARGETS ships DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()
