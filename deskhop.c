# 文件: ../CMakeLists.txt

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

## PIO USB Library Setup
set(PICO_PIO_USB_DIR ${CMAKE_CURRENT_LIST_DIR}/Pico-PIO-USB)

add_library(Pico-PIO-USB STATIC
  ${PICO_PIO_USB_DIR}/src/pio_usb.c
  ${PICO_PIO_USB_DIR}/src/pio_usb_host.c
  ${PICO_PIO_USB_DIR}/src/usb_crc.c
)
pico_generate_pio_header(Pico-PIO-USB ${PICO_PIO_USB_DIR}/src/usb_tx.pio)
pico_generate_pio_header(Pico-PIO-USB ${PICO_PIO_USB_DIR}/src/usb_rx.pio)

target_link_libraries(Pico-PIO-USB PRIVATE
  pico_stdlib
  pico_multicore
  hardware_pio
  hardware_dma
)
target_include_directories(Pico-PIO-USB PRIVATE ${PICO_PIO_USB_DIR})

## Source Files
set(COMMON_SOURCES
  ${SRC_DIR}/usb_descriptors.c
  ${SRC_DIR}/defaults.c
  ${SRC_DIR}/constants.c
  ${SRC_DIR}/protocol.c
  ${SRC_DIR}/hid_parser.c
  ${SRC_DIR}/hid_report.c
  ${SRC_DIR}/utils.c
  ${SRC_DIR}/handlers.c
  ${SRC_DIR}/setup.c
  ${SRC_DIR}/keyboard.c
  ${SRC_DIR}/mouse.c
  ${SRC_DIR}/tasks.c
  ${SRC_DIR}/led.c
  ${SRC_DIR}/uart.c
  ${SRC_DIR}/usb.c
  ${SRC_DIR}/main.c
  ${SRC_DIR}/ramdisk.c
  ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c
  ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)

## Include Directories
set(COMMON_INCLUDES
  ${SRC_DIR}/include
  ${PICO_PIO_USB_DIR}/src
)

