# Size Optimization With `libopencm3` and the STM32F0

## A Minimal USB CDCACM Example

Working from the `libopencm3-examples`, I copied the `f1/lisa-m-1/usb_cdcacm` example into a new board directory under the `f0` examples. The mcu on the F0 discovery board doesn't have a USB peripheral, so I'm just working with an F0 that does. To get the example working, all I needed to do was change the RCC and GPIO initialization calls, the reference to the usb driver in the `usbd_init` call, and the linker script. from there I've got a successful build.

To have more insight into the generated binaries, I added a target to the `Makefile` called `stats`. The four commands create an assembly listing, a memory map, a lists of functions sorted by size, and then a report of the final size.

```
stats: $(BINARY).elf
		arm-none-eabi-objdump -S cdcacm.elf >cdcacm.lst
		arm-none-eabi-objdump -t cdcacm.elf >cdcacm.map
		grep ".\+" cdcacm.map | sed 's/.hidden//' | awk '{print $$(NF - 1) " " $$NF}' | sort -r >cdcacm.topmap
		arm-none-eabi-size -x cdcacm.elf
```

My first version clocks in at 6424 bytes:

```
   text	   data	    bss	    dec	    hex	filename
 0x1934	   0x14	  0x184	   6860	   1acc	cdcacm.elf
```

Which I guess isn't horrible, but doesn't seem great? This chip has 32K of FLASH memory, so as I go over 1/5 of that space for basic startup and USB code, it seems worth trying to cut it down to me. `libopencm3-examples` already builds with `-Os`, so `gcc` has optimized this for size already as best it can.

## Link Time Optimization

Well, not quite as best it can. There is an optimization that isn't enabled by default in the libopencm3 builds: link-time optimization (LTO). The thrust of this optimization is that when the individual files are compiled, `gcc` can annotate them with information for the linker, which can then apply further optimizations, as it has more information about how the final binary uses the individual objets. What I'm going to really hope to see is some more aggressive dead code removal. The tradeoff is the final object is not as easy to trace through during debugging.

To enable LTO, I need to pass the `-flto` flag to the compiler and linker first while building `libopencm3`, and then again when building my own project. I quickly hacked in support for a user passing extra compiler flags to building the libraries in `lib/Makefile.include`:

```
USER_FLAGS ?= 

# Slightly bigger .elf files but gains the ability to decode macros
DEBUG_FLAGS ?= -ggdb3
STANDARD_FLAGS ?= -std=c99
STANDARD_FLAGS += $(USER_FLAGS)
```

And to the examples `rules.mk`

```
# allow the user to add some compiler flags
USER_FLAGS ?= 
ARCH_FLAGS += $(USER_FLAGS)
```

Now rebuilding the libraries, and then the example with `make USER_FLAGS=-flto clean stats` shows a small reduction in size:

```
   text	   data	    bss	    dec	    hex	filename
 0x16d8	    0xc	  0x184	   6248	   1868	cdcacm.elf
```

Well at least I know LTO is working and has given me an effectively free 612-byte win.

## Software Division

Looking at the top few functions in terms of compiled size, I see mostly library usb functions (not a surprise), but one line does stick out: `000001cc __divsi3`. That's 460 bytes (the size is reported in hex) for what looks like an integer division function. Flipping through the datasheet, I see that the F0 core doesn't have hardware integer division so it makes sense that a relatively long series of machine instructions to implement a division. But I don't do any division, so this must be happening in the `libopencm3` code. I may as well go see what they're doing, in case there's an easy workaround here.

Reading assembly, especially compiler-generated and optimized assembly, can be daunting, but this is a fairly lightweight investigation. Searching through the `cdcacm.lst` file for the string `div` turns up functions for the division and modulus operations, but more interestingly it also shows that there is only one place that it's called:

