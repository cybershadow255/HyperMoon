cmake_minimum_required(VERSION 3.15)
project(HyperMon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------
#  Optionen
# ----------------------------------------------------------------
option(BUILD_DRIVER "Versuche den Treiber zu bauen (erfordert WDK)" OFF)

# ----------------------------------------------------------------
#  Usermode - Setup Launcher
# ----------------------------------------------------------------
add_executable(setup
    HyperMon/HyperMon/usermode/setup.cpp
    HyperMon/HyperMon/usermode/mapper.h
    HyperMon/HyperMon/driver/shared.h
)
target_include_directories(setup PRIVATE HyperMon/HyperMon/driver)
target_link_libraries(setup PRIVATE advapi32 shell32)

# ----------------------------------------------------------------
#  Usermode - Monitor
# ----------------------------------------------------------------
add_executable(monitor
    HyperMon/HyperMon/usermode/monitor.cpp
    HyperMon/HyperMon/driver/shared.h
)
target_include_directories(monitor PRIVATE HyperMon/HyperMon/driver)
target_link_libraries(monitor PRIVATE advapi32)

# ----------------------------------------------------------------
#  Hinweis zum Treiber
# ----------------------------------------------------------------
if(BUILD_DRIVER)
    message(STATUS "Treiber-Build ist aktiviert. Stelle sicher, dass das WDK installiert ist.")
    # Die Kompilierung eines .sys Treibers via CMake ist komplex und erfordert
    # spezielle Toolchains (z.B. https://github.com/thecount/cmake-wdk).
    # Fuer dieses Projekt wird empfohlen, den Treiber weiterhin via Visual Studio (SLN) zu bauen.
else()
    message(STATUS "Treiber-Build ist deaktiviert. Nutze Visual Studio fuer HyperMon.sys.")
endif()

# ----------------------------------------------------------------
#  Installations-Struktur
# ----------------------------------------------------------------
install(TARGETS setup monitor DESTINATION bin)