## Library Dependencies
set(COMMON_LINK_LIBRARIES
  pico_stdlib
  hardware_flash
  hardware_uart
  hardware_gpio
  hardware_pio
  hardware_dma

  tinyusb_device 
  tinyusb_host
  pico_multicore
  pico_unique_id
  Pico-PIO-USB
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


# 文件: constants.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#include "main.h"

/* CRC32 Lookup Table, Polynomial = 0xEDB88320 */
const uint32_t crc32_lookup_table[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

# 文件: defaults.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#include "main.h"

/* Default configuration */
const config_t default_config = {
    .magic_header = 0xB00B1E5,
    .version = CURRENT_CONFIG_VERSION,
    .output[OUTPUT_A] =
        {
            .number = OUTPUT_A,
            .speed_x = MOUSE_SPEED_A_FACTOR_X,
            .speed_y = MOUSE_SPEED_A_FACTOR_Y,
            .border = {
                .top = 0,
                .bottom = MAX_SCREEN_COORD,
            },
            .screen_count = 1,
            .screen_index = 1,
            .os = OUTPUT_A_OS,
            .pos = RIGHT,
            .screensaver = {
                .mode = SCREENSAVER_A_MODE,
                .only_if_inactive = SCREENSAVER_A_ONLY_IF_INACTIVE,
                .idle_time_us = (uint64_t)SCREENSAVER_A_IDLE_TIME_SEC * 1000000,
                .max_time_us = (uint64_t)SCREENSAVER_A_MAX_TIME_SEC * 1000000,
            }
        },
    .output[OUTPUT_B] =
        {
            .number = OUTPUT_B,
            .speed_x = MOUSE_SPEED_B_FACTOR_X,
            .speed_y = MOUSE_SPEED_B_FACTOR_Y,
            .border = {
                .top = 0,
                .bottom = MAX_SCREEN_COORD,
            },
            .screen_count = 1,
            .screen_index = 1,
            .os = OUTPUT_B_OS,
            .pos = LEFT,
            .screensaver = {
                .mode = SCREENSAVER_B_MODE,
                .only_if_inactive = SCREENSAVER_B_ONLY_IF_INACTIVE,
                .idle_time_us = (uint64_t)SCREENSAVER_B_IDLE_TIME_SEC * 1000000,
                .max_time_us = (uint64_t)SCREENSAVER_B_MAX_TIME_SEC * 1000000,
            }
        },
    .enforce_ports = ENFORCE_PORTS,
    .force_kbd_boot_protocol = ENFORCE_KEYBOARD_BOOT_PROTOCOL,
    .force_mouse_boot_mode = false,
    .enable_acceleration = ENABLE_ACCELERATION,
    .hotkey_toggle = HOTKEY_TOGGLE,
    .kbd_led_as_indicator = KBD_LED_AS_INDICATOR,
    .jump_threshold = JUMP_THRESHOLD,
};

# 文件: handlers.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#include "main.h"

/* =================================================== *
 * ============  Hotkey Handler Routines  ============ *
 * =================================================== */

/* This is the main hotkey for switching outputs */
void output_toggle_hotkey_handler(device_t *state, hid_keyboard_report_t *report) {
    /* If switching explicitly disabled, return immediately */
    if (state->switch_lock)
        return;

    state->active_output ^= 1;
    set_active_output(state, state->active_output);
};

void _get_border_position(device_t *state, border_size_t *border) {
    /* To avoid having 2 different keys, if we're above half, it's the top coord */
    if (state->pointer_y > (MAX_SCREEN_COORD / 2))
        border->bottom = state->pointer_y;
    else
        border->top = state->pointer_y;
}

void _screensaver_set(device_t *state, uint8_t value) {
    if (CURRENT_BOARD_IS_ACTIVE_OUTPUT)
        state->config.output[BOARD_ROLE].screensaver.mode = value;
    else
        send_value(value, SCREENSAVER_MSG);
};

/* This key combo records switch y top coordinate for different-size monitors  */
void screen_border_hotkey_handler(device_t *state, hid_keyboard_report_t *report) {
    border_size_t *border = &state->config.output[state->active_output].border;
    if (CURRENT_BOARD_IS_ACTIVE_OUTPUT) {
        _get_border_position(state, border);
        save_config(state);
    }

    queue_packet((uint8_t *)border, SYNC_BORDERS_MSG, sizeof(border_size_t));
};

/* This key combo puts board A in firmware upgrade mode */
void fw_upgrade_hotkey_handler_A(device_t *state, hid_keyboard_report_t *report) {
    reset_usb_boot(1 << PICO_DEFAULT_LED_PIN, 0);
};

/* This key combo puts board B in firmware upgrade mode */
void fw_upgrade_hotkey_handler_B(device_t *state, hid_keyboard_report_t *report) {
    send_value(ENABLE, FIRMWARE_UPGRADE_MSG);
};

/* This key combo prevents mouse from switching outputs */
void switchlock_hotkey_handler(device_t *state, hid_keyboard_report_t *report) {
    state->switch_lock ^= 1;
    send_value(state->switch_lock, SWITCH_LOCK_MSG);
}

/* This key combo toggles gaming mode */
void toggle_gaming_mode_handler(device_t *state, hid_keyboard_report_t *report) {
    state->gaming_mode ^= 1;
    send_value(state->gaming_mode, GAMING_MODE_MSG);
};

/* This key combo locks both outputs simultaneously */
void screenlock_hotkey_handler(device_t *state, hid_keyboard_report_t *report) {
    hid_keyboard_report_t lock_report = {0}, release_keys = {0};

    for (int out = 0; out < NUM_SCREENS; out++) {
        switch (state->config.output[out].os) {
            case WINDOWS:
            case LINUX:
                lock_report.modifier   = KEYBOARD_MODIFIER_LEFTGUI;
                lock_report.keycode[0] = HID_KEY_L;
                break;
            case MACOS:
                lock_report.modifier   = KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTGUI;
                lock_report.keycode[0] = HID_KEY_Q;
                break;
            default:
                break;
        }

        if (BOARD_ROLE == out) {
            queue_kbd_report(&lock_report, state);
            release_all_keys(state);
        } else {
            queue_packet((uint8_t *)&lock_report, KEYBOARD_REPORT_MSG, KBD_REPORT_LENGTH);
            queue_packet((uint8_t *)&release_keys, KEYBOARD_REPORT_MSG, KBD_REPORT_LENGTH);
        }
    }
}

/* When pressed, erases stored config in flash and loads defaults on both boards */
void wipe_config_hotkey_handler(device_t *state, hid_keyboard_report_t *report) {
    wipe_config();
    load_config(state);
    send_value(ENABLE, WIPE_CONFIG_MSG);
}

/* When pressed, toggles the current mouse zoom mode state */
void mouse_zoom_hotkey_handler(device_t *state, hid_keyboard_report_t *report) {
    state->mouse_zoom ^= 1;
    send_value(state->mouse_zoom, MOUSE_ZOOM_MSG);
};

/* When pressed, enables the screensaver on active output */
void enable_screensaver_hotkey_handler(device_t *state, hid_keyboard_report_t *report) {
    uint8_t desired_mode = state->config.output[BOARD_ROLE].screensaver.mode;

    /* If the user explicitly asks for screensaver to be active, ignore config and turn it on */
    if (desired_mode == DISABLED)
        desired_mode = PONG;

    _screensaver_set(state, desired_mode);
}

/* When pressed, disables the screensaver on active output */
void disable_screensaver_hotkey_handler(device_t *state, hid_keyboard_report_t *report) {
    _screensaver_set(state, DISABLED);
}

/* Put the device into a special configuration mode */
void config_enable_hotkey_handler(device_t *state, hid_keyboard_report_t *report) {
    /* If config mode is already active, skip this and reboot to return to normal mode */
    if (!state->config_mode_active) {
        watchdog_hw->scratch[5] = MAGIC_WORD_1;
        watchdog_hw->scratch[6] = MAGIC_WORD_2;
    }

    release_all_keys(state);
    state->reboot_requested = true;
};


/* ==================================================== *
 * ==========  UART Message Handling Routines  ======== *
 * ==================================================== */

/* Function handles received keypresses from the other board */
void handle_keyboard_uart_msg(uart_packet_t *packet, device_t *state) {
    hid_keyboard_report_t *report = (hid_keyboard_report_t *)packet->data;

    /* Update the keyboard state for the remote device (using MAX_DEVICES-1 as the index) */
    update_kbd_state(state, report, MAX_DEVICES-1);

    /* Create a combined report from all device states */
    hid_keyboard_report_t combined_report;
    combine_kbd_states(state, &combined_report);

    /* Queue the combined report */
    queue_kbd_report(&combined_report, state);
    state->last_activity[BOARD_ROLE] = time_us_64();
}

/* Function handles received mouse moves from the other board */
void handle_mouse_abs_uart_msg(uart_packet_t *packet, device_t *state) {
    mouse_report_t *mouse_report = (mouse_report_t *)packet->data;
    queue_mouse_report(mouse_report, state);

    state->pointer_x       = mouse_report->x;
    state->pointer_y       = mouse_report->y;
    state->mouse_buttons   = mouse_report->buttons;

    state->last_activity[BOARD_ROLE] = time_us_64();
}

/* Function handles request to switch output  */
void handle_output_select_msg(uart_packet_t *packet, device_t *state) {
    state->active_output = packet->data[0];
    if (state->tud_connected)
        release_all_keys(state);

    restore_leds(state);
}

/* On firmware upgrade message, reboot into the BOOTSEL fw upgrade mode */
void handle_fw_upgrade_msg(uart_packet_t *packet, device_t *state) {
    reset_usb_boot(1 << PICO_DEFAULT_LED_PIN, 0);
}

/* Comply with request to turn mouse zoom mode on/off  */
void handle_mouse_zoom_msg(uart_packet_t *packet, device_t *state) {
    state->mouse_zoom = packet->data[0];
}

/* Process request to update keyboard LEDs */
void handle_set_report_msg(uart_packet_t *packet, device_t *state) {
    /* We got this via serial, so it's stored to the opposite of our board role */
    state->keyboard_leds[OTHER_ROLE] = packet->data[0];

    /* If we have a keyboard we can control leds on, restore state if active */
    if (global_state.keyboard_connected && !CURRENT_BOARD_IS_ACTIVE_OUTPUT)
        restore_leds(state);
}

/* Process request to block mouse from switching, update internal state */
void handle_switch_lock_msg(uart_packet_t *packet, device_t *state) {
    state->switch_lock = packet->data[0];
}

/* Handle border syncing message that lets the other device know about monitor height offset */
void handle_sync_borders_msg(uart_packet_t *packet, device_t *state) {
    border_size_t *border = &state->config.output[state->active_output].border;

    if (CURRENT_BOARD_IS_ACTIVE_OUTPUT) {
        _get_border_position(state, border);
        queue_packet((uint8_t *)border, SYNC_BORDERS_MSG, sizeof(border_size_t));
    } else
        memcpy(border, packet->data, sizeof(border_size_t));

    save_config(state);
}

/* When this message is received, flash the locally attached LED to verify serial comms */
void handle_flash_led_msg(uart_packet_t *packet, device_t *state) {
    blink_led(state);
}

/* When this message is received, wipe the local flash config */
void handle_wipe_config_msg(uart_packet_t *packet, device_t *state) {
    wipe_config();
    load_config(state);
}

/* Update screensaver state after received message */
void handle_screensaver_msg(uart_packet_t *packet, device_t *state) {
    state->config.output[BOARD_ROLE].screensaver.mode = packet->data[0];
}

/* Process consumer control message */
void handle_consumer_control_msg(uart_packet_t *packet, device_t *state) {
    queue_cc_packet(packet->data, state);
}

/* Process request to store config to flash */
void handle_save_config_msg(uart_packet_t *packet, device_t *state) {
    save_config(state);
}

/* Process request to reboot the board */
void handle_reboot_msg(uart_packet_t *packet, device_t *state) {
    reboot();
}

/* Decapsulate and send to the other box */
void handle_proxy_msg(uart_packet_t *packet, device_t *state) {
    queue_packet(&packet->data[1], (enum packet_type_e)packet->data[0], PACKET_DATA_LENGTH - 1);
}

/* Process relative mouse command */
void handle_toggle_gaming_msg(uart_packet_t *packet, device_t *state) {
    state->gaming_mode = packet->data[0];
}

/* Process api communication messages */
void handle_api_msgs(uart_packet_t *packet, device_t *state) {
    uint8_t value_idx = packet->data[0];
    const field_map_t *map = get_field_map_entry(value_idx);

    /* If we don't have a valid map entry, return immediately */
    if (map == NULL)
        return;

    /* Create a pointer to the offset into the structure we need to access */
    uint8_t *ptr = (((uint8_t *)&global_state) + map->offset);

    if (packet->type == SET_VAL_MSG) {
        /* Not allowing writes to objects defined as read-only */
        if (map->readonly)
            return;

        memcpy(ptr, &packet->data[1], map->len);
    }
    else if (packet->type == GET_VAL_MSG) {
        uart_packet_t response = {.type=GET_VAL_MSG, .data={[0] = value_idx}};
        memcpy(&response.data[1], ptr, map->len);
        queue_cfg_packet(&response, state);
    }

    /* With each GET/SET message, we reset the configuration mode timeout */
    reset_config_timer(state);
}

/* Handle the "read all" message by calling our "read one" handler for each type */
void handle_api_read_all_msg(uart_packet_t *packet, device_t *state) {
    uart_packet_t result = {.type=GET_VAL_MSG};

    for (int i = 0; i < get_field_map_length(); i++) {
        result.data[0] = get_field_map_index(i)->idx;
        handle_api_msgs(&result, state);
    }
}

/* Process request packet and create a response */
void handle_request_byte_msg(uart_packet_t *packet, device_t *state) {
    uint32_t address = packet->data32[0];

    if (address > STAGING_IMAGE_SIZE)
        return;

    /* Add requested data to bytes 4-7 in the packet and return it with a different type */
    uint32_t data = *(uint32_t *)&ADDR_FW_RUNNING[address];
    packet->data32[1] = data;

    queue_packet(packet->data, RESPONSE_BYTE_MSG, PACKET_DATA_LENGTH);
}

/* Process response message following a request we sent to read a byte */
/* state->page_offset and state->page_number are kept locally and compared to returned values */
void handle_response_byte_msg(uart_packet_t *packet, device_t *state) {
    uint16_t offset = packet->data[0];
    uint32_t address = packet->data32[0];

    if (address != state->fw.address) {
        state->fw.upgrade_in_progress = false;
        state->fw.address = 0;
        return;
    }
    else {
        /* Provide visual feedback of the ongoing copy by toggling LED for every sector */
        if((address & 0xfff) == 0x000)
            toggle_led();
    }

    /* Update checksum as we receive each byte */
    if (address < STAGING_IMAGE_SIZE - FLASH_SECTOR_SIZE)
        for (int i=0; i<4; i++)
            state->fw.checksum = crc32_iter(state->fw.checksum, packet->data[4 + i]);

    memcpy(state->page_buffer + offset, &packet->data32[1], sizeof(uint32_t));

    /* Neeeeeeext byte, please! */
    state->fw.address += sizeof(uint32_t);
    state->fw.byte_done = true;
}

/* Process a request to read a firmware package from flash */
void handle_heartbeat_msg(uart_packet_t *packet, device_t *state) {
    uint16_t other_running_version = packet->data16[0];

    if (state->fw.upgrade_in_progress)
        return;

    /* If the other board isn't running a newer version, we are done */
    if (other_running_version <= state->_running_fw.version)
        return;

    /* It is? Ok, kick off the firmware upgrade */
    state->fw = (fw_upgrade_state_t) {
        .upgrade_in_progress = true,
        .byte_done = true,
        .address = 0,
        .checksum = 0xffffffff,
    };
}


/* ==================================================== *
 * ==============  Output Switch Routines  ============ *
 * ==================================================== */

/* Update output variable, set LED on/off and notify the other board so they are in sync. */
void set_active_output(device_t *state, uint8_t new_output) {
    state->active_output = new_output;
    restore_leds(state);
    send_value(new_output, OUTPUT_SELECT_MSG);

    /* If we were holding a key down and drag the mouse to another screen, the key gets stuck.
       Changing outputs = no more keypresses on the previous system. */
    release_all_keys(state);
}

# 文件: hid_parser.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * Based on the TinyUSB HID parser routine and the amazing USB2N64
 * adapter (https://github.com/pdaxrom/usb2n64-adapter)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#include "main.h"

#define IS_BLOCK_END (parser->collection.start == parser->collection.end)

enum { SIZE_0_BIT = 0, SIZE_8_BIT = 1, SIZE_16_BIT = 2, SIZE_32_BIT = 3 };
const uint8_t SIZE_LOOKUP[4] = {0, 1, 2, 4};

/* Size is 0, 1, 2, or 3, describing cases of no data, 8-bit, 16-bit,
  or 32-bit data. */
uint32_t get_descriptor_value(uint8_t const *report, int size) {
    switch (size) {
        case SIZE_8_BIT:
            return report[0];
        case SIZE_16_BIT:
            return tu_u16(report[1], report[0]);
        case SIZE_32_BIT:
            return tu_u32(report[3], report[2], report[1], report[0]);
        default:
            return 0;
    }
}

void update_usage(parser_state_t *parser, int i) {
    /* If we don't have as many usages as elements, the usage for the previous element applies */
    if (i > 0 && i >= parser->usage_count && i < HID_MAX_USAGES)
        *(parser->p_usage + i) = *(parser->p_usage + i - 1);
}

void store_element(parser_state_t *parser, report_val_t *val, int i, uint32_t data, uint16_t size, hid_interface_t *iface) {
    *val = (report_val_t){
        .offset     = parser->offset_in_bits,
        .offset_idx = parser->offset_in_bits >> 3,
        .size       = size,

        .usage_max = parser->locals[RI_LOCAL_USAGE_MAX].val,
        .usage_min = parser->locals[RI_LOCAL_USAGE_MIN].val,

        .item_type   = (data & 0x01) ? CONSTANT : DATA,
        .data_type   = (data & 0x02) ? VARIABLE : ARRAY,

        .usage        = *(parser->p_usage + i),
        .usage_page   = parser->globals[RI_GLOBAL_USAGE_PAGE].val,
        .global_usage = parser->global_usage,
        .report_id    = parser->report_id
    };

    iface->uses_report_id |= (parser->report_id != 0);
}

void handle_global_item(parser_state_t *parser, item_t *item) {
    if (item->hdr.tag == RI_GLOBAL_REPORT_ID) {
        /* Reset offset for a new page */
        parser->offset_in_bits = 0;
        parser->report_id = item->val;
    }

    parser->globals[item->hdr.tag] = *item;
}

void handle_local_item(parser_state_t *parser, item_t *item) {
    /* There are just 16 possible tags, store any one that comes along to an array
        instead of doing switch and 16 cases */
    parser->locals[item->hdr.tag] = *item;

    if (item->hdr.tag == RI_LOCAL_USAGE) {
        if(IS_BLOCK_END)
            parser->global_usage = item->val;

        else if (parser->usage_count < HID_MAX_USAGES - 1)
            *(parser->p_usage + parser->usage_count++) = item->val;
    }
}

void handle_main_input(parser_state_t *parser, item_t *item, hid_interface_t *iface) {
    uint32_t size  = parser->globals[RI_GLOBAL_REPORT_SIZE].val;
    uint32_t count = parser->globals[RI_GLOBAL_REPORT_COUNT].val;
    report_val_t val = {0};

    /* Swap count and size for 1-bit variables, it makes sense to process e.g. NKRO with
       size = 1 and count = 240 in one go instead of doing 240 iterations
       Don't do this if there are usages in the queue, though.
       */
    if (size == 1 && parser->usage_count <= 1) {
        size  = count;
        count = 1;
    }

    for (int i = 0; i < count; i++) {
        update_usage(parser, i);
        store_element(parser, &val, i, item->val, size, iface);

        /* Use the parsed data to populate internal device structures */
        extract_data(iface, &val);

        /* Iterate <count> times and increase offset by <size> amount, moving by <count> x <size> bits */
        parser->offset_in_bits += size;
    }

    /* Advance the usage array pointer by global report count and reset the count variable */
    parser->p_usage += parser->usage_count;

    /* Carry the last usage to the new location */
    *parser->p_usage = *(parser->p_usage - parser->usage_count);
}

void handle_main_item(parser_state_t *parser, item_t *item, hid_interface_t *iface) {
    if (IS_BLOCK_END)
        parser->offset_in_bits = 0;

    switch (item->hdr.tag) {
        case RI_MAIN_COLLECTION:
            parser->collection.start++;
            break;

        case RI_MAIN_COLLECTION_END:
            parser->collection.end++;
            break;

        case RI_MAIN_INPUT:
            handle_main_input(parser, item, iface);
            break;
    }

    parser->usage_count = 0;

    /* Local items do not carry over to the next Main item (HID spec v1.11, section 6.2.2.8) */
    memset(parser->locals, 0, sizeof(parser->locals));
}


/* This method is sub-optimal and far from a generalized HID descriptor parsing, but should
 * hopefully work well enough to find the basic values we care about to move the mouse around.
 * Your descriptor for a mouse with 2 wheels and 264 buttons might not parse correctly.
 * */
parser_state_t parser_state = {0};  // Avoid placing it on the stack, it's large

void parse_report_descriptor(hid_interface_t *iface,
                            uint8_t const *report,
                            int desc_len
                            ) {
    item_t item = {0};

    /* Wipe parser_state clean */
    memset(&parser_state, 0, sizeof(parser_state_t));
    parser_state.p_usage = parser_state.usages;

    while (desc_len > 0) {
        item.hdr = *(header_t *)report++;
        item.val = get_descriptor_value(report, item.hdr.size);

        switch (item.hdr.type) {
            case RI_TYPE_MAIN:
                handle_main_item(&parser_state, &item, iface);
                break;

            case RI_TYPE_GLOBAL:
                handle_global_item(&parser_state, &item);
                break;

            case RI_TYPE_LOCAL:
                handle_local_item(&parser_state, &item);
                break;
        }
        /* Move to the next position and decrement size by header length + data length */
        report += SIZE_LOOKUP[item.hdr.size];
        desc_len -= (SIZE_LOOKUP[item.hdr.size] + 1);
    }
}

# 文件: hid_report.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#include "hid_report.h"
#include "main.h"

/* Given a value struct with size and offset in bits, find and return a value from the HID report */
int32_t get_report_value(uint8_t *report, int len, report_val_t *val) {
    /* Calculate the bit offset within the byte */
    uint16_t offset_in_bits = val->offset % 8;

    /* Calculate the remaining bits in the first byte */
    uint16_t remaining_bits = 8 - offset_in_bits;

    /* Calculate the byte offset in the array */
    uint16_t byte_offset = val->offset >> 3;

    if (byte_offset >= len)
        return 0;

    /* Create a mask for the specified number of bits */
    uint32_t mask = (1u << val->size) - 1;

    /* Initialize the result value with the bits from the first byte */
    int32_t result = report[byte_offset] >> offset_in_bits;

    /* Move to the next byte and continue fetching bits until the desired length is reached */
    while (val->size > remaining_bits) {
        result |= report[++byte_offset] << remaining_bits;
        remaining_bits += 8;
    }

    /* Apply the mask to retain only the desired number of bits */
    result = result & mask;

    /* Special case if our result is negative.
       Check if the most significant bit of 'val' is set */
    if (result & ((mask >> 1) + 1)) {
        /* If it is set, sign-extend 'val' by filling the higher bits with 1s */
        result |= (0xFFFFFFFFU << val->size);
    }

    return result;
}

/* After processing the descriptor, assign the values so we can later use them to interpret reports */
void handle_consumer_control_values(report_val_t *src, report_val_t *dst, hid_interface_t *iface) {
    if (src->offset > MAX_CC_BUTTONS) {
        return;
    }

    if (src->data_type == VARIABLE) {
        iface->keyboard.cc_array[src->offset] = src->usage;
        iface->consumer.is_variable = true;
    }

    iface->consumer.is_array |= (src->data_type == ARRAY);
}

/* After processing the descriptor, assign the values so we can later use them to interpret reports */
void handle_system_control_values(report_val_t *src, report_val_t *dst, hid_interface_t *iface) {
    if (src->offset > MAX_SYS_BUTTONS) {
        return;
    }

    if (src->data_type == VARIABLE) {
        iface->keyboard.sys_array[src->offset] = src->usage;
        iface->system.is_variable = true;
    }

    iface->system.is_array |= (src->data_type == ARRAY);
}

/* After processing the descriptor, assign the values so we can later use them to interpret reports */
void handle_keyboard_descriptor_values(report_val_t *src, report_val_t *dst, hid_interface_t *iface) {
    const int LEFT_CTRL = 0xE0;

    /* Constants are normally used for padding, so skip'em */
    if (src->item_type == CONSTANT)
        return;

    /* Detect and handle modifier keys. <= if modifier is less + constant padding? */
    if (src->size <= MODIFIER_BIT_LENGTH && src->data_type == VARIABLE) {
        /* To make sure this really is the modifier key, we expect e.g. left control to be
           within the usage interval */
        if (LEFT_CTRL >= src->usage_min && LEFT_CTRL <= src->usage_max)
            iface->keyboard.modifier = *src;
    }

    /* If we have an array member, that's most likely a key (0x00 - 0xFF, 1 byte) */
    if (src->offset_idx < MAX_KEYS) {
        iface->keyboard.key_array[src->offset_idx] = (src->data_type == ARRAY);
    }

    /* Handle NKRO, normally size = 1, count = 240 or so, but they are swapped. */
    if (src->size > 32 && src->data_type == VARIABLE) {
        iface->keyboard.is_nkro = true;
        iface->keyboard.nkro    = *src;
    }

    /* We found a keyboard on this interface. */
    iface->keyboard.is_found = true;
}

void handle_buttons(report_val_t *src, report_val_t *dst, hid_interface_t *iface) {
    /* Constant is normally used for padding with mouse buttons, aggregate to simplify things */
    if (src->item_type == CONSTANT) {
        iface->mouse.buttons.size += src->size;
        return;
    }

    iface->mouse.buttons = *src;

    /* We found a mouse on this interface. */
    iface->mouse.is_found = true;
}

void _store(report_val_t *src, report_val_t *dst, hid_interface_t *iface) {
    if (src->item_type != CONSTANT)
        *dst = *src;
}


void extract_data(hid_interface_t *iface, report_val_t *val) {
    const usage_map_t map[] = {
        {.usage_page   = HID_USAGE_PAGE_BUTTON,
         .global_usage = HID_USAGE_DESKTOP_MOUSE,
         .handler      = handle_buttons,
         .receiver     = process_mouse_report,
         .dst          = &iface->mouse.buttons,
         .id           = &iface->mouse.report_id},

        {.usage_page   = HID_USAGE_PAGE_DESKTOP,
         .global_usage = HID_USAGE_DESKTOP_MOUSE,
         .usage        = HID_USAGE_DESKTOP_X,
         .handler      = _store,
         .receiver     = process_mouse_report,
         .dst          = &iface->mouse.move_x,
         .id           = &iface->mouse.report_id},

        {.usage_page   = HID_USAGE_PAGE_DESKTOP,
         .global_usage = HID_USAGE_DESKTOP_MOUSE,
         .usage        = HID_USAGE_DESKTOP_Y,
         .handler      = _store,
         .receiver     = process_mouse_report,
         .dst          = &iface->mouse.move_y,
         .id           = &iface->mouse.report_id},

        {.usage_page   = HID_USAGE_PAGE_DESKTOP,
         .global_usage = HID_USAGE_DESKTOP_MOUSE,
         .usage        = HID_USAGE_DESKTOP_WHEEL,
         .handler      = _store,
         .receiver     = process_mouse_report,
         .dst          = &iface->mouse.wheel,
         .id           = &iface->mouse.report_id},

        {.usage_page   = HID_USAGE_PAGE_CONSUMER,
         .global_usage = HID_USAGE_DESKTOP_MOUSE,
         .usage        = HID_USAGE_CONSUMER_AC_PAN,
         .handler      = _store,
         .receiver     = process_mouse_report,
         .dst          = &iface->mouse.pan,
         .id           = &iface->mouse.report_id},

        {.usage_page   = HID_USAGE_PAGE_KEYBOARD,
         .global_usage = HID_USAGE_DESKTOP_KEYBOARD,
         .handler      = handle_keyboard_descriptor_values,
         .receiver     = process_keyboard_report,
         .id           = &iface->keyboard.report_id},

        {.usage_page   = HID_USAGE_PAGE_CONSUMER,
         .global_usage = HID_USAGE_CONSUMER_CONTROL,
         .handler      = handle_consumer_control_values,
         .receiver     = process_consumer_report,
         .dst          = &iface->consumer.val,
         .id           = &iface->consumer.report_id},

        {.usage_page   = HID_USAGE_PAGE_DESKTOP,
         .global_usage = HID_USAGE_DESKTOP_SYSTEM_CONTROL,
         .handler      = _store,
         .receiver     = process_system_report,
         .dst          = &iface->system.val,
         .id           = &iface->system.report_id},
    };

    /* We extracted all we could find in the descriptor to report_values, now go through them and
       match them up with the values in the table above, then store those values for later reference */

    for (const usage_map_t *hay = map; hay != &map[ARRAY_SIZE(map)]; hay++) {
        /* ---> If any condition is not defined, we consider it as matched <--- */
        bool global_usages_match = (val->global_usage == hay->global_usage) || (hay->global_usage == 0);
        bool usages_match        = (val->usage == hay->usage) || (hay->usage == 0);
        bool usage_pages_match   = (val->usage_page == hay->usage_page) || (hay->usage_page == 0);

        if (global_usages_match && usages_match && usage_pages_match) {
            hay->handler(val, hay->dst, iface);
            *hay->id = val->report_id;

            if (val->report_id < MAX_REPORTS)
                iface->report_handler[val->report_id] = hay->receiver;
        }
    }
}

int32_t extract_bit_variable(report_val_t *kbd, uint8_t *raw_report, int len, uint8_t *dst) {
    int key_count = 0;
    int bit_offset = kbd->offset & 0b111;

    for (int i = kbd->usage_min, j = bit_offset; i <= kbd->usage_max && key_count < len; i++, j++) {
        int byte_index = j >> 3;
        int bit_index  = j & 0b111;

        if (raw_report[byte_index] & (1 << bit_index)) {
            dst[key_count++] = i;
        }
    }

    return key_count;
}

int32_t _extract_kbd_boot(uint8_t *raw_report, int len, hid_keyboard_report_t *report) {
    uint8_t *src = raw_report;

    /* In case keyboard still uses report ID in this, just pick the last 8 bytes */
    if (len == KBD_REPORT_LENGTH + 1)
        src++;

    memcpy(report, src, KBD_REPORT_LENGTH);
    return KBD_REPORT_LENGTH;
}

int32_t _extract_kbd_other(uint8_t *raw_report, int len, hid_interface_t *iface, hid_keyboard_report_t *report) {
    uint8_t *src = raw_report;
    keyboard_t *kb = &iface->keyboard;

    if (iface->uses_report_id)
        src++;

    report->modifier = src[kb->modifier.offset_idx];
    for (int i=0, j=0; i < MAX_KEYS && j < KEYS_IN_USB_REPORT; i++) {
        if(kb->key_array[i])
            report->keycode[j++] = src[i];
    }

    return KBD_REPORT_LENGTH;
}

int32_t _extract_kbd_nkro(uint8_t *raw_report, int len, hid_interface_t *iface, hid_keyboard_report_t *report) {
    uint8_t *ptr = raw_report;
    keyboard_t *kb = &iface->keyboard;

    /* Skip report ID */
    if (iface->uses_report_id)
        ptr++;

    /* We expect array of bits mapping 1:1 from usage_min to usage_max, otherwise panic */
    if ((kb->nkro.usage_max - kb->nkro.usage_min + 1) != kb->nkro.size)
        return -1;

    /* We expect modifier to be 8 bits long, otherwise we'll fallback to boot mode */
    if (kb->modifier.size == MODIFIER_BIT_LENGTH) {
        report->modifier = ptr[kb->modifier.offset_idx];
    } else
        return -1;

    /* Move the pointer to the nkro offset's byte index */
    ptr = &ptr[kb->nkro.offset_idx];

    return extract_bit_variable(&kb->nkro, ptr, KEYS_IN_USB_REPORT, report->keycode);
}

int32_t extract_kbd_data(
    uint8_t *raw_report, int len, uint8_t itf, hid_interface_t *iface, hid_keyboard_report_t *report) {

    /* Clear the report to start fresh */
    memset(report, 0, KBD_REPORT_LENGTH);

    /* If we're in boot protocol mode, then it's easy to decide. */
    if (iface->protocol == HID_PROTOCOL_BOOT)
        return _extract_kbd_boot(raw_report, len, report);

    /* NKRO is a special case */
    if (iface->keyboard.is_nkro)
        return _extract_kbd_nkro(raw_report, len, iface, report);

    /* If we're getting 8 bytes of report, it's safe to assume standard modifier + reserved + keys */
    if (len == KBD_REPORT_LENGTH || len == KBD_REPORT_LENGTH + 1)
        return _extract_kbd_boot(raw_report, len, report);

    /* This is something completely different, look at the report  */
    return _extract_kbd_other(raw_report, len, iface, report);
}

# 文件: include


# 文件: keyboard.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#include "main.h"

/* ==================================================== *
 * Hotkeys to trigger actions via the keyboard.
 * ==================================================== */

hotkey_combo_t hotkeys[] = {
    /* Main keyboard switching hotkey */
    {.modifier       = HOTKEY_MODIFIER,
     .keys           = {HOTKEY_TOGGLE},
     .key_count      = 1,
     .pass_to_os     = false,
     .action_handler = &output_toggle_hotkey_handler},

    /* Pressing right ALT + right CTRL toggles the slow mouse mode */
    {.modifier       = KEYBOARD_MODIFIER_RIGHTALT | KEYBOARD_MODIFIER_RIGHTCTRL,
     .keys           = {},
     .key_count      = 0,
     .pass_to_os     = true,
     .acknowledge    = true,
     .action_handler = &mouse_zoom_hotkey_handler},

    /* Switch lock */
    {.modifier       = KEYBOARD_MODIFIER_RIGHTCTRL,
     .keys           = {HID_KEY_K},
     .key_count      = 1,
     .acknowledge    = true,
     .action_handler = &switchlock_hotkey_handler},

    /* Screen lock */
    {.modifier       = KEYBOARD_MODIFIER_RIGHTCTRL,
     .keys           = {HID_KEY_L},
     .key_count      = 1,
     .acknowledge    = true,
     .action_handler = &screenlock_hotkey_handler},

    /* Toggle gaming mode */
    {.modifier       = KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTSHIFT,
     .keys           = {HID_KEY_G},
     .key_count      = 1,
     .acknowledge    = true,
     .action_handler = &toggle_gaming_mode_handler},

    /* Enable screensaver for active output */
    {.modifier       = KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTSHIFT,
     .keys           = {HID_KEY_S},
     .key_count      = 1,
     .acknowledge    = true,
     .action_handler = &enable_screensaver_hotkey_handler},

    /* Disable screensaver for active output */
    {.modifier       = KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTSHIFT,
     .keys           = {HID_KEY_X},
     .key_count      = 1,
     .acknowledge    = true,
     .action_handler = &disable_screensaver_hotkey_handler},

    /* Erase stored config */
    {.modifier       = KEYBOARD_MODIFIER_RIGHTSHIFT,
     .keys           = {HID_KEY_F12, HID_KEY_D},
     .key_count      = 2,
     .acknowledge    = true,
     .action_handler = &wipe_config_hotkey_handler},

    /* Record switch y coordinate  */
    {.modifier       = KEYBOARD_MODIFIER_RIGHTSHIFT,
     .keys           = {HID_KEY_F12, HID_KEY_Y},
     .key_count      = 2,
     .acknowledge    = true,
     .action_handler = &screen_border_hotkey_handler},

    /* Switch to configuration mode  */
    {.modifier       = KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTSHIFT,
     .keys           = {HID_KEY_C, HID_KEY_O},
     .key_count      = 2,
     .acknowledge    = true,
     .action_handler = &config_enable_hotkey_handler},

    /* Hold down left shift + right shift + F12 + A ==> firmware upgrade mode for board A (kbd) */
    {.modifier       = KEYBOARD_MODIFIER_RIGHTSHIFT | KEYBOARD_MODIFIER_LEFTSHIFT,
     .keys           = {HID_KEY_A},
     .key_count      = 1,
     .acknowledge    = true,
     .action_handler = &fw_upgrade_hotkey_handler_A},

    /* Hold down left shift + right shift + F12 + B ==> firmware upgrade mode for board B (mouse) */
    {.modifier       = KEYBOARD_MODIFIER_RIGHTSHIFT | KEYBOARD_MODIFIER_LEFTSHIFT,
     .keys           = {HID_KEY_B},
     .key_count      = 1,
     .acknowledge    = true,
     .action_handler = &fw_upgrade_hotkey_handler_B}};

/* ============================================================ *
 * Detect if any hotkeys were pressed
 * ============================================================ */

/* Tries to find if the keyboard report contains key, returns true/false */
bool key_in_report(uint8_t key, const hid_keyboard_report_t *report) {
    for (int j = 0; j < KEYS_IN_USB_REPORT; j++) {
        if (key == report->keycode[j]) {
            return true;
        }
    }

    return false;
}

/* Check if the current report matches a specific hotkey passed on */
bool check_specific_hotkey(hotkey_combo_t keypress, const hid_keyboard_report_t *report) {
    /* We expect all modifiers specified to be detected in the report */
    if (keypress.modifier != (report->modifier & keypress.modifier))
        return false;

    for (int n = 0; n < keypress.key_count; n++) {
        if (!key_in_report(keypress.keys[n], report)) {
            return false;
        }
    }

    /* Getting here means all of the keys were found. */
    return true;
}

/* Go through the list of hotkeys, check if any of them match. */
hotkey_combo_t *check_all_hotkeys(hid_keyboard_report_t *report, device_t *state) {
    for (int n = 0; n < ARRAY_SIZE(hotkeys); n++) {
        if (check_specific_hotkey(hotkeys[n], report)) {
            return &hotkeys[n];
        }
    }

    return NULL;
}

/* ==================================================== *
 * Keyboard State Management
 * ==================================================== */

/* Update the keyboard state for a specific device */
void update_kbd_state(device_t *state, hid_keyboard_report_t *report, uint8_t device_idx) {
    /* Ensure device_idx is within bounds */
    if (device_idx >= MAX_DEVICES)
        return;

    /* Ensure local devices never use the last slot, which is reserved for the remote device */
    if (device_idx == MAX_DEVICES-1 && device_idx != 0) {
        /* Use the previous slot instead */
        device_idx = MAX_DEVICES-2;
    }

    /* Update the keyboard state for this device */
    memcpy(&state->kbd_states[device_idx], report, sizeof(hid_keyboard_report_t));

    /* Ensure kbd_device_count is at least device_idx + 1 */
    if (state->kbd_device_count <= device_idx)
        state->kbd_device_count = device_idx + 1;
}

/* Combine keyboard states from all devices into a single report */
void combine_kbd_states(device_t *state, hid_keyboard_report_t *combined_report) {
    /* Initialize combined report */
    memset(combined_report, 0, sizeof(hid_keyboard_report_t));

    /* Combine modifiers and keys from all devices */
    for (uint8_t i = 0; i < state->kbd_device_count; i++) {
        /* Combine modifiers with OR operation */
        combined_report->modifier |= state->kbd_states[i].modifier;

        /* Add keys from this device to the combined report */
        for (uint8_t j = 0; j < KEYS_IN_USB_REPORT; j++) {
            if (state->kbd_states[i].keycode[j] != 0) {
                /* Find an empty slot in the combined report */
                for (uint8_t k = 0; k < KEYS_IN_USB_REPORT; k++) {
                    if (combined_report->keycode[k] == 0) {
                        combined_report->keycode[k] = state->kbd_states[i].keycode[j];
                        break;
                    }
                }
            }
        }
    }
}

/* ==================================================== *
 * Keyboard Queue Section
 * ==================================================== */

void process_kbd_queue_task(device_t *state) {
    hid_keyboard_report_t report;

    /* If we're not connected, we have nowhere to send reports to. */
    if (!state->tud_connected)
        return;

    /* Peek first, if there is anything there... */
    if (!queue_try_peek(&state->kbd_queue, &report))
        return;

    /* If we are suspended, let's wake the host up */
    if (tud_suspended())
        tud_remote_wakeup();

    /* If it's not ok to send yet, we'll try on the next pass */
    if (!tud_hid_n_ready(ITF_NUM_HID))
        return;

    /* ... try sending it to the host, if it's successful */
    bool succeeded = tud_hid_keyboard_report(REPORT_ID_KEYBOARD, report.modifier, report.keycode);

    /* ... then we can remove it from the queue. Race conditions shouldn't happen [tm] */
    if (succeeded)
        queue_try_remove(&state->kbd_queue, &report);
}

void queue_kbd_report(hid_keyboard_report_t *report, device_t *state) {
    /* It wouldn't be fun to queue up a bunch of messages and then dump them all on host */
    if (!state->tud_connected)
        return;

    queue_try_add(&state->kbd_queue, report);
}

void release_all_keys(device_t *state) {
    static hid_keyboard_report_t no_keys_pressed_report = {0, 0, {0}};

    /* Clear keyboard states for all devices */
    for (uint8_t i = 0; i < state->kbd_device_count; i++) {
        memset(&state->kbd_states[i], 0, sizeof(hid_keyboard_report_t));
    }

    /* Send a report with no keys pressed */
    queue_try_add(&state->kbd_queue, &no_keys_pressed_report);
}

/* If keys need to go locally, queue packet to kbd queue, else send them through UART */
void send_key(hid_keyboard_report_t *report, device_t *state) {
    /* Create a combined report from all device states */
    hid_keyboard_report_t combined_report;
    combine_kbd_states(state, &combined_report);

    if (CURRENT_BOARD_IS_ACTIVE_OUTPUT) {
        /* Queue the combined report */
        queue_kbd_report(&combined_report, state);
        state->last_activity[BOARD_ROLE] = time_us_64();
    } else {
        /* Send the combined report to ensure all keys are included */
        queue_packet((uint8_t *)&combined_report, KEYBOARD_REPORT_MSG, KBD_REPORT_LENGTH);
    }
}

/* Decide if consumer control reports go local or to the other board */
void send_consumer_control(uint8_t *raw_report, device_t *state) {
    if (CURRENT_BOARD_IS_ACTIVE_OUTPUT) {
        queue_cc_packet(raw_report, state);
        state->last_activity[BOARD_ROLE] = time_us_64();
    } else {
        queue_packet((uint8_t *)raw_report, CONSUMER_CONTROL_MSG, CONSUMER_CONTROL_LENGTH);
    }
}

/* Decide if consumer control reports go local or to the other board */
void send_system_control(uint8_t *raw_report, device_t *state) {
    if (CURRENT_BOARD_IS_ACTIVE_OUTPUT) {
        queue_system_packet(raw_report, state);
        state->last_activity[BOARD_ROLE] = time_us_64();
    } else {
        queue_packet((uint8_t *)raw_report, SYSTEM_CONTROL_MSG, SYSTEM_CONTROL_LENGTH);
    }
}

/* ==================================================== *
 * Parse and interpret the keys pressed on the keyboard
 * ==================================================== */

void process_keyboard_report(uint8_t *raw_report, int length, uint8_t itf, hid_interface_t *iface) {
    hid_keyboard_report_t new_report = {0};
    device_t *state                  = &global_state;
    hotkey_combo_t *hotkey           = NULL;

    if (length < KBD_REPORT_LENGTH)
        return;

    /* No more keys accepted if we're about to reboot */
    if (global_state.reboot_requested)
        return;

    extract_kbd_data(raw_report, length, itf, iface, &new_report);

    /* Update the keyboard state for this device */
    update_kbd_state(state, &new_report, itf);

    /* Check if any hotkey was pressed */
    hotkey = check_all_hotkeys(&new_report, state);

    /* ... and take appropriate action */
    if (hotkey != NULL) {
        /* Provide visual feedback we received the action */
        if (hotkey->acknowledge)
            blink_led(state);

        /* Execute the corresponding handler */
        hotkey->action_handler(state, &new_report);

        /* And pass the key to the output PC if configured to do so. */
        if (!hotkey->pass_to_os)
            return;
    }

    /* This method will decide if the key gets queued locally or sent through UART */
    send_key(&new_report, state);
}

void process_consumer_report(uint8_t *raw_report, int length, uint8_t itf, hid_interface_t *iface) {
    uint8_t new_report[CONSUMER_CONTROL_LENGTH] = {0};
    uint16_t *report_ptr = (uint16_t *)new_report;
    device_t *state = &global_state;

    /* If consumer control is variable, read the values from cc_array and send as array. */
    if (iface->consumer.is_variable) {
        for (int i = 0; i < MAX_CC_BUTTONS && i < 8 * (length - 1); i++) {
            int bit_idx = i % 8;
            int byte_idx = i >> 3;

            if ((raw_report[byte_idx + 1] >> bit_idx) & 1) {
                report_ptr[0] = iface->keyboard.cc_array[i];
            }
        }
    }
    else {
        for (int i = 0; i < length - 1 && i < CONSUMER_CONTROL_LENGTH; i++)
            new_report[i] = raw_report[i + 1];
    }

    if (CURRENT_BOARD_IS_ACTIVE_OUTPUT) {
        send_consumer_control(new_report, state);
    } else {
        queue_packet((uint8_t *)new_report, CONSUMER_CONTROL_MSG, CONSUMER_CONTROL_LENGTH);
    }
}

void process_system_report(uint8_t *raw_report, int length, uint8_t itf, hid_interface_t *iface) {
    uint16_t new_report = raw_report[1];
    uint8_t *report_ptr = (uint8_t *)&new_report;
    device_t *state = &global_state;

    if (CURRENT_BOARD_IS_ACTIVE_OUTPUT) {
        send_system_control(report_ptr, state);
    } else {
        queue_packet(report_ptr, SYSTEM_CONTROL_MSG, SYSTEM_CONTROL_LENGTH);
    }
}

# 文件: led.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#include "main.h"

/* ==================================================== *
 * ========== Update pico and keyboard LEDs  ========== *
 * ==================================================== */

void set_keyboard_leds(uint8_t requested_led_state, device_t *state) {
    static uint8_t new_led_value;

    new_led_value = requested_led_state;
    if (state->keyboard_connected) {
        tuh_hid_set_report(state->kbd_dev_addr,
                           state->kbd_instance,
                           0,
                           HID_REPORT_TYPE_OUTPUT,
                           &new_led_value,
                           sizeof(uint8_t));
    }
}

void restore_leds(device_t *state) {
    /* Light up on-board LED if current board is active output */
    state->onboard_led_state = (state->active_output == BOARD_ROLE);
    gpio_put(GPIO_LED_PIN, state->onboard_led_state);

    /* Light up appropriate keyboard leds (if it's connected locally) */
    if (state->keyboard_connected) {
        uint8_t leds = state->keyboard_leds[state->active_output];
        set_keyboard_leds(leds, state);
    }
}

uint8_t toggle_led(void) {
    uint8_t new_led_state = gpio_get(GPIO_LED_PIN) ^ 1;
    gpio_put(GPIO_LED_PIN, new_led_state);

    return new_led_state;
}

void blink_led(device_t *state) {
    /* Since LEDs might be ON previously, we go OFF, ON, OFF, ON, OFF */
    state->blinks_left     = 5;
    state->last_led_change = time_us_32();
}

void led_blinking_task(device_t *state) {
    const int blink_interval_us = 80000; /* 80 ms off, 80 ms on */
    static uint8_t leds;

    /* If there is no more blinking to be done, exit immediately */
    if (state->blinks_left == 0)
        return;

    /* We have some blinks left to do, check if they are due, exit if not */
    if ((time_us_32()) - state->last_led_change < blink_interval_us)
        return;

    /* Toggle the LED state */
    uint8_t new_led_state = toggle_led();

    /* Also keyboard leds (if it's connected locally) since on-board leds are not visible */
    leds = new_led_state * 0x07; /* Numlock, capslock, scrollock */

    if (state->keyboard_connected)
        set_keyboard_leds(leds, state);

    /* Decrement the counter and update the last-changed timestamp */
    state->blinks_left--;
    state->last_led_change = time_us_32();

    /* Restore LEDs in the last pass */
    if (state->blinks_left == 0)
        restore_leds(state);
}

# 文件: main.c

#include "main.h" /* 引入项目主头文件 */

/* 如果没有在其它地方定义 DISABLE_PIO_USB，则默认禁用 PIO/TinyUSB（保守设置，便于把 PIO 留给 PS/2 项目） */
#ifndef DISABLE_PIO_USB
#define DISABLE_PIO_USB 1
#endif

/*********  Global Variables  **********/
device_t global_state     = {0}; /* 全局设备状态结构体实例，所有任务与模块共享 */
device_t *device          = &global_state; /* 指向全局状态的指针，便于函数调用 */

firmware_metadata_t _firmware_metadata __attribute__((section(".section_metadata"))) = {
    .version = 0x0001,
}; /* 固件元数据，放入特殊节以便在运行时读取或通过协议报告版本 */

/* ================================================== *
 * ==============  Main Program Loops  ============== *
 * ================================================== */

int main(void) {
    /* 定义 core0 的任务数组。数组元素为 task_t 结构，包含要执行的函数指针与频率。
       _TOP() 表示尽可能频繁运行，_HZ(x) 表示以 x 次/秒 的周期运行（通过微秒来计算下次运行时间）。 */
    static task_t tasks_core0[] = {
        [0] = {.exec = &usb_device_task,          .frequency = _TOP()},      // .-> USB device task, needs to run as often as possible
        [1] = {.exec = &kick_watchdog_task,       .frequency = _HZ(30)},     // | Verify core1 is still running and if so, reset watchdog timer
        [2] = {.exec = &process_kbd_queue_task,   .frequency = _HZ(2000)},   // | Check if there were any keypresses and send them
        [3] = {.exec = &process_mouse_queue_task, .frequency = _HZ(2000)},   // | Check if there were any mouse movements and send them
        [4] = {.exec = &process_hid_queue_task,   .frequency = _HZ(1000)},   // | Check if there are any packets to send over vendor link
        [5] = {.exec = &process_uart_tx_task,     .frequency = _TOP()},      // | Check if there are any packets to send over UART
    };                                                                       // `----- then go back and repeat forever
    const int NUM_TASKS = ARRAY_SIZE(tasks_core0); /* 当前 core0 任务数量 */

    // Wait for the board to settle
    sleep_ms(10); /* 简短延时，等待电源/外设稳定 */

    // Initial board setup
    initial_setup(device); /* 调用初始化：设置时钟、UART、DMA、看门狗、PIO/TinyUSB（可能被宏跳过）等 */

    // Initial state, A is the default output
    set_active_output(device, OUTPUT_A); /* 初始激活输出为 OUTPUT_A（A 板输出） */

    while (true) {
        for (int i = 0; i < NUM_TASKS; i++)
            task_scheduler(device, &tasks_core0[i]); /* 轮询任务调度器，按每个任务的 next_run 时间执行 */
    }
}

void core1_main() {
    /* core1 的任务列表，通常负责 USB Host、串口接收、LED 与定时器相关任务 */
    static task_t tasks_core1[] = {
        [0] = {.exec = &usb_host_task,           .frequency = _TOP()},       // .-> USB host task, needs to run as often as possible
        [1] = {.exec = &packet_receiver_task,    .frequency = _TOP()},       // | Receive data over serial from the other board
        [2] = {.exec = &led_blinking_task,       .frequency = _HZ(30)},      // | Check if LED needs blinking
        [3] = {.exec = &screensaver_task,        .frequency = _HZ(120)},     // | Handle "screensaver" movements
        [4] = {.exec = &firmware_upgrade_task,   .frequency = _HZ(4000)},    // | Send firmware to the other board if needed
        [5] = {.exec = &heartbeat_output_task,   .frequency = _HZ(1)},       // | Output periodic heartbeats
    };                                                                       // `----- then go back and repeat forever
    const int NUM_TASKS = ARRAY_SIZE(tasks_core1);

    while (true) {
        // Update the timestamp, so core0 can figure out if we're dead
        device->core1_last_loop_pass = time_us_64(); /* 更新 core1 的心跳时间戳，供 core0 的看门狗任务检查 */

        for (int i = 0; i < NUM_TASKS; i++)
            task_scheduler(device, &tasks_core1[i]); /* 轮询并执行 core1 的任务 */
    }
}
/* =======  End of Main Program Loops  ======= */

# 文件: mouse.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#include "main.h"
#include <math.h>

#define MACOS_SWITCH_MOVE_X 10
#define MACOS_SWITCH_MOVE_COUNT 5
#define ACCEL_POINTS 7

/* Check if our upcoming mouse movement would result in having to switch outputs */
enum screen_pos_e is_screen_switch_needed(int position, int offset) {
    if (position + offset < MIN_SCREEN_COORD - global_state.config.jump_threshold)
        return LEFT;

    if (position + offset > MAX_SCREEN_COORD + global_state.config.jump_threshold)
        return RIGHT;

    return NONE;
}

/* Move mouse coordinate 'position' by 'offset', but don't fall off the screen */
int32_t move_and_keep_on_screen(int position, int offset) {
    /* Lowest we can go is 0 */
    if (position + offset < MIN_SCREEN_COORD)
        return MIN_SCREEN_COORD;

    /* Highest we can go is MAX_SCREEN_COORD */
    else if (position + offset > MAX_SCREEN_COORD)
        return MAX_SCREEN_COORD;

    /* We're still on screen, all good */
    return position + offset;
}

/* Implement basic mouse acceleration based on actual 2D movement magnitude.
   Returns the acceleration factor to apply to both x and y components. */
float calculate_mouse_acceleration_factor(int32_t offset_x, int32_t offset_y) {
    const struct curve {
        int value;
        float factor;
    } acceleration[ACCEL_POINTS] = {
                   // 4 |                                        *
        {2, 1},    //   |                                  *
        {5, 1.1},  // 3 |
        {15, 1.4}, //   |                       *
        {30, 1.9}, // 2 |                *
        {45, 2.6}, //   |        *
        {60, 3.4}, // 1 |  *
        {70, 4.0}, //    -------------------------------------------
    };             //        10    20    30    40    50    60    70

    if (offset_x == 0 && offset_y == 0)
        return 1.0;

    if (!global_state.config.enable_acceleration)
        return 1.0;

    // Calculate the 2D movement magnitude
    const float movement_magnitude = sqrtf((float)(offset_x * offset_x) + (float)(offset_y * offset_y));

    if (movement_magnitude <= acceleration[0].value)
        return acceleration[0].factor;

    if (movement_magnitude >= acceleration[ACCEL_POINTS-1].value)
        return acceleration[ACCEL_POINTS-1].factor;

    const struct curve *lower = NULL;
    const struct curve *upper = NULL;

    for (int i = 0; i < ACCEL_POINTS-1; i++) {
        if (movement_magnitude < acceleration[i + 1].value) {
            lower = &acceleration[i];
            upper = &acceleration[i + 1];
            break;
        }
    }

    // Should never happen, but just in case
    if (lower == NULL || upper == NULL)
        return 1.0;

    const float interpolation_pos = (movement_magnitude - lower->value) /
                                  (upper->value - lower->value);

    return lower->factor + interpolation_pos * (upper->factor - lower->factor);
}

/* Returns LEFT if need to jump left, RIGHT if right, NONE otherwise */
enum screen_pos_e update_mouse_position(device_t *state, mouse_values_t *values) {
    output_t *current    = &state->config.output[state->active_output];
    uint8_t reduce_speed = 0;

    /* Check if we are configured to move slowly */
    if (state->mouse_zoom)
        reduce_speed = MOUSE_ZOOM_SCALING_FACTOR;

    /* Calculate movement */
    float acceleration_factor = calculate_mouse_acceleration_factor(values->move_x, values->move_y);
    int offset_x = round(values->move_x * acceleration_factor * (current->speed_x >> reduce_speed));
    int offset_y = round(values->move_y * acceleration_factor * (current->speed_y >> reduce_speed));

    /* Determine if our upcoming movement would stay within the screen */
    enum screen_pos_e switch_direction = is_screen_switch_needed(state->pointer_x, offset_x);

    /* Update movement */
    state->pointer_x = move_and_keep_on_screen(state->pointer_x, offset_x);
    state->pointer_y = move_and_keep_on_screen(state->pointer_y, offset_y);

    /* Update buttons state */
    state->mouse_buttons = values->buttons;

    return switch_direction;
}

/* If we are active output, queue packet to mouse queue, else send them through UART */
void output_mouse_report(mouse_report_t *report, device_t *state) {
    if (CURRENT_BOARD_IS_ACTIVE_OUTPUT) {
        queue_mouse_report(report, state);
        state->last_activity[BOARD_ROLE] = time_us_64();
    } else {
        queue_packet((uint8_t *)report, MOUSE_REPORT_MSG, MOUSE_REPORT_LENGTH);
    }
}

/* Calculate and return Y coordinate when moving from screen out_from to screen out_to */
int16_t scale_y_coordinate(int screen_from, int screen_to, device_t *state) {
    output_t *from = &state->config.output[screen_from];
    output_t *to   = &state->config.output[screen_to];

    int size_to   = to->border.bottom - to->border.top;
    int size_from = from->border.bottom - from->border.top;

    /* If sizes match, there is nothing to do */
    if (size_from == size_to)
        return state->pointer_y;

    /* Moving from smaller ==> bigger screen
       y_a = top + (((bottom - top) * y_b) / HEIGHT) */

    if (size_from > size_to) {
        return to->border.top + ((size_to * state->pointer_y) / MAX_SCREEN_COORD);
    }

    /* Moving from bigger ==> smaller screen
       y_b = ((y_a - top) * HEIGHT) / (bottom - top) */

    if (state->pointer_y < from->border.top)
        return MIN_SCREEN_COORD;

    if (state->pointer_y > from->border.bottom)
        return MAX_SCREEN_COORD;

    return ((state->pointer_y - from->border.top) * MAX_SCREEN_COORD) / size_from;
}

void switch_to_another_pc(
    device_t *state, output_t *output, int output_to, int direction) {
    uint8_t *mouse_park_pos = &state->config.output[state->active_output].mouse_park_pos;

    int16_t mouse_y = (*mouse_park_pos == 0) ? MIN_SCREEN_COORD : /* Top */
                      (*mouse_park_pos == 1) ? MAX_SCREEN_COORD : /* Bottom */
                                               state->pointer_y;  /* Previous */

    mouse_report_t hidden_pointer = {.y = mouse_y, .x = MAX_SCREEN_COORD};

    output_mouse_report(&hidden_pointer, state);
    set_active_output(state, output_to);
    state->pointer_x = (direction == LEFT) ? MAX_SCREEN_COORD : MIN_SCREEN_COORD;
    state->pointer_y = scale_y_coordinate(output->number, 1 - output->number, state);
}

void switch_virtual_desktop_macos(device_t *state, int direction) {
    /*
     * Fix for MACOS: Before sending new absolute report setting X to 0:
     * 1. Move the cursor to the edge of the screen directly in the middle to handle screens
     *    of different heights
     * 2. Send relative mouse movement one or two pixels in the direction of movement to get
     *    the cursor onto the next screen
     */
    mouse_report_t edge_position = {
        .x = (direction == LEFT) ? MIN_SCREEN_COORD : MAX_SCREEN_COORD,
        .y = MAX_SCREEN_COORD / 2,
        .mode = ABSOLUTE,
        .buttons = state->mouse_buttons,
    };

    uint16_t move = (direction == LEFT) ? -MACOS_SWITCH_MOVE_X : MACOS_SWITCH_MOVE_X;
    mouse_report_t move_relative_one = {
        .x = move,
        .mode = RELATIVE,
    };

    output_mouse_report(&edge_position, state);

    /* Once doesn't seem reliable enough, do it a few times */
    for (int i = 0; i < MACOS_SWITCH_MOVE_COUNT; i++)
        output_mouse_report(&move_relative_one, state);
}

void switch_virtual_desktop(device_t *state, output_t *output, int new_index, int direction) {
    switch (output->os) {
        case MACOS:
            switch_virtual_desktop_macos(state, direction);
            break;

        case WINDOWS:
            /* TODO: Switch to relative-only if index > 1, but keep tabs to switch back */
            state->relative_mouse = (new_index > 1);
            break;

        case LINUX:
        case ANDROID:
        case OTHER:
            /* Linux should treat all desktops as a single virtual screen, so you should leave
            screen_count at 1 and it should just work */
            break;
    }

    state->pointer_x       = (direction == RIGHT) ? MIN_SCREEN_COORD : MAX_SCREEN_COORD;
    output->screen_index = new_index;
}

/*                               BORDER
                                   |
       .---------.    .---------.  |  .---------.    .---------.    .---------.
      ||    B2   ||  ||    B1   || | ||    A1   ||  ||    A2   ||  ||    A3   ||   (output, index)
      ||  extra  ||  ||   main  || | ||   main  ||  ||  extra  ||  ||  extra  ||   (main or extra)
       '---------'    '---------'  |  '---------'    '---------'    '---------'
          )___(          )___(     |     )___(          )___(          )___(
*/
void do_screen_switch(device_t *state, int direction) {
    output_t *output = &state->config.output[state->active_output];

    /* No switching allowed if explicitly disabled or in gaming mode */
    if (state->switch_lock || state->gaming_mode)
        return;

    /* We want to jump in the direction of the other computer */
    if (output->pos != direction) {
        if (output->screen_index == 1) { /* We are at the border -> switch outputs */
            /* No switching allowed if mouse button is held. Should only apply to the border! */
            if (state->mouse_buttons)
                return;

            switch_to_another_pc(state, output, 1 - state->active_output, direction);
        }
        /* If here, this output has multiple desktops and we are not on the main one */
        else
            switch_virtual_desktop(state, output, output->screen_index - 1, direction);
    }

    /* We want to jump away from the other computer, only possible if there is another screen to jump to */
    else if (output->screen_index < output->screen_count)
        switch_virtual_desktop(state, output, output->screen_index + 1, direction);
}

static inline bool extract_value(bool uses_id, int32_t *dst, report_val_t *src, uint8_t *raw_report, int len) {
    /* If HID Report ID is used, the report is prefixed by the report ID so we have to move by 1 byte */
    if (uses_id && (*raw_report++ != src->report_id))
        return false;

    *dst = get_report_value(raw_report, len, src);
    return true;
}

void extract_report_values(uint8_t *raw_report, int len, device_t *state, mouse_values_t *values, hid_interface_t *iface) {
    /* Interpret values depending on the current protocol used. */
    if (iface->protocol == HID_PROTOCOL_BOOT) {
        hid_mouse_report_t *mouse_report = (hid_mouse_report_t *)raw_report;

        values->move_x  = mouse_report->x;
        values->move_y  = mouse_report->y;
        values->wheel   = mouse_report->wheel;
        values->pan     = mouse_report->pan;
        values->buttons = mouse_report->buttons;
        return;
    }
    mouse_t *mouse = &iface->mouse;
    bool uses_id = iface->uses_report_id;

    extract_value(uses_id, &values->move_x, &mouse->move_x, raw_report, len);
    extract_value(uses_id, &values->move_y, &mouse->move_y, raw_report, len);
    extract_value(uses_id, &values->wheel, &mouse->wheel, raw_report, len);
    extract_value(uses_id, &values->pan, &mouse->pan, raw_report, len);

    if (!extract_value(uses_id, &values->buttons, &mouse->buttons, raw_report, len)) {
        values->buttons = state->mouse_buttons;
    }
}

mouse_report_t create_mouse_report(device_t *state, mouse_values_t *values) {
    mouse_report_t mouse_report = {
        .buttons = values->buttons,
        .x       = state->pointer_x,
        .y       = state->pointer_y,
        .wheel   = values->wheel,
        .pan     = values->pan,
        .mode    = ABSOLUTE,
    };

    /* Workaround for Windows multiple desktops */
    if (state->relative_mouse || state->gaming_mode) {
        mouse_report.x = values->move_x;
        mouse_report.y = values->move_y;
        mouse_report.mode = RELATIVE;
    }

    return mouse_report;
}

void process_mouse_report(uint8_t *raw_report, int len, uint8_t itf, hid_interface_t *iface) {
    mouse_values_t values = {0};
    device_t *state = &global_state;

    /* Interpret the mouse HID report, extract and save values we need. */
    extract_report_values(raw_report, len, state, &values, iface);

    /* Calculate and update mouse pointer movement. */
    enum screen_pos_e switch_direction = update_mouse_position(state, &values);

    /* Create the report for the output PC based on the updated values */
    mouse_report_t report = create_mouse_report(state, &values);

    /* Move the mouse, depending where the output is supposed to go */
    output_mouse_report(&report, state);

    /* We use the mouse to switch outputs, if switch_direction is LEFT or RIGHT */
    if (switch_direction != NONE)
        do_screen_switch(state, switch_direction);
}

/* ==================================================== *
 * Mouse Queue Section
 * ==================================================== */

void process_mouse_queue_task(device_t *state) {
    mouse_report_t report = {0};

    /* We need to be connected to the host to send messages */
    if (!state->tud_connected)
        return;

    /* Peek first, if there is anything there... */
    if (!queue_try_peek(&state->mouse_queue, &report))
        return;

    /* If we are suspended, let's wake the host up */
    if (tud_suspended())
        tud_remote_wakeup();

    /* If it's not ready, we'll try on the next pass */
    if (!tud_hid_n_ready(ITF_NUM_HID))
        return;

    /* Try sending it to the host, if it's successful */
    bool succeeded
        = tud_mouse_report(report.mode, report.buttons, report.x, report.y, report.wheel, report.pan);

    /* ... then we can remove it from the queue */
    if (succeeded)
        queue_try_remove(&state->mouse_queue, &report);
}

void queue_mouse_report(mouse_report_t *report, device_t *state) {
    /* It wouldn't be fun to queue up a bunch of messages and then dump them all on host */
    if (!state->tud_connected)
        return;

    queue_try_add(&state->mouse_queue, report);
}

# 文件: protocol.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#include "main.h"

const field_map_t api_field_map[] = {
/* Index, Rdonly, Type, Len, Offset in struct */
    { 0,  true,  UINT8,  1, offsetof(device_t, active_output) },
    { 1,  true,  INT16,  2, offsetof(device_t, pointer_x) },
    { 2,  true,  INT16,  2, offsetof(device_t, pointer_y) },
    { 3,  true,  INT16,  2, offsetof(device_t, mouse_buttons) },

    /* Output A */
    { 10, false, UINT32, 4, offsetof(device_t, config.output[0].number) },
    { 11, false, UINT32, 4, offsetof(device_t, config.output[0].screen_count) },
    { 12, false, INT32,  4, offsetof(device_t, config.output[0].speed_x) },
    { 13, false, INT32,  4, offsetof(device_t, config.output[0].speed_y) },
    { 14, false, INT32,  4, offsetof(device_t, config.output[0].border.top) },
    { 15, false, INT32,  4, offsetof(device_t, config.output[0].border.bottom) },
    { 16, false, UINT8,  1, offsetof(device_t, config.output[0].os) },
    { 17, false, UINT8,  1, offsetof(device_t, config.output[0].pos) },
    { 18, false, UINT8,  1, offsetof(device_t, config.output[0].mouse_park_pos) },
    { 19, false, UINT8,  1, offsetof(device_t, config.output[0].screensaver.mode) },
    { 20, false, UINT8,  1, offsetof(device_t, config.output[0].screensaver.only_if_inactive) },

    /* Until we increase the payload size from 8 bytes, clamp to avoid exceeding the field size */
    { 21, false, UINT64, 7, offsetof(device_t, config.output[0].screensaver.idle_time_us) },
    { 22, false, UINT64, 7, offsetof(device_t, config.output[0].screensaver.max_time_us) },

    /* Output B */
    { 40, false, UINT32, 4, offsetof(device_t, config.output[1].number) },
    { 41, false, UINT32, 4, offsetof(device_t, config.output[1].screen_count) },
    { 42, false, INT32,  4, offsetof(device_t, config.output[1].speed_x) },
    { 43, false, INT32,  4, offsetof(device_t, config.output[1].speed_y) },
    { 44, false, INT32,  4, offsetof(device_t, config.output[1].border.top) },
    { 45, false, INT32,  4, offsetof(device_t, config.output[1].border.bottom) },
    { 46, false, UINT8,  1, offsetof(device_t, config.output[1].os) },
    { 47, false, UINT8,  1, offsetof(device_t, config.output[1].pos) },
    { 48, false, UINT8,  1, offsetof(device_t, config.output[1].mouse_park_pos) },
    { 49, false, UINT8,  1, offsetof(device_t, config.output[1].screensaver.mode) },
    { 50, false, UINT8,  1, offsetof(device_t, config.output[1].screensaver.only_if_inactive) },
    { 51, false, UINT64, 7, offsetof(device_t, config.output[1].screensaver.idle_time_us) },
    { 52, false, UINT64, 7, offsetof(device_t, config.output[1].screensaver.max_time_us) },

    /* Common config */
    { 70, false, UINT32, 4, offsetof(device_t, config.version) },
    { 71, false, UINT8,  1, offsetof(device_t, config.force_mouse_boot_mode) },
    { 72, false, UINT8,  1, offsetof(device_t, config.force_kbd_boot_protocol) },
    { 73, false, UINT8,  1, offsetof(device_t, config.kbd_led_as_indicator) },
    { 74, false, UINT8,  1, offsetof(device_t, config.hotkey_toggle) },
    { 75, false, UINT8,  1, offsetof(device_t, config.enable_acceleration) },
    { 76, false, UINT8,  1, offsetof(device_t, config.enforce_ports) },
    { 77, false, UINT16, 2, offsetof(device_t, config.jump_threshold) },

    /* Firmware */
    { 78, true,  UINT16, 2, offsetof(device_t, _running_fw.version) },
    { 79, true,  UINT32, 4, offsetof(device_t, _running_fw.checksum) },

    { 80, true,  UINT8,  1, offsetof(device_t, keyboard_connected) },
    { 81, true,  UINT8,  1, offsetof(device_t, switch_lock) },
    { 82, true,  UINT8,  1, offsetof(device_t, relative_mouse) },
};

const field_map_t* get_field_map_entry(uint32_t index) {
    for (unsigned int i = 0; i < ARRAY_SIZE(api_field_map); i++) {
        if (api_field_map[i].idx == index) {
            return &api_field_map[i];
        }
    }

    return NULL;
}


const field_map_t* get_field_map_index(uint32_t index) {
    return &api_field_map[index];
}

size_t get_field_map_length(void) {
    return ARRAY_SIZE(api_field_map);
}

void _queue_packet(uint8_t *payload, device_t *state, uint8_t type, uint8_t len, uint8_t id, uint8_t inst) {
    hid_generic_pkt_t generic_packet = {
        .instance = inst,
        .report_id = id,
        .type = type,
        .len = len,
    };

    memcpy(generic_packet.data, payload, len);
    queue_try_add(&state->hid_queue_out, &generic_packet);
}

void queue_cfg_packet(uart_packet_t *packet, device_t *state) {
    uint8_t raw_packet[RAW_PACKET_LENGTH];
    write_raw_packet(raw_packet, packet);
    _queue_packet(raw_packet, state, 0, RAW_PACKET_LENGTH, REPORT_ID_VENDOR, ITF_NUM_HID_VENDOR);
}

void queue_cc_packet(uint8_t *payload, device_t *state) {
    _queue_packet(payload, state, 1, CONSUMER_CONTROL_LENGTH, REPORT_ID_CONSUMER, ITF_NUM_HID);
}

void queue_system_packet(uint8_t *payload, device_t *state) {
    _queue_packet(payload, state, 2, SYSTEM_CONTROL_LENGTH, REPORT_ID_SYSTEM, ITF_NUM_HID);
}

# 文件: ramdisk.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 * Based on the TinyUSB example by Ha Thach.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#include "main.h"

#define NUMBER_OF_BLOCKS 4096
#define ACTUAL_NUMBER_OF_BLOCKS 128
#define BLOCK_SIZE       512

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    strcpy((char *)vendor_id, "DeskHop");
    strcpy((char *)product_id, "Config Mode");
    strcpy((char *)product_rev, "1.0");
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    *block_count = NUMBER_OF_BLOCKS;
    *block_size  = BLOCK_SIZE;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    return true;
}

/* Return the requested data, or -1 if out-of-bounds */
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    const uint8_t *addr = &ADDR_DISK_IMAGE[lba * BLOCK_SIZE + offset];

    if (lba >= NUMBER_OF_BLOCKS)
        return -1;

    /* We lie about the image size - actually it's 64 kB, not 512 kB, so if we're out of bounds, return zeros */
    else if (lba >= ACTUAL_NUMBER_OF_BLOCKS)
        memset(buffer, 0x00, bufsize);

    else
        memcpy(buffer, addr, bufsize);

    return (int32_t)bufsize;
}

/* We're writable, so return true */
bool tud_msc_is_writable_cb(uint8_t lun) {
    return true;
}

/* Simple firmware write routine, we get 512-byte uf2 blocks with 256 byte payload */
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    uf2_t *uf2 = (uf2_t *)&buffer[0];
    bool is_final_block = uf2->blockNo == (STAGING_IMAGE_SIZE / FLASH_PAGE_SIZE) - 1;
    uint32_t flash_addr = (uint32_t)ADDR_FW_RUNNING + uf2->blockNo * FLASH_PAGE_SIZE - XIP_BASE;

    if (lba >= NUMBER_OF_BLOCKS)
        return -1;

    /* If we're not detecting UF2 magic constants, we have nothing to do... */
    if (uf2->magicStart0 != UF2_MAGIC_START0 || uf2->magicStart1 != UF2_MAGIC_START1 || uf2->magicEnd != UF2_MAGIC_END)
        return (int32_t)bufsize;

    if (uf2->blockNo == 0) {
        global_state.fw.checksum = 0xffffffff;

        /* Make sure nobody else touches the flash during this operation, otherwise we get empty pages */
        global_state.fw.upgrade_in_progress = true;
    }

    /* Update checksum continuously as blocks are being received */
    const uint32_t last_block_with_checksum = (STAGING_IMAGE_SIZE - FLASH_SECTOR_SIZE) / FLASH_PAGE_SIZE;
    for (int i=0; i<FLASH_PAGE_SIZE && uf2->blockNo < last_block_with_checksum; i++)
        global_state.fw.checksum = crc32_iter(global_state.fw.checksum, buffer[32 + i]);

    write_flash_page(flash_addr, &buffer[32]);

    if (is_final_block) {
        global_state.fw.checksum = ~global_state.fw.checksum;

        /* If checksums don't match, overwrite first sector and rely on ROM bootloader for recovery */
        if (global_state.fw.checksum != calculate_firmware_crc32()) {
            flash_range_erase((uint32_t)ADDR_FW_RUNNING - XIP_BASE, FLASH_SECTOR_SIZE);
            reset_usb_boot(1 << PICO_DEFAULT_LED_PIN, 0);
        }
        else {
            global_state.reboot_requested = true;
        }
    }

    /* Provide some visual indication that fw is being uploaded */
    toggle_led();
    watchdog_update();

    return (int32_t)bufsize;
}

