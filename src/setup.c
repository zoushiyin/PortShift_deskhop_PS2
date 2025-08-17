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
