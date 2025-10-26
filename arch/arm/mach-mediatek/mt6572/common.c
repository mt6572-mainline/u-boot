/* Generic U-Boot initcalls */
#include <init.h>
#include <config.h>
#include <asm/global_data.h>
#include <fdt_support.h>
#include <vsprintf.h>
#include <dm.h>

#include "hw.h"

DECLARE_GLOBAL_DATA_PTR;

void board_debug_uart_init(void)
{
  /* LK calls this and then switches mode of UART1 pins */ 
  set_armpll();
  uart1_gpio_init();
  /* The GPT should already be configured by preloader but just in case */
  mtk_gpt_early_init();
}

int board_late_init(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device(UCLASS_USB_GADGET_GENERIC, 0, &dev);
	if (ret) {
		printf("%s: Failed to find USB device (err %d)\n", __func__, ret);
		return ret;
  }
  
  return 0;
}

int dram_init(void)
{
  /* 
   * The vast majority of MT6572 devices have 512MB of DRAM
   * hence that size is hardcoded in mt6572-generic.dts.
   * So far, the only known device to have 1GB is JTY D101.
   */
  gd->ram_size = get_ram_size((long*)CFG_SYS_SDRAM_BASE, 0x40000000);
  fdt_fixup_memory((void*)gd->fdt_blob, CFG_SYS_SDRAM_BASE, gd->ram_size);
  return 0;
}

int print_cpuinfo(void)
{
	printf("SoC:   MediaTek MT6572\n");
	return 0;
}