/* This is a super-dumb, rudimentary disk, any other scsi command is simply rejected */
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize) {
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}

# 文件: setup.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

/* ================================================== *
 * =============  Initial Board Setup  ============== *
 * ================================================== */

#include "main.h"  /* 包含全局头文件，项目的全局定义、类型和函数原型 */

/* ------------------------------------------------------------------
 * 最小改动说明：
 *  - 为了保留原始代码结构，只在两处添加了宏控制（不破坏原始函数签名和逻辑）：
 *      1) 在文件顶部添加 DISABLE_PIO_USB 宏（默认 1，表示禁用 PIO/TinyUSB 初始化）
 *      2) 在 pio_usb_host_config 函数体内部与 initial_setup 中调用 tud_init()/pio_usb_host_config 处
 *         使用 #if/#else 控制，DISABLE_PIO_USB=1 时跳过实际初始化/配置；否则保持原始行为。
 *  - 这样做可以最小影响其它模块且便于快速恢复（把宏改为 0 即可恢复原始 USB/PIO 行为）。
 * ------------------------------------------------------------------*/

#ifndef DISABLE_PIO_USB
#define DISABLE_PIO_USB 1  /* 默认禁用 PIO/TinyUSB，设置为 0 可恢复 USB/PIO 初始化 */
#endif

