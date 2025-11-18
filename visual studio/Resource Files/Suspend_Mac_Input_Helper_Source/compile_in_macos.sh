#!/bin/zsh
clang++ -std=c++17 -o ../Suspend_Input_Helper_Mac_Binary macos_helper.cpp \
    -framework CoreFoundation \
    -framework CoreGraphics \
    -framework ApplicationServices \
    -framework IOKit \
    -lpthread