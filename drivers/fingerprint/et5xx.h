/*
 * Copyright (C) 2016 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _ET5XX_LINUX_DIRVER_H_
#define _ET5XX_LINUX_DIRVER_H_

#ifdef ENABLE_SENSORS_FPRINT_SECURE
#define FEATURE_SPI_WAKELOCK
#endif /* CONFIG_SEC_FACTORY */

#include <linux/module.h>
#include <linux/spi/spi.h>

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include "fingerprint.h"
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>
#include <linux/sysfs.h>
#include <linux/cpu_input_boost.h>
#include <linux/devfreq_boost.h>
#include <linux/state_notifier.h>
#include <linux/pinctrl/consumer.h>
#include "../pinctrl/core.h"
#include <linux/platform_data/spi-s3c64xx.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#ifdef ENABLE_SENSORS_FPRINT_SECURE
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spidev.h>
#include <linux/smc.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl330.h>
#if defined(CONFIG_SECURE_OS_BOOSTER_API)
#if defined(CONFIG_SOC_EXYNOS8890) || defined(CONFIG_SOC_EXYNOS7870) \
	|| defined(CONFIG_SOC_EXYNOS7880) || defined(CONFIG_SOC_EXYNOS7570) \
	|| defined(CONFIG_SOC_EXYNOS7885) || defined(CONFIG_SOC_EXYNOS9810)
#include <soc/samsung/secos_booster.h>
#else
#include <mach/secos_booster.h>
#endif
#elif defined(CONFIG_TZDEV_BOOST)
#include <../drivers/misc/tzdev/tz_boost.h>
#endif

struct sec_spi_info {
	int		port;
	unsigned long	speed;
};
#endif

/*
 * This feature is temporary for exynos AP only.
 * It's for control GPIO config on enabled TZ before enable GPIO protection.
 * If it's still defined this feature after enable GPIO protection,
 * it will be happened kernel panic
 * So it should be un-defined after enable GPIO protection
 */
#undef DISABLED_GPIO_PROTECTION

/*#define ET5XX_SPI_DEBUG*/

#ifdef ET5XX_SPI_DEBUG
#define DEBUG_PRINT(fmt, args...) pr_err(fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)
#endif

#define VENDOR						"EGISTEC"
#define CHIP_ID						"ET5XX"

/* assigned */
#define ET5XX_MAJOR					152
/* ... up to 256 */
#define N_SPI_MINORS					32

#define OP_REG_R						0x20
#define OP_REG_R_C						0x22
#define OP_REG_R_C_BW					0x23
#define OP_REG_W						0x24
#define OP_REG_W_C						0x26
#define OP_REG_W_C_BW					0x27
#define OP_NVM_ON_R						0x40
#define OP_NVM_ON_W						0x42
#define OP_NVM_RE						0x44
#define OP_NVM_WE						0x46
#define OP_NVM_OFF						0x48
#define OP_IMG_R						0x50
#define OP_VDM_R						0x60
#define OP_VDM_W						0x62
#define BITS_PER_WORD					8

#define SLOW_BAUD_RATE					12500000

#define DRDY_IRQ_ENABLE					1
#define DRDY_IRQ_DISABLE				0

#define ET5XX_INT_DETECTION_PERIOD			10
#define ET5XX_DETECTION_THRESHOLD			10

#define FP_REGISTER_READ				0x01
#define FP_REGISTER_WRITE				0x02
#define FP_GET_ONE_IMG					0x03
#define FP_SENSOR_RESET					0x04
#define FP_POWER_CONTROL				0x05
#define FP_SET_SPI_CLOCK				0x06
#define FP_RESET_SET					0x07
#define FP_REGISTER_BREAD				0x20
#define FP_REGISTER_BWRITE				0x21
#define FP_REGISTER_MREAD				0x22
#define FP_REGISTER_MWRITE				0x23
#define FP_REGISTER_BREAD_BACKWARD		0x24
#define FP_REGISTER_BWRITE_BACKWARD		0x25
#define FP_VDM_READ						0x30
#define FP_VDM_WRITE					0x31
#define FP_NVM_READ						0X40
#define FP_NVM_WRITE					0x41
#define FP_NVM_OFF						0x42
#define FP_NVM_WRITEEX					0x43

#ifdef ENABLE_SENSORS_FPRINT_SECURE
#define FP_DISABLE_SPI_CLOCK				0x10
#define FP_CPU_SPEEDUP					0x11
#define FP_SET_SENSOR_TYPE				0x14
/* Do not use ioctl number 0x15 */
#define FP_SET_LOCKSCREEN				0x16
#define FP_SET_WAKE_UP_SIGNAL				0x17
#endif
#define FP_POWER_CONTROL_ET5XX			0x18
#define FP_SENSOR_ORIENT				0x19
#define FP_SPI_VALUE					0x1a
#define FP_IOCTL_RESERVED_01				0x1b
#define FP_IOCTL_RESERVED_02				0x1c



/* trigger signal initial routine */
#define INT_TRIGGER_INIT				0xa4
/* trigger signal close routine */
#define INT_TRIGGER_CLOSE				0xa5
/* read trigger status */
#define INT_TRIGGER_READ				0xa6
/* polling trigger status */
#define INT_TRIGGER_POLLING				0xa7
/* polling abort */
#define INT_TRIGGER_ABORT				0xa8
/* Sensor Registers */
#define FDATA_ET5XX_ADDR				0x00
#define FSTATUS_ET5XX_ADDR				0x01
/* Detect Define */
#define FRAME_READY_MASK				0x01

#define SHIFT_BYTE_OF_IMAGE 0
#define DIVISION_OF_IMAGE 4
#define LARGE_SPI_TRANSFER_BUFFER	64
#define MAX_NVM_LEN (32 * 2) /* NVM length in bytes (32 * 16 bits internally) */
#define NVM_WRITE_LENGTH 4096
#define DETECT_ADM 1

struct egis_ioc_transfer {
	u8 *tx_buf;
	u8 *rx_buf;

	__u32 len;
	__u32 speed_hz;

	__u16 delay_usecs;
	__u8 bits_per_word;
	__u8 cs_change;
	__u8 opcode;
	__u8 pad[3];
};

/*
 *	If platform is 32bit and kernel is 64bit
 *	We will alloc egis_ioc_transfer for 64bit and 32bit
 *	We use ioc_32(32bit) to get data from user mode.
 *	Then copy the ioc_32 to ioc(64bit).
 */
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
struct egis_ioc_transfer_32 {
	__u32 tx_buf;
	__u32 rx_buf;
	__u32 len;
	__u32 speed_hz;
	__u16 delay_usecs;
	__u8 bits_per_word;
	__u8 cs_change;
	__u8 opcode;
	__u8 pad[3];
};
#endif

#define EGIS_IOC_MAGIC			'k'
#define EGIS_MSGSIZE(N) \
	((((N)*(sizeof(struct egis_ioc_transfer))) < (1 << _IOC_SIZEBITS)) \
		? ((N)*(sizeof(struct egis_ioc_transfer))) : 0)
#define EGIS_IOC_MESSAGE(N) _IOW(EGIS_IOC_MAGIC, 0, char[EGIS_MSGSIZE(N)])

struct etspi_data {
	dev_t devt;
	spinlock_t spi_lock;
	struct spi_device *spi;
	struct list_head device_entry;

	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex buf_lock;
	unsigned int users;
	u8 *buf;/* tx buffer for sensor register read/write */
	unsigned int bufsiz; /* MAX size of tx and rx buffer */
	unsigned int drdyPin;	/* DRDY GPIO pin number */
	unsigned int sleepPin;	/* Sleep GPIO pin number */
	unsigned int ldo_pin;	/* Ldo GPIO pin number */
	const char *btp_vdd;
	struct regulator *regulator_3p3;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
	/* set cs pin in fp driver, use only Exynos8890 */
	/* for use auto cs mode with dualization fp sensor */
	unsigned int cs_gpio;
#endif
#endif
	struct pinctrl *p;
	struct pinctrl_state *pins_poweron;
	struct pinctrl_state *pins_poweroff;

	unsigned int spi_cs;	/* spi cs pin <temporary gpio setting> */

	unsigned int drdy_irq_flag;	/* irq flag */
	bool ldo_onoff;

	/* For polling interrupt */
	int int_count;
	struct timer_list timer;
	struct work_struct work_debug;
	struct workqueue_struct *wq_dbg;
	struct timer_list dbg_timer;
	int sensortype;
	u32 spi_value;
	struct device *fp_device;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	bool enabled_clk;
#ifdef FEATURE_SPI_WAKELOCK
	struct wake_lock fp_spi_lock;
#endif
#endif
	struct wake_lock fp_signal_lock;
	bool tz_mode;
	int detect_period;
	int detect_threshold;
	bool finger_on;
	const char *chipid;
	bool ldo_enabled;
	unsigned int orient;
	int reset_count;
	int interrupt_count;
};

static inline int etspi_io_burst_read_register(struct etspi_data *etspi,
		struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.rx_buf = etspi->buf,
		.len = ioc->len + 2,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		status = -ENOMEM;
		pr_err("%s error status = %d\n", __func__, status);
		goto end;
	}

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_R_C;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t) ioc->tx_buf, 1)) {
		pr_err("%s buffer copy_from_user fail\n", __func__);
		status = -EFAULT;
		goto end;
	}
	pr_debug("%s tx_buf = %p op = %x reg = %x, len = %d\n", __func__,
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status < 0) {
		status = -ENOMEM;
		pr_err("%s error status = %d\n", __func__, status);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf, etspi->buf + 2,
				ioc->len)) {
		status = -EFAULT;
		pr_err("%s buffer copy_to_user fail status\n", __func__);
		goto end;
	}
end:
	return status;
#endif
}

static inline int etspi_io_burst_read_register_backward(struct etspi_data *etspi,
		struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.rx_buf = etspi->buf,
		.len = ioc->len + 2,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		status = -ENOMEM;
		pr_err("%s error status = %d\n", __func__, status);
		goto end;
	}

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_R_C_BW;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t)ioc->tx_buf, 1)) {
		pr_err("%s buffer copy_from_user fail\n", __func__);
		status = -EFAULT;
		goto end;
	}
	pr_debug("%s tx_buf = %p op = %x reg = %x, len = %d\n", __func__,
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status < 0) {
		status = -ENOMEM;
		pr_err("%s error status = %d\n", __func__, status);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf, etspi->buf + 2,
			ioc->len)) {
		status = -EFAULT;
		pr_err("%s buffer copy_to_user fail status\n", __func__);
		goto end;
	}
end:
	return status;
