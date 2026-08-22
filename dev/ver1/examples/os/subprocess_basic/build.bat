@echo off
tcc main.c -o ..\..\bin\subprocess_basic.exe -lws2_32 -lshell32 -liphlpapi
