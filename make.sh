#!/bin/env bash
set -xe

CFLAGS="-Wall -Wextra"
LIBS="-lcurl -lz"
DIRBUILD="./build"

gcc $CFLAGS $LIBS -o $DIRBUILD/main src/main.c
