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
