cmake_minimum_required(VERSION 3.6)

## Version Configuration
set(VERSION_MAJOR 00)
set(VERSION_MINOR 170)

## Release Type Selection
option(DH_DEBUG "Build a debug version" OFF)

## Hardware Configuration
set(DP_PIN_DEFAULT 4 CACHE STRING "Default USB D+ Pin Number")
set(PIO_USE_TINYUSB 1 CACHE STRING "Make TinyUSB Manage the PIO USB Port")
set(PICO_BOARD "pico")

## Pico SDK Configuration
set(PICO_SDK_FETCH_FROM_GIT off)
set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
include(${PICO_SDK_PATH}/pico_sdk_import.cmake)

## Project Setup
project(deskhop_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_COPY_TO_RAM 1)

## Compiler Flags
set(CMAKE_C_FLAGS "-Ofast -Wall -mcpu=cortex-m0plus -mtune=cortex-m0plus -fstack-usage")

## Initialize Pico SDK
pico_sdk_init()

# 注释掉或删除PIO USB相关部分
### PIO USB Library Setup
#set(PICO_PIO_USB_DIR ${CMAKE_CURRENT_LIST_DIR}/Pico-PIO-USB)
#
#add_library(Pico-PIO-USB STATIC
#  ${PICO_PIO_USB_DIR}/src/pio_usb.c
#  ${PICO_PIO_USB_DIR}/src/pio_usb_host.c
#  ${PICO_PIO_USB_DIR}/src/usb_crc.c
#)
#pico_generate_pio_header(Pico-PIO-USB ${PICO_PIO_USB_DIR}/src/usb_tx.pio)
#pico_generate_pio_header(Pico-PIO-USB ${PICO_PIO_USB_DIR}/src/usb_rx.pio)
#
#target_link_libraries(Pico-PIO-USB PRIVATE
#  pico_stdlib
#  pico_multicore
#  hardware_pio
#  hardware_dma
#)
#target_include_directories(Pico-PIO-USB PRIVATE ${PICO_PIO_USB_DIR})

## Source Files
# 修改源文件列表，移除USB相关文件
set(COMMON_SOURCES
  ${SRC_DIR}/defaults.c           # 默认配置
  ${SRC_DIR}/constants.c          # 常量定义
  ${SRC_DIR}/protocol.c           # 协议处理
  ${SRC_DIR}/utils.c              # 工具函数
  ${SRC_DIR}/handlers.c           # 处理函数
  ${SRC_DIR}/setup.c              # 初始化设置
  ${SRC_DIR}/keyboard.c           # 键盘处理
  ${SRC_DIR}/mouse.c              # 鼠标处理
  ${SRC_DIR}/tasks.c              # 任务管理
  ${SRC_DIR}/led.c                # LED控制
  ${SRC_DIR}/uart.c               # UART通信
  ${SRC_DIR}/main.c               # 主函数
  # 移除USB相关文件
  # ${SRC_DIR}/usb_descriptors.c
  # ${SRC_DIR}/hid_parser.c
  # ${SRC_DIR}/hid_report.c
  # ${SRC_DIR}/ramdisk.c
  # ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c
  # ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)


## Include Directories
set(COMMON_INCLUDES
  ${SRC_DIR}/include
  ${PICO_PIO_USB_DIR}/src
)

# 修改库依赖，移除USB和PIO相关库
set(COMMON_LINK_LIBRARIES
  pico_stdlib                     # Pico标准库
  hardware_flash                  # 闪存硬件库
  hardware_uart                   # UART硬件库
  hardware_gpio                   # GPIO硬件库
  # 移除PIO和USB相关库
  # hardware_pio
  # hardware_dma
  # tinyusb_device
  # tinyusb_host
  pico_multicore                  # 多核处理库
  pico_unique_id                  # 唯一ID库
  # Pico-PIO-USB
)
set(binary deskhop_ps2)

## Disk Image Configuration
# This assembles disk.S, then updates the elf section in post-build
# With the disk FAT image binary in /disk/disk.img 

set(DISK_ASM "${CMAKE_CURRENT_LIST_DIR}/disk/disk.S")
set(DISK_BIN "${CMAKE_CURRENT_LIST_DIR}/disk/disk.img")
set_property(SOURCE ${DISK_ASM} APPEND PROPERTY COMPILE_OPTIONS "-x" "assembler-with-cpp")

add_executable(${binary} ${DISK_ASM})

target_sources(${binary} PUBLIC ${COMMON_SOURCES})
target_compile_definitions(${binary} 
    PRIVATE 
    PIO_USB_USE_TINYUSB=${PIO_USE_TINYUSB}
    PIO_USB_DP_PIN_DEFAULT=${DP_PIN_DEFAULT}
    __disk_file_path__="${DISK_BIN}"
)

## Support building a debug version
if (DH_DEBUG)
  add_definitions(-DDH_DEBUG)  
endif()

# 启用UART1调试输出
add_definitions(-DENABLE_UART1_DEBUG=1)

target_include_directories(${binary} PUBLIC ${COMMON_INCLUDES})
target_link_libraries(${binary} PUBLIC ${COMMON_LINK_LIBRARIES})

## Configure Pico Library
pico_enable_stdio_usb(${binary} 0)
pico_enable_stdio_uart(${binary} 0)
pico_set_linker_script(${binary} ${CMAKE_SOURCE_DIR}/misc/memory_map.ld)

## Build other file formats as well
pico_add_extra_outputs(${binary})

## Post-Build Commands
add_custom_command(
    TARGET ${binary} POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/misc/crc32.py ${binary}.bin ${binary}.crc ${VERSION_MAJOR}${VERSION_MINOR}
    COMMAND ${CMAKE_OBJCOPY} --update-section .section_metadata=${binary}.crc ${binary}.elf
    COMMAND ${CMAKE_OBJCOPY} -O binary ${binary}.elf ${binary}.bin
    COMMAND ${CMAKE_BINARY_DIR}/elf2uf2/elf2uf2 ${binary}.elf ${binary}.uf2
    COMMENT "Update CRC32 section to match the actual binary"  
)

## Linker Options
target_link_options(${binary} PRIVATE
  -Xlinker
  --print-memory-usage
)

