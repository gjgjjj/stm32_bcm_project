cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME BCM__Project)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
    BSP/Inc/bsp_gpio.h
    BSP/Inc/bsp_adc.h
    BSP/Inc/bsp_can.h
    BSP/Inc/bsp_uart.h
    BSP/Inc/bsp_rtc.h
    BSP/Inc/bsp_flash.h
    BSP/Inc/bsp_iwdg.h
    BSP/Inc/bsp_test.h

    OSAL/Inc/osal.h

    Service/Inc/key_manager.h
    Service/Inc/signal_filter.h
    Service/Inc/fault_manager.h
    Service/Inc/can_protocol.h
    Service/Inc/storage_manager.h
    Service/Inc/log_service.h
    Service/Inc/executor_diag.h
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    BSP/Src/bsp_gpio.c
    BSP/Src/bsp_adc.c
    BSP/Src/bsp_can.c
    BSP/Src/bsp_uart.c
    BSP/Src/bsp_rtc.c
    BSP/Src/bsp_flash.c
    BSP/Src/bsp_iwdg.c
    BSP/Src/bsp_test.c

    OSAL/Src/osal.c

    Service/Src/key_manager.c
    Service/Src/signal_filter.c
    Service/Src/fault_manager.c
    Service/Src/can_protocol.c
    Service/Src/storage_manager.c
    Service/Src/log_service.c
    Service/Src/executor_diag.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    BSP/Inc
    Service/Inc
    OSAL/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
