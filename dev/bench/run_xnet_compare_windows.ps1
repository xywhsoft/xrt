$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

$cert = "dev/xnet2_tls_test_cert.pem"
$key = "dev/xnet2_tls_test_key.pem"
$cflags = @("-O2", "-I", ".")
$winlibs = @("-lws2_32", "-liphlpapi")
$tcpIter = if ($env:XNET_COMPARE_TCP_ITER) { [uint32]$env:XNET_COMPARE_TCP_ITER } else { [uint32]500 }
$tlsIter = if ($env:XNET_COMPARE_TLS_ITER) { [uint32]$env:XNET_COMPARE_TLS_ITER } else { [uint32]100 }
$udpPackets = if ($env:XNET_COMPARE_UDP_PACKETS) { [uint32]$env:XNET_COMPARE_UDP_PACKETS } else { [uint32]8000 }
$messageSize = if ($env:XNET_COMPARE_MESSAGE_SIZE) { [uint32]$env:XNET_COMPARE_MESSAGE_SIZE } else { [uint32]64 }
$udpPacketSize = if ($env:XNET_COMPARE_UDP_PACKET_SIZE) { [uint32]$env:XNET_COMPARE_UDP_PACKET_SIZE } else { [uint32]128 }
$pressureMessages = if ($env:XNET_COMPARE_PRESSURE_MESSAGES) { [uint32]$env:XNET_COMPARE_PRESSURE_MESSAGES } else { [uint32]512 }
$pressureMessageSize = if ($env:XNET_COMPARE_PRESSURE_MESSAGE_SIZE) { [uint32]$env:XNET_COMPARE_PRESSURE_MESSAGE_SIZE } else { [uint32]1024 }
$pressurePauseMs = if ($env:XNET_COMPARE_PRESSURE_PAUSE_MS) { [uint32]$env:XNET_COMPARE_PRESSURE_PAUSE_MS } else { [uint32]250 }
$pressureDrainTimeoutMs = if ($env:XNET_COMPARE_PRESSURE_DRAIN_TIMEOUT_MS) { [uint32]$env:XNET_COMPARE_PRESSURE_DRAIN_TIMEOUT_MS } else { [uint32]120000 }
$cpuPin = if ($env:XBENCH_PIN_CPU) { $env:XBENCH_PIN_CPU } else { "disabled" }
$policyName = if ($env:XBENCH_PIN_CPU) { "diagnostic-process-pin" } else { "baseline-unpinned" }

Write-Output "xnet compare matrix (windows)"
Write-Output "tcp: $tcpIter x ${messageSize}B"
Write-Output "tls: $tlsIter x ${messageSize}B"
Write-Output "udp: $udpPackets x ${udpPacketSize}B"
Write-Output "pressure: messages=$pressureMessages size=$pressureMessageSize pause_ms=$pressurePauseMs drain_timeout_ms=$pressureDrainTimeoutMs"
Write-Output "policy: $policyName"
Write-Output "cpu-pin: $cpuPin"

gcc dev/bench/xnet_v1/bench_echo_tcp_v1.c xrt.c @cflags -o dev/bench/xnet_v1/bench_echo_tcp_v1_cmp.exe @winlibs
gcc dev/bench/xnet_v1/bench_echo_tls_v1.c xrt.c @cflags -o dev/bench/xnet_v1/bench_echo_tls_v1_cmp.exe @winlibs
gcc dev/bench/xnet_v1/bench_udp_burst_v1.c xrt.c @cflags -o dev/bench/xnet_v1/bench_udp_burst_v1_cmp.exe @winlibs
gcc dev/bench/xnet2/bench_echo_tcp.c xrt.c @cflags -o dev/bench/xnet2/bench_echo_tcp_cmp.exe @winlibs
gcc dev/bench/xnet2/bench_echo_tls.c xrt.c @cflags -o dev/bench/xnet2/bench_echo_tls_cmp.exe @winlibs
gcc dev/bench/xnet2/bench_udp_burst.c xrt.c @cflags -o dev/bench/xnet2/bench_udp_burst_cmp.exe @winlibs
gcc dev/bench/xnet2/bench_send_queue_pressure.c xrt.c @cflags -o dev/bench/xnet2/bench_send_queue_pressure_cmp.exe @winlibs

& "$root/dev/bench/xnet_v1/bench_echo_tcp_v1_cmp.exe" $tcpIter $messageSize 19271
& "$root/dev/bench/xnet2/bench_echo_tcp_cmp.exe" $tcpIter $messageSize
& "$root/dev/bench/xnet_v1/bench_echo_tls_v1_cmp.exe" $tlsIter $messageSize $cert $key 19272
& "$root/dev/bench/xnet2/bench_echo_tls_cmp.exe" $tlsIter $messageSize $cert $key
& "$root/dev/bench/xnet_v1/bench_udp_burst_v1_cmp.exe" $udpPackets $udpPacketSize 19273
& "$root/dev/bench/xnet2/bench_udp_burst_cmp.exe" $udpPackets $udpPacketSize
& "$root/dev/bench/xnet2/bench_send_queue_pressure_cmp.exe" $pressureMessages $pressureMessageSize $pressurePauseMs $pressureDrainTimeoutMs