#endif
}
static inline int etspi_io_burst_write_register(struct etspi_data *etspi,
		struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.len = ioc->len + 1,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		status = -ENOMEM;
		pr_err("%s error status = %d\n", __func__, status);
		goto end;
	}

	memset(etspi->buf, 0, ioc->len + 1);
	*etspi->buf = OP_REG_W_C;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t) ioc->tx_buf,
			ioc->len)) {
		pr_err("%s buffer copy_from_user fail\n", __func__);
		status = -EFAULT;
		goto end;
	}
	pr_debug("%s tx_buf = %p op = %x reg = %x, len = %d\n", __func__,
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status < 0) {
		pr_err("%s error status = %d\n", __func__, status);
		goto end;
	}
end:
	return status;
#endif
}

static inline int etspi_io_burst_write_register_backward(struct etspi_data *etspi,
		struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.len = ioc->len + 1,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		status = -ENOMEM;
		pr_err("%s error status = %d\n", __func__, status);
		goto end;
	}

	memset(etspi->buf, 0, ioc->len + 1);
	*etspi->buf = OP_REG_W_C_BW;
	if (copy_from_user(etspi->buf + 1,
		(const u8 __user *) (uintptr_t)ioc->tx_buf, ioc->len)) {
		pr_err("%s buffer copy_from_user fail\n", __func__);
		status = -EFAULT;
		goto end;
	}
	pr_debug("%s tx_buf = %p op = %x reg = %x, len = %d\n", __func__,
		ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status < 0) {
		pr_err("%s error status = %d\n", __func__, status);
		goto end;
	}
end:
	return status;
#endif
}
/* Read io register */
static inline int etspi_io_read_register(struct etspi_data *etspi, u8 *addr, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_message m;
	int read_len = 1;

	u8 tx[] = {OP_REG_R, 0x00, 0x00};
	u8 val, addrval;
	u8 rx[] = {0xFF, 0x00, 0x00};

	struct spi_transfer xfer = {
		.tx_buf = tx,
		.rx_buf = rx,
		.len = 3,
	};

	if (copy_from_user(&addrval, (const u8 __user *) (uintptr_t) addr
		, read_len)) {
		pr_err("%s buffer copy_from_user fail\n", __func__);
		status = -EFAULT;
		return status;
	}

	tx[1] = addrval;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);
	if (status < 0) {
		pr_err("%s read data error status = %d\n", __func__, status);
		return status;
	}

	val = rx[2];

	pr_debug("%s len = %d addr = %p val = %x\n", __func__,
			read_len, addr, val);

	if (copy_to_user((u8 __user *) (uintptr_t) buf, &val, read_len)) {
		pr_err("%s buffer copy_to_user fail status\n", __func__);
		status = -EFAULT;
		return status;
	}

	return status;
#endif
}
static inline int etspi_io_read_registerex(struct etspi_data *etspi, u8 *addr, u8 *buf,
		u32 len)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.rx_buf = etspi->buf,
		.len = len + 2,
	};

	if (len <= 0 || len + 2 > etspi->bufsiz) {
		status = -ENOMEM;
		pr_err("%s error status = %d", __func__, status);
		goto end;
	}

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_R;

	if (copy_from_user(etspi->buf + 1, (const u8 __user *) (uintptr_t) addr
			, 1)) {
		pr_err("%s buffer copy_from_user fail\n", __func__);
		status = -EFAULT;
		goto end;
	}

	pr_debug("%s addr = %p op = %x reg = %x len = %d tx = %p, rx = %p",
			__func__, addr, etspi->buf[0], etspi->buf[1], len,
			xfer.tx_buf, xfer.rx_buf);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);
	if (status < 0) {
		pr_err("%s read data error status = %d\n", __func__, status);
		goto end;
	}


	if (copy_to_user((u8 __user *) (uintptr_t) buf, etspi->buf + 2, len)) {
		pr_err("%s buffer copy_to_user fail status\n", __func__);
		status = -EFAULT;
		goto end;
	}
end:
	return status;
#endif
}
/* Write data to register */
static inline int etspi_io_write_register(struct etspi_data *etspi, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	int write_len = 2;
	struct spi_message m;

	u8 tx[] = {OP_REG_W, 0x00, 0x00};
	u8 val[3];

	struct spi_transfer xfer = {
		.tx_buf = tx,
		.len = 3,
	};

	if (copy_from_user(val, (const u8 __user *) (uintptr_t) buf,
			write_len)) {
		pr_err("%s buffer copy_from_user fail\n", __func__);
		status = -EFAULT;
		return status;
	}

	pr_debug("%s write_len = %d addr = %x data = %x\n", __func__,
			write_len, val[0], val[1]);

	tx[1] = val[0];
	tx[2] = val[1];

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);
	if (status < 0) {
		pr_err("%s read data error status = %d\n", __func__, status);
		return status;
	}

	return status;
#endif
}
static inline int etspi_read_register(struct etspi_data *etspi, u8 addr, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_message m;

	static u8 read_value[] = {OP_REG_R, 0x00, 0x00};
	static u8 result[] = {0xFF, 0xFF, 0xFF};

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf	= result,
		.len = 3,
	};

	read_value[1] = addr;
	xfer.tx_buf = read_value;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status == 0) {
		*buf = result[2];
		DEBUG_PRINT("%s address = %x result = %x %x\n"
					__func__, addr, result[1], result[2]);
	} else {
		pr_err("%s read data error status = %d\n", __func__, status);
	}

	return status;
#endif
}
inline int etspi_write_register(struct etspi_data *etspi, u8 addr, u8 buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_message m;

	u8 tx[] = {OP_REG_W, addr, buf};

	struct spi_transfer xfer = {
		.tx_buf = tx,
		.rx_buf	= NULL,
		.len = 3,
	};

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status == 0) {
		DEBUG_PRINT("%s address = %x result = %x %x\n"
					__func__, addr, result[1], result[2]);
	} else {
		pr_err("%s read data error status = %d\n", __func__, status);
	}

	return status;
#endif
}
static inline int etspi_io_nvm_read(struct etspi_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_message m;

	u8 addr/* nvm logical address */, buf[] = {OP_NVM_RE, 0x00};

	struct spi_transfer xfer = {
		.tx_buf = buf,
		.rx_buf	= NULL,
		.len = 2,
	};

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status == 0)
		DEBUG_PRINT("%s nvm enabled\n", __func__);
	else
		pr_err("%s nvm enable error status = %d\n", __func__, status);

	usleep_range(10, 50);

	if (copy_from_user(&addr, (const u8 __user *) (uintptr_t) ioc->tx_buf
		, 1)) {
		pr_err("%s buffer copy_from_user fail\n", __func__);
		status = -EFAULT;
		return status;
	}

	etspi->buf[0] = OP_NVM_ON_R;

	pr_debug("%s logical addr(%x) len(%d)\n", __func__, addr, ioc->len);
	if ((addr + ioc->len) > MAX_NVM_LEN)
		return -EINVAL;

	/* transfer to nvm physical address*/
	etspi->buf[1] = ((addr % 2) ? (addr - 1) : addr) / 2;
	/* thansfer to nvm physical length */
	xfer.len = ((ioc->len % 2) ? ioc->len + 1 :
			(addr % 2 ? ioc->len + 2 : ioc->len)) + 3;
	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}
	xfer.tx_buf = xfer.rx_buf = etspi->buf;

	pr_debug("%s nvm read addr(%d) len(%d) xfer.rx_buf(%p), etspi->buf(%p)\n",
			__func__, etspi->buf[1],
			xfer.len, xfer.rx_buf, etspi->buf);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);
	if (status < 0) {
		pr_err("%s error status = %d\n", __func__, status);
		return status;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) ioc->rx_buf, xfer.rx_buf + 3
			, ioc->len)) {
		pr_err("%s buffer copy_to_user fail status\n", __func__);
		status = -EFAULT;
		return status;
	}

	return status;
#endif
}
static inline int etspi_io_nvm_write(struct etspi_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status, i, j, len/* physical nvm length */;
	struct spi_message m;
	u8 *bufw = NULL;
	u8 buf[MAX_NVM_LEN + 1] = {OP_NVM_WE, 0x00};
	u8 addr/* nvm physical addr */;

	struct spi_transfer xfer = {
		.tx_buf = buf,
		.rx_buf	= NULL,
		.len = 2,
	};

	if (ioc->len > (MAX_NVM_LEN + 1))
		return -EINVAL;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status == 0)
		DEBUG_PRINT("%s nvm enabled\n", __func__);
	else
		pr_err("%s nvm enable error status = %d\n", __func__, status);

	usleep_range(10, 50);

	pr_debug("%s buf(%p) tx_buf(%p) len(%d)\n", __func__, buf,
			ioc->tx_buf, ioc->len);
	if (copy_from_user(buf, (const u8 __user *) (uintptr_t) ioc->tx_buf,
			ioc->len)) {
		pr_err("%s buffer copy_from_user fail\n", __func__);
		status = -EFAULT;
		return status;
	}

	if ((buf[0] + (ioc->len - 1)) > MAX_NVM_LEN)
		return -EINVAL;
	if ((buf[0] % 2) || ((ioc->len - 1) % 2)) {
		/* TODO: add non alignment handling */
		pr_err("%s can't handle address alignment issue. %d %d\n",
				__func__, buf[0], ioc->len);
		return -EINVAL;
	}

	bufw = kmalloc(NVM_WRITE_LENGTH, GFP_KERNEL);
	/*TODO: need to dynamic assign nvm length*/
	if (bufw == NULL) {
		status = -ENOMEM;
		pr_err("%s bufw kmalloc error\n", __func__);
		return status;
	}
	xfer.tx_buf = xfer.rx_buf = bufw;
	xfer.len = NVM_WRITE_LENGTH;

	len = (ioc->len - 1) / 2;
	pr_debug("%s nvm write addr(%d) len(%d) xfer.tx_buf(%p), etspi->buf(%p)\n",
			__func__, buf[0], len, xfer.tx_buf, etspi->buf);
	for (i = 0, addr = buf[0] / 2/* thansfer to nvm physical length */;
			i < len; i++) {
		bufw[0] = OP_NVM_ON_W;
		bufw[1] = addr++;
		bufw[2] = buf[i * 2 + 1];
		bufw[3] = buf[i * 2 + 2];
		memset(bufw + 4, 1, NVM_WRITE_LENGTH - 4);

		pr_debug("%s write transaction (%d): %x %x %x %x\n",
				__func__, i,
				bufw[0], bufw[1], bufw[2], bufw[3]);
		spi_message_init(&m);
		spi_message_add_tail(&xfer, &m);
		status = spi_sync(etspi->spi, &m);
		if (status < 0) {
			pr_err("%s error status = %d\n", __func__, status);
			goto end;
		}
		for (j = 0; j < NVM_WRITE_LENGTH - 4; j++) {
			if (bufw[4 + j] == 0) {
				pr_debug("%s nvm write ready(%d)\n",
						__func__, j);
				break;
			}
			if (j == NVM_WRITE_LENGTH - 5) {
				pr_err("%s nvm write fail(timeout)\n",
						__func__);
				status = -EIO;
				goto end;
			}
		}
	}
