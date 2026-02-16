/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sample_usbd.h>

#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_dfu.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/dfu/mcuboot.h>

#if defined(CONFIG_APP_USB_DFU_AUTH)
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

USBD_DEVICE_DEFINE(dfu_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   0x2fe3, 0xffff);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "DFU FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "DFU HS Configuration");

static const uint8_t attributes = (IS_ENABLED(CONFIG_SAMPLE_USBD_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_SAMPLE_USBD_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);
/* Full speed configuration */
USBD_CONFIGURATION_DEFINE(sample_fs_config,
			  attributes,
			  CONFIG_SAMPLE_USBD_MAX_POWER, &fs_cfg_desc);

/* High speed configuration */
USBD_CONFIGURATION_DEFINE(sample_hs_config,
			  attributes,
			  CONFIG_SAMPLE_USBD_MAX_POWER, &hs_cfg_desc);

#if defined(CONFIG_APP_USB_DFU_AUTH)
static const struct device *const cdc_acm_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
static K_SEM_DEFINE(dtr_sem, 0, 1);
static K_SEM_DEFINE(dfu_done_sem, 0, 1);

#define AUTH_RX_MSGQ_SIZE 64
K_MSGQ_DEFINE(rx_msgq, sizeof(uint8_t), AUTH_RX_MSGQ_SIZE, 1);

static void cdc_acm_irq_handler(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			uint8_t buf[64];
			int len = uart_fifo_read(dev, buf, sizeof(buf));

			for (int i = 0; i < len; i++) {
				k_msgq_put(&rx_msgq, &buf[i], K_NO_WAIT);
			}
		}
	}
}
#endif

static void switch_to_dfu_mode(struct usbd_context *const ctx);

struct dfu_ramdisk_data {
	const char *name;
	uint32_t last_block;
	uint32_t sector_size;
	uint32_t sector_count;
	union {
		uint32_t uploaded;
		uint32_t downloaded;
	};
};

static struct dfu_ramdisk_data ramdisk0_data = {
	.name = "image0",
};

static int init_dfu_ramdisk_data(struct dfu_ramdisk_data *const data)
{
	int err;

	err = disk_access_init(data->name);
	if (err) {
		return err;
	}

	err = disk_access_status(data->name);
	if (err) {
		return err;
	}

	err = disk_access_ioctl(data->name, DISK_IOCTL_GET_SECTOR_COUNT, &data->sector_count);
	if (err) {
		return err;
	}

	err = disk_access_ioctl(data->name, DISK_IOCTL_GET_SECTOR_SIZE, &data->sector_size);
	if (err) {
		return err;
	}

	LOG_INF("disk %s sector count %u sector size %u",
		data->name, data->sector_count, data->sector_size);

	return err;
}

static int ramdisk_read(void *const priv, const uint32_t block, const uint16_t size,
			uint8_t buf[static CONFIG_USBD_DFU_TRANSFER_SIZE])
{
	struct dfu_ramdisk_data *const data = priv;
	int err;

	if (size == 0) {
		/* There is nothing to upload */
		return 0;
	}

	if (block == 0) {
		if (init_dfu_ramdisk_data(data)) {
			LOG_ERR("Failed to init ramdisk data");
			return -EINVAL;
		}

		data->last_block = 0;
		data->uploaded = 0;
	} else {
		if (data->last_block + 1U != block) {
			return -EINVAL;
		}

	}

	if (block >= data->sector_count) {
		/* Nothing to upload */
		return 0;
	}

	err = disk_access_read(data->name, buf, block, 1);
	if (err) {
		LOG_ERR("Failed to read from RAMdisk");
		return err;
	}

	data->last_block = block;
	data->uploaded += MIN(size, data->sector_size);
	LOG_INF("block %u size %u uploaded %u", block, size, data->uploaded);

	return size;
}

static int ramdisk_write(void *const priv, const uint32_t block, const uint16_t size,
			 const uint8_t buf[static CONFIG_USBD_DFU_TRANSFER_SIZE])
{
	struct dfu_ramdisk_data *const data = priv;
	int err;

	if (block == 0) {
		if (init_dfu_ramdisk_data(data)) {
			LOG_ERR("Failed to init ramdisk data");
			return -EINVAL;
		}

		data->last_block = 0;
		data->downloaded = 0;
	} else {
		if (data->last_block + 1U != block) {
			return -EINVAL;
		}

	}

	if (size == 0) {
		/* Nothing to write */
		return 0;
	}

	err = disk_access_write(data->name, buf, block, 1);
	if (err) {
		LOG_ERR("Failed to write to RAMdisk");
		return err;
	}

	data->last_block = block;
	data->downloaded += size;
	LOG_INF("block %u size %u downloaded %u", block, size, data->downloaded);

	return 0;
}

