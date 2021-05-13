#!/bin/bash
clear
sudo dmesg --clear
make

sudo insmod lkm.ko
dmesg --notime > gpt-dump.txt
sudo dmesg --clear

sudo rmmod lkm.ko
cat gpt-dump.txt