end:
	kfree(bufw);
	return status;
#endif
}
static inline int etspi_io_nvm_writeex(struct etspi_data *etspi,
		struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status, i, j, len/* physical nvm length */, wlen;
	struct spi_message m;
	u8 *bufw = NULL;
	u8 bufr[MAX_NVM_LEN + 3];
	u8 buf[MAX_NVM_LEN + 3] = {OP_NVM_WE, 0x00};
	u8 addr/* nvm physical addr */, *tmp = NULL;
	struct egis_ioc_transfer r;

	struct spi_transfer xfer = {
		.tx_buf = buf,
		.rx_buf	= NULL,
		.len = 2,
	};

	pr_debug("%s buf(%p) tx_buf(%p) len(%d)\n", __func__,
			buf, ioc->tx_buf, ioc->len);
	if (copy_from_user(buf, (const u8 __user *) (uintptr_t) ioc->tx_buf
		, ioc->len)) {
		pr_err("%s buffer copy_from_user fail\n", __func__);
		status = -EFAULT;
		return status;
	}

	if ((buf[0] + (ioc->len - 3)) > MAX_NVM_LEN)
		return -EINVAL;
	if ((buf[0] % 2) || ((ioc->len - 3) % 2)) {
		/* address non-alignment handling */
		pr_debug("%s handle address alignment issue. %d %d\n",
				__func__, buf[0], ioc->len);

		r.tx_buf = r.rx_buf = bufr;
		r.len = ioc->len;
		if (buf[0] % 2) {
			r.tx_buf[0] = buf[0] - 1;
			r.len = ioc->len % 2 ? r.len + 1 : r.len + 2;
		} else {
			if (ioc->len % 2)
				r.len++;
		}
		pr_debug("%s fixed address alignment issue. %d %d\n",
				__func__, r.tx_buf[0], r.len);
		etspi_nvm_read(etspi, &r);

		tmp = bufr;
		if (buf[0] % 2)
			tmp++;
		memcpy(tmp, buf, ioc->len);
	}

	buf[0] = OP_NVM_WE;
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status == 0)
		DEBUG_PRINT("%s nvm enabled\n", __func__);
	else
		pr_err("%s nvm enable error status = %d\n", __func__, status);

	usleep_range(10, 50);

	wlen = *(u16 *)(buf + 1);
	pr_debug("%s wlen(%d)\n", __func__, wlen);
	if (wlen > 8192)
		wlen = 8196;
	bufw = kmalloc(wlen, GFP_KERNEL);
	if (bufw == NULL) {
		status = -ENOMEM;
		pr_err("%s bufw kmalloc error\n", __func__);
		return status;
	}
	xfer.tx_buf = xfer.rx_buf = bufw;
	xfer.len = wlen;

	if ((buf[0] % 2) || ((ioc->len - 3) % 2)) {
		memcpy(buf, bufr, r.len);
		ioc->len = r.len;
	}
	len = (ioc->len - 3) / 2;
	pr_debug("%s nvm write addr(%d) len(%d) xfer.tx_buf(%p), etspi->buf(%p), wlen(%d)\n",
			 __func__, buf[0], len, xfer.tx_buf, etspi->buf, wlen);
	for (i = 0, addr = buf[0] / 2/* thansfer to nvm physical length */;
			i < len; i++) {
		bufw[0] = OP_NVM_ON_W;
		bufw[1] = addr++;
		bufw[2] = buf[i * 2 + 3];
		bufw[3] = buf[i * 2 + 4];
		memset(bufw + 4, 1, wlen - 4);

		pr_debug("%s write transaction (%d): %x %x %x %x\n",
				__func__, i,
				bufw[0], bufw[1], bufw[2], bufw[3]);
		spi_message_init(&m);
		spi_message_add_tail(&xfer, &m);
		status = spi_sync(etspi->spi, &m);
		if (status < 0) {
			pr_err("%s error status = %d\n", __func__, status);
			goto end;
		}
		for (j = 0; j < wlen - 4; j++) {
			if (bufw[4 + j] == 0) {
				pr_debug("%s nvm write ready(%d)\n",
						__func__, j);
				break;
			}
			if (j == wlen - 5) {
				pr_err("%s nvm write fail(timeout)\n",
						__func__);
				status = -EIO;
				goto end;
			}
		}
	}
end:
	kfree(bufw);
	return status;
#endif
}
static inline int etspi_io_nvm_off(struct etspi_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_message m;

	u8 buf[] = {OP_NVM_OFF, 0x00};

	struct spi_transfer xfer = {
		.tx_buf = buf,
		.rx_buf	= NULL,
		.len = 2,
	};

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status == 0)
		DEBUG_PRINT("%s nvm disabled\n", __func__);
	else
		pr_err("%s nvm disable error status = %d\n", __func__, status);

	return status;
#endif
}
static inline int etspi_io_vdm_read(struct etspi_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = ioc->len + 1,
	};

	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}

	buf = kzalloc(xfer.len, GFP_KERNEL);

	if (buf == NULL)
		return -ENOMEM;

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_VDM_R;

	pr_debug("%s len = %d, xfer.len = %d, buf = %p, rx_buf = %p\n",
			__func__, ioc->len, xfer.len, buf, ioc->rx_buf);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);
	if (status < 0) {
		pr_err("%s read data error status = %d\n", __func__, status);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) ioc->rx_buf, buf + 1,
			ioc->len)) {
		pr_err("%s buffer copy_to_user fail status\n", __func__);
		status = -EFAULT;
	}
end:
	kfree(buf);
	return status;
#endif
}
static inline int etspi_io_vdm_write(struct etspi_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = ioc->len + 1,
	};

	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}

	buf = kzalloc(xfer.len, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user((u8 __user *) (uintptr_t) buf + 1, ioc->tx_buf,
			ioc->len)) {
		pr_err("buffer copy_from_user fail status\n");
		status = -EFAULT;
		goto end;
	}

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_VDM_W;

	pr_debug("%s len = %d, xfer.len = %d, buf = %p, tx_buf = %p\n",
			 __func__, ioc->len, xfer.len, buf, ioc->tx_buf);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status < 0)
		pr_err("%s read data error status = %d\n", __func__, status);
end:
	kfree(buf);
	return status;
#endif
}
static inline int etspi_io_get_frame(struct etspi_data *etspi, u8 *fr, u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = size + 1,
	};

	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}

	buf = kzalloc(xfer.len, GFP_KERNEL);

	if (buf == NULL)
		return -ENOMEM;

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_IMG_R;

	pr_debug("%s size = %d, xfer.len = %d, buf = %p, fr = %p\n", __func__,
		size, xfer.len, buf, fr);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);
	if (status < 0) {
		pr_err("%s read data error status = %d\n", __func__, status);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) fr, buf + 1, size)) {
		pr_err("%s buffer copy_to_user fail status\n", __func__);
		status = -EFAULT;
	}
end:
	kfree(buf);
	return status;
#endif
}

static inline int etspi_nvm_read(struct etspi_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_message m;

	u8 addr/* nvm logical address */, buf[] = {OP_NVM_RE, 0x00};

	struct spi_transfer xfer = {
		.tx_buf = buf,
		.rx_buf	= NULL,
		.len = 2,
	};

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);

	if (status == 0)
		DEBUG_PRINT("%s nvm enabled\n", __func__);
	else
		pr_err("%s nvm enable error status = %d\n", __func__, status);

	usleep_range(10, 50);

	addr = ioc->tx_buf[0];

	etspi->buf[0] = OP_NVM_ON_R;

	pr_debug("%s logical addr(%x) len(%d)\n", __func__, addr, ioc->len);
	if ((addr + ioc->len) > MAX_NVM_LEN)
		return -EINVAL;

	/* transfer to nvm physical address*/
	etspi->buf[1] = ((addr % 2) ? (addr - 1) : addr) / 2;
	/* thansfer to nvm physical length */
	xfer.len = ((ioc->len % 2) ? ioc->len + 1 :
			(addr % 2 ? ioc->len + 2 : ioc->len)) + 3;
	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}
	xfer.tx_buf = xfer.rx_buf = etspi->buf;

	pr_debug("%s nvm read addr(%d) len(%d) xfer.rx_buf(%p), etspi->buf(%p)\n",
			__func__, etspi->buf[1],
			xfer.len, xfer.rx_buf, etspi->buf);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	status = spi_sync(etspi->spi, &m);
	if (status < 0) {
		pr_err("%s error status = %d\n", __func__, status);
		return status;
	}

	if (memcpy((u8 __user *) (uintptr_t) ioc->rx_buf, xfer.rx_buf + 3,
			ioc->len)) {
		pr_err("%s buffer copy_to_user fail status\n", __func__);
		status = -EFAULT;
		return status;
	}

	return status;
#endif
}
/* et5xx-spi */

static DECLARE_BITMAP(minors, N_SPI_MINORS);

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

#ifdef ENABLE_SENSORS_FPRINT_SECURE
int fpsensor_goto_suspend =0;
#endif

static int gpio_irq;
static struct etspi_data *g_data;
static DECLARE_WAIT_QUEUE_HEAD(interrupt_waitq);
static unsigned int bufsiz = 1024;
module_param(bufsiz, uint, 0444);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

#if defined(ENABLE_SENSORS_FPRINT_SECURE)
inline int fps_resume_set(void) {
	int ret =0;

	if (fpsensor_goto_suspend) {
		fpsensor_goto_suspend = 0;
#if !defined(CONFIG_TZDEV)
		ret = exynos_smc(MC_FC_FP_PM_RESUME, 0, 0, 0);
		pr_info("etspi %s : smc ret = %d\n", __func__, ret);
#endif
	}
	return ret;
}
#endif

static inline irqreturn_t etspi_fingerprint_interrupt(int irq, void *dev_id)
{
	struct etspi_data *etspi = (struct etspi_data *)dev_id;

	etspi->int_count++;
	etspi->finger_on = 1;
	disable_irq_nosync(gpio_irq);
	wake_up_interruptible(&interrupt_waitq);
	wake_lock_timeout(&etspi->fp_signal_lock, 1 * HZ);
	pr_info("%s FPS triggered.int_count(%d) On(%d)\n", __func__,
		etspi->int_count, etspi->finger_on);
	etspi->interrupt_count++;

	if (state_suspended) {
		cpu_input_boost_kick_max(200);
		devfreq_boost_kick_wake(DEVFREQ_EXYNOS_MIF);
	}

	return IRQ_HANDLED;
}

