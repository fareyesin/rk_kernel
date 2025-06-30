#if 0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/kmod.h>
#include <linux/mutex.h>

static struct timer_list reload_timer;
static DEFINE_MUTEX(reload_mutex);

#define RELOAD_INTERVAL (10 * HZ) // 20秒

static void reload_nvme_modules(struct timer_list *t)
{
    char *envp[] = { "HOME=/", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };
    char *rmmod_argv[] = { "/sbin/rmmod", "nvme", "nvme_core", NULL };
    char *modprobe_argv[] = { "/sbin/modprobe", "nvme_core", "nvme", NULL };
    
    // 检查是否已有重载操作在进行
    if (!mutex_trylock(&reload_mutex)) {
        printk(KERN_WARNING "[RELOAD]:NVMe reload skipped: previous operation still in progress\n");
        goto reschedule;
    }
    
    printk(KERN_INFO "[RELOAD]1:Starting NVMe module reload sequence\n");
    
    // 卸载模块
    printk(KERN_INFO "[RELOAD]2:Unloading NVMe modules...\n");
    int ret = call_usermodehelper(rmmod_argv[0], rmmod_argv, envp, UMH_WAIT_PROC);
    if (ret) {
        printk(KERN_ERR "Failed to unload NVMe modules: %d\n", ret);
        goto unlock;
    }
    
    // 短暂延迟确保完全卸载
    msleep(500);
    
    // 重新加载模块
    printk(KERN_INFO "[RELOAD]3:Reloading NVMe modules...\n");
    ret = call_usermodehelper(modprobe_argv[0], modprobe_argv, envp, UMH_WAIT_PROC);
    if (ret) {
        printk(KERN_ERR "[RELOAD]:Failed to reload NVMe modules: %d\n", ret);
    } else {
        printk(KERN_INFO "[RELOAD]:NVMe modules reloaded successfully\n");
    }

unlock:
    mutex_unlock(&reload_mutex);
    
reschedule:
    // 重新设置定时器
    mod_timer(&reload_timer, jiffies + RELOAD_INTERVAL);
	printk(KERN_INFO "[RELOAD]:end\n");
}

static int __init reload_init(void)
{
    printk(KERN_INFO "NVMe periodic reload module initialized\n");
    
    // 初始化定时器
    timer_setup(&reload_timer, reload_nvme_modules, 0);
    mod_timer(&reload_timer, jiffies + RELOAD_INTERVAL);
    
    return 0;
}

static void __exit reload_exit(void)
{
    del_timer_sync(&reload_timer);
    printk(KERN_INFO "NVMe periodic reload module unloaded\n");
}