/* 声明 usb.c 中新增的 UART1 初始化函数，initial_setup 将调用它以启用 UART1 调试串口 */
extern void serial1_init(void); /* 在 usb.c 中实现，用于初始化 UART1（GPIO8/9）用于接收 A 板数据 */

/* ================================================== *
 * Perform initial UART setup
 * ================================================== */

void serial_init() {                      /* 初始化主串口（通常为 UART0） */
    /* Set up our UART with a default baudrate. */
    uart_init(SERIAL_UART, SERIAL_BAUDRATE); /* 调用底层 SDK 初始化 UART，设置波特率 */

    /* Set UART flow control CTS/RTS. We don't have these - turn them off.*/
    uart_set_hw_flow(SERIAL_UART, false, false); /* 关闭硬件流控 (CTS/RTS) */

    /* Set our data format */
    uart_set_format(SERIAL_UART, SERIAL_DATA_BITS, SERIAL_STOP_BITS, SERIAL_PARITY); /* 配置数据位/停止位/校验 */

    /* Turn of CRLF translation */
    uart_set_translate_crlf(SERIAL_UART, false); /* 关闭 CR/LF 转换，原始字节传输 */

    /* We do want FIFO, will help us have fewer interruptions */
    uart_set_fifo_enabled(SERIAL_UART, true); /* 启用 UART FIFO，降低中断频率 */

    /* Set the RX/TX pins, they differ based on the device role (A or B, check schematics) */
    gpio_set_function(SERIAL_TX_PIN, GPIO_FUNC_UART); /* 将 TX 引脚复用为 UART 功能 */
    gpio_set_function(SERIAL_RX_PIN, GPIO_FUNC_UART); /* 将 RX 引脚复用为 UART 功能 */
}

/* ================================================== *
 * PIO USB configuration, D+ pin 14, D- pin 15
 * ================================================== */

void pio_usb_host_config(device_t *state) {
#if DISABLE_PIO_USB
    /* DISABLE_PIO_USB == 1：为了把 PIO/USB 硬件资源留给后续的 PS2 功能，跳过 TinyUSB Host / PIO USB 的实际配置。
       仍保留函数签名以避免链接/引用错误。此处仅使用 (void)state 来避免未使用参数的警告。
       若需要恢复 PIO/TinyUSB，请将 DISABLE_PIO_USB 设为 0，然后函数行为即为原始实现。 */
    (void)state; /* 避免未使用参数警告，函数不做任何事 */
#else
    /* tuh_configure() must be called before tuh_init() */
    static pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG; /* PIO USB 配置结构体，使用默认值 */
    config.pin_dp                         = PIO_USB_DP_PIN_DEFAULT; /* 指定 D+ 引脚 */

    /* Board B is always report mode, board A is default-boot if configured */
    if (state->board_role == OUTPUT_B || ENFORCE_KEYBOARD_BOOT_PROTOCOL == 0)
        tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT); /* 设置 HID 默认协议为 report 模式 */

    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &config); /* 配置 PIO USB */

    /* Initialize and configure TinyUSB Host */
    tuh_init(1); /* 初始化 TinyUSB Host */
#endif
}

/* ================================================== *
 * Board Autoprobe Routine
 * ================================================== */

/* Probing algorithm logic:
    - RX pin is driven by the digital isolator IC
    - IF we are board A, it will be connected to pin 13
      and it will drive it either high or low at any given time
    - Before uart setup, enable it as an input
    - Go through a probing sequence of 8 values and pull either up or down
      to that value
    - Read out the value on the RX pin
    - If the entire sequence of values match, we are definitely floating
      so IC is not connected on BOARD_A_RX, and we're BOARD B
*/
int board_autoprobe(void) {
    const bool probing_sequence[] = {true, false, false, true, true, false, true, false}; /* 预定义探测序列 */
    const int seq_len = ARRAY_SIZE(probing_sequence); /* 序列长度 */

    /* Set the pin as INPUT and initialize it */
    gpio_init(BOARD_A_RX); /* 初始化 GPIO 引脚 */
    gpio_set_dir(BOARD_A_RX, GPIO_IN); /* 将引脚设为输入 */

    for (int i=0; i<seq_len; i++) {
        if (probing_sequence[i])
            gpio_pull_up(BOARD_A_RX); /* 根据序列拉上拉 */
        else
            gpio_pull_down(BOARD_A_RX); /* 或拉下拉 */

        /* Wait for value to settle */
        sleep_ms(3); /* 等待电平稳定 */

        /* Read the value */
        bool value = gpio_get(BOARD_A_RX); /* 读取 GPIO 电平 */
        gpio_disable_pulls(BOARD_A_RX); /* 禁用上/下拉 */

        /* If values mismatch at any point, means IC is connected and we're board A */
        if (probing_sequence[i] != value)
            return OUTPUT_A; /* 返回作为 A 板 */
    }

    /* If it was just reading the pull up/down in all cases, pin is floating and we're board B */
    return OUTPUT_B; /* 返回作为 B 板 */
}


/* ================================================== *
 * Check if we should boot in configuration mode or not
 * ================================================== */

bool is_config_mode_active(device_t *state) {
    /* Watchdog registers survive reboot (RP2040 datasheet section 2.8.1.1) */
    bool is_active = (watchdog_hw->scratch[5] == MAGIC_WORD_1 &&
                      watchdog_hw->scratch[6] == MAGIC_WORD_2); /* 通过看看看门狗的 scratch 寄存器判断是否进入配置模式 */

    /* Remove, so next reboot it's no longer active */
    if (is_active)
        watchdog_hw->scratch[5] = 0; /* 清除标志，避免下次启动依然进入配置 */

    reset_config_timer(state); /* 重置配置计时器 */

    return is_active; /* 返回是否处于配置模式 */
}


/* ================================================== *
 * Configure DMA for reliable UART transfers
 * ================================================== */
const uint8_t* uart_buffer_pointers[1] = {uart_rxbuf}; /* DMA 控制通道所指向的缓冲区指针数组 */
uint8_t uart_rxbuf[DMA_RX_BUFFER_SIZE] __attribute__((aligned(DMA_RX_BUFFER_SIZE))) ; /* DMA 接收环形缓冲区 */
uint8_t uart_txbuf[DMA_TX_BUFFER_SIZE] __attribute__((aligned(DMA_TX_BUFFER_SIZE))) ; /* DMA 发送缓冲区 */

static void configure_tx_dma(device_t *state) {
    state->dma_tx_channel = dma_claim_unused_channel(true); /* 申请一个空闲的 DMA 通道用于 TX */

    dma_channel_config tx_config = dma_channel_get_default_config(state->dma_tx_channel); /* 获取默认配置 */
    channel_config_set_transfer_data_size(&tx_config, DMA_SIZE_8); /* 8 位传输 */

    /* Writing uart (always write the same address, but source addr changes as we read) */
    channel_config_set_read_increment(&tx_config, true); /* 源地址递增（从缓冲区读取） */
    channel_config_set_write_increment(&tx_config, false); /* 目标地址不递增（写入 UART 寄存器） */

    // channel_config_set_ring(&tx_config, false, 4);
    channel_config_set_dreq(&tx_config, DREQ_UART0_TX); /* 使用 UART0 TX 触发 DMA */

    /* Configure, but don't start immediately. We'll do this each time the outgoing
       packet is ready and we copy it to the buffer */
    dma_channel_configure(
        state->dma_tx_channel,
        &tx_config,
        &uart0_hw->dr, /* 目标地址：UART0 数据寄存器 */
        uart_txbuf,    /* 源地址：TX 缓冲区 */
        0,             /* 初始传输长度 0，稍后设置 */
        false
    );
}

static void configure_rx_dma(device_t *state) {
    /* Find an empty channel, store it for later reference */
    state->dma_rx_channel = dma_claim_unused_channel(true); /* 申请 RX DMA 通道 */
    state->dma_control_channel = dma_claim_unused_channel(true); /* 申请控制链通道 */

    dma_channel_config config = dma_channel_get_default_config(state->dma_rx_channel);
    dma_channel_config control_config = dma_channel_get_default_config(state->dma_control_channel);

    channel_config_set_transfer_data_size(&config, DMA_SIZE_8); /* RX 通道传输 8 位 */
    channel_config_set_transfer_data_size(&control_config, DMA_SIZE_32); /* 控制通道 32 位 */

    // The read address is the address of the UART data register which is constant
    channel_config_set_read_increment(&config, false); /* 读取地址不变（从 UART 寄存器读） */
    channel_config_set_read_increment(&control_config, false);

    // Read into a ringbuffer with 1024 (2^10) elements
    channel_config_set_write_increment(&config, true); /* 写地址递增（写入环形缓冲区） */
    channel_config_set_write_increment(&control_config, false);

    channel_config_set_ring(&config, true, 10); /* 将写入地址设置为环形缓冲区（2^10 元素） */

    // The UART signals when data is avaliable
    channel_config_set_dreq(&config, DREQ_UART0_RX); /* 使用 UART0 RX 触发 DMA */

    channel_config_set_chain_to(&config, state->dma_control_channel); /* 链接到控制通道 */

    dma_channel_configure(
        state->dma_rx_channel,
        &config,
        uart_rxbuf,                         /* 目标：环形缓冲区 */
        &uart0_hw->dr,                      /* 源：UART0 数据寄存器 */
        DMA_RX_BUFFER_SIZE,
        false);

    dma_channel_configure(
        state->dma_control_channel,
        &control_config,
        &dma_hw->ch[state->dma_rx_channel].al2_write_addr_trig, /* 控制通道目标：触发写地址寄存器 */
        uart_buffer_pointers,
        1,
        false);

    dma_channel_start(state->dma_control_channel); /* 启动控制 DMA 通道，让环形缓冲区开始工作 */
}


/* ================================================== *
 * Perform initial board/usb setup
 * ================================================== */
int board; /* 全局变量，表示当前板的编号或角色（保留） */

void initial_setup(device_t *state) {
    /* PIO USB requires a clock multiple of 12 MHz, setting to 120 MHz */
    set_sys_clock_khz(120000, true); /* 将系统时钟设置为 120 MHz，满足 PIO USB 的频率需求 */

    /* Search the persistent storage sector in flash for valid config or use defaults */
    load_config(state); /* 从 Flash 加载配置，若无则使用默认 */

    /* Init and enable the on-board LED GPIO as output */
    gpio_init(GPIO_LED_PIN); /* 初始化板载 LED 引脚 */
    gpio_set_dir(GPIO_LED_PIN, GPIO_OUT); /* 将 LED 引脚设为输出 */

    /* Check if we should boot in configuration mode or not */
    state->config_mode_active = is_config_mode_active(state); /* 检查是否进入配置模式 */

    /* Detect which board we're running on */
    state->board_role = board_autoprobe(); /* 自动探测当前为 A 板 还是 B 板 */

    /* Initialize and configure UART */
    serial_init(); /* 初始化主串口（UART0） */

    /* 初始化并启用额外的 UART1 以用于从 A 板接收/调试（GPIO8=TX, GPIO9=RX） */
    serial1_init(); /* 新增：初始化 UART1（用于调试/接收 A 板数据） */

    /* Initialize keyboard and mouse queues */
    queue_init(&state->kbd_queue, sizeof(hid_keyboard_report_t), KBD_QUEUE_LENGTH); /* 初始化键盘队列 */
    queue_init(&state->mouse_queue, sizeof(mouse_report_t), MOUSE_QUEUE_LENGTH); /* 初始化鼠标队列 */

    /* Initialize keyboard states for all devices */
    memset(state->kbd_states, 0, sizeof(state->kbd_states)); /* 清零键盘状态数组 */
    state->kbd_device_count = 0; /* 键盘设备计数器清零 */

    /* Initialize generic HID packet queue */
    queue_init(&state->hid_queue_out, sizeof(hid_generic_pkt_t), HID_QUEUE_LENGTH); /* 初始化通用 HID 队列 */

    /* Initialize UART queue */
    queue_init(&state->uart_tx_queue, sizeof(uart_packet_t), UART_QUEUE_LENGTH); /* 初始化 UART 发送队列 */

    /* Setup RP2040 Core 1 */
    multicore_reset_core1(); /* 重置核心1 */
    multicore_launch_core1(core1_main); /* 启动核心1，运行 core1_main 函数 */

#if !DISABLE_PIO_USB
    /* Initialize and configure TinyUSB Device */
    tud_init(BOARD_TUD_RHPORT); /* 初始化 TinyUSB 设备堆栈（只有当未禁用时） */
#endif

#if !DISABLE_PIO_USB
    /* Initialize and configure TinyUSB Host */
    pio_usb_host_config(state); /* 配置 PIO USB 主机（只有当未禁用时） */
#endif

    /* Initialize and configure DMA */
    configure_tx_dma(state); /* 配置 TX DMA */
    configure_rx_dma(state); /* 配置 RX DMA */

    /* Load the current firmware info */
    state->_running_fw = _firmware_metadata; /* 保存当前运行的固件信息指针 */

    /* Update the core1 initial pass timestamp before enabling the watchdog */
    state->core1_last_loop_pass = time_us_64(); /* 记录 core1 的时间戳，供看门狗监控 */

    /* Setup the watchdog so we reboot and recover from a crash */
    watchdog_enable(WATCHDOG_TIMEOUT, WATCHDOG_PAUSE_ON_DEBUG); /* 启用看门狗，避免死循环挂起 */
}

/* ==========  End of setup.c portion ========== */

# 文件: tasks.c

/* task_scheduler: 简单的周期任务调度器
 * - 根据任务的 next_run 与 frequency 控制实际运行时间点
 */
void task_scheduler(device_t *state, task_t *task) {
    uint64_t current_time = time_us_64(); /* 当前微秒时间 */

    if (current_time < task->next_run)
        return; /* 未到执行时间，直接返回 */

    task->next_run = current_time + task->frequency; /* 计算下次执行时间 */
    task->exec(state); /* 调用任务函数 */
}

/* ================================================== *
 * ==============  Watchdog Functions  ============== *
 * ================================================== */

void kick_watchdog_task(device_t *state) {
    /* Read the timer AFTER duplicating the core1 timestamp,
       so it doesn't get updated in the meantime. */
    uint64_t core1_last_loop_pass = state->core1_last_loop_pass; /* 复制 core1 的时间戳副本 */
    uint64_t current_time         = time_us_64(); /* 当前时间 */

    /* If a reboot is requested, we'll stop updating watchdog */
    if (state->reboot_requested)
        return; /* 已请求重启则不喂狗 */

    /* If core1 stops updating the timestamp, we'll stop kicking the watchog and reboot */
    if (current_time - core1_last_loop_pass < CORE1_HANG_TIMEOUT_US)
        watchdog_update(); /* 如果 core1 仍在正常更新，重置看门狗计时器 */
}

/* ================================================== *
 * ===============  USB Device / Host  ============== *
 * 对 TinyUSB 的周期性任务调用进行宏保护，在禁用时避免任何 TinyUSB 代码被调用
 * ================================================== */