static inline int etspi_Interrupt_Init(
		struct etspi_data *etspi,
		int int_ctrl,
		int detect_period,
		int detect_threshold)
{
	int status = 0;

	etspi->finger_on = 0;
	etspi->int_count = 0;
	pr_info("%s int_ctrl = %d detect_period = %d detect_threshold = %d\n",
				__func__,
				int_ctrl,
				detect_period,
				detect_threshold);

	etspi->detect_period = detect_period;
	etspi->detect_threshold = detect_threshold;
	gpio_irq = gpio_to_irq(etspi->drdyPin);

	if (gpio_irq < 0) {
		pr_err("%s gpio_to_irq failed\n", __func__);
		status = gpio_irq;
		goto done;
	}

	if (etspi->drdy_irq_flag == DRDY_IRQ_DISABLE) {
		if (request_irq
			(gpio_irq, etspi_fingerprint_interrupt
			, int_ctrl, "etspi_irq", etspi) < 0) {
			pr_err("%s drdy request_irq failed\n", __func__);
			status = -EBUSY;
			goto done;
		} else {
			enable_irq_wake(gpio_irq);
			etspi->drdy_irq_flag = DRDY_IRQ_ENABLE;
		}
	}
done:
	return status;
}

static inline int etspi_Interrupt_Free(struct etspi_data *etspi)
{
	pr_info("%s\n", __func__);

	if (etspi != NULL) {
		if (etspi->drdy_irq_flag == DRDY_IRQ_ENABLE) {
			if (!etspi->int_count)
				disable_irq_nosync(gpio_irq);

			disable_irq_wake(gpio_irq);
			free_irq(gpio_irq, etspi);
			etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;
		}
		etspi->finger_on = 0;
		etspi->int_count = 0;
	}
	return 0;
}

static inline void etspi_Interrupt_Abort(struct etspi_data *etspi)
{
	wake_up_interruptible(&interrupt_waitq);
}

static inline unsigned int etspi_fps_interrupt_poll(
		struct file *file,
		struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct etspi_data *etspi = file->private_data;

	pr_debug("%s FPS fps_interrupt_poll, finger_on(%d), int_count(%d)\n",
		__func__, etspi->finger_on, etspi->int_count);

	if (!etspi->finger_on)
		poll_wait(file, &interrupt_waitq, wait);

	if (etspi->finger_on) {
		mask |= POLLIN | POLLRDNORM;
		etspi->finger_on = 0;
	}
	return mask;
}

/*-------------------------------------------------------------------------*/

static inline void etspi_reset(struct etspi_data *etspi)
{
	pr_info("%s\n", __func__);

	gpio_set_value(etspi->sleepPin, 0);
	usleep_range(1050, 1100);
	gpio_set_value(etspi->sleepPin, 1);
	etspi->reset_count++;
}

static inline void etspi_power_set(struct etspi_data *etspi, int status)
{
	int rc = 0;
	if (etspi->ldo_pin) {
		pr_info("%s ldo\n", __func__);
		if (status == 1) {
			if (etspi->ldo_pin) {
				gpio_set_value(etspi->ldo_pin, 1);
				etspi->ldo_enabled = 1;
			}
		} else if (status == 0) {
			if (etspi->ldo_pin) {
				gpio_set_value(etspi->ldo_pin, 0);
				etspi->ldo_enabled = 0;
			}
		} else {
			pr_err("%s can't support this value. %d\n"
				, __func__, status);
		}
	} else if (etspi->regulator_3p3 != NULL) {
		pr_info("%s regulator, status %d\n", __func__, status);
		if (status == 1) {
			if (regulator_is_enabled(etspi->regulator_3p3) == 0) {
				rc = regulator_enable(etspi->regulator_3p3);
				if (rc)
					pr_err("%s regulator enable failed, rc=%d\n"
					, __func__ , rc);
				else
					etspi->ldo_enabled = 1;
			} else
				pr_info("%s regulator is already enabled\n"
					, __func__);
		} else if (status == 0) {
			if (regulator_is_enabled(etspi->regulator_3p3)) {
				rc = regulator_disable(etspi->regulator_3p3);
				if (rc)
					pr_err("%s regulator disable failed, rc=%d\n"
						, __func__, rc);
				else
					etspi->ldo_enabled = 0;
			} else
				pr_info("%s regulator is already disabled\n"
					, __func__);
		} else {
			pr_err("%s can't support this value. %d\n"
				, __func__, status);
		}
	} else {
		pr_info("%s This HW revision does not support a power control\n"
			, __func__);
	}
}

static inline void etspi_pin_control(struct etspi_data *etspi,
	bool pin_set)
{
	int status = 0;

	etspi->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(etspi->pins_poweron)) {
			status = pinctrl_select_state(etspi->p,
				etspi->pins_poweron);
			if (status)
				pr_err("%s: can't set pin default state\n",
					__func__);
			pr_info("%s idle\n", __func__);
		}
	} else {
		if (!IS_ERR(etspi->pins_poweroff)) {
			status = pinctrl_select_state(etspi->p,
				etspi->pins_poweroff);
			if (status)
				pr_err("%s: can't set pin sleep state\n",
					__func__);
			pr_info("%s sleep\n", __func__);
		}
	}
}

static inline void etspi_power_control(struct etspi_data *etspi, int status)
{
	pr_info("%s status = %d\n", __func__, status);
	if (status == 1) {
		etspi_power_set(etspi, 1);
		etspi_pin_control(etspi, 1);
		usleep_range(1100, 1150);
		if (etspi->sleepPin)
			gpio_set_value(etspi->sleepPin, 1);
		usleep_range(12000, 12050);
	} else if (status == 0) {
#if defined(ENABLE_SENSORS_FPRINT_SECURE)
#if !defined(CONFIG_TZDEV)
		pr_info("%s: cs_set smc ret = %d\n", __func__,
			exynos_smc(MC_FC_FP_CS_SET, 0, 0, 0));
#endif
#endif
		if (etspi->sleepPin)
			gpio_set_value(etspi->sleepPin, 0);
		etspi_power_set(etspi, 0);
		etspi_pin_control(etspi, 0);
	} else {
		pr_err("%s can't support this value. %d\n", __func__, status);
	}
}

static inline  ssize_t etspi_read(struct file *filp,
						char __user *buf,
						size_t count,
						loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static inline ssize_t etspi_write(struct file *filp,
						const char __user *buf,
						size_t count,
						loff_t *f_pos)
{
/*Implement by vendor if needed*/
	return 0;
}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static inline int etspi_sec_spi_prepare(struct sec_spi_info *spi_info,
		struct spi_device *spi)
{
	struct clk *fp_spi_pclk, *fp_spi_sclk;
#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880)
	struct clk *fp_spi_dma;
	int ret = 0;
#endif

	fp_spi_pclk = clk_get(NULL, "fp-spi-pclk");
	if (IS_ERR(fp_spi_pclk)) {
		pr_err("%s Can't get fp_spi_pclk\n", __func__);
		return PTR_ERR(fp_spi_pclk);
	}

	fp_spi_sclk = clk_get(NULL, "fp-spi-sclk");
	if (IS_ERR(fp_spi_sclk)) {
		pr_err("%s Can't get fp_spi_sclk\n", __func__);
		return PTR_ERR(fp_spi_sclk);
	}
#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880)
	fp_spi_dma = clk_get(NULL, "apb_pclk");
	if (IS_ERR(fp_spi_dma)) {
		pr_err("%s Can't get apb_pclk\n", __func__);
		return PTR_ERR(fp_spi_dma);
	}
#endif
	clk_prepare_enable(fp_spi_pclk);
	clk_prepare_enable(fp_spi_sclk);
#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880)
	ret = clk_prepare_enable(fp_spi_dma);
	if (ret) {
		pr_err("%s clk_finger clk_prepare_enable failed %d\n",
			__func__, ret);
		return ret;
	}
#endif
#if defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS9610)
/* There is a quarter-multiplier before the SPI */
	clk_set_rate(fp_spi_sclk, spi_info->speed * 4);
#else
	clk_set_rate(fp_spi_sclk, spi_info->speed * 2);
#endif

	clk_put(fp_spi_pclk);
	clk_put(fp_spi_sclk);
#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880)
	clk_put(fp_spi_dma);
#endif
	return 0;
}

static inline int etspi_sec_spi_unprepare(struct sec_spi_info *spi_info,
		struct spi_device *spi)
{
	struct clk *fp_spi_pclk, *fp_spi_sclk;
#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880)
	struct clk *fp_spi_dma;
#endif

	fp_spi_pclk = clk_get(NULL, "fp-spi-pclk");
	if (IS_ERR(fp_spi_pclk)) {
		pr_err("%s Can't get fp_spi_pclk\n", __func__);
		return PTR_ERR(fp_spi_pclk);
	}

	fp_spi_sclk = clk_get(NULL, "fp-spi-sclk");
	if (IS_ERR(fp_spi_sclk)) {
		pr_err("%s Can't get fp_spi_sclk\n", __func__);
		return PTR_ERR(fp_spi_sclk);
	}
#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880)
	fp_spi_dma = clk_get(NULL, "apb_pclk");
	if (IS_ERR(fp_spi_dma)) {
		pr_err("%s Can't get apb_pclk\n", __func__);
		return PTR_ERR(fp_spi_dma);
	}
#endif
	clk_disable_unprepare(fp_spi_pclk);
	clk_disable_unprepare(fp_spi_sclk);
#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880)
	clk_disable_unprepare(fp_spi_dma);
#endif
	clk_put(fp_spi_pclk);
	clk_put(fp_spi_sclk);
#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880)
	clk_put(fp_spi_dma);
#endif

	return 0;
}

#if !defined(CONFIG_SOC_EXYNOS8890) && !defined(CONFIG_SOC_EXYNOS7570) \
	&& !defined(CONFIG_SOC_EXYNOS7870) && !defined(CONFIG_SOC_EXYNOS7880) \
	&& !defined(CONFIG_SOC_EXYNOS7885) && !defined(CONFIG_SOC_EXYNOS9810) \
	&& !defined(CONFIG_SOC_EXYNOS9610)
static struct amba_device *adev_dma;
static inline int etspi_sec_dma_prepare(struct sec_spi_info *spi_info)
{
	struct device_node *np;

	for_each_compatible_node(np, NULL, "arm,pl330") {
		if (!of_device_is_available(np))
			continue;

		if (!of_dma_secure_mode(np))
			continue;

		adev_dma = of_find_amba_device_by_node(np);
		pr_info("[%s]device_name:%s\n",
			__func__, dev_name(&adev_dma->dev));
		break;
	}

	if (adev_dma == NULL)
		return -1;

	pm_runtime_get_sync(&adev_dma->dev);

	return 0;
}

static inline int etspi_sec_dma_unprepare(void)
{
	if (adev_dma == NULL)
		return -1;

	pm_runtime_put(&adev_dma->dev);

	return 0;
}
#endif
#endif

static inline long etspi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, retval = 0;
	struct etspi_data *etspi;
	struct spi_device *spi;
	u32 tmp;
	struct egis_ioc_transfer *ioc = NULL;
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
	struct egis_ioc_transfer_32 *ioc_32 = NULL;
	u64 tx_buffer_64, rx_buffer_64;
#endif
	u8 *buf, *address, *result, *fr;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	struct sec_spi_info *spi_info = NULL;
#endif
	/* Check type and command number */
	if (_IOC_TYPE(cmd) != EGIS_IOC_MAGIC) {
		pr_err("%s _IOC_TYPE(cmd) != EGIS_IOC_MAGIC", __func__);
		return -ENOTTY;
	}

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
						(void __user *)arg,
						_IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
						(void __user *)arg,
						_IOC_SIZE(cmd));
	if (err) {
		pr_err("%s err", __func__);
		return -EFAULT;
	}

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	etspi = filp->private_data;
	spin_lock_irq(&etspi->spi_lock);
	spi = spi_dev_get(etspi->spi);
	spin_unlock_irq(&etspi->spi_lock);

	if (spi == NULL) {
		pr_err("%s spi == NULL", __func__);
		return -ESHUTDOWN;
	}

	mutex_lock(&etspi->buf_lock);

	/* segmented and/or full-duplex I/O request */
	if ((_IOC_NR(cmd)) != (_IOC_NR(EGIS_IOC_MESSAGE(0))) \
					|| (_IOC_DIR(cmd)) != _IOC_WRITE) {
		retval = -ENOTTY;
		goto out;
	}

	/*
	 *	If platform is 32bit and kernel is 64bit
	 *	We will alloc egis_ioc_transfer for 64bit and 32bit
	 *	We use ioc_32(32bit) to get data from user mode.
	 *	Then copy the ioc_32 to ioc(64bit).
	 */
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
	tmp = _IOC_SIZE(cmd);
	if ((tmp == 0) || (tmp % sizeof(struct egis_ioc_transfer_32)) != 0) {
		pr_err("%s ioc_32 size error\n", __func__);
		retval = -EINVAL;
		goto out;
	}
	ioc_32 = kmalloc(tmp, GFP_KERNEL);
	if (ioc_32 == NULL) {
		retval = -ENOMEM;
		pr_err("%s ioc_32 kmalloc error\n", __func__);
		goto out;
	}
	if (__copy_from_user(ioc_32, (void __user *)arg, tmp)) {
		retval = -EFAULT;
		pr_err("%s ioc_32 copy_from_user error\n", __func__);
		goto out;
	}
	ioc = kmalloc(sizeof(struct egis_ioc_transfer), GFP_KERNEL);
	if (ioc == NULL) {
		retval = -ENOMEM;
		pr_err("%s ioc kmalloc error\n", __func__);
		goto out;
	}
	tx_buffer_64 = (u64)ioc_32->tx_buf;
	rx_buffer_64 = (u64)ioc_32->rx_buf;
	ioc->tx_buf = (u8 *)tx_buffer_64;
	ioc->rx_buf = (u8 *)rx_buffer_64;
	ioc->len = ioc_32->len;
	ioc->speed_hz = ioc_32->speed_hz;
	ioc->delay_usecs = ioc_32->delay_usecs;
	ioc->bits_per_word = ioc_32->bits_per_word;
	ioc->cs_change = ioc_32->cs_change;
	ioc->opcode = ioc_32->opcode;
	memcpy(ioc->pad, ioc_32->pad, 3);
	kfree(ioc_32);
