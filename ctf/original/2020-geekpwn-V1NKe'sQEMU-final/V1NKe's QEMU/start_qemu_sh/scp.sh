#!/bin/bash

set -eux

gcc -static exp.c -o exp
scp -P 2222 ./exp pwn@localhost:/