void usb_device_task(device_t *state) {
#if !DISABLE_PIO_USB
    tud_task(); /* TinyUSB 设备栈的主循环，处理 USB 事件与请求 */
#else
    (void)state; /* 当禁用 USB 时，保持空实现以避免链接错误或调用未初始化的库 */
#endif
}

void usb_host_task(device_t *state) {
#if !DISABLE_PIO_USB
    if (tuh_inited())
        tuh_task(); /* TinyUSB Host 主循环，处理外接 HID 等设备 */
#else
    (void)state; /* 禁用时空实现 */
#endif
}

/* 屏保的两种移动策略：pong（碰撞反弹）和 jitter（抖动） */
mouse_report_t *screensaver_pong(device_t *state) {
    static mouse_report_t report = {0};
    static int dx = 20, dy = 25;

    /* Check if we are bouncing off the walls and reverse direction in that case. */
    if (report.x + dx < MIN_SCREEN_COORD || report.x + dx > MAX_SCREEN_COORD)
        dx = -dx; /* 碰到横向边界则反向 */

    if (report.y + dy < MIN_SCREEN_COORD || report.y + dy > MAX_SCREEN_COORD)
        dy = -dy; /* 碰到纵向边界则反向 */

    report.x += dx; /* 更新坐标 */
    report.y += dy;

    return &report; /* 返回指向静态报告的指针 */
}

mouse_report_t *screensaver_jitter(device_t *state) {
    static mouse_report_t report = {
        .y = JITTER_DISTANCE,
        .mode = RELATIVE,
    };
    report.y = -report.y; /* 翻转垂直抖动方向 */

    return &report;
}

/* Have something fun and entertaining when idle. */
void screensaver_task(device_t *state) {
    const uint32_t delays[] = {
        0,        /* DISABLED, unused index 0 */
        5000,     /* PONG, move mouse every 5 ms for a high framerate */
        10000000, /* JITTER, once every 10 sec is more than enough */
    };
    static int last_pointer_move = 0;
    screensaver_t *screensaver = &state->config.output[BOARD_ROLE].screensaver; /* 获取当前板的屏保配置 */
    uint64_t inactivity_period = time_us_64() - state->last_activity[BOARD_ROLE]; /* 计算空闲时间 */

    /* If we're not enabled, nothing to do here. */
    if (screensaver->mode == DISABLED)
        return; /* 屏保未启用 */

    /* System is still not idle for long enough to activate or screensaver mode is not supported */
    if (inactivity_period < screensaver->idle_time_us || screensaver->mode > MAX_SS_VAL)
        return; /* 未达到激活空闲时间或模式无效 */

    /* We exceeded the maximum permitted screensaver runtime */
    if (screensaver->max_time_us
        && inactivity_period > (screensaver->max_time_us + screensaver->idle_time_us))
        return; /* 超过允许的屏保最长运行时间 */

    /* If we're the selected output and we can only run on inactive output, nothing to do here. */
    if (screensaver->only_if_inactive && CURRENT_BOARD_IS_ACTIVE_OUTPUT)
        return; /* 仅在非激活输出上运行，但当前是激活输出，则返回 */

    /* We're active! Now check if it's time to move the cursor yet. */
    if (time_us_32() - last_pointer_move < delays[screensaver->mode])
        return; /* 尚未达到下一次移动时间间隔 */

    /* Return, if we're not connected or the host is suspended */
#if !DISABLE_PIO_USB
    if(!tud_ready()) {
        return; /* 如果 TinyUSB 设备栈未准备好（未连接主机或挂起），则不发送屏保移动 */
    }
#else
    /* 如果禁用了 USB，直接返回，屏保功能在无 USB 输出时无意义 */
    return;
#endif

    mouse_report_t *report;
    switch (screensaver->mode) {
        case PONG:
            report = screensaver_pong(state);
            break;

        case JITTER:
            report = screensaver_jitter(state);
            break;

        default:
            return;
    }

    /* Move mouse pointer */
    queue_mouse_report(report, state); /* 将鼠标报告加入队列，由 process_mouse_queue_task 发送 */

    /* Update timer of the last pointer move */
    last_pointer_move = time_us_32(); /* 记录上次移动时间 */
}

/* Periodically emit heartbeat packets */
void heartbeat_output_task(device_t *state) {
    /* If firmware upgrade is in progress, don't touch flash_cs */
    if (state->fw.upgrade_in_progress)
        return; /* 正在升级固件时不发送心跳，避免干扰写 Flash */

    if (state->config_mode_active) {
        /* Leave config mode if timeout expired and user didn't click exit */
        if (time_us_64() > state->config_mode_timer)
            reboot(); /* 配置模式超时则重启 */

        /* Keep notifying the user we're still in config mode */
        blink_led(state); /* 配置模式下闪烁 LED 提示 */
    }

#ifdef DH_DEBUG
    /* Holding the button invokes bootsel firmware upgrade */
    if (is_bootsel_pressed())
        reset_usb_boot(1 << PICO_DEFAULT_LED_PIN, 0);
#endif

    uart_packet_t packet = {
        .type = HEARTBEAT_MSG,
        .data16 = {
            [0] = state->_running_fw.version,
            [2] = state->active_output,
        },
    };

    queue_try_add(&global_state.uart_tx_queue, &packet); /* 将心跳包放入 UART 发送队列，发送给另一块板 */
}


/* Process other outgoing hid report messages. */
void process_hid_queue_task(device_t *state) {
#if !DISABLE_PIO_USB
    hid_generic_pkt_t packet;

    if (!queue_try_peek(&state->hid_queue_out, &packet))
        return; /* 队列为空 */

    if (!tud_hid_n_ready(packet.instance))
        return; /* 指定实例的 HID 不可用 */

    /* ... try sending it to the host, if it's successful */
    bool succeeded = tud_hid_n_report(packet.instance, packet.report_id, packet.data, packet.len);

    /* ... then we can remove it from the queue. Race conditions shouldn't happen [tm] */
    if (succeeded)
        queue_try_remove(&state->hid_queue_out, &packet);
#else
    (void)state; /* USB 禁用时，不处理 HID 上行队列 */
#endif
}

/* Task that handles copying firmware from the other device to ours */
void firmware_upgrade_task(device_t *state) {
    if (!state->fw.upgrade_in_progress || !state->fw.byte_done)
        return; /* 若没有升级在进行或当前没有新字节则返回 */

    if (queue_is_full(&state->uart_tx_queue))
        return; /* UART 发送队列满则等待 */

    /* End condition, when reached the process is completed. */
    if (state->fw.address > STAGING_IMAGE_SIZE) {
        state->fw.upgrade_in_progress = 0;
        state->fw.checksum = ~state->fw.checksum;

        /* Checksum mismatch, we wipe the stage 2 bootloader and rely on ROM recovery */
        if(calculate_firmware_crc32() != state->fw.checksum) {
            flash_range_erase((uint32_t)ADDR_FW_RUNNING - XIP_BASE, FLASH_SECTOR_SIZE);
            reset_usb_boot(1 << PICO_DEFAULT_LED_PIN, 0);
        }

        else {
            state->_running_fw = _firmware_metadata; /* 更新运行中的固件信息 */
            global_state.reboot_requested = true; /* 请求重启以运行新固件 */
        }
    }

    /* If we're on the last element of the current page, page is done - write it. */
    if (TU_U32_BYTE0(state->fw.address) == 0x00) {

        uint32_t page_start_addr = (state->fw.address - 1) & 0xFFFFFF00;
        write_flash_page((uint32_t)ADDR_FW_RUNNING + page_start_addr - XIP_BASE, state->page_buffer);
    }

    request_byte(state, state->fw.address); /* 请求下一字节以继续固件传输 */
}

void packet_receiver_task(device_t *state) {
    uint32_t current_pointer
        = (uint32_t)DMA_RX_BUFFER_SIZE - dma_channel_hw_addr(state->dma_rx_channel)->transfer_count; /* 当前 DMA 写入位置 */
    uint32_t delta = get_ptr_delta(current_pointer, state); /* 计算缓冲区中尚未读取的数据长度 */

    /* If we don't have enough characters for a packet, skip loop and return immediately */
    while (delta >= RAW_PACKET_LENGTH) {
        if (is_start_of_packet(state)) {
            fetch_packet(state); /* 从缓冲区拉取整包 */
            process_packet(&state->in_packet, state); /* 处理收到的包 */
            return; /* 处理完一包后返回，让任务稍后再被调度 */
        }

        /* No packet found, advance to next position and decrement delta */
        state->dma_ptr = NEXT_RING_IDX(state->dma_ptr); /* 环形缓冲区指针前进 */
        delta--;
    }
}

/* End of annotated main.c + task.c */

# 文件: uart.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#include "main.h"

/* ================================================== *
 * ===============  Sending Packets  ================ *
 * ================================================== */

/* Takes a packet as uart_packet_t struct, adds preamble, checksum and encodes it to a raw array. */
void write_raw_packet(uint8_t *dst, uart_packet_t *packet) {
    uint8_t pkt[RAW_PACKET_LENGTH] = {[0] = START1,
                                      [1] = START2,
                                      [2] = packet->type,
                                      /* [3-10] is data, defaults to 0 */
                                      [11] = calc_checksum(packet->data, PACKET_DATA_LENGTH)};

    memcpy(&pkt[START_LENGTH + TYPE_LENGTH], packet->data, PACKET_DATA_LENGTH);
    memcpy(dst, &pkt, RAW_PACKET_LENGTH);
}

/* Schedule packet for sending to the other box */
void queue_packet(const uint8_t *data, enum packet_type_e packet_type, int length) {
    uart_packet_t packet = {.type = packet_type};
    memcpy(packet.data, data, length);

    queue_try_add(&global_state.uart_tx_queue, &packet);
}

/* Sends just one byte of a certain packet type to the other box. */
void send_value(const uint8_t value, enum packet_type_e packet_type) {
    queue_packet(&value, packet_type, sizeof(uint8_t));
}

/* Process outgoing config report messages. */
void process_uart_tx_task(device_t *state) {
    uart_packet_t packet = {0};

    if (dma_channel_is_busy(state->dma_tx_channel))
        return;

    if (!queue_try_remove(&state->uart_tx_queue, &packet))
        return;

    write_raw_packet(uart_txbuf, &packet);
    dma_channel_transfer_from_buffer_now(state->dma_tx_channel, uart_txbuf, RAW_PACKET_LENGTH);
}

/* ================================================== *
 * ===============  Parsing Packets  ================ *
 * ================================================== */

const uart_handler_t uart_handler[] = {
    /* Core functions */
    {.type = KEYBOARD_REPORT_MSG, .handler = handle_keyboard_uart_msg},
    {.type = MOUSE_REPORT_MSG, .handler = handle_mouse_abs_uart_msg},
    {.type = OUTPUT_SELECT_MSG, .handler = handle_output_select_msg},

    /* Box control */
    {.type = MOUSE_ZOOM_MSG, .handler = handle_mouse_zoom_msg},
    {.type = KBD_SET_REPORT_MSG, .handler = handle_set_report_msg},
    {.type = SWITCH_LOCK_MSG, .handler = handle_switch_lock_msg},
    {.type = SYNC_BORDERS_MSG, .handler = handle_sync_borders_msg},
    {.type = FLASH_LED_MSG, .handler = handle_flash_led_msg},
    {.type = GAMING_MODE_MSG, .handler = handle_toggle_gaming_msg},
    {.type = CONSUMER_CONTROL_MSG, .handler = handle_consumer_control_msg},
    {.type = SCREENSAVER_MSG, .handler = handle_screensaver_msg},

    /* Config */
    {.type = WIPE_CONFIG_MSG, .handler = handle_wipe_config_msg},
    {.type = SAVE_CONFIG_MSG, .handler = handle_save_config_msg},
    {.type = REBOOT_MSG, .handler = handle_reboot_msg},
    {.type = GET_VAL_MSG, .handler = handle_api_msgs},
    {.type = GET_ALL_VALS_MSG, .handler = handle_api_read_all_msg},
    {.type = SET_VAL_MSG, .handler = handle_api_msgs},

    /* Firmware */
    {.type = REQUEST_BYTE_MSG, .handler = handle_request_byte_msg},
    {.type = RESPONSE_BYTE_MSG, .handler = handle_response_byte_msg},
    {.type = FIRMWARE_UPGRADE_MSG, .handler = handle_fw_upgrade_msg},

    {.type = HEARTBEAT_MSG, .handler = handle_heartbeat_msg},
    {.type = PROXY_PACKET_MSG, .handler = handle_proxy_msg},
};

void process_packet(uart_packet_t *packet, device_t *state) {
    if (!verify_checksum(packet))
        return;

    for (int i = 0; i < ARRAY_SIZE(uart_handler); i++) {
        if (uart_handler[i].type == packet->type) {
            uart_handler[i].handler(packet, state);
            return;
        }
    }
}

# 文件: usb.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#include "main.h" /* 包含项目主头文件 */

/* ================================================== *
 * ===========  TinyUSB Device Callbacks  =========== *
 * ================================================== */

#if !DISABLE_PIO_USB
/* Invoked when we get GET_REPORT control request.
 * We are expected to fill buffer with the report content, update reqlen
 * and return its length. We return 0 to STALL the request. */
uint16_t tud_hid_get_report_cb(uint8_t instance,
                               uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer,
                               uint16_t request_len) {
    return 0; /* 未实现 GET_REPORT，返回 0 表示 STALL */
}

/**
 * Computer controls our LEDs by sending USB SetReport messages with a payload
 * of just 1 byte and report type output. It's type 0x21 (USB_REQ_DIR_OUT |
 * USB_REQ_TYP_CLASS | USB_REQ_REC_IFACE) Request code for SetReport is 0x09,
 * report type is 0x02 (HID_REPORT_TYPE_OUTPUT). We get a set_report callback
 * from TinyUSB device HID and then figure out what to do with the LEDs.
 */
void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer,
                           uint16_t bufsize) {

    /* We received a report on the config report ID */
    if (instance == ITF_NUM_HID_VENDOR && report_id == REPORT_ID_VENDOR) {
        /* Security - only if config mode is enabled are we allowed to do anything. While the report_id
           isn't even advertised when not in config mode, security must always be explicit and never assume */
        if (!global_state.config_mode_active)
            return; /* 如果未进入配置模式，拒绝配置包 */

        /* We insist on a fixed size packet. No overflows. */
        if (bufsize != RAW_PACKET_LENGTH)
            return; /* 包长度不符，丢弃 */

        uart_packet_t *packet = (uart_packet_t *) (buffer + START_LENGTH); /* 解析出 uart_packet_t 结构 */

        /* Only a certain packet types are accepted */
        if (!validate_packet(packet))
            return; /* 校验失败，丢弃 */

        process_packet(packet, &global_state); /* 处理配置包 */
    }

    /* Only other set report we care about is LED state change, and that's exactly 1 byte long */
    if (report_id != REPORT_ID_KEYBOARD || bufsize != 1 || report_type != HID_REPORT_TYPE_OUTPUT)
        return; /* 非键盘 LED 或者长度不为1 则忽略 */

    uint8_t leds = buffer[0]; /* 读取 LED 状态字节 */

    /* If we are using caps lock LED to indicate the chosen output, that has priority */
    if (global_state.config.kbd_led_as_indicator) {
        leds = leds & 0xFD; /* 1111 1101 (Clear Caps Lock bit) */

        if (global_state.active_output)
            leds |= KEYBOARD_LED_CAPSLOCK; /* 根据活动输出设置 Caps LED */
    }

    global_state.keyboard_leds[BOARD_ROLE] = leds; /* 保存当前键盘 LED 状态 */

    /* If the board has a keyboard connected directly, restore those leds. */
    if (global_state.keyboard_connected && CURRENT_BOARD_IS_ACTIVE_OUTPUT)
        restore_leds(&global_state); /* 恢复实际连接键盘的 LED 状态 */

    /* Always send to the other one, so it is aware of the change */
    send_value(leds, KBD_SET_REPORT_MSG); /* 将 LED 状态通过协议发送给另一块板 */
}

/* Invoked when device is mounted */
void tud_mount_cb(void) {
    global_state.tud_connected = true; /* 标记为 USB 设备已挂载 */
}

/* Invoked when device is unmounted */
void tud_umount_cb(void) {
    global_state.tud_connected = false; /* 标记为 USB 设备已卸载 */
}
#endif /* !DISABLE_PIO_USB */

/* ================================================== *
 * ===============  USB HOST Section  =============== *
 * ================================================== */

#if !DISABLE_PIO_USB
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    uint8_t itf_protocol = tuh_hid_interface_protocol(dev_addr, instance); /* 获取接口协议（键盘/鼠标） */

    if (dev_addr >= MAX_DEVICES || instance > MAX_INTERFACES)
        return; /* 越界检查 */

    hid_interface_t *iface = &global_state.iface[dev_addr-1][instance]; /* 获取本地接口结构体 */

    switch (itf_protocol) {
        case HID_ITF_PROTOCOL_KEYBOARD:
            global_state.keyboard_connected = false; /* 键盘卸载，标记为未连接 */
            break;

        case HID_ITF_PROTOCOL_MOUSE:
            break;
    }

    /* Also clear the interface structure, otherwise plugging something else later
       might be a fun (and confusing) experience */
    memset(iface, 0, sizeof(hid_interface_t)); /* 清零接口结构，避免后续混乱 */
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
    uint8_t itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (dev_addr >= MAX_DEVICES || instance > MAX_INTERFACES)
        return; /* 越界保护 */

    /* Get interface information */
    hid_interface_t *iface = &global_state.iface[dev_addr-1][instance];

    iface->protocol = tuh_hid_get_protocol(dev_addr, instance); /* 获取当前接口的协议 */

    /* Safeguard against memory corruption in case the number of instances exceeds our maximum */
    if (instance >= MAX_INTERFACES)
        return; /* 再次保护 */

    /* Parse the report descriptor into our internal structure. */
    parse_report_descriptor(iface, desc_report, desc_len); /* 解析 HID 报告描述符 */

    switch (itf_protocol) {
        case HID_ITF_PROTOCOL_KEYBOARD:
            if (global_state.config.enforce_ports && BOARD_ROLE == OUTPUT_B)
                return; /* 根据配置强制端口策略，可能拒绝连接 */

            if (global_state.config.force_kbd_boot_protocol)
                tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT); /* 强制键盘使用 boot 协议 */

            /* Keeping this is required for setting leds from device set_report callback */
            global_state.kbd_dev_addr       = dev_addr; /* 保存主键盘地址 */
            global_state.kbd_instance       = instance; /* 保存实例号 */
            global_state.keyboard_connected = true; /* 标记键盘已连接 */
            break;

        case HID_ITF_PROTOCOL_MOUSE:
            if (global_state.config.enforce_ports && BOARD_ROLE == OUTPUT_A)
                return; /* 强制端口策略，拒绝鼠标 */

            /* Switch to using report protocol instead of boot, it's more complicated but
               at least we get all the information we need (looking at you, mouse wheel) */
            if (tuh_hid_get_protocol(dev_addr, instance) == HID_PROTOCOL_BOOT) {
                tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT); /* 切换到 report 协议以获取完整信息 */
            }
            break;

        case HID_ITF_PROTOCOL_NONE:
            break;
    }
    /* Flash local led to indicate a device was connected */
    blink_led(&global_state); /* 闪烁本地 LED，提示有设备连接 */

    /* Also signal the other board to flash LED, to enable easy verification if serial works */
    send_value(ENABLE, FLASH_LED_MSG); /* 发送消息给另一板，要求其也闪烁 LED */

    /* Kick off the report querying */
    tuh_hid_receive_report(dev_addr, instance); /* 请求第一个中断报告，开始周期性查询 */
}

/* Invoked when received report from device via interrupt endpoint */
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance); /* 获取接口协议 */

    if (dev_addr >= MAX_DEVICES || instance > MAX_INTERFACES)
        return; /* 越界保护 */

    hid_interface_t *iface = &global_state.iface[dev_addr-1][instance]; /* 获取接口结构 */

    /* Safeguard against memory corruption in case the number of instances exceeds our maximum */
    if (instance >= MAX_INTERFACES)
        return; /* 保护 */

    /* Calculate a device index that distinguishes between different devices
       while staying within the bounds of MAX_DEVICES.

       Device index assignment:
       - 0: Primary keyboard (the one set in tuh_hid_mount_cb)
       - 1: Mouse devices
       - MAX_DEVICES-2: Secondary keyboards (e.g., wireless keyboard through unified dongle)
       - (dev_addr-1) % (MAX_DEVICES-1): Other devices

       Note: Slot MAX_DEVICES-1 is reserved for the remote device (used in handle_keyboard_uart_msg) */
    uint8_t device_idx;

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        if (dev_addr == global_state.kbd_dev_addr && instance == global_state.kbd_instance) {
            /* Primary keyboard */
            device_idx = 0;
        } else {
            /* Secondary keyboard (e.g., wireless keyboard through unified dongle) */
            device_idx = (MAX_DEVICES - 2);
        }
    } else if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        /* Mouse devices */
        device_idx = 1;
    } else {
        /* Other devices */
        device_idx = (dev_addr - 1) % (MAX_DEVICES - 1);
    }

    if (iface->uses_report_id || itf_protocol == HID_ITF_PROTOCOL_NONE) {
        uint8_t report_id = 0;

        if (iface->uses_report_id)
            report_id = report[0]; /* 第一个字节为 report id */

        if (report_id < MAX_REPORTS) {
            process_report_f receiver = iface->report_handler[report_id]; /* 取出对应的处理函数 */

            if (receiver != NULL)
                receiver((uint8_t *)report, len, device_idx, iface); /* 调用对应的报文处理函数 */
        }
    }
    else if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        process_keyboard_report((uint8_t *)report, len, device_idx, iface); /* 处理键盘报文（boot 协议） */
    }
    else if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        process_mouse_report((uint8_t *)report, len, device_idx, iface); /* 处理鼠标报文 */
    }

    /* Continue requesting reports */
    tuh_hid_receive_report(dev_addr, instance); /* 继续请求下一个中断报告 */
}

/* Set protocol in a callback. This is tied to an interface, not a specific report ID */
void tuh_hid_set_protocol_complete_cb(uint8_t dev_addr, uint8_t idx, uint8_t protocol) {
    if (dev_addr >= MAX_DEVICES || idx > MAX_INTERFACES)
        return; /* 越界保护 */

    hid_interface_t *iface = &global_state.iface[dev_addr-1][idx]; /* 获取接口并保存协议 */
    iface->protocol = protocol; /* 更新协议 */
}
#endif /* !DISABLE_PIO_USB */

/* ================================================== *
 * 新增：UART1 初始化（用于从 A 板接收调试数据）
 * GPIO8 = UART1_TX (用于本地调试输出)
 * GPIO9 = UART1_RX (用于接收 A 板发来的数据)
 * ================================================== */

/* UART1 初始化函数实现，此函数在 setup.c 的 initial_setup 中被调用 */
void serial1_init(void) {
    /* 对 UART1 做类似 UART0 的初始化，但不配置 DMA（如果需要可扩展） */
    uart_init(uart1, 115200); /* 初始化 UART1，使用115200的波特率 */

    uart_set_hw_flow(uart1, false, false); /* 关闭硬件流控 */
    uart_set_format(uart1, SERIAL_DATA_BITS, SERIAL_STOP_BITS, SERIAL_PARITY); /* 设置数据格式 */
    uart_set_translate_crlf(uart1, false); /* 关闭 CR/LF 转换 */
    uart_set_fifo_enabled(uart1, true); /* 启用 FIFO */

    /* 将 GPIO8/GPIO9 复用为 UART1 功能：GPIO8 为 TX，GPIO9 为 RX */
    gpio_set_function(8, GPIO_FUNC_UART); /* 将 GPIO8 设为 UART 功能（TX） */
    gpio_set_function(9, GPIO_FUNC_UART); /* 将 GPIO9 设为 UART 功能（RX） */

    /* 若需要可在此注册中断或启动轮询读取 UART1 的任务 */
}

