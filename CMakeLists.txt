cmake_minimum_required(VERSION 3.13...3.27)

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
set(PICO_PLATFORM "rp2350")
set(PICO_BOARD "pico2")

set(libneural_network_a "${CMAKE_SOURCE_DIR}/lib/Z-Ant/zig-out/lib/libstatic_lib.a")

# Build the neural network library with size optimization
add_custom_command(
  OUTPUT "${libneural_network_a}"
  COMMAND cd ${CMAKE_SOURCE_DIR}/lib/Z-Ant && zig build lib -Dtarget=thumb-freestanding -Dcpu=cortex_m33 -Doptimize=ReleaseSmall
)
add_custom_target(libneural_network DEPENDS "${libneural_network_a}")

# Initialize the SDK based on PICO_SDK_PATH
include(pico_sdk_import.cmake)

project(pi_pico_zant CXX C ASM)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Enable size optimization and link-time optimization (LTO)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os")

# Strip debug symbols to reduce binary size
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g0")

# Add linker flags to remove unused sections
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")

# Disable floating-point support in printf to save space
add_definitions(-DPICO_FLOAT_SUPPORT_PRINTF=0)

# Define the executable
add_executable(main
    src/main.c
)
add_dependencies(main libneural_network)
target_link_libraries(main "${libneural_network_a}")
target_link_libraries(main hardware_spi)
target_link_libraries(main hardware_dma)

# Add pico_stdlib library for commonly used features
target_link_libraries(main pico_stdlib)

# Enable USB output, disable UART output
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)

# Create map/bin/hex/uf2 files in addition to ELF
pico_add_extra_outputs(main)

# Add a custom target to generate a memory map for analysis
add_custom_target(memory_map ALL
    COMMAND ${CMAKE_OBJDUMP} -h ${CMAKE_BINARY_DIR}/main.elf > ${CMAKE_BINARY_DIR}/memory_map.txt
    DEPENDS main
)