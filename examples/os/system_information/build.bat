@echo off
tcc main.c -o ..\..\bin\os_system_information.exe -lws2_32 -lshell32 -liphlpapi -lpsapi