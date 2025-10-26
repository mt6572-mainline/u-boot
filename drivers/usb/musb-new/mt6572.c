// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MUSB glue layer
 *
 * Copyright (C) 2019-2021 by Mediatek
 * Based on the AllWinner SUNXI "glue layer" code.
 * Copyright (C) 2015 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2013 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of the Inventra Controller Driver for Linux.
 */

#include <clk.h>
#include <generic-phy.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <linux/delay.h>
#include <linux/usb/musb.h>
#include <linux/usb/gadget.h>
#include <linux/bitops.h>
#include <vsprintf.h>
#include <usb.h>

#include "linux-compat.h"
#include "musb_core.h"
#include "musb_uboot.h"
#include "musb_gadget.h"
#include "musb_io.h"

#define to_mtk_musb_glue(d)	container_of(d, struct mtk_musb_glue, dev)

/* Registers */
#define USB_L1INTS		    0x00a0
#define USB_L1INTM		    0x00a4
#define MTK_MUSB_TXFUNCADDR	0x0480
/* MediaTek controller toggle enable and status reg */
#define MUSB_RXTOG		    0x80
#define MUSB_RXTOGEN		0x82
#define MUSB_TXTOG		    0x84
#define MUSB_TXTOGEN		0x86
#define MTK_TOGGLE_EN		GENMASK(15, 0)

#define TX_INT_STATUS		BIT(0)
#define RX_INT_STATUS		BIT(1)
#define USBCOM_INT_STATUS	BIT(2)

struct mtk_musb_glue {
	struct device dev;
	struct musb_host_data mdata;
	struct mtk_musb_config *cfg;
	/* track if hw is on or not */
	bool enabled;
	/* clocks */
	struct clk usbpll_clk;
	struct clk usbmcu_clk;
	struct clk usb_clk;
	/* phy */
	struct phy phy;
	enum phy_mode phy_mode;
};

struct mtk_musb_config {
	struct musb_hdrc_config *config;
};

static u8 mtk_musb_clearb(void __iomem *addr, unsigned int offset)
{
	u8 data;

	/* W1C */
	data = musb_readb(addr, offset);
	musb_writeb(addr, offset, data);
	return data;
}

static u16 mtk_musb_clearw(void __iomem *addr, unsigned int offset)
{
	u16 data;

	/* W1C */
	data = musb_readw(addr, offset);
	musb_writew(addr, offset, data);
	return data;
}

static irqreturn_t generic_interrupt(int irq, void *__hci)
{
	unsigned long flags;
	irqreturn_t retval = IRQ_NONE;
	struct musb *musb = __hci;

	spin_lock_irqsave(&musb->lock, flags);
	musb->int_usb = mtk_musb_clearb(musb->mregs, MUSB_INTRUSB);
	musb->int_rx = mtk_musb_clearw(musb->mregs, MUSB_INTRRX);
	musb->int_tx = mtk_musb_clearw(musb->mregs, MUSB_INTRTX);

	if ((musb->int_usb & MUSB_INTR_RESET) && !is_host_active(musb)) {
		/* ep0 FADDR must be 0 when (re)entering peripheral mode */
		musb_ep_select(musb->mregs, 0);
		musb_writeb(musb->mregs, MUSB_FADDR, 0);
	}

	if (musb->int_usb || musb->int_tx || musb->int_rx)
		retval = musb_interrupt(musb);

	spin_unlock_irqrestore(&musb->lock, flags);

	return retval;
}

static irqreturn_t mtk_musb_interrupt(int irq, void *dev_id)
{
	irqreturn_t retval = IRQ_NONE;
	struct musb *musb = (struct musb *)dev_id;
	u32 l1_ints;

	l1_ints = musb_readl(musb->mregs, USB_L1INTS) &
			musb_readl(musb->mregs, USB_L1INTM);

	if (l1_ints & (TX_INT_STATUS | RX_INT_STATUS | USBCOM_INT_STATUS))
		retval = generic_interrupt(irq, musb);

	return retval;
}

int mtk_musb_platform_init(struct musb *musb)
{
	struct mtk_musb_glue *glue = to_mtk_musb_glue(musb->controller);
	int ret;

	musb->isr = mtk_musb_interrupt;
	/* reset */
	musb_writeb(musb->mregs, MUSB_FADDR, 0);
  musb_writeb(musb->mregs, MUSB_POWER, MUSB_POWER_RESET);
  mdelay(10);
  musb_writeb(musb->mregs, MUSB_POWER, 0);
	
	udelay(100);
	musb_writeb(musb->mregs, MUSB_DEVCTL, MUSB_DEVCTL_BDEVICE);

	musb_writel(musb->mregs, USB_L1INTM, TX_INT_STATUS | RX_INT_STATUS | USBCOM_INT_STATUS);
	/* Set TX/RX toggle enable */
	musb_writew(musb->mregs, MUSB_TXTOGEN, MTK_TOGGLE_EN);
	musb_writew(musb->mregs, MUSB_RXTOGEN, MTK_TOGGLE_EN);

	return 0;
}

int mtk_musb_platform_exit(struct musb *musb)
{
	struct mtk_musb_glue *glue = to_mtk_musb_glue(musb->controller);
	int ret;
	u8 tmp;

	ret = generic_phy_exit(&glue->phy);
	if (ret) {
		printf("failed to power off USB PHY\n");
		return ret;
	}

	clk_disable(&glue->usb_clk);
	clk_disable(&glue->usbmcu_clk);
	clk_disable(&glue->usbpll_clk);
	return 0;
}

