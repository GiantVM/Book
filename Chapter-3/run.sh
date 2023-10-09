#!/bin/bash
clear
sudo dmesg --clear
make

sudo insmod gpt-dump.ko
dmesg --notime > gpt-dump.txt
sudo dmesg --clear

sudo rmmod gpt-dump.ko
cat gpt-dump.txt
