#!/bin/bash
sudo gdb attach `ps -aux | grep qemu | awk '$11 !~ /sudo/ {print $2}'`
