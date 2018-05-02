#!/bin/sh -e
rm -rf output
meson . output/debug --werror --buildtype=debug