#else
	tmp = _IOC_SIZE(cmd);
	if ((tmp == 0) || (tmp % sizeof(struct egis_ioc_transfer)) != 0) {
		pr_err("%s ioc size error\n", __func__);
		retval = -EINVAL;
		goto out;
	}
	/* copy into scratch area */
	ioc = kmalloc(tmp, GFP_KERNEL);
	if (!ioc) {
		retval = -ENOMEM;
		goto out;
	}
	if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
		pr_err("%s __copy_from_user error\n", __func__);
		retval = -EFAULT;
		goto out;
	}
#endif

	switch (ioc->opcode) {
	/*
	 * Read register
	 * tx_buf include register address will be read
	 */
	case FP_REGISTER_READ:
		address = ioc->tx_buf;
		result = ioc->rx_buf;
		pr_debug("etspi FP_REGISTER_READ\n");

		retval = etspi_io_read_register(etspi, address, result);
		if (retval < 0)	{
			pr_err("%s FP_REGISTER_READ error retval = %d\n"
			, __func__, retval);
		}
		break;

	/*
	 * Write data to register
	 * tx_buf includes address and value will be wrote
	 */
	case FP_REGISTER_WRITE:
		buf = ioc->tx_buf;
		pr_debug("%s FP_REGISTER_WRITE\n", __func__);

		retval = etspi_io_write_register(etspi, buf);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_WRITE error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_REGISTER_MREAD:
		address = ioc->tx_buf;
		result = ioc->rx_buf;
		pr_debug("%s FP_REGISTER_MREAD\n", __func__);
		retval = etspi_io_read_registerex(etspi, address, result,
				ioc->len);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_MREAD error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_REGISTER_BREAD:
		pr_debug("%s FP_REGISTER_BREAD\n", __func__);
		retval = etspi_io_burst_read_register(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_BREAD error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_REGISTER_BWRITE:
		pr_debug("%s FP_REGISTER_BWRITE\n", __func__);
		retval = etspi_io_burst_write_register(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_BWRITE error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_REGISTER_BREAD_BACKWARD:
		pr_debug("%s FP_REGISTER_BREAD_BACKWARD\n", __func__);
		retval = etspi_io_burst_read_register_backward(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_BREAD_BACKWARD error retval = %d\n"
				, __func__, retval);
		}
		break;
	case FP_REGISTER_BWRITE_BACKWARD:
		pr_debug("%s FP_REGISTER_BWRITE_BACKWARD\n", __func__);
		retval = etspi_io_burst_write_register_backward(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_BWRITE_BACKWARD error retval = %d\n"
				, __func__, retval);
		}
		break;
	case FP_NVM_READ:
		pr_debug("%s FP_NVM_READ, (%d)\n", __func__, spi->max_speed_hz);
		retval = etspi_io_nvm_read(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_NVM_READ error retval = %d\n"
			, __func__, retval);
		}
		retval = etspi_io_nvm_off(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_NVM_OFF error retval = %d\n"
			, __func__, retval);
		} else {
			pr_debug("%s FP_NVM_OFF\n", __func__);
		}
		break;
	case FP_NVM_WRITE:
		pr_debug("%s FP_NVM_WRITE, (%d)\n", __func__,
				spi->max_speed_hz);
		retval = etspi_io_nvm_write(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_NVM_WRITE error retval = %d\n"
			, __func__, retval);
		}
		retval = etspi_io_nvm_off(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_NVM_OFF error retval = %d\n"
			, __func__, retval);
		} else {
			pr_debug("%s FP_NVM_OFF\n", __func__);
		}
		break;
	case FP_NVM_WRITEEX:
		pr_debug("%s FP_NVM_WRITEEX, (%d)\n", __func__,
				spi->max_speed_hz);
		retval = etspi_io_nvm_writeex(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_NVM_WRITEEX error retval = %d\n"
			, __func__, retval);
		}
		retval = etspi_io_nvm_off(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_NVM_OFF error retval = %d\n"
			, __func__, retval);
		} else {
			pr_debug("%s FP_NVM_OFF\n", __func__);
		}
		break;
	case FP_NVM_OFF:
		pr_debug("%s FP_NVM_OFF\n", __func__);
		retval = etspi_io_nvm_off(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_NVM_OFF error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_VDM_READ:
		pr_debug("%s FP_VDM_READ\n", __func__);
		retval = etspi_io_vdm_read(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_VDM_READ error retval = %d\n"
			, __func__, retval);
		} else {
			pr_debug("%s FP_VDM_READ finished.\n", __func__);
		}
		break;
	case FP_VDM_WRITE:
		pr_debug("%s FP_VDM_WRITE\n", __func__);
		retval = etspi_io_vdm_write(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_VDM_WRITE error retval = %d\n"
			, __func__, retval);
		} else {
			pr_debug("%s FP_VDM_WRTIE finished.\n", __func__);
		}
		break;
	/*
	 * Get one frame data from sensor
	 */
	case FP_GET_ONE_IMG:
		fr = ioc->rx_buf;
		pr_debug("%s FP_GET_ONE_IMG\n", __func__);

		retval = etspi_io_get_frame(etspi, fr, ioc->len);
		if (retval < 0) {
			pr_err("%s FP_GET_ONE_IMG error retval = %d\n"
			, __func__, retval);
		}
		break;

	case FP_SENSOR_RESET:
		pr_info("%s FP_SENSOR_RESET\n", __func__);
		etspi_reset(etspi);
		break;

	case FP_RESET_SET:
		break;


	case FP_POWER_CONTROL:
	case FP_POWER_CONTROL_ET5XX:
		pr_info("%s FP_POWER_CONTROL, status = %d\n", __func__,
				ioc->len);
		etspi_power_control(etspi, ioc->len);
		break;

	case FP_SET_SPI_CLOCK:
		pr_info("%s FP_SET_SPI_CLOCK, clock = %d\n", __func__,
				ioc->speed_hz);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		if (etspi->enabled_clk) {
			if (spi->max_speed_hz == ioc->speed_hz) {
				pr_info("%s already enabled same clock.\n",
					__func__);
				break;
			}
			pr_info("%s already enabled. DISABLE_SPI_CLOCK\n",
				__func__);
			retval = etspi_sec_spi_unprepare(spi_info, spi);
			if (retval < 0)
				pr_err("%s: couldn't disable spi clks\n",
					__func__);
#if !defined(CONFIG_SOC_EXYNOS8890) && !defined(CONFIG_SOC_EXYNOS7570) \
	&& !defined(CONFIG_SOC_EXYNOS7870) && !defined(CONFIG_SOC_EXYNOS7880) \
	&& !defined(CONFIG_SOC_EXYNOS7885) && !defined(CONFIG_SOC_EXYNOS9810) \
	&& !defined(CONFIG_SOC_EXYNOS9610)
			retval = etspi_sec_dma_unprepare();
			if (retval < 0)
				pr_err("%s: couldn't disable spi dma\n",
					__func__);
#endif
#ifdef FEATURE_SPI_WAKELOCK
			wake_unlock(&etspi->fp_spi_lock);
#endif
			etspi->enabled_clk = false;
		}
		spi->max_speed_hz = ioc->speed_hz;
		spi_info = kmalloc(sizeof(struct sec_spi_info),
			GFP_KERNEL);
		if (spi_info != NULL) {
			pr_info("%s ENABLE_SPI_CLOCK\n", __func__);

			spi_info->speed = spi->max_speed_hz;
			retval = etspi_sec_spi_prepare(spi_info, spi);
			if (retval < 0)
				pr_err("%s: Unable to enable spi clk\n",
					__func__);
#if !defined(CONFIG_SOC_EXYNOS8890) && !defined(CONFIG_SOC_EXYNOS7570) \
	&& !defined(CONFIG_SOC_EXYNOS7870) && !defined(CONFIG_SOC_EXYNOS7880) \
	&& !defined(CONFIG_SOC_EXYNOS7885) && !defined(CONFIG_SOC_EXYNOS9810) \
	&& !defined(CONFIG_SOC_EXYNOS9610)
			retval = etspi_sec_dma_prepare(spi_info);
			if (retval < 0)
				pr_err("%s: Unable to enable spi dma\n",
					__func__);
#endif
			kfree(spi_info);
#ifdef FEATURE_SPI_WAKELOCK
			wake_lock(&etspi->fp_spi_lock);
#endif
			etspi->enabled_clk = true;
		} else
			retval = -ENOMEM;
