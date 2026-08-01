# --- Threading support (still needed for GCC even with C++11)
set(CMAKE_THREAD_PREFER_PTHREAD YES)
find_package(Threads REQUIRED)

# --- Ogre 3D graphics engine ---
#
# Was `find_package(OGRE 1.11 ...)`. OGRE's own OGREConfigVersion.cmake matches
# on major version, so asking for 1.11 REJECTS 14.5.2 outright:
#
#   Could not find a configuration file for package "OGRE" that is compatible
#   with requested version "1.11"
#     ... OGREConfig.cmake, version: 14.5.2
#
# Requesting 14 pins us to the major version we actually target (Phase 2.0), and
# will loudly reject a stale 1.x install rather than silently half-configuring.
find_package(OGRE 14 REQUIRED COMPONENTS Bites Overlay Paging RTShaderSystem MeshLodGenerator Terrain)

# --- Object Oriented Input System ---
find_package(OIS REQUIRED)

# --- MyGUI - graphical user inferface ---
find_package(MyGUI REQUIRED)

# --- fmt - A modern formatting library  ---
find_package(fmt REQUIRED)

# --- RapidJSON - JSON parser/generator ---
find_package(RapidJSON REQUIRED)

# Components

# --- OpenAL - audio library ---
find_package(OpenAL)
cmake_dependent_option(ROR_USE_OPENAL "use OPENAL" ON "OPENAL_FOUND" OFF)

# --- Discord RPC -- Rich Presence for discord ---
find_package(discord_rpc)
cmake_dependent_option(ROR_USE_DISCORD_RPC "use discord-rpc" ON "discord_rpc_FOUND" OFF)

# --- SocketW - networking library ---
find_package(SocketW)
cmake_dependent_option(ROR_USE_SOCKETW "use SOCKETW" ON "TARGET SocketW::SocketW" OFF)
if(TARGET SocketW::SocketW)
    # Fix to find SocketW+OpenSSL
    find_package(OpenSSL QUIET)
endif()

# --- AngelScript - scripting interface ---
find_package(Angelscript)
cmake_dependent_option(ROR_USE_ANGELSCRIPT "use angelscript" ON "TARGET Angelscript::angelscript" OFF)

# --- cURL ---
find_package(CURL)
cmake_dependent_option(ROR_USE_CURL "use curl" ON "CURL_FOUND" OFF)

# --- Caelum -- Ogre addon for realistic sky rendering ---
find_package(Caelum)
cmake_dependent_option(ROR_USE_CAELUM "use caelum" ON "TARGET Caelum::Caelum" OFF)

# --- PagedGeometry -- Ogre addon ---
find_package(PagedGeometry)
cmake_dependent_option(ROR_USE_PAGED "use pagedgeometry" ON "TARGET PagedGeometry::PagedGeometry" OFF)