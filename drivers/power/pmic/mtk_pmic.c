#include <dm.h>
#include <dm/lists.h>
#include <power/pmic.h>
#include <power/mtk_pmic.h>

static const struct pmic_child_info pmic_children_info[] = {
    { .prefix = "", .driver = "mtk_regulator" }
};

static int mtk_pmic_bind(struct udevice *dev)
{
    int ret;
    struct udevice *pwrkey;
    ofnode regulators_node;
    ofnode pwrkey_node = dev_read_subnode(dev, "pwrkey");

    regulators_node = dev_read_subnode(dev, "regulators");
    if (ofnode_valid(regulators_node)) {
        pmic_bind_children(dev, regulators_node, pmic_children_info);
    } else {
        debug("%s regulators subnode not found\n", dev->name);
    }

    if (ofnode_valid(pwrkey_node)) {
        struct udevice *child;
        ret = device_bind_driver_to_node(dev, "mtk_pwrkey", ofnode_get_name(pwrkey_node), pwrkey_node, &child);
        if (ret < 0) {
            printf("Failed to bind pwrkey (err %d)\n", ret);
        }
    }
    
    /* Trigger probe of pwrkey */
    ret = uclass_get_device(UCLASS_BUTTON, 0, &pwrkey);
	if (ret) {
		printf("%s: Failed to find PWRKEY device (err %d)\n", __func__, ret);
		return ret;
    }

    return 0;
}

/* 1st value - min, 2nd value - max */
static u32 mt6323_vmc_voltages[] = {
    1800000,
    3300000
};

static u32 mt6323_vmch_voltages[] = {
    3000000,
    3300000
};

static struct mtk_pmic_regulator mt6323_regulators[] = {
    {
        .name = "vmc",
        .en = MTK_PMIC_REG(0x504, 1, 12),
        .vosel = MTK_PMIC_REG(0x52a, 1, 4),
        .voltages_count = 2,
        .voltages = mt6323_vmc_voltages,
    },
    {
        .name = "vmch",
        .en = MTK_PMIC_REG(0x506, 1, 14),
        .vosel = MTK_PMIC_REG(0x52c, 1, 7),
        .voltages_count = 2,
        .voltages = mt6323_vmch_voltages,
    },
    { },
};

static struct mtk_pmic_pdata mt6323_reg_data = {
    .regulators = mt6323_regulators,
    .regs = { },
};

static const struct udevice_id mtk_pmic_match[] = {
    { .compatible = "mediatek,mt6323", .data = (ulong)&mt6323_reg_data },
    { }
};

U_BOOT_DRIVER(mtk_pmic) = {
    .name = "mtk_pmic",
    .id = UCLASS_NOP,
    .of_match = mtk_pmic_match,
    .bind = mtk_pmic_bind,
};