#else
		spi->max_speed_hz = ioc->speed_hz;
#endif
		break;

	/*
	 * Trigger initial routine
	 */
	case INT_TRIGGER_INIT:
		pr_debug("%s Trigger function init\n", __func__);
		retval = etspi_Interrupt_Init(
				etspi,
				(int)ioc->pad[0],
				(int)ioc->pad[1],
				(int)ioc->pad[2]);
		break;
	/* trigger */
	case INT_TRIGGER_CLOSE:
		pr_debug("%s Trigger function close\n", __func__);
		retval = etspi_Interrupt_Free(etspi);
		break;
	/* Poll Abort */
	case INT_TRIGGER_ABORT:
		pr_debug("%s Trigger function abort\n", __func__);
		etspi_Interrupt_Abort(etspi);
		break;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	case FP_DISABLE_SPI_CLOCK:
		pr_info("%s FP_DISABLE_SPI_CLOCK\n", __func__);

		if (etspi->enabled_clk) {
			pr_info("%s DISABLE_SPI_CLOCK\n", __func__);

			retval = etspi_sec_spi_unprepare(spi_info, spi);
			if (retval < 0)
				pr_err("%s: couldn't disable spi clks\n",
					__func__);
#if !defined(CONFIG_SOC_EXYNOS8890) && !defined(CONFIG_SOC_EXYNOS7570) \
	&& !defined(CONFIG_SOC_EXYNOS7870) && !defined(CONFIG_SOC_EXYNOS7880) \
	&& !defined(CONFIG_SOC_EXYNOS7885) && !defined(CONFIG_SOC_EXYNOS9810) \
	&& !defined(CONFIG_SOC_EXYNOS9610)
			retval = etspi_sec_dma_unprepare();
			if (retval < 0)
				pr_err("%s: couldn't disable spi dma\n",
					__func__);
#endif
#ifdef FEATURE_SPI_WAKELOCK
			wake_unlock(&etspi->fp_spi_lock);
#endif
			etspi->enabled_clk = false;
		}
		break;
	case FP_CPU_SPEEDUP:
		pr_info("%s FP_CPU_SPEEDUP\n", __func__);

		if (ioc->len) {
			u8 retry_cnt = 0;

			pr_info("%s FP_CPU_SPEEDUP ON:%d, retry: %d\n",
				__func__, ioc->len, retry_cnt);
#if defined(CONFIG_SECURE_OS_BOOSTER_API)
			do {
				retval = secos_booster_start(ioc->len - 1);
				retry_cnt++;
				if (retval) {
					pr_err("%s: booster start failed. (%d) retry: %d\n"
						, __func__, retval, retry_cnt);
					if (retry_cnt < 7)
						usleep_range(500, 510);
				}
			} while (retval && retry_cnt < 7);
#elif defined(CONFIG_TZDEV_BOOST)
			tz_boost_enable();
#endif
		} else {
			pr_info("%s FP_CPU_SPEEDUP OFF\n", __func__);
#if defined(CONFIG_SECURE_OS_BOOSTER_API)
			retval = secos_booster_stop();
			if (retval)
				pr_err("%s: booster stop failed. (%d)\n"
					, __func__, retval);
#elif defined(CONFIG_TZDEV_BOOST)
			tz_boost_disable();
#endif
		}
		break;
	case FP_SET_SENSOR_TYPE:
		if ((int)ioc->len >= SENSOR_OOO &&
				(int)ioc->len < SENSOR_MAXIMUM) {
			if ((int)ioc->len == SENSOR_OOO &&
					etspi->sensortype == SENSOR_FAILED) {
				pr_info("%s maintain type check from out of order :%s\n",
					__func__,
					sensor_status[g_data->sensortype + 2]);
			} else {
				etspi->sensortype = (int)ioc->len;
				pr_info("%s FP_SET_SENSOR_TYPE :%s\n",
					__func__,
					sensor_status[g_data->sensortype + 2]);
			}
		} else {
			pr_err("%s FP_SET_SENSOR_TYPE invalid value %d\n",
					__func__, (int)ioc->len);
			etspi->sensortype = SENSOR_UNKNOWN;
		}
		break;
	case FP_SET_LOCKSCREEN:
		pr_info("%s FP_SET_LOCKSCREEN\n", __func__);
		break;
	case FP_SET_WAKE_UP_SIGNAL:
		pr_info("%s FP_SET_WAKE_UP_SIGNAL\n", __func__);
		break;
#endif
	case FP_SENSOR_ORIENT:
		pr_info("%s: orient is %d", __func__, etspi->orient);

		retval = put_user(etspi->orient, (u8 __user *) (uintptr_t)ioc->rx_buf);
		if (retval != 0)
			pr_err("%s FP_SENSOR_ORIENT put_user fail: %d\n", __func__, retval);
		break;

	case FP_SPI_VALUE:
		etspi->spi_value = ioc->len;
		pr_info("%s spi_value: 0x%x\n", __func__,etspi->spi_value);
			break;
	case FP_IOCTL_RESERVED_01:
	case FP_IOCTL_RESERVED_02:
			break;
	default:
		retval = -EFAULT;
		break;

	}

out:
	if (ioc != NULL)
		kfree(ioc);

	mutex_unlock(&etspi->buf_lock);
	spi_dev_put(spi);
	if (retval < 0)
		pr_err("%s retval = %d\n", __func__, retval);
	return retval;
}

#ifdef CONFIG_COMPAT
static inline long etspi_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return etspi_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define etspi_compat_ioctl NULL
#endif
/* CONFIG_COMPAT */

