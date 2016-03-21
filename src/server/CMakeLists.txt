# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

file(GLOB_RECURSE sources_Session Session/*.cpp Session/*.h)
file(GLOB sources_localdir *.cpp *.h)

set(server_SRCS
  ${server_SRCS}
  ${sources_Session}
  ${sources_localdir}
)

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src/shared
  ${CMAKE_SOURCE_DIR}/src/shared/Network
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(server
  ${server_SRCS}
)

if( UNIX AND NOT NOJEM AND NOT APPLE )
  set(server_LINK_FLAGS "-pthread -lncurses ${server_LINK_FLAGS}")
endif()

set_target_properties(server PROPERTIES LINK_FLAGS "${server_LINK_FLAGS}")

target_link_libraries(server
  #game
  shared
  ${Boost_LIBRARIES}
)

if( UNIX )
  install(TARGETS server DESTINATION bin)
elseif( WIN32 )
  install(TARGETS server DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()