static int mtk_musb_enable(struct musb *musb)
{
	struct mtk_musb_glue *glue = to_mtk_musb_glue(musb->controller);
	int ret;
	u8 tmp;

	/* select EP0 */
	musb_ep_select(musb->mregs, 0);
	musb_writeb(musb->mregs, MUSB_FADDR, 0);

	if (glue->enabled)
		return 0;

	ret = generic_phy_init(&glue->phy);
	if (ret) {
		printf("failed to init USB PHY\n");
		return ret;
	}

	ret = generic_phy_power_on(&glue->phy);
	if (ret) {
		printf("failed to power on USB PHY\n");
		return ret;
	}
	generic_phy_set_mode(&glue->phy, PHY_MODE_USB_DEVICE, 0);

	glue->enabled = true;
	return 0;
}

static void mtk_musb_disable(struct musb *musb)
{
	struct mtk_musb_glue *glue = to_mtk_musb_glue(musb->controller);
	int ret;

	if (!glue->enabled)
		return;

	ret = generic_phy_power_off(&glue->phy);
	if (ret) {
		printf("couldn't power off USB PHY\n");
	}

	generic_phy_set_mode(&glue->phy, PHY_MODE_INVALID, 0);
	glue->enabled = false;
}

static const struct musb_platform_ops mtk_musb_ops = {
	.enable		= mtk_musb_enable,
	.disable	= mtk_musb_disable,
	.init		= mtk_musb_platform_init,
	.exit		= mtk_musb_platform_exit,
};

static int mtk_musb_probe(struct udevice *dev)
{
	struct mtk_musb_glue *glue = dev_get_priv(dev);
	struct musb_host_data *host = &glue->mdata;
	struct musb_hdrc_platform_data pdata;
	void *base = dev_read_addr_ptr(dev);
	int ret;

	if (!base)
		return -EINVAL;

	glue->cfg = (struct mtk_musb_config*)dev_get_driver_data(dev);
	if (!glue->cfg)
		return -EINVAL;

	/* GET clocks */
	ret = clk_get_by_name(dev, "usbpll", &glue->usbpll_clk);
	if (ret) {
		dev_err(dev, "failed to get usbpll clock\n");
		return ret;
	}
	ret = clk_get_by_name(dev, "usbmcu", &glue->usbmcu_clk);
	if (ret) {
		dev_err(dev, "failed to get usbmcu clock\n");
		return ret;
	}
	ret = clk_get_by_name(dev, "usb", &glue->usb_clk);
	if (ret) {
		dev_err(dev, "failed to get usb clock\n");
		return ret;
	}

	/* ENABLE clocks*/
	ret = clk_enable(&glue->usbpll_clk);
	if (ret) {
		dev_err(dev, "failed to enable usbpll clock\n");
		return ret;
	}
	ret = clk_enable(&glue->usbmcu_clk);
	if (ret) {
		dev_err(dev, "failed to enable usbmcu clock\n");
		return ret;
	}
	ret = clk_enable(&glue->usb_clk);
	if (ret) {
		dev_err(dev, "failed to enable usb clock\n");
		return ret;
	}

	/* GET phy */
	ret = generic_phy_get_by_name(dev, "usb", &glue->phy);
	if (ret) {
		pr_err("failed to get USB PHY\n");
		return ret;
	}

	/* ENABLE phy */
	ret = generic_phy_power_on(&glue->phy);
	if (ret) {
		pr_debug("failed to power on USB PHY\n");
		return ret;
	}

	memset(&pdata, 0, sizeof(pdata));
	pdata.power = (u8)400;
	pdata.platform_ops = &mtk_musb_ops;
	pdata.config = glue->cfg->config;
	pdata.mode = MUSB_PERIPHERAL;

	printf("%s: Registering MUSB\n", __func__);
	host->host = musb_register(&pdata, &glue->dev, base);
	if (!host->host)
		return -EIO;

	return 0;
}

static int mtk_musb_remove(struct udevice *dev)
{
	struct mtk_musb_glue *glue = dev_get_priv(dev);
	struct musb_host_data *host = &glue->mdata;

	printf("%s: Stopping MUSB\n", __func__);
	musb_stop(host->host);
	free(host->host);
	host->host = NULL;

	return 0;
}

static struct musb_fifo_cfg mtk_musb_mode_cfg[] = {
	MUSB_EP_FIFO_SINGLE(1, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(1, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(2, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(2, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(3, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(3, FIFO_RX, 512),
};

static struct musb_hdrc_config musb_config = {
	.fifo_cfg       = mtk_musb_mode_cfg,
	.fifo_cfg_size  = ARRAY_SIZE(mtk_musb_mode_cfg),
	.multipoint	    = true,
	.dyn_fifo	    = true,
	.num_eps	    = 4,
	.ram_bits	    = 11,
};

static const struct mtk_musb_config mt6572_cfg = {
	.config = &musb_config,
};

static const struct udevice_id mtk_musb_ids[] = {
	{ .compatible = "mediatek,mt6572-musb", .data = (ulong)&mt6572_cfg },
	{ }
};

U_BOOT_DRIVER(mt6572_musb) = {
	.name		= "mt6572_musb",
	.id		    = UCLASS_USB_GADGET_GENERIC,
	.of_match	= mtk_musb_ids,
	.probe		= mtk_musb_probe,
	.remove		= mtk_musb_remove,
	.plat_auto	= sizeof(struct usb_plat),
	.priv_auto	= sizeof(struct mtk_musb_glue),
};