static inline int etspi_open(struct inode *inode, struct file *filp)
{
	struct etspi_data *etspi;
	int	status = -ENXIO;

	pr_info("%s\n", __func__);
	mutex_lock(&device_list_lock);

	list_for_each_entry(etspi, &device_list, device_entry) {
		if (etspi->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (etspi->buf == NULL) {
			etspi->buf = kmalloc(bufsiz, GFP_KERNEL);
			if (etspi->buf == NULL) {
				dev_dbg(&etspi->spi->dev, "open/ENOMEM\n");
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			etspi->users++;
			filp->private_data = etspi;
			nonseekable_open(inode, filp);
			etspi->bufsiz = bufsiz;
		}
	} else
		pr_debug("%s nothing for minor %d\n"
			, __func__, iminor(inode));

	mutex_unlock(&device_list_lock);
	return status;
}

static inline int etspi_release(struct inode *inode, struct file *filp)
{
	struct etspi_data *etspi;

	pr_info("%s\n", __func__);
	mutex_lock(&device_list_lock);
	etspi = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	etspi->users--;
	if (etspi->users == 0) {
		int	dofree;

		kfree(etspi->buf);
		etspi->buf = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&etspi->spi_lock);
		dofree = (etspi->spi == NULL);
		spin_unlock_irq(&etspi->spi_lock);

		if (dofree)
			kfree(etspi);
	}
	mutex_unlock(&device_list_lock);

	return 0;
}

static inline int etspi_platformInit(struct etspi_data *etspi)
{
	int status = 0;

	pr_info("%s\n", __func__);
	/* gpio setting for ldo, ldo2, sleep, drdy pin */
	if (etspi != NULL) {
		etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;

		if (etspi->ldo_pin) {
			status = gpio_request(etspi->ldo_pin, "etspi_ldo_en");
			if (status < 0) {
				pr_err("%s gpio_request etspi_ldo_en failed\n",
					__func__);
				goto etspi_platformInit_ldo_failed;
			}
			gpio_direction_output(etspi->ldo_pin, 0);
			etspi->ldo_enabled = 0;
		}
		status = gpio_request(etspi->sleepPin, "etspi_sleep");
		if (status < 0) {
			pr_err("%s gpio_requset etspi_sleep failed\n",
				__func__);
			goto etspi_platformInit_sleep_failed;
		}

		gpio_direction_output(etspi->sleepPin, 0);
		if (status < 0) {
			pr_err("%s gpio_direction_output SLEEP failed\n",
					__func__);
			status = -EBUSY;
			goto etspi_platformInit_sleep_failed;
		}

		status = gpio_request(etspi->drdyPin, "etspi_drdy");
		if (status < 0) {
			pr_err("%s gpio_request etspi_drdy failed\n",
				__func__);
			goto etspi_platformInit_drdy_failed;
		}

		status = gpio_direction_input(etspi->drdyPin);
		if (status < 0) {
			pr_err("%s gpio_direction_input DRDY failed\n",
				__func__);
			goto etspi_platformInit_gpio_init_failed;
		}

		pr_info("%s sleep value =%d\n"
				"%s ldo en value =%d\n",
				__func__, gpio_get_value(etspi->sleepPin),
				__func__, gpio_get_value(etspi->ldo_pin));
	} else {
		status = -EFAULT;
	}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
#ifdef FEATURE_SPI_WAKELOCK
	wake_lock_init(&etspi->fp_spi_lock,
		WAKE_LOCK_SUSPEND, "etspi_wake_lock");
#endif
#endif
	wake_lock_init(&etspi->fp_signal_lock,
				WAKE_LOCK_SUSPEND, "etspi_sigwake_lock");

	pr_info("%s successful status=%d\n", __func__, status);
	return status;
etspi_platformInit_gpio_init_failed:
	gpio_free(etspi->drdyPin);
etspi_platformInit_drdy_failed:
	gpio_free(etspi->sleepPin);
etspi_platformInit_sleep_failed:
	gpio_free(etspi->ldo_pin);
etspi_platformInit_ldo_failed:
	pr_err("%s is failed\n", __func__);
	return status;
}

static inline void etspi_platformUninit(struct etspi_data *etspi)
{
	pr_info("%s\n", __func__);

	if (etspi != NULL) {
		disable_irq_wake(gpio_irq);
		disable_irq(gpio_irq);
		free_irq(gpio_irq, etspi);
		etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;
		if (etspi->ldo_pin)
			gpio_free(etspi->ldo_pin);
		gpio_free(etspi->sleepPin);
		gpio_free(etspi->drdyPin);
#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
		if (etspi->cs_gpio)
			gpio_free(etspi->cs_gpio);
#endif
#endif
#ifdef ENABLE_SENSORS_FPRINT_SECURE
#ifdef FEATURE_SPI_WAKELOCK
		wake_lock_destroy(&etspi->fp_spi_lock);
#endif
#endif
		wake_lock_destroy(&etspi->fp_signal_lock);
	}
}

static inline int etspi_parse_dt(struct device *dev,
	struct etspi_data *data)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int errorno = 0;
	int gpio;

#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
	gpio = of_get_named_gpio_flags(np, "etspi-csgpio",
		0, &flags);
	if (gpio < 0) {
		errorno = gpio;
		pr_err("%s: fail to get csgpio\n", __func__);
		goto dt_exit;
	} else {
		data->cs_gpio = gpio;
		pr_info("%s: cs_gpio=%d\n",
			__func__, data->cs_gpio);
	}
#endif
#endif
	gpio = of_get_named_gpio_flags(np, "etspi-sleepPin",
		0, &flags);
	if (gpio < 0) {
		errorno = gpio;
		pr_err("%s: fail to get sleepPin\n", __func__);
		goto dt_exit;
	} else {
		data->sleepPin = gpio;
		pr_info("%s: sleepPin=%d\n",
			__func__, data->sleepPin);
	}
	gpio = of_get_named_gpio_flags(np, "etspi-drdyPin",
		0, &flags);
	if (gpio < 0) {
		errorno = gpio;
		pr_err("%s: fail to get drdyPin\n", __func__);
		goto dt_exit;
	} else {
		data->drdyPin = gpio;
		pr_info("%s: drdyPin=%d\n",
			__func__, data->drdyPin);
	}
	gpio = of_get_named_gpio_flags(np, "etspi-ldoPin",
		0, &flags);
	if (gpio < 0) {
		data->ldo_pin = 0;
		pr_err("%s: fail to get ldo_pin\n", __func__);
	} else {
		data->ldo_pin = gpio;
		pr_info("%s: ldo_pin=%d\n",
			__func__, data->ldo_pin);
	}

	if (of_property_read_string(np, "etspi-regulator", &data->btp_vdd) < 0) {
		pr_info("%s: not use btp_regulator\n", __func__);
		data->btp_vdd = NULL;
	} else {
		data->regulator_3p3 = regulator_get(NULL, data->btp_vdd);
		if (IS_ERR(data->regulator_3p3) ||
				(data->regulator_3p3) == NULL) {
			pr_info("%s: not use regulator_3p3\n", __func__);
			data->regulator_3p3 = NULL;
		} else {
			pr_info("%s: btp_regulator ok\n", __func__);
		}
	}

	if (of_property_read_string_index(np, "etspi-chipid", 0,
			(const char **)&data->chipid)) {
		data->chipid = NULL;
	}
	pr_info("%s: chipid: %s\n", __func__, data->chipid);

	if (of_property_read_u32(np, "etspi-orient", &data->orient))
		data->orient = 0;
	pr_info("%s: orient: %d\n", __func__, data->orient);

	data->p = pinctrl_get_select_default(dev);
	if (IS_ERR(data->p)) {
		errorno = -EINVAL;
		pr_err("%s: failed pinctrl_get\n", __func__);
		goto dt_exit;
	}

#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
	data->pins_poweroff = pinctrl_lookup_state(data->p, "pins_poweroff");
#else
	data->pins_poweroff = pinctrl_lookup_state(data->p, "pins_poweroff_tz");
#endif
	if (IS_ERR(data->pins_poweroff)) {
		pr_err("%s : could not get pins sleep_state (%li)\n",
			__func__, PTR_ERR(data->pins_poweroff));
		goto fail_pinctrl_get;
	}

#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
	data->pins_poweron = pinctrl_lookup_state(data->p, "pins_poweron");
#else
	data->pins_poweron = pinctrl_lookup_state(data->p, "pins_poweron_tz");
#endif
	if (IS_ERR(data->pins_poweron)) {
		pr_err("%s : could not get pins idle_state (%li)\n",
			__func__, PTR_ERR(data->pins_poweron));
		goto fail_pinctrl_get;
	}

	pr_info("%s is successful\n", __func__);
	return errorno;
fail_pinctrl_get:
		pinctrl_put(data->p);
dt_exit:
	pr_err("%s is failed\n", __func__);
	return errorno;
}

static const struct file_operations etspi_fops = {
	.owner = THIS_MODULE,
	.write = etspi_write,
	.read = etspi_read,
	.unlocked_ioctl = etspi_ioctl,
	.compat_ioctl = etspi_compat_ioctl,
	.open = etspi_open,
	.release = etspi_release,
	.llseek = no_llseek,
	.poll = etspi_fps_interrupt_poll
};

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static inline int etspi_type_check(struct etspi_data *etspi)
{
	u8 buf1, buf2, buf3, buf4, buf5, buf6, buf7;

	etspi_power_control(g_data, 1);

	msleep(20);
	etspi_read_register(etspi, 0x00, &buf1);
	if (buf1 != 0xAA) {
		etspi->sensortype = SENSOR_FAILED;
		pr_info("%s sensor not ready, status = %x\n", __func__, buf1);
		etspi_power_control(g_data, 0);
		return -ENODEV;
	}

	etspi_read_register(etspi, 0xFD, &buf1);
	etspi_read_register(etspi, 0xFE, &buf2);
	etspi_read_register(etspi, 0xFF, &buf3);

	etspi_read_register(etspi, 0x20, &buf4);
	etspi_read_register(etspi, 0x21, &buf5);
	etspi_read_register(etspi, 0x23, &buf6);
	etspi_read_register(etspi, 0x24, &buf7);

	etspi_power_control(g_data, 0);

	pr_info("%s buf1-7: %x, %x, %x, %x, %x, %x, %x\n",
		__func__, buf1, buf2, buf3, buf4, buf5, buf6, buf7);

	/*
	 * type check return value
	 * ET510C : 0X00 / 0X66 / 0X00 / 0X33
	 * ET510D : 0x03 / 0x0A / 0x05
	 * ET516A : 0x00 / 0x10 / 0x05
	 * ET520  : 0x03 / 0x14 / 0x05
	 * ET520E  : 0x04 / 0x14 / 0x05
	 * ET523  : 0x00 / 0x17 / 0x05
	 */

	if ((buf1 == 0x00) && (buf2 == 0x10) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_EGIS;
		pr_info("%s sensor type is EGIS ET516A sensor\n", __func__);
	} else if ((buf1 == 0x03) && (buf2 == 0x0A) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_EGIS;
		pr_info("%s sensor type is EGIS ET510D sensor\n", __func__);
	} else if ((buf1 == 0x03) && (buf2 == 0x14) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_EGIS;
		pr_info("%s sensor type is EGIS ET520 sensor\n", __func__);
	} else if ((buf1 == 0x04) && (buf2 == 0x14) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_EGIS;
		pr_info("%s sensor type is EGIS ET520E sensor\n", __func__);
	} else if ((buf1 == 0x00) && (buf2 == 0x17) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_EGIS;
		pr_info("%s sensor type is EGIS ET523 sensor\n", __func__);
	} else {
		if ((buf4 == 0x00) && (buf5 == 0x66)
				&& (buf6 == 0x00) && (buf7 == 0x33)) {
			etspi->sensortype = SENSOR_EGIS;
			pr_info("%s sensor type is EGIS ET510C sensor\n",
					__func__);
		} else {
			etspi->sensortype = SENSOR_FAILED;
			pr_info("%s sensor type is FAILED\n", __func__);
			return -ENODEV;
		}
	}
	return 0;
}
#endif

static inline ssize_t etspi_bfs_values_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct etspi_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "\"FP_SPICLK\":\"%d\"\n",
			data->spi->max_speed_hz);
}

static inline ssize_t etspi_type_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct etspi_data *data = dev_get_drvdata(dev);
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	int retry = 0;
	int status = 0;

	do {
		status = etspi_type_check(data);
		pr_info("%s type (%u), retry (%d)\n"
			, __func__, data->sensortype, retry);
	} while (!data->sensortype && ++retry < 3);

	if (status == -ENODEV)
		pr_info("%s type check fail\n", __func__);
#endif
	return snprintf(buf, PAGE_SIZE, "%d\n", data->sensortype);
}

static inline ssize_t etspi_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static inline ssize_t etspi_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", g_data->chipid);
}

static inline ssize_t etspi_adm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", DETECT_ADM);
}

static inline ssize_t etspi_intcnt_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct etspi_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", data->interrupt_count);
}

static inline ssize_t etspi_intcnt_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct etspi_data *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "c")) {
		data->interrupt_count = 0;
		pr_info("initialization is done\n");
	}
	return size;
}

static inline ssize_t etspi_resetcnt_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct etspi_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", data->reset_count);
}

static inline ssize_t etspi_resetcnt_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct etspi_data *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "c")) {
		data->reset_count = 0;
		pr_info("initialization is done\n");
	}
	return size;
}

static DEVICE_ATTR(bfs_values, 0444, etspi_bfs_values_show, NULL);
static DEVICE_ATTR(type_check, 0444, etspi_type_check_show, NULL);
static DEVICE_ATTR(vendor, 0444, etspi_vendor_show, NULL);
static DEVICE_ATTR(name, 0444, etspi_name_show, NULL);
static DEVICE_ATTR(adm, 0444, etspi_adm_show, NULL);
static DEVICE_ATTR(intcnt, 0664, etspi_intcnt_show, etspi_intcnt_store);
static DEVICE_ATTR(resetcnt, 0664, etspi_resetcnt_show, etspi_resetcnt_store);

static struct device_attribute *fp_attrs[] = {
	&dev_attr_bfs_values,
	&dev_attr_type_check,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_adm,
	&dev_attr_intcnt,
	&dev_attr_resetcnt,
	NULL,
};

static inline void etspi_enable_debug_timer(void)
{

}

static inline void etspi_disable_debug_timer(void)
{

}

static inline void etspi_timer_func(unsigned long ptr)
{

}

