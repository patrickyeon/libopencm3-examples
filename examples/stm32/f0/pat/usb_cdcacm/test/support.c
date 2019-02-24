#include "./usb.h"
#include <stdio.h>


uint16_t usbd_ep_write_packet(usbd_device *usbd_dev, uint8_t addr,
			 const void *buf, uint16_t len) {
    // each packet we pretend to write will be prefixed by "> " and end in a
    // newline. Beyond that, the data is simple the byte location, 1-indexed
    printf("> ");
    for (int i = 0; i < len; i++) {
        putchar('1' + i);
    }
    printf("\n");
    return len;
}

enum usbd_request_return_codes usb_control_request_dispatch(
             usbd_device *usbd_dev, struct usb_setup_data *req) {
    // haha abusing what things mean
    usbd_dev->control_state.ctrl_len = req->wValue;
    return USBD_REQ_HANDLED;
}

void stall_transaction(usbd_device *usbd_dev) {
    printf("- stalling...\n");
    return;
}

void usbd_ep_nak_set(usbd_device *usbd_dev, uint8_t addr, uint8_t nak) {
    printf("- naknaknak\n");
    return;
}
