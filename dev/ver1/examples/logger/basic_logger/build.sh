#!/bin/sh
set -e

mkdir -p ../../bin
gcc main.c -o ../../bin/logger_basic -lpthread -lm