USBD_DFU_DEFINE_IMG(ramdisk0, "ramdisk0", &ramdisk0_data, ramdisk_read, ramdisk_write, NULL);

static void msg_cb(struct usbd_context *const usbd_ctx,
		   const struct usbd_msg *const msg)
{
	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

	if (msg->type == USBD_MSG_CONFIGURATION) {
		LOG_INF("\tConfiguration value %d", msg->status);
	}

	if (usbd_can_detect_vbus(usbd_ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			if (usbd_enable(usbd_ctx)) {
				LOG_ERR("Failed to enable device support");
			}
		}

		if (msg->type == USBD_MSG_VBUS_REMOVED) {
			if (usbd_disable(usbd_ctx)) {
				LOG_ERR("Failed to disable device support");
			}
		}
	}

	if (msg->type == USBD_MSG_DFU_APP_DETACH) {
#if defined(CONFIG_APP_USB_DFU_AUTH)
		/* When auth is enabled, DFU mode transitions are controlled
		 * by the main loop after successful authentication only.
		 */
		LOG_WRN("DFU detach ignored, authentication required");
#else
		switch_to_dfu_mode(usbd_ctx);
#endif
	}

	if (msg->type == USBD_MSG_DFU_DOWNLOAD_COMPLETED) {
		if (IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT) &&
		    IS_ENABLED(CONFIG_APP_USB_DFU_USE_FLASH_BACKEND)) {
			boot_request_upgrade(false);
		}

#if defined(CONFIG_APP_USB_DFU_AUTH)
		k_sem_give(&dfu_done_sem);
#endif
	}

#if defined(CONFIG_APP_USB_DFU_AUTH)
	if (msg->type == USBD_MSG_CDC_ACM_CONTROL_LINE_STATE) {
		uint32_t dtr = 0U;

		uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_DTR, &dtr);
		if (dtr) {
			k_sem_give(&dtr_sem);
		}
	}
#endif
}

static void switch_to_dfu_mode(struct usbd_context *const ctx)
{
	int err;

	if (ctx != NULL) {
		LOG_INF("Detach USB device");
		usbd_disable(ctx);
		usbd_shutdown(ctx);
	}

	err = usbd_add_descriptor(&dfu_usbd, &sample_lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		return;
	}

	if (usbd_caps_speed(&dfu_usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&dfu_usbd, USBD_SPEED_HS, &sample_hs_config);
		if (err) {
			LOG_ERR("Failed to add High-Speed configuration");
			return;
		}

		err = usbd_register_class(&dfu_usbd, "dfu_dfu", USBD_SPEED_HS, 1);
		if (err) {
			LOG_ERR("Failed to add register classes");
			return;
		}

		usbd_device_set_code_triple(&dfu_usbd, USBD_SPEED_HS, 0, 0, 0);
	}

	err = usbd_add_configuration(&dfu_usbd, USBD_SPEED_FS, &sample_fs_config);
	if (err) {
		LOG_ERR("Failed to add Full-Speed configuration");
		return;
	}

	err = usbd_register_class(&dfu_usbd, "dfu_dfu", USBD_SPEED_FS, 1);
	if (err) {
		LOG_ERR("Failed to add register classes");
		return;
	}

	usbd_device_set_code_triple(&dfu_usbd, USBD_SPEED_FS, 0, 0, 0);

	err = usbd_init(&dfu_usbd);
	if (err) {
		LOG_ERR("Failed to initialize USB device support");
		return;
	}

	err = usbd_msg_register_cb(&dfu_usbd, msg_cb);
	if (err) {
		LOG_ERR("Failed to register message callback");
		return;
	}

	err = usbd_enable(&dfu_usbd);
	if (err) {
		LOG_ERR("Failed to enable USB device support");
	}
}

#if defined(CONFIG_APP_USB_DFU_AUTH)

#define AUTH_LINE_MAX 64
#define AUTH_DFU_TIMEOUT K_MINUTES(5)

static void auth_uart_write(const char *str)
{
	while (*str) {
		uart_poll_out(cdc_acm_dev, *str++);
	}
}