```
08000a7e <usb_control_setup_read>:
 8000a7e:   b570        push    {r4, r5, r6, lr}
 8000a80:   6903        ldr r3, [r0, #16]
 8000a82:   0004        movs    r4, r0
 8000a84:   6383        str r3, [r0, #56]   ; 0x38
 8000a86:   79cb        ldrb    r3, [r1, #7]
 8000a88:   798a        ldrb    r2, [r1, #6]
 8000a8a:   021b        lsls    r3, r3, #8
 8000a8c:   4313        orrs    r3, r2
 8000a8e:   8783        strh    r3, [r0, #60]   ; 0x3c
 8000a90:   000d        movs    r5, r1
 8000a92:   f7ff fea1   bl  80007d8 <usb_control_request_dispatch>
 8000a96:   2800        cmp r0, #0
 8000a98:   d022        beq.n   8000ae0 <usb_control_setup_read+0x62>
 8000a9a:   79e9        ldrb    r1, [r5, #7]
 8000a9c:   79ab        ldrb    r3, [r5, #6]
 8000a9e:   0209        lsls    r1, r1, #8
 8000aa0:   4319        orrs    r1, r3
 8000aa2:   d014        beq.n   8000ace <usb_control_setup_read+0x50>
 8000aa4:   6823        ldr r3, [r4, #0]
 8000aa6:   8fa0        ldrh    r0, [r4, #60]   ; 0x3c
 8000aa8:   79da        ldrb    r2, [r3, #7]
 8000aaa:   2300        movs    r3, #0
 8000aac:   4281        cmp r1, r0
 8000aae:   d907        bls.n   8000ac0 <usb_control_setup_read+0x42>
 8000ab0:   4298        cmp r0, r3
 8000ab2:   d005        beq.n   8000ac0 <usb_control_setup_read+0x42>
 8000ab4:   0011        movs    r1, r2
 8000ab6:   f000 fd61   bl  800157c <__aeabi_idivmod>
 8000aba:   424b        negs    r3, r1
 8000abc:   414b        adcs    r3, r1
 8000abe:   b2db        uxtb    r3, r3
 8000ac0:   0022        movs    r2, r4
 8000ac2:   3244        adds    r2, #68 ; 0x44
 8000ac4:   7013        strb    r3, [r2, #0]
 8000ac6:   0020        movs    r0, r4
 8000ac8:   f7ff ff7f   bl  80009ca <usb_control_send_chunk>
 8000acc:   e00b        b.n 8000ae6 <usb_control_setup_read+0x68>
 8000ace:   000b        movs    r3, r1
 8000ad0:   0020        movs    r0, r4
 8000ad2:   000a        movs    r2, r1
 8000ad4:   f7ff ff2d   bl  8000932 <usbd_ep_write_packet>
 8000ad8:   2304        movs    r3, #4
 8000ada:   342c        adds    r4, #44 ; 0x2c
 8000adc:   7023        strb    r3, [r4, #0]
 8000ade:   e002        b.n 8000ae6 <usb_control_setup_read+0x68>
 8000ae0:   0020        movs    r0, r4
 8000ae2:   f7ff fefa   bl  80008da <stall_transaction>
 8000ae6:   bd70        pop {r4, r5, r6, pc}
```

Looking at `usb_control_setup_read()` in `libopencm3/lib/usb/usb_control.c` there's no obvious division, but digging around a little bit I can see a modulus operation in `needs_zlp()`. I guess that function got inlined.

```
/**
 * If we're replying with _some_ data, but less than the host is expecting,
 * then we normally just do a short transfer.  But if it's short, but a
 * multiple of the endpoint max packet size, we need an explicit ZLP.
 * @param len how much data we want to transfer
 * @param wLength how much the host asked for
 * @param ep_size
 * @return
 */
static bool needs_zlp(uint16_t len, uint16_t wLength, uint8_t ep_size)
{
	if (len < wLength) {
		if (len && (len % ep_size == 0)) {
			return true;
		}
	}
	return false;
}

/* ... */

/* Handle commands and read requests. */
static void usb_control_setup_read(usbd_device *usbd_dev,
		struct usb_setup_data *req)
{
	usbd_dev->control_state.ctrl_buf = usbd_dev->ctrl_buf;
	usbd_dev->control_state.ctrl_len = req->wLength;

	if (usb_control_request_dispatch(usbd_dev, req)) {
		if (req->wLength) {
			usbd_dev->control_state.needs_zlp =
				needs_zlp(usbd_dev->control_state.ctrl_len,
					req->wLength,
					usbd_dev->desc->bMaxPacketSize0);
			/* Go to data out stage if handled. */
			usb_control_send_chunk(usbd_dev);
		} else {
			/* Go to status stage if handled. */
			usbd_ep_write_packet(usbd_dev, 0, NULL, 0);
			usbd_dev->control_state.state = STATUS_IN;
		}
	} else {
		/* Stall endpoint on failure. */
		stall_transaction(usbd_dev);
	}
}
```

Looking into `usb_control_send_chunk()` I _think_ I could rework the logic to flag the need for a ZLP if the last data packet is exactly the max packet size, but I'm not entirely sure, as I haven't put in the work to understand the whole subsystem. Before I dig in there though, I'm surprised `ep_size` doesn't seem to be a constant that's propagated by the compiler, as it is a power-of-two in my example, so the modulus operation could be replaced by a simple bitwise operation. (Division by powers of two in binary is rightwards bitshifts, and modulus is the remainder of a division. So the remainder of a division will just be the bits that would be shifted out.)

