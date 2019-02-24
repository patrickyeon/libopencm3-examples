#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define USB_REQ_SET_ADDRESS			5

// ==== some structs ====

/* USB Setup Data structure - Table 9-2 */
struct usb_setup_data {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} __attribute__((packed));

/* USB Standard Device Descriptor - Table 9-8 */
struct usb_device_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
} __attribute__((packed));

/** Internal collection of device information. */
// trimmed down to just what's used
struct _usbd_device {
	const struct usb_device_descriptor *desc;

	uint8_t *ctrl_buf;  /**< Internal buffer used for control transfers */
	uint16_t ctrl_buf_len;

	struct usb_control_state {
		enum {
			IDLE, STALLED,
			DATA_IN, LAST_DATA_IN, STATUS_IN,
			DATA_OUT, LAST_DATA_OUT, STATUS_OUT,
		} state;
		struct usb_setup_data req __attribute__((aligned(4)));
		uint8_t *ctrl_buf;
		uint16_t ctrl_len;
		void (*complete)(struct _usbd_device *usbd_dev,
		        struct usb_setup_data *req);
		bool may_need_zlp;
	} control_state;

	const struct _usbd_driver *driver;
};

typedef struct _usbd_device usbd_device;

// faked out, because it's only used for set_address
struct _usbd_driver {
	void (*set_address)(usbd_device *usbd_dev, uint8_t addr);
};

void usb_control_setup_read(usbd_device *usbd_dev, struct usb_setup_data *req);
void _usbd_control_in(usbd_device *usbd_dev, uint8_t ea);

void old_usb_control_setup_read(usbd_device *usbd_dev, struct usb_setup_data *req);
void old_usbd_control_in(usbd_device *usbd_dev, uint8_t ea);

// things we'll need to fake out
uint16_t usbd_ep_write_packet(usbd_device *usbd_dev, uint8_t addr,
			 const void *buf, uint16_t len);

enum usbd_request_return_codes {
	USBD_REQ_NOTSUPP	= 0,
	USBD_REQ_HANDLED	= 1,
	USBD_REQ_NEXT_CALLBACK	= 2,
};

enum usbd_request_return_codes usb_control_request_dispatch(
             usbd_device *usbd_dev, struct usb_setup_data *req);
void stall_transaction(usbd_device *usbd_dev);
void usbd_ep_nak_set(usbd_device *usbd_dev, uint8_t addr, uint8_t nak);
