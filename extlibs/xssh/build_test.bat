@echo off
setlocal

set GCC_WARN=-Wall -Wextra

if not exist .\bin (
	mkdir .\bin || exit /b 1
)

gcc -m64 xssh_wire_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_wire_test.exe || exit /b 1
gcc -m64 xssh_packet_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_packet_test.exe || exit /b 1
gcc -m64 xssh_crypto_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_crypto_test.exe || exit /b 1
gcc -m64 xssh_kex_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_kex_test.exe || exit /b 1
gcc -m64 xssh_transport_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_transport_test.exe || exit /b 1
gcc -m64 xssh_auth_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_auth_test.exe || exit /b 1
gcc -m64 xssh_channel_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_channel_test.exe || exit /b 1
gcc -m64 xssh_known_hosts_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_known_hosts_test.exe || exit /b 1
gcc -m64 xssh_mock_server_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_mock_server_test.exe || exit /b 1
gcc -m64 xssh_runtime_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_runtime_test.exe || exit /b 1
gcc -m64 xssh_interop_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_interop_test.exe || exit /b 1
gcc -m64 examples\xssh_exec.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_exec.exe || exit /b 1
gcc -m64 examples\xssh_shell.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_shell.exe || exit /b 1
gcc -m64 examples\xssh_forward.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xssh_forward.exe || exit /b 1

.\bin\xssh_wire_test.exe || exit /b 1
.\bin\xssh_packet_test.exe || exit /b 1
.\bin\xssh_crypto_test.exe || exit /b 1
.\bin\xssh_kex_test.exe || exit /b 1
.\bin\xssh_transport_test.exe || exit /b 1
.\bin\xssh_auth_test.exe || exit /b 1
.\bin\xssh_channel_test.exe || exit /b 1
.\bin\xssh_known_hosts_test.exe || exit /b 1
.\bin\xssh_mock_server_test.exe || exit /b 1
.\bin\xssh_runtime_test.exe || exit /b 1
.\bin\xssh_interop_test.exe || exit /b 1
.\bin\xssh_exec.exe --mock --command "printf ok" || exit /b 1
.\bin\xssh_shell.exe --mock --input "hello" --frames 2 || exit /b 1
echo.
.\bin\xssh_forward.exe --mock --bind-port 10022 --target-host example.com --target-port 22 --frames 1 || exit /b 1
.\bin\xssh_forward.exe --mock --self-test --bind-port 19022 --target-host example.com --target-port 22 --idle-timeout-ms 3000 || exit /b 1

echo.
echo build_test.bat: PASS
exit /b 0
