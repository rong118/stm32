/*
 * STM32F103C8T6 (Blue Pill) 裸机 LED 闪烁程序
 * 无 HAL / 无 CMSIS，直接操作寄存器
 * 板载 LED 接在 PC13，低电平点亮
 */

/* 提供 uint32_t 等定义，-ffreestanding 下少数可用的标准头文件之一 */
#include <stdint.h>

/* ============ 外设基地址 (来自 RM0008 Memory Map) ============ */

/* RCC (Reset and Clock Control) 挂在 AHB 总线上 */
#define RCC_BASE        0x40021000UL
/* GPIOC 挂在 APB2 总线上 */
#define GPIOC_BASE      0x40011000UL

/* ============ 寄存器定义 ============
 *
 * 通过 volatile uint32_t* 将地址转为指针再解引用，使其可像变量一样读写。
 * volatile 防止编译器优化掉对硬件寄存器的读写（寄存器值可能被硬件随时改变）。
 */

/* APB2 外设时钟使能寄存器 (偏移 0x18)，控制 GPIOA~GPIOE 等外设的时钟开关 */
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18))
/* 端口配置高寄存器 (偏移 0x04)，每 4 位配置一个引脚 (Pin8~Pin15) 的模式 */
#define GPIOC_CRH       (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
/* 输出数据寄存器 (偏移 0x0C)，对应位写 1 输出高电平，写 0 输出低电平 */
#define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))

/* ============ 位掩码常量 ============ */

/* RCC_APB2ENR 第 4 位: GPIOC 时钟使能 */
#define RCC_IOPCEN      (1U << 4)
/* ODR 第 13 位: 对应 Pin 13 */
#define GPIO_PIN_13     (1U << 13)

/* ============ 延时函数 ============ */

/*
 * 软件延时：循环 50 万次 nop。
 * i 声明为 volatile 防止编译器把"无副作用"的循环优化掉。
 * __asm volatile("nop") 双重保险，确保循环体不被消除。
 * 8MHz 默认主频下大约延时几百毫秒。
 */
static void delay(void)
{
    for (volatile uint32_t i = 0; i < 500000; i++)
    {
        __asm volatile ("nop");
    }
}

/* ============ 主函数 ============ */

int main(void)
{
    /*
     * 第一步：开启 GPIOC 时钟
     * STM32 上电后外设时钟默认关闭，不开时钟无法操作对应外设。
     * |= 只置位第 4 位，不影响其他外设的时钟设置。
     */
    RCC_APB2ENR |= RCC_IOPCEN;

    /*
     * 第二步：配置 PC13 为通用推挽输出，速度 2MHz
     *
     * CRH 中每 4 位控制一个引脚 (Pin8~Pin15)：
     *   bit[23:22] = CNF13   (配置)
     *   bit[21:20] = MODE13  (模式)
     *
     * PC13 的偏移 = (13 - 8) * 4 = 20
     *
     * 先清零 bit[23:20]（&= ~(0xF << 20)），再写入 0x2（|= (0x2 << 20)）：
     *   CNF13  = 00  → 通用推挽输出
     *   MODE13 = 10  → 输出模式，最大速度 2MHz
     */
    GPIOC_CRH &= ~(0xFU << 20);
    GPIOC_CRH |=  (0x2U << 20);

    /*
     * 第三步：死循环闪烁 LED
     * 嵌入式程序永远不能从 main 返回（没有 OS 接管），所以用 while(1)。
     */
    while (1)
    {
        /*
         * ^= 异或翻转 PC13 电平：
         *   当前 1 → 变 0（LED 亮，低电平点亮）
         *   当前 0 → 变 1（LED 灭）
         */
        GPIOC_ODR ^= GPIO_PIN_13;

        /* 延时，让人眼能看到闪烁 */
        delay();
    }
}
