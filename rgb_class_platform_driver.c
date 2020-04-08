#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h> /* stryct file_operations */
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/leds.h>

#define GPIO_22	(22)
#define GPIO_26 (26)
#define GPIO_27	(27)

#define GPIO_22_INDEX (1 << GPIO_22)
#define GPIO_26_INDEX (1 << GPIO_26)
#define GPIO_27_INDEX (1 << GPIO_27)

#define GPIO_22_FUNC (0x40)
#define GPIO_26_FUNC (0x40000)
#define GPIO_27_FUNC (0x200000)

#define FSEL_22_MASK (0x1C0)
#define FSEL_26_MASK (0x1C0000)
#define FSEL_27_MASK (0xE00000)

#define GPFSEL2_offset 	 (0x08) /* GPIO Function Select 2 */
#define GPSET0_offset  	 (0x1C) /* GPIO Pin Output Set 0 */
#define GPCLR0_offset	 (0x28) /* GPIO Pin Output Clear 0 */

#define GPIO_MASK_ALL_LEDS 		(FSEL_27_MASK | FSEL_26_MASK | FSEL_22_MASK)
#define GPIO_SET_FUNCTION_LEDS 	(GPIO_27_FUNC | GPIO_26_FUNC | GPIO_22_FUNC)
#define GPIO_SET_ALL_LEDS		(GPIO_27_INDEX | GPIO_26_INDEX | GPIO_22_INDEX)

struct led_dev
{
	u32 led_mask; /* Mask for the corresponding GPIO */
	void __iomem *base;
	struct led_classdev cdev;
};

static void led_control(struct led_classdev *led_cdev, enum led_brightness b)
{
	struct led_dev *led = container_of(led_cdev, struct led_dev, cdev);

	if (b != LED_OFF){
		iowrite32(led->led_mask, led->base + GPSET0_offset);
	}
	else {
		iowrite32(led->led_mask, led->base + GPCLR0_offset);
	}
}


static int __init ledclass_probe(struct platform_device *pdev)
{
	void __iomem *g_ioremap_addr;
	struct device_node *child;
	struct resource *r;
	struct device *dev = &pdev->dev;
	u32 funct_sel_write;
	u32 funct_sel_read;
	int ret;

	pr_info("Get memory resource\n");
	/* get your first memory resource from device tree */
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (NULL == r) {
		dev_err(dev, "IORESOURCE_MEM, 0 - does not exist\n");
		return -EINVAL;
	}
	/* ioremap your memory region to device*/
	g_ioremap_addr = devm_ioremap(dev, r->start, resource_size(r));
	if (!g_ioremap_addr){
		dev_err(dev, "ioremap failed \n");
		return -ENOMEM;
	}

	/* The ranges used in the DT are remmaped to follow the datasheet of the bcm2835 */
	pr_info("r->start = 0x%08lx\n", (long unsigned int)r->start);
	pr_info("r->end = 0x%08lx\n", (long unsigned int)r->end);

	funct_sel_read = ioread32(g_ioremap_addr + GPFSEL2_offset); /* Read current configuration */

	funct_sel_write = (funct_sel_read & ~GPIO_MASK_ALL_LEDS) |
						(GPIO_SET_FUNCTION_LEDS & GPIO_MASK_ALL_LEDS);

	/* It is not needed to set the gpios as outputs, this is done by pinctrl-bcm2835 */
	iowrite32(funct_sel_write, g_ioremap_addr + GPFSEL2_offset); /* Set all leds to outputs */
	iowrite32(GPIO_SET_ALL_LEDS, g_ioremap_addr + GPCLR0_offset); /* turn off all leds */

	/* parse each children device under LED RGB parent node */
	for_each_child_of_node(dev->of_node, child){
		struct led_dev *led_device;
		/* creates an Led_classdev struct for each child device */
		struct led_classdev *cdev;

		/* allocates a private structure in each "for" iteration */
		led_device  = devm_kzalloc(dev, sizeof(*led_device), GFP_KERNEL);

		cdev = &led_device->cdev;
		led_device->base = g_ioremap_addr;

		/* assigns a mask to each children (child) device */
		of_property_read_string(child, "label", &cdev->name);

		/* TODO: import the GPIO mask from the device node*/
		if (strcmp(cdev->name, "red") == 0){
			led_device->led_mask = GPIO_27_INDEX;
			led_device->cdev.default_trigger = "heartbeat";
		}
		else if (strcmp(cdev->name, "green") == 0){
			led_device->led_mask = GPIO_22_INDEX;
		}
		else if (strcmp(cdev->name, "blue") == 0){
			led_device->led_mask = GPIO_26_INDEX;
		}
		else {	
			dev_err(dev, "Bad device tree value\n");
			return -EINVAL;
		}

		/* Initialize each led_classdev struct */
		/* Disable timer trigger until led is on */
		led_device->cdev.brightness = LED_OFF;
		led_device->cdev.brightness_set = led_control;

		/* register each LED class device */
		ret = devm_led_classdev_register(dev, &led_device->cdev);
	}

	dev_info(dev, "leds_probe exit\n");

	return 0;
}

static int __exit ledclass_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "ledclass_remove called\n");
	return 0;
}

static const struct of_device_id my_of_ids[] = {
	{ .compatible = "arrow,RGBclassleds" },
	{},
};

MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver led_platform_driver = {
    .probe  = ledclass_probe,
    .remove = ledclass_remove,
    .driver = {
        .name = "RGBclassleds",
        .of_match_table = my_of_ids,
        .owner = THIS_MODULE,
    }
};

static int led_init(void)
{
	int ret_val;

	ret_val = platform_driver_register(&led_platform_driver);

	if (0 != ret_val){
		pr_err("Unable to register platform_driver\n");
	}

	return ret_val;
}

static void led_exit(void)
{
	platform_driver_unregister(&led_platform_driver);
	pr_info("Deinit driver\n");
}

module_init(led_init);
module_exit(led_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel O. Delgado <danielo.delgado@gmail.com>");
MODULE_DESCRIPTION("RGB class platform driver");
