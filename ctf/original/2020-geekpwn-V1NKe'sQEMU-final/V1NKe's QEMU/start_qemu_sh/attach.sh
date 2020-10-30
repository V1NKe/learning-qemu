#!/bin/bash

sudo gdb attach `ps -aux | grep qemu-system | awk '$11 !~ /sudo/ {print $2}'` -q
