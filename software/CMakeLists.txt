cmake_minimum_required(VERSION 3.16)

# PICO_SDK_PATH setup - Keep these lines as they are
set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "Path to the Pico SDK")
list(APPEND CMAKE_MODULE_PATH "${PICO_SDK_PATH}/cmake")
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(can_bridge C CXX ASM) # Your project name
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# Add the rp2040_agrolib/software as a subdirectory.
# This ensures all its libraries (including rp_agrolib_can and its dependencies) are built.
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../rp2040_agrolib/software
    ${CMAKE_CURRENT_BINARY_DIR}/rp2040_agrolib_build
)
# Define your main executable.
# Now, src/main.cpp IS your application's source.
add_executable(can_bridge

                src/main.cpp    # This is your main application file

                src/CAN_bridge.cpp # The CAN bridge function definition
)

# Set common include directories for your project.
# You likely want your own `include` folder and any other shared headers here.
target_include_directories(can_bridge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include # Your project's own headers
)

pico_enable_stdio_usb(can_bridge 1)
pico_enable_stdio_uart(can_bridge 0)

pico_add_extra_outputs(can_bridge)

# Link libraries required by your main.cpp
target_link_libraries(can_bridge
    
        
    #CAN#  
    rp_agrolib_can

    #MULTICORE FUNCTIONS#
    pico_multicore

    #Default for pico#
    pico_stdlib
)