A hacky check here is to hard-code the endpoint size and see if the compiler can make it work:

```
	if (len < wLength) {
		if (len && (len % 64 == 0)) {
			return true;
		}
```

```
   text	   data	    bss	    dec	    hex	filename
 0x1518	    0xc	  0x184	   5800	   16a8	cdcacm.elf
```

Oh yeah, 448 bytes saved! Even generalizing this a little bit to handle all power-of-two sizes is a pretty good savings:

```
static bool needs_zlp(uint16_t len, uint16_t wLength, uint8_t ep_size)
{
	// XXX hack! this is only correct if ep_size is a power of two
	if (len < wLength && len && (len & (ep_size - 1)) == 0) {
        return true;
	}
	return false;
}
```

```
   text	   data	    bss	    dec	    hex	filename
 0x151c	    0xc	  0x184	   5804	   16ac	cdcacm.elf
```

Unfortunately that assumes that `ep_size` is a power of two, and while I could make an argument for adding that constraint, documenting it, and having a custom branch of `libopencm3` for my own use, that's nasty and breaks any existing code that uses the USB CDCACM library. I was really trying to force the compiler to recognize that, in my case, `ep_size` is a power of two, but haven't been able to do so. In some situations I'd be tempted to add a new call to the API, something like `usbd_init_nomod()` that documents the requirement that max packet size be a power of two and allows me to avoid any division/modulus operations at compile time, but that seems too invasive to me here.

## Following the ZLP logic

Luckily, the only references to `usbd_dev->control_state.needs_zlp` are in `usb_control.c`, so it seems tractable to understand everything that has to do with it. Furthermore, tracing through the `usbd_ep_write_packet()` logic and calls, I can see that all of the control state and remaining data length management is done in `usb_control.c` as well. So the current ZLP logic goes like so:

```
- if we're transmitting less than the expected amount, but ending with a max-size packet, we will need to add a zero-length packet
- while there's still data to be sent out:
  - if it's more than our max packet size:
    - transmit a chunk
    - subtract the length from the remaining length
  - else (it all fits inside a packet)
    - transmit a chunk
	- set the remaining length to 0
    - if that was our last chunk, and we don't need a ZLP
	  - mark the transmission as done
	- else (we do need a ZLP)
	  - mark the transmission as not done
	  - record that we don't need a ZLP (on the next pass, it'll send the ZLP and then end the transmission)
```

What I'm implementing is:

```
- if we're transmitting less than the expected amount, we _may_ need to add a ZLP
- while there's still data to be sent out:
  - if it's more than our max packet size:
    - transmit a chunk
	- subtract the length sent from the remaining length
  - else if we may need a zlp, and it's exactly our max packet size:
    - transmit a chunk
	- set the remaining length to zero
  - else (it's less than the max packet size, down to including being a needed ZLP)
    - transmit what's left
	- mark the transmission as done
```

Here's the relevent code in `usb_control.c`:
```
static void usb_control_send_chunk(usbd_device *usbd_dev)
{
	if (usbd_dev->desc->bMaxPacketSize0 <
			usbd_dev->control_state.ctrl_len) {
		/* Data stage, normal transmission */
		usbd_ep_write_packet(usbd_dev, 0,
				     usbd_dev->control_state.ctrl_buf,
				     usbd_dev->desc->bMaxPacketSize0);
		usbd_dev->control_state.state = DATA_IN;
		usbd_dev->control_state.ctrl_buf +=
			usbd_dev->desc->bMaxPacketSize0;
		usbd_dev->control_state.ctrl_len -=
			usbd_dev->desc->bMaxPacketSize0;
	} else if (usbd_dev->control_state.may_need_zlp &&
               (usbd_dev->desc->bMaxPacketSize0 ==
                usbd_dev->control_state.ctrl_len)) {
        /* Data stage, normal transmission but we will need a ZLP */
        usbd_ep_write_packet(usbd_dev, 0,
                    usbd_dev->control_state.ctrl_buf,
                    usbd_dev->control_state.ctrl_len);
        usbd_dev->control_state.state = DATA_IN;
        usbd_dev->control_state.ctrl_len = 0;
    } else {
		/* Data stage, end of transmission, possibly a ZLP */
		usbd_ep_write_packet(usbd_dev, 0,
				     usbd_dev->control_state.ctrl_buf,
				     usbd_dev->control_state.ctrl_len);

		usbd_dev->control_state.state = LAST_DATA_IN;
		usbd_dev->control_state.may_need_zlp = false;
		usbd_dev->control_state.ctrl_len = 0;
		usbd_dev->control_state.ctrl_buf = NULL;
	}
}

/* Handle commands and read requests. */
static void usb_control_setup_read(usbd_device *usbd_dev,
		struct usb_setup_data *req)
{
	usbd_dev->control_state.ctrl_buf = usbd_dev->ctrl_buf;
	usbd_dev->control_state.ctrl_len = req->wLength;

	if (usb_control_request_dispatch(usbd_dev, req)) {
		if (req->wLength) {
            if (usbd_dev->control_state.ctrl_len < req->wLength) {
                /* We're replying with _some_ data, but less than the host is
                 * expecting. If this is a multiple of the endpoint max packet
                 * size, we're going to need an explicit ZLP. */
			    usbd_dev->control_state.may_need_zlp = true;
            } else {
			    usbd_dev->control_state.may_need_zlp = false;
            }
			/* Go to data out stage if handled. */
			usb_control_send_chunk(usbd_dev);
		} else {
			/* Go to status stage if handled. */
			usbd_ep_write_packet(usbd_dev, 0, NULL, 0);
			usbd_dev->control_state.state = STATUS_IN;
		}
	} else {
		/* Stall endpoint on failure. */
		stall_transaction(usbd_dev);
	}
}

```