static int auth_read_line(char *buf, size_t size)
{
	size_t pos = 0;
	uint8_t c;

	while (pos < size - 1) {
		if (k_msgq_get(&rx_msgq, &c, K_FOREVER) != 0) {
			continue;
		}

		if (c == '\r' || c == '\n') {
			uart_poll_out(cdc_acm_dev, '\r');
			uart_poll_out(cdc_acm_dev, '\n');
			break;
		}

		if (c == '\b' || c == 0x7f) {
			if (pos > 0) {
				pos--;
				auth_uart_write("\b \b");
			}
			continue;
		}

		buf[pos++] = c;
		uart_poll_out(cdc_acm_dev, c);
	}

	buf[pos] = '\0';
	return pos;
}

static bool auth_check(const char *line)
{
	const char prefix[] = "auth ";
	size_t prefix_len = strlen(prefix);

	if (strncmp(line, prefix, prefix_len) != 0) {
		return false;
	}

	return strcmp(line + prefix_len, CONFIG_APP_USB_DFU_AUTH_SECRET) == 0;
}

static struct usbd_context *auth_usbd;

static int run_auth_phase(void)
{
	char line[AUTH_LINE_MAX];
	int ret;

	k_sem_reset(&dtr_sem);

	if (auth_usbd == NULL) {
		/* First call: full setup + init */
		auth_usbd = sample_usbd_init_device(msg_cb);
		if (auth_usbd == NULL) {
			LOG_ERR("Failed to initialize USB device");
			return -ENODEV;
		}
	} else {
		/* Subsequent calls: context already configured, just re-init.
		 * The message callback persists across shutdown/init cycles,
		 * so we only need to call usbd_init() here.
		 */
		ret = usbd_init(auth_usbd);
		if (ret) {
			LOG_ERR("Failed to initialize USB device");
			return ret;
		}
	}

	if (!usbd_can_detect_vbus(auth_usbd)) {
		ret = usbd_enable(auth_usbd);
		if (ret) {
			LOG_ERR("Failed to enable device support");
			return ret;
		}
	}

	LOG_INF("Waiting for DTR on CDC ACM");
	k_sem_take(&dtr_sem, K_FOREVER);
	k_msleep(100);

	k_msgq_purge(&rx_msgq);
	uart_irq_callback_set(cdc_acm_dev, cdc_acm_irq_handler);
	uart_irq_rx_enable(cdc_acm_dev);

	auth_uart_write("DFU Auth> ");

	while (true) {
		int len = auth_read_line(line, sizeof(line));

		if (len == 0) {
			auth_uart_write("DFU Auth> ");
			continue;
		}

		if (auth_check(line)) {
			auth_uart_write("OK, entering DFU mode\r\n");
			LOG_INF("Authentication successful");
			k_sleep(K_MSEC(100));
			break;
		}

		auth_uart_write("Invalid auth\r\nDFU Auth> ");
		LOG_WRN("Authentication failed");
	}

	uart_irq_rx_disable(cdc_acm_dev);
	usbd_disable(auth_usbd);
	usbd_shutdown(auth_usbd);

	return 0;
}

#endif /* CONFIG_APP_USB_DFU_AUTH */

int main(void)
{
#if defined(CONFIG_APP_USB_DFU_AUTH)
	int ret;

	LOG_INF("USB DFU sample with authentication enabled");

	while (true) {
		ret = run_auth_phase();
		if (ret) {
			return ret;
		}

		k_sem_reset(&dfu_done_sem);
		switch_to_dfu_mode(NULL);

		if (k_sem_take(&dfu_done_sem, AUTH_DFU_TIMEOUT) != 0) {
			LOG_WRN("DFU timeout, returning to auth");
		} else {
			/* Let dfu-util read the final DFU status before
			 * shutting down the USB stack.
			 */
			k_sleep(K_MSEC(100));
		}

		usbd_disable(&dfu_usbd);
		usbd_shutdown(&dfu_usbd);
	}
#else
	struct usbd_context *sample_usbd;
	int ret;

	sample_usbd = sample_usbd_init_device(msg_cb);
	if (sample_usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	if (!usbd_can_detect_vbus(sample_usbd)) {
		ret = usbd_enable(sample_usbd);
		if (ret) {
			LOG_ERR("Failed to enable device support");
			return ret;
		}
	}

	LOG_INF("USB DFU sample is initialized");

	return 0;
#endif
}
