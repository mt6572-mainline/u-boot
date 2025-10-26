#include <dm.h>
#include <dm/device_compat.h>
#include <dm/uclass.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>
#include <power/pmic.h>
#include <power/mtk_pwrap.h>
#include <linux/input.h>
#include <vsprintf.h>
#include <button.h>

#define MT6323_CHRSTATUS   0x0142

struct mtk_pwrkey_priv {
    struct udevice *pwrap;
};

static int mtk_pwrkey_get_code(struct udevice *dev)
{
  return KEY_ENTER;
}

static enum button_state_t mtk_pwrkey_get_state(struct udevice *dev)
{
	struct mtk_pwrkey_priv *priv = dev_get_priv(dev);
  int val = pmic_reg_read(priv->pwrap, MT6323_CHRSTATUS);
  return ((val >> 1) & 0x1) ? BUTTON_OFF : BUTTON_ON;
}

static int mtk_pwrkey_probe(struct udevice *dev)
{
	struct mtk_pwrkey_priv *priv = dev_get_priv(dev);
  struct udevice *mt6323 = dev_get_parent(dev);

  if (!mt6323) {
    printf("Failed to find mt6323 parent\n");
    return -ENODEV;
  }

	priv->pwrap = dev_get_parent(mt6323);
  if (!priv->pwrap) {
    printf("Failed to find pwrap\n");
    return -ENODEV;
  }
	
	return 0;
}

static int mtk_pwrkey_of_to_plat(struct udevice *dev)
{
  struct button_uc_plat *plat = dev_get_uclass_plat(dev);
  plat->label = "power";
  return 0;
}

static const struct button_ops mtk_pwrkey_ops = {
	.get_state = mtk_pwrkey_get_state,
  .get_code = mtk_pwrkey_get_code,
};

static const struct udevice_id mtk_pwrkey_ids[] = {
	{ .compatible = "mediatek,mt6323-pwrkey" },
	{ }
};

U_BOOT_DRIVER(mtk_pwrkey) = {
	.name = "mtk_pwrkey",
	.id = UCLASS_BUTTON,
	.of_match = mtk_pwrkey_ids,
	.ops = &mtk_pwrkey_ops,
	.probe = mtk_pwrkey_probe,
	.of_to_plat = mtk_pwrkey_of_to_plat,
	.priv_auto  = sizeof(struct mtk_pwrkey_priv),
};
