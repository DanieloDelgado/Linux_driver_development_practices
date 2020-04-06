#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h> /* stryct file_operations */
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

#define BCM2836_PERI_BASE		(0x3F000000)
#define GPIO_BASE				(BCM2836_PERI_BASE + 0x200000)	/* GPIO controller: 3F200000 */

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

#define GPFSEL2 	 (GPIO_BASE + 0x08) /* GPIO Function Select 2 */
#define GPSET0   	 (GPIO_BASE + 0x1C) /* GPIO Pin Output Set 0 */
#define GPCLR0 		 (GPIO_BASE + 0x28) /* GPIO Pin Output Clear 0 */

#define GPIO_MASK_ALL_LEDS 		(FSEL_27_MASK | FSEL_26_MASK | FSEL_22_MASK)
#define GPIO_SET_FUNCTION_LEDS 	(GPIO_27_FUNC | GPIO_26_FUNC | GPIO_22_FUNC)
#define GPIO_SET_ALL_LEDS		(GPIO_27_INDEX | GPIO_26_INDEX | GPIO_22_INDEX)

static void __iomem *GPFSEL2_V;
static void __iomem *GPSET0_V;
static void __iomem *GPCLR0_V;

struct led_dev
{
	struct miscdevice led_misc_device; /* assign device for each led */
	u32 led_mask; /* Different mask depending on the LED */ /* TODO: NOt sure what does it mean */
	const char *led_name; /* stores "label" string */
	char led_value[8];
};

static ssize_t led_read(struct file *file, char __user *buff, size_t count, loff_t *ppos)
{
	int len;
	struct led_dev *led_device;

	led_device = container_of(file->private_data, struct led_dev, led_misc_device);	

	if (0 == *ppos){
		len = strlen(led_device->led_value);
		led_device->led_value[len] = '\n'; /* add \n after on/off */

		if (copy_to_user(buff, &led_device->led_value, len+1)){
			pr_info("Failed to return led_value to user space\n");
			return -EFAULT;
		}
		
		*ppos += 1;
		return sizeof(led_device->led_value); /* exit first func call */
	}

	return 0; /* exit and do not recall func again */
}

static ssize_t led_write(struct file *file, const char __user *buff,
							size_t count, loff_t *ppos)
{
	const char *led_on  = "on";
	const char *led_off = "off";
	struct led_dev *led_device;

	led_device = container_of(file->private_data, struct led_dev, led_misc_device);

	if (copy_from_user(led_device->led_value, buff, count)) {
		pr_info("Bad copied value\n");
		return -EFAULT;
	}

	led_device->led_value[count-1] = '\0';

	/* compare strings to switch on/off the LED */
	if (strcmp(led_device->led_value, led_on) == 0){
		iowrite32(led_device->led_mask, GPSET0_V);
	}
	else if (strcmp(led_device->led_value, led_off) == 0){
		iowrite32(led_device->led_mask, GPCLR0_V);
	}
	else {
		pr_info("Bad value\n");
		return -EINVAL;
	}

	return count;
}

static const struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.read  = led_read,
	.write = led_write,
};

static int __init led_probe(struct platform_device *pdev)
{
	int ret_val;
    struct led_dev *led_device;
	char led_val[8] = "off\n";

    pr_info("led_probe enter\n");
 
    led_device = devm_kzalloc(&pdev->dev, sizeof(struct led_dev), GFP_KERNEL);

    if (NULL == led_device)
    {
    	pr_err("The device couldn't be allocated\n");
    	return -1;
    }

    of_property_read_string(pdev->dev.of_node, "label", &led_device->led_name);


    led_device->led_misc_device.minor = MISC_DYNAMIC_MINOR;
    led_device->led_misc_device.name = led_device->led_name;
	led_device->led_misc_device.fops = &led_fops;

	ret_val = misc_register(&led_device->led_misc_device);

	if ( 0 != ret_val )
	{
		pr_err("Unable to register device %s\n", led_device->led_name);
		devm_kfree(&pdev->dev, NULL);
	}

	if (strcmp(led_device->led_name, "ledred") == 0){
		led_device->led_mask = GPIO_27_INDEX;
	}
	else if (strcmp(led_device->led_name, "ledgreen") == 0){
		led_device->led_mask = GPIO_22_INDEX;
	}
	else if (strcmp(led_device->led_name, "ledblue") == 0){
		led_device->led_mask = GPIO_26_INDEX;
	}
	else{
		pr_err("Bad device tree value\n");
		return -EINVAL;
	}

	/* Initialize the led status to off */
	memcpy(led_device->led_value, led_val, sizeof(led_val));

	platform_set_drvdata(pdev, led_device);

	pr_info("Device %s\n", led_device->led_name);
    
    return 0;
}

static int __exit led_remove(struct platform_device *pdev)
{
	struct led_dev *led_device = platform_get_drvdata(pdev);

	pr_info("led_remove called\n");

	misc_deregister(&led_device->led_misc_device);

	return 0;
}

static const struct of_device_id my_of_ids[] = {
	{ .compatible = "arrow,RGBleds" },
	{},
};

MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver led_platform_driver = {
    .probe  = led_probe,
    .remove = led_remove,
    .driver = {
        .name = "RGBleds",
        .of_match_table = my_of_ids,
        .owner = THIS_MODULE,
    }
};

static int led_init(void)
{
	int ret_val;
	u32 funct_sel_read;
	u32 funct_sel_write;
	
	pr_info("Init driver\n");

	ret_val = platform_driver_register(&led_platform_driver);

	if (0 != ret_val){
		pr_err("Unable to register platform_driver\n");
		return ret_val;
	}

	GPFSEL2_V = ioremap(GPFSEL2, sizeof(u32));
	GPSET0_V = ioremap(GPSET0, sizeof(u32));
	GPCLR0_V = ioremap(GPCLR0, sizeof(u32));

	funct_sel_read = ioread32(GPFSEL2_V); /* Read current configuration */

	funct_sel_write = (funct_sel_read & ~GPIO_MASK_ALL_LEDS) |
						(GPIO_SET_FUNCTION_LEDS & GPIO_MASK_ALL_LEDS);


	iowrite32(funct_sel_write, GPFSEL2_V); /* Set all leds to outputs */
	iowrite32(GPIO_27_INDEX, GPCLR0_V); /* turn off all leds */
	
	return 0;
}

static void led_exit(void)
{
	platform_driver_unregister(&led_platform_driver);

	iounmap(GPCLR0_V);
	iounmap(GPSET0_V);
	iounmap(GPFSEL2_V);

	pr_info("Deinit driver\n");
}

module_init(led_init);
module_exit(led_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel O. Delgado <danielo.delgado@gmail.com>");
MODULE_DESCRIPTION("RGB platform driver");
 