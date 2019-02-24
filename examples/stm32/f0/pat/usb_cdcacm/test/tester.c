#include <stdlib.h>
#include <stdio.h>

#include "./usb.h"

// A simple host-side test script to show that changes to the ZLP logic haven't
// changed the functionality of the library. Basically everything that the USB
// library does is ignored now, except the "control setup read".

// some cruft
void set_address(usbd_device *usbd_dev, uint8_t addr) {
    // no-op
    return;
}
struct _usbd_driver noop = (struct _usbd_driver){&set_address};

// set up a driver that has just the minimum that we need
void init_dev(usbd_device *dev) {
    if (dev->desc) {
        free(dev->desc);
    }
    struct usb_device_descriptor *d = calloc(1, sizeof(struct usb_device_descriptor));
    d->bMaxPacketSize0 = 4;
    dev->desc = d;

    if (dev->ctrl_buf) {
        free(dev->ctrl_buf);
    }
    dev->ctrl_buf = malloc(128);
    dev->ctrl_buf_len = 128;

    dev->control_state.state = IDLE;
    dev->control_state.req  = (struct usb_setup_data) {0, 0, 0, 0, 0};
    dev->control_state.complete = 0;
    dev->control_state.may_need_zlp = false;
    
    dev->driver = &noop;
    return;
}

void msg(usbd_device *dev, int len) {
    // create a request object
    // req.wValue is used in this test setup to set how many bytes long the
    // "device" is returning (see usb_control_request_dispatch in support.c)
    dev->control_state.req = (struct usb_setup_data){128, 6, len, 0, 12};

    // call usb_control_setup_read() (which will do the first send_chunk())
    usb_control_setup_read(dev, &(dev->control_state.req));
    // call _usbd_control_in() until control_state.state == STATUS_OUT
    while (dev->control_state.state != STATUS_OUT) {
        _usbd_control_in(dev, 0);
    }
}

void old_msg(usbd_device *dev, int len) {
    // create a request object
    dev->control_state.req = (struct usb_setup_data){128, 6, len, 0, 12};

    // call usb_control_setup_read() (which will do the first send_chunk())
    old_usb_control_setup_read(dev, &(dev->control_state.req));
    // call _usbd_control_in() until control_state.state == STATUS_OUT
    while (dev->control_state.state != STATUS_OUT) {
        old_usbd_control_in(dev, 0);
    }
}

int main(void) {
    usbd_device dev = {0};
    init_dev(&dev);
    // We're going to use wLen==12 in all our requests (hard-coded in msg()),
    // and the max packet size is 4 (hard-coded in init_dev()). So if we are
    // returning < 12 bytes, and it's a multiple of 4, we'll need a ZLP.
    // Otherwise, there should be no ZLPs.
    printf("== Running with the old code\n\n");
    for (int i = 7; i <= 9; i++) {
        printf("requesting message of len %d\n", i);
        old_msg(&dev, i);
    }

    printf("\n== Now with the new code\n\n");

    for (int i = 7; i <= 9; i++) {
        printf("requesting message of len %d\n", i);
        msg(&dev, i);
    }

    printf("\nThose should've both been the same. I hope they were.\n");
}