This builds clean and the final size is

```
   text	   data	    bss	    dec	    hex	filename
 0x1508	    0xc	  0x184	   5784	   1698	cdcacm.elf
```

Not too shabby, a whole 464 bytes less than the stock `libopencm3` when I first enabled LTO (1076 bytes total saved by the change to LTO and this). Even better, I originally enabled LTO because I thought I would need it to do some dead code removal if I could convince the compiler that `ep_size` would always be a power of two. Compiling with and without LTO shows that these two improvements are independent, so if someone has reservations about using LTO they can still get this improvement!

## Was It Worth It?

That's a fair bit of spelunking to save not-quite-half-a-KB on a relatively small selection of microcontrollers, and only in the situation where the user is using the USB CDCACM library *and* has otherwise avoided any other integer divisions. An initial glance would suggest that in this case the savings are about 1.5% of the total code space available, which is to say not a huge amount. As a personal stance, I think performance improvements are *always* worthwhile if they don't impact correctness, usability, or readability of the code, and even if the do they are *often* worthwhile so long as they don't impart correctness.

Furthermore, this may "only" be 464 bytes, but FLASH has a certain peculiarity that embedded devs will be well familiar with: it cannot be erased byte-wise, it must be erased a page at a time. For an embedded device where some of the FLASH is executable code, and the rest is reserved for application storage (eg, settings or data logging), the seperation needs to be on a page boundary. If the compiled object overflows a boundary by a single byte, the storage area has to be reduced by 1KB. 

## Testing This Code

This was all good in theory (for what it's worth, the code seemed to run fine on my board, plugged into my machine), but I wanted to be a little bit more confident in my code before letting anybody rely on it working. I'm not sure how I would set up a real-world situation to exercise the boundaries of the ZLP situation on USB control transfers, so instead I created a test stub that I can run on the host.

By simply copying in the old code and new code, I can control the size of the various buffers and packet limits, and force it to exercise the different paths. To finish it off, instead of sending packets over the wire I just print out representations of the packet and can check the command output to make sure everything is alright.

```
[pat@pat-thinkpad test]$ ./testme.sh
tester.c: In function ‘init_dev’:
tester.c:20:17: warning: passing argument 1 of ‘free’ discards ‘const’ qualifier from pointer target type [-Wdiscarded-qualifiers]
         free(dev->desc);
              ~~~^~~~~~
In file included from tester.c:1:
/usr/include/stdlib.h:563:25: note: expected ‘void *’ but argument is of type ‘const struct usb_device_descriptor *’
 extern void free (void *__ptr) __THROW;
                   ~~~~~~^~~~~
== Running with the old code

requesting message of len 7
> 1234
> 123
- naknaknak
requesting message of len 8
> 1234
> 1234
>
- naknaknak
requesting message of len 9
> 1234
> 1234
> 1
- naknaknak

== Now with the new code

requesting message of len 7
> 1234
> 123
- naknaknak
requesting message of len 8
> 1234
> 1234
>
- naknaknak
requesting message of len 9
> 1234
> 1234
> 1
- naknaknak

Those should've both been the same. I hope they were.

```

Good enough for me.