module_init(reload_init);
module_exit(reload_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LJZ");
MODULE_DESCRIPTION("Periodic NVMe driver reload every 20 seconds");

#endif

//////////////////////////////////////////
#if 1

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/gpio/consumer.h>
#include <linux/types.h>

#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/gfp.h>
#include<linux/kthread.h>
#include<linux/delay.h>
#include <linux/pci.h>
#if 0

struct my_gpio_data {
	int irq_num;
	struct gpio_chip chip;
	struct gpio_desc *gpio;
};




static irqreturn_t my_gpio_handler(int irq, void *dev_id)
{
	// 中断处理逻辑
	printk("**********LJZ DEBUG : get GPIO egde************\r\n");
	pr_info("GPIO IRQ triggered\n");
	return IRQ_HANDLED;
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


static DEVICE_ATTR(value, 0444, gpio_value_show,#include NULL);
static DEVICE_ATTR(edge, 0644, gpio_edge_show, gpio_edge_store);



static int my_gpio_probe(struct platform_device *pdev)
{
	struct my_gpio_data *data;pci_register_driver
   printk("LJZ: [MY_GPIO] 1.probe\r\n");
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

   
    if (!pdev->dev.of_node) {
        dev_err(&pdev->dev, "No device tree node associated\n");
        return -EINVAL;#include
    data->gpio = devm_gpiod_get(&pdev->dev, NULL, GPIOD_IN);
    if (IS_ERR(data->gpio)) {
        ret = PTR_ERR(data->gpio);
        dev_err(&pdev->dev, "Failed to get GPIO: %d\n", ret);
        return ret;
    }

    // 将GPIO转换为中断号
    data->irq_num = gpiod_to_irq(data->gpio);
    if (data->irq_num < 0) {
        dev_err(&pdev->dev, "Failed to get IRQ from GPIO: %d\n", data->irq_num);
        return data->irq_num;
    }

    printk("LJZ Debug: Using IRQ number %d\n", data->irq_num);
#include

	// 获取中断号
	// data->irq_num = platform_gepci_register_driver
	// 请求中断 tag ：1
	// ret = devm_request_irq(&pdev->dev, data->irq_num, my_gpio_handler,
	// 		      IRQF_TRIGGER_RISING, "re_gpio", data);

	ret = devm_request_irq(&pdev->dev, data->irq_num, my_gpio_handler,
			      IRQF_TRIGGER_RISING, "ljz_ssdirq", data);
	printk("LJZ Debug: GPIO get irq num:%d \r\n",data->irq_num );

	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ\n");
		return ret;
	}else{
		printk("LJZ Debug: IRQ suc \r\n");#include
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
}#include




static const struct of_device_id my_gpio_of_match[] = {
	{ .compatible = "ljz_ssdirq"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_gpio_of_match);

static int my_gpio_remove(struct platform_device *pdev)
{
	struct my_gpio_data *data = platform_get_drvdata(pdev);
	device_remove_file(&pdev->dev, &dev_attr_value);
	device_remove_file(&pdev->dev, &dev_attr_edge);
	devm_free_irq(&pdev->dev, data->irq_num, data);
	return 0;
}
#endif


static struct task_struct *unload_thread; 

extern int nvme_init(void);
extern void nvme_exit(void);

extern void __exit nvme_core_exit(void);
extern int __init nvme_core_init(void);


//extern int cpci_hp_start(void);
//6.30
struct pci_dev *pdev = NULL;


#if 1
extern void nvme_remove(struct pci_dev *pdev);
extern int nvme_probe(struct pci_dev *pdev, const struct pci_device_id *id);

static const struct pci_device_id nvme_id_table[] = {
    //{ PCI_DEVICE(0x144d, 0xa821), .driver_data = NVME_QUIRK_SAMSUNG },  // 三星NVMe SSD
    { PCI_DEVICE_CLASS(PCI_CLASS_STORAGE_EXPRESS, 0xffffff) },          // 通用NVMe设备
    { 0 }  // 结束标记
};

static int unload_thread_func(void *data)
{
	int ret = -ENOMEM,i=0;
	printk("LJZ[DRIVER GPIO] in thread\r\n");
	while(1)
	{    
		printk("LJZ[DRIVER GPIO] in loop\r\n");
		for(i=0;i<6;i++)
		{
          msleep(5000); // 休眠5秒
		  printk("delay %dth\r\n",i);
		}
		
		while ((pdev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, pdev))) {
		// 仅处理 NVMe 设备（Class Code: 0x010802）
			printk("LJZ :ALL Found device Vendor=%04x\r\n",pdev->vendor);
			if (pdev->class == PCI_CLASS_STORAGE_EXPRESS) {
				printk(KERN_INFO "LJZ:Found NVMe device: Vendor=%04x Device=%04x\n",
					pdev->vendor, pdev->device);
                nvme_remove(pdev);

				ret=nvme_probe(pdev,nvme_id_table);
				if(ret<0)
				{
					printk("LJZ :nvme_probe failed\r\n");
					break;
				}
					
			}
	    }
	}
	return 0; 
}

#else

struct pci_host_bridge *bridge;
static int unload_thread_func(void *data)
{
	 unsigned int devfn;
	
	printk("LJZ[DRIVER GPIO] in thread\r\n");

	bridge = pci_alloc_host_bridge(0); //扫描桥接芯片的根设备
		if (!bridge)
		{
		 printk("LJZ:pci_alloc_host_bridge->no found bridge\r\n");
         return 0;
		}
		while(1)
		{
			printk("LJZ:pci_host_probe begain\r\n");
			// pci_host_probe(bridge);
			for (devfn = 0; devfn < 256; devfn += 8) 
		    	pci_scan_slot(bridge->bus, devfn);
			printk("LJZ:pci_host_probe end\r\n");
			msleep(5000); // 休眠5秒
		}
    return 0; 
}

#endif

static const struct of_device_id my_gpio_of_match[] = {
	{ .compatible = "ljz_ssdirq"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_gpio_of_match);


static int my_gpio_remove(struct platform_device *pdev)
{
	printk("LJZ[DRIVER GPIO] beigain remove\r\n");
	return 0;
}


static int my_gpio_probe(struct platform_device *pdev)
{
	printk("LJZ[DRIVER GPIO] beigain probe\r\n");
	unload_thread = kthread_run(unload_thread_func, NULL, "unload_driver");
	return 0;
}


static struct platform_driver my_gpio_driver __refdata = {
	.probe = my_gpio_probe,
	.remove = my_gpio_remove,
	.driver = {
		.name = "ljz_ssdirq",
		.of_match_table = my_gpio_of_match,
	},
};

module_platform_driver(my_gpio_driver);
// static const struct platform_driver my_gpio_driver = {
// 	.probe = my_gpio_probe,
// 	.remove = my_gpio_remove,
// 	.driver = {
// 		.name = "myre_gpio",
// 		.of_match_table = my_gpio_of_match,
// 	},
// };
MODULE_AUTHOR("BIWIN-LJZ");
MODULE_DESCRIPTION("GPIO IRQ Driver for MINSSD");
MODULE_LICENSE("GPL");

#endif
