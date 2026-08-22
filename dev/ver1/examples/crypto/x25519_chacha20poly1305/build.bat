@echo off
tcc main.c -o ..\..\bin\crypto_x25519_chacha20poly1305.exe -lws2_32 -lshell32 -liphlpapi
