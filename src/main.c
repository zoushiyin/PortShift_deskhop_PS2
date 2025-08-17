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