/* ==========  End of usb.c portion ========== */

# 文件: usb_descriptors.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#include "usb_descriptors.h"
#include "main.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+

                                        // https://github.com/raspberrypi/usb-pid
tusb_desc_device_t const desc_device = DEVICE_DESCRIPTOR(0x2e8a, 0x107c);

                                        // https://pid.codes/1209/C000/
tusb_desc_device_t const desc_device_config = DEVICE_DESCRIPTOR(0x1209, 0xc000);

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void) {
    if (global_state.config_mode_active)
        return (uint8_t const *)&desc_device_config;
    else
        return (uint8_t const *)&desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

// Relative mouse is used to overcome limitations of multiple desktops on MacOS and Windows

uint8_t const desc_hid_report[] = {TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
                                   TUD_HID_REPORT_DESC_ABS_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
                                   TUD_HID_REPORT_DESC_CONSUMER_CTRL(HID_REPORT_ID(REPORT_ID_CONSUMER)),
                                   TUD_HID_REPORT_DESC_SYSTEM_CONTROL(HID_REPORT_ID(REPORT_ID_SYSTEM))
                                   };

uint8_t const desc_hid_report_relmouse[] = {TUD_HID_REPORT_DESC_MOUSEHELP(HID_REPORT_ID(REPORT_ID_RELMOUSE))};

uint8_t const desc_hid_report_vendor[] = {TUD_HID_REPORT_DESC_VENDOR_CTRL(HID_REPORT_ID(REPORT_ID_VENDOR))};


// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    if (global_state.config_mode_active)
        if (instance == ITF_NUM_HID_VENDOR)
            return desc_hid_report_vendor;

    switch(instance) {
        case ITF_NUM_HID:
            return desc_hid_report;
        case ITF_NUM_HID_REL_M:
            return desc_hid_report_relmouse;
        default:
            return desc_hid_report;
    }
}

bool tud_mouse_report(uint8_t mode, uint8_t buttons, int16_t x, int16_t y, int8_t wheel, int8_t pan) {
    mouse_report_t report = {.buttons = buttons, .wheel = wheel, .x = x, .y = y, .mode = mode, .pan = pan};
    uint8_t instance = ITF_NUM_HID;
    uint8_t report_id = REPORT_ID_MOUSE;

    if (mode == RELATIVE) {
        instance = ITF_NUM_HID_REL_M;
        report_id = REPORT_ID_RELMOUSE;
    }

    return tud_hid_n_report(instance, report_id, &report, sizeof(report));
}


//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "Hrvoje Cavrak",            // 1: Manufacturer
    "DeskHop Switch",           // 2: Product
    "0",                        // 3: Serials, should use chip ID
    "DeskHop Helper",           // 4: Mouse Helper Interface
    "DeskHop Config",           // 5: Vendor Interface
    "DeskHop Disk",             // 6: Disk Interface
#ifdef DH_DEBUG
    "DeskHop Debug",            // 7: Debug Interface
#endif
};

// String Descriptor Index
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_MOUSE,
    STRID_VENDOR,
    STRID_DISK,
    STRID_DEBUG,
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to
// complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    uint8_t chr_count;

    // 2 (hex) characters for every byte + 1 '\0' for string end
    static char serial_number[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1] = {0};

    if (!serial_number[0]) {
       pico_get_unique_board_id_string(serial_number, sizeof(serial_number));
    }

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
            return NULL;

        const char *str = (index == STRID_SERIAL) ? serial_number : string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31)
            chr_count = 31;

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

#define EPNUM_HID        0x81
#define EPNUM_HID_REL_M  0x82
#define EPNUM_HID_VENDOR 0x83

#define EPNUM_MSC_OUT    0x04
#define EPNUM_MSC_IN     0x84

#ifndef DH_DEBUG

#define ITF_NUM_TOTAL 2
#define ITF_NUM_TOTAL_CONFIG 3
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + 2 * TUD_HID_DESC_LEN)
#define CONFIG_TOTAL_LEN_CFG (TUD_CONFIG_DESC_LEN + 2 * TUD_HID_DESC_LEN + TUD_MSC_DESC_LEN)

#else
#define ITF_NUM_CDC 3
#define ITF_NUM_TOTAL 3
#define ITF_NUM_TOTAL_CONFIG 4

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + 2 * TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)
#define CONFIG_TOTAL_LEN_CFG (TUD_CONFIG_DESC_LEN + 2 * TUD_HID_DESC_LEN + TUD_MSC_DESC_LEN + TUD_CDC_DESC_LEN)

#define EPNUM_CDC_NOTIF  0x85
#define EPNUM_CDC_OUT    0x06
#define EPNUM_CDC_IN     0x86

#endif


uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID,
                       STRID_PRODUCT,
                       HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report),
                       EPNUM_HID,
                       CFG_TUD_HID_EP_BUFSIZE,
                       1),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID_REL_M,
                       STRID_MOUSE,
                       HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report_relmouse),
                       EPNUM_HID_REL_M,
                       CFG_TUD_HID_EP_BUFSIZE,
                       1),
#ifdef DH_DEBUG
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(
        ITF_NUM_CDC, STRID_DEBUG, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, CFG_TUD_CDC_EP_BUFSIZE),
#endif
};

uint8_t const desc_configuration_config[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_CONFIG, 0, CONFIG_TOTAL_LEN_CFG, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID,
                       STRID_PRODUCT,
                       HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report),
                       EPNUM_HID,
                       CFG_TUD_HID_EP_BUFSIZE,
                       1),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID_VENDOR,
                       STRID_VENDOR,
                       HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report_vendor),
                       EPNUM_HID_VENDOR,
                       CFG_TUD_HID_EP_BUFSIZE,
                       1),

    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC,
                       STRID_DISK,
                       EPNUM_MSC_OUT,
                       EPNUM_MSC_IN,
                       64),
#ifdef DH_DEBUG
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(
        ITF_NUM_CDC, STRID_DEBUG, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, CFG_TUD_CDC_EP_BUFSIZE),
#endif
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index; // for multiple configurations

    if (global_state.config_mode_active)
        return desc_configuration_config;
    else
        return desc_configuration;
}

# 文件: utils.c

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#include "main.h"

/* ================================================== *
 * ==============  Checksum Functions  ============== *
 * ================================================== */

uint8_t calc_checksum(const uint8_t *data, int length) {
    uint8_t checksum = 0;

    for (int i = 0; i < length; i++) {
        checksum ^= data[i];
    }

    return checksum;
}

bool verify_checksum(const uart_packet_t *packet) {
    uint8_t checksum = calc_checksum(packet->data, PACKET_DATA_LENGTH);
    return checksum == packet->checksum;
}

uint32_t crc32_iter(uint32_t crc, const uint8_t byte) {
    return crc32_lookup_table[(byte ^ crc) & 0xff] ^ (crc >> 8);
}

/* TODO - use DMA sniffer's built-in CRC32 */
uint32_t calc_crc32(const uint8_t *s, size_t n) {
    uint32_t crc = 0xffffffff;

    for(size_t i=0; i < n; i++) {
        crc = crc32_iter(crc, s[i]);
    }

    return ~crc;
}

uint32_t calculate_firmware_crc32(void) {
    return calc_crc32(ADDR_FW_RUNNING, STAGING_IMAGE_SIZE - FLASH_SECTOR_SIZE);
}

/* ================================================== *
 * Flash and config functions
 * ================================================== */