static inline int etspi_set_timer(struct etspi_data *etspi)
{
	return 0;
}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
static inline int etspi_set_cs_gpio(struct etspi_data *etspi,
	struct s3c64xx_spi_csinfo *cs)
{
	int status = -1;

	pr_info("%s, spi auto cs mode(%d)\n", __func__, cs->cs_mode);

	if (etspi->cs_gpio) {
		cs->line = etspi->cs_gpio;
		if (!gpio_is_valid(cs->line))
			cs->line = 0;
	} else {
		cs->line = 0;
	}

	if (cs->line != 0) {
		status = gpio_request_one(cs->line, GPIOF_OUT_INIT_HIGH,
					dev_name(&etspi->spi->dev));
		if (status) {
			dev_err(&etspi->spi->dev,
				"Failed to get /CS gpio [%d]: %d\n",
				cs->line, status);
		}
	}

	return status;
}
#endif
#endif

struct class *fingerprint_class;
EXPORT_SYMBOL_GPL(fingerprint_class);

static inline void set_fingerprint_attr(struct device *dev,
	struct device_attribute *attributes[])
{
	int i;

	for (i = 0; attributes[i] != NULL; i++)
		if ((device_create_file(dev, attributes[i])) < 0)
			pr_err("%s: fail device_create_file! %d\n", __func__, i);
}

static inline int fingerprint_register(struct device *dev, void *drvdata,
	struct device_attribute *attributes[], char *name)
{
	int ret = 0;

	if (!fingerprint_class) {
		fingerprint_class = class_create(THIS_MODULE, "fingerprint");
		if (IS_ERR(fingerprint_class))
			return PTR_ERR(fingerprint_class);
	}

	dev = device_create(fingerprint_class, NULL, 0, drvdata, "%s", name);

	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		pr_err("%s: device_create failed! %d\n", __func__, ret);
		return ret;
	}

	set_fingerprint_attr(dev, attributes);

	return 0;
}

static inline void fingerprint_unregister(struct device *dev,
	struct device_attribute *attributes[])
{
	int i;

	for (i = 0; attributes[i] != NULL; i++)
		device_remove_file(dev, attributes[i]);
}
/*-------------------------------------------------------------------------*/

static struct class *etspi_class;

/*-------------------------------------------------------------------------*/

static inline int etspi_probe(struct spi_device *spi)
{
	struct etspi_data *etspi;
	int status;
	unsigned long minor;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	int retry = 0;
#ifdef CONFIG_SOC_EXYNOS8890
	struct s3c64xx_spi_csinfo *cs;
#endif
#endif

	pr_info("%s\n", __func__);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	fpsensor_goto_suspend = 0;
#endif
	/* Allocate driver data */
	etspi = kzalloc(sizeof(*etspi), GFP_KERNEL);
	if (!etspi)
		return -ENOMEM;

	/* device tree call */
	if (spi->dev.of_node) {
		status = etspi_parse_dt(&spi->dev, etspi);
		if (status) {
			pr_err("%s - Failed to parse DT\n", __func__);
			goto etspi_probe_parse_dt_failed;
		}
	}

	/* Initialize the driver data */
	etspi->spi = spi;
	g_data = etspi;

	spin_lock_init(&etspi->spi_lock);
	mutex_init(&etspi->buf_lock);
	mutex_init(&device_list_lock);

	INIT_LIST_HEAD(&etspi->device_entry);

	/* platform init */
	status = etspi_platformInit(etspi);
	if (status != 0) {
		pr_err("%s platforminit failed\n", __func__);
		goto etspi_probe_platformInit_failed;
	}

	spi->bits_per_word = 8;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;
	spi->chip_select = 0;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
	/* set cs pin in fp driver, use only Exynos8890 */
	/* for use auto cs mode with dualization fp sensor */
	cs = spi->controller_data;

	if (cs->cs_mode == 1)
		status = etspi_set_cs_gpio(etspi, cs);
	else
		pr_info("%s, spi manual mode(%d)\n", __func__, cs->cs_mode);
#endif

	status = spi_setup(spi);
	if (status != 0) {
		pr_err("%s spi_setup() is failed. status : %d\n",
			__func__, status);
		return status;
	}
#endif
	etspi->spi_value = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	etspi->sensortype = SENSOR_UNKNOWN;
#else
	/* sensor hw type check */
	do {
		status = etspi_type_check(etspi);
		pr_info("%s type (%u), retry (%d)\n"
			, __func__, etspi->sensortype, retry);
	} while (!etspi->sensortype && ++retry < 3);

	if (status == -ENODEV)
		pr_info("%s type check fail\n", __func__);
#endif

#if defined(DISABLED_GPIO_PROTECTION)
	etspi_pin_control(etspi, 0);
#endif

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	etspi->tz_mode = true;
#endif
	etspi->reset_count = 0;
	etspi->interrupt_count = 0;
	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		etspi->devt = MKDEV(ET5XX_MAJOR, (unsigned int)minor);
		dev = device_create(etspi_class, &spi->dev,
				etspi->devt, etspi, "esfp0");
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		dev_dbg(&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&etspi->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	if (status == 0)
		spi_set_drvdata(spi, etspi);
	else
		goto etspi_create_failed;

	status = fingerprint_register(etspi->fp_device,
		etspi, fp_attrs, "fingerprint");
	if (status) {
		pr_err("%s sysfs register failed\n", __func__);
		goto etspi_register_failed;
	}

	status = etspi_set_timer(etspi);
	if (status)
		goto etspi_sysfs_failed;
	etspi_enable_debug_timer();
	pr_info("%s is successful\n", __func__);

	return status;

etspi_sysfs_failed:
	fingerprint_unregister(etspi->fp_device, fp_attrs);

etspi_register_failed:
	device_destroy(etspi_class, etspi->devt);
	class_destroy(etspi_class);
etspi_create_failed:
	etspi_platformUninit(etspi);
etspi_probe_platformInit_failed:
etspi_probe_parse_dt_failed:
	kfree(etspi);
	pr_err("%s is failed\n", __func__);

	return status;
}

static inline int etspi_remove(struct spi_device *spi)
{
	struct etspi_data *etspi = spi_get_drvdata(spi);

	pr_info("%s\n", __func__);

	if (etspi != NULL) {
		etspi_disable_debug_timer();
		etspi_platformUninit(etspi);

		/* make sure ops on existing fds can abort cleanly */
		spin_lock_irq(&etspi->spi_lock);
		etspi->spi = NULL;
		spi_set_drvdata(spi, NULL);
		spin_unlock_irq(&etspi->spi_lock);

		/* prevent new opens */
		mutex_lock(&device_list_lock);
		fingerprint_unregister(etspi->fp_device, fp_attrs);

		list_del(&etspi->device_entry);
		device_destroy(etspi_class, etspi->devt);
		clear_bit(MINOR(etspi->devt), minors);
		if (etspi->users == 0)
			kfree(etspi);
		mutex_unlock(&device_list_lock);
	}
	return 0;
}

static inline int etspi_pm_suspend(struct device *dev)
{
#if defined(ENABLE_SENSORS_FPRINT_SECURE)
#if !defined(CONFIG_TZDEV)
	int ret = 0;
#endif
#endif

	pr_info("%s\n", __func__);

	if (g_data != NULL) {
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		fpsensor_goto_suspend = 1; /* used by pinctrl_samsung.c */
#endif
		etspi_disable_debug_timer();

		if (!g_data->ldo_enabled) {
#if defined(ENABLE_SENSORS_FPRINT_SECURE)
#if !defined(CONFIG_TZDEV)
			ret = exynos_smc(MC_FC_FP_PM_SUSPEND, 0, 0, 0);
			pr_info("%s: suspend smc ret = %d\n", __func__, ret);
#endif
#endif
		} else {
#if defined(ENABLE_SENSORS_FPRINT_SECURE)
#if !defined(CONFIG_TZDEV)
			ret = exynos_smc(MC_FC_FP_PM_SUSPEND_CS_HIGH, 0, 0, 0);
			pr_info("%s: suspend_cs_high smc ret = %d\n",
				__func__, ret);
#endif
#endif
		}
	}
	return 0;
}

static inline int etspi_pm_resume(struct device *dev)
{
	pr_info("%s\n", __func__);
	if (g_data != NULL) {
		etspi_enable_debug_timer();
#if defined(ENABLE_SENSORS_FPRINT_SECURE)
		if (fpsensor_goto_suspend) {
			fps_resume_set();
		}
#endif
	}
	return 0;
}

static const struct dev_pm_ops etspi_pm_ops = {
	.suspend = etspi_pm_suspend,
	.resume = etspi_pm_resume
};

static const struct of_device_id etspi_match_table[] = {
	{ .compatible = "etspi,et5xx",},
	{},
};

static struct spi_driver etspi_spi_driver = {
	.driver = {
		.name =	"egis_fingerprint",
		.owner = THIS_MODULE,
		.pm = &etspi_pm_ops,
		.of_match_table = etspi_match_table
	},
	.probe = etspi_probe,
	.remove = etspi_remove,
};

/*-------------------------------------------------------------------------*/

static inline int __init etspi_init(void)
{
	int status;

	pr_info("%s\n", __func__);

#if defined(CONFIG_SENSORS_FINGERPRINT_DUALIZATION) \
		&& defined(CONFIG_SENSORS_VFS7XXX)
	/* vendor check */
	pr_info("%s FP_CHECK value (%d)\n", __func__, FP_CHECK);
	if (FP_CHECK) {
		pr_err("%s It is not egis sensor\n", __func__);
		return -ENODEV;
	}
#endif
	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(ET5XX_MAJOR, "egis_fingerprint", &etspi_fops);
	if (status < 0) {
		pr_err("%s register_chrdev error.status:%d\n", __func__,
				status);
		return status;
	}

	etspi_class = class_create(THIS_MODULE, "egis_fingerprint");
	if (IS_ERR(etspi_class)) {
		pr_err("%s class_create error.\n", __func__);
		unregister_chrdev(ET5XX_MAJOR, etspi_spi_driver.driver.name);
		return PTR_ERR(etspi_class);
	}

	status = spi_register_driver(&etspi_spi_driver);
	if (status < 0) {
		pr_err("%s spi_register_driver error.\n", __func__);
		class_destroy(etspi_class);
		unregister_chrdev(ET5XX_MAJOR, etspi_spi_driver.driver.name);
		return status;
	}

	pr_info("%s is successful\n", __func__);

	return status;
}

static inline void __exit etspi_exit(void)
{
	pr_info("%s\n", __func__);

	spi_unregister_driver(&etspi_spi_driver);
	class_destroy(etspi_class);
	unregister_chrdev(ET5XX_MAJOR, etspi_spi_driver.driver.name);
}

module_init(etspi_init);
module_exit(etspi_exit);

/* et5xx-spi */



#endif
