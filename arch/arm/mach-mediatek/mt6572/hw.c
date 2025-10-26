/* Hardware initialization functions that get called by board_debug_uart_init() */
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/bitops.h>

#include "hw.h"

void set_armpll(void)
{
  u32 val;

  val = readl(0x10001000);
  val = 0;
  writel(val, 0x10001000);
  writel(0x8009a000, 0x10205104);

  udelay(100);
  
  val = readl(0x10001000);
  val |= 0x4;
  writel(val, 0x10001000);
}

void uart1_gpio_init(void)
{
  /* 
   * HACK: The preloader (our previous stage that loads U-Boot) 
   * switches the auxiliary mode of UART1 pins to mode 5 (MD debug) 
   * before jumping to LK, which then switches them back early.
   * We switch to mode 1 to be able to see logs.
   *
   * WARNING: seems to somehow break kermit (works fine as 2nd stage)
   */
  u32 val;
    
  /* Pin 103 (URXD1) */
  val = readl(0x100053c0);
  val &= ~(0x7 << 28);
  val |=  (0x1 << 28);
  writel(val, 0x100053c0);
  /* Pin 104 (UTXD1) */
  val = readl(0x100053d0);
  val &= ~(0x7 << 0);
  val |=  (0x1 << 0); 
  writel(val, 0x100053d0);
  
  /* Zero ESCAPE_EN (escape character enable) */
  writel(0, 0x11005044);
}

void mtk_gpt_early_init(void)
{
  /* 
   * lib/time.c somewhy fails to init the timer with -EAGAIN.
   * We implement init here and timer_early_get_{rate,count}() 
   * in drivers/timer/mtk_timer.c
   */

  /* Enable and set to FREERUN */
  writel(GENMASK(5, 4) | BIT(1) | BIT(0), 0x10008000 + 0x40 + 0x0);
  /* Use 13MHz system clock, set div to 1 */
  writel(0, 0x10008000 + 0x40 + 0x4);
}