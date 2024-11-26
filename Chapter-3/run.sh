#!/bin/bash
clear
sudo dmesg --clear
make

sudo insmod gpt-dump.ko
sudo dmesg --notime | tee gpt-dump.txt
sudo dmesg --clear

sudo rmmod gpt-dump.ko
