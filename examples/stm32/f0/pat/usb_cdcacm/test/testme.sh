#!/bin/bash
gcc -ggdb usb.c support.c tester.c -o zlp_test
./zlp_test
