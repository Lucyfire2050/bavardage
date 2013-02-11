# Partie libclient.so
set(libclient_SRCS ../common/SocketTCP.c libclient.c ../common/SocketTCP.h libclient.h)
add_library(client STATIC ${libclient_SRCS})

# Partie client lignes de commandes
set(CLIENT_CLI_FLAGS -w -pthread -std=c99)
add_definitions(${CLIENT_CLI_FLAGS})
set(client_cli_SRCS client-cli.c)
find_package(Threads)
add_executable(client-cli ${client_cli_SRCS})
target_link_libraries(client-cli ${CMAKE_THREAD_LIBS_INIT} client)

# Partie Client graphique
find_package(Vala REQUIRED)
include(ValaVersion)
ensure_vala_version("0.16.0" MINIMUM)
include(ValaPrecompile)
vala_precompile(VALA_C 
  Client.vala
PACKAGES
  gtk+-3.0
  gee-1.0
  glib-2.0
OPTIONS
  --thread
  --target-glib=2.32
CUSTOM_VAPIS
  libclient.vapi
)

find_package(PkgConfig)
pkg_check_modules(DEPS REQUIRED gtk+-3.0 gee-1.0 glib-2.0)
set(CFLAGS ${DEPS_CFLAGS} ${DEPS_CFLAGS_OTHER} -w)

set(LIB_PATHS ${DEPS_LIBRARY_DIRS} ${CMAKE_BIN_DIR})
link_directories(${LIB_PATHS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
remove_definitions(${CLIENT_CLI_FLAGS})
add_definitions(${CFLAGS})

add_executable(Client ${VALA_C})
target_link_libraries(Client ${DEPS_LIBRARIES} client)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/interface.ui DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS Client RUNTIME DESTINATION bin)