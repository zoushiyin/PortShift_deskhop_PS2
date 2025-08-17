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