void wipe_config(void) {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase((uint32_t)ADDR_CONFIG - XIP_BASE, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

void write_flash_page(uint32_t target_addr, uint8_t *buffer) {
    /* Start of sector == first 256-byte page in a 4096 byte block */
    bool is_sector_start = (target_addr & 0xf00) == 0;

    uint32_t ints = save_and_disable_interrupts();
    if (is_sector_start)
        flash_range_erase(target_addr, FLASH_SECTOR_SIZE);

    flash_range_program(target_addr, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

void load_config(device_t *state) {
    const config_t *config   = ADDR_CONFIG;
    config_t *running_config = &state->config;

    /* Load the flash config first, including the checksum */
    memcpy(running_config, config, sizeof(config_t));

    /* Calculate and update checksum, size without checksum */
    uint8_t checksum = calc_crc32((uint8_t *)running_config, sizeof(config_t) - sizeof(uint32_t));

    /* We expect a certain byte to start the config header */
    bool magic_header_fail = (running_config->magic_header != 0xB00B1E5);

    /* We expect the checksum to match */
    bool checksum_fail = (running_config->checksum != checksum);

    /* We expect the config version to match exactly, to avoid erroneous values */
    bool version_fail = (running_config->version != CURRENT_CONFIG_VERSION);

    /* On any condition failing, we fall back to default config */
    if (magic_header_fail || checksum_fail || version_fail)
        memcpy(running_config, &default_config, sizeof(config_t));
}

void save_config(device_t *state) {
    uint8_t *raw_config = (uint8_t *)&state->config;

    /* Calculate and update checksum, size without checksum */
    uint8_t checksum       = calc_crc32(raw_config, sizeof(config_t) - sizeof(uint32_t));
    state->config.checksum = checksum;

    /* Copy the config to buffer and pad the rest with zeros */
    memcpy(state->page_buffer, raw_config, sizeof(config_t));
    memset(state->page_buffer + sizeof(config_t), 0, FLASH_PAGE_SIZE - sizeof(config_t));

    /* Write the new config to flash */
    write_flash_page((uint32_t)ADDR_CONFIG - XIP_BASE, state->page_buffer);
}

void reset_config_timer(device_t *state) {
    /* Once this is reached, we leave the config mode */
    state->config_mode_timer = time_us_64() + CONFIG_MODE_TIMEOUT;
}

void _configure_flash_cs(enum gpio_override gpo, uint pin_index) {
  hw_write_masked(&ioqspi_hw->io[pin_index].ctrl,
                  gpo << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
}

bool is_bootsel_pressed(void) {
  const uint CS_PIN_INDEX = 1;
  uint32_t flags = save_and_disable_interrupts();

  /* Set chip select to high impedance */
  _configure_flash_cs(GPIO_OVERRIDE_LOW, CS_PIN_INDEX);
  sleep_us(20);

  /* Button pressed pulls pin DOWN, so invert */
  bool button_pressed = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

  /* Restore chip select state */
  _configure_flash_cs(GPIO_OVERRIDE_NORMAL, CS_PIN_INDEX);
  restore_interrupts(flags);

  return button_pressed;
}

void request_byte(device_t *state, uint32_t address) {
    uart_packet_t packet = {
        .data32[0] = address,
        .type = REQUEST_BYTE_MSG,
    };
    state->fw.byte_done = false;

    queue_try_add(&global_state.uart_tx_queue, &packet);
}

void reboot(void) {
    *((volatile uint32_t*)(PPB_BASE + 0x0ED0C)) = 0x5FA0004;
}

bool is_start_of_packet(device_t *state) {
    return (uart_rxbuf[state->dma_ptr] == START1 && uart_rxbuf[NEXT_RING_IDX(state->dma_ptr)] == START2);
}

uint32_t get_ptr_delta(uint32_t current_pointer, device_t *state) {
    uint32_t delta;

    if (current_pointer >= state->dma_ptr)
        delta = current_pointer - state->dma_ptr;
    else
        delta = DMA_RX_BUFFER_SIZE - state->dma_ptr + current_pointer;

    /* Clamp to 12 bits since it can never be bigger */
    delta = delta & 0x3FF;

    return delta;
}

void fetch_packet(device_t *state) {
    uint8_t *dst = (uint8_t *)&state->in_packet;

    for (int i = 0; i < RAW_PACKET_LENGTH; i++) {
        /* Skip the header preamble */
        if (i >= START_LENGTH)
            dst[i - START_LENGTH] = uart_rxbuf[state->dma_ptr];

        state->dma_ptr = NEXT_RING_IDX(state->dma_ptr);
    }
}

/* Validating any input is mandatory. Only packets of these type are allowed
   to be sent to the device over configuration endpoint. */
bool validate_packet(uart_packet_t *packet) {
    const enum packet_type_e ALLOWED_PACKETS[] = {
        FLASH_LED_MSG,
        GET_VAL_MSG,
        GET_ALL_VALS_MSG,
        SET_VAL_MSG,
        WIPE_CONFIG_MSG,
        SAVE_CONFIG_MSG,
        REBOOT_MSG,
        PROXY_PACKET_MSG,
    };
    uint8_t packet_type = packet->type;

    /* Proxied packets are encapsulated in the data field, but same rules apply */
    if (packet->type == PROXY_PACKET_MSG)
        packet_type = packet->data[0];

    for (int i = 0; i < ARRAY_SIZE(ALLOWED_PACKETS); i++) {
        if (ALLOWED_PACKETS[i] == packet_type)
            return true;
    }
    return false;
}


/* ================================================== *
 * Debug functions
 * ================================================== */
#ifdef DH_DEBUG

int dh_debug_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[512];

    int string_len = vsnprintf(buffer, 512, format, args);

    tud_cdc_n_write(0, buffer, string_len);
    tud_cdc_write_flush();

    va_end(args);
    return string_len;
}
#else

int dh_debug_printf(const char *format, ...) {
    return 0;
}

#endif

# 文件: include/config.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include <stdint.h>
#include "structs.h"
#include "misc.h"
#include "screen.h"

#define CURRENT_CONFIG_VERSION 8

/*==============================================================================
 *  Configuration Data
 *  Structures and variables related to device configuration.
 *==============================================================================*/

extern const config_t default_config;

/*==============================================================================
 *  Configuration API
 *  Functions and data structures for accessing and modifying configuration.
 *==============================================================================*/

extern const field_map_t api_field_map[];
const field_map_t* get_field_map_entry(uint32_t);
const field_map_t* get_field_map_index(uint32_t);
size_t             get_field_map_length(void);

/*==============================================================================
 *  Configuration Management and Packet Processing
 *  Functions for loading, saving, wiping, and resetting device configuration.
 *==============================================================================*/

void load_config(device_t *);
void queue_cfg_packet(uart_packet_t *, device_t *);
void reset_config_timer(device_t *);
void save_config(device_t *);
bool validate_packet(uart_packet_t *);
void wipe_config(void);

# 文件: include/constants.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once
#include <stdint.h>

/*==============================================================================
 *  Board and Output Roles
 *==============================================================================*/

#define OUTPUT_A 0
#define OUTPUT_B 1

/*==============================================================================
 *  HID Interface Numbers
 *==============================================================================*/

#define ITF_NUM_HID        0
#define ITF_NUM_HID_REL_M  1
#define ITF_NUM_HID_VENDOR 1
#define ITF_NUM_MSC        2

/*==============================================================================
 *  Mouse Modes
 *==============================================================================*/

#define ABSOLUTE 0
#define RELATIVE 1
#define TOUCH 2

/*==============================================================================
 *  Boolean States
 *==============================================================================*/

#define ENABLE  1
#define DISABLE 0

/*==============================================================================
 *  Numerical Constants
 *==============================================================================*/

#define CONFIG_MODE_TIMEOUT 300000000 // 5 minutes into the future
#define JITTER_DISTANCE 2
#define MOUSE_BOOT_REPORT_LEN 4
#define MOUSE_ZOOM_SCALING_FACTOR 2
#define NUM_SCREENS 2

/*==============================================================================
 *  Utility Macros
 *==============================================================================*/

#define _HZ(x) ((uint64_t)((1000000) / (x)))
#define _MS(x) (x * 1000)
#define _SEC(x) (x * 1000000)
#define _TOP()  0
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define CURRENT_BOARD_IS_ACTIVE_OUTPUT (global_state.active_output == global_state.board_role)

# 文件: include/dma.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include <hardware/dma.h>

/*==============================================================================
 *  DMA Buffer Sizes
 *==============================================================================*/

#define DMA_RX_BUFFER_SIZE 1024
#define DMA_TX_BUFFER_SIZE 32

/*==============================================================================
 *  DMA Buffers
 *==============================================================================*/

extern uint8_t uart_rxbuf[DMA_RX_BUFFER_SIZE] __attribute__((aligned(DMA_RX_BUFFER_SIZE)));
extern uint8_t uart_txbuf[DMA_TX_BUFFER_SIZE] __attribute__((aligned(DMA_TX_BUFFER_SIZE)));

/*==============================================================================
 *  Ring Buffer Macro
 *==============================================================================*/

#define NEXT_RING_IDX(x) ((x + 1) & 0x3FF)

# 文件: include/firmware.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
 #pragma once

 #include "structs.h"

 /*==============================================================================
  *  Firmware Update Functions
  *  Functions for managing firmware updates, CRC calculation, and related tasks.
  *==============================================================================*/

 uint32_t calculate_firmware_crc32(void);
 void     reboot(void);
 void     write_flash_page(uint32_t, uint8_t *);

 /*==============================================================================
  *  UART Packet Fetching
  *  Functions to handle incoming UART packets, especially for firmware updates.
  *==============================================================================*/
 void     fetch_packet(device_t *);
 uint32_t get_ptr_delta(uint32_t, device_t *);
 bool     is_start_of_packet(device_t *);
 void     request_byte(device_t *, uint32_t);

 /*==============================================================================
  *  Button Interaction
  *  Functions interacting with the button, e.g. checking if pressed.
  *==============================================================================*/

 bool is_bootsel_pressed(void);

# 文件: include/flash.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#pragma once
#include <stdint.h>
#include <hardware/flash.h>

/*==============================================================================
 *  Firmware Metadata
 *==============================================================================*/

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint32_t checksum;
} firmware_metadata_t;

extern firmware_metadata_t _firmware_metadata;
#define FIRMWARE_METADATA_MAGIC   0xf00d

/*==============================================================================
 *  Firmware Transfer Packet
 *==============================================================================*/

typedef struct {
    uint8_t cmd;          // Byte 0 = command
    uint16_t page_number; // Bytes 1-2 = page number
    union {
        uint8_t offset;   // Byte 3 = offset
        uint8_t checksum; // In write packets, it's checksum
    };
    uint8_t data[4]; // Bytes 4-7 = data
} fw_packet_t;

/*==============================================================================
 *  Flash Memory Layout
 *==============================================================================*/

#define RUNNING_FIRMWARE_SLOT     0
#define STAGING_FIRMWARE_SLOT     1
#define STAGING_PAGES_CNT         1024
#define STAGING_IMAGE_SIZE        STAGING_PAGES_CNT * FLASH_PAGE_SIZE

/*==============================================================================
*  Lookup Tables
*==============================================================================*/

extern const uint32_t crc32_lookup_table[];

/*==============================================================================
 *  UF2 Firmware Format Structure
 *==============================================================================*/
typedef struct {
    uint32_t magicStart0;
    uint32_t magicStart1;
    uint32_t flags;
    uint32_t targetAddr;
    uint32_t payloadSize;
    uint32_t blockNo;
    uint32_t numBlocks;
    uint32_t fileSize;
    uint8_t data[476];
    uint32_t magicEnd;
} uf2_t;

#define UF2_MAGIC_START0 0x0A324655
#define UF2_MAGIC_START1 0x9E5D5157
#define UF2_MAGIC_END    0x0AB16F30

# 文件: include/handlers.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include "structs.h"
#include "keyboard.h"

/*==============================================================================
 *  Hotkey Handlers
 *  These handlers are invoked when specific hotkey combinations are detected.
 *==============================================================================*/

void config_enable_hotkey_handler(device_t *, hid_keyboard_report_t *);
void disable_screensaver_hotkey_handler(device_t *, hid_keyboard_report_t *);
void enable_screensaver_hotkey_handler(device_t *, hid_keyboard_report_t *);
void fw_upgrade_hotkey_handler_A(device_t *, hid_keyboard_report_t *);
void fw_upgrade_hotkey_handler_B(device_t *, hid_keyboard_report_t *);
void mouse_zoom_hotkey_handler(device_t *, hid_keyboard_report_t *);
void output_config_hotkey_handler(device_t *, hid_keyboard_report_t *);
void output_toggle_hotkey_handler(device_t *, hid_keyboard_report_t *);
void screen_border_hotkey_handler(device_t *, hid_keyboard_report_t *);
void screenlock_hotkey_handler(device_t *, hid_keyboard_report_t *);
void switchlock_hotkey_handler(device_t *, hid_keyboard_report_t *);
void toggle_gaming_mode_handler(device_t *, hid_keyboard_report_t *);
void wipe_config_hotkey_handler(device_t *, hid_keyboard_report_t *);

/*==============================================================================
 *  UART Message Handlers
 *  These handlers process incoming messages received over the UART interface.
 *==============================================================================*/

void handle_api_msgs(uart_packet_t *, device_t *);
void handle_api_read_all_msg(uart_packet_t *, device_t *);
void handle_consumer_control_msg(uart_packet_t *, device_t *);
void handle_flash_led_msg(uart_packet_t *, device_t *);
void handle_fw_upgrade_msg(uart_packet_t *, device_t *);
void handle_toggle_gaming_msg(uart_packet_t *, device_t *);
void handle_heartbeat_msg(uart_packet_t *, device_t *);
void handle_keyboard_uart_msg(uart_packet_t *, device_t *);
void handle_mouse_abs_uart_msg(uart_packet_t *, device_t *);
void handle_mouse_zoom_msg(uart_packet_t *, device_t *);
void handle_output_select_msg(uart_packet_t *, device_t *);
void handle_proxy_msg(uart_packet_t *, device_t *);
void handle_read_config_msg(uart_packet_t *, device_t *);
void handle_reboot_msg(uart_packet_t *, device_t *);
void handle_request_byte_msg(uart_packet_t *, device_t *);
void handle_response_byte_msg(uart_packet_t *, device_t *);
void handle_save_config_msg(uart_packet_t *, device_t *);
void handle_screensaver_msg(uart_packet_t *, device_t *);
void handle_set_report_msg(uart_packet_t *, device_t *);
void handle_switch_lock_msg(uart_packet_t *, device_t *);
void handle_sync_borders_msg(uart_packet_t *, device_t *);
void handle_wipe_config_msg(uart_packet_t *, device_t *);
void handle_write_fw_msg(uart_packet_t *, device_t *);

/*==============================================================================
 *  Output Control
 *  Functions related to managing the active output.
 *==============================================================================*/

void set_active_output(device_t *, uint8_t);

# 文件: include/hid_parser.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2024 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include "main.h"
#include "tusb.h"

/*==============================================================================
 *  Constants
 *==============================================================================*/

#define HID_DEFAULT_NUM_COLLECTIONS 16
#define HID_MAX_USAGES              128
#define MAX_CC_BUTTONS              16
#define MAX_DEVICES                 3
#define MAX_INTERFACES              6
#define MAX_KEYS                    32
#define MAX_REPORTS                 24
#define MAX_SYS_BUTTONS             8

/*==============================================================================
 *  Data Structures
 *==============================================================================*/

/* Counts how many collection starts and ends we've seen, when they equalize
   (and not zero), we are at the end of a block */
typedef struct {
    uint8_t start;
    uint8_t end;
} collection_t;

/* Header byte is unpacked to size/type/tag using this struct */
typedef struct TU_ATTR_PACKED {
    uint8_t size : 2;
    uint8_t type : 2;
    uint8_t tag : 4;
} header_t;

/* We store a header block and corresponding data in an array of these
   to avoid having to use numerous switch-case checks */
typedef struct {
    header_t hdr;
    uint32_t val;
} item_t;

typedef enum {
    DATA = 0,
    CONSTANT,
    ARRAY,
    VARIABLE,
    ABSOLUTE_DATA,
    RELATIVE_DATA,
    NO_WRAP,
    WRAP,
    LINEAR,
    NONLINEAR,
} data_type_e;

// Extended precision mouse movement information
typedef struct {
    int32_t move_x;
    int32_t move_y;
    int32_t wheel;
    int32_t pan;
    int32_t buttons;
} mouse_values_t;

/* Describes where can we find a value in a HID report */
typedef struct TU_ATTR_PACKED {
    uint16_t offset;     // In bits
    uint16_t offset_idx; // In bytes
    uint16_t size;       // In bits

    int32_t usage_min;
    int32_t usage_max;

    uint8_t item_type;
    uint8_t data_type;

    uint8_t report_id;
    uint16_t global_usage;
    uint16_t usage_page;
    uint16_t usage;
} report_val_t;

/* Defines information about HID report format for the mouse. */
typedef struct {
    report_val_t buttons;
    report_val_t move_x;
    report_val_t move_y;
    report_val_t wheel;
    report_val_t pan;

    uint8_t report_id;

    bool is_found;
    bool uses_report_id;
} mouse_t;

typedef struct hid_interface_t hid_interface_t;
typedef void (*process_report_f)(uint8_t *, int, uint8_t, hid_interface_t *);

/* Defines information about HID report format for the keyboard. */
typedef struct {
    report_val_t modifier;
    report_val_t nkro;
    uint16_t cc_array[MAX_CC_BUTTONS];
    uint16_t sys_array[MAX_SYS_BUTTONS];
    bool key_array[MAX_KEYS];

    uint8_t report_id;
    uint8_t key_array_idx;

    bool uses_report_id;
    bool is_found;
    bool is_nkro;
} keyboard_t;

typedef struct {
    report_val_t val;
    uint8_t report_id;
    bool is_variable;
    bool is_array;
} report_t;

struct hid_interface_t {
    keyboard_t keyboard;
    mouse_t mouse;
    report_t consumer;
    report_t system;
    process_report_f report_handler[MAX_REPORTS];
    uint8_t protocol;
    bool uses_report_id;
};

typedef struct {
    report_val_t *map;
    int map_index; /* Index of the current element we've found */
    int report_id; /* Report ID of the current section we're parsing */

    uint32_t usage_count;
    uint32_t offset_in_bits;
    uint16_t usages[HID_MAX_USAGES];
    uint16_t *p_usage;
    uint16_t global_usage;

    collection_t collection;

    /* as tag is 4 bits, there can be 16 different tags in global header type */
    item_t globals[16];

    /* as tag is 4 bits, there can be 16 different tags in local header type */
    item_t locals[16];
} parser_state_t;

///////////////

# 文件: include/hid_report.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include "main.h"

/*==============================================================================
 *  Function Pointer Definitions
 *==============================================================================*/

typedef void (*value_handler_f)(report_val_t *, report_val_t *, hid_interface_t *);

/*==============================================================================
 *  Data Structures
 *==============================================================================*/

typedef struct {
    int global_usage;
    int usage_page;
    int usage;
    uint8_t *id;
    report_val_t *dst;
    value_handler_f handler;
    process_report_f receiver;
} usage_map_t;

# 文件: include/keyboard.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#pragma once

#include "structs.h"
#include "hid_parser.h"

/*==============================================================================
 *  Data Extraction
 *==============================================================================*/

int32_t  extract_bit_variable(report_val_t *, uint8_t *, int, uint8_t *);
int32_t  extract_kbd_data(uint8_t *, int, uint8_t, hid_interface_t *, hid_keyboard_report_t *);

/*==============================================================================
 *  Hotkey Handling
 *==============================================================================*/

bool check_specific_hotkey(hotkey_combo_t, const hid_keyboard_report_t *);

/*==============================================================================
 *  Keyboard State Management
 *==============================================================================*/
void     update_kbd_state(device_t *, hid_keyboard_report_t *, uint8_t);
void     combine_kbd_states(device_t *, hid_keyboard_report_t *);

/*==============================================================================
 *  Keyboard Report Processing
 *==============================================================================*/
bool     key_in_report(uint8_t, const hid_keyboard_report_t *);
void     process_consumer_report(uint8_t *, int, uint8_t, hid_interface_t *);
void     process_keyboard_report(uint8_t *, int, uint8_t, hid_interface_t *);
void     process_system_report(uint8_t *, int, uint8_t, hid_interface_t *);
void     queue_cc_packet(uint8_t *, device_t *);
void     queue_kbd_report(hid_keyboard_report_t *, device_t *);
void     queue_system_packet(uint8_t *, device_t *);
void     release_all_keys(device_t *);
void     send_consumer_control(uint8_t *, device_t *);
void     send_key(hid_keyboard_report_t *, device_t *);

/* ==================================================== *
 * Map hotkeys to alternative layouts
 * ==================================================== */

/* DVORAK */
#define DVORAK_HID_KEY_A HID_KEY_A
#define DVORAK_HID_KEY_B HID_KEY_N
#define DVORAK_HID_KEY_C HID_KEY_I
#define DVORAK_HID_KEY_D HID_KEY_H
#define DVORAK_HID_KEY_E HID_KEY_D
#define DVORAK_HID_KEY_F HID_KEY_Y
#define DVORAK_HID_KEY_G HID_KEY_U
#define DVORAK_HID_KEY_H HID_KEY_J
#define DVORAK_HID_KEY_I HID_KEY_G
#define DVORAK_HID_KEY_J HID_KEY_C
#define DVORAK_HID_KEY_K HID_KEY_V
#define DVORAK_HID_KEY_L HID_KEY_P
#define DVORAK_HID_KEY_M HID_KEY_M
#define DVORAK_HID_KEY_N HID_KEY_L
#define DVORAK_HID_KEY_O HID_KEY_S
#define DVORAK_HID_KEY_P HID_KEY_R
#define DVORAK_HID_KEY_Q HID_KEY_X
#define DVORAK_HID_KEY_R HID_KEY_O
#define DVORAK_HID_KEY_S HID_KEY_SEMICOLON
#define DVORAK_HID_KEY_T HID_KEY_K
#define DVORAK_HID_KEY_U HID_KEY_F
#define DVORAK_HID_KEY_V HID_KEY_PERIOD
#define DVORAK_HID_KEY_W HID_KEY_COMMA
#define DVORAK_HID_KEY_X HID_KEY_B
#define DVORAK_HID_KEY_Y HID_KEY_T
#define DVORAK_HID_KEY_Z HID_KEY_SLASH

/* COLEMAK */
#define COLEMAK_HID_KEY_A HID_KEY_A
#define COLEMAK_HID_KEY_B HID_KEY_B
#define COLEMAK_HID_KEY_C HID_KEY_C
#define COLEMAK_HID_KEY_D HID_KEY_G
#define COLEMAK_HID_KEY_E HID_KEY_K
#define COLEMAK_HID_KEY_F HID_KEY_E
#define COLEMAK_HID_KEY_G HID_KEY_T
#define COLEMAK_HID_KEY_H HID_KEY_H
#define COLEMAK_HID_KEY_I HID_KEY_L
#define COLEMAK_HID_KEY_J HID_KEY_Y
#define COLEMAK_HID_KEY_K HID_KEY_N
#define COLEMAK_HID_KEY_L HID_KEY_U
#define COLEMAK_HID_KEY_M HID_KEY_M
#define COLEMAK_HID_KEY_N HID_KEY_J
#define COLEMAK_HID_KEY_O HID_KEY_SEMICOLON
#define COLEMAK_HID_KEY_P HID_KEY_R
#define COLEMAK_HID_KEY_Q HID_KEY_Q
#define COLEMAK_HID_KEY_R HID_KEY_S
#define COLEMAK_HID_KEY_S HID_KEY_D
#define COLEMAK_HID_KEY_T HID_KEY_F
#define COLEMAK_HID_KEY_U HID_KEY_I
#define COLEMAK_HID_KEY_V HID_KEY_V
#define COLEMAK_HID_KEY_W HID_KEY_W
#define COLEMAK_HID_KEY_X HID_KEY_X
#define COLEMAK_HID_KEY_Y HID_KEY_O
#define COLEMAK_HID_KEY_Z HID_KEY_Z

/* QWERTY needs no change */
#if KEYBOARD_LAYOUT == 0
#define KBD_REMAP(key) key

/* For DVORAK we prepend DVORAK_ and reference the definitions above */
#elif KEYBOARD_LAYOUT == 1
#define KBD_REMAP(key) DVORAK_##key

/* For COLEMAK we prepend COLEMAK_ and reference the definitions above */
#elif KEYBOARD_LAYOUT == 2
#define KBD_REMAP(key) COLEMAK_##key
#endif

# 文件: include/main.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pico/util/queue.h>
#include "hid_parser.h"

#include "constants.h"
#include "misc.h"
#include "structs.h"
#include "config.h"

#include "pio_usb.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "user_config.h"
#include "protocol.h"

#include "dma.h"

#include "firmware.h"
#include "flash.h"
#include "handlers.h"
#include "keyboard.h"
#include "mouse.h"
#include "packet.h"
#include "pinout.h"
#include "screen.h"
#include "serial.h"
#include "setup.h"
#include "tasks.h"
#include "watchdog.h"


#include <hardware/structs/ioqspi.h>
#include <hardware/structs/sio.h>
#include <hardware/dma.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/unique_id.h>

# 文件: include/misc.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include <stdint.h>
#include "structs.h"

/*==============================================================================
 *  Checksum Functions
 *==============================================================================*/

uint8_t  calc_checksum(const uint8_t *, int);
uint32_t crc32(const uint8_t *, size_t);
uint32_t crc32_iter(uint32_t, const uint8_t);
bool     verify_checksum(const uart_packet_t *);

/*==============================================================================
 *  Global State
 *==============================================================================*/

extern device_t global_state;

/*==============================================================================
 *  LED Control
 *==============================================================================*/

void    blink_led(device_t *);
void    restore_leds(device_t *);
uint8_t toggle_led(void);

# 文件: include/mouse.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include "structs.h"
#include "hid_parser.h"

/*==============================================================================
 *  Data Extraction
 *==============================================================================*/
void      extract_data(hid_interface_t *, report_val_t *);
int32_t   get_report_value(uint8_t *, int, report_val_t *);
void      parse_report_descriptor(hid_interface_t *, uint8_t const *, int);

/*==============================================================================
 *  Mouse Report Handling
 *==============================================================================*/
void process_mouse_report(uint8_t *, int, uint8_t, hid_interface_t *);
void queue_mouse_report(mouse_report_t *, device_t *);
bool tud_mouse_report(uint8_t mode, uint8_t buttons, int16_t x, int16_t y, int8_t wheel, int8_t pan);
void output_mouse_report(mouse_report_t *, device_t *);

# 文件: include/packet.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include <stdint.h>
#include "protocol.h"


/*==============================================================================
 *  Constants
 *==============================================================================*/

/* Preamble */
#define START1        0xAA
#define START2        0x55
#define START_LENGTH  2

/* Packet Queue Definitions  */
#define UART_QUEUE_LENGTH  256
#define HID_QUEUE_LENGTH   128
#define KBD_QUEUE_LENGTH   128
#define MOUSE_QUEUE_LENGTH 512

/* Packet Lengths and Offsets */
#define PACKET_LENGTH          (TYPE_LENGTH + PACKET_DATA_LENGTH + CHECKSUM_LENGTH)
#define RAW_PACKET_LENGTH      (START_LENGTH + PACKET_LENGTH)

#define TYPE_LENGTH             1
#define PACKET_DATA_LENGTH      8 // For simplicity, all packet types are the same length
#define CHECKSUM_LENGTH         1

#define KEYARRAY_BIT_OFFSET     16
#define KEYS_IN_USB_REPORT      6
#define KBD_REPORT_LENGTH       8
#define MOUSE_REPORT_LENGTH     8
#define CONSUMER_CONTROL_LENGTH 4
#define SYSTEM_CONTROL_LENGTH   1
#define MODIFIER_BIT_LENGTH     8

/*==============================================================================
 *  Data Structures
 *==============================================================================*/

 typedef struct {
    uint8_t type;     // Enum field describing the type of packet
    union {
        uint8_t data[8];      // Data goes here (type + payload + checksum)
        uint16_t data16[4];   // We can treat it as 4 16-byte chunks
        uint32_t data32[2];   // We can treat it as 2 32-byte chunks
    };
    uint8_t checksum; // Checksum, a simple XOR-based one
} __attribute__((packed)) uart_packet_t;

# 文件: include/pinout.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

/*==============================================================================
 *  Board Roles
 *==============================================================================*/

 #define BOARD_ROLE (global_state.board_role)
#define OTHER_ROLE (BOARD_ROLE == OUTPUT_A ? OUTPUT_B : OUTPUT_A)

/*==============================================================================
 *  GPIO Pins (LED, USB)
 *==============================================================================*/

#define GPIO_LED_PIN   25 // LED is connected to pin 25 on a PICO
#define PIO_USB_DP_PIN 14 // D+ is pin 14, D- is pin 15

/*==============================================================================
 *  Serial Pins
 *==============================================================================*/

/* GP12 / GP13, Pins 16 (TX), 17 (RX) on the Pico board */
#define BOARD_A_RX 13
#define BOARD_A_TX 12

/* GP16 / GP17, Pins 21 (TX), 22 (RX) on the Pico board */
#define BOARD_B_RX 17
#define BOARD_B_TX 16

#define SERIAL_RX_PIN (global_state.board_role == OUTPUT_A ? BOARD_A_RX : BOARD_B_RX)
#define SERIAL_TX_PIN (global_state.board_role == OUTPUT_A ? BOARD_A_TX : BOARD_B_TX)

# 文件: include/protocol.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include <stdint.h>

enum packet_type_e {
    KEYBOARD_REPORT_MSG  = 1,
    MOUSE_REPORT_MSG     = 2,
    OUTPUT_SELECT_MSG    = 3,
    FIRMWARE_UPGRADE_MSG = 4,
    MOUSE_ZOOM_MSG       = 5,
    KBD_SET_REPORT_MSG   = 6,
    SWITCH_LOCK_MSG      = 7,
    SYNC_BORDERS_MSG     = 8,
    FLASH_LED_MSG        = 9,
    WIPE_CONFIG_MSG      = 10,
    SCREENSAVER_MSG      = 11,
    HEARTBEAT_MSG        = 12,
    GAMING_MODE_MSG      = 13,
    CONSUMER_CONTROL_MSG = 14,
    SYSTEM_CONTROL_MSG   = 15,
    SAVE_CONFIG_MSG      = 18,
    REBOOT_MSG           = 19,
    GET_VAL_MSG          = 20,
    SET_VAL_MSG          = 21,
    GET_ALL_VALS_MSG     = 22,
    PROXY_PACKET_MSG     = 23,
    REQUEST_BYTE_MSG     = 24,
    RESPONSE_BYTE_MSG    = 25,
};

typedef enum {
    UINT8 = 0,
    UINT16 = 1,
    UINT32 = 2,
    UINT64 = 3,
    INT8 = 4,
    INT16 = 5,
    INT32 = 6,
    INT64 = 7,
    BOOL  = 8
} type_e;

/*==============================================================================
 *  API Request Data Structure, defines offset within struct, length, type,
    write permissions and packet ID type (index)
 *==============================================================================*/

typedef struct {
    uint32_t idx;
    bool readonly;
    type_e type;
    uint32_t len;
    size_t offset;
} field_map_t;

# 文件: include/screen.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include <stdint.h>


/*==============================================================================
 *  Constants
 *==============================================================================*/

#define MAX_SCREEN_COORD 32767
#define MIN_SCREEN_COORD 0

/*==============================================================================
 *  Data Structures
 *==============================================================================*/

typedef struct {
    int top;    // When jumping from a smaller to a bigger screen, go to THIS top height
    int bottom; // When jumping from a smaller to a bigger screen, go to THIS bottom
                // height
} border_size_t;

typedef struct {
    uint8_t mode;
    uint8_t only_if_inactive;
    uint64_t idle_time_us;
    uint64_t max_time_us;
} screensaver_t;

typedef struct {
    uint32_t number;           // Number of this output (e.g. OUTPUT_A = 0 etc)
    uint32_t screen_count;     // How many monitors per output (e.g. Output A is Windows with 3 monitors)
    uint32_t screen_index;     // Current active screen
    int32_t speed_x;           // Mouse speed per output, in direction X
    int32_t speed_y;           // Mouse speed per output, in direction Y
    border_size_t border;      // Screen border size/offset to keep cursor at same height when switching
    uint8_t os;                // Operating system on this output
    uint8_t pos;               // Screen position on this output
    uint8_t mouse_park_pos;    // Where the mouse goes after switch
    screensaver_t screensaver; // Screensaver parameters for this output
} output_t;

# 文件: include/serial.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include <hardware/uart.h>
#include "structs.h"

/*==============================================================================
 *  Constants
 *==============================================================================*/

#define SERIAL_BAUDRATE   3686400
#define SERIAL_DATA_BITS  8
#define SERIAL_PARITY     UART_PARITY_NONE
#define SERIAL_STOP_BITS  1
#define SERIAL_UART       uart0

/*==============================================================================
 *  Serial Communication Functions
 *==============================================================================*/

bool get_packet_from_buffer(device_t *);
void process_packet(uart_packet_t *, device_t *);
void queue_packet(const uint8_t *, enum packet_type_e, int);
void send_value(const uint8_t, enum packet_type_e);
void write_raw_packet(uint8_t *, uart_packet_t *);

# 文件: include/setup.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include "structs.h"

/*==============================================================================
 *  Initialization Functions
 *==============================================================================*/

void initial_setup(device_t *);
void serial_init(void);
void core1_main(void);

# 文件: include/structs.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include <stdint.h>
#include "flash.h"
#include "packet.h"
#include "screen.h"

typedef void (*action_handler_t)();

typedef struct { // Maps message type -> message handler function
    enum packet_type_e type;
    action_handler_t handler;
} uart_handler_t;

typedef struct {
    uint8_t modifier;                 // Which modifier is pressed
    uint8_t keys[KEYS_IN_USB_REPORT]; // Which keys need to be pressed
    uint8_t key_count;                // How many keys are pressed
    action_handler_t action_handler;  // What to execute when the key combination is detected
    bool pass_to_os;                  // True if we are to pass the key to the OS too
    bool acknowledge;                 // True if we are to notify the user about registering keypress
} hotkey_combo_t;

typedef struct TU_ATTR_PACKED {
    uint8_t buttons;
    int16_t x;
    int16_t y;
    int8_t wheel;
    int8_t pan;
    uint8_t mode;
} mouse_report_t;

typedef struct {
    uint8_t tip_pressure;
    uint8_t buttons; // Digitizer buttons
    uint16_t x;      // X coordinate (0-32767)
    uint16_t y;      // Y coordinate (0-32767)
} touch_report_t;

typedef struct {
    uint8_t instance;
    uint8_t report_id;
    uint8_t type;
    uint8_t len;
    uint8_t data[RAW_PACKET_LENGTH];
} hid_generic_pkt_t;

typedef enum { IDLE, READING_PACKET, PROCESSING_PACKET } receiver_state_t;

typedef struct {
    uint32_t address;         // Address we're sending to the other box
    uint32_t checksum;
    uint16_t version;
    bool byte_done;           // Has the byte been successfully transferred
    bool upgrade_in_progress; // True if firmware transfer from the other box is in progress
} fw_upgrade_state_t;

typedef struct {
    uint32_t magic_header;
    uint32_t version;

    uint8_t force_mouse_boot_mode;
    uint8_t force_kbd_boot_protocol;

    uint8_t kbd_led_as_indicator;
    uint8_t hotkey_toggle;
    uint8_t enable_acceleration;

    uint8_t enforce_ports;
    uint16_t jump_threshold;

    output_t output[NUM_SCREENS];
    uint32_t _reserved;

    // Keep checksum at the end of the struct
    uint32_t checksum;
} config_t;


/*==============================================================================
 *  Device State
 *==============================================================================*/
typedef struct {
    uint8_t kbd_dev_addr; // Address of the Keyboard device
    uint8_t kbd_instance; // Keyboard instance (d'uh - isn't this a useless comment)

    uint8_t keyboard_leds[NUM_SCREENS];  // State of keyboard LEDs (index 0 = A, index 1 = B)
    uint64_t last_activity[NUM_SCREENS]; // Timestamp of the last input activity (-||-)
    uint64_t core1_last_loop_pass;       // Timestamp of last core1 loop execution
    uint8_t active_output;               // Currently selected output (0 = A, 1 = B)
    uint8_t board_role;                  // Which board are we running on? (0 = A, 1 = B, etc.)

    // Track keyboard state for each device
    hid_keyboard_report_t kbd_states[MAX_DEVICES]; // Store keyboard state for each device
    uint8_t kbd_device_count;                      // Number of active keyboard devices

    int16_t pointer_x; // Store and update the location of our mouse pointer
    int16_t pointer_y;
    int16_t mouse_buttons; // Store and update the state of mouse buttons

    config_t config;       // Device configuration, loaded from flash or defaults used
    queue_t hid_queue_out; // Queue that stores outgoing hid messages
    queue_t kbd_queue;     // Queue that stores keyboard reports
    queue_t mouse_queue;   // Queue that stores mouse reports
    queue_t uart_tx_queue; // Queue that stores outgoing packets

    hid_interface_t iface[MAX_DEVICES][MAX_INTERFACES]; // Store info about HID interfaces
    uart_packet_t in_packet;

    /* DMA */
    uint32_t dma_ptr;             // Stores info about DMA ring buffer last checked position
    uint32_t dma_rx_channel;      // DMA RX channel we're using to receive
    uint32_t dma_control_channel; // DMA channel that controls the RX transfer channel
    uint32_t dma_tx_channel;      // DMA TX channel we're using to send

    /* Firmware */
    fw_upgrade_state_t fw;           // State of the firmware upgrader
    firmware_metadata_t _running_fw; // RAM copy of running fw metadata
    bool reboot_requested;           // If set, stop updating watchdog
    uint64_t config_mode_timer;      // Counts how long are we to remain in config mode

    uint8_t page_buffer[FLASH_PAGE_SIZE]; // For firmware-over-serial upgrades

    /* Connection status flags */
    bool tud_connected;      // True when TinyUSB device successfully connects
    bool keyboard_connected; // True when our keyboard is connected locally

    /* Feature flags */
    bool mouse_zoom;         // True when "mouse zoom" is enabled
    bool switch_lock;        // True when device is prevented from switching
    bool onboard_led_state;  // True when LED is ON
    bool relative_mouse;     // True when relative mouse mode is used
    bool gaming_mode;        // True when gaming mode is on (relative passthru + lock)
    bool config_mode_active; // True when config mode is active
    bool digitizer_active;   // True when digitizer Win/Mac workaround is active

    /* Onboard LED blinky (provide feedback when e.g. mouse connected) */
    int32_t blinks_left;     // How many blink transitions are left
    int32_t last_led_change; // Timestamp of the last time led state transitioned
} device_t;
/*==============================================================================*/


typedef struct {
    void (*exec)(device_t *state);
    uint64_t frequency;
    uint64_t next_run;
    bool *enabled;
} task_t;

enum os_type_e {
    LINUX   = 1,
    MACOS   = 2,
    WINDOWS = 3,
    ANDROID = 4,
    OTHER   = 255,
};

enum screen_pos_e {
    NONE   = 0,
    LEFT   = 1,
    RIGHT  = 2,
    MIDDLE = 3,
};

enum screensaver_mode_e {
    DISABLED   = 0,
    PONG       = 1,
    JITTER     = 2,
    MAX_SS_VAL = JITTER,
};

extern const config_t default_config;
extern const config_t ADDR_CONFIG[];
extern const uint8_t ADDR_FW_METADATA[];
extern const uint8_t ADDR_FW_RUNNING[];
extern const uint8_t ADDR_FW_STAGING[];
extern const uint8_t ADDR_DISK_IMAGE[];

# 文件: include/tasks.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include "structs.h"

/*==============================================================================
 *  Core Task Scheduling
 *==============================================================================*/

 void task_scheduler(device_t *, task_t *);

/*==============================================================================
 *  Individual Task Functions
 *==============================================================================*/

void firmware_upgrade_task(device_t *);
void heartbeat_output_task(device_t *);
void kick_watchdog_task(device_t *);
void led_blinking_task(device_t *);
void packet_receiver_task(device_t *);
void process_hid_queue_task(device_t *);
void process_kbd_queue_task(device_t *);
void process_mouse_queue_task(device_t *);
void process_uart_tx_task(device_t *);
void screensaver_task(device_t *);
void usb_device_task(device_t *);
void usb_host_task(device_t *);

# 文件: include/tusb_config.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 * Based on the example by Ha Thach
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

/*==============================================================================
 *  Common Configuration
 *  Settings applicable to both device and host modes.
 *==============================================================================*/

#define CFG_TUSB_OS OPT_OS_PICO

/*==============================================================================
 *  Device Mode Configuration
 *  Settings specific to USB device mode operation.
 *==============================================================================*/

// Enable the TinyUSB device stack.
#define CFG_TUD_ENABLED 1

// Root Hub port number used for the device (typically port 0).
#define BOARD_TUD_RHPORT 0

// Device mode configuration: sets the mode (device) and speed.
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | BOARD_DEVICE_RHPORT_SPEED)

// Root Hub port maximum operational speed (defaults to Full Speed).
#ifndef BOARD_DEVICE_RHPORT_SPEED
#define BOARD_DEVICE_RHPORT_SPEED OPT_MODE_FULL_SPEED
#endif

// Root Hub port number (defaults to 0).
#ifndef BOARD_DEVICE_RHPORT_NUM
#define BOARD_DEVICE_RHPORT_NUM 0
#endif

/*==============================================================================
 *  Host Mode Configuration
 *  Settings specific to USB host mode operation.
 *==============================================================================*/

// Enable the TinyUSB host stack (requires Pico-PIO-USB).
#define CFG_TUH_ENABLED     1
#define CFG_TUH_RPI_PIO_USB 1

// Root Hub port number used for the host (typically port 1).
#define BOARD_TUH_RHPORT 1
// Host mode configuration: sets the mode (host) and speed
#define CFG_TUSB_RHPORT1_MODE (OPT_MODE_HOST | BOARD_DEVICE_RHPORT_SPEED)

/*==============================================================================
 *  Memory Configuration
 *  Settings for USB memory sections and alignment.
 *==============================================================================*/

// Define a custom memory section for USB buffers (optional).
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

// Define the memory alignment for USB buffers (typically 4-byte aligned).
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

/*==============================================================================
 *  Device: Endpoint 0 Size
 *  Configuration for the default control endpoint (Endpoint 0).
 *==============================================================================*/

// Size of the control endpoint (Endpoint 0) buffer.
#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

/*==============================================================================
 *  Device: CDC Class Configuration (Serial Communication)
 *  Settings for the CDC (Communication Device Class) for serial communication.
 *==============================================================================*/

#ifdef DH_DEBUG

// Enable CDC class for debugging over serial.
#define CFG_TUD_CDC           1

// Use a custom debug printf function.
#define CFG_TUSB_DEBUG_PRINTF dh_debug_printf
extern int dh_debug_printf(const char *__restrict __format, ...);

// Buffer sizes for CDC RX and TX.
#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 64

// Line coding settings to use on CDC enumeration.
#define CFG_TUH_CDC_LINE_CODING_ON_ENUM \
    { 921600, CDC_LINE_CONDING_STOP_BITS_1, CDC_LINE_CONDING_PARITY_NONE, 8 }

#else
// Disable CDC class when not debugging.
#define CFG_TUD_CDC 0
#endif

/*==============================================================================
 *  Device: Class Configuration
 *  Enable/disable various USB device classes.
 *==============================================================================*/

// Enable HID (Human Interface Device) class (keyboard, mouse, etc.).
#define CFG_TUD_HID    2

// Enable MSC (Mass Storage Class) class.
#define CFG_TUD_MSC    1

/*==============================================================================
 *  Device: Endpoint Buffer Sizes
 *  Configuration for endpoint buffer sizes for different classes.
 *==============================================================================*/

// HID endpoint buffer size (must be large enough for report ID + data).
#define CFG_TUD_HID_EP_BUFSIZE 32

// MSC endpoint buffer size.
#define CFG_TUD_MSC_EP_BUFSIZE 512

/*==============================================================================
 *  Host: Enumeration Buffer Size
 *  Configuration for the buffer used during device enumeration.
 *==============================================================================*/

#define CFG_TUH_ENUMERATION_BUFSIZE 512

/*==============================================================================
 *  Host: Class Configuration
 *  Settings for the USB host class drivers.
 *==============================================================================*/

// Enable USB Hub support.
#define CFG_TUH_HUB 1

// Maximum number of connected devices (excluding the hub itself).
#define CFG_TUH_DEVICE_MAX (CFG_TUH_HUB ? 4 : 1) // Hub typically has 4 ports

// Maximum number of HID instances.
#define CFG_TUH_HID               3 * CFG_TUH_DEVICE_MAX

// HID endpoint buffer sizes (IN and OUT).
#define CFG_TUH_HID_EPIN_BUFSIZE  64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64

#endif /* _TUSB_CONFIG_H_ */

# 文件: include/usb_descriptors.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

// Interface 0
#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_MOUSE    2
#define REPORT_ID_CONSUMER 3
#define REPORT_ID_SYSTEM   4

// Interface 1
#define REPORT_ID_RELMOUSE  5
#define REPORT_ID_DIGITIZER 7

// Interface 2
#define REPORT_ID_VENDOR 6


#define DEVICE_DESCRIPTOR(vid, pid) \
{.bLength         = sizeof(tusb_desc_device_t),\
  .bDescriptorType = TUSB_DESC_DEVICE,\
  .bcdUSB          = 0x0200,\
  .bDeviceClass    = 0x00,\
  .bDeviceSubClass = 0x00,\
  .bDeviceProtocol = 0x00,\
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,\
  .idVendor  = vid,\
  .idProduct = pid,\
  .bcdDevice = 0x0100,\
  .iManufacturer = 0x01,\
  .iProduct      = 0x02,\
  .iSerialNumber = 0x03,\
  .bNumConfigurations = 0x01}\

/* Common mouse descriptor. Use HID_RELATIVE or HID_ABSOLUTE for ABS_OR_REL. */
#define TUD_HID_REPORT_DESC_MOUSE_COMMON(ABS_OR_REL, MOUSE_MIN, ...)\
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP      )                   ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_MOUSE     )                   ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION  )                   ,\
    /* Report ID if any */\
    __VA_ARGS__ \
    HID_USAGE      ( HID_USAGE_DESKTOP_POINTER )                   ,\
    HID_COLLECTION ( HID_COLLECTION_PHYSICAL   )                   ,\
      HID_USAGE_PAGE  ( HID_USAGE_PAGE_BUTTON  )                   ,\
        HID_USAGE_MIN   ( 1                                      ) ,\
        HID_USAGE_MAX   ( 8                                      ) ,\
        HID_LOGICAL_MIN ( 0                                      ) ,\
        HID_LOGICAL_MAX ( 1                                      ) ,\
        \
        /* Left, Right, Mid, Back, Forward buttons + 3 extra */     \
        HID_REPORT_COUNT( 8                                      ) ,\
        HID_REPORT_SIZE ( 1                                      ) ,\
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
        \
      HID_USAGE_PAGE  ( HID_USAGE_PAGE_DESKTOP )                   ,\
        \
        /* X, Y position [MOUSE_MIN, 32767] */ \
        HID_USAGE       ( HID_USAGE_DESKTOP_X                    ) ,\
        HID_USAGE       ( HID_USAGE_DESKTOP_Y                    ) ,\
        MOUSE_MIN                                                  ,\
        HID_LOGICAL_MAX_N( 0x7FFF, 2                             ) ,\
        HID_REPORT_SIZE  ( 16                                    ) ,\
        HID_REPORT_COUNT ( 2                                     ) ,\
        HID_INPUT       ( HID_DATA | HID_VARIABLE | ABS_OR_REL   ) ,\
        \
        /* Vertical wheel scroll [-127, 127] */ \
        HID_USAGE       ( HID_USAGE_DESKTOP_WHEEL                ) ,\
        HID_LOGICAL_MIN ( 0x81                                   ) ,\
        HID_LOGICAL_MAX ( 0x7f                                   ) ,\
        HID_REPORT_COUNT( 1                                      ) ,\
        HID_REPORT_SIZE ( 8                                      ) ,\
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_RELATIVE ) ,\
        \
        /* Horizontal wheel (AC Pan) */ \
        HID_USAGE_PAGE  ( HID_USAGE_PAGE_CONSUMER                ) ,\
        HID_LOGICAL_MIN ( 0x81                                   ) ,\
        HID_LOGICAL_MAX ( 0x7f                                   ) ,\
        HID_REPORT_COUNT( 1                                      ) ,\
        HID_REPORT_SIZE ( 8                                      ) ,\
        HID_USAGE_N     ( HID_USAGE_CONSUMER_AC_PAN, 2           ) ,\
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_RELATIVE ) ,\
        \
        /* Mouse mode (0 = absolute, 1 = relative) */ \
        HID_REPORT_COUNT( 1                                      ), \
        HID_REPORT_SIZE ( 8                                      ), \
        HID_INPUT       ( HID_CONSTANT                           ), \
    HID_COLLECTION_END                                            , \
  HID_COLLECTION_END \

