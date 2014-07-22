

#ifndef __RGS_USBTOUCHSCREEN_H
#define __RGS_USBTOUCHSCREEN_H

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
#include <linux/uaccess.h>

#define DRIVER_AUTHOR ""
#define DRIVER_DESC "3M 7730UC touchscreen driver"
#define DRIVER_VERSION "0.1alpha"

#ifdef CONFIG_TOUCHSCREEN_USB_3M
#define USB_PRODUCT_ID_3M				0x0596
#define USB_VENDOR_ID_7730UC			0x0001
#endif

#define COMMAND_REQUEST	_IOWR('U', 1, struct usb_packet)

#define CO_ORDINATE_REPORT_SIZE         11
#define MIN_XC                          0x0
#define MAX_XC                          0x3FFF
#define MIN_YC                          0x0
#define MAX_YC                          0x3FFF

int pipe;
static int swap_xy;
module_param(swap_xy, bool, 0644);
MODULE_PARM_DESC(swap_xy, "If set X and Y axes are swapped.");

struct usb_packet {
	unsigned char bmRequestType;
	unsigned char bRequest;
	unsigned short wValue;
	unsigned short wIndex;
	unsigned short wLength;
	char *response;
};

struct ex111_usb {
	unsigned char *data;
	dma_addr_t data_dma;
	unsigned char *buffer;
	int buf_len;
	struct urb *irq;
	struct usb_interface *interface;
	struct input_dev *input;
	char name[128];
	char phys[64];
	void *priv;
	struct work_struct reset_pipe_work;
	unsigned int x, y, touch;
};
#endif /* __RGS_USBTOUCHSCREEN_H */
