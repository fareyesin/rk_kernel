#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

struct my_gpio_data {
	int irq_num;
	struct gpio_chip chip;
};

static irqreturn_t my_gpio_handler(int irq, void *dev_id)
{
	// 中断处理逻辑
	pr_info("GPIO IRQ triggered\n");
	return IRQ_HANDLED;
}

static int my_gpio_probe(struct platform_device *pdev)
{
	struct my_gpio_data *data;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	// 获取中断号
	data->irq_num = platform_get_irq(pdev, 0);
	if (data->irq_num < 0)
		return data->irq_num;

	// 请求中断
	ret = devm_request_irq(&pdev->dev, data->irq_num, my_gpio_handler, 
			      IRQF_TRIGGER_RISING, "my_gpio", data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ\n");
		return ret;
	}
	// 初始化GPIO芯片
	data->chip.label = "my_gpio_chip";
	data->chip.base = -1;
	data->chip.ngpio = 1;
	data->chip.parent = &pdev->dev;

	// 注册sysfs接口
	ret = device_create_file(&pdev->dev, &dev_attr_value);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create value sysfs\n");
		goto err_irq;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_edge);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create edge sysfs\n");
		goto err_value;
	}

	platform_set_drvdata(pdev, data);
	return 0;

err_value:
	device_remove_file(&pdev->dev, &dev_attr_value);
err_irq:
	devm_free_irq(&pdev->dev, data->irq_num, data);
	return ret;
}

static ssize_t gpio_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct my_gpio_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", gpio_get_value(data->chip.base));
}

static ssize_t gpio_edge_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "rising\n");
}

static ssize_t gpio_edge_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct my_gpio_data *data = dev_get_drvdata(dev);
	if (sysfs_streq(buf, "rising")) {
		irq_set_irq_type(data->irq_num, IRQF_TRIGGER_RISING);
		return count;
	}
	return -EINVAL;
}

static DEVICE_ATTR(value, 0444, gpio_value_show, NULL);
static DEVICE_ATTR(edge, 0644, gpio_edge_show, gpio_edge_store);

static const struct of_device_id my_gpio_of_match[] = {
	{ .compatible = "re-gpio" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_gpio_of_match);

static struct platform_driver my_gpio_driver __refdata = {
	.probe = my_gpio_probe,
	.remove = my_gpio_remove,
	.driver = {
		.name = "my_gpio",
		.of_match_table = my_gpio_of_match,
	},
};

module_platform_driver(my_gpio_driver);


static int my_gpio_remove(struct platform_device *pdev)
{
	struct my_gpio_data *data = platform_get_drvdata(pdev);
	device_remove_file(&pdev->dev, &dev_attr_value);
	device_remove_file(&pdev->dev, &dev_attr_edge);
	devm_free_irq(&pdev->dev, data->irq_num, data);
	return 0;
}
static const struct platform_driver my_gpio_driver = {
	.probe = my_gpio_probe,
	.remove = my_gpio_remove,
	.driver = {
		.name = "my_gpio",
		.of_match_table = my_gpio_of_match,
	},
};

MODULE_AUTHOR("BIWIN-LJZ");
MODULE_DESCRIPTION("GPIO IRQ Driver for MINSSD");
MODULE_LICENSE("GPL");