/* Absolute mouse, range=[0..32767] */
#define TUD_HID_REPORT_DESC_ABS_MOUSE(...) TUD_HID_REPORT_DESC_MOUSE_COMMON(HID_ABSOLUTE, HID_LOGICAL_MIN(0), __VA_ARGS__)

/* Relative mouse, range=[-32767..32767] */
#define TUD_HID_REPORT_DESC_MOUSEHELP(...) TUD_HID_REPORT_DESC_MOUSE_COMMON(HID_RELATIVE, HID_LOGICAL_MIN_N(-32767, 2), __VA_ARGS__)

// Consumer Control Report Descriptor Template
#define TUD_HID_REPORT_DESC_CONSUMER_CTRL(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_CONSUMER    )              ,\
  HID_USAGE      ( HID_USAGE_CONSUMER_CONTROL )              ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )              ,\
    /* Report ID if any */\
    __VA_ARGS__ \
    HID_LOGICAL_MIN  ( 0x00                                ) ,\
    HID_LOGICAL_MAX_N( 0x0FFF, 2                           ) ,\
    HID_USAGE_MIN    ( 0x00                                ) ,\
    HID_USAGE_MAX_N  ( 0x0FFF, 2                           ) ,\
    HID_REPORT_SIZE  ( 16                                  ) ,\
    HID_REPORT_COUNT ( 2                                   ) ,\
    HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ) ,\
  HID_COLLECTION_END \

// System Control Report Descriptor Template
#define TUD_HID_REPORT_DESC_SYSTEM_CTRL(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP    )               ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_SYSTEM_CONTROL )        ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )              ,\
    /* Report ID if any */\
    __VA_ARGS__ \
    HID_LOGICAL_MIN ( 0x00                                )  ,\
    HID_LOGICAL_MAX ( 0xff                                )  ,\
    HID_REPORT_COUNT( 1                                   )  ,\
    HID_REPORT_SIZE ( 8                                   )  ,\
    HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ) ,\
  HID_COLLECTION_END \

// Vendor Config Descriptor Template
#define TUD_HID_REPORT_DESC_VENDOR_CTRL(...) \
  HID_USAGE_PAGE_N ( HID_USAGE_PAGE_VENDOR, 2 )             ,\
  HID_USAGE      ( 0x10 )                                   ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )             ,\
    /* Report ID if any */\
    __VA_ARGS__ \
    HID_LOGICAL_MIN ( 0x80                                )  ,\
    HID_LOGICAL_MAX ( 0x7f                                )  ,\
    HID_REPORT_COUNT( 12                                  )  ,\
    HID_REPORT_SIZE ( 8                                   )  ,\
    HID_USAGE       ( 0x10                                )  ,\
    HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ) ,\
    HID_USAGE       ( 0x10                                )  ,\
    HID_OUTPUT       ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ) ,\
  HID_COLLECTION_END \

#define HID_USAGE_DIGITIZER 0x01

#define TUD_HID_REPORT_DESC_DIGITIZER_PEN(...) \
HID_USAGE_PAGE ( HID_USAGE_PAGE_DIGITIZER )                 ,\
HID_USAGE ( HID_USAGE_DIGITIZER )                           ,\
HID_COLLECTION ( HID_COLLECTION_APPLICATION )               ,\
  /* Report ID if any */\
  __VA_ARGS__ \
  HID_USAGE ( HID_USAGE_DIGITIZER )                         ,\
  HID_COLLECTION ( HID_COLLECTION_PHYSICAL )                ,\
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DIGITIZER )             ,\
    /* Tip Pressure */\
    HID_USAGE      ( 0x30 )                                 ,\
    HID_LOGICAL_MIN ( 0x00                                )  ,\
    HID_LOGICAL_MAX ( 0xff                                )  ,\
    HID_REPORT_COUNT( 1                                      )  ,\
    HID_REPORT_SIZE ( 8                                      )  ,\
    HID_INPUT ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE )    ,\
    \
    HID_REPORT_COUNT( 5                                      )  ,\
    HID_REPORT_SIZE ( 1                                      )  ,\
    /* In range */\
    HID_USAGE ( 0x32 )                       ,\
    /* Tip switch */\
    HID_USAGE ( 0x42 )                       ,\
    /* Eraser */\
    HID_USAGE ( 0x45 )                       ,\
    /* Barrel switch */\
    HID_USAGE ( 0x44 )                       ,\
    /* Invert */\
    HID_USAGE ( 0x3c )                       ,\
    HID_INPUT ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE )    ,\
    \
    HID_REPORT_COUNT( 3                                      ) ,\
    HID_REPORT_SIZE ( 1                                      ) ,\
    HID_INPUT ( HID_CONSTANT )    ,\
    /* X and Y coordinates */\
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP )               ,\
    HID_USAGE ( HID_USAGE_DESKTOP_X )                       ,\
    HID_USAGE ( HID_USAGE_DESKTOP_Y )                       ,\
    HID_LOGICAL_MIN ( 0 )                                   ,\
    HID_LOGICAL_MAX_N ( 32767, 2 )                          ,\
    HID_REPORT_SIZE ( 16 )                                  ,\
    HID_REPORT_COUNT ( 2 )                                  ,\
    HID_INPUT ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE)     ,\
  HID_COLLECTION_END                                        ,\
HID_COLLECTION_END

#endif /* USB_DESCRIPTORS_H_ */

# 文件: include/user_config.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 *
 **===================================================== *
 * ==========  Keyboard LED Output Indicator  ========== *
 * ===================================================== *
 *
 * If you are willing to give up on using the keyboard LEDs for their original purpose,
 * you can use them as a convenient way to indicate which output is selected.
 *
 * KBD_LED_AS_INDICATOR set to 0 will use the keyboard LEDs as normal.
 * KBD_LED_AS_INDICATOR set to 1 will use the Caps Lock LED as indicator.
 *
 * */

#define KBD_LED_AS_INDICATOR 0

/**===================================================== *
 * ===========  Hotkey for output switching  =========== *
 * ===================================================== *
 *
 * Everyone is different, I prefer to use caps lock because I HATE SHOUTING :)
 * You might prefer something else. Pick something from the list found at:
 *
 * https://github.com/hathach/tinyusb/blob/master/src/class/hid/hid.h
 *
 * defined as HID_KEY_<something>
 *
 * In addition, there is an optional modifier you can use for the hotkey
 * switching. Currently, it's set to LEFT CTRL, you can change it by
 * redefining HOTKEY_MODIFIER here from KEYBOARD_MODIFIER_LEFTCTRL to something
 * else (check file referenced below) or HID_KEY_NONE.
 *
 * If you do not want to use a key for switching outputs, you may be tempted
 * to select HID_KEY_NONE here as well; don't do that! That code appears in many
 * HID messages and the result will be a non-functional keyboard. Instead, choose
 * a key that is unlikely to ever appear on a keyboard that you will use.
 * HID_KEY_F24 is probably a good choice as keyboards with 24 function keys
 * are rare.
 *
 * */

#define HOTKEY_MODIFIER  KEYBOARD_MODIFIER_LEFTCTRL
#define HOTKEY_TOGGLE    HID_KEY_CAPS_LOCK

/**================================================== *
 * ==============  Mouse Speed Factor  ============== *
 * ================================================== *
 *
 * This affects how fast the mouse moves.
 *
 * MOUSE_SPEED_A_FACTOR_X: [1-128], mouse moves at this speed in X direction
 * MOUSE_SPEED_A_FACTOR_Y: [1-128], mouse moves at this speed in Y direction
 *
 * JUMP_THRESHOLD: [0-32768], sets the "force" you need to use to drag the
 * mouse to another screen, 0 meaning no force needed at all, and ~500 some force
 * needed, ~1000 no accidental jumps, you need to really mean it.
 *
 * This is now configurable per-screen.
 *
 * ENABLE_ACCELERATION: [0-1], disables or enables mouse acceleration.
 *
 * */

/* Output A values, default is for the most common ~ 16:9 ratio screen */
#define MOUSE_SPEED_A_FACTOR_X 16
#define MOUSE_SPEED_A_FACTOR_Y 28

/* Output B values, default is for the most common ~ 16:9 ratio screen */
#define MOUSE_SPEED_B_FACTOR_X 16
#define MOUSE_SPEED_B_FACTOR_Y 28

#define JUMP_THRESHOLD 0

/* Mouse acceleration */
#define ENABLE_ACCELERATION 1

/**================================================== *
 * ==============  Screensaver Config  ============== *
 * ================================================== *
 *
 * While this feature is called 'screensaver', it's not actually a
 * screensaver :) Really it's a way to ensure that some sort of mouse
 * activity will be sent to one (or both) outputs when the user has
 * not interacted with that output. This can be used to stop a
 * screensaver or screenlock from activating on the attached computer,
 * or to just watch the mouse pointer bouncing around!
 *
 * When the mode is active on an output, the pointer will jump around
 * the screen like a bouncing-ball in a Pong game (however no click
 * events will be generated, of course).
 *
 * This mode is activated by 'idle time' on a per-output basis; if the
 * mode is enabled for output B, and output B doesn't have any
 * activity for (at least) the specified idle time, then the mode will
 * be activated and will continue until the inactivity time reaches
 * the maximum (if one has been specified). This allows you to stop a
 * screensaver/screenlock from activating while you are still at your
 * desk (but just interacting with the other computer attached to
 * Deskhop), but let it activate if you leave your desk for an
 * extended period of time.
 *
 * Additionally, this mode can be automatically disabled if the output
 * is the currently-active output.
 *
 * If you only set the ENABLED options below, and leave the rest of
 * the defaults in place, then the screensaver mode will activate
 * after 4 minutes (240 seconds) of inactivity, will continue forever,
 * but will only activate on an output that is not currently
 * active.
 *
 * */

/**================================================== *
 *
 * SCREENSAVER_{A|B}_MODE: DISABLED
 *                         PONG
 *                         JITTER
 *
 * */

#define SCREENSAVER_A_MODE DISABLED
#define SCREENSAVER_B_MODE DISABLED

/**================================================== *
 *
 * SCREENSAVER_{A|B}_IDLE_TIME_SEC: Number of seconds that an output
 * must be inactive before the screensaver mode will be activated.
 *
 * */

#define SCREENSAVER_A_IDLE_TIME_SEC 240
#define SCREENSAVER_B_IDLE_TIME_SEC 240

/**================================================== *
 *
 * SCREENSAVER_{A|B}_MAX_TIME_SEC: Number of seconds that the screensaver
 * will run on an output before being deactivated. 0 for indefinitely.
 *
 * */

#define SCREENSAVER_A_MAX_TIME_SEC 0
#define SCREENSAVER_B_MAX_TIME_SEC 0

/**================================================== *
 *
 * SCREENSAVER_{A|B}_ONLY_IF_INACTIVE: [0 or 1] 1 means the
 * screensaver will activate only if the output is inactive.
 *
 * */

#define SCREENSAVER_A_ONLY_IF_INACTIVE 0
#define SCREENSAVER_B_ONLY_IF_INACTIVE 0

/**================================================== *
 * ================  Output OS Config =============== *
 * ================================================== *
 *
 * Defines OS an output connects to. You will need to worry about this only if you have
 * multiple desktops and one of your outputs is MacOS or Windows.
 *
 * Available options: LINUX, MACOS, WINDOWS, OTHER (check main.h for details)
 *
 * OUTPUT_A_OS: OS for output A
 * OUTPUT_B_OS: OS for output B
 *
 * */

#define OUTPUT_A_OS MACOS
#define OUTPUT_B_OS LINUX


/**================================================== *
 * =================  Enforce Ports ================= *
 * ================================================== *
 *
 * If enabled, fixes some device incompatibilities by
 * enforcing keyboard has to be in port A and mouse in port B.
 *
 * ENFORCE_PORTS: [0, 1] - 1 means keyboard has to plug in A and mouse in B
 *                         0 means no such layout is enforced
 *
 * */

#define ENFORCE_PORTS 0


/**================================================== *
 * =============  Enforce Boot Protocol ============= *
 * ================================================== *
 *
 * If enabled, fixes some device incompatibilities by
 * enforcing the boot protocol (which is simpler to parse
 * and with less variation)
 *
 * ENFORCE_KEYBOARD_BOOT_PROTOCOL: [0, 1] - 1 means keyboard will forcefully use
 *                                          the boot protocol
 *                                        - 0 means no such thing is enforced
 *
 * */

#define ENFORCE_KEYBOARD_BOOT_PROTOCOL 0

# 文件: include/watchdog.h

/*
 * This file is part of DeskHop (https://github.com/hrvach/deskhop).
 * Copyright (c) 2025 Hrvoje Cavrak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * See the file LICENSE for the full license text.
 */
#pragma once

#include <hardware/watchdog.h>

#define WATCHDOG_TIMEOUT        500                     // In milliseconds => needs to be reset at least every 200ms
#define WATCHDOG_PAUSE_ON_DEBUG 1                       // When using a debugger, disable watchdog
#define CORE1_HANG_TIMEOUT_US   WATCHDOG_TIMEOUT * 1000 // Convert to microseconds

#define MAGIC_WORD_1 0xdeadf00f // When these are set, we'll boot to configuration mode
#define MAGIC_WORD_2 0x00c0ffee

