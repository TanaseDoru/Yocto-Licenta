// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#define DRIVER_NAME "dht11_kmod"
#define DEVICE_NAME "dht11"

static int gpio_pin = 4;
module_param(gpio_pin, int, 0644);
MODULE_PARM_DESC(gpio_pin, "GPIO pin connected to DHT11 data line");

static int major;
static struct gpio_desc *dht11_gpio;
static ktime_t last_sample_time;

static int dht11_open(struct inode *inode, struct file *file)
{
	return 0;
}

static long wait_for_level(struct gpio_desc *gpio, int expected, unsigned int timeout_us)
{
	ktime_t start = ktime_get();
	while (gpiod_get_value(gpio) == expected) {
		if (ktime_us_delta(ktime_get(), start) > timeout_us)
			return -ETIMEDOUT;
		cpu_relax();
	}
	return ktime_us_delta(ktime_get(), start);
}

static int dht11_read_data(struct gpio_desc *gpio, uint8_t data[5])
{
	int i, j;
	long pulse;

	memset(data, 0, 5);

	/* Start signal */
	gpiod_direction_output(gpio, 0);
	msleep(20);
	gpiod_direction_input(gpio);
	udelay(40);

	/* Response */
	wait_for_level(gpio, 1, 100);
	wait_for_level(gpio, 0, 100);

	/* Read 40 bits */
	for (i = 0; i < 40; i++) {
		wait_for_level(gpio, 1, 70);
		pulse = wait_for_level(gpio, 0, 150);
		j = i / 8;
		data[j] <<= 1;
		if (pulse > 50)
			data[j] |= 1;
	}

	return 0;
}

static ssize_t dht11_read(struct file *file, char __user *buf,
			  size_t len, loff_t *offset)
{
	uint8_t data[5];
	uint8_t checksum;
	int humi, temp;
	char outbuf[64];
	int outlen;
	int ret;

	if (ktime_to_ns(last_sample_time) != 0 &&
	    ktime_us_delta(ktime_get(), last_sample_time) < 1000000)
		msleep(1000);

	ret = dht11_read_data(dht11_gpio, data);
	if (ret)
		return ret;

	checksum = data[0] + data[1] + data[2] + data[3];
	last_sample_time = ktime_get();

	humi = data[0];
	temp = data[2];

	outlen = snprintf(outbuf, sizeof(outbuf),
			  "Temp:%dC Hum:%d%% %s\n",
			  temp, humi, (checksum != data[4]) ? "(checksum!)" : "");

	if (copy_to_user(buf, outbuf, outlen))
		return -EFAULT;

	return outlen;
}

static const struct file_operations dht11_fops = {
	.owner = THIS_MODULE,
	.open  = dht11_open,
	.read  = dht11_read,
};

static int __init dht11_init(void)
{
	struct gpio_desc *gpio;
	int ret;

	pr_info(DRIVER_NAME ": init (GPIO %d)\n", gpio_pin);

	gpio = gpio_to_desc(gpio_pin);
	if (!gpio) {
		pr_err(DRIVER_NAME ": cannot get GPIO descriptor for %d\n", gpio_pin);
		return -ENODEV;
	}
	dht11_gpio = gpio;

	/* Make sure we start with pin high */
	gpiod_direction_output(dht11_gpio, 1);

	major = register_chrdev(0, DEVICE_NAME, &dht11_fops);
	if (major < 0) {
		pr_err(DRIVER_NAME ": failed to register char device\n");
		return major;
	}

	pr_info(DRIVER_NAME ": loaded with major %d\n", major);
	return 0;
}

static void __exit dht11_exit(void)
{
	unregister_chrdev(major, DEVICE_NAME);
	pr_info(DRIVER_NAME ": unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tanase / GPT-5");
MODULE_DESCRIPTION("DHT11 kernel driver using gpiod API (for 6.x kernels)");
MODULE_VERSION("1.1");

module_init(dht11_init);
module_exit(dht11_exit);
