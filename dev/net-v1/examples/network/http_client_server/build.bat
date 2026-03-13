@echo off
tcc main.c -o ..\..\bin\network_http_client_server.exe -lws2_32 -lshell32 -liphlpapi