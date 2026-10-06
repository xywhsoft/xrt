/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xssh requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XSSH_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XSSH_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XSSH_SINGLE_HEADER_H
#define XSSH_SINGLE_HEADER_H
#define XSSH_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xssh/include/xssh/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XSSH_FEATURES_H
#define XSSH_FEATURES_H

/* xssh 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_XSSH)
#ifndef XSSH_FEATURE_SSH
#define XSSH_FEATURE_SSH
#endif
#ifndef XSSH_MODULE_SSH_PACKET_CODEC_RANDOM
#define XSSH_MODULE_SSH_PACKET_CODEC_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_KEXINIT_RANDOM
#define XSSH_MODULE_SSH_KEXINIT_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_KEX_ECDH
#define XSSH_MODULE_SSH_KEX_ECDH
#endif
#ifndef XSSH_MODULE_SSH_KEX_SHA256
#define XSSH_MODULE_SSH_KEX_SHA256
#endif
#ifndef XSSH_MODULE_SSH_KEX_CURVE25519_RANDOM
#define XSSH_MODULE_SSH_KEX_CURVE25519_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_HOSTKEY_ED25519
#define XSSH_MODULE_SSH_HOSTKEY_ED25519
#endif
#ifndef XSSH_MODULE_SSH_KEY_TEXT
#define XSSH_MODULE_SSH_KEY_TEXT
#endif
#ifndef XSSH_MODULE_SSH_KNOWN_HOST
#define XSSH_MODULE_SSH_KNOWN_HOST
#endif
#ifndef XSSH_MODULE_SSH_KNOWN_HOST_HASH
#define XSSH_MODULE_SSH_KNOWN_HOST_HASH
#endif
#ifndef XSSH_MODULE_SSH_KNOWN_HOST_DB
#define XSSH_MODULE_SSH_KNOWN_HOST_DB
#endif
#ifndef XSSH_MODULE_SSH_FINGERPRINT
#define XSSH_MODULE_SSH_FINGERPRINT
#endif
#ifndef XSSH_MODULE_SSH_PRIVATE_KEY_PEM
#define XSSH_MODULE_SSH_PRIVATE_KEY_PEM
#endif
#ifndef XSSH_MODULE_SSH_PRIVATE_KEY_ED25519
#define XSSH_MODULE_SSH_PRIVATE_KEY_ED25519
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_MESSAGE
#define XSSH_MODULE_SSH_TRANSPORT_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_REKEY
#define XSSH_MODULE_SSH_TRANSPORT_REKEY
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_STATE
#define XSSH_MODULE_SSH_TRANSPORT_STATE
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_CORE
#define XSSH_MODULE_SSH_TRANSPORT_CORE
#endif
#ifndef XSSH_MODULE_SSH_KEX_SESSION
#define XSSH_MODULE_SSH_KEX_SESSION
#endif
#ifndef XSSH_MODULE_SSH_KEX_SESSION_RANDOM
#define XSSH_MODULE_SSH_KEX_SESSION_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_KEX_EXCHANGE
#define XSSH_MODULE_SSH_KEX_EXCHANGE
#endif
#ifndef XSSH_MODULE_SSH_KEX_EXCHANGE_RANDOM
#define XSSH_MODULE_SSH_KEX_EXCHANGE_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_TCP
#define XSSH_MODULE_SSH_TRANSPORT_TCP
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_TCP_RANDOM
#define XSSH_MODULE_SSH_TRANSPORT_TCP_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_AUTH_PASSWORD
#define XSSH_MODULE_SSH_AUTH_PASSWORD
#endif
#ifndef XSSH_MODULE_SSH_AUTH_PUBLICKEY
#define XSSH_MODULE_SSH_AUTH_PUBLICKEY
#endif
#ifndef XSSH_MODULE_SSH_AUTH_KEYBOARD
#define XSSH_MODULE_SSH_AUTH_KEYBOARD
#endif
#ifndef XSSH_MODULE_SSH_AUTH_HOSTBASED
#define XSSH_MODULE_SSH_AUTH_HOSTBASED
#endif
#ifndef XSSH_MODULE_SSH_AUTH_GUARD
#define XSSH_MODULE_SSH_AUTH_GUARD
#endif
#ifndef XSSH_MODULE_SSH_AUTH_SESSION
#define XSSH_MODULE_SSH_AUTH_SESSION
#endif
#ifndef XSSH_MODULE_SSH_CONNECTION_MESSAGE
#define XSSH_MODULE_SSH_CONNECTION_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_MESSAGE
#define XSSH_MODULE_SSH_CHANNEL_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_WINDOW
#define XSSH_MODULE_SSH_CHANNEL_WINDOW
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_REQUEST
#define XSSH_MODULE_SSH_CHANNEL_REQUEST
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_PTY
#define XSSH_MODULE_SSH_CHANNEL_PTY
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_STATE
#define XSSH_MODULE_SSH_CHANNEL_STATE
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_CORE
#define XSSH_MODULE_SSH_CHANNEL_CORE
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_IO
#define XSSH_MODULE_SSH_CHANNEL_IO
#endif
#ifndef XSSH_MODULE_SSH_FORWARD_MESSAGE
#define XSSH_MODULE_SSH_FORWARD_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_REPLY_QUEUE
#define XSSH_MODULE_SSH_REPLY_QUEUE
#endif
#ifndef XSSH_MODULE_SSH_CONNECTION_SESSION
#define XSSH_MODULE_SSH_CONNECTION_SESSION
#endif
#ifndef XSSH_MODULE_SSH_CHANNELS
#define XSSH_MODULE_SSH_CHANNELS
#endif
#ifndef XSSH_MODULE_SSH_SESSION_CORE
#define XSSH_MODULE_SSH_SESSION_CORE
#endif
#ifndef XSSH_MODULE_SSH_SESSION_CORE_RANDOM
#define XSSH_MODULE_SSH_SESSION_CORE_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_SESSION_TCP
#define XSSH_MODULE_SSH_SESSION_TCP
#endif
#ifndef XSSH_MODULE_SSH_SESSION_READER
#define XSSH_MODULE_SSH_SESSION_READER
#endif
#ifndef XSSH_MODULE_SSH_SESSION_STREAM
#define XSSH_MODULE_SSH_SESSION_STREAM
#endif
#ifndef XSSH_MODULE_SSH_SESSION_TCP_RANDOM
#define XSSH_MODULE_SSH_SESSION_TCP_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_CLIENT_CORE
#define XSSH_MODULE_SSH_CLIENT_CORE
#endif
#ifndef XSSH_MODULE_SSH_CLIENT_AUTH_ED25519
#define XSSH_MODULE_SSH_CLIENT_AUTH_ED25519
#endif
#ifndef XSSH_MODULE_SSH_CLIENT
#define XSSH_MODULE_SSH_CLIENT
#endif
#ifndef XSSH_MODULE_SSH_CLIENT_DIAL
#define XSSH_MODULE_SSH_CLIENT_DIAL
#endif
#ifndef XSSH_MODULE_SSH_CLIENT_FUTURE
#define XSSH_MODULE_SSH_CLIENT_FUTURE
#endif
#ifndef XSSH_MODULE_SSH_CLIENT_SESSION
#define XSSH_MODULE_SSH_CLIENT_SESSION
#endif
#ifndef XSSH_MODULE_SSH_CLIENT_FORWARD
#define XSSH_MODULE_SSH_CLIENT_FORWARD
#endif
#ifndef XSSH_MODULE_SSH_CLIENT_PTY
#define XSSH_MODULE_SSH_CLIENT_PTY
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#endif

/* ssh_client_pty 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CLIENT_PTY)
#ifndef XSSH_FEATURE_CLIENT_PTY
#define XSSH_FEATURE_CLIENT_PTY
#endif
#ifndef XSSH_MODULE_SSH_CLIENT_SESSION
#define XSSH_MODULE_SSH_CLIENT_SESSION
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_PTY
#define XSSH_MODULE_SSH_CHANNEL_PTY
#endif
#endif

/* ssh_client_future 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CLIENT_FUTURE)
#ifndef XSSH_FEATURE_CLIENT_FUTURE
#define XSSH_FEATURE_CLIENT_FUTURE
#endif
#ifndef XSSH_MODULE_SSH_CLIENT
#define XSSH_MODULE_SSH_CLIENT
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_SPIN
#define XRT_MODULE_SPIN
#endif
#endif

/* ssh_client_forward 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CLIENT_FORWARD)
#ifndef XSSH_FEATURE_CLIENT_FORWARD
#define XSSH_FEATURE_CLIENT_FORWARD
#endif
#ifndef XSSH_MODULE_SSH_CLIENT
#define XSSH_MODULE_SSH_CLIENT
#endif
#ifndef XSSH_MODULE_SSH_FORWARD_MESSAGE
#define XSSH_MODULE_SSH_FORWARD_MESSAGE
#endif
#endif

/* ssh_client_dial 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CLIENT_DIAL)
#ifndef XSSH_FEATURE_CLIENT_DIAL
#define XSSH_FEATURE_CLIENT_DIAL
#endif
#ifndef XSSH_MODULE_SSH_CLIENT
#define XSSH_MODULE_SSH_CLIENT
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL
#define XRT_MODULE_NET_TCP_DIAL
#endif
#endif

/* ssh_client_session 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CLIENT_SESSION)
#ifndef XSSH_FEATURE_CLIENT_SESSION
#define XSSH_FEATURE_CLIENT_SESSION
#endif
#ifndef XSSH_MODULE_SSH_CLIENT
#define XSSH_MODULE_SSH_CLIENT
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_REQUEST
#define XSSH_MODULE_SSH_CHANNEL_REQUEST
#endif
#endif

/* ssh_client 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CLIENT)
#ifndef XSSH_FEATURE_CLIENT
#define XSSH_FEATURE_CLIENT
#endif
#ifndef XSSH_MODULE_SSH_CLIENT_CORE
#define XSSH_MODULE_SSH_CLIENT_CORE
#endif
#ifndef XSSH_MODULE_SSH_SESSION_STREAM
#define XSSH_MODULE_SSH_SESSION_STREAM
#endif
#ifndef XSSH_MODULE_SSH_CHANNELS
#define XSSH_MODULE_SSH_CHANNELS
#endif
#endif

/* ssh_client_auth_ed25519 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CLIENT_AUTH_ED25519)
#ifndef XSSH_FEATURE_CLIENT_AUTH_ED25519
#define XSSH_FEATURE_CLIENT_AUTH_ED25519
#endif
#ifndef XSSH_MODULE_SSH_CLIENT_CORE
#define XSSH_MODULE_SSH_CLIENT_CORE
#endif
#ifndef XSSH_MODULE_SSH_AUTH_PUBLICKEY
#define XSSH_MODULE_SSH_AUTH_PUBLICKEY
#endif
#ifndef XSSH_MODULE_SSH_PRIVATE_KEY_ED25519
#define XSSH_MODULE_SSH_PRIVATE_KEY_ED25519
#endif
#endif

/* ssh_client_core 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CLIENT_CORE)
#ifndef XSSH_FEATURE_CLIENT_CORE
#define XSSH_FEATURE_CLIENT_CORE
#endif
#ifndef XSSH_MODULE_SSH_SESSION_READER
#define XSSH_MODULE_SSH_SESSION_READER
#endif
#ifndef XSSH_MODULE_SSH_SESSION_TCP_RANDOM
#define XSSH_MODULE_SSH_SESSION_TCP_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_KEXINIT_RANDOM
#define XSSH_MODULE_SSH_KEXINIT_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_AUTH_PASSWORD
#define XSSH_MODULE_SSH_AUTH_PASSWORD
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_MESSAGE
#define XSSH_MODULE_SSH_TRANSPORT_MESSAGE
#endif
#endif

/* ssh_session_tcp_random 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_SESSION_TCP_RANDOM)
#ifndef XSSH_FEATURE_SESSION_TCP_RANDOM
#define XSSH_FEATURE_SESSION_TCP_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_SESSION_TCP
#define XSSH_MODULE_SSH_SESSION_TCP
#endif
#ifndef XSSH_MODULE_SSH_SESSION_CORE_RANDOM
#define XSSH_MODULE_SSH_SESSION_CORE_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_TCP_RANDOM
#define XSSH_MODULE_SSH_TRANSPORT_TCP_RANDOM
#endif
#endif

/* ssh_session_stream 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_SESSION_STREAM)
#ifndef XSSH_FEATURE_SESSION_STREAM
#define XSSH_FEATURE_SESSION_STREAM
#endif
#ifndef XSSH_MODULE_SSH_SESSION_READER
#define XSSH_MODULE_SSH_SESSION_READER
#endif
#endif

/* ssh_session_reader 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_SESSION_READER)
#ifndef XSSH_FEATURE_SESSION_READER
#define XSSH_FEATURE_SESSION_READER
#endif
#ifndef XSSH_MODULE_SSH_SESSION_TCP
#define XSSH_MODULE_SSH_SESSION_TCP
#endif
#endif

/* ssh_session_tcp 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_SESSION_TCP)
#ifndef XSSH_FEATURE_SESSION_TCP
#define XSSH_FEATURE_SESSION_TCP
#endif
#ifndef XSSH_MODULE_SSH_SESSION_CORE
#define XSSH_MODULE_SSH_SESSION_CORE
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_TCP
#define XSSH_MODULE_SSH_TRANSPORT_TCP
#endif
#endif

/* ssh_session_core_random 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_SESSION_CORE_RANDOM)
#ifndef XSSH_FEATURE_SESSION_CORE_RANDOM
#define XSSH_FEATURE_SESSION_CORE_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_SESSION_CORE
#define XSSH_MODULE_SSH_SESSION_CORE
#endif
#ifndef XSSH_MODULE_SSH_KEX_EXCHANGE_RANDOM
#define XSSH_MODULE_SSH_KEX_EXCHANGE_RANDOM
#endif
#endif

/* ssh_session_core 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_SESSION_CORE)
#ifndef XSSH_FEATURE_SESSION_CORE
#define XSSH_FEATURE_SESSION_CORE
#endif
#ifndef XSSH_MODULE_SSH_KEX_EXCHANGE
#define XSSH_MODULE_SSH_KEX_EXCHANGE
#endif
#ifndef XSSH_MODULE_SSH_AUTH_SESSION
#define XSSH_MODULE_SSH_AUTH_SESSION
#endif
#ifndef XSSH_MODULE_SSH_CONNECTION_SESSION
#define XSSH_MODULE_SSH_CONNECTION_SESSION
#endif
#endif

/* ssh_channels 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CHANNELS)
#ifndef XSSH_FEATURE_CHANNELS
#define XSSH_FEATURE_CHANNELS
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_IO
#define XSSH_MODULE_SSH_CHANNEL_IO
#endif
#ifndef XSSH_MODULE_SSH_CONNECTION_SESSION
#define XSSH_MODULE_SSH_CONNECTION_SESSION
#endif
#ifndef XRT_MODULE_INT_MAP
#define XRT_MODULE_INT_MAP
#endif
#endif

/* ssh_connection_session 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CONNECTION_SESSION)
#ifndef XSSH_FEATURE_CONNECTION_SESSION
#define XSSH_FEATURE_CONNECTION_SESSION
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_CORE
#define XSSH_MODULE_SSH_CHANNEL_CORE
#endif
#ifndef XSSH_MODULE_SSH_CONNECTION_MESSAGE
#define XSSH_MODULE_SSH_CONNECTION_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_REPLY_QUEUE
#define XSSH_MODULE_SSH_REPLY_QUEUE
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_CORE
#define XSSH_MODULE_SSH_TRANSPORT_CORE
#endif
#endif

/* ssh_reply_queue 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_REPLY_QUEUE)
#ifndef XSSH_FEATURE_REPLY_QUEUE
#define XSSH_FEATURE_REPLY_QUEUE
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_forward_message 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_FORWARD_MESSAGE)
#ifndef XSSH_FEATURE_FORWARD_MESSAGE
#define XSSH_FEATURE_FORWARD_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_CONNECTION_MESSAGE
#define XSSH_MODULE_SSH_CONNECTION_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_MESSAGE
#define XSSH_MODULE_SSH_CHANNEL_MESSAGE
#endif
#endif

/* ssh_channel_io 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CHANNEL_IO)
#ifndef XSSH_FEATURE_CHANNEL_IO
#define XSSH_FEATURE_CHANNEL_IO
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_CORE
#define XSSH_MODULE_SSH_CHANNEL_CORE
#endif
#ifndef XRT_MODULE_NET_BUFFER
#define XRT_MODULE_NET_BUFFER
#endif
#endif

/* ssh_channel_core 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CHANNEL_CORE)
#ifndef XSSH_FEATURE_CHANNEL_CORE
#define XSSH_FEATURE_CHANNEL_CORE
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_MESSAGE
#define XSSH_MODULE_SSH_CHANNEL_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_STATE
#define XSSH_MODULE_SSH_CHANNEL_STATE
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_WINDOW
#define XSSH_MODULE_SSH_CHANNEL_WINDOW
#endif
#endif

/* ssh_channel_state 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CHANNEL_STATE)
#ifndef XSSH_FEATURE_CHANNEL_STATE
#define XSSH_FEATURE_CHANNEL_STATE
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_channel_pty 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CHANNEL_PTY)
#ifndef XSSH_FEATURE_CHANNEL_PTY
#define XSSH_FEATURE_CHANNEL_PTY
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_REQUEST
#define XSSH_MODULE_SSH_CHANNEL_REQUEST
#endif
#endif

/* ssh_channel_request 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CHANNEL_REQUEST)
#ifndef XSSH_FEATURE_CHANNEL_REQUEST
#define XSSH_FEATURE_CHANNEL_REQUEST
#endif
#ifndef XSSH_MODULE_SSH_CHANNEL_MESSAGE
#define XSSH_MODULE_SSH_CHANNEL_MESSAGE
#endif
#endif

/* ssh_channel_window 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CHANNEL_WINDOW)
#ifndef XSSH_FEATURE_CHANNEL_WINDOW
#define XSSH_FEATURE_CHANNEL_WINDOW
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_channel_message 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CHANNEL_MESSAGE)
#ifndef XSSH_FEATURE_CHANNEL_MESSAGE
#define XSSH_FEATURE_CHANNEL_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* ssh_connection_message 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_CONNECTION_MESSAGE)
#ifndef XSSH_FEATURE_CONNECTION_MESSAGE
#define XSSH_FEATURE_CONNECTION_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_auth_session 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_AUTH_SESSION)
#ifndef XSSH_FEATURE_AUTH_SESSION
#define XSSH_FEATURE_AUTH_SESSION
#endif
#ifndef XSSH_MODULE_SSH_AUTH_GUARD
#define XSSH_MODULE_SSH_AUTH_GUARD
#endif
#ifndef XSSH_MODULE_SSH_AUTH_MESSAGE
#define XSSH_MODULE_SSH_AUTH_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_CORE
#define XSSH_MODULE_SSH_TRANSPORT_CORE
#endif
#endif

/* ssh_auth_guard 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_AUTH_GUARD)
#ifndef XSSH_FEATURE_AUTH_GUARD
#define XSSH_FEATURE_AUTH_GUARD
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_auth_hostbased 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_AUTH_HOSTBASED)
#ifndef XSSH_FEATURE_AUTH_HOSTBASED
#define XSSH_FEATURE_AUTH_HOSTBASED
#endif
#ifndef XSSH_MODULE_SSH_AUTH_MESSAGE
#define XSSH_MODULE_SSH_AUTH_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_HOSTKEY
#define XSSH_MODULE_SSH_HOSTKEY
#endif
#endif

/* ssh_auth_keyboard 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_AUTH_KEYBOARD)
#ifndef XSSH_FEATURE_AUTH_KEYBOARD
#define XSSH_FEATURE_AUTH_KEYBOARD
#endif
#ifndef XSSH_MODULE_SSH_AUTH_MESSAGE
#define XSSH_MODULE_SSH_AUTH_MESSAGE
#endif
#endif

/* ssh_auth_publickey 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_AUTH_PUBLICKEY)
#ifndef XSSH_FEATURE_AUTH_PUBLICKEY
#define XSSH_FEATURE_AUTH_PUBLICKEY
#endif
#ifndef XSSH_MODULE_SSH_AUTH_MESSAGE
#define XSSH_MODULE_SSH_AUTH_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_HOSTKEY
#define XSSH_MODULE_SSH_HOSTKEY
#endif
#endif

/* ssh_auth_password 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_AUTH_PASSWORD)
#ifndef XSSH_FEATURE_AUTH_PASSWORD
#define XSSH_FEATURE_AUTH_PASSWORD
#endif
#ifndef XSSH_MODULE_SSH_AUTH_MESSAGE
#define XSSH_MODULE_SSH_AUTH_MESSAGE
#endif
#endif

/* ssh_auth_message 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_AUTH_MESSAGE)
#ifndef XSSH_FEATURE_AUTH_MESSAGE
#define XSSH_FEATURE_AUTH_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* ssh_transport_tcp_random 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_TRANSPORT_TCP_RANDOM)
#ifndef XSSH_FEATURE_TRANSPORT_TCP_RANDOM
#define XSSH_FEATURE_TRANSPORT_TCP_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_TCP
#define XSSH_MODULE_SSH_TRANSPORT_TCP
#endif
#ifndef XSSH_MODULE_SSH_PACKET_RANDOM
#define XSSH_MODULE_SSH_PACKET_RANDOM
#endif
#endif

/* ssh_transport_tcp 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_TRANSPORT_TCP)
#ifndef XSSH_FEATURE_TRANSPORT_TCP
#define XSSH_FEATURE_TRANSPORT_TCP
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_CORE
#define XSSH_MODULE_SSH_TRANSPORT_CORE
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#endif

/* ssh_kex_exchange_random 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEX_EXCHANGE_RANDOM)
#ifndef XSSH_FEATURE_KEX_EXCHANGE_RANDOM
#define XSSH_FEATURE_KEX_EXCHANGE_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_KEX_EXCHANGE
#define XSSH_MODULE_SSH_KEX_EXCHANGE
#endif
#ifndef XSSH_MODULE_SSH_KEX_CURVE25519_RANDOM
#define XSSH_MODULE_SSH_KEX_CURVE25519_RANDOM
#endif
#endif

/* ssh_kex_exchange 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEX_EXCHANGE)
#ifndef XSSH_FEATURE_KEX_EXCHANGE
#define XSSH_FEATURE_KEX_EXCHANGE
#endif
#ifndef XSSH_MODULE_SSH_KEX_SESSION
#define XSSH_MODULE_SSH_KEX_SESSION
#endif
#ifndef XRT_MODULE_NET_BUFFER
#define XRT_MODULE_NET_BUFFER
#endif
#endif

/* ssh_kex_session_random 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEX_SESSION_RANDOM)
#ifndef XSSH_FEATURE_KEX_SESSION_RANDOM
#define XSSH_FEATURE_KEX_SESSION_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_KEX_SESSION
#define XSSH_MODULE_SSH_KEX_SESSION
#endif
#ifndef XSSH_MODULE_SSH_KEX_CURVE25519_RANDOM
#define XSSH_MODULE_SSH_KEX_CURVE25519_RANDOM
#endif
#endif

/* ssh_kex_session 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEX_SESSION)
#ifndef XSSH_FEATURE_KEX_SESSION
#define XSSH_FEATURE_KEX_SESSION
#endif
#ifndef XSSH_MODULE_SSH_HOSTKEY_ED25519
#define XSSH_MODULE_SSH_HOSTKEY_ED25519
#endif
#ifndef XSSH_MODULE_SSH_KEX_CURVE25519
#define XSSH_MODULE_SSH_KEX_CURVE25519
#endif
#ifndef XSSH_MODULE_SSH_KEX_ECDH
#define XSSH_MODULE_SSH_KEX_ECDH
#endif
#ifndef XSSH_MODULE_SSH_KEX_SHA256
#define XSSH_MODULE_SSH_KEX_SHA256
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_CORE
#define XSSH_MODULE_SSH_TRANSPORT_CORE
#endif
#endif

/* ssh_transport_core 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_TRANSPORT_CORE)
#ifndef XSSH_FEATURE_TRANSPORT_CORE
#define XSSH_FEATURE_TRANSPORT_CORE
#endif
#ifndef XSSH_MODULE_SSH_PACKET_CODEC
#define XSSH_MODULE_SSH_PACKET_CODEC
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_REKEY
#define XSSH_MODULE_SSH_TRANSPORT_REKEY
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_STATE
#define XSSH_MODULE_SSH_TRANSPORT_STATE
#endif
#endif

/* ssh_transport_state 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_TRANSPORT_STATE)
#ifndef XSSH_FEATURE_TRANSPORT_STATE
#define XSSH_FEATURE_TRANSPORT_STATE
#endif
#ifndef XSSH_MODULE_SSH_KEXINIT
#define XSSH_MODULE_SSH_KEXINIT
#endif
#ifndef XSSH_MODULE_SSH_TRANSPORT_MESSAGE
#define XSSH_MODULE_SSH_TRANSPORT_MESSAGE
#endif
#endif

/* ssh_transport_rekey 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_TRANSPORT_REKEY)
#ifndef XSSH_FEATURE_TRANSPORT_REKEY
#define XSSH_FEATURE_TRANSPORT_REKEY
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_transport_message 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_TRANSPORT_MESSAGE)
#ifndef XSSH_FEATURE_TRANSPORT_MESSAGE
#define XSSH_FEATURE_TRANSPORT_MESSAGE
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_private_key_ed25519 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_PRIVATE_KEY_ED25519)
#ifndef XSSH_FEATURE_PRIVATE_KEY_ED25519
#define XSSH_FEATURE_PRIVATE_KEY_ED25519
#endif
#ifndef XSSH_MODULE_SSH_PRIVATE_KEY
#define XSSH_MODULE_SSH_PRIVATE_KEY
#endif
#ifndef XRT_MODULE_CRYPTO_ED25519_SIGN
#define XRT_MODULE_CRYPTO_ED25519_SIGN
#endif
#endif

/* ssh_private_key_pem 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_PRIVATE_KEY_PEM)
#ifndef XSSH_FEATURE_PRIVATE_KEY_PEM
#define XSSH_FEATURE_PRIVATE_KEY_PEM
#endif
#ifndef XSSH_MODULE_SSH_PRIVATE_KEY
#define XSSH_MODULE_SSH_PRIVATE_KEY
#endif
#ifndef XRT_MODULE_PEM
#define XRT_MODULE_PEM
#endif
#endif

/* ssh_private_key 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_PRIVATE_KEY)
#ifndef XSSH_FEATURE_PRIVATE_KEY
#define XSSH_FEATURE_PRIVATE_KEY
#endif
#ifndef XSSH_MODULE_SSH_HOSTKEY
#define XSSH_MODULE_SSH_HOSTKEY
#endif
#endif

/* ssh_fingerprint 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_FINGERPRINT)
#ifndef XSSH_FEATURE_FINGERPRINT
#define XSSH_FEATURE_FINGERPRINT
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#endif

/* ssh_known_host_db 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KNOWN_HOST_DB)
#ifndef XSSH_FEATURE_KNOWN_HOST_DB
#define XSSH_FEATURE_KNOWN_HOST_DB
#endif
#ifndef XSSH_MODULE_SSH_KNOWN_HOST_HASH
#define XSSH_MODULE_SSH_KNOWN_HOST_HASH
#endif
#endif

/* ssh_known_host_hash 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KNOWN_HOST_HASH)
#ifndef XSSH_FEATURE_KNOWN_HOST_HASH
#define XSSH_FEATURE_KNOWN_HOST_HASH
#endif
#ifndef XSSH_MODULE_SSH_KNOWN_HOST
#define XSSH_MODULE_SSH_KNOWN_HOST
#endif
#ifndef XRT_MODULE_CRYPTO_SHA1
#define XRT_MODULE_CRYPTO_SHA1
#endif
#endif

/* ssh_known_host 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KNOWN_HOST)
#ifndef XSSH_FEATURE_KNOWN_HOST
#define XSSH_FEATURE_KNOWN_HOST
#endif
#ifndef XSSH_MODULE_SSH_KEY_TEXT
#define XSSH_MODULE_SSH_KEY_TEXT
#endif
#endif

/* ssh_key_text 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEY_TEXT)
#ifndef XSSH_FEATURE_KEY_TEXT
#define XSSH_FEATURE_KEY_TEXT
#endif
#ifndef XSSH_MODULE_SSH_HOSTKEY
#define XSSH_MODULE_SSH_HOSTKEY
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* ssh_hostkey_ed25519 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_HOSTKEY_ED25519)
#ifndef XSSH_FEATURE_HOSTKEY_ED25519
#define XSSH_FEATURE_HOSTKEY_ED25519
#endif
#ifndef XSSH_MODULE_SSH_HOSTKEY
#define XSSH_MODULE_SSH_HOSTKEY
#endif
#ifndef XRT_MODULE_CRYPTO_ED25519_VERIFY
#define XRT_MODULE_CRYPTO_ED25519_VERIFY
#endif
#endif

/* ssh_hostkey 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_HOSTKEY)
#ifndef XSSH_FEATURE_HOSTKEY
#define XSSH_FEATURE_HOSTKEY
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_kex_curve25519_random 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEX_CURVE25519_RANDOM)
#ifndef XSSH_FEATURE_KEX_CURVE25519_RANDOM
#define XSSH_FEATURE_KEX_CURVE25519_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_KEX_CURVE25519
#define XSSH_MODULE_SSH_KEX_CURVE25519
#endif
#ifndef XRT_MODULE_CRYPTO_X25519_KEYPAIR
#define XRT_MODULE_CRYPTO_X25519_KEYPAIR
#endif
#endif

/* ssh_kex_curve25519 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEX_CURVE25519)
#ifndef XSSH_FEATURE_KEX_CURVE25519
#define XSSH_FEATURE_KEX_CURVE25519
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#ifndef XRT_MODULE_CRYPTO_X25519
#define XRT_MODULE_CRYPTO_X25519
#endif
#endif

/* ssh_kex_sha256 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEX_SHA256)
#ifndef XSSH_FEATURE_KEX_SHA256
#define XSSH_FEATURE_KEX_SHA256
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#endif

/* ssh_kex_ecdh 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEX_ECDH)
#ifndef XSSH_FEATURE_KEX_ECDH
#define XSSH_FEATURE_KEX_ECDH
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_kexinit_random 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEXINIT_RANDOM)
#ifndef XSSH_FEATURE_KEXINIT_RANDOM
#define XSSH_FEATURE_KEXINIT_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_KEXINIT
#define XSSH_MODULE_SSH_KEXINIT
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* ssh_kexinit 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_KEXINIT)
#ifndef XSSH_FEATURE_KEXINIT
#define XSSH_FEATURE_KEXINIT
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_packet_codec_random 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_PACKET_CODEC_RANDOM)
#ifndef XSSH_FEATURE_PACKET_CODEC_RANDOM
#define XSSH_FEATURE_PACKET_CODEC_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_PACKET_CODEC
#define XSSH_MODULE_SSH_PACKET_CODEC
#endif
#ifndef XSSH_MODULE_SSH_PACKET_RANDOM
#define XSSH_MODULE_SSH_PACKET_RANDOM
#endif
#endif

/* ssh_packet_codec 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_PACKET_CODEC)
#ifndef XSSH_FEATURE_PACKET_CODEC
#define XSSH_FEATURE_PACKET_CODEC
#endif
#ifndef XSSH_MODULE_SSH_PACKET_AES_GCM
#define XSSH_MODULE_SSH_PACKET_AES_GCM
#endif
#endif

/* ssh_packet_aes_gcm 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_PACKET_AES_GCM)
#ifndef XSSH_FEATURE_PACKET_AES_GCM
#define XSSH_FEATURE_PACKET_AES_GCM
#endif
#ifndef XSSH_MODULE_SSH_PACKET
#define XSSH_MODULE_SSH_PACKET
#endif
#ifndef XRT_MODULE_CRYPTO_AES_GCM
#define XRT_MODULE_CRYPTO_AES_GCM
#endif
#endif

/* ssh_packet_random 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_PACKET_RANDOM)
#ifndef XSSH_FEATURE_PACKET_RANDOM
#define XSSH_FEATURE_PACKET_RANDOM
#endif
#ifndef XSSH_MODULE_SSH_PACKET
#define XSSH_MODULE_SSH_PACKET
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* ssh_packet 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_PACKET)
#ifndef XSSH_FEATURE_PACKET
#define XSSH_FEATURE_PACKET
#endif
#ifndef XSSH_MODULE_SSH_WIRE
#define XSSH_MODULE_SSH_WIRE
#endif
#endif

/* ssh_wire 及其直接依赖。 */
#if defined(XSSH_MODULE_ALL) || defined(XSSH_MODULE_SSH_WIRE)
#ifndef XSSH_FEATURE_WIRE
#define XSSH_FEATURE_WIRE
#endif
#endif

#endif /* XSSH_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_wire.h */
/* ========================================================================== */

#ifndef XRT_SSH_WIRE_H
#define XRT_SSH_WIRE_H




#if defined(XSSH_FEATURE_SSH) && \
	(!defined(XSSH_FEATURE_PACKET_CODEC_RANDOM) || \
	 !defined(XSSH_FEATURE_KEXINIT_RANDOM) || \
	 !defined(XSSH_FEATURE_KEX_ECDH) || \
	 !defined(XSSH_FEATURE_KEX_SHA256) || \
	 !defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM) || \
	 !defined(XSSH_FEATURE_HOSTKEY_ED25519) || \
	 !defined(XSSH_FEATURE_KEY_TEXT) || \
	 !defined(XSSH_FEATURE_KNOWN_HOST) || \
	 !defined(XSSH_FEATURE_KNOWN_HOST_HASH) || \
	 !defined(XSSH_FEATURE_KNOWN_HOST_DB) || \
	 !defined(XSSH_FEATURE_FINGERPRINT) || \
	 !defined(XSSH_FEATURE_PRIVATE_KEY) || \
	 !defined(XSSH_FEATURE_PRIVATE_KEY_PEM) || \
	 !defined(XSSH_FEATURE_PRIVATE_KEY_ED25519) || \
	 !defined(XSSH_FEATURE_TRANSPORT_MESSAGE) || \
	 !defined(XSSH_FEATURE_TRANSPORT_REKEY) || \
	 !defined(XSSH_FEATURE_TRANSPORT_STATE) || \
	 !defined(XSSH_FEATURE_TRANSPORT_CORE) || \
	 !defined(XSSH_FEATURE_KEX_SESSION) || \
	 !defined(XSSH_FEATURE_KEX_SESSION_RANDOM) || \
	 !defined(XSSH_FEATURE_KEX_EXCHANGE) || \
	 !defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM) || \
	 !defined(XSSH_FEATURE_TRANSPORT_TCP) || \
	 !defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM) || \
	 !defined(XSSH_FEATURE_AUTH_PASSWORD) || \
	 !defined(XSSH_FEATURE_AUTH_PUBLICKEY) || \
	 !defined(XSSH_FEATURE_AUTH_KEYBOARD) || \
	 !defined(XSSH_FEATURE_AUTH_HOSTBASED) || \
	 !defined(XSSH_FEATURE_AUTH_GUARD) || \
	 !defined(XSSH_FEATURE_AUTH_SESSION) || \
	 !defined(XSSH_FEATURE_CONNECTION_MESSAGE) || \
	 !defined(XSSH_FEATURE_CHANNEL_MESSAGE) || \
	 !defined(XSSH_FEATURE_CHANNEL_WINDOW) || \
	 !defined(XSSH_FEATURE_CHANNEL_REQUEST) || \
	 !defined(XSSH_FEATURE_CHANNEL_PTY) || \
	 !defined(XSSH_FEATURE_CHANNEL_STATE) || \
	 !defined(XSSH_FEATURE_CHANNEL_CORE) || \
	 !defined(XSSH_FEATURE_CHANNEL_IO) || \
	 !defined(XSSH_FEATURE_FORWARD_MESSAGE) || \
	 !defined(XSSH_FEATURE_REPLY_QUEUE) || \
	 !defined(XSSH_FEATURE_CONNECTION_SESSION) || \
	 !defined(XSSH_FEATURE_CHANNELS) || \
	 !defined(XSSH_FEATURE_SESSION_CORE) || \
	 !defined(XSSH_FEATURE_SESSION_CORE_RANDOM) || \
	 !defined(XSSH_FEATURE_SESSION_TCP) || \
	 !defined(XSSH_FEATURE_SESSION_READER) || \
	 !defined(XSSH_FEATURE_SESSION_STREAM) || \
	 !defined(XSSH_FEATURE_SESSION_TCP_RANDOM) || \
	 !defined(XSSH_FEATURE_CLIENT_CORE))
	#error "XSSH_FEATURE_SSH requires the complete xssh dependency closure"
#endif



#if defined(XSSH_FEATURE_WIRE)

/* SSH identification 整行上限，包含 CRLF 或兼容性的 LF。 */
#define XSSH_IDENTIFICATION_MAX 255u



/* 无分配协议原语使用稳定结果码；NEED_MORE 不是错误。 */
typedef enum xsshcode {
	XSSH_ERROR_TIMEOUT = -9,
	XSSH_ERROR_STATE = -8,
	XSSH_ERROR_AUTHENTICATION = -7,
	XSSH_ERROR_CALLBACK = -6,
	XSSH_ERROR_PROTOCOL = -5,
	XSSH_ERROR_UNSUPPORTED = -4,
	XSSH_ERROR_OVERFLOW = -3,
	XSSH_ERROR_SPACE = -2,
	XSSH_ERROR_ARGUMENT = -1,
	XSSH_OK = 0,
	XSSH_NEED_MORE = 1
} xsshcode;



/* Reader 借用完整输入，失败的读取不会推进 Position。 */
typedef struct xsshreader {
	xbytesview Source;
	size_t Position;
} xsshreader;



/* Writer 借用调用方缓冲，失败的写入不会推进 Size。 */
typedef struct xsshwriter {
	bytes Data;
	size_t Capacity;
	size_t Size;
} xsshwriter;



XRT_EXTERN_C_BEGIN



/* 初始化借用输入的 SSH reader；空输入允许 Data 为 NULL。 */
XRT_API bool xrtSshReaderInit(xsshreader* pReader, xbytesview Source);



/* 初始化借用输出缓冲的 SSH writer；零容量允许 Data 为 NULL。 */
XRT_API bool xrtSshWriterInit(xsshwriter* pWriter, void* pData, size_t iCapacity);



/* 返回 reader 尚未消费的字节数；无效状态返回零。 */
XRT_API size_t xrtSshReaderRemaining(const xsshreader* pReader);



/* 返回 writer 尚可写入的字节数；无效状态返回零。 */
XRT_API size_t xrtSshWriterRemaining(const xsshwriter* pWriter);



/* 校验 writer 并确认后续写入可一次完成，不改变 writer 状态。 */
XRT_API xsshcode xrtSshWriterReserve(
	const xsshwriter* pWriter,
	size_t iSize
);



/* 校验整段输出与多个借用输入不重叠，不改变 writer 状态。 */
XRT_API xsshcode xrtSshWriterReserveInputs(
	const xsshwriter* pWriter,
	size_t iSize,
	const xbytesview* pInputs,
	size_t iInputCount
);



/* 读取一个 SSH byte，输入不足返回 XSSH_NEED_MORE。 */
XRT_API xsshcode xrtSshReadByte(xsshreader* pReader, uint8* pValue);



/* 读取一个 SSH boolean；零为 false，任意非零值为 true。 */
XRT_API xsshcode xrtSshReadBool(xsshreader* pReader, bool* pValue);



/* 读取一个网络字节序 uint32。 */
XRT_API xsshcode xrtSshReadU32(xsshreader* pReader, uint32* pValue);



/* 读取一个网络字节序 uint64。 */
XRT_API xsshcode xrtSshReadU64(xsshreader* pReader, uint64* pValue);



/* 读取 uint32 长度前缀的 SSH string，并返回借用视图。 */
XRT_API xsshcode xrtSshReadString(xsshreader* pReader, xbytesview* pValue);



/* 读取指定数量的原始字节，并返回借用视图。 */
XRT_API xsshcode xrtSshReadBytes(
	xsshreader* pReader,
	size_t iSize,
	xbytesview* pValue
);



/* 写入一个 SSH byte。 */
XRT_API xsshcode xrtSshWriteByte(xsshwriter* pWriter, uint8 iValue);



/* 写入规范的单字节 SSH boolean。 */
XRT_API xsshcode xrtSshWriteBool(xsshwriter* pWriter, bool bValue);



/* 以网络字节序写入 uint32。 */
XRT_API xsshcode xrtSshWriteU32(xsshwriter* pWriter, uint32 iValue);



/* 以网络字节序写入 uint64。 */
XRT_API xsshcode xrtSshWriteU64(xsshwriter* pWriter, uint64 iValue);



/* 写入 uint32 长度前缀的 SSH string。 */
XRT_API xsshcode xrtSshWriteString(xsshwriter* pWriter, xbytesview Value);



/* 不添加长度前缀，直接写入原始字节。 */
XRT_API xsshcode xrtSshWriteBytes(xsshwriter* pWriter, xbytesview Value);



/* 校验并写入一个 SSH name-list string。 */
XRT_API xsshcode xrtSshWriteNameList(xsshwriter* pWriter, xstrview List);



/* 校验一个非空、可打印 US-ASCII 且不含逗号的 SSH 名称。 */
XRT_API bool xrtSshNameValid(xstrview Name);



/* 校验允许为空且不含控制字符的 ASCII language tag。 */
XRT_API bool xrtSshLanguageValid(xstrview Language);



/* 校验 SSH name-list 的非空项与可打印 US-ASCII 约束。 */
XRT_API bool xrtSshNameListValid(xstrview List);



/* 判断有效 name-list 是否包含一个完整名称。 */
XRT_API bool xrtSshNameListContains(xstrview List, xstrview Name);



/* 判断有效 name-list 是否存在完全相同的重复项。 */
XRT_API bool xrtSshNameListHasDuplicate(xstrview List);



/* 按 Preferred 的顺序返回 Available 中首个匹配名称的借用视图。 */
XRT_API xsshcode xrtSshNameListFirstMatch(
	xstrview Preferred,
	xstrview Available,
	xstrview* pMatch
);



/* 读取规范的非负 SSH mpint，返回原始二进制补码字节视图。 */
XRT_API xsshcode xrtSshReadMpint(xsshreader* pReader, xbytesview* pValue);



/* 将大端无符号 magnitude 规范化并写成非负 SSH mpint。 */
XRT_API xsshcode xrtSshWriteMpint(xsshwriter* pWriter, xbytesview Magnitude);



/* 读取规范的有符号 SSH mpint，并报告其符号。 */
XRT_API xsshcode xrtSshReadSignedMpint(
	xsshreader* pReader,
	xbytesview* pValue,
	bool* pNegative
);



/* 将大端 magnitude 和符号规范化并写成 SSH mpint。 */
XRT_API xsshcode xrtSshWriteSignedMpint(
	xsshwriter* pWriter,
	xbytesview Magnitude,
	bool bNegative
);



/* 从增量输入读取 SSH-2.0/SSH-1.99 identification，可跳过服务端前置行。 */
XRT_API xsshcode xrtSshBannerRead(
	xstrview Data,
	xstrview* pBanner,
	size_t* pConsumed
);



/* 严格校验并写入本端 SSH-2.0 identification 与 CRLF。 */
XRT_API xsshcode xrtSshBannerWrite(
	xsshwriter* pWriter,
	xstrview Banner
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_packet.h */
/* ========================================================================== */

#ifndef XRT_SSH_PACKET_H
#define XRT_SSH_PACKET_H




#if defined(XSSH_FEATURE_PACKET) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_PACKET requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_PACKET)

/* RFC 4253 要求实现至少接收 32768 字节 payload；默认同时限制线路包长。 */
#define XSSH_PACKET_MAX_DEFAULT 35000u
#define XSSH_PACKET_BLOCK_MIN 8u
#define XSSH_PACKET_PADDING_MIN 4u
#define XSSH_PACKET_PADDING_MAX 255u



/* Packet view 借用 reader 输入，PacketSize 不包含四字节长度字段。 */
typedef struct xsshpacketview {
	uint32 Sequence;
	uint32 PacketSize;
	uint8 PaddingSize;
	xbytesview Payload;
	xbytesview Padding;
} xsshpacketview;



/* Padding 回调只填写当前临时片段，返回后不得持有 pOutput。 */
typedef bool (*xsshpaddingproc)(
	void* pOutput,
	size_t iSize,
	ptr pUserData
);



XRT_EXTERN_C_BEGIN



/* 计算 plain packet 的 padding 和 packet_length；零块长使用八字节。 */
XRT_API xsshcode xrtSshPacketMeasure(
	size_t iPayloadSize,
	size_t iBlockSize,
	uint8* pPaddingSize,
	uint32* pPacketSize
);



/* 使用调用方 padding 源写入一个完整 plain packet，并在成功后递增序列号。 */
XRT_API xsshcode xrtSshPacketWrite(
	xsshwriter* pWriter,
	xbytesview Payload,
	size_t iBlockSize,
	uint32* pSequence,
	xsshpaddingproc pPadding,
	ptr pUserData
);



/* 从增量输入读取一个完整 plain packet，并在成功后递增序列号。 */
XRT_API xsshcode xrtSshPacketRead(
	xsshreader* pReader,
	size_t iBlockSize,
	uint32 iMaxPacketSize,
	uint32* pSequence,
	xsshpacketview* pPacket
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_packet_random.h */
/* ========================================================================== */

#ifndef XRT_SSH_PACKET_RANDOM_H
#define XRT_SSH_PACKET_RANDOM_H




#if defined(XSSH_FEATURE_PACKET_RANDOM) && \
	(!defined(XSSH_FEATURE_PACKET) || !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XSSH_FEATURE_PACKET_RANDOM requires packet and random_secure"
#endif



#if defined(XSSH_FEATURE_PACKET_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用操作系统密码学安全随机源填充 packet padding；可直接作为 padding 回调。 */
XRT_API bool xrtSshSecurePadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
);



/* 使用操作系统密码学安全随机源写入一个 plain packet。 */
XRT_API xsshcode xrtSshPacketWriteSecure(
	xsshwriter* pWriter,
	xbytesview Payload,
	size_t iBlockSize,
	uint32* pSequence
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_packet_aes_gcm.h */
/* ========================================================================== */

#ifndef XRT_SSH_PACKET_AES_GCM_H
#define XRT_SSH_PACKET_AES_GCM_H




#if defined(XSSH_FEATURE_PACKET_AES_GCM) && \
	(!defined(XSSH_FEATURE_PACKET) || !defined(XRT_FEATURE_CRYPTO_AES_GCM))
	#error "XSSH_FEATURE_PACKET_AES_GCM requires packet and crypto_aes_gcm"
#endif



#if defined(XSSH_FEATURE_PACKET_AES_GCM)

#define XSSH_AES_GCM_BLOCK_SIZE 16u
#define XSSH_AES_GCM_IV_SIZE 12u
#define XSSH_AES_GCM_TAG_SIZE 16u
#define XSSH_AES_GCM_FIXED_IV_SIZE 4u



/* 每个方向独立持有一个状态；同一状态不得被多个线程并发推进。 */
typedef struct xsshaesgcm {
	xaesgcm Cipher;
	uint8 FixedIV[XSSH_AES_GCM_FIXED_IV_SIZE];
	uint64 Invocation;
	uint32 Guard;
} xsshaesgcm;



XRT_EXTERN_C_BEGIN



/* 计算 AES-GCM packet 的 padding 和 packet_length。 */
XRT_API xsshcode xrtSshAesGcmMeasure(
	size_t iPayloadSize,
	uint8* pPaddingSize,
	uint32* pPacketSize
);



/* 用 AES-128/256 密钥和十二字节 initial IV 初始化单向 packet 状态。 */
XRT_API xsshcode xrtSshAesGcmInit(
	xsshaesgcm* pState,
	xbytesview Key,
	xbytesview InitialIV
);



/* 清除 AES-GCM 密钥、固定 IV 和 invocation counter。 */
XRT_API void xrtSshAesGcmClear(xsshaesgcm* pState);



/* 读取下一次 packet 使用的 invocation counter。 */
XRT_API xsshcode xrtSshAesGcmInvocation(
	const xsshaesgcm* pState,
	uint64* pInvocation
);



/* 原位构建并加密一个 AES-GCM packet；成功后才推进状态和序列号。 */
XRT_API xsshcode xrtSshAesGcmWrite(
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshaesgcm* pState,
	uint32* pSequence,
	xsshpaddingproc pPadding,
	ptr pUserData
);



/* 认证并解密一个 AES-GCM packet；payload 与 padding 借用 plain 缓冲。 */
XRT_API xsshcode xrtSshAesGcmRead(
	xsshreader* pReader,
	xsshaesgcm* pState,
	uint32 iMaxPacketSize,
	uint32* pSequence,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_packet_codec.h */
/* ========================================================================== */

#ifndef XRT_SSH_PACKET_CODEC_H
#define XRT_SSH_PACKET_CODEC_H




#if defined(XSSH_FEATURE_PACKET_CODEC) && \
	!defined(XSSH_FEATURE_PACKET_AES_GCM)
	#error "XSSH_FEATURE_PACKET_CODEC requires plain and AES-GCM packet support"
#endif



#if defined(XSSH_FEATURE_PACKET_CODEC)

/* 收发方向独立切换；NEWKEYS 不会隐式重置 RFC 4253 序列号。 */
typedef enum xsshpacketmode {
	XSSH_PACKET_MODE_PLAIN = 0,
	XSSH_PACKET_MODE_AES_GCM = 1
} xsshpacketmode;



/* 尺寸探测只读取四字节 packet_length，不要求完整 packet 已经到达。 */
typedef struct xsshpacketneed {
	size_t WireSize;
	size_t PlainSize;
	uint32 PacketSize;
} xsshpacketneed;



/* Codec 不拥有收发缓冲；每个方向只能由一个执行流推进。 */
typedef struct xsshpacketcodec {
	xsshaesgcm ReadAesGcm;
	xsshaesgcm WriteAesGcm;
	uint32 ReadSequence;
	uint32 WriteSequence;
	uint32 MaxPacketSize;
	xsshpacketmode ReadMode;
	xsshpacketmode WriteMode;
	bool WritePending;
	uint32 Guard;
} xsshpacketcodec;



XRT_EXTERN_C_BEGIN



/* 初始化双向 plain codec；零上限使用 XSSH_PACKET_MAX_DEFAULT。 */
XRT_API xsshcode xrtSshPacketCodecInit(
	xsshpacketcodec* pCodec,
	uint32 iMaxPacketSize
);



/* 清除双向 cipher、nonce、序列号和状态标记。 */
XRT_API void xrtSshPacketCodecClear(xsshpacketcodec* pCodec);



/* 在收到 peer NEWKEYS 后原子切换读取方向，不修改读取序列号。 */
XRT_API xsshcode xrtSshPacketCodecSetReadAesGcm(
	xsshpacketcodec* pCodec,
	xbytesview Key,
	xbytesview InitialIV
);



/* 在发送本端 NEWKEYS 后原子切换写入方向，不修改写入序列号。 */
XRT_API xsshcode xrtSshPacketCodecSetWriteAesGcm(
	xsshpacketcodec* pCodec,
	xbytesview Key,
	xbytesview InitialIV
);



/* 仅供协商 strict-kex 后在对应方向 NEWKEYS 边界重置序列号。 */
XRT_API xsshcode xrtSshPacketCodecResetReadSequence(
	xsshpacketcodec* pCodec
);
XRT_API xsshcode xrtSshPacketCodecResetWriteSequence(
	xsshpacketcodec* pCodec
);



/* 探测 reader 当前 packet 的完整线长和所需解密工作区。 */
XRT_API xsshcode xrtSshPacketCodecInspect(
	const xsshpacketcodec* pCodec,
	const xsshreader* pReader,
	xsshpacketneed* pNeed
);



/* 按当前写方向精确计算 packet_length 和最终线路长度。 */
XRT_API xsshcode xrtSshPacketCodecWriteMeasure(
	const xsshpacketcodec* pCodec,
	size_t iPayloadSize,
	xsshpacketneed* pNeed
);



/*
	使用调用方 padding 源准备当前方向的最终线路包。
	成功后 writer 已推进，但 sequence 和 nonce 必须等可靠入队后再提交。
*/
XRT_API xsshcode xrtSshPacketCodecWritePrepareWithPadding(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData
);



/* 提交唯一一个已可靠入队的写事务，并推进 sequence 与 cipher nonce。 */
XRT_API xsshcode xrtSshPacketCodecWriteCommit(xsshpacketcodec* pCodec);



/* 放弃尚未发送的写事务；调用方负责丢弃已生成的线路字节。 */
XRT_API xsshcode xrtSshPacketCodecWriteAbort(xsshpacketcodec* pCodec);



/* 使用调用方 padding 源一次性准备并提交，适用于无需背压重试的路径。 */
XRT_API xsshcode xrtSshPacketCodecWriteWithPadding(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData
);



/* 从当前方向增量读取；AES-GCM 模式把明文写入调用方工作区。 */
XRT_API xsshcode xrtSshPacketCodecRead(
	xsshpacketcodec* pCodec,
	xsshreader* pReader,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_packet_codec_random.h */
/* ========================================================================== */

#ifndef XRT_SSH_PACKET_CODEC_RANDOM_H
#define XRT_SSH_PACKET_CODEC_RANDOM_H




#if defined(XSSH_FEATURE_PACKET_CODEC_RANDOM) && \
	(!defined(XSSH_FEATURE_PACKET_CODEC) || \
	 !defined(XSSH_FEATURE_PACKET_RANDOM))
	#error "XSSH_FEATURE_PACKET_CODEC_RANDOM requires packet codec and secure random padding"
#endif



#if defined(XSSH_FEATURE_PACKET_CODEC_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用 XRT 系统安全随机源准备写事务，可靠入队后必须另行提交。 */
XRT_API xsshcode xrtSshPacketCodecWritePrepare(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload
);



/* 使用 XRT 系统安全随机源写入 codec 当前方向。 */
XRT_API xsshcode xrtSshPacketCodecWrite(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_kexinit.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEXINIT_H
#define XRT_SSH_KEXINIT_H




#if defined(XSSH_FEATURE_KEXINIT) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_KEXINIT requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_KEXINIT)

#define XSSH_MSG_KEXINIT 20u
#define XSSH_KEX_COOKIE_SIZE 16u

#define XSSH_KEX_DEFAULT \
	"curve25519-sha256,curve25519-sha256@libssh.org"
#define XSSH_KEX_EXT_INFO_CLIENT "ext-info-c"
#define XSSH_KEX_EXT_INFO_SERVER "ext-info-s"
#define XSSH_KEX_STRICT_CLIENT "kex-strict-c"
#define XSSH_KEX_STRICT_SERVER "kex-strict-s"
#define XSSH_KEX_STRICT_CLIENT_PRE_STANDARD \
	"kex-strict-c-v00@openssh.com"
#define XSSH_KEX_STRICT_SERVER_PRE_STANDARD \
	"kex-strict-s-v00@openssh.com"
#define XSSH_KEX_CLIENT_INITIAL_DEFAULT \
	XSSH_KEX_DEFAULT "," XSSH_KEX_EXT_INFO_CLIENT "," \
	XSSH_KEX_STRICT_CLIENT "," XSSH_KEX_STRICT_CLIENT_PRE_STANDARD
#define XSSH_KEX_SERVER_INITIAL_DEFAULT \
	XSSH_KEX_DEFAULT "," XSSH_KEX_EXT_INFO_SERVER "," \
	XSSH_KEX_STRICT_SERVER "," XSSH_KEX_STRICT_SERVER_PRE_STANDARD
#define XSSH_HOSTKEY_DEFAULT "ssh-ed25519"
#define XSSH_CIPHER_DEFAULT \
	"aes128-gcm@openssh.com,aes256-gcm@openssh.com"
#define XSSH_MAC_DEFAULT "hmac-sha2-256,hmac-sha2-512"
#define XSSH_COMPRESSION_DEFAULT "none"



/* Endpoint 角色在整个 SSH 会话及其重协商期间保持不变。 */
typedef enum xsshrole {
	XSSH_ROLE_CLIENT = 0,
	XSSH_ROLE_SERVER = 1
} xsshrole;



/* KEXINIT 构建参数；Data 为 NULL 的列表使用 xssh 默认值。 */
typedef struct xsshkexinitconfig {
	xsshrole Role;
	bool Initial;
	xstrview KexAlgorithms;
	xstrview ServerHostKeyAlgorithms;
	xstrview EncryptionClientToServer;
	xstrview EncryptionServerToClient;
	xstrview MacClientToServer;
	xstrview MacServerToClient;
	xstrview CompressionClientToServer;
	xstrview CompressionServerToClient;
	xstrview LanguagesClientToServer;
	xstrview LanguagesServerToClient;
	bool FirstKexPacketFollows;
} xsshkexinitconfig;



/* KEXINIT 借用输入 payload；Cookie 和所有列表都不复制。 */
typedef struct xsshkexinit {
	xbytesview Cookie;
	xstrview KexAlgorithms;
	xstrview ServerHostKeyAlgorithms;
	xstrview EncryptionClientToServer;
	xstrview EncryptionServerToClient;
	xstrview MacClientToServer;
	xstrview MacServerToClient;
	xstrview CompressionClientToServer;
	xstrview CompressionServerToClient;
	xstrview LanguagesClientToServer;
	xstrview LanguagesServerToClient;
	bool FirstKexPacketFollows;
} xsshkexinit;



/* 协商结果中的视图借用 client KEXINIT payload。 */
typedef struct xsshkexnegotiation {
	xstrview KexAlgorithm;
	xstrview ServerHostKeyAlgorithm;
	xstrview CipherClientToServer;
	xstrview CipherServerToClient;
	xstrview MacClientToServer;
	xstrview MacServerToClient;
	xstrview CompressionClientToServer;
	xstrview CompressionServerToClient;
} xsshkexnegotiation;



/* 首次 KEX 的扩展方向与 strict-kex 协商结果。 */
typedef struct xsshkexfeatures {
	bool AcceptExtInfo;
	bool SendExtInfo;
	bool Strict;
} xsshkexfeatures;



XRT_EXTERN_C_BEGIN



/* 按 endpoint 角色和首次/重协商阶段初始化安全默认清单。 */
XRT_API bool xrtSshKexInitConfigInit(
	xsshkexinitconfig* pConfig,
	xsshrole Role,
	bool bInitial
);



/* 使用调用方提供的十六字节 cookie 构建完整 KEXINIT payload。 */
XRT_API xsshcode xrtSshKexInitWrite(
	xsshwriter* pWriter,
	xbytesview Cookie,
	const xsshkexinitconfig* pConfig
);



/* 解析一个完整 KEXINIT payload，并拒绝保留字段或尾随数据。 */
XRT_API xsshcode xrtSshKexInitRead(
	xbytesview Payload,
	xsshkexinit* pKexInit
);



/* 按 RFC 4253 客户端优先级协商双向算法。 */
XRT_API xsshcode xrtSshKexNegotiate(
	const xsshkexinit* pClient,
	const xsshkexinit* pServer,
	xsshkexnegotiation* pNegotiation
);



/* 根据双方 KEXINIT 判定 EXT_INFO 方向和 strict-kex 是否启用。 */
XRT_API xsshcode xrtSshKexFeatures(
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	xsshrole Role,
	bool bInitial,
	xsshkexfeatures* pFeatures
);



/* 判断已知 cipher 是否自带认证而不消费协商出的 MAC。 */
XRT_API bool xrtSshCipherIsAead(xstrview Cipher);



/* 判断 peer 的 first_kex_packet_follows 猜测包是否必须丢弃。 */
XRT_API xsshcode xrtSshKexGuessSkip(
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	bool* pSkip
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_kexinit_random.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEXINIT_RANDOM_H
#define XRT_SSH_KEXINIT_RANDOM_H




#if defined(XSSH_FEATURE_KEXINIT_RANDOM) && \
	(!defined(XSSH_FEATURE_KEXINIT) || !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XSSH_FEATURE_KEXINIT_RANDOM requires kexinit and random_secure"
#endif



#if defined(XSSH_FEATURE_KEXINIT_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用操作系统安全随机 cookie 和显式角色配置构建 KEXINIT payload。 */
XRT_API xsshcode xrtSshKexInitWriteSecure(
	xsshwriter* pWriter,
	const xsshkexinitconfig* pConfig
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_kex_ecdh.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEX_ECDH_H
#define XRT_SSH_KEX_ECDH_H




#if defined(XSSH_FEATURE_KEX_ECDH) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_KEX_ECDH requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_KEX_ECDH)

#define XSSH_MSG_KEX_ECDH_INIT 30u
#define XSSH_MSG_KEX_ECDH_REPLY 31u



/* ECDH init 视图借用完整 payload。 */
typedef struct xsshecdhinit {
	xbytesview ClientPublic;
} xsshecdhinit;



/* ECDH reply 视图借用完整 payload。 */
typedef struct xsshecdhreply {
	xbytesview ServerHostKey;
	xbytesview ServerPublic;
	xbytesview Signature;
} xsshecdhreply;



XRT_EXTERN_C_BEGIN



/* 构建 SSH_MSG_KEX_ECDH_INIT payload。 */
XRT_API xsshcode xrtSshEcdhInitWrite(
	xsshwriter* pWriter,
	xbytesview ClientPublic
);



/* 严格解析完整 SSH_MSG_KEX_ECDH_INIT payload。 */
XRT_API xsshcode xrtSshEcdhInitRead(
	xbytesview Payload,
	xsshecdhinit* pMessage
);



/* 构建 SSH_MSG_KEX_ECDH_REPLY payload。 */
XRT_API xsshcode xrtSshEcdhReplyWrite(
	xsshwriter* pWriter,
	xbytesview ServerHostKey,
	xbytesview ServerPublic,
	xbytesview Signature
);



/* 严格解析完整 SSH_MSG_KEX_ECDH_REPLY payload。 */
XRT_API xsshcode xrtSshEcdhReplyRead(
	xbytesview Payload,
	xsshecdhreply* pMessage
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_kex_sha256.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEX_SHA256_H
#define XRT_SSH_KEX_SHA256_H




#if defined(XSSH_FEATURE_KEX_SHA256) && \
	(!defined(XSSH_FEATURE_WIRE) || !defined(XRT_FEATURE_CRYPTO_SHA256))
	#error "XSSH_FEATURE_KEX_SHA256 requires wire and crypto_sha256"
#endif



#if defined(XSSH_FEATURE_KEX_SHA256)

#define XSSH_SHA256_SIZE 32u



/* Curve25519/ECDH exchange hash 输入均为未带外层 packet framing 的协议值。 */
typedef struct xsshkexhashsha256 {
	xbytesview ClientVersion;
	xbytesview ServerVersion;
	xbytesview ClientKexInit;
	xbytesview ServerKexInit;
	xbytesview ServerHostKey;
	xbytesview ClientEphemeral;
	xbytesview ServerEphemeral;
	xbytesview SharedSecret;
} xsshkexhashsha256;



XRT_EXTERN_C_BEGIN



/* 按 RFC 5656/8731 顺序计算 Curve25519 SHA-256 exchange hash。 */
XRT_API xsshcode xrtSshKexHashSha256(
	const xsshkexhashsha256* pInput,
	void* pHash
);



/* 按 RFC 4253 扩展 A-F 类密钥材料，输出可大于单个摘要。 */
XRT_API xsshcode xrtSshKexDeriveSha256(
	void* pOutput,
	size_t iOutputSize,
	xbytesview SharedSecret,
	const void* pExchangeHash,
	const void* pSessionId,
	uint8 iLetter
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_kex_curve25519.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEX_CURVE25519_H
#define XRT_SSH_KEX_CURVE25519_H




#if defined(XSSH_FEATURE_KEX_CURVE25519) && \
	(!defined(XSSH_FEATURE_WIRE) || !defined(XRT_FEATURE_CRYPTO_X25519))
	#error "XSSH_FEATURE_KEX_CURVE25519 requires wire and crypto_x25519"
#endif



#if defined(XSSH_FEATURE_KEX_CURVE25519)

#define XSSH_CURVE25519_PRIVATE_SIZE 32u
#define XSSH_CURVE25519_PUBLIC_SIZE 32u
#define XSSH_CURVE25519_SHARED_SIZE 32u

XRT_EXTERN_C_BEGIN



/* 从固定长度私钥导出 Curve25519 SSH 临时公钥。 */
XRT_API xsshcode xrtSshCurve25519Public(
	const void* pPrivate,
	void* pPublic
);



/* 计算共享秘密并拒绝低阶公钥产生的全零结果。 */
XRT_API xsshcode xrtSshCurve25519Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_kex_curve25519_random.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEX_CURVE25519_RANDOM_H
#define XRT_SSH_KEX_CURVE25519_RANDOM_H




#if defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM) && \
	(!defined(XSSH_FEATURE_KEX_CURVE25519) || \
	 !defined(XRT_FEATURE_CRYPTO_X25519_KEYPAIR))
	#error "XSSH_FEATURE_KEX_CURVE25519_RANDOM requires curve25519 and X25519 keypair"
#endif



#if defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用操作系统安全随机源生成 Curve25519 SSH 临时密钥对。 */
XRT_API xsshcode xrtSshCurve25519KeyPair(
	void* pPrivate,
	void* pPublic
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_hostkey.h */
/* ========================================================================== */

#ifndef XRT_SSH_HOSTKEY_H
#define XRT_SSH_HOSTKEY_H




#if defined(XSSH_FEATURE_HOSTKEY) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_HOSTKEY requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_HOSTKEY)

#define XSSH_ED25519_PUBLIC_SIZE 32u
#define XSSH_ED25519_SIGNATURE_SIZE 64u
#define XSSH_HOSTKEY_ED25519 "ssh-ed25519"



/* 公钥视图借用完整 key blob；Parameters 保留算法字段后的原始编码。 */
typedef struct xsshpublickey {
	xstrview Algorithm;
	xbytesview Parameters;
} xsshpublickey;



/* 签名视图借用完整 signature blob。 */
typedef struct xsshsignature {
	xstrview Algorithm;
	xbytesview Signature;
} xsshsignature;



XRT_EXTERN_C_BEGIN



/* 读取公钥算法和其余算法专用字段，不解释 Parameters。 */
XRT_API xsshcode xrtSshPublicKeyRead(
	xbytesview Blob,
	xsshpublickey* pPublicKey
);



/* 严格读取由算法名和签名字节组成的 SSH signature blob。 */
XRT_API xsshcode xrtSshSignatureRead(
	xbytesview Blob,
	xsshsignature* pSignature
);



/* 写入算法无关的 SSH signature blob。 */
XRT_API xsshcode xrtSshSignatureWrite(
	xsshwriter* pWriter,
	xstrview Algorithm,
	xbytesview Signature
);



/* 严格读取 ssh-ed25519 公钥并返回 32 字节借用视图。 */
XRT_API xsshcode xrtSshEd25519PublicKeyRead(
	xbytesview Blob,
	xbytesview* pPublicKey
);



/* 写入规范 ssh-ed25519 公钥 blob。 */
XRT_API xsshcode xrtSshEd25519PublicKeyWrite(
	xsshwriter* pWriter,
	xbytesview PublicKey
);



/* 严格读取 ssh-ed25519 签名并返回 64 字节借用视图。 */
XRT_API xsshcode xrtSshEd25519SignatureRead(
	xbytesview Blob,
	xbytesview* pSignature
);



/* 写入规范 ssh-ed25519 signature blob。 */
XRT_API xsshcode xrtSshEd25519SignatureWrite(
	xsshwriter* pWriter,
	xbytesview Signature
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_hostkey_ed25519.h */
/* ========================================================================== */

#ifndef XRT_SSH_HOSTKEY_ED25519_H
#define XRT_SSH_HOSTKEY_ED25519_H




#if defined(XSSH_FEATURE_HOSTKEY_ED25519) && \
	(!defined(XSSH_FEATURE_HOSTKEY) || \
	 !defined(XRT_FEATURE_CRYPTO_ED25519_VERIFY))
	#error "XSSH_FEATURE_HOSTKEY_ED25519 requires hostkey and crypto_ed25519_verify"
#endif



#if defined(XSSH_FEATURE_HOSTKEY_ED25519)

XRT_EXTERN_C_BEGIN



/* 验证 ssh-ed25519 主机密钥对 Message 的签名。 */
XRT_API xsshcode xrtSshEd25519HostKeyVerify(
	xbytesview PublicKeyBlob,
	xbytesview SignatureBlob,
	xbytesview Message
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_key_text.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEY_TEXT_H
#define XRT_SSH_KEY_TEXT_H




#if defined(XSSH_FEATURE_KEY_TEXT) && \
	(!defined(XSSH_FEATURE_HOSTKEY) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XSSH_FEATURE_KEY_TEXT requires SSH host-key and XRT Base64 support"
#endif



#if defined(XSSH_FEATURE_KEY_TEXT)

/* OpenSSH 公钥文本行视图借用原始行，BlobSize 给出解码缓冲需求。 */
typedef struct xsshopensshkeyline {
	xstrview Options;
	xstrview Algorithm;
	xstrview Base64;
	xstrview Comment;
	size_t BlobSize;
} xsshopensshkeyline;



XRT_EXTERN_C_BEGIN



/*
	解析 OpenSSH public-key/authorized_keys 行。
	识别带引号与转义的 options，但不解释具体 option；允许任意 SSH 算法名。
*/
XRT_API xsshcode xrtSshPublicKeyLineRead(
	xstrview Line,
	xsshopensshkeyline* pKeyLine
);



/*
	把文本行的 Base64 解码到调用方缓冲，并校验 blob 内算法与文本算法一致。
	协议校验失败时 pPublicKey 不变；pBlob 是解码工作区，成功解码后可能已写入。
*/
XRT_API xsshcode xrtSshPublicKeyLineDecode(
	const xsshopensshkeyline* pKeyLine,
	void* pBlob,
	size_t iCapacity,
	xsshpublickey* pPublicKey
);



/* 不分配解码缓冲，直接比较文本行与完整原始公钥 blob。 */
XRT_API xsshcode xrtSshPublicKeyLineMatch(
	const xsshopensshkeyline* pKeyLine,
	xbytesview Blob,
	bool* pMatch
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_known_host.h */
/* ========================================================================== */

#ifndef XRT_SSH_KNOWN_HOST_H
#define XRT_SSH_KNOWN_HOST_H




#if defined(XSSH_FEATURE_KNOWN_HOST) && \
	!defined(XSSH_FEATURE_KEY_TEXT)
	#error "XSSH_FEATURE_KNOWN_HOST requires XSSH_FEATURE_KEY_TEXT"
#endif



#if defined(XSSH_FEATURE_KNOWN_HOST)

#define XSSH_DEFAULT_PORT 22u
#define XSSH_KNOWN_HOST_CERT_AUTHORITY "@cert-authority"
#define XSSH_KNOWN_HOST_REVOKED "@revoked"



/* 未知 marker 被保留，信任策略可以显式拒绝或扩展。 */
typedef enum xsshknownhostmarker {
	XSSH_KNOWN_HOST_MARKER_NONE = 0,
	XSSH_KNOWN_HOST_MARKER_CERT_AUTHORITY = 1,
	XSSH_KNOWN_HOST_MARKER_REVOKED = 2,
	XSSH_KNOWN_HOST_MARKER_UNKNOWN = 3
} xsshknownhostmarker;



/* 否定 pattern 优先于同一列表中的任意正向匹配。 */
typedef enum xsshknownhostmatch {
	XSSH_KNOWN_HOST_NO_MATCH = 0,
	XSSH_KNOWN_HOST_MATCH = 1,
	XSSH_KNOWN_HOST_NEGATED = 2
} xsshknownhostmatch;



/* known_hosts 行视图借用原始文本，密钥 blob 由调用方按 BlobSize 解码。 */
typedef struct xsshknownhostline {
	xstrview Marker;
	xsshknownhostmarker MarkerKind;
	xstrview Hosts;
	xstrview Algorithm;
	xstrview Base64;
	xstrview Comment;
	size_t BlobSize;
	bool Hashed;
} xsshknownhostline;



XRT_EXTERN_C_BEGIN



/* 解析 marker、host patterns、算法、Base64 和注释，不执行信任决策。 */
XRT_API xsshcode xrtSshKnownHostLineRead(
	xstrview Line,
	xsshknownhostline* pKnownHost
);



/* 解码并验证 known_hosts 行中的算法无关公钥 blob。 */
XRT_API xsshcode xrtSshKnownHostLineDecode(
	const xsshknownhostline* pKnownHost,
	void* pBlob,
	size_t iCapacity,
	xsshpublickey* pPublicKey
);



/* 不分配解码缓冲，直接比较 known_hosts 行与完整原始公钥 blob。 */
XRT_API xsshcode xrtSshKnownHostLineKeyMatch(
	const xsshknownhostline* pKnownHost,
	xbytesview Blob,
	bool* pMatch
);



/*
	按 OpenSSH 规则匹配明文 host pattern 列表。
	Host 传未加方括号的主机名或地址；非 22 端口按 [host]:port 匹配。
*/
XRT_API xsshcode xrtSshKnownHostPatternsMatch(
	xstrview Patterns,
	xstrview Host,
	uint32 iPort,
	xsshknownhostmatch* pMatch
);



/* 对已解析行执行明文 host pattern 匹配；hashed 行返回不支持。 */
XRT_API xsshcode xrtSshKnownHostLineMatch(
	const xsshknownhostline* pKnownHost,
	xstrview Host,
	uint32 iPort,
	xsshknownhostmatch* pMatch
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_known_host_hash.h */
/* ========================================================================== */

#ifndef XRT_SSH_KNOWN_HOST_HASH_H
#define XRT_SSH_KNOWN_HOST_HASH_H




#if defined(XSSH_FEATURE_KNOWN_HOST_HASH) && \
	(!defined(XSSH_FEATURE_KNOWN_HOST) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA1))
	#error "XSSH_FEATURE_KNOWN_HOST_HASH requires known-host and XRT SHA-1"
#endif



#if defined(XSSH_FEATURE_KNOWN_HOST_HASH)

#define XSSH_KNOWN_HOST_HASH_SIZE 20u



XRT_EXTERN_C_BEGIN



/* 计算 OpenSSH hashed-host 使用的 HMAC-SHA1；Salt 必须为 20 字节。 */
XRT_API xsshcode xrtSshKnownHostHash(
	xstrview Host,
	uint32 iPort,
	xbytesview Salt,
	void* pHash
);



/*
	生成 |1|salt|hash 文本；查询模式允许 sOutput 为 NULL、容量为零。
	实际写入容量必须包含末尾零字节，pOutputSize 不包含末尾零字节。
*/
XRT_API xsshcode xrtSshKnownHostHashWrite(
	xstrview Host,
	uint32 iPort,
	xbytesview Salt,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 常量时间校验一个完整 OpenSSH |1| hashed-host token。 */
XRT_API xsshcode xrtSshKnownHostHashMatch(
	xstrview HashedHost,
	xstrview Host,
	uint32 iPort,
	bool* pMatch
);



/* 对已解析且 Hashed 的 known_hosts 行执行便利匹配。 */
XRT_API xsshcode xrtSshKnownHostLineHashMatch(
	const xsshknownhostline* pKnownHost,
	xstrview Host,
	uint32 iPort,
	bool* pMatch
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_known_host_db.h */
/* ========================================================================== */

#ifndef XRT_SSH_KNOWN_HOST_DB_H
#define XRT_SSH_KNOWN_HOST_DB_H




#if defined(XSSH_FEATURE_KNOWN_HOST_DB) && \
	!defined(XSSH_FEATURE_KNOWN_HOST_HASH)
	#error "XSSH_FEATURE_KNOWN_HOST_DB requires SSH known-host hash support"
#endif



#if defined(XSSH_FEATURE_KNOWN_HOST_DB)

/* 严格模式把坏行和未知 marker 作为可定位的 INVALID 结果。 */
typedef enum xsshknownhostdbflag {
	XSSH_KNOWN_HOST_DB_STRICT = UINT32_C(0x00000001)
} xsshknownhostdbflag;



/* CA 表示存在候选 CA，仍需上层验证主机证书，不能直接视为已信任。 */
typedef enum xsshknownhosttrust {
	XSSH_KNOWN_HOST_TRUST_NEW = 0,
	XSSH_KNOWN_HOST_TRUST_MATCH = 1,
	XSSH_KNOWN_HOST_TRUST_CHANGED = 2,
	XSSH_KNOWN_HOST_TRUST_REVOKED = 3,
	XSSH_KNOWN_HOST_TRUST_CERT_AUTHORITY = 4,
	XSSH_KNOWN_HOST_TRUST_INVALID = 5
} xsshknownhosttrust;



/* 数据库游标借用完整文本；Position 和 LineNumber 只由迭代函数推进。 */
typedef struct xsshknownhostdb {
	xstrview Source;
	size_t Position;
	size_t LineNumber;
	uint32 Flags;
} xsshknownhostdb;



/* 条目及其字段都借用数据库文本；Valid 为 false 时仅 Source 和行号有效。 */
typedef struct xsshknownhostentry {
	xstrview Source;
	size_t LineNumber;
	bool Valid;
	xsshknownhostline KnownHost;
} xsshknownhostentry;



/* 常见信任判定返回决定性条目；NEW 时 Entry 清零。 */
typedef struct xsshknownhostcheck {
	xsshknownhosttrust Trust;
	xsshknownhostentry Entry;
} xsshknownhostcheck;



XRT_EXTERN_C_BEGIN



/* 初始化无分配 known_hosts 文本游标；空文本允许 Data 为 NULL。 */
XRT_API xsshcode xrtSshKnownHostDbInit(
	xsshknownhostdb* pDatabase,
	xstrview Source,
	uint32 iFlags
);



/*
	返回下一条记录；注释和空行总被跳过，非严格模式也跳过坏行。
	到达文本末尾返回 XSSH_NEED_MORE，并保持 pEntry 不变。
*/
XRT_API xsshcode xrtSshKnownHostDbNext(
	xsshknownhostdb* pDatabase,
	xsshknownhostentry* pEntry
);



/*
	用完整原始 host-key blob 扫描明文和 |1| 哈希记录，不分配密钥缓冲。
	REVOKED 优先于 MATCH，随后依次为 CA、CHANGED 和 NEW；CHANGED 跨算法生效。
*/
XRT_API xsshcode xrtSshKnownHostDbCheck(
	xstrview Source,
	xstrview Host,
	uint32 iPort,
	xbytesview KeyBlob,
	uint32 iFlags,
	xsshknownhostcheck* pCheck
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_fingerprint.h */
/* ========================================================================== */

#ifndef XRT_SSH_FINGERPRINT_H
#define XRT_SSH_FINGERPRINT_H




#if defined(XSSH_FEATURE_FINGERPRINT) && \
	(!defined(XSSH_FEATURE_WIRE) || \
	 !defined(XRT_FEATURE_CODEC_BASE64) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256))
	#error "XSSH_FEATURE_FINGERPRINT requires wire, Base64 and SHA-256"
#endif



#if defined(XSSH_FEATURE_FINGERPRINT)

#define XSSH_FINGERPRINT_SHA256_SIZE 32u



XRT_EXTERN_C_BEGIN



/* 计算完整 host-key blob 的 32 字节 SHA-256 摘要。 */
XRT_API xsshcode xrtSshHostKeyDigestSha256(
	xbytesview HostKey,
	void* pDigest
);



/*
	输出 OpenSSH 风格 SHA256:<base64-no-padding> 指纹。
	查询模式允许 sOutput 为 NULL、容量为零；实际容量必须包含末尾零字节。
*/
XRT_API xsshcode xrtSshHostKeyFingerprintSha256(
	xbytesview HostKey,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_private_key.h */
/* ========================================================================== */

#ifndef XRT_SSH_PRIVATE_KEY_H
#define XRT_SSH_PRIVATE_KEY_H




#if defined(XSSH_FEATURE_PRIVATE_KEY) && !defined(XSSH_FEATURE_HOSTKEY)
	#error "XSSH_FEATURE_PRIVATE_KEY requires SSH host-key support"
#endif



#if defined(XSSH_FEATURE_PRIVATE_KEY)

#define XSSH_PRIVATE_KEY_MAGIC "openssh-key-v1"
#define XSSH_PRIVATE_KEY_MAGIC_SIZE 15u
#define XSSH_PRIVATE_KEY_NONE "none"



/* 容器及全部字段借用完整 openssh-key-v1 二进制输入。 */
typedef struct xsshopensshprivatekey {
	xbytesview Blob;
	xstrview Cipher;
	xstrview Kdf;
	xbytesview KdfOptions;
	uint32 KeyCount;
	xbytesview PublicKeys;
	xbytesview PrivateList;
} xsshopensshprivatekey;



/* 公钥游标借用容器中的连续 SSH string 序列。 */
typedef struct xsshprivatekeypublics {
	xsshreader Reader;
	uint32 Remaining;
} xsshprivatekeypublics;



XRT_EXTERN_C_BEGIN



/* 解析二进制 openssh-key-v1 容器，并预验证全部公开公钥 blob。 */
XRT_API xsshcode xrtSshPrivateKeyRead(
	xbytesview Blob,
	xsshopensshprivatekey* pPrivateKey
);



/* 判断容器是否需要外部 cipher/KDF 层先解密 PrivateList。 */
XRT_API xsshcode xrtSshPrivateKeyIsEncrypted(
	const xsshopensshprivatekey* pPrivateKey,
	bool* pEncrypted
);



/* 初始化容器公开公钥游标。 */
XRT_API xsshcode xrtSshPrivateKeyPublicsInit(
	const xsshopensshprivatekey* pPrivateKey,
	xsshprivatekeypublics* pPublics
);



/* 返回下一把公开公钥 blob；遍历完成返回 XSSH_NEED_MORE。 */
XRT_API xsshcode xrtSshPrivateKeyPublicsNext(
	xsshprivatekeypublics* pPublics,
	xbytesview* pPublicKey
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_private_key_pem.h */
/* ========================================================================== */

#ifndef XRT_SSH_PRIVATE_KEY_PEM_H
#define XRT_SSH_PRIVATE_KEY_PEM_H




#if defined(XSSH_FEATURE_PRIVATE_KEY_PEM) && \
	(!defined(XSSH_FEATURE_PRIVATE_KEY) || !defined(XRT_FEATURE_PEM))
	#error "XSSH_FEATURE_PRIVATE_KEY_PEM requires SSH private-key and XRT PEM"
#endif



#if defined(XSSH_FEATURE_PRIVATE_KEY_PEM)

#define XSSH_PRIVATE_KEY_PEM_LABEL "OPENSSH PRIVATE KEY"



XRT_EXTERN_C_BEGIN



/*
	查找并解码 OpenSSH 私钥 PEM 到调用方缓冲，再解析二进制容器。
	查询模式要求 pBinary 和 pPrivateKey 为 NULL、容量为零，只返回精确二进制长度。
*/
XRT_API xsshcode xrtSshPrivateKeyPemRead(
	xstrview Text,
	void* pBinary,
	size_t iCapacity,
	size_t* pBinarySize,
	xsshopensshprivatekey* pPrivateKey
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_private_key_ed25519.h */
/* ========================================================================== */

#ifndef XRT_SSH_PRIVATE_KEY_ED25519_H
#define XRT_SSH_PRIVATE_KEY_ED25519_H




#if defined(XSSH_FEATURE_PRIVATE_KEY_ED25519) && \
	(!defined(XSSH_FEATURE_PRIVATE_KEY) || \
	 !defined(XRT_FEATURE_CRYPTO_ED25519_SIGN))
	#error "XSSH_FEATURE_PRIVATE_KEY_ED25519 requires private-key and Ed25519 signing"
#endif



#if defined(XSSH_FEATURE_PRIVATE_KEY_ED25519)

/* 身份借用解码后的秘密缓冲；释放前由调用方不可消除地清零该缓冲。 */
typedef struct xsshed25519identity {
	xbytesview PublicKeyBlob;
	xbytesview Seed;
	xbytesview PublicKey;
	xbytesview Comment;
} xsshed25519identity;



XRT_EXTERN_C_BEGIN



/* 严格读取未加密、单密钥 openssh-key-v1 Ed25519 身份。 */
XRT_API xsshcode xrtSshPrivateKeyEd25519Read(
	const xsshopensshprivatekey* pPrivateKey,
	xsshed25519identity* pIdentity
);



/* 使用借用 seed 签署任意二进制消息，输出固定 64 字节原始签名。 */
XRT_API xsshcode xrtSshPrivateKeyEd25519Sign(
	const xsshed25519identity* pIdentity,
	xbytesview Message,
	void* pSignature
);



/* 签名并直接向最终 writer 写入 ssh-ed25519 signature blob。 */
XRT_API xsshcode xrtSshPrivateKeyEd25519SignatureWrite(
	xsshwriter* pWriter,
	const xsshed25519identity* pIdentity,
	xbytesview Message
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_transport_message.h */
/* ========================================================================== */

#ifndef XRT_SSH_TRANSPORT_MESSAGE_H
#define XRT_SSH_TRANSPORT_MESSAGE_H




#if defined(XSSH_FEATURE_TRANSPORT_MESSAGE) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_TRANSPORT_MESSAGE requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_MESSAGE)

#define XSSH_MSG_DISCONNECT 1u
#define XSSH_MSG_IGNORE 2u
#define XSSH_MSG_UNIMPLEMENTED 3u
#define XSSH_MSG_DEBUG 4u
#define XSSH_MSG_SERVICE_REQUEST 5u
#define XSSH_MSG_SERVICE_ACCEPT 6u
#define XSSH_MSG_EXT_INFO 7u
#define XSSH_MSG_NEWCOMPRESS 8u
#define XSSH_MSG_NEWKEYS 21u



/* RFC 4253 disconnect reason code。 */
typedef enum xsshdisconnectreason {
	XSSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT = 1,
	XSSH_DISCONNECT_PROTOCOL_ERROR = 2,
	XSSH_DISCONNECT_KEY_EXCHANGE_FAILED = 3,
	XSSH_DISCONNECT_RESERVED = 4,
	XSSH_DISCONNECT_MAC_ERROR = 5,
	XSSH_DISCONNECT_COMPRESSION_ERROR = 6,
	XSSH_DISCONNECT_SERVICE_NOT_AVAILABLE = 7,
	XSSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED = 8,
	XSSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE = 9,
	XSSH_DISCONNECT_CONNECTION_LOST = 10,
	XSSH_DISCONNECT_BY_APPLICATION = 11,
	XSSH_DISCONNECT_TOO_MANY_CONNECTIONS = 12,
	XSSH_DISCONNECT_AUTH_CANCELLED_BY_USER = 13,
	XSSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE = 14,
	XSSH_DISCONNECT_ILLEGAL_USER_NAME = 15
} xsshdisconnectreason;



/* Disconnect 视图借用完整 payload。 */
typedef struct xsshdisconnect {
	uint32 Reason;
	xstrview Description;
	xstrview Language;
} xsshdisconnect;



/* Ignore 视图借用完整 payload。 */
typedef struct xsshignore {
	xbytesview Data;
} xsshignore;



/* Debug 视图借用完整 payload。 */
typedef struct xsshdebug {
	bool AlwaysDisplay;
	xstrview Message;
	xstrview Language;
} xsshdebug;



/* Service 视图借用完整 payload。 */
typedef struct xsshservice {
	xstrview Name;
} xsshservice;



/* 单个 EXT_INFO 扩展视图借用完整 payload。 */
typedef struct xsshextension {
	xstrview Name;
	xbytesview Value;
} xsshextension;



/* EXT_INFO 迭代器已在初始化时严格验证全部字段。 */
typedef struct xsshextinfo {
	uint32 Count;
	uint32 Index;
	xsshreader Reader;
} xsshextinfo;



XRT_EXTERN_C_BEGIN



/* 读取 payload 的消息号，不推进任何外部状态。 */
XRT_API xsshcode xrtSshMessageType(xbytesview Payload, uint8* pMessage);



/* 写入或严格读取无字段 SSH_MSG_NEWKEYS。 */
XRT_API xsshcode xrtSshNewKeysWrite(xsshwriter* pWriter);
XRT_API xsshcode xrtSshNewKeysRead(xbytesview Payload);



/* 写入或严格读取 SSH_MSG_DISCONNECT。 */
XRT_API xsshcode xrtSshDisconnectWrite(
	xsshwriter* pWriter,
	uint32 iReason,
	xstrview Description,
	xstrview Language
);
XRT_API xsshcode xrtSshDisconnectRead(
	xbytesview Payload,
	xsshdisconnect* pMessage
);



/* 写入或严格读取 SSH_MSG_IGNORE。 */
XRT_API xsshcode xrtSshIgnoreWrite(
	xsshwriter* pWriter,
	xbytesview Data
);
XRT_API xsshcode xrtSshIgnoreRead(
	xbytesview Payload,
	xsshignore* pMessage
);



/* 写入或严格读取 SSH_MSG_UNIMPLEMENTED 的拒绝序列号。 */
XRT_API xsshcode xrtSshUnimplementedWrite(
	xsshwriter* pWriter,
	uint32 iSequence
);
XRT_API xsshcode xrtSshUnimplementedRead(
	xbytesview Payload,
	uint32* pSequence
);



/* 写入或严格读取 SSH_MSG_DEBUG。 */
XRT_API xsshcode xrtSshDebugWrite(
	xsshwriter* pWriter,
	bool bAlwaysDisplay,
	xstrview Message,
	xstrview Language
);
XRT_API xsshcode xrtSshDebugRead(
	xbytesview Payload,
	xsshdebug* pMessage
);



/* 写入或严格读取 service request/accept。 */
XRT_API xsshcode xrtSshServiceRequestWrite(
	xsshwriter* pWriter,
	xstrview Service
);
XRT_API xsshcode xrtSshServiceRequestRead(
	xbytesview Payload,
	xsshservice* pMessage
);
XRT_API xsshcode xrtSshServiceAcceptWrite(
	xsshwriter* pWriter,
	xstrview Service
);
XRT_API xsshcode xrtSshServiceAcceptRead(
	xbytesview Payload,
	xsshservice* pMessage
);



/* 写入、验证并迭代 RFC 8308 SSH_MSG_EXT_INFO。 */
XRT_API xsshcode xrtSshExtInfoWrite(
	xsshwriter* pWriter,
	const xsshextension* pExtensions,
	size_t iCount
);
XRT_API xsshcode xrtSshExtInfoRead(
	xbytesview Payload,
	xsshextinfo* pExtInfo
);
XRT_API bool xrtSshExtInfoNext(
	xsshextinfo* pExtInfo,
	xsshextension* pExtension
);



/* 写入或严格读取 RFC 8308 SSH_MSG_NEWCOMPRESS。 */
XRT_API xsshcode xrtSshNewCompressWrite(xsshwriter* pWriter);
XRT_API xsshcode xrtSshNewCompressRead(xbytesview Payload);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_transport_rekey.h */
/* ========================================================================== */

#ifndef XRT_SSH_TRANSPORT_REKEY_H
#define XRT_SSH_TRANSPORT_REKEY_H




#if defined(XSSH_FEATURE_TRANSPORT_REKEY) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_TRANSPORT_REKEY requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_REKEY)

#define XSSH_REKEY_DEFAULT_BYTE_LIMIT UINT64_C(1073741824)
#define XSSH_REKEY_DEFAULT_SEND_PACKET_LIMIT UINT64_C(2147483648)
#define XSSH_REKEY_DEFAULT_RECEIVE_PACKET_LIMIT UINT64_C(2147483648)
#define XSSH_REKEY_DEFAULT_BLOCK_LIMIT UINT64_C(4294967296)
#define XSSH_REKEY_DEFAULT_TIME_LIMIT_MS UINT64_C(3600000)
#define XSSH_REKEY_HARD_PACKET_LIMIT UINT64_C(4294967296)



/* Rekey 决策按严重程度单调递增。 */
typedef enum xsshrekeydecision {
	XSSH_REKEY_NONE = 0,
	XSSH_REKEY_RECOMMENDED = 1,
	XSSH_REKEY_REQUIRED = 2
} xsshrekeydecision;



/* 零值软阈值表示禁用；HardPacketLimit 必须非零且不超过协议硬上限。 */
typedef struct xsshrekeypolicy {
	uint64 ByteLimit;
	uint64 SendPacketLimit;
	uint64 ReceivePacketLimit;
	uint64 BlockLimit;
	uint64 TimeLimitMs;
	uint64 HardPacketLimit;
} xsshrekeypolicy;



/* 单方向计数器使用饱和加法，永不回绕。 */
typedef struct xsshrekeycounter {
	uint64 Bytes;
	uint64 Packets;
	uint64 Blocks;
} xsshrekeycounter;



/* Rekey 状态不拥有时钟；时间由 transport 的单调时钟显式传入。 */
typedef struct xsshrekeystate {
	xsshrekeypolicy Policy;
	xsshrekeycounter Sent;
	xsshrekeycounter Received;
	double SendStartedTimer;
	double ReceiveStartedTimer;
	bool Requested;
} xsshrekeystate;



/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 初始化 RFC 4253/4344 对应的保守默认策略。 */
XRT_API void xrtSshRekeyPolicyInit(xsshrekeypolicy* pPolicy);



/* 校验策略并初始化一代密钥的计数状态。 */
XRT_API bool xrtSshRekeyInit(
	xsshrekeystate* pState,
	const xsshrekeypolicy* pPolicy,
	double Timer
);



/* 同时清空双向计数并开始新一代，适用于两方向具有同一提交边界的驱动。 */
XRT_API bool xrtSshRekeyReset(
	xsshrekeystate* pState,
	double Timer
);



/* 写密钥生效后只清空发送方向计数和时间。 */
XRT_API bool xrtSshRekeyResetSend(
	xsshrekeystate* pState,
	double Timer
);



/* 读密钥生效后只清空接收方向计数和时间。 */
XRT_API bool xrtSshRekeyResetReceive(
	xsshrekeystate* pState,
	double Timer
);



/* 双向新密钥均已生效后结束主动 rekey 请求，不修改新代计数。 */
XRT_API bool xrtSshRekeyComplete(xsshrekeystate* pState);



/* 请求策略阈值之外的主动 rekey。 */
XRT_API bool xrtSshRekeyRequest(xsshrekeystate* pState);



/* 查询当前计数、主动请求和时间阈值产生的决策。 */
XRT_API xsshcode xrtSshRekeyCheck(
	const xsshrekeystate* pState,
	double Timer,
	xsshrekeydecision* pDecision
);



/* 在发送前预留下一包；REQUIRED 时状态不变且该包不得发送。 */
XRT_API xsshcode xrtSshRekeyReserveSend(
	xsshrekeystate* pState,
	uint64 iWireBytes,
	uint64 iCipherBlocks,
	double Timer,
	xsshrekeydecision* pDecision
);



/* 在认证接收包前预留下一包；REQUIRED 时状态不变且该包不得接受。 */
XRT_API xsshcode xrtSshRekeyReserveReceive(
	xsshrekeystate* pState,
	uint64 iWireBytes,
	uint64 iCipherBlocks,
	double Timer,
	xsshrekeydecision* pDecision
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_transport_state.h */
/* ========================================================================== */

#ifndef XRT_SSH_TRANSPORT_STATE_H
#define XRT_SSH_TRANSPORT_STATE_H




#if defined(XSSH_FEATURE_TRANSPORT_STATE) && \
	(!defined(XSSH_FEATURE_KEXINIT) || \
	 !defined(XSSH_FEATURE_TRANSPORT_MESSAGE))
	#error "XSSH_FEATURE_TRANSPORT_STATE requires KEXINIT and transport messages"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_STATE)

#define XSSH_KEX_METHOD_MIN 30u
#define XSSH_KEX_METHOD_MAX 49u
#define XSSH_KEX_METHOD_COUNT 20u

#if !defined(XSSH_MSG_USERAUTH_SUCCESS)
	#define XSSH_MSG_USERAUTH_SUCCESS 52u
#endif



/* Transport 阶段不表示网络状态，只表示已经可靠提交的 SSH 协议状态。 */
typedef enum xsshtransportphase {
	XSSH_TRANSPORT_IDENTIFICATION = 0,
	XSSH_TRANSPORT_KEY_EXCHANGE = 1,
	XSSH_TRANSPORT_OPEN = 2,
	XSSH_TRANSPORT_CLOSING = 3,
	XSSH_TRANSPORT_CLOSED = 4
} xsshtransportphase;



/* LOCAL 表示本端发送方向，PEER 表示认证完成的对端接收方向。 */
typedef enum xsshtransportdirection {
	XSSH_TRANSPORT_LOCAL = 0,
	XSSH_TRANSPORT_PEER = 1
} xsshtransportdirection;



/* NEWKEYS 提交动作由调用方在对应 packet codec 方向执行。 */
typedef enum xsshtransportaction {
	XSSH_TRANSPORT_ACTION_NONE = 0,
	XSSH_TRANSPORT_ACTION_ACTIVATE_KEYS = 1,
	XSSH_TRANSPORT_ACTION_RESET_SEQUENCE = 2,
	XSSH_TRANSPORT_ACTION_KEX_COMPLETE = 4
} xsshtransportaction;



/* 每个 KEX 方法消息的额度为精确次数，零表示该方向禁止。 */
typedef struct xsshtransportkexrules {
	uint8 Local[XSSH_KEX_METHOD_COUNT];
	uint8 Peer[XSSH_KEX_METHOD_COUNT];
	uint32 Guard;
} xsshtransportkexrules;



/*
 * 状态对象不拥有 KEXINIT payload、密钥、时钟、socket 或缓冲。
 * 单个对象只能由一个执行流推进，跨线程串行化由调用方负责。
 */
typedef struct xsshtransportstate {
	uint8 LocalKexRemaining[XSSH_KEX_METHOD_COUNT];
	uint8 PeerKexRemaining[XSSH_KEX_METHOD_COUNT];
	uint64 LocalPackets;
	uint64 PeerPackets;
	uint64 LocalKexInitOrdinal;
	uint64 PeerKexInitOrdinal;
	uint64 KexCount;
	xsshrole Role;
	xsshtransportphase Phase;
	uint8 LocalGuessMessage;
	uint8 PeerGuessMessage;
	bool LocalIdentification;
	bool PeerIdentification;
	bool LocalKexInit;
	bool PeerKexInit;
	bool LocalNewKeys;
	bool PeerNewKeys;
	bool KexConfigured;
	bool Strict;
	bool AcceptExtInfo;
	bool SendExtInfo;
	bool LocalGuessExpected;
	bool PeerGuessExpected;
	bool LocalGuessSeen;
	bool PeerGuessSeen;
	bool LocalGuessSkip;
	bool PeerGuessSkip;
	bool LocalStrictViolation;
	bool PeerStrictViolation;
	bool LocalFirstExtOpen;
	bool PeerFirstExtOpen;
	bool LocalFirstExtUsed;
	bool PeerFirstExtUsed;
	bool LocalSecondExtUsed;
	bool PeerSecondExtUsed;
	bool LocalAuthSuccessPending;
	bool PeerAuthSuccessPending;
	bool LocalAuthSuccess;
	bool PeerAuthSuccess;
	uint32 Guard;
} xsshtransportstate;



XRT_EXTERN_C_BEGIN



/* 初始化空 KEX 方法规则。 */
XRT_API bool xrtSshTransportKexRulesInit(xsshtransportkexrules* pRules);



/* 设置一个 30..49 方法消息在本端或对端方向的精确额度。 */
XRT_API bool xrtSshTransportKexRuleSet(
	xsshtransportkexrules* pRules,
	xsshtransportdirection Direction,
	uint8 iMessage,
	uint8 iCount
);



/* 初始化一个不拥有外部资源的 client 或 server transport。 */
XRT_API bool xrtSshTransportStateInit(
	xsshtransportstate* pState,
	xsshrole Role
);



/* 清除状态；不会清理任何调用方密钥或缓冲。 */
XRT_API void xrtSshTransportStateClear(xsshtransportstate* pState);



/* 提交本端 identification 已发送或对端 identification 已验证。 */
XRT_API xsshcode xrtSshTransportIdentificationCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 判断对应方向当前是否可以提交应用层消息。 */
XRT_API bool xrtSshTransportCanApplication(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 判断是否已收到对端 KEXINIT，且本端必须回复 KEXINIT。 */
XRT_API bool xrtSshTransportKexReplyNeeded(
	const xsshtransportstate* pState
);



/* 检查 KEXINIT 是否可在指定方向可靠提交。 */
XRT_API xsshcode xrtSshTransportKexInitCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 提交 KEXINIT，并记录下一包是否为猜测的 KEX 方法消息。 */
XRT_API xsshcode xrtSshTransportKexInitCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	bool bFirstKexPacketFollows
);



/*
 * 双方 KEXINIT 到达后提交本代协商和方法规则。
 * KEXINIT 与协商视图仅在调用期间借用，不保存在状态对象中。
 */
XRT_API xsshcode xrtSshTransportKexConfigure(
	xsshtransportstate* pState,
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	const xsshtransportkexrules* pRules
);



/* 检查普通 transport、KEX 方法或应用消息；KEXINIT/NEWKEYS 使用专用 API。 */
XRT_API xsshcode xrtSshTransportMessageCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
);



/* 在消息已可靠入队或已认证接收后提交普通消息。 */
XRT_API xsshcode xrtSshTransportMessageCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
);



/* 检查对应方向已经完成全部 KEX 方法消息，可以发送或接受 NEWKEYS。 */
XRT_API xsshcode xrtSshTransportNewKeysCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 提交 NEWKEYS，并返回密钥切换、序列重置和整代完成动作。 */
XRT_API xsshcode xrtSshTransportNewKeysCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint32* pActions
);



/* 检查 server USERAUTH_SUCCESS；用于约束第二次 EXT_INFO 必须紧邻在前。 */
XRT_API xsshcode xrtSshTransportAuthSuccessCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 提交 server USERAUTH_SUCCESS；消息格式仍由 ssh_auth_message 处理。 */
XRT_API xsshcode xrtSshTransportAuthSuccessCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 网络关闭后终止状态机；重复调用保持关闭状态。 */
XRT_API void xrtSshTransportClose(xsshtransportstate* pState);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_transport_core.h */
/* ========================================================================== */

#ifndef XRT_SSH_TRANSPORT_CORE_H
#define XRT_SSH_TRANSPORT_CORE_H




#if defined(XSSH_FEATURE_TRANSPORT_CORE) && \
	(!defined(XSSH_FEATURE_PACKET_CODEC) || \
	 !defined(XSSH_FEATURE_TRANSPORT_REKEY) || \
	 !defined(XSSH_FEATURE_TRANSPORT_STATE))
	#error "XSSH_FEATURE_TRANSPORT_CORE requires packet codec, rekey and transport state"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_CORE)

/* 待提交包的类别由 core 从完整 payload 自动识别。 */
typedef enum xsshtransportpacketkind {
	XSSH_TRANSPORT_PACKET_MESSAGE = 0,
	XSSH_TRANSPORT_PACKET_KEXINIT = 1,
	XSSH_TRANSPORT_PACKET_NEWKEYS = 2,
	XSSH_TRANSPORT_PACKET_AUTH_SUCCESS = 3
} xsshtransportpacketkind;



/* 待提交状态不拥有 payload 或线路缓冲，字段只供诊断读取。 */
typedef struct xsshtransportpending {
	uint64 WireBytes;
	uint64 CipherBlocks;
	xsshtransportpacketkind Kind;
	uint8 Message;
	bool FirstKexPacketFollows;
	bool Active;
} xsshtransportpending;



/*
	Core 只拥有 packet、顺序和预算状态，不拥有网络、时钟、密钥原文或缓冲。
	单个对象由一个执行流推进，读写网络等待可以由同步、future 或协程驱动共享。
*/
typedef struct xsshtransportcore {
	xsshpacketcodec Codec;
	xsshtransportstate State;
	xsshrekeystate Rekey;
	xsshtransportpending Write;
	xsshtransportpending Read;
	uint32 WriteKeyActions;
	uint32 ReadKeyActions;
	bool KexCompletePending;
	uint32 Guard;
} xsshtransportcore;



/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 初始化无缓冲 transport core；零包上限和空策略分别使用默认值。 */
XRT_API bool xrtSshTransportCoreInit(
	xsshtransportcore* pCore,
	xsshrole Role,
	uint32 iMaxPacketSize,
	const xsshrekeypolicy* pRekeyPolicy,
	double Timer
);



/* 清除 cipher、序列、协议和预算状态，不处理任何调用方缓冲。 */
XRT_API void xrtSshTransportCoreClear(xsshtransportcore* pCore);



/* 提交本端 identification 已发送或对端 identification 已验证。 */
XRT_API xsshcode xrtSshTransportCoreIdentificationCommit(
	xsshtransportcore* pCore,
	xsshtransportdirection Direction
);



/* 判断对应方向当前是否允许应用消息且 NEWKEYS 密钥已经生效。 */
XRT_API bool xrtSshTransportCoreCanApplication(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
);



/* 判断收到对端 KEXINIT 后本端是否必须回复。 */
XRT_API bool xrtSshTransportCoreKexReplyNeeded(
	const xsshtransportcore* pCore
);



/* 提交双方 KEXINIT 的协商结果和当前算法消息额度。 */
XRT_API xsshcode xrtSshTransportCoreKexConfigure(
	xsshtransportcore* pCore,
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	const xsshtransportkexrules* pRules
);



/* 请求一次策略阈值之外的主动 rekey。 */
XRT_API bool xrtSshTransportCoreRekeyRequest(xsshtransportcore* pCore);



/* 查询当前双向预算和时间产生的 rekey 决策。 */
XRT_API xsshcode xrtSshTransportCoreRekeyCheck(
	const xsshtransportcore* pCore,
	double Timer,
	xsshrekeydecision* pDecision
);



/* 探测下一包所需完整线路长度和明文工作区。 */
XRT_API xsshcode xrtSshTransportCoreInspect(
	const xsshtransportcore* pCore,
	const xsshreader* pReader,
	xsshpacketneed* pNeed
);



/*
	生成最终线路包但不推进协议、sequence、nonce 或 rekey 预算。
	网络队列返回 AGAIN 时保留 writer 新增字节并重试同一包。
*/
XRT_API xsshcode xrtSshTransportCoreWritePrepareWithPadding(
	xsshtransportcore* pCore,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData,
	double Timer
);



/* 线路包可靠入队后提交写事务并返回更新后的 rekey 决策。 */
XRT_API xsshcode xrtSshTransportCoreWriteCommit(
	xsshtransportcore* pCore,
	double Timer,
	xsshrekeydecision* pDecision
);



/* 放弃未发送的写事务；调用方负责丢弃 writer 新增的线路字节。 */
XRT_API xsshcode xrtSshTransportCoreWriteAbort(xsshtransportcore* pCore);



/*
	认证并准备一个完整接收包；成功后 packet 借用输入或 pPlain。
	调用方只能解析当前 packet，随后必须 Commit 或 Abort。
*/
XRT_API xsshcode xrtSshTransportCoreReadPrepare(
	xsshtransportcore* pCore,
	xsshreader* pReader,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity,
	double Timer
);



/* 接收包完成协议处理后提交状态和 rekey 预算。 */
XRT_API xsshcode xrtSshTransportCoreReadCommit(
	xsshtransportcore* pCore,
	double Timer,
	xsshrekeydecision* pDecision
);



/* 放弃已认证但不能接受的接收包，并关闭不可继续推进的 transport。 */
XRT_API xsshcode xrtSshTransportCoreReadAbort(xsshtransportcore* pCore);



/* 判断本端 NEWKEYS 后是否正在等待写方向新密钥。 */
XRT_API bool xrtSshTransportCoreWriteKeysPending(
	const xsshtransportcore* pCore
);



/* 判断对端 NEWKEYS 后是否正在等待读方向新密钥。 */
XRT_API bool xrtSshTransportCoreReadKeysPending(
	const xsshtransportcore* pCore
);



/* 在本端 NEWKEYS 可靠入队后激活写方向 AES-GCM 与方向性 rekey 新代。 */
XRT_API xsshcode xrtSshTransportCoreSetWriteAesGcm(
	xsshtransportcore* pCore,
	xbytesview Key,
	xbytesview InitialIV,
	double Timer
);



/* 在对端 NEWKEYS 已认证后激活读方向 AES-GCM 与方向性 rekey 新代。 */
XRT_API xsshcode xrtSshTransportCoreSetReadAesGcm(
	xsshtransportcore* pCore,
	xbytesview Key,
	xbytesview InitialIV,
	double Timer
);



/* 判断至少一代 KEX 已完成且双向 NEWKEYS 密钥均已实际生效。 */
XRT_API bool xrtSshTransportCoreKexComplete(
	const xsshtransportcore* pCore
);



/* 关闭 transport；未发送写事务会安全放弃，已准备读事务不会回滚。 */
XRT_API void xrtSshTransportCoreClose(xsshtransportcore* pCore);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_kex_session.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEX_SESSION_H
#define XRT_SSH_KEX_SESSION_H




#if defined(XSSH_FEATURE_KEX_SESSION) && \
	(!defined(XSSH_FEATURE_HOSTKEY_ED25519) || \
	 !defined(XSSH_FEATURE_KEX_CURVE25519) || \
	 !defined(XSSH_FEATURE_KEX_ECDH) || \
	 !defined(XSSH_FEATURE_KEX_SHA256) || \
	 !defined(XSSH_FEATURE_TRANSPORT_CORE))
	#error "XSSH_FEATURE_KEX_SESSION requires Ed25519, Curve25519, ECDH, SHA-256 and transport core"
#endif



#if defined(XSSH_FEATURE_KEX_SESSION)

#define XSSH_KEX_SESSION_KEY_MAX 32u



/* KEX transcript 借用调用方稳定存储，字段均不包含 packet framing。 */
typedef struct xsshkextranscript {
	xbytesview ClientVersion;
	xbytesview ServerVersion;
	xbytesview ClientKexInit;
	xbytesview ServerKexInit;
} xsshkextranscript;



/* 会话阶段只表达 KEX 编排，不表达 socket、等待或任务状态。 */
typedef enum xsshkexsessionphase {
	XSSH_KEX_SESSION_IDLE = 0,
	XSSH_KEX_SESSION_METHOD = 1,
	XSSH_KEX_SESSION_HOST_KEY = 2,
	XSSH_KEX_SESSION_NEW_KEYS = 3,
	XSSH_KEX_SESSION_COMPLETE = 4,
	XSSH_KEX_SESSION_FAILED = 5
} xsshkexsessionphase;



/* Event 给驱动指出下一项常见动作；接收 NEWKEYS 可按网络到达顺序提前处理。 */
typedef enum xsshkexsessionevent {
	XSSH_KEX_EVENT_NONE = 0,
	XSSH_KEX_EVENT_WRITE_ECDH_INIT = 1,
	XSSH_KEX_EVENT_READ_ECDH_INIT = 2,
	XSSH_KEX_EVENT_WRITE_ECDH_REPLY = 3,
	XSSH_KEX_EVENT_READ_ECDH_REPLY = 4,
	XSSH_KEX_EVENT_VERIFY_HOST_KEY = 5,
	XSSH_KEX_EVENT_WRITE_NEWKEYS = 6,
	XSSH_KEX_EVENT_READ_NEWKEYS = 7,
	XSSH_KEX_EVENT_ACTIVATE_WRITE = 8,
	XSSH_KEX_EVENT_ACTIVATE_READ = 9,
	XSSH_KEX_EVENT_COMPLETE = 10,
	XSSH_KEX_EVENT_FAILED = 11
} xsshkexsessionevent;



/* Prepare 事务类型公开用于诊断，调用方不得直接修改。 */
typedef enum xsshkexsessionpacket {
	XSSH_KEX_PACKET_NONE = 0,
	XSSH_KEX_PACKET_DISCARD = 1,
	XSSH_KEX_PACKET_ECDH_INIT = 2,
	XSSH_KEX_PACKET_ECDH_REPLY = 3,
	XSSH_KEX_PACKET_NEWKEYS = 4
} xsshkexsessionpacket;



/*
	对象只保存固定尺寸密码状态和借用视图，不保存 KEXINIT、packet 或网络缓冲。
	同一对象由一个执行流推进；SessionId 跨 rekey 保留，Clear 时安全清零。
*/
typedef struct xsshkexsession {
	xsshkextranscript Transcript;
	xsshkexnegotiation Negotiation;
	xbytesview ServerHostKey;
	uint8 PrivateKey[XSSH_CURVE25519_PRIVATE_SIZE];
	uint8 PublicKey[XSSH_CURVE25519_PUBLIC_SIZE];
	uint8 PeerPublicKey[XSSH_CURVE25519_PUBLIC_SIZE];
	uint8 SharedSecret[XSSH_CURVE25519_SHARED_SIZE];
	uint8 ExchangeHash[XSSH_SHA256_SIZE];
	uint8 SessionId[XSSH_SHA256_SIZE];
	uint8 ClientToServerIV[XSSH_AES_GCM_IV_SIZE];
	uint8 ServerToClientIV[XSSH_AES_GCM_IV_SIZE];
	uint8 ClientToServerKey[XSSH_KEX_SESSION_KEY_MAX];
	uint8 ServerToClientKey[XSSH_KEX_SESSION_KEY_MAX];
	xsshrole Role;
	xsshkexsessionphase Phase;
	xsshkexsessionpacket WritePending;
	xsshkexsessionpacket ReadPending;
	uint8 ClientToServerKeySize;
	uint8 ServerToClientKeySize;
	bool Active;
	bool HasSessionId;
	bool KeysDerived;
	bool MethodWriteCommitted;
	bool MethodReadCommitted;
	bool HostKeyVerified;
	bool HostKeyAccepted;
	bool LocalNewKeys;
	bool PeerNewKeys;
	bool WriteActivated;
	bool ReadActivated;
	uint32 Guard;
} xsshkexsession;



/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 校验四段 transcript 借用视图；调用方保证其存活到本轮 KEX 完成。 */
XRT_API xsshcode xrtSshKexTranscriptInit(
	xsshkextranscript* pTranscript,
	xbytesview ClientVersion,
	xbytesview ServerVersion,
	xbytesview ClientKexInit,
	xbytesview ServerKexInit
);



/* 计算复制完整 transcript 所需的精确字节数。 */
XRT_API xsshcode xrtSshKexTranscriptMeasure(
	const xsshkextranscript* pTranscript,
	size_t* pSize
);



/* 将 transcript 追加到 writer，并返回借用 writer 输出的稳定视图。 */
XRT_API xsshcode xrtSshKexTranscriptWrite(
	xsshwriter* pWriter,
	const xsshkextranscript* pInput,
	xsshkextranscript* pOutput
);



/* 初始化可重复执行初始 KEX 和 rekey 的确定性会话。 */
XRT_API bool xrtSshKexSessionInit(
	xsshkexsession* pSession,
	xsshrole Role
);



/* 安全清除临时私钥、共享秘密、派生密钥和 SessionId。 */
XRT_API void xrtSshKexSessionClear(xsshkexsession* pSession);



/*
	使用显式 Curve25519 私钥开始一代 KEX，并配置已经提交双方 KEXINIT 的 core。
	服务端必须提供稳定的 ssh-ed25519 HostKey；客户端传空视图。
*/
XRT_API xsshcode xrtSshKexSessionBeginWithPrivate(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	const xsshkextranscript* pTranscript,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
);



/* 返回当前最常见的下一动作，不推进任何状态。 */
XRT_API xsshkexsessionevent xrtSshKexSessionEvent(
	const xsshkexsession* pSession
);



/* 返回本代协商结果；视图借用 transcript。 */
XRT_API xsshcode xrtSshKexSessionNegotiation(
	const xsshkexsession* pSession,
	xsshkexnegotiation* pNegotiation
);



/* 返回本代 exchange hash，服务端可交给本地密钥或 HSM 签名。 */
XRT_API xsshcode xrtSshKexSessionExchangeHash(
	const xsshkexsession* pSession,
	xbytesview* pHash
);



/* 返回首次 exchange hash 固化的 SessionId。 */
XRT_API xsshcode xrtSshKexSessionId(
	const xsshkexsession* pSession,
	xbytesview* pSessionId
);



/* 返回已完成密码学验签、等待信任策略确认的服务端主机公钥。 */
XRT_API xsshcode xrtSshKexSessionHostKey(
	const xsshkexsession* pSession,
	xbytesview* pHostKey
);



/* 客户端确认主机密钥信任；之后才允许发送 NEWKEYS。 */
XRT_API xsshcode xrtSshKexSessionHostKeyAccept(
	xsshkexsession* pSession
);



/* 将会话置为不可继续状态并安全清除本代秘密。 */
XRT_API void xrtSshKexSessionFail(xsshkexsession* pSession);



/* 客户端准备 SSH_MSG_KEX_ECDH_INIT；可靠提交后调用 WriteCommit。 */
XRT_API xsshcode xrtSshKexSessionEcdhInitPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter
);



/* 服务端用外部签名 blob 准备 SSH_MSG_KEX_ECDH_REPLY。 */
XRT_API xsshcode xrtSshKexSessionEcdhReplyPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter,
	xbytesview Signature
);



/* 准备 SSH_MSG_NEWKEYS；方法交换和客户端主机信任必须已完成。 */
XRT_API xsshcode xrtSshKexSessionNewKeysPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter
);



/* transport core 已可靠提交当前输出后提交 KEX 写事务。 */
XRT_API xsshcode xrtSshKexSessionWriteCommit(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃尚未交给 transport 的 KEX 输出，不消费任何会话状态。 */
XRT_API xsshcode xrtSshKexSessionWriteAbort(xsshkexsession* pSession);



/*
	解析 transport core 已认证准备的 peer payload。
	客户端 ECDH_REPLY 会把 HostKey 复制到调用方存储，空间不足可按其长度重试。
*/
XRT_API xsshcode xrtSshKexSessionReadPrepare(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize
);



/* transport core 已提交当前输入后提交 KEX 读事务。 */
XRT_API xsshcode xrtSshKexSessionReadCommit(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃已认证 KEX 输入并终止会话；对应 transport 也必须关闭。 */
XRT_API xsshcode xrtSshKexSessionReadAbort(xsshkexsession* pSession);



/* 本端 NEWKEYS 提交后，把角色对应的派生密钥装入写 codec。 */
XRT_API xsshcode xrtSshKexSessionActivateWrite(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	double Timer
);



/* 对端 NEWKEYS 提交后，把角色对应的派生密钥装入读 codec。 */
XRT_API xsshcode xrtSshKexSessionActivateRead(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	double Timer
);



/* 判断本代双向密钥和 transport core 均已完成切换。 */
XRT_API bool xrtSshKexSessionComplete(
	const xsshkexsession* pSession,
	const xsshtransportcore* pCore
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_kex_session_random.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEX_SESSION_RANDOM_H
#define XRT_SSH_KEX_SESSION_RANDOM_H




#if defined(XSSH_FEATURE_KEX_SESSION_RANDOM) && \
	(!defined(XSSH_FEATURE_KEX_SESSION) || \
	 !defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM))
	#error "XSSH_FEATURE_KEX_SESSION_RANDOM requires KEX session and secure Curve25519 keypair"
#endif



#if defined(XSSH_FEATURE_KEX_SESSION_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用 XRT 系统安全随机源开始一代 Curve25519 KEX。 */
XRT_API xsshcode xrtSshKexSessionBegin(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	const xsshkextranscript* pTranscript,
	xbytesview ServerHostKey
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_kex_exchange.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEX_EXCHANGE_H
#define XRT_SSH_KEX_EXCHANGE_H




#if defined(XSSH_FEATURE_KEX_EXCHANGE) && \
	(!defined(XSSH_FEATURE_KEX_SESSION) || \
	 !defined(XRT_FEATURE_NET_BUFFER))
	#error "XSSH_FEATURE_KEX_EXCHANGE requires KEX session and XRT network buffer"
#endif



#if defined(XSSH_FEATURE_KEX_EXCHANGE)

/* 交换阶段独立于网络驱动；READY 表示四段 transcript 已经稳定。 */
typedef enum xsshkexexchangephase {
	XSSH_KEX_EXCHANGE_IDENTIFICATION = 0,
	XSSH_KEX_EXCHANGE_KEXINIT = 1,
	XSSH_KEX_EXCHANGE_READY = 2,
	XSSH_KEX_EXCHANGE_METHOD = 3,
	XSSH_KEX_EXCHANGE_COMPLETE = 4,
	XSSH_KEX_EXCHANGE_FAILED = 5
} xsshkexexchangephase;



/* 一个交换对象最多存在一个 identification 或 KEXINIT 保存事务。 */
typedef enum xsshkexexchangepending {
	XSSH_KEX_EXCHANGE_PENDING_NONE = 0,
	XSSH_KEX_EXCHANGE_PENDING_VERSION = 1,
	XSSH_KEX_EXCHANGE_PENDING_KEXINIT = 2
} xsshkexexchangepending;



/*
	对象拥有连接级版本串、本代与下一代 KEXINIT 动态链，以及可重复 rekey 的 KEX 会话。
	对象不拥有 transport、网络、随机源、主机密钥或任务；公开字段只供诊断读取。
*/
typedef struct xsshkexexchange {
	xsshkexsession Session;
	xnetbuf ClientVersion;
	xnetbuf ServerVersion;
	xnetbuf ClientKexInit;
	xnetbuf ServerKexInit;
	xnetbuf NextClientKexInit;
	xnetbuf NextServerKexInit;
	xnetbuf Staging;
	uint64 PendingOrdinal;
	uint64 PendingKexCount;
	xsshrole Role;
	xsshtransportdirection PendingDirection;
	xsshtransportphase PendingCorePhase;
	xsshkexexchangephase Phase;
	xsshkexexchangepending Pending;
	bool PendingFirstKexPacketFollows;
	bool Initialized;
	uint32 Guard;
} xsshkexexchange;



XRT_EXTERN_C_BEGIN



/* 使用同一动态缓冲池初始化连接级 KEX 交换对象；空池使用全局分配器。 */
XRT_API bool xrtSshKexExchangeInit(
	xsshkexexchange* pExchange,
	xnetbufpool* pPool,
	xsshrole Role
);



/* 释放全部 transcript 动态块，并安全清除 KEX 密钥和 SessionId。 */
XRT_API void xrtSshKexExchangeClear(xsshkexexchange* pExchange);



/*
	在 transport identification 提交前复制本端或对端的无换行版本串。
	本端只接受 SSH-2.0；对端同时接受 wire 层支持的 SSH-1.99 兼容形式。
*/
XRT_API xsshcode xrtSshKexExchangeVersionPrepare(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xstrview Version
);



/* transport 已提交对应 identification 后，发布连接级稳定版本串。 */
XRT_API xsshcode xrtSshKexExchangeVersionCommit(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/* transport 尚未提交对应 identification 时放弃暂存副本。 */
XRT_API xsshcode xrtSshKexExchangeVersionAbort(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/*
	复制完整 KEXINIT payload；本端在 transport Prepare 前调用，对端在认证读取后调用。
	本端 guessed packet 暂不受 KEX 会话支持，会在这里明确返回 UNSUPPORTED。
*/
XRT_API xsshcode xrtSshKexExchangeKexInitPrepare(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xbytesview Payload
);



/* transport 已提交同一方向和 packet 序号后，发布本代 KEXINIT。 */
XRT_API xsshcode xrtSshKexExchangeKexInitCommit(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/* transport 状态和 packet 计数尚未推进时放弃暂存 KEXINIT。 */
XRT_API xsshcode xrtSshKexExchangeKexInitAbort(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/* 判断版本串、双方 KEXINIT 与 transport 状态是否可以开始本代方法交换。 */
XRT_API bool xrtSshKexExchangeReady(
	const xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/* 返回 READY、METHOD 或 COMPLETE 阶段的稳定 transcript 借用视图。 */
XRT_API xsshcode xrtSshKexExchangeTranscript(
	xsshkexexchange* pExchange,
	xsshkextranscript* pTranscript
);



/* 使用显式 Curve25519 私钥开始本代 KEX，并在成功后晋升下一代动态缓冲。 */
XRT_API xsshcode xrtSshKexExchangeBeginWithPrivate(
	xsshkexexchange* pExchange,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
);



/* 返回 METHOD 或 COMPLETE 阶段可推进的 KEX 会话。 */
XRT_API xsshkexsession* xrtSshKexExchangeSession(
	xsshkexexchange* pExchange
);



/* 返回 METHOD 或 COMPLETE 阶段的只读 KEX 会话。 */
XRT_API const xsshkexsession* xrtSshKexExchangeSessionConst(
	const xsshkexexchange* pExchange
);



/* 确认 KEX 会话和 transport 均已完成本代双向密钥切换。 */
XRT_API xsshcode xrtSshKexExchangeComplete(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/* 终止交换，放弃未发布的下一代材料并清除本代临时秘密。 */
XRT_API void xrtSshKexExchangeFail(xsshkexexchange* pExchange);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_kex_exchange_random.h */
/* ========================================================================== */

#ifndef XRT_SSH_KEX_EXCHANGE_RANDOM_H
#define XRT_SSH_KEX_EXCHANGE_RANDOM_H




#if defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM) && \
	(!defined(XSSH_FEATURE_KEX_EXCHANGE) || \
	 !defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM))
	#error "XSSH_FEATURE_KEX_EXCHANGE_RANDOM requires KEX exchange and secure Curve25519 keys"
#endif



#if defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用操作系统安全随机临时私钥开始本代 KEX，并在返回前清除临时副本。 */
XRT_API xsshcode xrtSshKexExchangeBegin(
	xsshkexexchange* pExchange,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_transport_tcp.h */
/* ========================================================================== */

#ifndef XRT_SSH_TRANSPORT_TCP_H
#define XRT_SSH_TRANSPORT_TCP_H




#if defined(XSSH_FEATURE_TRANSPORT_TCP) && \
	(!defined(XSSH_FEATURE_TRANSPORT_CORE) || \
	 !defined(XRT_FEATURE_NET_TCP))
	#error "XSSH_FEATURE_TRANSPORT_TCP requires transport core and XRT TCP"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_TCP)

#define XSSH_TRANSPORT_TCP_BANNER_LIMIT_DEFAULT 65536u



/* TCP 适配层一次只借出一个 identification 或 packet 事务。 */
typedef enum xsshtransporttcppending {
	XSSH_TRANSPORT_TCP_PENDING_NONE = 0,
	XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION = 1,
	XSSH_TRANSPORT_TCP_PENDING_PACKET = 2
} xsshtransporttcppending;



/* 配置只描述协议预算；Engine、Stream、Worker 和时钟均由调用方持有。 */
typedef struct xsshtransporttcpconfig {
	xsshrekeypolicy Rekey;
	size_t MaxBannerBytes;
	uint32 MaxPacketSize;
	xsshrole Role;
} xsshtransporttcpconfig;



/*
	TCP transport 只拥有 core 与按需输出链，不含固定收发数组。
	除 API 明确返回的 Core 指针外，公开字段只供诊断读取。
*/
typedef struct xsshtransporttcp {
	xsshtransportcore Core;
	xnetbuf Output;
	xnetbuf* Input;
	size_t MaxBannerBytes;
	size_t ReadSize;
	xsshtransporttcppending WritePending;
	xsshtransporttcppending ReadPending;
	uint32 Guard;
} xsshtransporttcp;



/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 写入指定角色、默认 packet、banner 和 rekey 预算。 */
XRT_API bool xrtSshTransportTcpConfigInit(
	xsshtransporttcpconfig* pConfig,
	xsshrole Role
);



/* 使用所属 Worker 的缓冲池初始化 TCP transport；不接管缓冲池。 */
XRT_API bool xrtSshTransportTcpInit(
	xsshtransporttcp* pTransport,
	xnetbufpool* pPool,
	const xsshtransporttcpconfig* pConfig,
	double Timer
);



/* 放弃未决事务、释放动态块并安全清除 cipher 状态。 */
XRT_API void xrtSshTransportTcpClear(xsshtransporttcp* pTransport);



/* 返回可推进 KEX、密钥、认证和连接协议的 transport core。 */
XRT_API xsshtransportcore* xrtSshTransportTcpCore(
	xsshtransporttcp* pTransport
);



/* 返回只读 transport core；无效对象返回空。 */
XRT_API const xsshtransportcore* xrtSshTransportTcpCoreConst(
	const xsshtransporttcp* pTransport
);



/* 在动态输出链中准备本端 identification，但不推进协议状态。 */
XRT_API xsshcode xrtSshTransportTcpIdentificationPrepare(
	xsshtransporttcp* pTransport,
	xstrview Banner
);



/* 使用调用方 padding 源在动态输出链中准备唯一 packet。 */
XRT_API xsshcode xrtSshTransportTcpWritePrepareWithPadding(
	xsshtransporttcp* pTransport,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData,
	double Timer
);



/*
	把未决输出零复制交给 Stream；AGAIN 或 ERROR 时事务和缓冲保持不变。
	成功接管后立即提交 identification 或 packet 状态。
*/
XRT_API xnetresult xrtSshTransportTcpWriteSubmit(
	xsshtransporttcp* pTransport,
	xnetstream* pStream,
	double Timer,
	xsshrekeydecision* pDecision
);



/* 放弃尚未被 TCP 接管的输出；packet sequence 与 nonce 保持不变。 */
XRT_API xsshcode xrtSshTransportTcpWriteAbort(
	xsshtransporttcp* pTransport
);



/* 返回当前待重试输出字节数；没有未决输出时返回零。 */
XRT_API size_t xrtSshTransportTcpWriteSize(
	const xsshtransporttcp* pTransport
);



/* 从分块 TCP 输入准备 peer identification，返回值借用到 ReadCommit/Abort。 */
XRT_API xsshcode xrtSshTransportTcpIdentificationReadPrepare(
	xsshtransporttcp* pTransport,
	xnetbuf* pInput,
	xstrview* pBanner
);



/* 仅复制四字节长度头，探测下一 packet 的线路和解密工作区需求。 */
XRT_API xsshcode xrtSshTransportTcpReadInspect(
	const xsshtransporttcp* pTransport,
	const xnetbuf* pInput,
	xsshpacketneed* pNeed
);



/* 按需连续化一个完整 packet，认证后返回借用 view。 */
XRT_API xsshcode xrtSshTransportTcpReadPrepare(
	xsshtransporttcp* pTransport,
	xnetbuf* pInput,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity,
	double Timer
);



/* 提交上层已经接受的输入并从原 TCP 缓冲精确消费。 */
XRT_API xsshcode xrtSshTransportTcpReadCommit(
	xsshtransporttcp* pTransport,
	double Timer,
	xsshrekeydecision* pDecision
);



/* 拒绝当前借用输入、消费其线路字节并关闭不可继续的 transport。 */
XRT_API xsshcode xrtSshTransportTcpReadAbort(
	xsshtransporttcp* pTransport
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_transport_tcp_random.h */
/* ========================================================================== */

#ifndef XRT_SSH_TRANSPORT_TCP_RANDOM_H
#define XRT_SSH_TRANSPORT_TCP_RANDOM_H




#if defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM) && \
	(!defined(XSSH_FEATURE_TRANSPORT_TCP) || \
	 !defined(XSSH_FEATURE_PACKET_RANDOM))
	#error "XSSH_FEATURE_TRANSPORT_TCP_RANDOM requires TCP transport and secure padding"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM)

/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 使用 XRT 系统安全随机 padding 在动态输出链中准备唯一 packet。 */
XRT_API xsshcode xrtSshTransportTcpWritePrepare(
	xsshtransporttcp* pTransport,
	xbytesview Payload,
	double Timer
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_auth_message.h */
/* ========================================================================== */

#ifndef XRT_SSH_AUTH_MESSAGE_H
#define XRT_SSH_AUTH_MESSAGE_H




#if defined(XSSH_FEATURE_AUTH_MESSAGE) && \
	(!defined(XSSH_FEATURE_WIRE) || !defined(XRT_FEATURE_UNICODE))
	#error "XSSH_FEATURE_AUTH_MESSAGE requires XSSH_FEATURE_WIRE and XRT_FEATURE_UNICODE"
#endif



#if defined(XSSH_FEATURE_AUTH_MESSAGE)

#define XSSH_MSG_USERAUTH_REQUEST 50u
#define XSSH_MSG_USERAUTH_FAILURE 51u
#if !defined(XSSH_MSG_USERAUTH_SUCCESS)
	#define XSSH_MSG_USERAUTH_SUCCESS 52u
#endif
#define XSSH_MSG_USERAUTH_BANNER 53u

#define XSSH_SERVICE_USERAUTH "ssh-userauth"
#define XSSH_SERVICE_CONNECTION "ssh-connection"

#define XSSH_AUTH_METHOD_NONE "none"
#define XSSH_AUTH_METHOD_PASSWORD "password"
#define XSSH_AUTH_METHOD_PUBLICKEY "publickey"
#define XSSH_AUTH_METHOD_HOSTBASED "hostbased"
#define XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE "keyboard-interactive"



/* 通用认证请求借用完整 payload，并保留方法专用原始字段。 */
typedef struct xsshauthrequest {
	xstrview User;
	xstrview Service;
	xstrview Method;
	xbytesview Fields;
} xsshauthrequest;



/* 认证失败消息借用完整 payload。 */
typedef struct xsshauthfailure {
	xstrview Methods;
	bool PartialSuccess;
} xsshauthfailure;



/* 认证横幅消息借用完整 payload。 */
typedef struct xsshauthbanner {
	xstrview Message;
	xstrview Language;
} xsshauthbanner;



XRT_EXTERN_C_BEGIN



/* 计算通用 USERAUTH_REQUEST 总长度，不访问方法字段内容。 */
XRT_API xsshcode xrtSshAuthRequestSize(
	xstrview User,
	xstrview Service,
	xstrview Method,
	size_t iFieldsSize,
	size_t* pSize
);



/* 写入或读取可扩展的通用 USERAUTH_REQUEST。 */
XRT_API xsshcode xrtSshAuthRequestWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Service,
	xstrview Method,
	xbytesview Fields
);
XRT_API xsshcode xrtSshAuthRequestRead(
	xbytesview Payload,
	xsshauthrequest* pRequest
);



/* 使用 ssh-connection 服务写入或读取 none 探测。 */
XRT_API xsshcode xrtSshAuthNoneWrite(
	xsshwriter* pWriter,
	xstrview User
);
XRT_API xsshcode xrtSshAuthNoneRead(
	xbytesview Payload,
	xstrview* pUser
);



/* 写入或严格读取认证失败及可继续方法。 */
XRT_API xsshcode xrtSshAuthFailureWrite(
	xsshwriter* pWriter,
	xstrview Methods,
	bool bPartialSuccess
);
XRT_API xsshcode xrtSshAuthFailureRead(
	xbytesview Payload,
	xsshauthfailure* pFailure
);



/* 写入或严格读取无字段认证成功消息。 */
XRT_API xsshcode xrtSshAuthSuccessWrite(xsshwriter* pWriter);
XRT_API xsshcode xrtSshAuthSuccessRead(xbytesview Payload);



/* 写入或严格读取 UTF-8 认证横幅。 */
XRT_API xsshcode xrtSshAuthBannerWrite(
	xsshwriter* pWriter,
	xstrview Message,
	xstrview Language
);
XRT_API xsshcode xrtSshAuthBannerRead(
	xbytesview Payload,
	xsshauthbanner* pBanner
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_auth_password.h */
/* ========================================================================== */

#ifndef XRT_SSH_AUTH_PASSWORD_H
#define XRT_SSH_AUTH_PASSWORD_H




#if defined(XSSH_FEATURE_AUTH_PASSWORD) && \
	!defined(XSSH_FEATURE_AUTH_MESSAGE)
	#error "XSSH_FEATURE_AUTH_PASSWORD requires XSSH_FEATURE_AUTH_MESSAGE"
#endif



#if defined(XSSH_FEATURE_AUTH_PASSWORD)

#define XSSH_MSG_USERAUTH_PASSWD_CHANGEREQ 60u



/* Password 请求借用完整 payload；Password 是当前或旧密码。 */
typedef struct xsshauthpassword {
	xstrview User;
	bool Change;
	xstrview Password;
	xstrview NewPassword;
} xsshauthpassword;



/* Password 更改提示借用完整 payload。 */
typedef struct xsshauthpasswordprompt {
	xstrview Prompt;
	xstrview Language;
} xsshauthpasswordprompt;



XRT_EXTERN_C_BEGIN



/* 使用 ssh-connection 服务写入普通密码认证请求。 */
XRT_API xsshcode xrtSshAuthPasswordWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Password
);



/* 使用 ssh-connection 服务写入旧密码与新密码。 */
XRT_API xsshcode xrtSshAuthPasswordChangeWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Password,
	xstrview NewPassword
);



/* 严格读取普通或更改密码的 ssh-connection 请求。 */
XRT_API xsshcode xrtSshAuthPasswordRead(
	xbytesview Payload,
	xsshauthpassword* pPassword
);



/* 写入或严格读取服务端密码更改提示。 */
XRT_API xsshcode xrtSshAuthPasswordPromptWrite(
	xsshwriter* pWriter,
	xstrview Prompt,
	xstrview Language
);
XRT_API xsshcode xrtSshAuthPasswordPromptRead(
	xbytesview Payload,
	xsshauthpasswordprompt* pPrompt
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_auth_publickey.h */
/* ========================================================================== */

#ifndef XRT_SSH_AUTH_PUBLICKEY_H
#define XRT_SSH_AUTH_PUBLICKEY_H




#if defined(XSSH_FEATURE_AUTH_PUBLICKEY) && \
	(!defined(XSSH_FEATURE_AUTH_MESSAGE) || !defined(XSSH_FEATURE_HOSTKEY))
	#error "XSSH_FEATURE_AUTH_PUBLICKEY requires auth message and hostkey"
#endif



#if defined(XSSH_FEATURE_AUTH_PUBLICKEY)

#define XSSH_MSG_USERAUTH_PK_OK 60u



/* Publickey 请求借用完整 payload。 */
typedef struct xsshauthpublickey {
	xstrview User;
	bool HasSignature;
	xstrview Algorithm;
	xbytesview PublicKey;
	xbytesview Signature;
} xsshauthpublickey;



/* Publickey 探测成功响应借用完整 payload。 */
typedef struct xsshauthpublickeyok {
	xstrview Algorithm;
	xbytesview PublicKey;
} xsshauthpublickeyok;



XRT_EXTERN_C_BEGIN



/* 使用 ssh-connection 服务写入无签名 publickey 探测。 */
XRT_API xsshcode xrtSshAuthPublicKeyWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey
);



/* 使用 ssh-connection 服务写入带签名 publickey 请求。 */
XRT_API xsshcode xrtSshAuthPublicKeySignedWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xbytesview Signature
);



/* 严格读取 publickey 探测或带签名请求。 */
XRT_API xsshcode xrtSshAuthPublicKeyRead(
	xbytesview Payload,
	xsshauthpublickey* pPublicKey
);



/* 写入 RFC 4252 publickey 签名原文，不执行签名。 */
XRT_API xsshcode xrtSshAuthPublicKeySignDataWrite(
	xsshwriter* pWriter,
	xbytesview SessionId,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey
);



/* 写入或严格读取服务端 publickey 探测成功响应。 */
XRT_API xsshcode xrtSshAuthPublicKeyOkWrite(
	xsshwriter* pWriter,
	xstrview Algorithm,
	xbytesview PublicKey
);
XRT_API xsshcode xrtSshAuthPublicKeyOkRead(
	xbytesview Payload,
	xsshauthpublickeyok* pPublicKey
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_auth_keyboard.h */
/* ========================================================================== */

#ifndef XRT_SSH_AUTH_KEYBOARD_H
#define XRT_SSH_AUTH_KEYBOARD_H




#if defined(XSSH_FEATURE_AUTH_KEYBOARD) && \
	!defined(XSSH_FEATURE_AUTH_MESSAGE)
	#error "XSSH_FEATURE_AUTH_KEYBOARD requires XSSH_FEATURE_AUTH_MESSAGE"
#endif



#if defined(XSSH_FEATURE_AUTH_KEYBOARD)

#define XSSH_MSG_USERAUTH_INFO_REQUEST 60u
#define XSSH_MSG_USERAUTH_INFO_RESPONSE 61u



/* Keyboard-interactive 请求借用完整 payload。 */
typedef struct xsshauthkeyboard {
	xstrview User;
	xstrview Language;
	xstrview Submethods;
} xsshauthkeyboard;



/* 单个交互提示借用完整 challenge payload。 */
typedef struct xsshauthkeyboardprompt {
	xstrview Prompt;
	bool Echo;
} xsshauthkeyboardprompt;



/* Challenge 迭代器已在初始化时严格验证全部提示。 */
typedef struct xsshauthkeyboardchallenge {
	xstrview Name;
	xstrview Instruction;
	xstrview Language;
	uint32 Count;
	uint32 Index;
	xsshreader Reader;
} xsshauthkeyboardchallenge;



/* Response 迭代器已在初始化时严格验证全部响应。 */
typedef struct xsshauthkeyboardresponses {
	uint32 Count;
	uint32 Index;
	xsshreader Reader;
} xsshauthkeyboardresponses;



XRT_EXTERN_C_BEGIN



/* 使用空 language tag 写入 ssh-connection keyboard-interactive 请求。 */
XRT_API xsshcode xrtSshAuthKeyboardWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Submethods
);



/* 写入带显式 language tag 的 keyboard-interactive 请求。 */
XRT_API xsshcode xrtSshAuthKeyboardWriteLanguage(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Language,
	xstrview Submethods
);



/* 严格读取 keyboard-interactive 请求。 */
XRT_API xsshcode xrtSshAuthKeyboardRead(
	xbytesview Payload,
	xsshauthkeyboard* pKeyboard
);



/* 写入、验证并迭代服务端交互挑战。 */
XRT_API xsshcode xrtSshAuthKeyboardChallengeWrite(
	xsshwriter* pWriter,
	xstrview Name,
	xstrview Instruction,
	xstrview Language,
	const xsshauthkeyboardprompt* pPrompts,
	size_t iCount
);
XRT_API xsshcode xrtSshAuthKeyboardChallengeRead(
	xbytesview Payload,
	xsshauthkeyboardchallenge* pChallenge
);
XRT_API bool xrtSshAuthKeyboardChallengeNext(
	xsshauthkeyboardchallenge* pChallenge,
	xsshauthkeyboardprompt* pPrompt
);



/* 写入、验证并迭代客户端交互响应。 */
XRT_API xsshcode xrtSshAuthKeyboardResponseWrite(
	xsshwriter* pWriter,
	const xstrview* pResponses,
	size_t iCount
);
XRT_API xsshcode xrtSshAuthKeyboardResponseRead(
	xbytesview Payload,
	xsshauthkeyboardresponses* pResponses
);
XRT_API bool xrtSshAuthKeyboardResponseNext(
	xsshauthkeyboardresponses* pResponses,
	xstrview* pResponse
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_auth_hostbased.h */
/* ========================================================================== */

#ifndef XRT_SSH_AUTH_HOSTBASED_H
#define XRT_SSH_AUTH_HOSTBASED_H




#if defined(XSSH_FEATURE_AUTH_HOSTBASED) && \
	(!defined(XSSH_FEATURE_AUTH_MESSAGE) || !defined(XSSH_FEATURE_HOSTKEY))
	#error "XSSH_FEATURE_AUTH_HOSTBASED requires auth message and hostkey"
#endif



#if defined(XSSH_FEATURE_AUTH_HOSTBASED)

#define XSSH_AUTH_HOST_NAME_MAX 254u



/* Hostbased 请求借用完整 payload。 */
typedef struct xsshauthhostbased {
	xstrview User;
	xstrview Algorithm;
	xbytesview PublicKey;
	xstrview HostName;
	xstrview ClientUser;
	xbytesview Signature;
} xsshauthhostbased;



XRT_EXTERN_C_BEGIN



/* 校验 US-ASCII DNS 主机名；允许末尾根标签点。 */
XRT_API bool xrtSshAuthHostNameValid(xstrview HostName);



/* 使用 ssh-connection 服务写入完整 hostbased 请求。 */
XRT_API xsshcode xrtSshAuthHostBasedWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser,
	xbytesview Signature
);



/* 严格读取 hostbased 请求。 */
XRT_API xsshcode xrtSshAuthHostBasedRead(
	xbytesview Payload,
	xsshauthhostbased* pHostBased
);



/* 写入 RFC 4252 hostbased 签名原文，不执行签名。 */
XRT_API xsshcode xrtSshAuthHostBasedSignDataWrite(
	xsshwriter* pWriter,
	xbytesview SessionId,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_auth_guard.h */
/* ========================================================================== */

#ifndef XRT_SSH_AUTH_GUARD_H
#define XRT_SSH_AUTH_GUARD_H




#if defined(XSSH_FEATURE_AUTH_GUARD) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_AUTH_GUARD requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_AUTH_GUARD)

#define XSSH_AUTH_DEFAULT_TIMEOUT_MS UINT64_C(600000)
#define XSSH_AUTH_DEFAULT_BYTE_LIMIT UINT64_C(16777216)
#define XSSH_AUTH_DEFAULT_ATTEMPT_LIMIT 20u
#define XSSH_AUTH_DEFAULT_ROUND_LIMIT 32u
#define XSSH_AUTH_DEFAULT_MESSAGE_LIMIT 256u



/* 每条认证消息只选择一个预算事件，避免重复计数。 */
typedef enum xsshauthevent {
	XSSH_AUTH_EVENT_MESSAGE = 0,
	XSSH_AUTH_EVENT_ATTEMPT,
	XSSH_AUTH_EVENT_ROUND
} xsshauthevent;



/* Guard 决策区分正常处理、成功后忽略和必须断开。 */
typedef enum xsshauthguarddecision {
	XSSH_AUTH_GUARD_ALLOW = 0,
	XSSH_AUTH_GUARD_IGNORE,
	XSSH_AUTH_GUARD_DISCONNECT
} xsshauthguarddecision;



/* 首个耗尽原因保持稳定，便于结构化错误和统计。 */
typedef enum xsshauthexhaustion {
	XSSH_AUTH_EXHAUST_NONE = 0,
	XSSH_AUTH_EXHAUST_TIMEOUT,
	XSSH_AUTH_EXHAUST_ATTEMPTS,
	XSSH_AUTH_EXHAUST_ROUNDS,
	XSSH_AUTH_EXHAUST_MESSAGES,
	XSSH_AUTH_EXHAUST_BYTES
} xsshauthexhaustion;



/* 零值单项限制表示禁用；时间统一使用单调毫秒。 */
typedef struct xsshauthguardpolicy {
	int64 TimeoutMs;
	uint64 ByteLimit;
	uint32 AttemptLimit;
	uint32 RoundLimit;
	uint32 MessageLimit;
} xsshauthguardpolicy;



/* Guard 只保存会话总预算，不保存用户名、凭据或报文借用视图。 */
typedef struct xsshauthguard {
	xsshauthguardpolicy Policy;
	double StartedTimer;
	uint64 Bytes;
	uint32 Attempts;
	uint32 Rounds;
	uint32 Messages;
	xsshauthexhaustion Exhaustion;
	bool Complete;
	bool Initialized;
} xsshauthguard;



/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 初始化 RFC 推荐值和有界资源默认策略。 */
XRT_API void xrtSshAuthGuardPolicyInit(xsshauthguardpolicy* pPolicy);



/* 复制显式或默认策略并开始一个认证会话。 */
XRT_API bool xrtSshAuthGuardInit(
	xsshauthguard* pGuard,
	const xsshauthguardpolicy* pPolicy,
	double Timer
);



/* 查询当前时间、完成状态和已有预算产生的决策。 */
XRT_API xsshcode xrtSshAuthGuardCheck(
	xsshauthguard* pGuard,
	double Timer,
	xsshauthguarddecision* pDecision
);



/* 原子预留一条认证消息；超过任一上限时进入不可恢复的断开状态。 */
XRT_API xsshcode xrtSshAuthGuardReserve(
	xsshauthguard* pGuard,
	xsshauthevent Event,
	uint64 iMessageBytes,
	double Timer,
	xsshauthguarddecision* pDecision
);



/* 认证成功后冻结预算；后续认证消息统一返回 IGNORE。 */
XRT_API bool xrtSshAuthGuardComplete(xsshauthguard* pGuard);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_auth_session.h */
/* ========================================================================== */

#ifndef XRT_SSH_AUTH_SESSION_H
#define XRT_SSH_AUTH_SESSION_H




#if defined(XSSH_FEATURE_AUTH_SESSION) && \
	(!defined(XSSH_FEATURE_AUTH_GUARD) || \
	 !defined(XSSH_FEATURE_AUTH_MESSAGE) || \
	 !defined(XSSH_FEATURE_TRANSPORT_CORE))
	#error "XSSH_FEATURE_AUTH_SESSION requires auth guard, auth message and transport core"
#endif



#if defined(XSSH_FEATURE_AUTH_SESSION)

/* 会话阶段只表达认证编排，不表达 socket、等待或认证后端状态。 */
typedef enum xsshauthsessionphase {
	XSSH_AUTH_SESSION_IDLE = 0,
	XSSH_AUTH_SESSION_SERVICE = 1,
	XSSH_AUTH_SESSION_AUTHENTICATION = 2,
	XSSH_AUTH_SESSION_COMPLETE = 3,
	XSSH_AUTH_SESSION_FAILED = 4
} xsshauthsessionphase;



/* Event 表达驱动下一步应发送或接收哪类认证消息。 */
typedef enum xsshauthsessionevent {
	XSSH_AUTH_SESSION_EVENT_NONE = 0,
	XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST = 1,
	XSSH_AUTH_SESSION_EVENT_READ_SERVICE_REQUEST = 2,
	XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_ACCEPT = 3,
	XSSH_AUTH_SESSION_EVENT_READ_SERVICE_ACCEPT = 4,
	XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST = 5,
	XSSH_AUTH_SESSION_EVENT_READ_REQUEST = 6,
	XSSH_AUTH_SESSION_EVENT_WRITE_RESULT = 7,
	XSSH_AUTH_SESSION_EVENT_READ_RESULT = 8,
	XSSH_AUTH_SESSION_EVENT_COMPLETE = 9,
	XSSH_AUTH_SESSION_EVENT_FAILED = 10
} xsshauthsessionevent;



/* Packet 对通用认证编排分类，METHOD 的字段由具体认证模块解释。 */
typedef enum xsshauthsessionpacket {
	XSSH_AUTH_SESSION_PACKET_NONE = 0,
	XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST = 1,
	XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT = 2,
	XSSH_AUTH_SESSION_PACKET_REQUEST = 3,
	XSSH_AUTH_SESSION_PACKET_FAILURE = 4,
	XSSH_AUTH_SESSION_PACKET_SUCCESS = 5,
	XSSH_AUTH_SESSION_PACKET_BANNER = 6,
	XSSH_AUTH_SESSION_PACKET_METHOD = 7
} xsshauthsessionpacket;



/*
	对象不拥有 payload；Request、Failure、Banner 和 Method 只在读事务期间借用输入。
	同一对象由一个执行流推进，认证后端需要异步决策时由调用方复制所需字段。
*/
typedef struct xsshauthsession {
	xsshauthguard Budget;
	xsshauthguard PendingBudget;
	xsshauthrequest Request;
	xsshauthfailure Failure;
	xsshauthbanner Banner;
	xbytesview Method;
	uint64 WriteOrdinal;
	uint64 ReadOrdinal;
	xsshrole Role;
	xsshauthsessionphase Phase;
	xsshauthsessionevent Event;
	xsshauthsessionpacket WritePending;
	xsshauthsessionpacket ReadPending;
	bool Active;
	bool ServiceAccepted;
	bool ContinueAllowed;
	uint32 ObjectGuard;
} xsshauthsession;



/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 初始化不拥有外部资源的 client 或 server 认证会话。 */
XRT_API bool xrtSshAuthSessionInit(
	xsshauthsession* pSession,
	xsshrole Role
);



/* 清除会话状态和当前借用视图，不处理 transport 或调用方缓冲。 */
XRT_API void xrtSshAuthSessionClear(xsshauthsession* pSession);



/* 在首轮 KEX 完成后开始 service 与 USERAUTH 编排。 */
XRT_API xsshcode xrtSshAuthSessionBegin(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	const xsshauthguardpolicy* pPolicy,
	double Timer
);



/* 返回当前最常见的下一动作，不推进任何状态。 */
XRT_API xsshauthsessionevent xrtSshAuthSessionEvent(
	const xsshauthsession* pSession
);



/* 复制当前认证资源预算，输出不借用会话内部地址。 */
XRT_API xsshcode xrtSshAuthSessionBudget(
	const xsshauthsession* pSession,
	xsshauthguard* pBudget
);



/* 检查认证超时和资源预算；耗尽时会话进入失败状态。 */
XRT_API xsshcode xrtSshAuthSessionCheck(
	xsshauthsession* pSession,
	double Timer,
	xsshauthguarddecision* pDecision
);



/*
	验证一个已经构建的 service、USERAUTH 或方法 payload，并准备写事务。
	函数不复制、不修改 payload，也不生成 packet framing。
*/
XRT_API xsshcode xrtSshAuthSessionWritePrepare(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	double Timer
);



/* transport core 已可靠提交当前输出后提交认证写事务。 */
XRT_API xsshcode xrtSshAuthSessionWriteCommit(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃尚未交给 transport 的认证输出，不消费状态或资源预算。 */
XRT_API xsshcode xrtSshAuthSessionWriteAbort(xsshauthsession* pSession);



/*
	解析 transport core 已认证准备的 peer payload，并返回通用消息类别。
	成功后必须先读取所需借用视图，再依次提交 core 和 session。
*/
XRT_API xsshcode xrtSshAuthSessionReadPrepare(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	double Timer,
	xsshauthsessionpacket* pPacket
);



/* 返回当前待读事务中的通用 USERAUTH_REQUEST 借用视图。 */
XRT_API xsshcode xrtSshAuthSessionRequest(
	const xsshauthsession* pSession,
	xsshauthrequest* pRequest
);



/* 返回当前待读事务中的 USERAUTH_FAILURE 借用视图。 */
XRT_API xsshcode xrtSshAuthSessionFailure(
	const xsshauthsession* pSession,
	xsshauthfailure* pFailure
);



/* 返回当前待读事务中的 USERAUTH_BANNER 借用视图。 */
XRT_API xsshcode xrtSshAuthSessionBanner(
	const xsshauthsession* pSession,
	xsshauthbanner* pBanner
);



/* 返回当前待读事务中的方法专用完整 payload。 */
XRT_API xsshcode xrtSshAuthSessionMethod(
	const xsshauthsession* pSession,
	xbytesview* pPayload
);



/* transport core 已提交当前输入后提交认证读事务。 */
XRT_API xsshcode xrtSshAuthSessionReadCommit(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃已认证但不能接受的认证输入，并把会话置为失败状态。 */
XRT_API xsshcode xrtSshAuthSessionReadAbort(xsshauthsession* pSession);



/* 显式终止认证编排；重复调用保持失败状态。 */
XRT_API void xrtSshAuthSessionFail(xsshauthsession* pSession);



/* 判断本端会话、预算和 transport 的 server 成功方向都已提交。 */
XRT_API bool xrtSshAuthSessionComplete(
	const xsshauthsession* pSession,
	const xsshtransportcore* pCore
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_connection_message.h */
/* ========================================================================== */

#ifndef XRT_SSH_CONNECTION_MESSAGE_H
#define XRT_SSH_CONNECTION_MESSAGE_H




#if defined(XSSH_FEATURE_CONNECTION_MESSAGE) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_CONNECTION_MESSAGE requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_CONNECTION_MESSAGE)

#define XSSH_MSG_GLOBAL_REQUEST 80u
#define XSSH_MSG_REQUEST_SUCCESS 81u
#define XSSH_MSG_REQUEST_FAILURE 82u



/* 全局请求借用完整 payload，未知类型字段保持原始字节。 */
typedef struct xsshglobalrequest {
	xstrview Name;
	bool WantReply;
	xbytesview Fields;
} xsshglobalrequest;



XRT_EXTERN_C_BEGIN



/* 写入或严格读取可扩展全局请求。 */
XRT_API xsshcode xrtSshGlobalRequestWrite(
	xsshwriter* pWriter,
	xstrview Name,
	bool bWantReply,
	xbytesview Fields
);
XRT_API xsshcode xrtSshGlobalRequestRead(
	xbytesview Payload,
	xsshglobalrequest* pRequest
);



/* 写入或严格读取带任意请求专用数据的成功响应。 */
XRT_API xsshcode xrtSshGlobalSuccessWrite(
	xsshwriter* pWriter,
	xbytesview Fields
);
XRT_API xsshcode xrtSshGlobalSuccessRead(
	xbytesview Payload,
	xbytesview* pFields
);



/* 写入或严格读取无字段失败响应。 */
XRT_API xsshcode xrtSshGlobalFailureWrite(xsshwriter* pWriter);
XRT_API xsshcode xrtSshGlobalFailureRead(xbytesview Payload);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_channel_message.h */
/* ========================================================================== */

#ifndef XRT_SSH_CHANNEL_MESSAGE_H
#define XRT_SSH_CHANNEL_MESSAGE_H




#if defined(XSSH_FEATURE_CHANNEL_MESSAGE) && \
	(!defined(XSSH_FEATURE_WIRE) || !defined(XRT_FEATURE_UNICODE))
	#error "XSSH_FEATURE_CHANNEL_MESSAGE requires XSSH_FEATURE_WIRE and XRT_FEATURE_UNICODE"
#endif



#if defined(XSSH_FEATURE_CHANNEL_MESSAGE)

#define XSSH_MSG_CHANNEL_OPEN 90u
#define XSSH_MSG_CHANNEL_OPEN_CONFIRMATION 91u
#define XSSH_MSG_CHANNEL_OPEN_FAILURE 92u
#define XSSH_MSG_CHANNEL_WINDOW_ADJUST 93u
#define XSSH_MSG_CHANNEL_DATA 94u
#define XSSH_MSG_CHANNEL_EXTENDED_DATA 95u
#define XSSH_MSG_CHANNEL_EOF 96u
#define XSSH_MSG_CHANNEL_CLOSE 97u
#define XSSH_MSG_CHANNEL_REQUEST 98u
#define XSSH_MSG_CHANNEL_SUCCESS 99u
#define XSSH_MSG_CHANNEL_FAILURE 100u

#define XSSH_CHANNEL_OPEN_ADMINISTRATIVELY_PROHIBITED 1u
#define XSSH_CHANNEL_OPEN_CONNECT_FAILED 2u
#define XSSH_CHANNEL_OPEN_UNKNOWN_CHANNEL_TYPE 3u
#define XSSH_CHANNEL_OPEN_RESOURCE_SHORTAGE 4u

#define XSSH_CHANNEL_EXTENDED_DATA_STDERR 1u



/* Channel open 借用类型专用字段，不限制扩展 channel 类型。 */
typedef struct xsshchannelopen {
	xstrview Type;
	uint32 Sender;
	uint32 Window;
	uint32 MaxPacket;
	xbytesview Fields;
} xsshchannelopen;



/* Channel open confirmation 借用类型专用确认字段。 */
typedef struct xsshchannelconfirmation {
	uint32 Recipient;
	uint32 Sender;
	uint32 Window;
	uint32 MaxPacket;
	xbytesview Fields;
} xsshchannelconfirmation;



/* Channel open failure 借用 UTF-8 描述和 ASCII language tag。 */
typedef struct xsshchannelopenfailure {
	uint32 Recipient;
	uint32 Reason;
	xstrview Description;
	xstrview Language;
} xsshchannelopenfailure;



/* Window adjust 保留完整 uint32 增量。 */
typedef struct xsshchanneladjust {
	uint32 Recipient;
	uint32 Bytes;
} xsshchanneladjust;



/* 普通 channel data 借用二进制 string 内容。 */
typedef struct xsshchanneldata {
	uint32 Recipient;
	xbytesview Data;
} xsshchanneldata;



/* Extended data 保留未知类型码与二进制内容。 */
typedef struct xsshchannelextendeddata {
	uint32 Recipient;
	uint32 Type;
	xbytesview Data;
} xsshchannelextendeddata;



/* Channel request 借用未知请求的全部专用字段。 */
typedef struct xsshchannelrequest {
	uint32 Recipient;
	xstrview Type;
	bool WantReply;
	xbytesview Fields;
} xsshchannelrequest;



XRT_EXTERN_C_BEGIN



/* 写入或读取可扩展 channel open。 */
XRT_API xsshcode xrtSshChannelOpenWrite(
	xsshwriter* pWriter,
	xstrview Type,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Fields
);
XRT_API xsshcode xrtSshChannelOpenRead(
	xbytesview Payload,
	xsshchannelopen* pOpen
);



/* 写入或读取带类型专用字段的 channel open confirmation。 */
XRT_API xsshcode xrtSshChannelOpenConfirmationWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Fields
);
XRT_API xsshcode xrtSshChannelOpenConfirmationRead(
	xbytesview Payload,
	xsshchannelconfirmation* pConfirmation
);



/* 写入或读取 channel open failure。 */
XRT_API xsshcode xrtSshChannelOpenFailureWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iReason,
	xstrview Description,
	xstrview Language
);
XRT_API xsshcode xrtSshChannelOpenFailureRead(
	xbytesview Payload,
	xsshchannelopenfailure* pFailure
);



/* 写入或读取 channel window adjust。 */
XRT_API xsshcode xrtSshChannelWindowAdjustWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iBytes
);
XRT_API xsshcode xrtSshChannelWindowAdjustRead(
	xbytesview Payload,
	xsshchanneladjust* pAdjust
);



/* 写入或读取普通 channel data。 */
XRT_API xsshcode xrtSshChannelDataWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xbytesview Data
);
XRT_API xsshcode xrtSshChannelDataRead(
	xbytesview Payload,
	xsshchanneldata* pData
);



/* 写入或读取带类型码的 channel extended data。 */
XRT_API xsshcode xrtSshChannelExtendedDataWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iType,
	xbytesview Data
);
XRT_API xsshcode xrtSshChannelExtendedDataRead(
	xbytesview Payload,
	xsshchannelextendeddata* pData
);



/* 写入或严格读取 channel EOF。 */
XRT_API xsshcode xrtSshChannelEofWrite(
	xsshwriter* pWriter,
	uint32 iRecipient
);
XRT_API xsshcode xrtSshChannelEofRead(
	xbytesview Payload,
	uint32* pRecipient
);



/* 写入或严格读取 channel close。 */
XRT_API xsshcode xrtSshChannelCloseWrite(
	xsshwriter* pWriter,
	uint32 iRecipient
);
XRT_API xsshcode xrtSshChannelCloseRead(
	xbytesview Payload,
	uint32* pRecipient
);



/* 写入或读取保留未知专用字段的 channel request。 */
XRT_API xsshcode xrtSshChannelRequestWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Type,
	bool bWantReply,
	xbytesview Fields
);
XRT_API xsshcode xrtSshChannelRequestRead(
	xbytesview Payload,
	xsshchannelrequest* pRequest
);



/* 写入或严格读取 channel request success。 */
XRT_API xsshcode xrtSshChannelSuccessWrite(
	xsshwriter* pWriter,
	uint32 iRecipient
);
XRT_API xsshcode xrtSshChannelSuccessRead(
	xbytesview Payload,
	uint32* pRecipient
);



/* 写入或严格读取 channel request failure。 */
XRT_API xsshcode xrtSshChannelFailureWrite(
	xsshwriter* pWriter,
	uint32 iRecipient
);
XRT_API xsshcode xrtSshChannelFailureRead(
	xbytesview Payload,
	uint32* pRecipient
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_channel_window.h */
/* ========================================================================== */

#ifndef XRT_SSH_CHANNEL_WINDOW_H
#define XRT_SSH_CHANNEL_WINDOW_H




#if defined(XSSH_FEATURE_CHANNEL_WINDOW) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_CHANNEL_WINDOW requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_CHANNEL_WINDOW)

/* 单个 channel 的双向窗口状态；字段只读，更新必须经过公开操作。 */
typedef struct xsshchannelwindow {
	uint32 SendWindow;
	uint32 SendMaxPacket;
	uint32 ReceiveWindow;
	uint32 ReceiveMaxPacket;
	uint32 AdjustThreshold;
	uint64 ReceiveBuffered;
	uint64 ReceivePending;
} xsshchannelwindow;



XRT_EXTERN_C_BEGIN



/* 初始化远端发送额度和本地接收额度，不分配缓冲。 */
XRT_API bool xrtSshChannelWindowInit(
	xsshchannelwindow* pWindow,
	uint32 iSendWindow,
	uint32 iSendMaxPacket,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
);



/* 返回下一条普通或扩展数据消息可发送的最大字节数。 */
XRT_API uint32 xrtSshChannelSendLimit(const xsshchannelwindow* pWindow);



/* 提交已排队的数据字节，并扣减远端窗口。 */
XRT_API xsshcode xrtSshChannelSendCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



/* 应用远端 WINDOW_ADJUST，溢出视为协议错误。 */
XRT_API xsshcode xrtSshChannelSendAdjust(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



/* 接收一条数据消息并校验本地窗口与最大包限制。 */
XRT_API xsshcode xrtSshChannelReceiveCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



/* 标记应用已经消费的接收字节，使额度可稍后返还。 */
XRT_API xsshcode xrtSshChannelReceiveConsume(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



/* 判断已消费额度是否达到返还阈值或本地窗口已经耗尽。 */
XRT_API bool xrtSshChannelReceiveAdjustReady(
	const xsshchannelwindow* pWindow
);



/* 返回当前单条 WINDOW_ADJUST 可安全返还的最大额度。 */
XRT_API uint32 xrtSshChannelReceiveAdjustLimit(
	const xsshchannelwindow* pWindow
);



/* 在 WINDOW_ADJUST 已可靠排队后提交返还额度。 */
XRT_API xsshcode xrtSshChannelReceiveAdjustCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



/* 提交独立于已消费字节的新接收额度，用于动态扩容或零窗口恢复。 */
XRT_API xsshcode xrtSshChannelReceiveGrantCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_channel_request.h */
/* ========================================================================== */

#ifndef XRT_SSH_CHANNEL_REQUEST_H
#define XRT_SSH_CHANNEL_REQUEST_H




#if defined(XSSH_FEATURE_CHANNEL_REQUEST) && \
	!defined(XSSH_FEATURE_CHANNEL_MESSAGE)
	#error "XSSH_FEATURE_CHANNEL_REQUEST requires XSSH_FEATURE_CHANNEL_MESSAGE"
#endif



#if defined(XSSH_FEATURE_CHANNEL_REQUEST)

#define XSSH_CHANNEL_REQUEST_ENV "env"
#define XSSH_CHANNEL_REQUEST_SHELL "shell"
#define XSSH_CHANNEL_REQUEST_EXEC "exec"
#define XSSH_CHANNEL_REQUEST_SUBSYSTEM "subsystem"
#define XSSH_CHANNEL_REQUEST_XON_XOFF "xon-xoff"
#define XSSH_CHANNEL_REQUEST_WINDOW_CHANGE "window-change"
#define XSSH_CHANNEL_REQUEST_SIGNAL "signal"
#define XSSH_CHANNEL_REQUEST_EXIT_STATUS "exit-status"
#define XSSH_CHANNEL_REQUEST_EXIT_SIGNAL "exit-signal"
#define XSSH_CHANNEL_REQUEST_BREAK "break"



/* Env request 借用不限定编码的名称和值。 */
typedef struct xsshchannelenv {
	xbytesview Name;
	xbytesview Value;
} xsshchannelenv;



/* Window-change request 保留字符和像素两个尺寸系统。 */
typedef struct xsshchannelwindowchange {
	uint32 Columns;
	uint32 Rows;
	uint32 PixelWidth;
	uint32 PixelHeight;
} xsshchannelwindowchange;



/* Exit-signal request 借用规范信号名、UTF-8 描述和 language tag。 */
typedef struct xsshchannelexitsignal {
	xstrview Signal;
	bool CoreDumped;
	xstrview Message;
	xstrview Language;
} xsshchannelexitsignal;



XRT_EXTERN_C_BEGIN



/* 校验 RFC channel signal 名称，并拒绝多余的 SIG 前缀。 */
XRT_API bool xrtSshChannelSignalValid(xstrview Signal);



/* 写入或严格读取无专用字段的 shell request。 */
XRT_API xsshcode xrtSshChannelShellWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply
);
XRT_API xsshcode xrtSshChannelShellRead(
	const xsshchannelrequest* pRequest
);



/* 写入或严格读取不限定编码的 exec command。 */
XRT_API xsshcode xrtSshChannelExecWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Command
);
XRT_API xsshcode xrtSshChannelExecRead(
	const xsshchannelrequest* pRequest,
	xbytesview* pCommand
);



/* 写入或严格读取不限定编码的 subsystem 名称。 */
XRT_API xsshcode xrtSshChannelSubsystemWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Subsystem
);
XRT_API xsshcode xrtSshChannelSubsystemRead(
	const xsshchannelrequest* pRequest,
	xbytesview* pSubsystem
);



/* 写入或严格读取 env 名称和值。 */
XRT_API xsshcode xrtSshChannelEnvWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Name,
	xbytesview Value
);
XRT_API xsshcode xrtSshChannelEnvRead(
	const xsshchannelrequest* pRequest,
	xsshchannelenv* pEnv
);



/* 写入或严格读取不要求回复的 xon-xoff 通知。 */
XRT_API xsshcode xrtSshChannelXonXoffWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bClientCanDo
);
XRT_API xsshcode xrtSshChannelXonXoffRead(
	const xsshchannelrequest* pRequest,
	bool* pClientCanDo
);



/* 写入或严格读取不要求回复的终端尺寸变更。 */
XRT_API xsshcode xrtSshChannelWindowChangeWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight
);
XRT_API xsshcode xrtSshChannelWindowChangeRead(
	const xsshchannelrequest* pRequest,
	xsshchannelwindowchange* pChange
);



/* 写入或严格读取不要求回复的信号通知。 */
XRT_API xsshcode xrtSshChannelSignalWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Signal
);
XRT_API xsshcode xrtSshChannelSignalRead(
	const xsshchannelrequest* pRequest,
	xstrview* pSignal
);



/* 写入或严格读取 RFC 4335 break request。 */
XRT_API xsshcode xrtSshChannelBreakWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	uint32 iLengthMs
);
XRT_API xsshcode xrtSshChannelBreakRead(
	const xsshchannelrequest* pRequest,
	uint32* pLengthMs
);



/* 写入或严格读取不要求回复的进程退出状态。 */
XRT_API xsshcode xrtSshChannelExitStatusWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iStatus
);
XRT_API xsshcode xrtSshChannelExitStatusRead(
	const xsshchannelrequest* pRequest,
	uint32* pStatus
);



/* 写入或严格读取不要求回复的进程退出信号。 */
XRT_API xsshcode xrtSshChannelExitSignalWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Signal,
	bool bCoreDumped,
	xstrview Message,
	xstrview Language
);
XRT_API xsshcode xrtSshChannelExitSignalRead(
	const xsshchannelrequest* pRequest,
	xsshchannelexitsignal* pSignal
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_channel_pty.h */
/* ========================================================================== */

#ifndef XRT_SSH_CHANNEL_PTY_H
#define XRT_SSH_CHANNEL_PTY_H




#if defined(XSSH_FEATURE_CHANNEL_PTY) && \
	!defined(XSSH_FEATURE_CHANNEL_REQUEST)
	#error "XSSH_FEATURE_CHANNEL_PTY requires XSSH_FEATURE_CHANNEL_REQUEST"
#endif



#if defined(XSSH_FEATURE_CHANNEL_PTY)

#define XSSH_CHANNEL_REQUEST_PTY "pty-req"

#define XSSH_TTY_OP_END 0u
#define XSSH_TTY_OP_VINTR 1u
#define XSSH_TTY_OP_VQUIT 2u
#define XSSH_TTY_OP_VERASE 3u
#define XSSH_TTY_OP_VKILL 4u
#define XSSH_TTY_OP_VEOF 5u
#define XSSH_TTY_OP_VEOL 6u
#define XSSH_TTY_OP_VEOL2 7u
#define XSSH_TTY_OP_VSTART 8u
#define XSSH_TTY_OP_VSTOP 9u
#define XSSH_TTY_OP_VSUSP 10u
#define XSSH_TTY_OP_VDSUSP 11u
#define XSSH_TTY_OP_VREPRINT 12u
#define XSSH_TTY_OP_VWERASE 13u
#define XSSH_TTY_OP_VLNEXT 14u
#define XSSH_TTY_OP_VFLUSH 15u
#define XSSH_TTY_OP_VSWTCH 16u
#define XSSH_TTY_OP_VSTATUS 17u
#define XSSH_TTY_OP_VDISCARD 18u
#define XSSH_TTY_OP_IGNPAR 30u
#define XSSH_TTY_OP_PARMRK 31u
#define XSSH_TTY_OP_INPCK 32u
#define XSSH_TTY_OP_ISTRIP 33u
#define XSSH_TTY_OP_INLCR 34u
#define XSSH_TTY_OP_IGNCR 35u
#define XSSH_TTY_OP_ICRNL 36u
#define XSSH_TTY_OP_IUCLC 37u
#define XSSH_TTY_OP_IXON 38u
#define XSSH_TTY_OP_IXANY 39u
#define XSSH_TTY_OP_IXOFF 40u
#define XSSH_TTY_OP_IMAXBEL 41u
#define XSSH_TTY_OP_ISIG 50u
#define XSSH_TTY_OP_ICANON 51u
#define XSSH_TTY_OP_XCASE 52u
#define XSSH_TTY_OP_ECHO 53u
#define XSSH_TTY_OP_ECHOE 54u
#define XSSH_TTY_OP_ECHOK 55u
#define XSSH_TTY_OP_ECHONL 56u
#define XSSH_TTY_OP_NOFLSH 57u
#define XSSH_TTY_OP_TOSTOP 58u
#define XSSH_TTY_OP_IEXTEN 59u
#define XSSH_TTY_OP_ECHOCTL 60u
#define XSSH_TTY_OP_ECHOKE 61u
#define XSSH_TTY_OP_PENDIN 62u
#define XSSH_TTY_OP_OPOST 70u
#define XSSH_TTY_OP_OLCUC 71u
#define XSSH_TTY_OP_ONLCR 72u
#define XSSH_TTY_OP_OCRNL 73u
#define XSSH_TTY_OP_ONOCR 74u
#define XSSH_TTY_OP_ONLRET 75u
#define XSSH_TTY_OP_CS7 90u
#define XSSH_TTY_OP_CS8 91u
#define XSSH_TTY_OP_PARENB 92u
#define XSSH_TTY_OP_PARODD 93u
#define XSSH_TTY_OP_ISPEED 128u
#define XSSH_TTY_OP_OSPEED 129u
#define XSSH_TTY_OP_UNSUPPORTED_MIN 160u



/* 单个 terminal mode 保留 opcode 和 uint32 参数。 */
typedef struct xsshterminalmode {
	uint8 Opcode;
	uint32 Value;
} xsshterminalmode;



/* 已完整验证的 terminal mode 借用迭代器，没有固定数量上限。 */
typedef struct xsshterminalmodes {
	xsshreader Reader;
	size_t Count;
	size_t Index;
	bool Unsupported;
} xsshterminalmodes;



/* PTY request 借用终端名称和原始 mode stream。 */
typedef struct xsshchannelpty {
	xbytesview Terminal;
	uint32 Columns;
	uint32 Rows;
	uint32 PixelWidth;
	uint32 PixelHeight;
	xbytesview Modes;
} xsshchannelpty;



XRT_EXTERN_C_BEGIN



/* 向调用方缓冲追加一个 opcode/value terminal mode。 */
XRT_API xsshcode xrtSshTerminalModeWrite(
	xsshwriter* pWriter,
	uint8 iOpcode,
	uint32 iValue
);



/* 向 terminal mode stream 追加 TTY_OP_END。 */
XRT_API xsshcode xrtSshTerminalModeEnd(xsshwriter* pWriter);



/* 完整验证并初始化无固定数量上限的 terminal mode 迭代器。 */
XRT_API xsshcode xrtSshTerminalModesRead(
	xbytesview Modes,
	xsshterminalmodes* pModes
);



/* 返回下一项已验证 terminal mode；迭代结束返回 false。 */
XRT_API bool xrtSshTerminalModesNext(
	xsshterminalmodes* pModes,
	xsshterminalmode* pMode
);



/* 写入或严格读取 PTY request。 */
XRT_API xsshcode xrtSshChannelPtyWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Terminal,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight,
	xbytesview Modes
);
XRT_API xsshcode xrtSshChannelPtyRead(
	const xsshchannelrequest* pRequest,
	xsshchannelpty* pPty
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_channel_state.h */
/* ========================================================================== */

#ifndef XRT_SSH_CHANNEL_STATE_H
#define XRT_SSH_CHANNEL_STATE_H




#if defined(XSSH_FEATURE_CHANNEL_STATE) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_CHANNEL_STATE requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_CHANNEL_STATE)

/* Channel 生命周期只记录双向 EOF/CLOSE，不拥有网络和缓冲。 */
typedef struct xsshchannelstate {
	bool LocalEof;
	bool RemoteEof;
	bool LocalClose;
	bool RemoteClose;
	bool Initialized;
} xsshchannelstate;



XRT_EXTERN_C_BEGIN



/* 初始化已经完成 open confirmation 的 channel 生命周期。 */
XRT_API bool xrtSshChannelStateInit(xsshchannelstate* pState);



/* 判断本端是否仍可发送 data 或 extended-data。 */
XRT_API bool xrtSshChannelCanSendData(const xsshchannelstate* pState);



/* 判断远端 data 或 extended-data 是否仍可被接受。 */
XRT_API bool xrtSshChannelCanReceiveData(const xsshchannelstate* pState);



/* 判断本端是否仍可发送 channel request。 */
XRT_API bool xrtSshChannelCanSendRequest(const xsshchannelstate* pState);



/* 判断收到远端 close 后是否仍需排队本端 close。 */
XRT_API bool xrtSshChannelCloseReplyNeeded(const xsshchannelstate* pState);



/* 判断双向 close 握手是否完成，可以回收 channel slot。 */
XRT_API bool xrtSshChannelClosed(const xsshchannelstate* pState);



/* 在本端 EOF 已可靠排队后提交单向发送结束。 */
XRT_API xsshcode xrtSshChannelLocalEofCommit(xsshchannelstate* pState);



/* 接收远端 EOF；重复或 close 后 EOF 是协议错误。 */
XRT_API xsshcode xrtSshChannelRemoteEofCommit(xsshchannelstate* pState);



/* 在本端 close 已可靠排队后提交本端关闭。 */
XRT_API xsshcode xrtSshChannelLocalCloseCommit(xsshchannelstate* pState);



/* 接收远端 close；调用方随后按需回复 close。 */
XRT_API xsshcode xrtSshChannelRemoteCloseCommit(xsshchannelstate* pState);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_channel_core.h */
/* ========================================================================== */

#ifndef XRT_SSH_CHANNEL_CORE_H
#define XRT_SSH_CHANNEL_CORE_H




#if defined(XSSH_FEATURE_CHANNEL_CORE) && \
	(!defined(XSSH_FEATURE_CHANNEL_MESSAGE) || \
	 !defined(XSSH_FEATURE_CHANNEL_STATE) || \
	 !defined(XSSH_FEATURE_CHANNEL_WINDOW))
	#error "XSSH_FEATURE_CHANNEL_CORE requires channel message, state and window"
#endif



#if defined(XSSH_FEATURE_CHANNEL_CORE)

/* Channel core 阶段不表达应用缓冲、请求处理或网络等待。 */
typedef enum xsshchannelcorephase {
	XSSH_CHANNEL_CORE_OPENING = 0,
	XSSH_CHANNEL_CORE_ACCEPTING = 1,
	XSSH_CHANNEL_CORE_OPEN = 2,
	XSSH_CHANNEL_CORE_FAILED = 3,
	XSSH_CHANNEL_CORE_CLOSED = 4
} xsshchannelcorephase;



/* 单个 channel 只保存编号、窗口和关闭状态，不拥有 payload 或数据队列。 */
typedef struct xsshchannelcore {
	xsshchannelwindow Window;
	xsshchannelstate State;
	uint32 Local;
	uint32 Remote;
	uint32 FailureReason;
	xsshchannelcorephase Phase;
	bool Initialized;
} xsshchannelcore;



XRT_EXTERN_C_BEGIN



/* 初始化等待 peer confirmation 的本端 channel open。 */
XRT_API bool xrtSshChannelCoreOpenInit(
	xsshchannelcore* pChannel,
	uint32 iLocal,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
);



/* 从已解析 peer open 初始化等待本端 confirmation 的 channel。 */
XRT_API bool xrtSshChannelCoreAcceptInit(
	xsshchannelcore* pChannel,
	uint32 iLocal,
	const xsshchannelopen* pOpen,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
);



/* 清除 channel core；不会清理调用方数据或 request reply 队列。 */
XRT_API void xrtSshChannelCoreClear(xsshchannelcore* pChannel);



/* 返回公开 channel 阶段；无效对象返回 FAILED。 */
XRT_API xsshchannelcorephase xrtSshChannelCorePhase(
	const xsshchannelcore* pChannel
);



/* 返回本端 channel id；远端 id 仅在 accepting/open/closed 阶段可用。 */
XRT_API bool xrtSshChannelCoreIds(
	const xsshchannelcore* pChannel,
	uint32* pLocal,
	uint32* pRemote
);



/* 提交 peer 对本端 open 的 confirmation，并原子开放数据面。 */
XRT_API xsshcode xrtSshChannelCoreConfirmationCommit(
	xsshchannelcore* pChannel,
	const xsshchannelconfirmation* pConfirmation
);



/* 提交 peer 对本端 open 的 failure，并保留失败 reason。 */
XRT_API xsshcode xrtSshChannelCoreFailureCommit(
	xsshchannelcore* pChannel,
	const xsshchannelopenfailure* pFailure
);



/* 本端 confirmation 已可靠排队后提交 peer open。 */
XRT_API xsshcode xrtSshChannelCoreAcceptCommit(xsshchannelcore* pChannel);



/* 本端 failure 已可靠排队后提交拒绝结果。 */
XRT_API xsshcode xrtSshChannelCoreRejectCommit(
	xsshchannelcore* pChannel,
	uint32 iReason
);



/* 判断数据面是否已打开或双向 close 是否已经完成。 */
XRT_API bool xrtSshChannelCoreOpen(const xsshchannelcore* pChannel);
XRT_API bool xrtSshChannelCoreClosed(const xsshchannelcore* pChannel);



/* 返回下一条 data/extended-data payload 可发送的最大字节数。 */
XRT_API uint32 xrtSshChannelCoreSendLimit(
	const xsshchannelcore* pChannel
);



/* data 可靠排队后扣减远端窗口；EOF/CLOSE 后拒绝新数据。 */
XRT_API xsshcode xrtSshChannelCoreDataSendCommit(
	xsshchannelcore* pChannel,
	uint32 iBytes
);



/* 提交已验证的 peer data，并校验本端 recipient、窗口和生命周期。 */
XRT_API xsshcode xrtSshChannelCoreDataReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient,
	uint32 iBytes
);



/* 应用消费已接收数据；close 后仍可释放此前缓冲的数据。 */
XRT_API xsshcode xrtSshChannelCoreDataConsume(
	xsshchannelcore* pChannel,
	uint32 iBytes
);



/* 判断是否应发送 WINDOW_ADJUST，并返回当前可安全返还额度。 */
XRT_API bool xrtSshChannelCoreAdjustReady(
	const xsshchannelcore* pChannel
);
XRT_API uint32 xrtSshChannelCoreAdjustLimit(
	const xsshchannelcore* pChannel
);



/* WINDOW_ADJUST 可靠排队后提交本端返还额度。 */
XRT_API xsshcode xrtSshChannelCoreAdjustSendCommit(
	xsshchannelcore* pChannel,
	uint32 iBytes
);



/* 提交已验证的 peer WINDOW_ADJUST。 */
XRT_API xsshcode xrtSshChannelCoreAdjustReceiveCommit(
	xsshchannelcore* pChannel,
	const xsshchanneladjust* pAdjust
);



/* 判断当前方向是否还能发送或接收 channel request。 */
XRT_API bool xrtSshChannelCoreCanSendRequest(
	const xsshchannelcore* pChannel
);
XRT_API bool xrtSshChannelCoreCanReceiveRequest(
	const xsshchannelcore* pChannel
);



/* 校验已解析 peer channel 消息的 recipient，不推进状态。 */
XRT_API xsshcode xrtSshChannelCoreRecipientCheck(
	const xsshchannelcore* pChannel,
	uint32 iRecipient
);



/* EOF 可靠排队或验证接收后提交对应方向的半关闭。 */
XRT_API xsshcode xrtSshChannelCoreEofSendCommit(
	xsshchannelcore* pChannel
);
XRT_API xsshcode xrtSshChannelCoreEofReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient
);



/* CLOSE 可靠排队或验证接收后提交；双向完成时进入 CLOSED。 */
XRT_API xsshcode xrtSshChannelCoreCloseSendCommit(
	xsshchannelcore* pChannel
);
XRT_API xsshcode xrtSshChannelCoreCloseReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_channel_io.h */
/* ========================================================================== */

#ifndef XRT_SSH_CHANNEL_IO_H
#define XRT_SSH_CHANNEL_IO_H




#if defined(XSSH_FEATURE_CHANNEL_IO) && \
	(!defined(XSSH_FEATURE_CHANNEL_CORE) || \
	 !defined(XRT_FEATURE_NET_BUFFER))
	#error "XSSH_FEATURE_CHANNEL_IO requires channel core and XRT network buffer"
#endif



#if defined(XSSH_FEATURE_CHANNEL_IO)

#define XSSH_CHANNEL_IO_LIMIT_DEFAULT 2097152u



/* DATA 表示普通 channel data，STDERR 表示 RFC 4254 标准错误扩展流。 */
typedef enum xsshchanneliostream {
	XSSH_CHANNEL_IO_DATA = 0,
	XSSH_CHANNEL_IO_STDERR = 1
} xsshchanneliostream;



/* 待提交类型只允许一个收发事务，避免同一 channel 的窗口与队首交错变化。 */
typedef enum xsshchanneliopending {
	XSSH_CHANNEL_IO_PENDING_NONE = 0,
	XSSH_CHANNEL_IO_PENDING_RECEIVE = 1,
	XSSH_CHANNEL_IO_PENDING_SEND = 2
} xsshchanneliopending;



/* 收发限制分别约束两条流的总量；默认值均为 2 MiB。 */
typedef struct xsshchannelioconfig {
	size_t ReceiveLimit;
	size_t SendLimit;
} xsshchannelioconfig;



/*
	对象只拥有动态缓冲链，并借用一个 channel core；不拥有网络、packet 或请求队列。
	字段公开用于诊断，缓冲内容只能通过本模块和 xnetbuf 的只读视图访问。
*/
typedef struct xsshchannelio {
	xnetbuf ReceiveData;
	xnetbuf ReceiveError;
	xnetbuf SendData;
	xnetbuf SendError;
	xnetbuf ReceiveStaging;
	xsshchannelcore ChannelBefore;
	xsshchannelcore ChannelAfter;
	xsshchannelcore* Channel;
	cbytes SendHead;
	size_t ReceiveLimit;
	size_t SendLimit;
	size_t PendingBytes;
	xsshchanneliostream PendingStream;
	xsshchanneliopending Pending;
	bool Initialized;
	uint32 Guard;
} xsshchannelio;



XRT_EXTERN_C_BEGIN



/* 写入 2 MiB 收发硬上限。 */
XRT_API void xrtSshChannelIoConfigInit(xsshchannelioconfig* pConfig);



/*
	绑定一个尚无已接收数据的 channel，并用同一缓冲池初始化五条动态链。
	ReceiveLimit 必须覆盖当前已通告接收窗口；空配置使用默认值。
*/
XRT_API bool xrtSshChannelIoInit(
	xsshchannelio* pIo,
	xnetbufpool* pPool,
	xsshchannelcore* pChannel,
	const xsshchannelioconfig* pConfig
);



/* 释放全部动态块；未读取数据按应用丢弃处理并转入 channel 待返还额度。 */
XRT_API void xrtSshChannelIoClear(xsshchannelio* pIo);



/* 返回指定接收流当前已经可靠提交的可读字节数。 */
XRT_API size_t xrtSshChannelIoReadable(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
);



/* 返回指定接收流的借用只读缓冲；对象失效或流类型错误时返回空。 */
XRT_API const xnetbuf* xrtSshChannelIoReadBuffer(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
);



/* 复制并消费最多 Capacity 字节，同时把实际消费量转入接收窗口待返还额度。 */
XRT_API xsshcode xrtSshChannelIoRead(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	void* pOutput,
	size_t iCapacity,
	size_t* pRead
);



/* 零复制消费指定接收流的前缀，并更新 channel 接收窗口计数。 */
XRT_API xsshcode xrtSshChannelIoConsume(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	size_t iSize
);



/* 返回指定发送流当前排队字节数。 */
XRT_API size_t xrtSshChannelIoQueued(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
);



/* 返回两条发送流共享硬上限中尚可受理的字节数。 */
XRT_API size_t xrtSshChannelIoWritable(const xsshchannelio* pIo);



/* 返回指定流下一条消息可发送的连续队首字节数，零表示无数据或远端窗口阻塞。 */
XRT_API size_t xrtSshChannelIoSendLimit(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
);



/* 在共享发送预算内追加一份数据副本。 */
XRT_API xsshcode xrtSshChannelIoWrite(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize
);



/* 追加借用数据；调用方保证其存活到消费或清理。 */
XRT_API xsshcode xrtSshChannelIoWriteBorrow(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize
);



/* 接管由 xrtMalloc 家族分配的数据；失败或零长度不转移所有权。 */
XRT_API xsshcode xrtSshChannelIoWriteTake(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	void* pData,
	size_t iSize
);



/* 接管带释放过程的外部数据；失败或零长度不转移所有权。 */
XRT_API xsshcode xrtSshChannelIoWriteRef(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
);



/* 把调用方缓冲链移动到指定发送流；超出共享预算时源缓冲保持不变。 */
XRT_API xsshcode xrtSshChannelIoWriteBuffer(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	xnetbuf* pBuffer
);



/*
	为一条已解析 data 预分配接收存储，但不改变 channel 或可读缓冲。
	STDERR 只应用于 extended-data type 1；未知扩展类型应走 connection 的借用快路径。
*/
XRT_API xsshcode xrtSshChannelIoReceivePrepare(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	uint32 iRecipient,
	xbytesview Data
);



/* 外层已提交 channel 接收状态后，把预分配块无分配移动到可读缓冲。 */
XRT_API xsshcode xrtSshChannelIoReceiveCommit(xsshchannelio* pIo);



/* 在 channel 状态尚未提交时放弃接收预分配。 */
XRT_API xsshcode xrtSshChannelIoReceiveAbort(xsshchannelio* pIo);



/*
	把发送队首按远端窗口、最大包和 writer 空间切成一条最终 channel payload。
	成功后外层应依次提交 transport、connection/channel，再调用 SendCommit。
*/
XRT_API xsshcode xrtSshChannelIoSendPrepare(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	xsshwriter* pWriter,
	xbytesview* pPayload
);



/* 外层已提交 channel 发送状态后消费本次发送队首。 */
XRT_API xsshcode xrtSshChannelIoSendCommit(xsshchannelio* pIo);



/* 在 channel 状态尚未提交时放弃发送事务，排队数据保持不变。 */
XRT_API xsshcode xrtSshChannelIoSendAbort(xsshchannelio* pIo);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_forward_message.h */
/* ========================================================================== */

#ifndef XRT_SSH_FORWARD_MESSAGE_H
#define XRT_SSH_FORWARD_MESSAGE_H




#if defined(XSSH_FEATURE_FORWARD_MESSAGE) && \
	(!defined(XSSH_FEATURE_CHANNEL_MESSAGE) || \
	 !defined(XSSH_FEATURE_CONNECTION_MESSAGE))
	#error "XSSH_FEATURE_FORWARD_MESSAGE requires SSH channel and connection messages"
#endif



#if defined(XSSH_FEATURE_FORWARD_MESSAGE)

#define XSSH_GLOBAL_REQUEST_TCPIP_FORWARD "tcpip-forward"
#define XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD "cancel-tcpip-forward"
#define XSSH_CHANNEL_TYPE_DIRECT_TCPIP "direct-tcpip"
#define XSSH_CHANNEL_TYPE_FORWARDED_TCPIP "forwarded-tcpip"



/* Remote forwarding 请求借用线路地址，端口限制为 0 至 65535。 */
typedef struct xsshtcpipforward {
	xbytesview Address;
	uint32 Port;
} xsshtcpipforward;



/* TCP/IP channel open 借用目标与来源地址，两个端口均不超过 65535。 */
typedef struct xsshtcpipopen {
	xbytesview Host;
	uint32 Port;
	xbytesview Originator;
	uint32 OriginatorPort;
} xsshtcpipopen;



XRT_EXTERN_C_BEGIN



/* 写入或严格读取要求回复的 tcpip-forward；端口零请求动态分配。 */
XRT_API xsshcode xrtSshTcpipForwardWrite(
	xsshwriter* pWriter,
	xbytesview Address,
	uint32 iPort
);
XRT_API xsshcode xrtSshTcpipForwardRead(
	const xsshglobalrequest* pRequest,
	xsshtcpipforward* pForward
);



/* 写入或严格读取要求回复的 cancel-tcpip-forward 请求。 */
XRT_API xsshcode xrtSshTcpipForwardCancelWrite(
	xsshwriter* pWriter,
	xbytesview Address,
	uint32 iPort
);
XRT_API xsshcode xrtSshTcpipForwardCancelRead(
	const xsshglobalrequest* pRequest,
	xsshtcpipforward* pForward
);



/* 写入或严格读取动态分配端口的 REQUEST_SUCCESS。 */
XRT_API xsshcode xrtSshTcpipForwardSuccessWrite(
	xsshwriter* pWriter,
	uint32 iPort
);
XRT_API xsshcode xrtSshTcpipForwardSuccessRead(
	xbytesview Payload,
	uint32* pPort
);



/* 写入或严格读取 direct-tcpip channel open。 */
XRT_API xsshcode xrtSshDirectTcpipOpenWrite(
	xsshwriter* pWriter,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort
);
XRT_API xsshcode xrtSshDirectTcpipOpenRead(
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip
);



/* 写入或严格读取 forwarded-tcpip channel open。 */
XRT_API xsshcode xrtSshForwardedTcpipOpenWrite(
	xsshwriter* pWriter,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort
);
XRT_API xsshcode xrtSshForwardedTcpipOpenRead(
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_reply_queue.h */
/* ========================================================================== */

#ifndef XRT_SSH_REPLY_QUEUE_H
#define XRT_SSH_REPLY_QUEUE_H




#if defined(XSSH_FEATURE_REPLY_QUEUE) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_REPLY_QUEUE requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_REPLY_QUEUE)

/* Reply FIFO 使用调用方 token 存储，不拥有 future、任务或内存分配。 */
typedef struct xsshreplyqueue {
	uint64* Tokens;
	size_t Capacity;
	size_t Head;
	size_t Count;
	bool Initialized;
} xsshreplyqueue;



XRT_EXTERN_C_BEGIN



/* 初始化调用方存储支持的 reply FIFO；零容量允许 Tokens 为 NULL。 */
XRT_API bool xrtSshReplyQueueInit(
	xsshreplyqueue* pQueue,
	uint64* pTokens,
	size_t iCapacity
);



/* 返回当前等待回复的请求数量；无效状态返回零。 */
XRT_API size_t xrtSshReplyQueueCount(const xsshreplyqueue* pQueue);



/* 在 want-reply 请求可靠排队后追加调用方 token。 */
XRT_API xsshcode xrtSshReplyQueuePush(
	xsshreplyqueue* pQueue,
	uint64 iToken
);



/* 查看队首 token，不消费对应回复位置。 */
XRT_API xsshcode xrtSshReplyQueueFront(
	const xsshreplyqueue* pQueue,
	uint64* pToken
);



/* 收到 success/failure 时按协议顺序消费队首 token。 */
XRT_API xsshcode xrtSshReplyQueuePop(
	xsshreplyqueue* pQueue,
	uint64* pToken
);



/* 将未完成 token 按顺序迁移到更大的不重叠调用方存储。 */
XRT_API xsshcode xrtSshReplyQueueRebind(
	xsshreplyqueue* pQueue,
	uint64* pTokens,
	size_t iCapacity
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_connection_session.h */
/* ========================================================================== */

#ifndef XRT_SSH_CONNECTION_SESSION_H
#define XRT_SSH_CONNECTION_SESSION_H




#if defined(XSSH_FEATURE_CONNECTION_SESSION) && \
	(!defined(XSSH_FEATURE_CHANNEL_CORE) || \
	 !defined(XSSH_FEATURE_CONNECTION_MESSAGE) || \
	 !defined(XSSH_FEATURE_REPLY_QUEUE) || \
	 !defined(XSSH_FEATURE_TRANSPORT_CORE))
	#error "XSSH_FEATURE_CONNECTION_SESSION requires channel core, connection message, reply queue and transport core"
#endif



#if defined(XSSH_FEATURE_CONNECTION_SESSION)

/* Connection packet 覆盖 RFC 4254 公共全局与 channel 消息。 */
typedef enum xsshconnectionpacketkind {
	XSSH_CONNECTION_PACKET_NONE = 0,
	XSSH_CONNECTION_PACKET_GLOBAL_REQUEST = 1,
	XSSH_CONNECTION_PACKET_GLOBAL_SUCCESS = 2,
	XSSH_CONNECTION_PACKET_GLOBAL_FAILURE = 3,
	XSSH_CONNECTION_PACKET_CHANNEL_OPEN = 4,
	XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION = 5,
	XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE = 6,
	XSSH_CONNECTION_PACKET_CHANNEL_ADJUST = 7,
	XSSH_CONNECTION_PACKET_CHANNEL_DATA = 8,
	XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA = 9,
	XSSH_CONNECTION_PACKET_CHANNEL_EOF = 10,
	XSSH_CONNECTION_PACKET_CHANNEL_CLOSE = 11,
	XSSH_CONNECTION_PACKET_CHANNEL_REQUEST = 12,
	XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS = 13,
	XSSH_CONNECTION_PACKET_CHANNEL_FAILURE = 14
} xsshconnectionpacketkind;



/* 借用视图只在对应 transport read 事务提交前有效。 */
typedef union xsshconnectionmessage {
	xsshglobalrequest GlobalRequest;
	xbytesview GlobalSuccess;
	xsshchannelopen ChannelOpen;
	xsshchannelconfirmation ChannelConfirmation;
	xsshchannelopenfailure ChannelOpenFailure;
	xsshchanneladjust ChannelAdjust;
	xsshchanneldata ChannelData;
	xsshchannelextendeddata ChannelExtendedData;
	xsshchannelrequest ChannelRequest;
	uint32 Recipient;
} xsshconnectionmessage;



/* ReplyToken 只在 success/failure 已关联等待队首时有效。 */
typedef struct xsshconnectionpacket {
	xsshconnectionmessage Message;
	uint64 ReplyToken;
	xsshconnectionpacketkind Kind;
	bool HasReplyToken;
} xsshconnectionpacket;



/* Resolver 把本地 recipient 映射到调用方 channel 与可选 request reply FIFO。 */
typedef bool (*xsshchannelresolveproc)(
	ptr pUserData,
	uint32 iLocal,
	xsshchannelcore** ppChannel,
	xsshreplyqueue** ppReplies
);



/* QueueAction 只描述当前待提交事务，不改变调用方 FIFO。 */
typedef enum xsshconnectionqueueaction {
	XSSH_CONNECTION_QUEUE_NONE = 0,
	XSSH_CONNECTION_QUEUE_PUSH = 1,
	XSSH_CONNECTION_QUEUE_POP = 2
} xsshconnectionqueueaction;



/* 会话持有短事务快照，不拥有 channel、FIFO、payload 或网络对象。 */
typedef struct xsshconnectionsession {
	xsshchannelcore ChannelBefore;
	xsshchannelcore ChannelPending;
	xsshreplyqueue QueueBefore;
	xsshchannelcore* Channel;
	xsshreplyqueue* Queue;
	xsshreplyqueue* GlobalReplies;
	xsshchannelresolveproc Resolve;
	ptr UserData;
	uint64 QueueToken;
	uint64 WriteOrdinal;
	uint64 ReadOrdinal;
	xsshconnectionpacketkind WritePending;
	xsshconnectionpacketkind ReadPending;
	xsshconnectionqueueaction QueueAction;
	xsshrole Role;
	bool Active;
	bool Failed;
	uint32 ObjectGuard;
} xsshconnectionsession;



XRT_EXTERN_C_BEGIN



/* 初始化无网络会话；全局 FIFO 必须为空，空 resolver 只允许 global 与新 channel open。 */
XRT_API bool xrtSshConnectionSessionInit(
	xsshconnectionsession* pSession,
	xsshrole Role,
	xsshchannelresolveproc pResolve,
	ptr pUserData,
	xsshreplyqueue* pGlobalReplies
);



/* 清除事务与借用指针，不清理调用方 channel 或 reply FIFO。 */
XRT_API void xrtSshConnectionSessionClear(xsshconnectionsession* pSession);



/* 在 server USERAUTH_SUCCESS 已按正确方向提交后开放 connection 层。 */
XRT_API xsshcode xrtSshConnectionSessionBegin(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
);



/* 返回会话是否仍可处理 connection 消息。 */
XRT_API bool xrtSshConnectionSessionActive(
	const xsshconnectionsession* pSession
);



/*
	解析最终输出 payload，并在 channel 副本中准备提交结果。
	pChannel 只用于 channel 消息；want-reply request 额外传入对应 FIFO 与 token。
*/
XRT_API xsshcode xrtSshConnectionSessionWritePrepare(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
);



/* transport 已可靠提交输出后，原子提交 channel 与 request FIFO 状态。 */
XRT_API xsshcode xrtSshConnectionSessionWriteCommit(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃未交给 transport 的输出，不修改 channel 或 reply FIFO。 */
XRT_API xsshcode xrtSshConnectionSessionWriteAbort(
	xsshconnectionsession* pSession
);



/*
	解析 transport 已认证的 peer payload，并借出通用 packet。
	已有关联 channel 的消息通过 resolver 在副本中验证；新 CHANNEL_OPEN 由调用方决定存储。
*/
XRT_API xsshcode xrtSshConnectionSessionReadPrepare(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshconnectionpacket* pPacket
);



/* transport 已提交输入后，原子提交 channel 与 reply FIFO 状态。 */
XRT_API xsshcode xrtSshConnectionSessionReadCommit(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃已认证输入并终止 connection 会话；对应 transport 也必须关闭。 */
XRT_API xsshcode xrtSshConnectionSessionReadAbort(
	xsshconnectionsession* pSession
);



/* 显式终止 connection 编排；重复调用保持失败状态。 */
XRT_API void xrtSshConnectionSessionFail(xsshconnectionsession* pSession);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_channels.h */
/* ========================================================================== */

#ifndef XRT_SSH_CHANNELS_H
#define XRT_SSH_CHANNELS_H




#if defined(XSSH_FEATURE_CHANNELS) && \
	(!defined(XSSH_FEATURE_CHANNEL_IO) || \
	 !defined(XSSH_FEATURE_CONNECTION_SESSION) || \
	 !defined(XRT_FEATURE_INT_MAP))
	#error "XSSH_FEATURE_CHANNELS requires channel I/O, connection session and XRT int map"
#endif



#if defined(XSSH_FEATURE_CHANNELS)

#define XSSH_CHANNELS_MAX_DEFAULT 1024u
#define XSSH_CHANNELS_REPLY_LIMIT_DEFAULT 64u
#define XSSH_CHANNELS_WINDOW_DEFAULT 2097152u
#define XSSH_CHANNELS_PACKET_DEFAULT 32768u
#define XSSH_CHANNELS_ADJUST_DEFAULT 1048576u



typedef struct xsshchannels xsshchannels;



/* 删除观察器只接收已经失效的本端 id，不借用已释放 channel。 */
typedef void (*xsshchannelsremovedproc)(
	xsshchannels* pChannels,
	uint32 iLocal,
	ptr pUserData
);



/* Channel 集合配置同时约束对象数量、窗口、动态数据和请求回复内存。 */
typedef struct xsshchannelsconfig {
	size_t MaxChannels;
	size_t ReplyLimit;
	uint32 ReceiveWindow;
	uint32 ReceiveMaxPacket;
	uint32 AdjustThreshold;
	xsshchannelioconfig Io;
} xsshchannelsconfig;



/* 单个动态 channel 组合协议核心、数据缓冲和按需回复队列。 */
typedef struct xsshchannel {
	xsshchannelcore Core;
	xsshchannelio Io;
	xsshreplyqueue Replies;
	uint64* ReplyTokens;
	size_t ReplyCapacity;
	size_t ReplyLimit;
	ptr UserData;
	bool Incoming;
	bool Initialized;
	uint32 Guard;
} xsshchannel;



/* Channel 集合借用网络缓冲池，拥有全部 channel 对象和回复 token。 */
struct xsshchannels {
	xintmap Map;
	xsshchannelsconfig Config;
	xnetbufpool* Pool;
	xsshchannelsremovedproc Removed;
	ptr RemovedData;
	uint32 NextLocal;
	bool Initialized;
	uint32 Guard;
};



/* 外置迭代器允许调用方遍历活动 channel，结构修改会使其失效。 */
typedef struct xsshchannelsiter {
	xintmapiter Base;
	bool Active;
} xsshchannelsiter;



XRT_EXTERN_C_BEGIN



/* 写入适合交互会话和并发客户端的有界默认配置。 */
XRT_API void xrtSshChannelsConfigInit(xsshchannelsconfig* pConfig);



/* 初始化空集合；缓冲池只借用，空指针使用 XRT 默认网络缓冲池。 */
XRT_API bool xrtSshChannelsInit(
	xsshchannels* pChannels,
	xnetbufpool* pPool,
	const xsshchannelsconfig* pConfig
);



/* 设置唯一删除观察器；成功删除后同步报告 id，空回调可取消观察。 */
XRT_API bool xrtSshChannelsOnRemoved(
	xsshchannels* pChannels,
	xsshchannelsremovedproc pRemoved,
	ptr pUserData
);



/* 释放全部 channel、动态数据和回复 token，不释放借用缓冲池。 */
XRT_API void xrtSshChannelsClear(xsshchannels* pChannels);



/* 返回当前活动 channel 数量；无效集合返回零。 */
XRT_API size_t xrtSshChannelsCount(const xsshchannels* pChannels);



/* 创建等待 peer confirmation 的本端 channel，并返回稳定借用地址。 */
XRT_API xsshcode xrtSshChannelsOpen(
	xsshchannels* pChannels,
	xsshchannel** ppChannel
);



/* 为一条 peer CHANNEL_OPEN 创建等待本端决定的动态 channel。 */
XRT_API xsshcode xrtSshChannelsAccept(
	xsshchannels* pChannels,
	const xsshchannelopen* pOpen,
	xsshchannel** ppChannel
);



/* 按本端 channel id 返回稳定借用地址；未找到是正常结果。 */
XRT_API xsshchannel* xrtSshChannelsGet(
	xsshchannels* pChannels,
	uint32 iLocal
);



/* 返回只读 channel；未找到是正常结果。 */
XRT_API const xsshchannel* xrtSshChannelsConstGet(
	const xsshchannels* pChannels,
	uint32 iLocal
);



/* 为 want-reply 请求按需扩展 token 存储，已有 token 顺序保持不变。 */
XRT_API xsshcode xrtSshChannelReplyReserve(
	xsshchannel* pChannel,
	size_t iCapacity
);



/* 删除已经结束且没有未消费数据或回复的 channel。 */
XRT_API bool xrtSshChannelsRemove(
	xsshchannels* pChannels,
	uint32 iLocal
);



/* 强制丢弃指定 channel 及其全部排队数据，供连接关闭和策略拒绝使用。 */
XRT_API bool xrtSshChannelsDiscard(
	xsshchannels* pChannels,
	uint32 iLocal
);



/* 直接适配 xsshchannelresolveproc，UserData 必须指向活动集合。 */
XRT_API bool xrtSshChannelsResolve(
	ptr pUserData,
	uint32 iLocal,
	xsshchannelcore** ppChannel,
	xsshreplyqueue** ppReplies
);



/* 启动按本端 channel id 递增的外置迭代。 */
XRT_API bool xrtSshChannelsIterBegin(
	xsshchannels* pChannels,
	xsshchannelsiter* pIterator
);



/* 返回下一活动 channel，并可选返回本端 id。 */
XRT_API xsshchannel* xrtSshChannelsIterNext(
	xsshchannelsiter* pIterator,
	uint32* pLocal
);



/* 提前结束迭代并释放结构修改保护。 */
XRT_API void xrtSshChannelsIterEnd(xsshchannelsiter* pIterator);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_session_core.h */
/* ========================================================================== */

#ifndef XRT_SSH_SESSION_CORE_H
#define XRT_SSH_SESSION_CORE_H




#if defined(XSSH_FEATURE_SESSION_CORE) && \
	(!defined(XSSH_FEATURE_AUTH_SESSION) || \
	 !defined(XSSH_FEATURE_CONNECTION_SESSION) || \
	 !defined(XSSH_FEATURE_KEX_EXCHANGE))
	#error "XSSH_FEATURE_SESSION_CORE requires KEX, auth and connection sessions"
#endif



#if defined(XSSH_FEATURE_SESSION_CORE)

/* 连接级阶段只描述协议编排，不描述 socket、等待或任务状态。 */
typedef enum xsshsessionphase {
	XSSH_SESSION_IDENTIFICATION = 0,
	XSSH_SESSION_KEY_EXCHANGE = 1,
	XSSH_SESSION_AUTHENTICATION = 2,
	XSSH_SESSION_CONNECTION = 3,
	XSSH_SESSION_REKEY = 4,
	XSSH_SESSION_CLOSING = 5,
	XSSH_SESSION_FAILED = 6
} xsshsessionphase;



/*
	Action 把 identification、KEX 与认证子状态统一为驱动的下一项常见动作。
	WRITE/READ_PENDING 表示事务必须先提交或中止，CONNECTION 表示由应用层选择消息。
*/
typedef enum xsshsessionaction {
	XSSH_SESSION_ACTION_NONE = 0,
	XSSH_SESSION_ACTION_WRITE_IDENTIFICATION = 1,
	XSSH_SESSION_ACTION_READ_IDENTIFICATION = 2,
	XSSH_SESSION_ACTION_WRITE_KEXINIT = 3,
	XSSH_SESSION_ACTION_READ_KEXINIT = 4,
	XSSH_SESSION_ACTION_BEGIN_KEX = 5,
	XSSH_SESSION_ACTION_WRITE_ECDH_INIT = 6,
	XSSH_SESSION_ACTION_READ_ECDH_INIT = 7,
	XSSH_SESSION_ACTION_WRITE_ECDH_REPLY = 8,
	XSSH_SESSION_ACTION_READ_ECDH_REPLY = 9,
	XSSH_SESSION_ACTION_VERIFY_HOST_KEY = 10,
	XSSH_SESSION_ACTION_WRITE_NEWKEYS = 11,
	XSSH_SESSION_ACTION_READ_NEWKEYS = 12,
	XSSH_SESSION_ACTION_ACTIVATE_WRITE_KEYS = 13,
	XSSH_SESSION_ACTION_ACTIVATE_READ_KEYS = 14,
	XSSH_SESSION_ACTION_COMPLETE_KEX = 15,
	XSSH_SESSION_ACTION_BEGIN_AUTH = 16,
	XSSH_SESSION_ACTION_WRITE_SERVICE_REQUEST = 17,
	XSSH_SESSION_ACTION_READ_SERVICE_REQUEST = 18,
	XSSH_SESSION_ACTION_WRITE_SERVICE_ACCEPT = 19,
	XSSH_SESSION_ACTION_READ_SERVICE_ACCEPT = 20,
	XSSH_SESSION_ACTION_WRITE_AUTH_REQUEST = 21,
	XSSH_SESSION_ACTION_READ_AUTH_REQUEST = 22,
	XSSH_SESSION_ACTION_WRITE_AUTH_RESULT = 23,
	XSSH_SESSION_ACTION_READ_AUTH_RESULT = 24,
	XSSH_SESSION_ACTION_COMPLETE_AUTH = 25,
	XSSH_SESSION_ACTION_CONNECTION = 26,
	XSSH_SESSION_ACTION_WRITE_PENDING = 27,
	XSSH_SESSION_ACTION_READ_PENDING = 28,
	XSSH_SESSION_ACTION_CLOSING = 29,
	XSSH_SESSION_ACTION_FAILED = 30
} xsshsessionaction;



/* Packet 分类保留 transport 控制消息和未知扩展的直接访问路径。 */
typedef enum xsshsessionpacketkind {
	XSSH_SESSION_PACKET_NONE = 0,
	XSSH_SESSION_PACKET_DISCONNECT = 1,
	XSSH_SESSION_PACKET_IGNORE = 2,
	XSSH_SESSION_PACKET_UNIMPLEMENTED = 3,
	XSSH_SESSION_PACKET_DEBUG = 4,
	XSSH_SESSION_PACKET_EXT_INFO = 5,
	XSSH_SESSION_PACKET_NEWCOMPRESS = 6,
	XSSH_SESSION_PACKET_KEXINIT = 7,
	XSSH_SESSION_PACKET_KEX = 8,
	XSSH_SESSION_PACKET_AUTH = 9,
	XSSH_SESSION_PACKET_CONNECTION = 10,
	XSSH_SESSION_PACKET_EXTENSION = 11
} xsshsessionpacketkind;



/* 消息视图只在对应 transport 读事务提交或中止前有效。 */
typedef union xsshsessionmessage {
	xsshdisconnect Disconnect;
	xsshignore Ignore;
	uint32 UnimplementedSequence;
	xsshdebug Debug;
	xsshextinfo ExtInfo;
	xsshkexsessionpacket Kex;
	xsshauthsessionpacket Auth;
	xsshconnectionpacket Connection;
} xsshsessionmessage;



/* 未知扩展保留完整 Payload，已知消息同时提供轻量解析结果。 */
typedef struct xsshsessionpacket {
	xsshsessionmessage Message;
	xbytesview Payload;
	xsshsessionpacketkind Kind;
	uint8 Number;
} xsshsessionpacket;



/*
	会话核心拥有 KEX transcript、认证和 connection 状态，不拥有 transport、channel 表或凭据。
	同一对象由一个执行流推进；公开字段只供诊断读取，子对象通过访问器继续暴露底层能力。
*/
typedef struct xsshsessioncore {
	xsshkexexchange Kex;
	xsshauthsession Auth;
	xsshconnectionsession Connection;
	xbytesview WritePayload;
	uint64 WriteOrdinal;
	uint64 ReadOrdinal;
	xsshrole Role;
	xsshsessionpacketkind WritePending;
	xsshsessionpacketkind ReadPending;
	uint8 WriteMessage;
	uint8 ReadMessage;
	bool WriteBound;
	bool Initialized;
	bool Failed;
	uint32 Guard;
} xsshsessioncore;



/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 初始化连接级协议核心；动态内存只用于 KEX transcript。 */
XRT_API bool xrtSshSessionCoreInit(
	xsshsessioncore* pSession,
	xnetbufpool* pPool,
	xsshrole Role,
	xsshchannelresolveproc pResolve,
	ptr pUserData,
	xsshreplyqueue* pGlobalReplies
);



/* 释放 transcript、清除密码材料和全部借用事务，不处理 transport 或外部存储。 */
XRT_API void xrtSshSessionCoreClear(xsshsessioncore* pSession);



/* 根据连接级对象和 transport 返回当前协议阶段。 */
XRT_API xsshsessionphase xrtSshSessionCorePhase(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore
);



/* 返回稳定状态下建议驱动的下一动作；本端 identification 与 KEXINIT 优先于等待对端。 */
XRT_API xsshsessionaction xrtSshSessionCoreAction(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore
);



/* 返回可直接使用的 KEX 交换对象。 */
XRT_API xsshkexexchange* xrtSshSessionCoreKex(
	xsshsessioncore* pSession
);



/* 返回只读 KEX 交换对象。 */
XRT_API const xsshkexexchange* xrtSshSessionCoreKexConst(
	const xsshsessioncore* pSession
);



/* 返回认证会话；认证方法和凭据策略仍由调用方驱动。 */
XRT_API xsshauthsession* xrtSshSessionCoreAuth(
	xsshsessioncore* pSession
);



/* 返回只读认证会话。 */
XRT_API const xsshauthsession* xrtSshSessionCoreAuthConst(
	const xsshsessioncore* pSession
);



/* 返回 connection 会话；channel 表和 reply FIFO 仍由调用方持有。 */
XRT_API xsshconnectionsession* xrtSshSessionCoreConnection(
	xsshsessioncore* pSession
);



/* 返回只读 connection 会话。 */
XRT_API const xsshconnectionsession* xrtSshSessionCoreConnectionConst(
	const xsshsessioncore* pSession
);



/* 在 transport identification 提交前保存本端或对端版本串。 */
XRT_API xsshcode xrtSshSessionCoreVersionPrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xstrview Version
);



/* transport 已提交对应 identification 后发布版本串。 */
XRT_API xsshcode xrtSshSessionCoreVersionCommit(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
);



/* transport 尚未推进时放弃版本串暂存副本。 */
XRT_API xsshcode xrtSshSessionCoreVersionAbort(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
);



/* 使用显式 Curve25519 私钥开始当前已经就绪的一代 KEX。 */
XRT_API xsshcode xrtSshSessionCoreKexBeginWithPrivate(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
);



/* 首轮 KEX 完成后开始双端 ssh-userauth 编排。 */
XRT_API xsshcode xrtSshSessionCoreAuthBegin(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	const xsshauthguardpolicy* pPolicy,
	double Timer
);



/*
	在 transport 写 Prepare 前分类并准备一个最终 payload。
	KEX 方法 payload 必须先由 KEX 会话构建；channel 和 FIFO 只用于 connection 消息。
*/
XRT_API xsshcode xrtSshSessionCoreWritePrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	double Timer,
	xsshsessionpacketkind* pKind
);



/* transport 已准备同一 payload 后绑定 packet 类型和序号，尚不推进状态。 */
XRT_API xsshcode xrtSshSessionCoreWriteBind(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload
);



/* transport 已可靠提交输出后提交对应 KEX、认证或 connection 事务。 */
XRT_API xsshcode xrtSshSessionCoreWriteCommit(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	double Timer
);



/* transport 未推进时放弃上层写事务；transport 自身仍由调用方中止。 */
XRT_API xsshcode xrtSshSessionCoreWriteAbort(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
);



/*
	分类并准备 transport 已认证的 peer payload。
	客户端 ECDH_REPLY 的主机公钥复制到调用方存储，其余消息不使用该存储。
*/
XRT_API xsshcode xrtSshSessionCoreReadPrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	double Timer,
	xsshsessionpacket* pPacket
);



/* transport 已提交输入后提交上层事务并按需激活读密钥。 */
XRT_API xsshcode xrtSshSessionCoreReadCommit(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	double Timer
);



/* 放弃不可回滚的认证输入并终止会话；transport 自身仍由调用方中止。 */
XRT_API xsshcode xrtSshSessionCoreReadAbort(xsshsessioncore* pSession);



/* 终止全部协议子状态；transport、网络和外部 channel 由调用方关闭。 */
XRT_API void xrtSshSessionCoreFail(xsshsessioncore* pSession);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_session_core_random.h */
/* ========================================================================== */

#ifndef XRT_SSH_SESSION_CORE_RANDOM_H
#define XRT_SSH_SESSION_CORE_RANDOM_H




#if defined(XSSH_FEATURE_SESSION_CORE_RANDOM) && \
	(!defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM) || \
	 !defined(XSSH_FEATURE_SESSION_CORE))
	#error "XSSH_FEATURE_SESSION_CORE_RANDOM requires session core and secure KEX exchange"
#endif



#if defined(XSSH_FEATURE_SESSION_CORE_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用操作系统安全随机临时私钥开始当前已经就绪的一代 KEX。 */
XRT_API xsshcode xrtSshSessionCoreKexBegin(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_session_tcp.h */
/* ========================================================================== */

#ifndef XRT_SSH_SESSION_TCP_H
#define XRT_SSH_SESSION_TCP_H




#if defined(XSSH_FEATURE_SESSION_TCP) && \
	(!defined(XSSH_FEATURE_SESSION_CORE) || \
	 !defined(XSSH_FEATURE_TRANSPORT_TCP))
	#error "XSSH_FEATURE_SESSION_TCP requires session core and TCP transport"
#endif



#if defined(XSSH_FEATURE_SESSION_TCP)

/* TCP 会话配置只组合 transport 预算与外部 connection 查找器，不接管外部对象。 */
typedef struct xsshsessiontcpconfig {
	xsshtransporttcpconfig Transport;
	xsshchannelresolveproc ChannelResolve;
	ptr ChannelUserData;
	xsshreplyqueue* GlobalReplies;
} xsshsessiontcpconfig;



/* 一次读取同时保留 packet 线路信息和连接级轻量解析结果。 */
typedef struct xsshsessiontcppacket {
	xsshpacketview Transport;
	xsshsessionpacket Session;
} xsshsessiontcppacket;



/*
	TCP 会话只拥有动态 transport 与协议核心，不拥有 Stream、等待、时钟、凭据或 channel 表。
	ReadPacket 和 ReadVersion 都是未决读事务期间的借用视图，不增加固定报文缓冲。
*/
typedef struct xsshsessiontcp {
	xsshtransporttcp Transport;
	xsshsessioncore Session;
	xsshpacketview ReadPacket;
	xstrview ReadVersion;
	void* ReadPlain;
	size_t ReadPlainCapacity;
	uint32 Guard;
} xsshsessiontcp;



/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 写入指定角色的默认 transport、rekey 与空外部 connection 配置。 */
XRT_API bool xrtSshSessionTcpConfigInit(
	xsshsessiontcpconfig* pConfig,
	xsshrole Role
);



/* 使用同一动态缓冲池初始化 TCP transport 与连接级协议核心。 */
XRT_API bool xrtSshSessionTcpInit(
	xsshsessiontcp* pSession,
	xnetbufpool* pPool,
	const xsshsessiontcpconfig* pConfig,
	double Timer
);



/* 放弃未决事务、释放动态链并清除全部密码状态；不处理 Stream 和外部对象。 */
XRT_API void xrtSshSessionTcpClear(xsshsessiontcp* pSession);



/* 返回可直接访问 packet、cipher、rekey 和 TCP 动态缓冲的 transport。 */
XRT_API xsshtransporttcp* xrtSshSessionTcpTransport(
	xsshsessiontcp* pSession
);



/* 返回只读 TCP transport；无效对象返回空。 */
XRT_API const xsshtransporttcp* xrtSshSessionTcpTransportConst(
	const xsshsessiontcp* pSession
);



/* 返回 KEX、认证和 connection 编排核心，保留全部底层访问能力。 */
XRT_API xsshsessioncore* xrtSshSessionTcpCore(xsshsessiontcp* pSession);



/* 返回只读连接级协议核心；无效对象返回空。 */
XRT_API const xsshsessioncore* xrtSshSessionTcpCoreConst(
	const xsshsessiontcp* pSession
);



/* 返回当前 identification、KEX、认证、connection、rekey 或失败阶段。 */
XRT_API xsshsessionphase xrtSshSessionTcpPhase(
	const xsshsessiontcp* pSession
);



/* 返回统一的 identification、KEX、认证或 connection 下一动作。 */
XRT_API xsshsessionaction xrtSshSessionTcpAction(
	const xsshsessiontcp* pSession
);



/* 使用显式 Curve25519 私钥开始当前已就绪的 KEX。 */
XRT_API xsshcode xrtSshSessionTcpKexBeginWithPrivate(
	xsshsessiontcp* pSession,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
);



/* 首轮 KEX 完成后开始 USERAUTH；策略和单调时钟仍由调用方提供。 */
XRT_API xsshcode xrtSshSessionTcpAuthBegin(
	xsshsessiontcp* pSession,
	const xsshauthguardpolicy* pPolicy,
	double Timer
);



/* 同时准备本端版本 transcript 与唯一 identification 线路输出。 */
XRT_API xsshcode xrtSshSessionTcpIdentificationWritePrepare(
	xsshsessiontcp* pSession,
	xstrview Version
);



/* 同时准备上层协议事务和使用调用方 padding 的唯一 packet 线路输出。 */
XRT_API xsshcode xrtSshSessionTcpWritePrepareWithPadding(
	xsshsessiontcp* pSession,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	xsshpaddingproc pPadding,
	ptr pPaddingData,
	double Timer,
	xsshsessionpacketkind* pKind
);



/*
	把未决 identification 或 packet 零复制交给 Stream。
	AGAIN 和网络 ERROR 保留两层事务，成功接管才按 transport、session 顺序共同提交。
*/
XRT_API xnetresult xrtSshSessionTcpWriteSubmit(
	xsshsessiontcp* pSession,
	xnetstream* pStream,
	double Timer,
	xsshrekeydecision* pDecision
);



/* 先回滚上层候选，再放弃尚未进入 TCP 队列的动态输出。 */
XRT_API xsshcode xrtSshSessionTcpWriteAbort(xsshsessiontcp* pSession);



/* 返回当前可直接重试提交的 identification 或 packet 线路字节数。 */
XRT_API size_t xrtSshSessionTcpWriteSize(
	const xsshsessiontcp* pSession
);



/* 从分块 TCP 输入准备 peer identification，并同步保存版本 transcript。 */
XRT_API xsshcode xrtSshSessionTcpIdentificationReadPrepare(
	xsshsessiontcp* pSession,
	xnetbuf* pInput,
	xstrview* pVersion
);



/* 只探测下一 packet 的线路尺寸和明文工作区需求，不改变会话状态。 */
XRT_API xsshcode xrtSshSessionTcpReadInspect(
	const xsshsessiontcp* pSession,
	const xnetbuf* pInput,
	xsshpacketneed* pNeed
);



/*
	认证并轻量解析下一 packet；返回值借用到 ReadCommit/Abort。
	主机密钥空间不足时保留 transport 事务，使用相同 Input 和 Plain 扩容后可重试。
*/
XRT_API xsshcode xrtSshSessionTcpReadPrepare(
	xsshsessiontcp* pSession,
	xnetbuf* pInput,
	void* pPlain,
	size_t iPlainCapacity,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	double Timer,
	xsshsessiontcppacket* pPacket
);



/* 先消费并提交 transport，再提交版本或协议事务并按需切换读密钥。 */
XRT_API xsshcode xrtSshSessionTcpReadCommit(
	xsshsessiontcp* pSession,
	double Timer,
	xsshrekeydecision* pDecision
);



/* 拒绝当前借用输入，消费对应线路前缀并终止不可继续的 transport 与会话。 */
XRT_API xsshcode xrtSshSessionTcpReadAbort(xsshsessiontcp* pSession);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_session_reader.h */
/* ========================================================================== */

#ifndef XRT_SSH_SESSION_READER_H
#define XRT_SSH_SESSION_READER_H




#if defined(XSSH_FEATURE_SESSION_READER) && \
	!defined(XSSH_FEATURE_SESSION_TCP)
	#error "XSSH_FEATURE_SESSION_READER requires the SSH TCP session"
#endif



#if defined(XSSH_FEATURE_SESSION_READER)

/* Reader 只允许一个动态 packet 读取事务。 */
typedef enum xsshsessionreaderstate {
	XSSH_SESSION_READER_IDLE = 0,
	XSSH_SESSION_READER_HOST_KEY = 1,
	XSSH_SESSION_READER_RETRY = 2,
	XSSH_SESSION_READER_READY = 3,
	XSSH_SESSION_READER_INVALID = 4
} xsshsessionreaderstate;



/*
	Reader 借用一个 TCP 会话，拥有按实际报文申请的明文工作区和稳定主机公钥。
	它不拥有 Stream、输入、会话、缓冲池、等待、任务或 channel 数据。
*/
typedef struct xsshsessionreader {
	xnetbuf Plain;
	xnetbuf HostKey;
	xsshsessiontcp* Session;
	xnetbuf* Input;
	xnetwspan PlainSpan;
	xnetwspan HostKeySpan;
	xsshpacketneed Need;
	size_t HostKeyOldSize;
	size_t HostKeySize;
	xsshsessionreaderstate State;
	uint32 Guard;
} xsshsessionreader;



/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 使用与 TCP 会话相同的 Worker 缓冲池初始化空动态读取器。 */
XRT_API bool xrtSshSessionReaderInit(
	xsshsessionreader* pReader,
	xnetbufpool* pPool,
	xsshsessiontcp* pSession
);



/* 中止未决读取并释放动态块；不会清理借用的 TCP 会话。 */
XRT_API void xrtSshSessionReaderClear(xsshsessionreader* pReader);



/* 返回读取器绑定的 TCP 会话；无效对象返回空。 */
XRT_API xsshsessiontcp* xrtSshSessionReaderSession(
	xsshsessionreader* pReader
);



/* 返回只读 TCP 会话；无效对象返回空。 */
XRT_API const xsshsessiontcp* xrtSshSessionReaderSessionConst(
	const xsshsessionreader* pReader
);



/* 返回空闲、空间重试或已准备状态；无效对象返回独立 INVALID。 */
XRT_API xsshsessionreaderstate xrtSshSessionReaderState(
	const xsshsessionreader* pReader
);



/*
	按 transport 探测结果解析下一 packet；明文包零分配，加密包按需申请工作区。
	客户端 ECDH_REPLY 会在同一未消费输入上自动按精确长度扩容并重试主机公钥复制。
*/
XRT_API xsshcode xrtSshSessionReaderPrepare(
	xsshsessionreader* pReader,
	xnetbuf* pInput,
	double Timer,
	xsshsessiontcppacket* pPacket
);



/* 提交已接受 packet，释放临时明文并发布本轮主机公钥存储。 */
XRT_API xsshcode xrtSshSessionReaderCommit(
	xsshsessionreader* pReader,
	double Timer,
	xsshrekeydecision* pDecision
);



/* 拒绝当前 packet、消费其线路前缀并终止绑定会话。 */
XRT_API xsshcode xrtSshSessionReaderAbort(xsshsessionreader* pReader);



/* 返回当前或最近一轮 KEX 已验签主机公钥的稳定借用视图。 */
XRT_API xbytesview xrtSshSessionReaderHostKey(
	const xsshsessionreader* pReader
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_session_stream.h */
/* ========================================================================== */

#ifndef XRT_SSH_SESSION_STREAM_H
#define XRT_SSH_SESSION_STREAM_H




#if defined(XSSH_FEATURE_SESSION_STREAM) && \
	!defined(XSSH_FEATURE_SESSION_READER)
	#error "XSSH_FEATURE_SESSION_STREAM requires the SSH session reader"
#endif



#if defined(XSSH_FEATURE_SESSION_STREAM)

typedef struct xsshsessionstream xsshsessionstream;



/* Stream 驱动只保留一个未决读事务；RETRY 表示内存恢复后可重试同一输入。 */
typedef enum xsshsessionstreamstate {
	XSSH_SESSION_STREAM_CREATED = 0,
	XSSH_SESSION_STREAM_OPEN = 1,
	XSSH_SESSION_STREAM_HOLD_IDENTIFICATION = 2,
	XSSH_SESSION_STREAM_HOLD_PACKET = 3,
	XSSH_SESSION_STREAM_RETRY = 4,
	XSSH_SESSION_STREAM_CLOSING = 5,
	XSSH_SESSION_STREAM_CLOSED = 6,
	XSSH_SESSION_STREAM_INVALID = 7
} xsshsessionstreamstate;



/* 接受立即提交，保留暂停读取并等待显式决定，中止会关闭 SSH 与 TCP。 */
typedef enum xsshsessionstreamdecision {
	XSSH_SESSION_STREAM_ACCEPT = 0,
	XSSH_SESSION_STREAM_HOLD = 1,
	XSSH_SESSION_STREAM_ABORT = 2
} xsshsessionstreamdecision;



/*
	全部回调都在 Stream 所属 Worker 串行执行，借用值只在对应事务提交或中止前有效。
	Action 负责协议选择，Identification 与 Packet 负责线路输入的提交决定。
*/
typedef struct xsshsessionstreamevents {
	void (*Open)(xsshsessionstream* pSession, ptr pData);
	void (*Action)(
		xsshsessionstream* pSession,
		xsshsessionaction Action,
		ptr pData
	);
	xsshsessionstreamdecision (*Identification)(
		xsshsessionstream* pSession,
		xstrview Version,
		ptr pData
	);
	xsshsessionstreamdecision (*Packet)(
		xsshsessionstream* pSession,
		const xsshsessiontcppacket* pPacket,
		ptr pData
	);
	void (*Rekey)(
		xsshsessionstream* pSession,
		xsshrekeydecision Decision,
		ptr pData
	);
	void (*Error)(
		xsshsessionstream* pSession,
		xsshcode Code,
		const xerror* pError,
		ptr pData
	);
	void (*End)(xsshsessionstream* pSession, ptr pData);
	void (*HighWater)(
		xsshsessionstream* pSession,
		size_t iQueued,
		ptr pData
	);
	void (*LowWater)(
		xsshsessionstream* pSession,
		size_t iQueued,
		ptr pData
	);
	void (*Drain)(xsshsessionstream* pSession, ptr pData);
	void (*Close)(
		xsshsessionstream* pSession,
		xnetresult Result,
		const xerror* pError,
		ptr pData
	);
} xsshsessionstreamevents;



/*
	驱动拥有 SSH TCP 会话与动态 Reader，借用 Stream、Worker 缓冲池、配置外部对象和用户数据。
	Packet 与 Version 仅保存 HOLD 事务的借用结果，不引入固定报文缓冲。
*/
struct xsshsessionstream {
	xsshsessiontcp Session;
	xsshsessionreader Reader;
	xsshsessiontcpconfig Config;
	xsshsessionstreamevents Events;
	xsshsessiontcppacket Packet;
	xnetstream* Stream;
	xnetbuf* Input;
	ptr UserData;
	xstrview Version;
	xsshsessionaction NotifiedAction;
	xsshsessionstreamstate State;
	bool SessionReady;
	bool Driving;
	bool DriveAgain;
	bool Paused;
	bool WritePaused;
	bool ReadEnded;
	uint32 Guard;
};



XRT_EXTERN_C_BEGIN



/* 复制会话配置和回调；此时不创建 Engine、Stream、会话动态块或后台任务。 */
XRT_API bool xrtSshSessionStreamInit(
	xsshsessionstream* pSession,
	const xsshsessiontcpconfig* pConfig,
	const xsshsessionstreamevents* pEvents,
	ptr pData
);



/* 只清理尚未附着或已经关闭的驱动；活动连接必须先中止并等待 Close。 */
XRT_API bool xrtSshSessionStreamClear(xsshsessionstream* pSession);



/*
	返回供 xrtNetStreamConnect 直接使用的稳定事件表，Stream data 必须是已初始化驱动。
	服务端可在 Accept 回调中改用 xrtSshSessionStreamAttach。
*/
XRT_API const xnetstreamevents* xrtSshSessionStreamNetEvents(void);



/* 在 Stream Worker 上接管事件；已打开 Stream 必须尚未积压输入。 */
XRT_API bool xrtSshSessionStreamAttach(
	xsshsessionstream* pSession,
	xnetstream* pStream
);



/* 返回驱动状态；无效对象返回独立 INVALID。 */
XRT_API xsshsessionstreamstate xrtSshSessionStreamState(
	const xsshsessionstream* pSession
);



/* 返回已经附着且尚未关闭的借用 Stream；连接建立前也可能非空。 */
XRT_API xnetstream* xrtSshSessionStreamTcp(xsshsessionstream* pSession);



/* 返回可直接驱动 KEX、认证和 connection 的底层 SSH TCP 会话。 */
XRT_API xsshsessiontcp* xrtSshSessionStreamSession(
	xsshsessionstream* pSession
);



/* 返回按需明文和主机公钥读取器，保留完整低层事务能力。 */
XRT_API xsshsessionreader* xrtSshSessionStreamReader(
	xsshsessionstream* pSession
);



/* 返回 HOLD 的 peer identification；其他状态返回空视图。 */
XRT_API xstrview xrtSshSessionStreamVersion(
	const xsshsessionstream* pSession
);



/* 返回 HOLD 的已认证 packet；其他状态返回空指针。 */
XRT_API const xsshsessiontcppacket* xrtSshSessionStreamPacket(
	const xsshsessionstream* pSession
);



/*
	在所属 Worker 上推进 Action、待提交输出和当前输入。
	NEED_MORE 与 SPACE 都保留连接；SPACE 进入 RETRY，释放内存后可再次调用。
*/
XRT_API xsshcode xrtSshSessionStreamDrive(xsshsessionstream* pSession);



/* 提交 HOLD 的 identification 或 packet，恢复读取并继续推进。 */
XRT_API xsshcode xrtSshSessionStreamAccept(xsshsessionstream* pSession);



/* 拒绝 HOLD 输入并异常关闭；返回底层读事务的中止结果。 */
XRT_API xsshcode xrtSshSessionStreamReject(xsshsessionstream* pSession);



/* 在所属 Worker 上回滚未进入 TCP 队列的输出、终止未决读取并请求异常关闭。 */
XRT_API bool xrtSshSessionStreamAbort(xsshsessionstream* pSession);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_session_tcp_random.h */
/* ========================================================================== */

#ifndef XRT_SSH_SESSION_TCP_RANDOM_H
#define XRT_SSH_SESSION_TCP_RANDOM_H




#if defined(XSSH_FEATURE_SESSION_TCP_RANDOM) && \
	(!defined(XSSH_FEATURE_SESSION_TCP) || \
	 !defined(XSSH_FEATURE_SESSION_CORE_RANDOM) || \
	 !defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM))
	#error "XSSH_FEATURE_SESSION_TCP_RANDOM requires TCP session and secure-random helpers"
#endif



#if defined(XSSH_FEATURE_SESSION_TCP_RANDOM)

/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 使用 XRT 系统安全随机临时私钥开始当前已就绪的 Curve25519 KEX。 */
XRT_API xsshcode xrtSshSessionTcpKexBegin(
	xsshsessiontcp* pSession,
	xbytesview ServerHostKey
);



/* 使用 XRT 系统安全随机 padding 同时准备协议与 TCP packet 事务。 */
XRT_API xsshcode xrtSshSessionTcpWritePrepare(
	xsshsessiontcp* pSession,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	double Timer,
	xsshsessionpacketkind* pKind
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_client_core.h */
/* ========================================================================== */

#ifndef XRT_SSH_CLIENT_CORE_H
#define XRT_SSH_CLIENT_CORE_H




#if defined(XSSH_FEATURE_CLIENT_CORE) && \
	(!defined(XSSH_FEATURE_AUTH_PASSWORD) || \
	 !defined(XSSH_FEATURE_KEXINIT_RANDOM) || \
	 !defined(XSSH_FEATURE_SESSION_READER) || \
	 !defined(XSSH_FEATURE_SESSION_TCP_RANDOM))
	#error "XSSH_FEATURE_CLIENT_CORE requires password auth, secure KEXINIT, session reader and secure TCP session helpers"
#endif



#if defined(XSSH_FEATURE_CLIENT_CORE)

#define XSSH_CLIENT_OUTPUT_INITIAL_DEFAULT 4096u
#define XSSH_CLIENT_OUTPUT_LIMIT_DEFAULT 1048576u
#define XSSH_CLIENT_VERSION_DEFAULT "SSH-2.0-xssh"



typedef struct xsshclientcore xsshclientcore;



/* 主机密钥策略必须显式接受；延迟决定时底层连接保持在 VERIFY_HOST_KEY。 */
typedef enum xsshclienthostdecision {
	XSSH_CLIENT_HOST_REJECT = 0,
	XSSH_CLIENT_HOST_ACCEPT = 1,
	XSSH_CLIENT_HOST_DEFER = 2
} xsshclienthostdecision;



/* 一次主机验证同时提供完整 key blob 和本轮稳定协商结果。 */
typedef struct xsshclienthost {
	xbytesview Key;
	xsshkexnegotiation Negotiation;
} xsshclienthost;



/* 认证构建器可根据服务端方法列表和已经提交的尝试数选择下一种方法。 */
typedef struct xsshclientauth {
	xstrview User;
	xstrview Methods;
	xbytesview SessionId;
	uint32 Attempts;
	bool PartialSuccess;
} xsshclientauth;



typedef xsshclienthostdecision (*xsshclienthostproc)(
	xsshclientcore* pClient,
	const xsshclienthost* pHost,
	ptr pUserData
);



/* 返回 SPACE 时核心扩展动态输出并重试；NEED_MORE 表示凭据尚未就绪。 */
typedef xsshcode (*xsshclientauthproc)(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pUserData
);



/* 客户端核心配置只借用文本、凭据上下文和回调，不拥有网络对象。 */
typedef struct xsshclientcoreconfig {
	xsshkexinitconfig Kex;
	xsshauthguardpolicy AuthGuard;
	xstrview Version;
	xstrview User;
	xsshclienthostproc HostKey;
	ptr HostKeyData;
	xsshclientauthproc Authenticate;
	ptr AuthenticateData;
	size_t OutputInitial;
	size_t OutputLimit;
	bool ProbeNone;
} xsshclientcoreconfig;



/* Next 返回的结果区分线路输出、输入等待、外部决策和 connection 就绪。 */
typedef enum xsshclientnextkind {
	XSSH_CLIENT_NEXT_INPUT = 0,
	XSSH_CLIENT_NEXT_TRANSACTION = 1,
	XSSH_CLIENT_NEXT_IDENTIFICATION = 2,
	XSSH_CLIENT_NEXT_PAYLOAD = 3,
	XSSH_CLIENT_NEXT_HOST_KEY = 4,
	XSSH_CLIENT_NEXT_AUTH = 5,
	XSSH_CLIENT_NEXT_READY = 6,
	XSSH_CLIENT_NEXT_CLOSING = 7
} xsshclientnextkind;



/* IDENTIFICATION 使用 Text，PAYLOAD 使用 Data，其余分类返回空视图。 */
typedef struct xsshclientnext {
	xstrview Text;
	xbytesview Data;
	xsshclientnextkind Kind;
} xsshclientnext;



/* 核心拥有可增长敏感输出和服务端方法副本，不拥有 SSH 会话与 Reader。 */
struct xsshclientcore {
	xsshclientcoreconfig Config;
	bytes Output;
	char* AuthMethods;
	size_t OutputCapacity;
	size_t AuthMethodsSize;
	bool AuthPartialSuccess;
	bool Initialized;
	uint32 Guard;
};



/* Timer 参数为 xrtTimer() 的 double 秒数，必须有限且非负；配置时长仍用毫秒。 */
XRT_EXTERN_C_BEGIN



/* 写入客户端安全默认值；默认拒绝未配置验证器的主机密钥。 */
XRT_API bool xrtSshClientCoreConfigInit(xsshclientcoreconfig* pConfig);



/* 复制配置并创建有界动态输出；所有借用配置必须存活到 Clear。 */
XRT_API bool xrtSshClientCoreInit(
	xsshclientcore* pClient,
	const xsshclientcoreconfig* pConfig
);



/* 安全清除可能包含口令的输出、认证方法和配置借用视图。 */
XRT_API void xrtSshClientCoreClear(xsshclientcore* pClient);



/*
	推进全部无线路等待的客户端动作，直到需要输入、输出、外部决定或已经就绪。
	PAYLOAD 借用核心动态输出，调用方必须在再次调用 Next 前完成 SessionTcpWritePrepare。
*/
XRT_API xsshcode xrtSshClientCoreNext(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession,
	const xsshsessionreader* pReader,
	double Timer,
	xsshclientnext* pNext
);



/* 在认证 packet 提交前复制 FAILURE 方法列表；空间不足可重试同一输入。 */
XRT_API xsshcode xrtSshClientCoreObserve(
	xsshclientcore* pClient,
	const xsshsessiontcp* pSession,
	const xsshsessiontcppacket* pPacket
);



/* 显式完成此前延迟的主机密钥决定；拒绝会终止当前 KEX。 */
XRT_API xsshcode xrtSshClientCoreHostKeyAccept(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession
);
XRT_API xsshcode xrtSshClientCoreHostKeyReject(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession
);



/* 返回最近一次 USERAUTH_FAILURE 的稳定方法列表与 partial-success 标记。 */
XRT_API xstrview xrtSshClientCoreAuthMethods(
	const xsshclientcore* pClient,
	bool* pPartialSuccess
);



/* 通用 password 构建器；UserData 指向在调用期间有效的 xstrview 口令。 */
XRT_API xsshcode xrtSshClientPasswordAuth(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pUserData
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_client_auth_ed25519.h */
/* ========================================================================== */

#ifndef XRT_SSH_CLIENT_AUTH_ED25519_H
#define XRT_SSH_CLIENT_AUTH_ED25519_H




#if defined(XSSH_FEATURE_CLIENT_AUTH_ED25519) && \
	(!defined(XSSH_FEATURE_CLIENT_CORE) || \
	 !defined(XSSH_FEATURE_AUTH_PUBLICKEY) || \
	 !defined(XSSH_FEATURE_PRIVATE_KEY_ED25519))
	#error "XSSH_FEATURE_CLIENT_AUTH_ED25519 requires client core, publickey auth and Ed25519 private-key support"
#endif



#if defined(XSSH_FEATURE_CLIENT_AUTH_ED25519)

XRT_EXTERN_C_BEGIN



/* 使用借用的 Ed25519 身份直接构建带签名 publickey 请求，不执行多余的 probe 往返。 */
XRT_API xsshcode xrtSshClientEd25519Auth(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pUserData
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_client.h */
/* ========================================================================== */

#ifndef XRT_SSH_CLIENT_H
#define XRT_SSH_CLIENT_H




#if defined(XSSH_FEATURE_CLIENT) && \
	(!defined(XSSH_FEATURE_CHANNELS) || \
	 !defined(XSSH_FEATURE_CLIENT_CORE) || \
	 !defined(XSSH_FEATURE_SESSION_STREAM))
	#error "XSSH_FEATURE_CLIENT requires channels, client core and session stream"
#endif



#if defined(XSSH_FEATURE_CLIENT)

#define XSSH_CLIENT_CONTROL_INITIAL_DEFAULT 4096u
#define XSSH_CLIENT_CONTROL_LIMIT_DEFAULT 1048576u
#define XSSH_CLIENT_GLOBAL_REPLY_LIMIT_DEFAULT 64u
/* TCP 建连后到 Ready 的默认截止时间，单位为毫秒；配置为零时禁用。 */
#define XSSH_CLIENT_READY_TIMEOUT_DEFAULT INT64_C(30000)



typedef struct xsshclient xsshclient;



/* 客户端状态只表达对象与连接生命周期，不复制底层 SSH 阶段。 */
typedef enum xsshclientstate {
	XSSH_CLIENT_CREATED = 0,
	XSSH_CLIENT_HANDSHAKE = 1,
	XSSH_CLIENT_READY = 2,
	XSSH_CLIENT_CLOSING = 3,
	XSSH_CLIENT_CLOSED = 4,
	XSSH_CLIENT_INVALID = 5
} xsshclientstate;



/* Peer channel open 的决定先暂存，读事务提交后才发送线路响应。 */
typedef enum xsshclientchanneldecision {
	XSSH_CLIENT_CHANNEL_NONE = 0,
	XSSH_CLIENT_CHANNEL_ACCEPT = 1,
	XSSH_CLIENT_CHANNEL_REJECT = 2
} xsshclientchanneldecision;



/* Channel 事件只在对应 SSH 读写事务可靠提交后发布。 */
typedef enum xsshclientchannelevent {
	XSSH_CLIENT_CHANNEL_EVENT_OPENED = 0,
	XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED = 1,
	XSSH_CLIENT_CHANNEL_EVENT_WRITABLE = 2,
	XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS = 3,
	XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE = 4,
	XSSH_CLIENT_CHANNEL_EVENT_EOF = 5,
	XSSH_CLIENT_CHANNEL_EVENT_CLOSED = 6
} xsshclientchannelevent;



/* 通知不借用 packet；Channel 保持有效，直到应用显式移除或客户端清理。 */
typedef struct xsshclientchannelnotice {
	xsshchannel* Channel;
	uint64 ReplyToken;
	uint32 Reason;
	xsshclientchannelevent Event;
	bool HasReplyToken;
	bool Incoming;
} xsshclientchannelnotice;



/* 全局请求回复只在 FIFO 出队与 SSH 读事务共同提交后发布。 */
typedef enum xsshclientglobalevent {
	XSSH_CLIENT_GLOBAL_EVENT_REQUEST_SUCCESS = 0,
	XSSH_CLIENT_GLOBAL_EVENT_REQUEST_FAILURE = 1
} xsshclientglobalevent;



/* 全局通知不借用 packet，ReplyToken 是调用请求时提供的稳定关联值。 */
typedef struct xsshclientglobalnotice {
	uint64 ReplyToken;
	xsshclientglobalevent Event;
} xsshclientglobalnotice;



/* 动态控制报文构建器可返回 SPACE 扩容重试，或 NEED_MORE 等待外部数据。 */
typedef xsshcode (*xsshclientbuildproc)(
	xsshwriter* pWriter,
	ptr pUserData
);



/* Channel open 构建器借用只读 core，用于 session 与 forwarding 类型扩展。 */
typedef xsshcode (*xsshclientchannelopenproc)(
	xsshwriter* pWriter,
	const xsshchannelcore* pChannel,
	ptr pUserData
);



/* Packet 在读事务提交前执行；Data 在对应 channel I/O 提交后执行。 */
typedef struct xsshclientevents {
	void (*Open)(xsshclient* pClient, ptr pData);
	void (*Ready)(xsshclient* pClient, ptr pData);
	void (*HostKey)(xsshclient* pClient, ptr pData);
	void (*Authenticate)(xsshclient* pClient, ptr pData);
	xsshsessionstreamdecision (*Packet)(
		xsshclient* pClient,
		const xsshsessiontcppacket* pPacket,
		ptr pData
	);
	void (*Data)(
		xsshclient* pClient,
		xsshchannel* pChannel,
		xsshchanneliostream Stream,
		ptr pData
	);
	void (*Rekey)(
		xsshclient* pClient,
		xsshrekeydecision Decision,
		ptr pData
	);
	void (*Error)(
		xsshclient* pClient,
		xsshcode Code,
		const xerror* pError,
		ptr pData
	);
	void (*End)(xsshclient* pClient, ptr pData);
	void (*HighWater)(
		xsshclient* pClient,
		size_t iQueued,
		ptr pData
	);
	void (*LowWater)(
		xsshclient* pClient,
		size_t iQueued,
		ptr pData
	);
	void (*Drain)(xsshclient* pClient, ptr pData);
	void (*Close)(
		xsshclient* pClient,
		xnetresult Result,
		const xerror* pError,
		ptr pData
	);
	void (*Channel)(
		xsshclient* pClient,
		const xsshclientchannelnotice* pNotice,
		ptr pData
	);
	void (*Global)(
		xsshclient* pClient,
		const xsshclientglobalnotice* pNotice,
		ptr pData
	);
} xsshclientevents;



/* 配置组合握手策略、Ready 截止时间、动态 channel 预算和控制报文上限。 */
typedef struct xsshclientconfig {
	xsshclientcoreconfig Core;
	xsshchannelsconfig Channels;
	int64 ReadyTimeout;
	size_t ControlInitial;
	size_t ControlLimit;
	size_t GlobalReplyLimit;
} xsshclientconfig;



/* 客户端拥有协议状态和动态数据，但只借用 Stream、Worker 与用户配置视图。 */
struct xsshclient {
	xsshsessionstream Stream;
	xsshclientcore Core;
	xsshchannels Channels;
	xsshreplyqueue GlobalReplies;
	xnetbuf Control;
	xsshclientconfig Config;
	xsshclientevents Events;
	xerror* TerminalError;
	xsshchannel* ReceiveChannel;
	xsshchannel* SendChannel;
	xsshchannel* OpenPendingChannel;
	xsshchannel* OpenSendChannel;
	const xsshchannelopen* OpenCurrent;
	xsshclientchannelnotice ChannelNotice;
	xsshclientglobalnotice GlobalNotice;
	uint64* GlobalReplyTokens;
	uint64 ReadyTimer;
	ptr UserData;
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		ptr FutureState;
		xsshchannel* FutureWritableChannel;
		uint32 FutureWritableLocal;
	#endif
	size_t ControlTarget;
	size_t GlobalReplyCapacity;
	xsshchanneliostream ReceiveStream;
	xsshclientstate State;
	xsshclientchanneldecision OpenDecision;
	uint32 OpenReason;
	bool ResourcesReady;
	bool ReceivePending;
	bool ReceiveRetry;
	bool ReceiveRetryOpen;
	bool SendPending;
	bool ChannelNoticePending;
	bool GlobalNoticePending;
	bool ReadyNotified;
	bool HostNotified;
	bool AuthNotified;
	uint32 Guard;
};



XRT_EXTERN_C_BEGIN



/* 写入没有隐藏 Engine、默认拒绝未知主机且预算有界的客户端配置。 */
XRT_API bool xrtSshClientConfigInit(xsshclientconfig* pConfig);



/* 初始化未附着客户端；配置中的文本、策略数据和回调上下文保持借用。 */
XRT_API bool xrtSshClientInit(
	xsshclient* pClient,
	const xsshclientconfig* pConfig,
	const xsshclientevents* pEvents,
	ptr pData
);



/* 只清理尚未附着或已经关闭的客户端及其全部动态 channel 数据。 */
XRT_API bool xrtSshClientClear(xsshclient* pClient);



/* 返回交给 xrtNetStreamConnect 的事件表和 data 指针。 */
XRT_API const xnetstreamevents* xrtSshClientNetEvents(void);
XRT_API ptr xrtSshClientNetData(xsshclient* pClient);



/* 在 Stream 所属 Worker 中接管已打开且尚无积压输入的 TCP Stream。 */
XRT_API bool xrtSshClientAttach(
	xsshclient* pClient,
	xnetstream* pStream
);



/* 返回客户端生命周期以及完整底层对象，保留高级用户的直接控制能力。 */
XRT_API xsshclientstate xrtSshClientState(const xsshclient* pClient);
XRT_API bool xrtSshClientIsCurrent(const xsshclient* pClient);
XRT_API xsshsessionstream* xrtSshClientStream(xsshclient* pClient);
XRT_API xsshsessiontcp* xrtSshClientSession(xsshclient* pClient);
XRT_API xsshsessionreader* xrtSshClientReader(xsshclient* pClient);
XRT_API xsshchannels* xrtSshClientChannels(xsshclient* pClient);



/* 判断借用 channel 是否仍由当前客户端拥有。 */
XRT_API bool xrtSshClientOwnsChannel(
	const xsshclient* pClient,
	const xsshchannel* pChannel
);



/* 在 CHANNEL_OPEN Packet/HOLD 期间暂存接受或拒绝决定。 */
XRT_API xsshcode xrtSshClientChannelAccept(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	xsshchannel** ppChannel
);
XRT_API xsshcode xrtSshClientChannelReject(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	uint32 iReason
);



/* 返回全局 request reply FIFO，并按需扩展其有界 token 存储。 */
XRT_API xsshreplyqueue* xrtSshClientGlobalReplies(xsshclient* pClient);
XRT_API xsshcode xrtSshClientGlobalReplyReserve(
	xsshclient* pClient,
	size_t iCapacity
);



/* 凭据或其他外部认证数据就绪后，重新推进当前客户端动作。 */
XRT_API xsshcode xrtSshClientContinue(xsshclient* pClient);



/* 完成被 HostKey 回调延迟的信任决定。 */
XRT_API xsshcode xrtSshClientHostKeyAccept(xsshclient* pClient);
XRT_API xsshcode xrtSshClientHostKeyReject(xsshclient* pClient);



/* 提交或拒绝用户 Packet 回调保留的输入；Retry 专门重试内部 OOM 暂停。 */
XRT_API xsshcode xrtSshClientPacketAccept(xsshclient* pClient);
XRT_API xsshcode xrtSshClientPacketReject(xsshclient* pClient);
XRT_API xsshcode xrtSshClientPacketRetry(xsshclient* pClient);



/* 直接把完整 payload 编码并提交给有界 TCP 队列；视图只借用到函数返回。 */
XRT_API xsshcode xrtSshClientSend(
	xsshclient* pClient,
	xbytesview Payload,
	xsshchannel* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
);



/* 在动态连续 scratch 中构建并发送一个控制报文，SPACE 会按上限扩容重试。 */
XRT_API xsshcode xrtSshClientBuild(
	xsshclient* pClient,
	xsshclientbuildproc pBuild,
	ptr pBuildData,
	xsshchannel* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
);



/* 创建动态 channel，并用类型专用构建器发送唯一 CHANNEL_OPEN。 */
XRT_API xsshcode xrtSshClientChannelOpen(
	xsshclient* pClient,
	xsshclientchannelopenproc pOpen,
	ptr pOpenData,
	xsshchannel** ppChannel
);



/* 把指定 channel 发送队首封装为一条受窗口和最大包约束的数据消息。 */
XRT_API xsshcode xrtSshClientChannelFlush(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream
);



/* 发送当前已消费接收数据对应的 WINDOW_ADJUST；无额度时返回 NEED_MORE。 */
XRT_API xsshcode xrtSshClientChannelAdjust(
	xsshclient* pClient,
	xsshchannel* pChannel
);



/* 发送 channel 写方向 EOF 或双向关闭消息。 */
XRT_API xsshcode xrtSshClientChannelEof(
	xsshclient* pClient,
	xsshchannel* pChannel
);
XRT_API xsshcode xrtSshClientChannelClose(
	xsshclient* pClient,
	xsshchannel* pChannel
);



/* 中止未决 SSH 事务并异常关闭底层 TCP Stream。 */
XRT_API bool xrtSshClientAbort(xsshclient* pClient);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_client_session.h */
/* ========================================================================== */

#ifndef XRT_SSH_CLIENT_SESSION_H
#define XRT_SSH_CLIENT_SESSION_H




#if defined(XSSH_FEATURE_CLIENT_SESSION) && \
	(!defined(XSSH_FEATURE_CHANNEL_REQUEST) || \
	 !defined(XSSH_FEATURE_CLIENT))
	#error "XSSH_FEATURE_CLIENT_SESSION requires channel request and client"
#endif



#if defined(XSSH_FEATURE_CLIENT_SESSION)

#define XSSH_CHANNEL_TYPE_SESSION "session"



XRT_EXTERN_C_BEGIN



/* 创建等待服务端 confirmation 的 session channel。 */
XRT_API xsshcode xrtSshClientSessionOpen(
	xsshclient* pClient,
	xsshchannel** ppChannel
);



/* 发送保留未知扩展字段的 session request。 */
XRT_API xsshcode xrtSshClientSessionRequest(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xstrview Type,
	xbytesview Fields,
	bool bWantReply,
	uint64 iReplyToken
);



/* 设置一个环境变量；回复 token 按同一 channel 的 FIFO 顺序返回。 */
XRT_API xsshcode xrtSshClientSessionEnv(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Name,
	xbytesview Value,
	bool bWantReply,
	uint64 iReplyToken
);



/* 请求交互 shell、单条命令或命名 subsystem。 */
XRT_API xsshcode xrtSshClientSessionShell(
	xsshclient* pClient,
	xsshchannel* pChannel,
	bool bWantReply,
	uint64 iReplyToken
);
XRT_API xsshcode xrtSshClientSessionExec(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Command,
	bool bWantReply,
	uint64 iReplyToken
);
XRT_API xsshcode xrtSshClientSessionSubsystem(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Subsystem,
	bool bWantReply,
	uint64 iReplyToken
);



/* 向远端进程发送规范 signal 或 RFC 4335 break。 */
XRT_API xsshcode xrtSshClientSessionSignal(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xstrview Signal
);
XRT_API xsshcode xrtSshClientSessionBreak(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint32 iLengthMs,
	bool bWantReply,
	uint64 iReplyToken
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_client_dial.h */
/* ========================================================================== */

#ifndef XRT_SSH_CLIENT_DIAL_H
#define XRT_SSH_CLIENT_DIAL_H




#if defined(XSSH_FEATURE_CLIENT_DIAL) && \
	(!defined(XSSH_FEATURE_CLIENT) || \
	 !defined(XRT_FEATURE_NET_TCP_DIAL))
	#error "XSSH_FEATURE_CLIENT_DIAL requires SSH client and XRT TCP Dial"
#endif



#if defined(XSSH_FEATURE_CLIENT_DIAL)

XRT_EXTERN_C_BEGIN



/*
	通过 XRT Resolver 和 TCP Dial 建链，并在公开 TCP Open 前安装 SSH 驱动。
	完成回调只表示 TCP Dial 终态；SSH 握手完成由 xsshclientevents.Ready 发布。
*/
XRT_API xnetdial* xrtSshClientDial(
	xsshclient* pClient,
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	xnetdialproc pDone,
	ptr pData
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_client_forward.h */
/* ========================================================================== */

#ifndef XRT_SSH_CLIENT_FORWARD_H
#define XRT_SSH_CLIENT_FORWARD_H




#if defined(XSSH_FEATURE_CLIENT_FORWARD) && \
	(!defined(XSSH_FEATURE_CLIENT) || \
	 !defined(XSSH_FEATURE_FORWARD_MESSAGE))
	#error "XSSH_FEATURE_CLIENT_FORWARD requires client and forward message"
#endif



#if defined(XSSH_FEATURE_CLIENT_FORWARD)

XRT_EXTERN_C_BEGIN



/* 打开由 SSH 服务端连接目标地址的 direct-tcpip channel。 */
XRT_API xsshcode xrtSshClientDirectTcpipOpen(
	xsshclient* pClient,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort,
	xsshchannel** ppChannel
);



/* 在 Packet 回调/HOLD 中解析并暂存接受 forwarded-tcpip channel。 */
XRT_API xsshcode xrtSshClientForwardedTcpipAccept(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip,
	xsshchannel** ppChannel
);



/* 请求或取消服务端监听 remote forwarding 地址。 */
XRT_API xsshcode xrtSshClientTcpipForward(
	xsshclient* pClient,
	xbytesview Address,
	uint32 iPort,
	uint64 iReplyToken
);
XRT_API xsshcode xrtSshClientTcpipForwardCancel(
	xsshclient* pClient,
	xbytesview Address,
	uint32 iPort,
	uint64 iReplyToken
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_client_future.h */
/* ========================================================================== */

#ifndef XRT_SSH_CLIENT_FUTURE_H
#define XRT_SSH_CLIENT_FUTURE_H




#if defined(XSSH_FEATURE_CLIENT_FUTURE) && \
	(!defined(XSSH_FEATURE_CLIENT) || \
	 !defined(XRT_FEATURE_FUTURE) || \
	 !defined(XRT_FEATURE_SPIN))
	#error "XSSH_FEATURE_CLIENT_FUTURE requires SSH client, XRT Future and spin"
#endif



#if defined(XSSH_FEATURE_CLIENT_FUTURE)

/* 客户端等待是水平条件；Drain 只表示底层 TCP 发送队列已经排空。 */
typedef enum xsshclientwait {
	XSSH_CLIENT_WAIT_READY = 0,
	XSSH_CLIENT_WAIT_DRAIN = 1,
	XSSH_CLIENT_WAIT_CLOSE = 2
} xsshclientwait;



/* Channel 等待不消费数据，也不改变 EOF/CLOSE 或请求回复状态。 */
typedef enum xsshclientchannelwait {
	XSSH_CLIENT_CHANNEL_WAIT_OPEN = 0,
	XSSH_CLIENT_CHANNEL_WAIT_WRITE = 1,
	XSSH_CLIENT_CHANNEL_WAIT_EOF = 2,
	XSSH_CLIENT_CHANNEL_WAIT_CLOSE = 3
} xsshclientchannelwait;



XRT_EXTERN_C_BEGIN



/* 在客户端 Worker 上创建 Ready、TCP Drain 或 Close 的单次等待。 */
XRT_API xfuture* xrtSshClientWaitAsync(
	xsshclient* pClient,
	xsshclientwait Wait
);



/* 在客户端 Worker 上创建 channel open、可写、EOF 或 close 等待。 */
XRT_API xfuture* xrtSshClientChannelWaitAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshclientchannelwait Wait
);



/* 等待指定接收流出现至少一个可读字节；成功不会消费缓冲。 */
XRT_API xfuture* xrtSshClientChannelReadAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream
);



/* 等待指定 channel 请求 token 的 success/failure 回复。 */
XRT_API xfuture* xrtSshClientChannelReplyAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint64 iReplyToken
);



/* 等待指定全局请求 token 的 success/failure 回复。 */
XRT_API xfuture* xrtSshClientGlobalReplyAsync(
	xsshclient* pClient,
	uint64 iReplyToken
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xrt/ssh_client_pty.h */
/* ========================================================================== */

#ifndef XRT_SSH_CLIENT_PTY_H
#define XRT_SSH_CLIENT_PTY_H




#if defined(XSSH_FEATURE_CLIENT_PTY) && \
	(!defined(XSSH_FEATURE_CHANNEL_PTY) || \
	 !defined(XSSH_FEATURE_CLIENT_SESSION))
	#error "XSSH_FEATURE_CLIENT_PTY requires channel PTY and client session"
#endif



#if defined(XSSH_FEATURE_CLIENT_PTY)

XRT_EXTERN_C_BEGIN



/* 请求 PTY；Modes 必须是以 TTY_OP_END 结束的已编码 mode 流。 */
XRT_API xsshcode xrtSshClientSessionPty(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Terminal,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight,
	xbytesview Modes,
	bool bWantReply,
	uint64 iReplyToken
);



/* 发送不要求回复的终端窗口尺寸变化。 */
XRT_API xsshcode xrtSshClientSessionResize(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xssh/include/xssh.h */
/* ========================================================================== */

#ifndef XSSH_H
#define XSSH_H


#if defined(XSSH_FEATURE_SSH) && \
	(!defined(XSSH_FEATURE_SESSION_STREAM) || \
	 !defined(XSSH_FEATURE_CLIENT_CORE) || \
	 !defined(XSSH_FEATURE_CLIENT_AUTH_ED25519) || \
	 !defined(XSSH_FEATURE_CLIENT) || \
	 !defined(XSSH_FEATURE_CLIENT_DIAL) || \
	 !defined(XSSH_FEATURE_CLIENT_FUTURE) || \
	 !defined(XSSH_FEATURE_CLIENT_SESSION) || \
	 !defined(XSSH_FEATURE_CLIENT_FORWARD) || \
	 !defined(XSSH_FEATURE_CLIENT_PTY))
	#error "XSSH_FEATURE_SSH requires stream and client session support"
#endif

#endif

#endif

#if defined(XSSH_IMPLEMENTATION) && !defined(XSSH_IMPLEMENTATION_ONCE)
#define XSSH_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xssh/src/key/ssh_key_text_internal.h */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEY_TEXT)
#ifndef XSSH_KEY_TEXT_INTERNAL_H
#define XSSH_KEY_TEXT_INTERNAL_H


#include <string.h>



/* OpenSSH 文本字段只把空格和水平制表符视为字段分隔。 */
static inline bool xsshKeyTextSpace(unsigned char iCharacter)
{
	return (iCharacter == (unsigned char)' ') ||
		(iCharacter == (unsigned char)'\t');
}



/* 去掉水平空白和一个行结束符，并拒绝嵌入行、NUL 与 DEL。 */
static inline xsshcode xsshKeyTextBounds(
	xstrview Line,
	size_t* pStart,
	size_t* pEnd
)
{
	size_t iStart = 0u;
	size_t iEnd = Line.Size;
	size_t i;

	if ( !xrtMemRangeValid(Line.Data, Line.Size) ||
		(pStart == NULL) || (pEnd == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	while ( (iStart < iEnd) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iStart]
	) ) {
		++iStart;
	}
	while ( (iEnd > iStart) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iEnd - 1u]
	) ) {
		--iEnd;
	}
	if ( (iEnd > iStart) && (Line.Data[iEnd - 1u] == '\n') ) {
		--iEnd;
		if ( (iEnd > iStart) && (Line.Data[iEnd - 1u] == '\r') ) {
			--iEnd;
		}
	} else if ( (iEnd > iStart) && (Line.Data[iEnd - 1u] == '\r') ) {
		--iEnd;
	}
	while ( (iEnd > iStart) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iEnd - 1u]
	) ) {
		--iEnd;
	}
	if ( (iStart == iEnd) || (Line.Data[iStart] == '#') ) {
		return XSSH_ERROR_PROTOCOL;
	}
	for ( i = iStart; i < iEnd; ++i ) {
		unsigned char iCharacter = (unsigned char)Line.Data[i];

		if ( (iCharacter == 0u) || (iCharacter == 0x7fu) ||
			(iCharacter == (unsigned char)'\r') ||
			(iCharacter == (unsigned char)'\n') ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	*pStart = iStart;
	*pEnd = iEnd;
	return XSSH_OK;
}



/* 读取一个允许双引号与反斜杠转义的 authorized_keys 字段。 */
static inline xsshcode xsshKeyTextToken(
	xstrview Line,
	size_t iEnd,
	size_t* pPosition,
	xstrview* pToken,
	bool* pPresent
)
{
	size_t i;
	size_t iStart;
	bool bQuoted = false;

	if ( (pPosition == NULL) || (pToken == NULL) ||
		(pPresent == NULL) || (*pPosition > iEnd) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	i = *pPosition;
	while ( (i < iEnd) && xsshKeyTextSpace(
		(unsigned char)Line.Data[i]
	) ) {
		++i;
	}
	if ( i == iEnd ) {
		*pPresent = false;
		return XSSH_OK;
	}
	iStart = i;
	while ( i < iEnd ) {
		unsigned char iCharacter = (unsigned char)Line.Data[i];

		if ( !bQuoted && xsshKeyTextSpace(iCharacter) ) {
			break;
		}
		if ( iCharacter == (unsigned char)'"' ) {
			bQuoted = !bQuoted;
			++i;
			continue;
		}
		if ( bQuoted && (iCharacter == (unsigned char)'\\') ) {
			if ( (i + 1u) == iEnd ) {
				return XSSH_ERROR_PROTOCOL;
			}
			i += 2u;
			continue;
		}
		if ( (iCharacter < 0x20u) &&
			(iCharacter != (unsigned char)'\t') ) {
			return XSSH_ERROR_PROTOCOL;
		}
		++i;
	}
	if ( bQuoted || (i == iStart) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pToken->Data = Line.Data + iStart;
	pToken->Size = i - iStart;
	*pPosition = i;
	*pPresent = true;
	return XSSH_OK;
}



/* 快速筛选标准 Base64 token；规范尾位仍由 XRT Base64 严格校验。 */
static inline bool xsshKeyTextBase64Shape(xstrview Text)
{
	size_t iPadding = 0u;
	size_t i;

	if ( !xrtMemRangeValid(Text.Data, Text.Size) ||
		(Text.Size < 4u) || ((Text.Size % 4u) != 0u) ) {
		return false;
	}
	if ( Text.Data[Text.Size - 1u] == '=' ) {
		++iPadding;
	}
	if ( Text.Data[Text.Size - 2u] == '=' ) {
		++iPadding;
	}
	for ( i = 0u; i < (Text.Size - iPadding); ++i ) {
		unsigned char iCharacter = (unsigned char)Text.Data[i];

		if ( !(((iCharacter >= (unsigned char)'A') &&
			(iCharacter <= (unsigned char)'Z')) ||
			((iCharacter >= (unsigned char)'a') &&
			 (iCharacter <= (unsigned char)'z')) ||
			((iCharacter >= (unsigned char)'0') &&
			 (iCharacter <= (unsigned char)'9')) ||
			(iCharacter == (unsigned char)'+') ||
			(iCharacter == (unsigned char)'/')) ) {
			return false;
		}
	}
	for ( ; i < Text.Size; ++i ) {
		if ( Text.Data[i] != '=' ) {
			return false;
		}
	}
	return true;
}



/* 返回已通过标准 Base64 校验的一个字符值，填充只用于尾组。 */
static inline uint32 xsshKeyTextBase64Value(unsigned char iCharacter)
{
	if ( (iCharacter >= (unsigned char)'A') &&
		(iCharacter <= (unsigned char)'Z') ) {
		return (uint32)(iCharacter - (unsigned char)'A');
	}
	if ( (iCharacter >= (unsigned char)'a') &&
		(iCharacter <= (unsigned char)'z') ) {
		return (uint32)(iCharacter - (unsigned char)'a') + 26u;
	}
	if ( (iCharacter >= (unsigned char)'0') &&
		(iCharacter <= (unsigned char)'9') ) {
		return (uint32)(iCharacter - (unsigned char)'0') + 52u;
	}
	return iCharacter == (unsigned char)'+' ? 62u :
		(iCharacter == (unsigned char)'/' ? 63u : 0u);
}



/* 从已验证 Base64 直接读取指定解码字节，不复制其余 blob。 */
static inline unsigned char xsshKeyTextBase64At(
	xstrview Text,
	size_t iIndex
)
{
	size_t iInput = (iIndex / 3u) * 4u;
	uint32 iValue =
		(xsshKeyTextBase64Value((unsigned char)Text.Data[iInput]) << 18u) |
		(xsshKeyTextBase64Value((unsigned char)Text.Data[iInput + 1u]) << 12u) |
		(xsshKeyTextBase64Value((unsigned char)Text.Data[iInput + 2u]) << 6u) |
		xsshKeyTextBase64Value((unsigned char)Text.Data[iInput + 3u]);

	switch ( iIndex % 3u ) {
		case 0u:
			return (unsigned char)(iValue >> 16u);
		case 1u:
			return (unsigned char)(iValue >> 8u);
		default:
			return (unsigned char)iValue;
	}
}



/* 校验 Base64 blob 的首个 SSH string 与文本算法完全一致。 */
static inline bool xsshKeyTextBase64AlgorithmEqual(
	xstrview Text,
	size_t iBlobSize,
	xstrview Algorithm
)
{
	uint32 iAlgorithmSize;
	size_t i;

	if ( (Algorithm.Size > UINT32_MAX) ||
		(Algorithm.Size > (SIZE_MAX - 4u)) ||
		(iBlobSize < (4u + Algorithm.Size)) ) {
		return false;
	}
	iAlgorithmSize = (uint32)Algorithm.Size;
	if ( (xsshKeyTextBase64At(Text, 0u) !=
		 (unsigned char)(iAlgorithmSize >> 24u)) ||
		(xsshKeyTextBase64At(Text, 1u) !=
		 (unsigned char)(iAlgorithmSize >> 16u)) ||
		(xsshKeyTextBase64At(Text, 2u) !=
		 (unsigned char)(iAlgorithmSize >> 8u)) ||
		(xsshKeyTextBase64At(Text, 3u) !=
		 (unsigned char)iAlgorithmSize) ) {
		return false;
	}
	for ( i = 0u; i < Algorithm.Size; ++i ) {
		if ( xsshKeyTextBase64At(Text, 4u + i) !=
			(unsigned char)Algorithm.Data[i] ) {
			return false;
		}
	}
	return true;
}



/* 比较两个借用文本视图。 */
static inline bool xsshKeyTextEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 逐组比较规范 Base64 与原始字节，不建立完整编码或解码副本。 */
static inline bool xsshKeyTextBase64Equal(
	xstrview Text,
	xbytesview Data
)
{
	static const char sAlphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t iGroups = Data.Size / 3u;
	size_t iRemain = Data.Size % 3u;
	size_t iTail = iRemain == 0u ? 0u : 4u;
	size_t iExpected;
	size_t i;

	if ( !xrtMemRangeValid(Text.Data, Text.Size) ||
		!xrtMemRangeValid(Data.Data, Data.Size) ||
		(iGroups > ((SIZE_MAX - iTail) / 4u)) ) {
		return false;
	}
	iExpected = (iGroups * 4u) + iTail;
	if ( Text.Size != iExpected ) {
		return false;
	}
	for ( i = 0u; i < iGroups; ++i ) {
		size_t iInput = i * 3u;
		size_t iOutput = i * 4u;
		uint32 iValue = ((uint32)Data.Data[iInput] << 16u) |
			((uint32)Data.Data[iInput + 1u] << 8u) |
			(uint32)Data.Data[iInput + 2u];

		if ( (Text.Data[iOutput] != sAlphabet[(iValue >> 18u) & 0x3fu]) ||
			(Text.Data[iOutput + 1u] != sAlphabet[(iValue >> 12u) & 0x3fu]) ||
			(Text.Data[iOutput + 2u] != sAlphabet[(iValue >> 6u) & 0x3fu]) ||
			(Text.Data[iOutput + 3u] != sAlphabet[iValue & 0x3fu]) ) {
			return false;
		}
	}
	if ( iRemain != 0u ) {
		size_t iInput = iGroups * 3u;
		size_t iOutput = iGroups * 4u;
		uint32 iValue = (uint32)Data.Data[iInput] << 16u;

		if ( iRemain == 2u ) {
			iValue |= (uint32)Data.Data[iInput + 1u] << 8u;
		}
		if ( (Text.Data[iOutput] != sAlphabet[(iValue >> 18u) & 0x3fu]) ||
			(Text.Data[iOutput + 1u] != sAlphabet[(iValue >> 12u) & 0x3fu]) ||
			(Text.Data[iOutput + 2u] != (iRemain == 2u ?
			 sAlphabet[(iValue >> 6u) & 0x3fu] : '=')) ||
			(Text.Data[iOutput + 3u] != '=') ) {
			return false;
		}
	}
	return true;
}

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xssh/src/key/ssh_known_host_internal.h */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KNOWN_HOST)
#ifndef XSSH_KNOWN_HOST_INTERNAL_H
#define XSSH_KNOWN_HOST_INTERNAL_H




/* 非默认端口使用虚拟 [host]:port 视图，避免为任意长度主机分配缓冲。 */
typedef struct xsshknownhosttarget {
	xstrview Host;
	char Port[5];
	size_t PortSize;
	size_t Size;
	bool Bracketed;
} xsshknownhosttarget;



/* 校验原始主机与端口，并预计算十进制端口。 */
static inline bool xsshKnownHostTargetInit(
	xsshknownhosttarget* pTarget,
	xstrview Host,
	uint32 iPort
)
{
	char arrReverse[5];
	size_t iPortSize = 0u;
	size_t i;
	uint32 iValue = iPort;

	if ( (pTarget == NULL) || !xrtMemRangeValid(Host.Data, Host.Size) ||
		(Host.Size == 0u) || (iPort == 0u) || (iPort > 65535u) ||
		(Host.Size > (SIZE_MAX - 8u)) ) {
		return false;
	}
	for ( i = 0u; i < Host.Size; ++i ) {
		unsigned char iCharacter = (unsigned char)Host.Data[i];

		if ( (iCharacter <= 0x20u) || (iCharacter == 0x7fu) ||
			(iCharacter == (unsigned char)',') ||
			(iCharacter == (unsigned char)'!') ||
			(iCharacter == (unsigned char)'*') ||
			(iCharacter == (unsigned char)'?') ||
			(iCharacter == (unsigned char)'[') ||
			(iCharacter == (unsigned char)']') ) {
			return false;
		}
	}
	do {
		arrReverse[iPortSize++] = (char)('0' + (iValue % 10u));
		iValue /= 10u;
	} while ( iValue != 0u );
	for ( i = 0u; i < iPortSize; ++i ) {
		pTarget->Port[i] = arrReverse[iPortSize - i - 1u];
	}
	pTarget->Host = Host;
	pTarget->PortSize = iPortSize;
	pTarget->Bracketed = iPort != XSSH_DEFAULT_PORT;
	pTarget->Size = pTarget->Bracketed ?
		Host.Size + iPortSize + 3u : Host.Size;
	return true;
}



/* 返回虚拟 target 的一个字节。 */
static inline unsigned char xsshKnownHostTargetAt(
	const xsshknownhosttarget* pTarget,
	size_t iIndex
)
{
	if ( !pTarget->Bracketed ) {
		return (unsigned char)pTarget->Host.Data[iIndex];
	}
	if ( iIndex == 0u ) {
		return (unsigned char)'[';
	}
	--iIndex;
	if ( iIndex < pTarget->Host.Size ) {
		return (unsigned char)pTarget->Host.Data[iIndex];
	}
	iIndex -= pTarget->Host.Size;
	if ( iIndex == 0u ) {
		return (unsigned char)']';
	}
	if ( iIndex == 1u ) {
		return (unsigned char)':';
	}
	return (unsigned char)pTarget->Port[iIndex - 2u];
}

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xssh/src/connection/ssh_connection_message_internal.h */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CONNECTION_MESSAGE)
#ifndef XSSH_CONNECTION_MESSAGE_INTERNAL_H
#define XSSH_CONNECTION_MESSAGE_INTERNAL_H




/* 预留并写入 global request 公共前缀。 */
static inline xsshcode xsshGlobalRequestWriteBegin(
	xsshwriter* pWriter,
	xstrview Name,
	bool bWantReply,
	size_t iFieldsSize,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pWork
)
{
	xbytesview NameBytes = {
		(const unsigned char*)Name.Data,
		Name.Size
	};
	xbytesview arrInputs[5];
	size_t iTotal;
	size_t i;
	xsshcode Code;

	if ( (pWork == NULL) || !xrtSshNameValid(Name) ||
		(iInputCount > 4u) ||
		((pInputs == NULL) && (iInputCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Name.Size > (SIZE_MAX - 6u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal = 6u + Name.Size;
	if ( iFieldsSize > (SIZE_MAX - iTotal) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal += iFieldsSize;
	arrInputs[0] = NameBytes;
	for ( i = 0u; i < iInputCount; ++i ) {
		arrInputs[i + 1u] = pInputs[i];
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		iInputCount + 1u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWork = *pWriter;
	if ( (xrtSshWriteByte(pWork, XSSH_MSG_GLOBAL_REQUEST) != XSSH_OK) ||
		(xrtSshWriteString(pWork, NameBytes) != XSSH_OK) ||
		(xrtSshWriteBool(pWork, bWantReply) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xssh/src/connection/ssh_channel_message_internal.h */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CHANNEL_MESSAGE)
#ifndef XSSH_CHANNEL_MESSAGE_INTERNAL_H
#define XSSH_CHANNEL_MESSAGE_INTERNAL_H




/* 将文本视图转换为不改变所有权的字节视图。 */
static inline xbytesview xsshChannelBytes(xstrview Text)
{
	return (xbytesview){ (const unsigned char*)Text.Data, Text.Size };
}



/* 将字节视图转换为不改变所有权的文本视图。 */
static inline xstrview xsshChannelText(xbytesview Value)
{
	return (xstrview){ (const char*)Value.Data, Value.Size };
}



/* 将 SSH string 的编码长度安全加入总长度。 */
static inline xsshcode xsshChannelAddString(
	xbytesview Value,
	size_t* pTotal
)
{
	if ( (pTotal == NULL) || !xrtMemRangeValid(Value.Data, Value.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Value.Size > UINT32_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( (*pTotal > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotal += 4u + Value.Size;
	return XSSH_OK;
}



/* 预留整条消息并复制 writer，供事务式构建使用。 */
static inline xsshcode xsshChannelPrepare(
	xsshwriter* pWriter,
	size_t iTotal,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pWork
)
{
	xsshcode Code;

	if ( pWork == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		pInputs,
		iInputCount
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWork = *pWriter;
	return XSSH_OK;
}



/* 预留并写入 channel open 公共前缀。 */
static inline xsshcode xsshChannelOpenWriteBegin(
	xsshwriter* pWriter,
	xstrview Type,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	size_t iFieldsSize,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pWork
)
{
	xbytesview TypeBytes = xsshChannelBytes(Type);
	xbytesview arrInputs[6];
	size_t iTotal = 13u;
	size_t i;
	xsshcode Code;

	if ( (pWork == NULL) || !xrtSshNameValid(Type) ||
		(iMaxPacket == 0u) || (iInputCount > 5u) ||
		((pInputs == NULL) && (iInputCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshChannelAddString(TypeBytes, &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iFieldsSize > (SIZE_MAX - iTotal) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal += iFieldsSize;
	arrInputs[0] = TypeBytes;
	for ( i = 0u; i < iInputCount; ++i ) {
		arrInputs[i + 1u] = pInputs[i];
	}
	Code = xsshChannelPrepare(
		pWriter,
		iTotal,
		arrInputs,
		iInputCount + 1u,
		pWork
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(pWork, XSSH_MSG_CHANNEL_OPEN) != XSSH_OK) ||
		(xrtSshWriteString(pWork, TypeBytes) != XSSH_OK) ||
		(xrtSshWriteU32(pWork, iSender) != XSSH_OK) ||
		(xrtSshWriteU32(pWork, iWindow) != XSSH_OK) ||
		(xrtSshWriteU32(pWork, iMaxPacket) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xssh/src/connection/ssh_channel_request_internal.h */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CHANNEL_REQUEST)
#ifndef XSSH_CHANNEL_REQUEST_INTERNAL_H
#define XSSH_CHANNEL_REQUEST_INTERNAL_H

#include <string.h>




/* 将文本视图转换为不改变所有权的字节视图。 */
static inline xbytesview xsshRequestBytes(xstrview Text)
{
	return (xbytesview){ (const unsigned char*)Text.Data, Text.Size };
}



/* 将字节视图转换为不改变所有权的文本视图。 */
static inline xstrview xsshRequestText(xbytesview Value)
{
	return (xstrview){ (const char*)Value.Data, Value.Size };
}



/* 比较 request 类型与编译期常量。 */
static inline bool xsshRequestTypeEqual(
	xstrview Type,
	const char* pExpected,
	size_t iExpected
)
{
	return (Type.Size == iExpected) &&
		(memcmp(Type.Data, pExpected, iExpected) == 0);
}



/* 将一个 SSH string 安全加入专用字段长度。 */
static inline xsshcode xsshRequestAddString(
	xbytesview Value,
	size_t* pSize
)
{
	if ( (pSize == NULL) || !xrtMemRangeValid(Value.Data, Value.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Value.Size > UINT32_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( (*pSize > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pSize += 4u + Value.Size;
	return XSSH_OK;
}



/* 预留完整 request 并写入公共前缀。 */
static inline xsshcode xsshRequestWriteBegin(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Type,
	bool bWantReply,
	size_t iFieldsSize,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pWork
)
{
	xbytesview TypeBytes = xsshRequestBytes(Type);
	xbytesview arrInputs[5];
	size_t iTotal;
	size_t i;
	xsshcode Code;

	if ( (pWork == NULL) || !xrtSshNameValid(Type) ||
		(iInputCount > 4u) ||
		((pInputs == NULL) && (iInputCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Type.Size > (SIZE_MAX - 10u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal = 10u + Type.Size;
	if ( iFieldsSize > (SIZE_MAX - iTotal) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal += iFieldsSize;
	arrInputs[0] = TypeBytes;
	for ( i = 0u; i < iInputCount; ++i ) {
		arrInputs[i + 1u] = pInputs[i];
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		iInputCount + 1u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWork = *pWriter;
	if ( (xrtSshWriteByte(
		pWork,
		XSSH_MSG_CHANNEL_REQUEST
	) != XSSH_OK) || (xrtSshWriteU32(
		pWork,
		iRecipient
	) != XSSH_OK) || (xrtSshWriteString(
		pWork,
		TypeBytes
	) != XSSH_OK) || (xrtSshWriteBool(
		pWork,
		bWantReply
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 校验 request 类型、回复标志和输出重叠后初始化字段 reader。 */
static inline xsshcode xsshRequestReadBegin(
	const xsshchannelrequest* pRequest,
	const char* pType,
	size_t iTypeSize,
	int iWantReply,
	void* pOutput,
	size_t iOutputSize,
	xsshreader* pReader
)
{
	if ( (pRequest == NULL) || (pType == NULL) || (pReader == NULL) ||
		((pOutput == NULL) && (iOutputSize != 0u)) ||
		!xrtSshNameValid(pRequest->Type) ||
		!xrtMemRangeValid(pRequest->Fields.Data, pRequest->Fields.Size) ||
		!xsshRequestTypeEqual(pRequest->Type, pType, iTypeSize) ||
		((iWantReply >= 0) &&
		 (pRequest->WantReply != (iWantReply != 0))) ||
		xrtMemRangesOverlap(
			pRequest->Fields.Data,
			pRequest->Fields.Size,
			pOutput,
			iOutputSize
		) || !xrtSshReaderInit(pReader, pRequest->Fields) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return XSSH_OK;
}



/* 确认专用字段 reader 已经严格消费到末尾。 */
static inline xsshcode xsshRequestReadEnd(const xsshreader* pReader)
{
	return xrtSshReaderRemaining(pReader) == 0u ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xssh/src/session/ssh_client_future_internal.h */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CLIENT_FUTURE)
#ifndef XRT_SSH_CLIENT_FUTURE_INTERNAL_H
#define XRT_SSH_CLIENT_FUTURE_INTERNAL_H




#if defined(XSSH_FEATURE_CLIENT_FUTURE)

/* 内部信号只在对应 SSH/TCP 事务提交后发布。 */
typedef enum xsshclientfuturesignal {
	XSSH_CLIENT_FUTURE_READY = 0,
	XSSH_CLIENT_FUTURE_DRAIN = 1,
	XSSH_CLIENT_FUTURE_CLOSE = 2,
	XSSH_CLIENT_FUTURE_CHANNEL = 3,
	XSSH_CLIENT_FUTURE_DATA = 4,
	XSSH_CLIENT_FUTURE_WRITABLE = 5,
	XSSH_CLIENT_FUTURE_GLOBAL = 6,
	XSSH_CLIENT_FUTURE_CHANNEL_REMOVED = 7
} xsshclientfuturesignal;



/* 通知只借用现有稳定对象和回调期错误，不借用输入 packet。 */
typedef struct xsshclientfuturenotice {
	xsshclientfuturesignal Signal;
	xsshchannel* Channel;
	const xsshclientchannelnotice* ChannelNotice;
	const xsshclientglobalnotice* GlobalNotice;
	const xerror* Error;
	xsshchanneliostream Stream;
	uint32 ChannelLocal;
	bool HasChannelLocal;
} xsshclientfuturenotice;



/* 发布一个已经提交的客户端条件变化。 */
void __xrtSshClientFutureNotify(
	xsshclient* pClient,
	const xsshclientfuturenotice* pNotice
);



/* 终结并分离客户端拥有的等待管理器。 */
void __xrtSshClientFutureClear(xsshclient* pClient);

#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/wire/ssh_wire.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_WIRE)
#include <string.h>




#if defined(XSSH_FEATURE_WIRE)

/* 校验只借用内存的 reader 状态。 */
static bool xsshReaderValid(const xsshreader* pReader)
{
	return (pReader != NULL) &&
		xrtMemRangeValid(pReader->Source.Data, pReader->Source.Size) &&
		(pReader->Position <= pReader->Source.Size);
}



/* 校验只借用内存的 writer 状态。 */
static bool xsshWriterValid(const xsshwriter* pWriter)
{
	return (pWriter != NULL) &&
		xrtMemRangeValid(pWriter->Data, pWriter->Capacity) &&
		(pWriter->Size <= pWriter->Capacity);
}



/* 将 uint32 写入已经验证过的四字节目标。 */
static void xsshStoreU32(bytes pOutput, uint32 iValue)
{
	pOutput[0] = (uint8)((iValue >> 24u) & 0xffu);
	pOutput[1] = (uint8)((iValue >> 16u) & 0xffu);
	pOutput[2] = (uint8)((iValue >> 8u) & 0xffu);
	pOutput[3] = (uint8)(iValue & 0xffu);
}



/* 校验单个 SSH 名称，不接受分隔符或非 ASCII 字节。 */
bool xrtSshNameValid(xstrview Name)
{
	size_t i;

	if ( (Name.Size == 0u) || !xrtMemRangeValid(Name.Data, Name.Size) ) {
		return false;
	}
	for ( i = 0u; i < Name.Size; ++i ) {
		uint8 iByte = (uint8)Name.Data[i];

		if ( (iByte < 33u) || (iByte > 126u) || (iByte == (uint8)',') ) {
			return false;
		}
	}
	return true;
}



/* 校验允许为空且不含控制字符的 ASCII language tag。 */
bool xrtSshLanguageValid(xstrview Language)
{
	size_t i;

	if ( !xrtMemRangeValid(Language.Data, Language.Size) ) {
		return false;
	}
	for ( i = 0u; i < Language.Size; ++i ) {
		uint8 iByte = (uint8)Language.Data[i];

		if ( (iByte < 33u) || (iByte > 126u) ) {
			return false;
		}
	}
	return true;
}



/* 比较一个 name-list 项与独立名称。 */
static bool xsshNameEqual(
	xstrview List,
	size_t iStart,
	size_t iEnd,
	xstrview Name
)
{
	return ((iEnd - iStart) == Name.Size) &&
		(memcmp(List.Data + iStart, Name.Data, Name.Size) == 0);
}



/* 校验非负 mpint 的规范二进制补码形式。 */
static bool xsshPositiveMpintCanonical(xbytesview Value)
{
	if ( (Value.Data == NULL) && (Value.Size != 0u) ) {
		return false;
	}
	if ( Value.Size == 0u ) {
		return true;
	}
	if ( (Value.Data[0] & 0x80u) != 0u ) {
		return false;
	}
	if ( Value.Data[0] == 0u ) {
		return (Value.Size > 1u) && ((Value.Data[1] & 0x80u) != 0u);
	}
	return true;
}



/* 校验任意符号 mpint 的最短二进制补码形式。 */
static bool xsshSignedMpintCanonical(xbytesview Value)
{
	if ( (Value.Data == NULL) && (Value.Size != 0u) ) {
		return false;
	}
	if ( Value.Size == 0u ) {
		return true;
	}
	if ( (Value.Size == 1u) && (Value.Data[0] == 0u) ) {
		return false;
	}
	if ( (Value.Size > 1u) && (Value.Data[0] == 0u) &&
		((Value.Data[1] & 0x80u) == 0u) ) {
		return false;
	}
	if ( (Value.Size > 1u) && (Value.Data[0] == 0xffu) &&
		((Value.Data[1] & 0x80u) != 0u) ) {
		return false;
	}
	return true;
}



/* 初始化借用输入的 SSH reader。 */
bool xrtSshReaderInit(xsshreader* pReader, xbytesview Source)
{
	if ( (pReader == NULL) ||
		!xrtMemRangeValid(Source.Data, Source.Size) ) {
		return false;
	}
	pReader->Source = Source;
	pReader->Position = 0u;
	return true;
}



/* 初始化借用输出缓冲的 SSH writer。 */
bool xrtSshWriterInit(xsshwriter* pWriter, void* pData, size_t iCapacity)
{
	if ( (pWriter == NULL) || !xrtMemRangeValid(pData, iCapacity) ) {
		return false;
	}
	pWriter->Data = (bytes)pData;
	pWriter->Capacity = iCapacity;
	pWriter->Size = 0u;
	return true;
}



/* 返回 reader 中仍可读取的字节数。 */
size_t xrtSshReaderRemaining(const xsshreader* pReader)
{
	if ( !xsshReaderValid(pReader) ) {
		return 0u;
	}
	return pReader->Source.Size - pReader->Position;
}



/* 返回 writer 中仍可写入的字节数。 */
size_t xrtSshWriterRemaining(const xsshwriter* pWriter)
{
	if ( !xsshWriterValid(pWriter) ) {
		return 0u;
	}
	return pWriter->Capacity - pWriter->Size;
}



/* 校验 writer 状态并确认本次写入有完整空间。 */
xsshcode xrtSshWriterReserve(const xsshwriter* pWriter, size_t iSize)
{
	if ( !xsshWriterValid(pWriter) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iSize > xrtSshWriterRemaining(pWriter) ) {
		return XSSH_ERROR_SPACE;
	}
	return XSSH_OK;
}



/* 校验完整输出范围与输入视图及描述数组不重叠。 */
xsshcode xrtSshWriterReserveInputs(
	const xsshwriter* pWriter,
	size_t iSize,
	const xbytesview* pInputs,
	size_t iInputCount
)
{
	const void* pOutput;
	size_t iInputBytes;
	size_t i;
	xsshcode Code;

	if ( (pInputs == NULL) && (iInputCount != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iInputCount > (SIZE_MAX / sizeof(*pInputs)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshWriterReserve(pWriter, iSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pOutput = pWriter->Data == NULL ? NULL :
		(const void*)(pWriter->Data + pWriter->Size);
	iInputBytes = iInputCount * sizeof(*pInputs);
	if ( xrtMemRangesOverlap(
		pOutput,
		iSize,
		pWriter,
		sizeof(*pWriter)
	) || xrtMemRangesOverlap(
		pOutput,
		iSize,
		pInputs,
		iInputBytes
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < iInputCount; ++i ) {
		if ( !xrtMemRangeValid(pInputs[i].Data, pInputs[i].Size) ||
			xrtMemRangesOverlap(
				pOutput,
				iSize,
				pInputs[i].Data,
				pInputs[i].Size
			) ) {
			return XSSH_ERROR_ARGUMENT;
		}
	}
	return XSSH_OK;
}



/* 读取一个 SSH byte。 */
xsshcode xrtSshReadByte(xsshreader* pReader, uint8* pValue)
{
	uint8 iValue;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtSshReaderRemaining(pReader) < 1u ) {
		return XSSH_NEED_MORE;
	}
	iValue = pReader->Source.Data[pReader->Position];
	pReader->Position++;
	*pValue = iValue;
	return XSSH_OK;
}



/* 读取零或非零编码的 SSH boolean。 */
xsshcode xrtSshReadBool(xsshreader* pReader, bool* pValue)
{
	uint8 iValue;
	xsshcode Code;

	if ( pValue == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(pReader, &iValue);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pValue = iValue != 0u;
	return XSSH_OK;
}



/* 读取一个网络字节序 uint32。 */
xsshcode xrtSshReadU32(xsshreader* pReader, uint32* pValue)
{
	cbytes pData;
	uint32 iValue;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtSshReaderRemaining(pReader) < 4u ) {
		return XSSH_NEED_MORE;
	}
	pData = pReader->Source.Data + pReader->Position;
	iValue = ((uint32)pData[0] << 24u) |
		((uint32)pData[1] << 16u) |
		((uint32)pData[2] << 8u) |
		(uint32)pData[3];
	pReader->Position += 4u;
	*pValue = iValue;
	return XSSH_OK;
}



/* 读取一个网络字节序 uint64。 */
xsshcode xrtSshReadU64(xsshreader* pReader, uint64* pValue)
{
	cbytes pData;
	uint64 iValue;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtSshReaderRemaining(pReader) < 8u ) {
		return XSSH_NEED_MORE;
	}
	pData = pReader->Source.Data + pReader->Position;
	iValue = ((uint64)pData[0] << 56u) |
		((uint64)pData[1] << 48u) |
		((uint64)pData[2] << 40u) |
		((uint64)pData[3] << 32u) |
		((uint64)pData[4] << 24u) |
		((uint64)pData[5] << 16u) |
		((uint64)pData[6] << 8u) |
		(uint64)pData[7];
	pReader->Position += 8u;
	*pValue = iValue;
	return XSSH_OK;
}



/* 读取指定数量的原始字节。 */
xsshcode xrtSshReadBytes(
	xsshreader* pReader,
	size_t iSize,
	xbytesview* pValue
)
{
	xbytesview Value;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtSshReaderRemaining(pReader) < iSize ) {
		return XSSH_NEED_MORE;
	}
	Value.Data = pReader->Source.Data == NULL ?
		NULL : pReader->Source.Data + pReader->Position;
	Value.Size = iSize;
	pReader->Position += iSize;
	*pValue = Value;
	return XSSH_OK;
}



/* 读取 uint32 长度前缀的 SSH string。 */
xsshcode xrtSshReadString(xsshreader* pReader, xbytesview* pValue)
{
	cbytes pData;
	size_t iSize;
	xbytesview Value;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtSshReaderRemaining(pReader) < 4u ) {
		return XSSH_NEED_MORE;
	}
	pData = pReader->Source.Data + pReader->Position;
	iSize = ((size_t)pData[0] << 24u) |
		((size_t)pData[1] << 16u) |
		((size_t)pData[2] << 8u) |
		(size_t)pData[3];
	if ( iSize > (xrtSshReaderRemaining(pReader) - 4u) ) {
		return XSSH_NEED_MORE;
	}
	Value.Data = pData + 4u;
	Value.Size = iSize;
	pReader->Position += 4u + iSize;
	*pValue = Value;
	return XSSH_OK;
}



/* 写入一个 SSH byte。 */
xsshcode xrtSshWriteByte(xsshwriter* pWriter, uint8 iValue)
{
	xsshcode Code = xrtSshWriterReserve(pWriter, 1u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	pWriter->Data[pWriter->Size] = iValue;
	pWriter->Size++;
	return XSSH_OK;
}



/* 写入规范的 SSH boolean。 */
xsshcode xrtSshWriteBool(xsshwriter* pWriter, bool bValue)
{
	return xrtSshWriteByte(pWriter, bValue ? 1u : 0u);
}



/* 以网络字节序写入 uint32。 */
xsshcode xrtSshWriteU32(xsshwriter* pWriter, uint32 iValue)
{
	xsshcode Code = xrtSshWriterReserve(pWriter, 4u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	xsshStoreU32(pWriter->Data + pWriter->Size, iValue);
	pWriter->Size += 4u;
	return XSSH_OK;
}



/* 以网络字节序写入 uint64。 */
xsshcode xrtSshWriteU64(xsshwriter* pWriter, uint64 iValue)
{
	bytes pOutput;
	xsshcode Code = xrtSshWriterReserve(pWriter, 8u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	pOutput = pWriter->Data + pWriter->Size;
	pOutput[0] = (uint8)((iValue >> 56u) & 0xffu);
	pOutput[1] = (uint8)((iValue >> 48u) & 0xffu);
	pOutput[2] = (uint8)((iValue >> 40u) & 0xffu);
	pOutput[3] = (uint8)((iValue >> 32u) & 0xffu);
	pOutput[4] = (uint8)((iValue >> 24u) & 0xffu);
	pOutput[5] = (uint8)((iValue >> 16u) & 0xffu);
	pOutput[6] = (uint8)((iValue >> 8u) & 0xffu);
	pOutput[7] = (uint8)(iValue & 0xffu);
	pWriter->Size += 8u;
	return XSSH_OK;
}



/* 不添加长度前缀，直接写入原始字节。 */
xsshcode xrtSshWriteBytes(xsshwriter* pWriter, xbytesview Value)
{
	xsshcode Code;

	if ( (Value.Data == NULL) && (Value.Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshWriterReserve(pWriter, Value.Size);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Value.Size != 0u ) {
		memcpy(pWriter->Data + pWriter->Size, Value.Data, Value.Size);
	}
	pWriter->Size += Value.Size;
	return XSSH_OK;
}



/* 写入 uint32 长度前缀的 SSH string。 */
xsshcode xrtSshWriteString(xsshwriter* pWriter, xbytesview Value)
{
	size_t iStart;
	xsshcode Code;

	if ( (Value.Data == NULL) && (Value.Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (Value.Size > UINT32_MAX) || (Value.Size > (SIZE_MAX - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshWriterReserve(pWriter, 4u + Value.Size);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iStart = pWriter->Size;
	xsshStoreU32(pWriter->Data + iStart, (uint32)Value.Size);
	if ( Value.Size != 0u ) {
		memcpy(pWriter->Data + iStart + 4u, Value.Data, Value.Size);
	}
	pWriter->Size = iStart + 4u + Value.Size;
	return XSSH_OK;
}



/* 校验并写入 SSH name-list string。 */
xsshcode xrtSshWriteNameList(xsshwriter* pWriter, xstrview List)
{
	xbytesview Value;

	if ( !xrtSshNameListValid(List) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Value.Data = (cbytes)List.Data;
	Value.Size = List.Size;
	return xrtSshWriteString(pWriter, Value);
}



/* 校验 SSH name-list。 */
bool xrtSshNameListValid(xstrview List)
{
	size_t iStart = 0u;
	size_t i;

	if ( (List.Data == NULL) && (List.Size != 0u) ) {
		return false;
	}
	if ( List.Size == 0u ) {
		return true;
	}
	for ( i = 0u; i <= List.Size; ++i ) {
		if ( (i == List.Size) || (List.Data[i] == ',') ) {
			xstrview Name;

			Name.Data = List.Data + iStart;
			Name.Size = i - iStart;
			if ( !xrtSshNameValid(Name) ) {
				return false;
			}
			iStart = i + 1u;
		}
	}
	return true;
}



/* 判断 SSH name-list 是否包含完整名称。 */
bool xrtSshNameListContains(xstrview List, xstrview Name)
{
	size_t iStart = 0u;
	size_t i;

	if ( !xrtSshNameListValid(List) || !xrtSshNameValid(Name) ) {
		return false;
	}
	for ( i = 0u; i <= List.Size; ++i ) {
		if ( (i == List.Size) || (List.Data[i] == ',') ) {
			if ( xsshNameEqual(List, iStart, i, Name) ) {
				return true;
			}
			iStart = i + 1u;
		}
	}
	return false;
}



/* 判断 SSH name-list 是否存在重复项。 */
bool xrtSshNameListHasDuplicate(xstrview List)
{
	size_t iStart = 0u;
	size_t i;

	if ( !xrtSshNameListValid(List) ) {
		return false;
	}
	for ( i = 0u; i <= List.Size; ++i ) {
		if ( (i == List.Size) || (List.Data[i] == ',') ) {
			xstrview Name;
			size_t jStart = i + 1u;
			size_t j;

			Name.Data = List.Data + iStart;
			Name.Size = i - iStart;
			for ( j = i + 1u; j <= List.Size; ++j ) {
				if ( (j == List.Size) || (List.Data[j] == ',') ) {
					if ( xsshNameEqual(List, jStart, j, Name) ) {
						return true;
					}
					jStart = j + 1u;
				}
			}
			iStart = i + 1u;
		}
	}
	return false;
}



/* 按首选顺序选择双方共同名称。 */
xsshcode xrtSshNameListFirstMatch(
	xstrview Preferred,
	xstrview Available,
	xstrview* pMatch
)
{
	size_t iStart = 0u;
	size_t i;

	if ( (pMatch == NULL) || !xrtSshNameListValid(Preferred) ||
		!xrtSshNameListValid(Available) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i <= Preferred.Size; ++i ) {
		if ( (i == Preferred.Size) || (Preferred.Data[i] == ',') ) {
			xstrview Name;

			Name.Data = Preferred.Data + iStart;
			Name.Size = i - iStart;
			if ( xrtSshNameListContains(Available, Name) ) {
				*pMatch = Name;
				return XSSH_OK;
			}
			iStart = i + 1u;
		}
	}
	return XSSH_ERROR_UNSUPPORTED;
}



/* 读取规范的非负 SSH mpint。 */
xsshcode xrtSshReadMpint(xsshreader* pReader, xbytesview* pValue)
{
	size_t iPosition;
	xbytesview Value;
	xsshcode Code;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iPosition = pReader->Position;
	Code = xrtSshReadString(pReader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshPositiveMpintCanonical(Value) ) {
		pReader->Position = iPosition;
		return XSSH_ERROR_PROTOCOL;
	}
	*pValue = Value;
	return XSSH_OK;
}



/* 将大端 magnitude 规范化为非负 SSH mpint。 */
xsshcode xrtSshWriteMpint(xsshwriter* pWriter, xbytesview Magnitude)
{
	size_t iOffset = 0u;
	size_t iEncoded;
	size_t iStart;
	bool bPrefix;
	xsshcode Code;

	if ( (Magnitude.Data == NULL) && (Magnitude.Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	while ( (iOffset < Magnitude.Size) && (Magnitude.Data[iOffset] == 0u) ) {
		iOffset++;
	}
	if ( iOffset == Magnitude.Size ) {
		return xrtSshWriteU32(pWriter, 0u);
	}
	Magnitude.Data += iOffset;
	Magnitude.Size -= iOffset;
	bPrefix = (Magnitude.Data[0] & 0x80u) != 0u;
	if ( (Magnitude.Size > UINT32_MAX) ||
		(bPrefix && (Magnitude.Size == UINT32_MAX)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iEncoded = Magnitude.Size + (bPrefix ? 1u : 0u);
	if ( iEncoded > (SIZE_MAX - 4u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshWriterReserve(pWriter, 4u + iEncoded);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iStart = pWriter->Size;
	xsshStoreU32(pWriter->Data + iStart, (uint32)iEncoded);
	if ( bPrefix ) {
		pWriter->Data[iStart + 4u] = 0u;
	}
	memcpy(
		pWriter->Data + iStart + 4u + (bPrefix ? 1u : 0u),
		Magnitude.Data,
		Magnitude.Size
	);
	pWriter->Size = iStart + 4u + iEncoded;
	return XSSH_OK;
}



/* 读取规范的任意符号 SSH mpint。 */
xsshcode xrtSshReadSignedMpint(
	xsshreader* pReader,
	xbytesview* pValue,
	bool* pNegative
)
{
	size_t iPosition;
	xbytesview Value;
	bool bNegative;
	xsshcode Code;

	if ( !xsshReaderValid(pReader) || (pValue == NULL) ||
		(pNegative == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iPosition = pReader->Position;
	Code = xrtSshReadString(pReader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshSignedMpintCanonical(Value) ) {
		pReader->Position = iPosition;
		return XSSH_ERROR_PROTOCOL;
	}
	bNegative = (Value.Size != 0u) && ((Value.Data[0] & 0x80u) != 0u);
	*pValue = Value;
	*pNegative = bNegative;
	return XSSH_OK;
}



/* 将大端 magnitude 与符号规范化为 SSH mpint。 */
xsshcode xrtSshWriteSignedMpint(
	xsshwriter* pWriter,
	xbytesview Magnitude,
	bool bNegative
)
{
	size_t iOffset = 0u;
	size_t iEncoded;
	size_t iStart;
	size_t i;
	uint8 iCarryToTop = 1u;
	uint8 iTop;
	uint16 iCarry = 1u;
	bool bPrefix;
	bytes pOutput;
	xsshcode Code;

	if ( (Magnitude.Data == NULL) && (Magnitude.Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !bNegative ) {
		return xrtSshWriteMpint(pWriter, Magnitude);
	}
	while ( (iOffset < Magnitude.Size) && (Magnitude.Data[iOffset] == 0u) ) {
		iOffset++;
	}
	if ( iOffset == Magnitude.Size ) {
		return xrtSshWriteU32(pWriter, 0u);
	}
	Magnitude.Data += iOffset;
	Magnitude.Size -= iOffset;
	for ( i = 1u; i < Magnitude.Size; ++i ) {
		if ( Magnitude.Data[i] != 0u ) {
			iCarryToTop = 0u;
			break;
		}
	}
	iTop = (uint8)((uint8)~Magnitude.Data[0] + iCarryToTop);
	bPrefix = (iTop & 0x80u) == 0u;
	if ( (Magnitude.Size > UINT32_MAX) ||
		(bPrefix && (Magnitude.Size == UINT32_MAX)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iEncoded = Magnitude.Size + (bPrefix ? 1u : 0u);
	if ( iEncoded > (SIZE_MAX - 4u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshWriterReserve(pWriter, 4u + iEncoded);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iStart = pWriter->Size;
	pOutput = pWriter->Data + iStart + 4u;
	if ( bPrefix ) {
		*pOutput++ = 0xffu;
	}
	for ( i = Magnitude.Size; i > 0u; --i ) {
		uint16 iValue = (uint16)((uint8)~Magnitude.Data[i - 1u]) + iCarry;

		pOutput[i - 1u] = (uint8)(iValue & 0xffu);
		iCarry = (uint16)(iValue >> 8u);
	}
	xsshStoreU32(pWriter->Data + iStart, (uint32)iEncoded);
	pWriter->Size = iStart + 4u + iEncoded;
	return XSSH_OK;
}



/* 校验 protoversion 后的非空 softwareversion。 */
static bool xsshSoftwareVersionValid(
	const char* pData,
	size_t iSize,
	size_t iOffset
)
{
	size_t iStart = iOffset;

	while ( (iOffset < iSize) && (pData[iOffset] != ' ') ) {
		uint8 iByte = (uint8)pData[iOffset];

		if ( (iByte < 0x21u) || (iByte > 0x7eu) ||
			(iByte == (uint8)'-') ) {
			return false;
		}
		iOffset++;
	}
	return iOffset > iStart;
}



/* 从增量输入中查找并校验 SSH identification 行。 */
xsshcode xrtSshBannerRead(
	xstrview Data,
	xstrview* pBanner,
	size_t* pConsumed
)
{
	size_t iLineStart = 0u;
	size_t i;

	if ( (pBanner == NULL) || (pConsumed == NULL) ||
		((Data.Data == NULL) && (Data.Size != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < Data.Size; ++i ) {
		uint8 iByte = (uint8)Data.Data[i];

		if ( iByte == (uint8)'\r' ) {
			if ( (i + 1u) == Data.Size ) {
				break;
			}
			if ( Data.Data[i + 1u] != '\n' ) {
				return XSSH_ERROR_PROTOCOL;
			}
			continue;
		}
		if ( iByte == (uint8)'\n' ) {
			size_t iLineEnd = i;
			size_t iLineSize;
			size_t iPrefixSize;

			if ( (i + 1u - iLineStart) > XSSH_IDENTIFICATION_MAX ) {
				return XSSH_ERROR_OVERFLOW;
			}
			if ( (iLineEnd > iLineStart) &&
				(Data.Data[iLineEnd - 1u] == '\r') ) {
				iLineEnd--;
			}
			iLineSize = iLineEnd - iLineStart;
			if ( (iLineSize >= 4u) &&
				(memcmp(Data.Data + iLineStart, "SSH-", 4u) == 0) ) {
				bool bVersion2 = (iLineSize > 8u) &&
					(memcmp(Data.Data + iLineStart, "SSH-2.0-", 8u) == 0);
				bool bVersion199 = (iLineSize > 9u) &&
					(memcmp(Data.Data + iLineStart, "SSH-1.99-", 9u) == 0);

				if ( !bVersion2 && !bVersion199 ) {
					return (iLineSize > 8u) ?
						XSSH_ERROR_UNSUPPORTED : XSSH_ERROR_PROTOCOL;
				}
				iPrefixSize = bVersion2 ? 8u : 9u;
				if ( !xsshSoftwareVersionValid(
					Data.Data + iLineStart,
					iLineSize,
					iPrefixSize
				) ) {
					return XSSH_ERROR_PROTOCOL;
				}
				pBanner->Data = Data.Data + iLineStart;
				pBanner->Size = iLineSize;
				*pConsumed = i + 1u;
				return XSSH_OK;
			}
			iLineStart = i + 1u;
			continue;
		}
		if ( (iByte < 0x20u) || (iByte == 0x7fu) ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	if ( (Data.Size - iLineStart) >= XSSH_IDENTIFICATION_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	return XSSH_NEED_MORE;
}



/* 本端只发送 SSH-2.0，并严格校验 softwareversion 与整行长度。 */
xsshcode xrtSshBannerWrite(
	xsshwriter* pWriter,
	xstrview Banner
)
{
	xbytesview Input;
	xsshwriter Writer;
	size_t i;
	xsshcode Code;

	if ( !xrtMemRangeValid(Banner.Data, Banner.Size) ||
		(Banner.Size < 9u) ||
		(Banner.Size > (XSSH_IDENTIFICATION_MAX - 2u)) ||
		(memcmp(Banner.Data, "SSH-2.0-", 8u) != 0) ||
		!xsshSoftwareVersionValid(Banner.Data, Banner.Size, 8u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 8u; i < Banner.Size; ++i ) {
		uint8 iByte = (uint8)Banner.Data[i];

		if ( (iByte < 0x20u) || (iByte > 0x7eu) ) {
			return XSSH_ERROR_ARGUMENT;
		}
	}
	Input.Data = (const unsigned char*)Banner.Data;
	Input.Size = Banner.Size;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		Banner.Size + 2u,
		&Input,
		1u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteBytes(&Writer, Input) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, XRT_BYTES_LITERAL("\r\n")) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/packet/ssh_packet.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_PACKET)
#include <string.h>




#if defined(XSSH_FEATURE_PACKET)

/* 规范化可选块长并拒绝无法放入 padding_length 的值。 */
static xsshcode xsshPacketBlock(size_t iRequested, size_t* pBlockSize)
{
	if ( pBlockSize == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iRequested == 0u ) {
		*pBlockSize = XSSH_PACKET_BLOCK_MIN;
		return XSSH_OK;
	}
	if ( (iRequested < XSSH_PACKET_BLOCK_MIN) ||
		(iRequested > XSSH_PACKET_PADDING_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pBlockSize = iRequested;
	return XSSH_OK;
}



/* 计算 plain packet 的规范 padding 与线路长度。 */
xsshcode xrtSshPacketMeasure(
	size_t iPayloadSize,
	size_t iBlockSize,
	uint8* pPaddingSize,
	uint32* pPacketSize
)
{
	size_t iBaseSize;
	size_t iPaddingSize;
	size_t iPacketSize;
	xsshcode Code;

	if ( (pPaddingSize == NULL) || (pPacketSize == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPacketBlock(iBlockSize, &iBlockSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iPayloadSize > ((size_t)UINT32_MAX - 1u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( iPayloadSize > (SIZE_MAX - 5u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iBaseSize = 5u + iPayloadSize;
	iPaddingSize = iBaseSize % iBlockSize;
	iPaddingSize = iPaddingSize == 0u ? 0u : iBlockSize - iPaddingSize;
	while ( iPaddingSize < XSSH_PACKET_PADDING_MIN ) {
		if ( iPaddingSize > (XSSH_PACKET_PADDING_MAX - iBlockSize) ) {
			return XSSH_ERROR_OVERFLOW;
		}
		iPaddingSize += iBlockSize;
	}
	iPacketSize = 1u + iPayloadSize + iPaddingSize;
	if ( (iPaddingSize > XSSH_PACKET_PADDING_MAX) ||
		(iPacketSize > UINT32_MAX) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pPaddingSize = (uint8)iPaddingSize;
	*pPacketSize = (uint32)iPacketSize;
	return XSSH_OK;
}



/* 使用临时 padding 先完成所有可失败工作，再提交 packet。 */
xsshcode xrtSshPacketWrite(
	xsshwriter* pWriter,
	xbytesview Payload,
	size_t iBlockSize,
	uint32* pSequence,
	xsshpaddingproc pPadding,
	ptr pUserData
)
{
	unsigned char arrPadding[XSSH_PACKET_PADDING_MAX];
	xsshwriter Writer;
	xbytesview Padding;
	xbytesview Input;
	uint8 iPaddingSize;
	uint32 iPacketSize;
	size_t iTotalSize;
	xsshcode Code;

	if ( (pWriter == NULL) || (pPadding == NULL) ||
		((Payload.Data == NULL) && (Payload.Size != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPacketMeasure(
		Payload.Size,
		iBlockSize,
		&iPaddingSize,
		&iPacketSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iTotalSize = 4u + (size_t)iPacketSize;
	Input = Payload;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotalSize,
		&Input,
		1u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( !pPadding(arrPadding, iPaddingSize, pUserData) ) {
		return XSSH_ERROR_CALLBACK;
	}
	Padding.Data = arrPadding;
	Padding.Size = iPaddingSize;
	if ( (xrtSshWriteU32(&Writer, iPacketSize) != XSSH_OK) ||
		(xrtSshWriteByte(&Writer, iPaddingSize) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, Payload) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, Padding) != XSSH_OK) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pWriter = Writer;
	if ( pSequence != NULL ) {
		*pSequence = *pSequence + 1u;
	}
	return XSSH_OK;
}



/* 在 reader 副本中完成 packet 校验，成功后统一发布视图与序列。 */
xsshcode xrtSshPacketRead(
	xsshreader* pReader,
	size_t iBlockSize,
	uint32 iMaxPacketSize,
	uint32* pSequence,
	xsshpacketview* pPacket
)
{
	xsshreader Reader;
	xsshpacketview Packet;
	uint32 iPacketSize;
	size_t iPayloadSize;
	size_t iTotalSize;
	uint8 iPaddingSize;
	xsshcode Code;

	if ( (pReader == NULL) || (pPacket == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Reader = *pReader;
	Code = xsshPacketBlock(iBlockSize, &iBlockSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iPacketSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iPacketSize < (1u + XSSH_PACKET_PADDING_MIN) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( iMaxPacketSize == 0u ) {
		iMaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	}
	if ( iPacketSize > iMaxPacketSize ) {
		return XSSH_ERROR_OVERFLOW;
	}
	#if SIZE_MAX <= UINT32_MAX
		if ( (size_t)iPacketSize > (SIZE_MAX - 4u) ) {
			return XSSH_ERROR_OVERFLOW;
		}
	#endif
	iTotalSize = 4u + (size_t)iPacketSize;
	if ( (iTotalSize % iBlockSize) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( xrtSshReaderRemaining(&Reader) < (size_t)iPacketSize ) {
		return XSSH_NEED_MORE;
	}
	Code = xrtSshReadByte(&Reader, &iPaddingSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (iPaddingSize < XSSH_PACKET_PADDING_MIN) ||
		((uint32)iPaddingSize + 1u > iPacketSize) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	iPayloadSize = (size_t)iPacketSize - (size_t)iPaddingSize - 1u;
	Code = xrtSshReadBytes(&Reader, iPayloadSize, &Packet.Payload);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadBytes(&Reader, iPaddingSize, &Packet.Padding);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Packet.Sequence = pSequence == NULL ? 0u : *pSequence;
	Packet.PacketSize = iPacketSize;
	Packet.PaddingSize = iPaddingSize;
	*pReader = Reader;
	*pPacket = Packet;
	if ( pSequence != NULL ) {
		*pSequence = *pSequence + 1u;
	}
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/packet/ssh_packet_random.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_PACKET_RANDOM)



#if defined(XSSH_FEATURE_PACKET_RANDOM)

/* 把 XRT 的系统安全随机源适配为 SSH packet padding 回调。 */
bool xrtSshSecurePadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	(void)pUserData;
	return xrtSecureRandom(pOutput, iSize);
}



/* 为常用安全路径省去手工传递 padding 回调。 */
xsshcode xrtSshPacketWriteSecure(
	xsshwriter* pWriter,
	xbytesview Payload,
	size_t iBlockSize,
	uint32* pSequence
)
{
	return xrtSshPacketWrite(
		pWriter,
		Payload,
		iBlockSize,
		pSequence,
		xrtSshSecurePadding,
		NULL
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/packet/ssh_packet_aes_gcm.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_PACKET_AES_GCM)
#include <string.h>
#include <stdint.h>




#if defined(XSSH_FEATURE_PACKET_AES_GCM)

#define XSSH_AES_GCM_GUARD UINT32_C(0x53414743)



/* 读取大端 invocation counter。 */
static uint64 xsshAesGcmLoadU64(const uint8* pData)
{
	uint64 iValue = 0u;
	size_t i;

	for ( i = 0u; i < 8u; ++i ) {
		iValue = (iValue << 8u) | (uint64)pData[i];
	}
	return iValue;
}



/* 写入大端 invocation counter。 */
static void xsshAesGcmStoreU64(uint8* pData, uint64 iValue)
{
	size_t i;

	for ( i = 0u; i < 8u; ++i ) {
		pData[7u - i] = (uint8)(iValue & UINT64_C(0xff));
		iValue >>= 8u;
	}
}



/* 组合 OpenSSH AES-GCM 使用的 fixed || invocation nonce。 */
static void xsshAesGcmNonce(
	const xsshaesgcm* pState,
	uint8 pNonce[XSSH_AES_GCM_IV_SIZE]
)
{
	memcpy(pNonce, pState->FixedIV, XSSH_AES_GCM_FIXED_IV_SIZE);
	xsshAesGcmStoreU64(
		pNonce + XSSH_AES_GCM_FIXED_IV_SIZE,
		pState->Invocation
	);
}



/* 验证状态和 counter，避免 nonce 回绕后复用。 */
static bool xsshAesGcmCanUse(const xsshaesgcm* pState)
{
	return (pState != NULL) && (pState->Guard == XSSH_AES_GCM_GUARD) &&
		(pState->Invocation != UINT64_MAX) &&
		(xrtAesGcmTagSize(&pState->Cipher) == XSSH_AES_GCM_TAG_SIZE);
}



/* 计算 AES-GCM 明文体的十六字节对齐 padding。 */
xsshcode xrtSshAesGcmMeasure(
	size_t iPayloadSize,
	uint8* pPaddingSize,
	uint32* pPacketSize
)
{
	size_t iBaseSize;
	size_t iPaddingSize;
	size_t iPacketSize;

	if ( (pPaddingSize == NULL) || (pPacketSize == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (iPayloadSize > ((size_t)UINT32_MAX - 1u)) ||
		(iPayloadSize > (SIZE_MAX - 1u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iBaseSize = 1u + iPayloadSize;
	iPaddingSize = iBaseSize % XSSH_AES_GCM_BLOCK_SIZE;
	iPaddingSize = iPaddingSize == 0u ? 0u :
		XSSH_AES_GCM_BLOCK_SIZE - iPaddingSize;
	while ( iPaddingSize < XSSH_PACKET_PADDING_MIN ) {
		iPaddingSize += XSSH_AES_GCM_BLOCK_SIZE;
	}
	if ( iBaseSize > (SIZE_MAX - iPaddingSize) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iPacketSize = iBaseSize + iPaddingSize;
	if ( (iPaddingSize > XSSH_PACKET_PADDING_MAX) ||
		(iPacketSize > UINT32_MAX) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pPaddingSize = (uint8)iPaddingSize;
	*pPacketSize = (uint32)iPacketSize;
	return XSSH_OK;
}



/* 初始化单向状态，禁止没有可用 nonce 的最大初始 counter。 */
xsshcode xrtSshAesGcmInit(
	xsshaesgcm* pState,
	xbytesview Key,
	xbytesview InitialIV
)
{
	xsshaesgcm State;
	uint64 iInvocation;

	if ( (pState == NULL) || (Key.Data == NULL) ||
		((Key.Size != 16u) && (Key.Size != 32u)) ||
		(InitialIV.Data == NULL) ||
		(InitialIV.Size != XSSH_AES_GCM_IV_SIZE) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iInvocation = xsshAesGcmLoadU64(
		InitialIV.Data + XSSH_AES_GCM_FIXED_IV_SIZE
	);
	if ( iInvocation == UINT64_MAX ) {
		return XSSH_ERROR_STATE;
	}
	memset(&State, 0, sizeof(State));
	if ( !xrtAesGcmInit(
		&State.Cipher,
		Key.Data,
		Key.Size,
		XSSH_AES_GCM_TAG_SIZE
	) ) {
		xrtSecureZero(&State, sizeof(State));
		return XSSH_ERROR_ARGUMENT;
	}
	memcpy(
		State.FixedIV,
		InitialIV.Data,
		XSSH_AES_GCM_FIXED_IV_SIZE
	);
	State.Invocation = iInvocation;
	State.Guard = XSSH_AES_GCM_GUARD;
	xrtSecureZero(pState, sizeof(*pState));
	*pState = State;
	xrtSecureZero(&State, sizeof(State));
	return XSSH_OK;
}



/* 清除完整的 SSH AES-GCM 状态。 */
void xrtSshAesGcmClear(xsshaesgcm* pState)
{
	if ( pState == NULL ) {
		return;
	}
	xrtSecureZero(pState, sizeof(*pState));
}



/* 查询 counter 时保持无效调用的输出不变。 */
xsshcode xrtSshAesGcmInvocation(
	const xsshaesgcm* pState,
	uint64* pInvocation
)
{
	if ( (pInvocation == NULL) || (pState == NULL) ||
		(pState->Guard != XSSH_AES_GCM_GUARD) ||
		(xrtAesGcmTagSize(&pState->Cipher) != XSSH_AES_GCM_TAG_SIZE) ) {
		return XSSH_ERROR_STATE;
	}
	*pInvocation = pState->Invocation;
	return XSSH_OK;
}



/* 在目标缓冲区构建明文体，再用 XRT AES-GCM 原位加密。 */
xsshcode xrtSshAesGcmWrite(
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshaesgcm* pState,
	uint32* pSequence,
	xsshpaddingproc pPadding,
	ptr pUserData
)
{
	unsigned char arrPadding[XSSH_PACKET_PADDING_MAX];
	uint8 arrNonce[XSSH_AES_GCM_IV_SIZE];
	xsshwriter Writer;
	xbytesview arrInputs[2];
	bytes pBody;
	bytes pTag;
	uint8 iPaddingSize;
	uint32 iPacketSize;
	size_t iTotalSize;
	xsshcode Code;

	if ( (pWriter == NULL) || (pPadding == NULL) ||
		((Payload.Data == NULL) && (Payload.Size != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xsshAesGcmCanUse(pState) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshAesGcmMeasure(
		Payload.Size,
		&iPaddingSize,
		&iPacketSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iTotalSize = 4u + (size_t)iPacketSize + XSSH_AES_GCM_TAG_SIZE;
	arrInputs[0].Data = (const unsigned char*)pState;
	arrInputs[0].Size = sizeof(*pState);
	arrInputs[1] = Payload;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotalSize,
		arrInputs,
		2u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( !pPadding(arrPadding, iPaddingSize, pUserData) ) {
		xrtSecureZero(arrPadding, sizeof(arrPadding));
		return XSSH_ERROR_CALLBACK;
	}
	if ( xrtSshWriteU32(&Writer, iPacketSize) != XSSH_OK ) {
		xrtSecureZero(arrPadding, sizeof(arrPadding));
		return XSSH_ERROR_PROTOCOL;
	}
	pBody = Writer.Data + Writer.Size;
	pTag = pBody + iPacketSize;
	pBody[0] = iPaddingSize;
	if ( Payload.Size != 0u ) {
		memmove(pBody + 1u, Payload.Data, Payload.Size);
	}
	memcpy(pBody + 1u + Payload.Size, arrPadding, iPaddingSize);
	xrtSecureZero(arrPadding, sizeof(arrPadding));
	xsshAesGcmNonce(pState, arrNonce);
	if ( !xrtAesGcmEncrypt(
		&pState->Cipher,
		arrNonce,
		sizeof(arrNonce),
		Writer.Data + Writer.Size - 4u,
		4u,
		pBody,
		iPacketSize,
		pBody,
		pTag
	) ) {
		xrtSecureZero(arrNonce, sizeof(arrNonce));
		xrtSecureZero(pBody, iPacketSize);
		return XSSH_ERROR_STATE;
	}
	xrtSecureZero(arrNonce, sizeof(arrNonce));
	Writer.Size += (size_t)iPacketSize + XSSH_AES_GCM_TAG_SIZE;
	*pWriter = Writer;
	pState->Invocation++;
	if ( pSequence != NULL ) {
		*pSequence = *pSequence + 1u;
	}
	return XSSH_OK;
}



/* 先完成长度与容量校验，再认证、解密和发布 packet view。 */
xsshcode xrtSshAesGcmRead(
	xsshreader* pReader,
	xsshaesgcm* pState,
	uint32 iMaxPacketSize,
	uint32* pSequence,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity
)
{
	uint8 arrNonce[XSSH_AES_GCM_IV_SIZE];
	xsshreader Reader;
	xsshpacketview Packet;
	const uint8* pHeader;
	const uint8* pCipher;
	const uint8* pTag;
	bytes pBody = (bytes)pPlain;
	uint32 iPacketSize;
	uint8 iPaddingSize;
	size_t iPayloadSize;
	size_t iCipherAndTagSize;
	xsshcode Code;

	if ( (pReader == NULL) || (pPacket == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xsshAesGcmCanUse(pState) ) {
		return XSSH_ERROR_STATE;
	}
	Reader = *pReader;
	pHeader = Reader.Source.Data == NULL ? NULL :
		Reader.Source.Data + Reader.Position;
	Code = xrtSshReadU32(&Reader, &iPacketSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (iPacketSize < (1u + XSSH_PACKET_PADDING_MIN)) ||
		((iPacketSize % XSSH_AES_GCM_BLOCK_SIZE) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( iMaxPacketSize == 0u ) {
		iMaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	}
	if ( iPacketSize > iMaxPacketSize ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iCipherAndTagSize = (size_t)iPacketSize + XSSH_AES_GCM_TAG_SIZE;
	if ( xrtSshReaderRemaining(&Reader) < iCipherAndTagSize ) {
		return XSSH_NEED_MORE;
	}
	if ( (pPlain == NULL) || (iPlainCapacity < (size_t)iPacketSize) ) {
		return XSSH_ERROR_SPACE;
	}
	pCipher = Reader.Source.Data + Reader.Position;
	pTag = pCipher + iPacketSize;
	if ( xrtMemRangesOverlap(
		pPlain,
		iPacketSize,
		pHeader,
		4u + iCipherAndTagSize
	) || xrtMemRangesOverlap(
		pPlain,
		iPacketSize,
		pState,
		sizeof(*pState)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	xsshAesGcmNonce(pState, arrNonce);
	if ( !xrtAesGcmDecrypt(
		&pState->Cipher,
		arrNonce,
		sizeof(arrNonce),
		pHeader,
		4u,
		pCipher,
		iPacketSize,
		pTag,
		pPlain
	) ) {
		xrtSecureZero(arrNonce, sizeof(arrNonce));
		return XSSH_ERROR_AUTHENTICATION;
	}
	xrtSecureZero(arrNonce, sizeof(arrNonce));
	iPaddingSize = pBody[0];
	if ( (iPaddingSize < XSSH_PACKET_PADDING_MIN) ||
		((uint32)iPaddingSize + 1u > iPacketSize) ) {
		xrtSecureZero(pPlain, iPacketSize);
		return XSSH_ERROR_PROTOCOL;
	}
	iPayloadSize = (size_t)iPacketSize - (size_t)iPaddingSize - 1u;
	Packet.Sequence = pSequence == NULL ? 0u : *pSequence;
	Packet.PacketSize = iPacketSize;
	Packet.PaddingSize = iPaddingSize;
	Packet.Payload.Data = pBody + 1u;
	Packet.Payload.Size = iPayloadSize;
	Packet.Padding.Data = pBody + 1u + iPayloadSize;
	Packet.Padding.Size = iPaddingSize;
	Reader.Position += iCipherAndTagSize;
	*pReader = Reader;
	*pPacket = Packet;
	pState->Invocation++;
	if ( pSequence != NULL ) {
		*pSequence = *pSequence + 1u;
	}
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/packet/ssh_packet_codec.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_PACKET_CODEC)

#include <string.h>



#if defined(XSSH_FEATURE_PACKET_CODEC)

#define XSSH_PACKET_CODEC_GUARD UINT32_C(0x53504344)



/* 校验公开状态，避免伪造 mode 绕过 cipher 生命周期。 */
static bool xsshPacketCodecValid(const xsshpacketcodec* pCodec)
{
	if ( !xrtMemRangeValid(pCodec, sizeof(*pCodec)) ||
		(pCodec->Guard != XSSH_PACKET_CODEC_GUARD) ||
		(pCodec->MaxPacketSize < (1u + XSSH_PACKET_PADDING_MIN)) ||
		((pCodec->ReadMode != XSSH_PACKET_MODE_PLAIN) &&
		 (pCodec->ReadMode != XSSH_PACKET_MODE_AES_GCM)) ||
		((pCodec->WriteMode != XSSH_PACKET_MODE_PLAIN) &&
		 (pCodec->WriteMode != XSSH_PACKET_MODE_AES_GCM)) ) {
		return false;
	}
	return true;
}



/* 在目标状态保持有效时提交新 cipher，失败不会清除旧密钥。 */
static xsshcode xsshPacketCodecSetAesGcm(
	xsshaesgcm* pTarget,
	xsshpacketmode* pMode,
	xbytesview Key,
	xbytesview InitialIV
)
{
	xsshaesgcm State;
	xsshcode Code;

	memset(&State, 0, sizeof(State));
	Code = xrtSshAesGcmInit(&State, Key, InitialIV);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	xrtSshAesGcmClear(pTarget);
	*pTarget = State;
	*pMode = XSSH_PACKET_MODE_AES_GCM;
	xrtSecureZero(&State, sizeof(State));
	return XSSH_OK;
}



/* 预验 writer 的本次输出区不会覆盖 codec 状态。 */
static xsshcode xsshPacketCodecWriterCheck(
	const xsshpacketcodec* pCodec,
	const xsshwriter* pWriter,
	size_t iWireSize
)
{
	const unsigned char* pOutput;

	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pWriter->Data, pWriter->Capacity) ||
		(pWriter->Size > pWriter->Capacity) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtMemRangesOverlap(
		pWriter,
		sizeof(*pWriter),
		pCodec,
		sizeof(*pCodec)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iWireSize > (pWriter->Capacity - pWriter->Size) ) {
		return XSSH_ERROR_SPACE;
	}
	pOutput = pWriter->Data + pWriter->Size;
	if ( xrtMemRangesOverlap(
		pOutput,
		iWireSize,
		pCodec,
		sizeof(*pCodec)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return XSSH_OK;
}



/* 复用底层测量函数，统一 plain 与 AES-GCM 的线路尺寸口径。 */
static xsshcode xsshPacketCodecWriteNeed(
	const xsshpacketcodec* pCodec,
	size_t iPayloadSize,
	xsshpacketneed* pNeed
)
{
	xsshpacketneed Need;
	uint8 iPaddingSize;
	xsshcode Code;

	if ( pCodec->WriteMode == XSSH_PACKET_MODE_PLAIN ) {
		Code = xrtSshPacketMeasure(
			iPayloadSize,
			XSSH_PACKET_BLOCK_MIN,
			&iPaddingSize,
			&Need.PacketSize
		);
	} else {
		Code = xrtSshAesGcmMeasure(
			iPayloadSize,
			&iPaddingSize,
			&Need.PacketSize
		);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Need.PacketSize > pCodec->MaxPacketSize ) {
		return XSSH_ERROR_OVERFLOW;
	}
	#if SIZE_MAX <= UINT32_MAX
		if ( (size_t)Need.PacketSize > (SIZE_MAX - 4u -
			(pCodec->WriteMode == XSSH_PACKET_MODE_AES_GCM ?
			 XSSH_AES_GCM_TAG_SIZE : 0u)) ) {
			return XSSH_ERROR_OVERFLOW;
		}
	#endif
	Need.WireSize = 4u + (size_t)Need.PacketSize;
	Need.PlainSize = 0u;
	if ( pCodec->WriteMode == XSSH_PACKET_MODE_AES_GCM ) {
		Need.WireSize += XSSH_AES_GCM_TAG_SIZE;
	}
	*pNeed = Need;
	return XSSH_OK;
}



/* 初始化时明确写入默认上限，避免零值在后续被解释为不同策略。 */
xsshcode xrtSshPacketCodecInit(
	xsshpacketcodec* pCodec,
	uint32 iMaxPacketSize
)
{
	xsshpacketcodec Codec;

	if ( !xrtMemRangeValid(pCodec, sizeof(*pCodec)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iMaxPacketSize == 0u ) {
		iMaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	}
	if ( iMaxPacketSize < (1u + XSSH_PACKET_PADDING_MIN) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	memset(&Codec, 0, sizeof(Codec));
	Codec.MaxPacketSize = iMaxPacketSize;
	Codec.ReadMode = XSSH_PACKET_MODE_PLAIN;
	Codec.WriteMode = XSSH_PACKET_MODE_PLAIN;
	Codec.Guard = XSSH_PACKET_CODEC_GUARD;
	xrtSecureZero(pCodec, sizeof(*pCodec));
	*pCodec = Codec;
	xrtSecureZero(&Codec, sizeof(Codec));
	return XSSH_OK;
}



/* Cipher 状态始终按秘密材料处理，即使 codec 尚未初始化。 */
void xrtSshPacketCodecClear(xsshpacketcodec* pCodec)
{
	if ( pCodec == NULL ) {
		return;
	}
	xrtSecureZero(pCodec, sizeof(*pCodec));
}



/* 读取方向在 peer NEWKEYS 已认证后独立提交。 */
xsshcode xrtSshPacketCodecSetReadAesGcm(
	xsshpacketcodec* pCodec,
	xbytesview Key,
	xbytesview InitialIV
)
{
	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	return xsshPacketCodecSetAesGcm(
		&pCodec->ReadAesGcm,
		&pCodec->ReadMode,
		Key,
		InitialIV
	);
}



/* 写入方向在本端 NEWKEYS 已入队后独立提交。 */
xsshcode xrtSshPacketCodecSetWriteAesGcm(
	xsshpacketcodec* pCodec,
	xbytesview Key,
	xbytesview InitialIV
)
{
	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pCodec->WritePending ) {
		return XSSH_ERROR_STATE;
	}
	return xsshPacketCodecSetAesGcm(
		&pCodec->WriteAesGcm,
		&pCodec->WriteMode,
		Key,
		InitialIV
	);
}



/* strict-kex 只重置完成 NEWKEYS 的对应读取方向。 */
xsshcode xrtSshPacketCodecResetReadSequence(xsshpacketcodec* pCodec)
{
	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	pCodec->ReadSequence = 0u;
	return XSSH_OK;
}



/* strict-kex 只重置完成 NEWKEYS 的对应写入方向。 */
xsshcode xrtSshPacketCodecResetWriteSequence(xsshpacketcodec* pCodec)
{
	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pCodec->WritePending ) {
		return XSSH_ERROR_STATE;
	}
	pCodec->WriteSequence = 0u;
	return XSSH_OK;
}



/* 长度头保持明文，因此 plain 与 AES-GCM 都能先确定完整缓冲需求。 */
xsshcode xrtSshPacketCodecInspect(
	const xsshpacketcodec* pCodec,
	const xsshreader* pReader,
	xsshpacketneed* pNeed
)
{
	xsshpacketneed Need;
	xsshreader Reader;
	uint32 iPacketSize;
	xsshcode Code;

	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		!xrtMemRangeValid(pNeed, sizeof(*pNeed)) ||
		xrtMemRangesOverlap(
			pCodec,
			sizeof(*pCodec),
			pNeed,
			sizeof(*pNeed)
		) || xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pNeed,
			sizeof(*pNeed)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Reader = *pReader;
	Code = xrtSshReadU32(&Reader, &iPacketSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iPacketSize < (1u + XSSH_PACKET_PADDING_MIN) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( iPacketSize > pCodec->MaxPacketSize ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Need.PacketSize = iPacketSize;
	Need.PlainSize = 0u;
	#if SIZE_MAX <= UINT32_MAX
		if ( (size_t)iPacketSize > (SIZE_MAX - 4u -
			(pCodec->ReadMode == XSSH_PACKET_MODE_AES_GCM ?
			 XSSH_AES_GCM_TAG_SIZE : 0u)) ) {
			return XSSH_ERROR_OVERFLOW;
		}
	#endif
	Need.WireSize = 4u + (size_t)iPacketSize;
	if ( pCodec->ReadMode == XSSH_PACKET_MODE_PLAIN ) {
		if ( (Need.WireSize % XSSH_PACKET_BLOCK_MIN) != 0u ) {
			return XSSH_ERROR_PROTOCOL;
		}
	} else {
		if ( (iPacketSize % XSSH_AES_GCM_BLOCK_SIZE) != 0u ) {
			return XSSH_ERROR_PROTOCOL;
		}
		Need.PlainSize = (size_t)iPacketSize;
		Need.WireSize += XSSH_AES_GCM_TAG_SIZE;
	}
	if ( xrtMemRangesOverlap(
		pReader->Source.Data,
		pReader->Source.Size,
		pNeed,
		sizeof(*pNeed)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pNeed = Need;
	return XSSH_OK;
}



/* 尺寸探测不推进 sequence、nonce 或唯一写事务。 */
xsshcode xrtSshPacketCodecWriteMeasure(
	const xsshpacketcodec* pCodec,
	size_t iPayloadSize,
	xsshpacketneed* pNeed
)
{
	xsshpacketneed Need;
	xsshcode Code;

	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pNeed, sizeof(*pNeed)) ||
		xrtMemRangesOverlap(
			pCodec,
			sizeof(*pCodec),
			pNeed,
			sizeof(*pNeed)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPacketCodecWriteNeed(pCodec, iPayloadSize, &Need);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pNeed = Need;
	return XSSH_OK;
}



/*
	准备线路包后回退底层一次性 writer 推进的计数。
	WritePending 阻止同一 sequence/nonce 在提交或放弃前被再次使用。
*/
xsshcode xrtSshPacketCodecWritePrepareWithPadding(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData
)
{
	xsshpacketneed Need;
	xsshcode Code;

	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pCodec->WritePending ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(Payload.Data, Payload.Size) ||
		(pPadding == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPacketCodecWriteNeed(pCodec, Payload.Size, &Need);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshPacketCodecWriterCheck(
		pCodec,
		pWriter,
		Need.WireSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( pCodec->WriteMode == XSSH_PACKET_MODE_PLAIN ) {
		Code = xrtSshPacketWrite(
			pWriter,
			Payload,
			XSSH_PACKET_BLOCK_MIN,
			&pCodec->WriteSequence,
			pPadding,
			pUserData
		);
	} else {
		Code = xrtSshAesGcmWrite(
			pWriter,
			Payload,
			&pCodec->WriteAesGcm,
			&pCodec->WriteSequence,
			pPadding,
			pUserData
		);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pCodec->WriteSequence--;
	if ( pCodec->WriteMode == XSSH_PACKET_MODE_AES_GCM ) {
		pCodec->WriteAesGcm.Invocation--;
	}
	pCodec->WritePending = true;
	return XSSH_OK;
}



/* 只有可靠入队边界能够消费本次 sequence 与 nonce。 */
xsshcode xrtSshPacketCodecWriteCommit(xsshpacketcodec* pCodec)
{
	if ( !xsshPacketCodecValid(pCodec) || !pCodec->WritePending ) {
		return XSSH_ERROR_STATE;
	}
	pCodec->WriteSequence++;
	if ( pCodec->WriteMode == XSSH_PACKET_MODE_AES_GCM ) {
		pCodec->WriteAesGcm.Invocation++;
	}
	pCodec->WritePending = false;
	return XSSH_OK;
}



/* 未发送的线路包可以丢弃，不留下 sequence 或 nonce 缺口。 */
xsshcode xrtSshPacketCodecWriteAbort(xsshpacketcodec* pCodec)
{
	if ( !xsshPacketCodecValid(pCodec) || !pCodec->WritePending ) {
		return XSSH_ERROR_STATE;
	}
	pCodec->WritePending = false;
	return XSSH_OK;
}



/* 无背压便利路径仍复用同一事务边界。 */
xsshcode xrtSshPacketCodecWriteWithPadding(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData
)
{
	xsshcode Code = xrtSshPacketCodecWritePrepareWithPadding(
		pCodec,
		pWriter,
		Payload,
		pPadding,
		pUserData
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtSshPacketCodecWriteCommit(pCodec);
}



/* 解码成功后才推进 reader、序列号和 AEAD invocation。 */
xsshcode xrtSshPacketCodecRead(
	xsshpacketcodec* pCodec,
	xsshreader* pReader,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity
)
{
	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		xrtMemRangesOverlap(
			pCodec,
			sizeof(*pCodec),
			pReader,
			sizeof(*pReader)
		) || xrtMemRangesOverlap(
			pCodec,
			sizeof(*pCodec),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pReader->Source.Data,
			pReader->Source.Size,
			pCodec,
			sizeof(*pCodec)
		) || xrtMemRangesOverlap(
			pReader->Source.Data,
			pReader->Source.Size,
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pReader->Source.Data,
			pReader->Source.Size,
			pReader,
			sizeof(*pReader)
		) || xrtMemRangesOverlap(
			pPlain,
			iPlainCapacity,
			pPacket,
			sizeof(*pPacket)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pCodec->ReadMode == XSSH_PACKET_MODE_AES_GCM) &&
		(!xrtMemRangeValid(pPlain, iPlainCapacity) ||
		 xrtMemRangesOverlap(
			pPlain,
			iPlainCapacity,
			pCodec,
			sizeof(*pCodec)
		 ) || xrtMemRangesOverlap(
			pPlain,
			iPlainCapacity,
			pReader,
			sizeof(*pReader)
		 )) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pCodec->ReadMode == XSSH_PACKET_MODE_PLAIN ) {
		return xrtSshPacketRead(
			pReader,
			XSSH_PACKET_BLOCK_MIN,
			pCodec->MaxPacketSize,
			&pCodec->ReadSequence,
			pPacket
		);
	}
	return xrtSshAesGcmRead(
		pReader,
		&pCodec->ReadAesGcm,
		pCodec->MaxPacketSize,
		&pCodec->ReadSequence,
		pPacket,
		pPlain,
		iPlainCapacity
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/packet/ssh_packet_codec_random.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_PACKET_CODEC_RANDOM)



#if defined(XSSH_FEATURE_PACKET_CODEC_RANDOM)

/* 系统 CSPRNG 只负责填充准备阶段的 padding。 */
xsshcode xrtSshPacketCodecWritePrepare(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload
)
{
	return xrtSshPacketCodecWritePrepareWithPadding(
		pCodec,
		pWriter,
		Payload,
		xrtSshSecurePadding,
		NULL
	);
}



/* 默认生产路径只组合 codec 与 XRT 系统安全随机源。 */
xsshcode xrtSshPacketCodecWrite(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload
)
{
	return xrtSshPacketCodecWriteWithPadding(
		pCodec,
		pWriter,
		Payload,
		xrtSshSecurePadding,
		NULL
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_kexinit.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEXINIT)
#include <string.h>




#if defined(XSSH_FEATURE_KEXINIT)

/* 返回显式列表或模块默认列表。 */
static xstrview xsshKexList(
	xstrview Value,
	xstrview DefaultValue
)
{
	return (Value.Data == NULL) && (Value.Size == 0u) ?
		DefaultValue : Value;
}



/* 验证单个 name-list 并累加长度前缀。 */
static xsshcode xsshKexListSize(xstrview Value, size_t* pTotalSize)
{
	if ( (pTotalSize == NULL) || (Value.Size > UINT32_MAX) ||
		!xrtSshNameListValid(Value) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pTotalSize > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotalSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotalSize += 4u + Value.Size;
	return XSSH_OK;
}



/* 读取并严格验证一个 SSH name-list。 */
static xsshcode xsshKexReadList(
	xsshreader* pReader,
	xstrview* pList
)
{
	xbytesview Value;
	xsshcode Code;

	Code = xrtSshReadString(pReader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshNameListValid((xstrview){
		(const char*)Value.Data,
		Value.Size
	}) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pList->Data = (const char*)Value.Data;
	pList->Size = Value.Size;
	return XSSH_OK;
}



/* 返回 name-list 的第一项。 */
static xsshcode xsshKexFirst(
	xstrview List,
	xstrview* pFirst
)
{
	size_t i;

	if ( (pFirst == NULL) || !xrtSshNameListValid(List) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( List.Size == 0u ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	for ( i = 0u; i < List.Size; ++i ) {
		if ( List.Data[i] == ',' ) {
			break;
		}
	}
	pFirst->Data = List.Data;
	pFirst->Size = i;
	return XSSH_OK;
}



/* 比较两个文本视图。 */
static bool xsshKexEqual(xstrview Left, xstrview Right)
{
	if ( ((Left.Data == NULL) && (Left.Size != 0u)) ||
		((Right.Data == NULL) && (Right.Size != 0u)) ) {
		return false;
	}
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		(memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 判断 endpoint 角色是否属于公开枚举。 */
static bool xsshKexRoleValid(xsshrole Role)
{
	return (Role == XSSH_ROLE_CLIENT) || (Role == XSSH_ROLE_SERVER);
}



/* 判断名称是否只是 KEX 协商扩展标记，而不是真实算法。 */
static bool xsshKexIndicator(xstrview Name)
{
	return xsshKexEqual(Name, XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT)) ||
		xsshKexEqual(Name, XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_SERVER)) ||
		xsshKexEqual(Name, XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT)) ||
		xsshKexEqual(Name, XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER)) ||
		xsshKexEqual(
			Name,
			XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT_PRE_STANDARD)
		) || xsshKexEqual(
			Name,
			XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER_PRE_STANDARD)
		);
}



/* 返回 KEX 清单中第一项真实算法，跳过 RFC 8308/strict-kex 标记。 */
static xsshcode xsshKexFirstAlgorithm(
	xstrview List,
	xstrview* pFirst
)
{
	size_t iStart = 0u;
	size_t i;

	if ( (pFirst == NULL) || !xrtSshNameListValid(List) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( List.Size == 0u ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	for ( i = 0u; i <= List.Size; ++i ) {
		if ( (i == List.Size) || (List.Data[i] == ',') ) {
			xstrview Name = { List.Data + iStart, i - iStart };

			if ( !xsshKexIndicator(Name) ) {
				*pFirst = Name;
				return XSSH_OK;
			}
			iStart = i + 1u;
		}
	}
	return XSSH_ERROR_UNSUPPORTED;
}



/* 判断清单是否包含任一首次 KEX 专用标记。 */
static bool xsshKexHasIndicator(xstrview List)
{
	return xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT)
	) || xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_SERVER)
	) || xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT)
	) || xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER)
	) || xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT_PRE_STANDARD)
	) || xrtSshNameListContains(
		List,
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER_PRE_STANDARD)
	);
}



/* 返回角色和 KEX 阶段对应的默认算法清单。 */
static xstrview xsshKexDefaultAlgorithms(
	xsshrole Role,
	bool bInitial
)
{
	if ( !bInitial ) {
		return XRT_STR_LITERAL(XSSH_KEX_DEFAULT);
	}
	return Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT) :
		XRT_STR_LITERAL(XSSH_KEX_SERVER_INITIAL_DEFAULT);
}



/* 校验本端清单没有使用对端标记或在重协商中保留首次标记。 */
static bool xsshKexConfigIndicatorsValid(
	const xsshkexinitconfig* pConfig,
	xstrview Algorithms
)
{
	xstrview WrongExt = pConfig->Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_SERVER) :
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT);
	xstrview WrongStrict = pConfig->Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT);
	xstrview WrongStrictPreStandard = pConfig->Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER_PRE_STANDARD) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT_PRE_STANDARD);

	if ( xrtSshNameListContains(Algorithms, WrongExt) ||
		xrtSshNameListContains(Algorithms, WrongStrict) ||
		xrtSshNameListContains(Algorithms, WrongStrictPreStandard) ) {
		return false;
	}
	return pConfig->Initial || !xsshKexHasIndicator(Algorithms);
}



/* 初始化当前已经闭环实现的默认算法和现代扩展标记。 */
bool xrtSshKexInitConfigInit(
	xsshkexinitconfig* pConfig,
	xsshrole Role,
	bool bInitial
)
{
	xsshkexinitconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		!xsshKexRoleValid(Role) ) {
		return false;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Role = Role;
	Config.Initial = bInitial;
	Config.KexAlgorithms = xsshKexDefaultAlgorithms(Role, bInitial);
	Config.ServerHostKeyAlgorithms = XRT_STR_LITERAL(XSSH_HOSTKEY_DEFAULT);
	Config.EncryptionClientToServer = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Config.EncryptionServerToClient = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Config.MacClientToServer = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Config.MacServerToClient = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Config.CompressionClientToServer =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Config.CompressionServerToClient =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Config.LanguagesClientToServer = XRT_STR_LITERAL("");
	Config.LanguagesServerToClient = XRT_STR_LITERAL("");
	*pConfig = Config;
	return true;
}



/* 预先验证完整 payload，随后一次性提交 writer 状态。 */
xsshcode xrtSshKexInitWrite(
	xsshwriter* pWriter,
	xbytesview Cookie,
	const xsshkexinitconfig* pConfig
)
{
	xstrview arrLists[10];
	xbytesview arrInputs[12];
	xsshwriter Writer;
	size_t iTotalSize = 1u + XSSH_KEX_COOKIE_SIZE + 1u + 4u;
	size_t i;
	xsshcode Code;

	if ( (pWriter == NULL) || (Cookie.Data == NULL) ||
		(Cookie.Size != XSSH_KEX_COOKIE_SIZE) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		!xsshKexRoleValid(pConfig->Role) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	arrLists[0] = xsshKexList(
		pConfig->KexAlgorithms,
		xsshKexDefaultAlgorithms(pConfig->Role, pConfig->Initial)
	);
	arrLists[1] = xsshKexList(
		pConfig->ServerHostKeyAlgorithms,
		XRT_STR_LITERAL(XSSH_HOSTKEY_DEFAULT)
	);
	arrLists[2] = xsshKexList(
		pConfig->EncryptionClientToServer,
		XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT)
	);
	arrLists[3] = xsshKexList(
		pConfig->EncryptionServerToClient,
		XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT)
	);
	arrLists[4] = xsshKexList(
		pConfig->MacClientToServer,
		XRT_STR_LITERAL(XSSH_MAC_DEFAULT)
	);
	arrLists[5] = xsshKexList(
		pConfig->MacServerToClient,
		XRT_STR_LITERAL(XSSH_MAC_DEFAULT)
	);
	arrLists[6] = xsshKexList(
		pConfig->CompressionClientToServer,
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT)
	);
	arrLists[7] = xsshKexList(
		pConfig->CompressionServerToClient,
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT)
	);
	arrLists[8] = xsshKexList(
		pConfig->LanguagesClientToServer,
		XRT_STR_LITERAL("")
	);
	arrLists[9] = xsshKexList(
		pConfig->LanguagesServerToClient,
		XRT_STR_LITERAL("")
	);
	for ( i = 0u; i < 10u; ++i ) {
		Code = xsshKexListSize(arrLists[i], &iTotalSize);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( !xsshKexConfigIndicatorsValid(pConfig, arrLists[0]) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	arrInputs[0] = Cookie;
	arrInputs[1].Data = (const unsigned char*)pConfig;
	arrInputs[1].Size = sizeof(*pConfig);
	for ( i = 0u; i < 10u; ++i ) {
		arrInputs[i + 2u].Data = (const unsigned char*)arrLists[i].Data;
		arrInputs[i + 2u].Size = arrLists[i].Size;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotalSize,
		arrInputs,
		12u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_KEXINIT) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, Cookie) != XSSH_OK) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	for ( i = 0u; i < 10u; ++i ) {
		if ( xrtSshWriteString(
			&Writer,
			(xbytesview){
				(const unsigned char*)arrLists[i].Data,
				arrLists[i].Size
			}
		) != XSSH_OK ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	if ( (xrtSshWriteBool(
		&Writer,
		pConfig->FirstKexPacketFollows
	) != XSSH_OK) || (xrtSshWriteU32(&Writer, 0u) != XSSH_OK) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 在 reader 副本中完成全量解析，成功后发布所有借用视图。 */
xsshcode xrtSshKexInitRead(
	xbytesview Payload,
	xsshkexinit* pKexInit
)
{
	xsshreader Reader;
	xsshkexinit KexInit;
	uint8 iMessage;
	uint32 iReserved;
	xsshcode Code;

	if ( (pKexInit == NULL) ||
		((Payload.Data == NULL) && (Payload.Size != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	memset(&KexInit, 0, sizeof(KexInit));
	if ( !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_KEXINIT ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadBytes(
		&Reader,
		XSSH_KEX_COOKIE_SIZE,
		&KexInit.Cookie
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xsshKexReadList(&Reader, &KexInit.KexAlgorithms)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.ServerHostKeyAlgorithms
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.EncryptionClientToServer
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.EncryptionServerToClient
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(&Reader, &KexInit.MacClientToServer)) != XSSH_OK) ||
		((Code = xsshKexReadList(&Reader, &KexInit.MacServerToClient)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.CompressionClientToServer
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.CompressionServerToClient
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.LanguagesClientToServer
		)) != XSSH_OK) ||
		((Code = xsshKexReadList(
			&Reader,
			&KexInit.LanguagesServerToClient
		)) != XSSH_OK) ) {
		return Code;
	}
	Code = xrtSshReadBool(&Reader, &KexInit.FirstKexPacketFollows);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iReserved);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (iReserved != 0u) || (xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pKexInit = KexInit;
	return XSSH_OK;
}



/* 已知 AEAD cipher 不需要传统 MAC 选择。 */
bool xrtSshCipherIsAead(xstrview Cipher)
{
	return xsshKexEqual(
		Cipher,
		XRT_STR_LITERAL("aes128-gcm@openssh.com")
	) || xsshKexEqual(
		Cipher,
		XRT_STR_LITERAL("aes256-gcm@openssh.com")
	) || xsshKexEqual(
		Cipher,
		XRT_STR_LITERAL("chacha20-poly1305@openssh.com")
	);
}



/* 按客户端列表顺序协商；AEAD 方向保留空 MAC 视图。 */
xsshcode xrtSshKexNegotiate(
	const xsshkexinit* pClient,
	const xsshkexinit* pServer,
	xsshkexnegotiation* pNegotiation
)
{
	xsshkexnegotiation Negotiation;
	xsshcode Code;

	if ( (pClient == NULL) || (pServer == NULL) ||
		(pNegotiation == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	memset(&Negotiation, 0, sizeof(Negotiation));
	if ( ((Code = xrtSshNameListFirstMatch(
		pClient->KexAlgorithms,
		pServer->KexAlgorithms,
		&Negotiation.KexAlgorithm
	)) != XSSH_OK) || ((Code = xrtSshNameListFirstMatch(
		pClient->ServerHostKeyAlgorithms,
		pServer->ServerHostKeyAlgorithms,
		&Negotiation.ServerHostKeyAlgorithm
	)) != XSSH_OK) || ((Code = xrtSshNameListFirstMatch(
		pClient->EncryptionClientToServer,
		pServer->EncryptionClientToServer,
		&Negotiation.CipherClientToServer
	)) != XSSH_OK) || ((Code = xrtSshNameListFirstMatch(
		pClient->EncryptionServerToClient,
		pServer->EncryptionServerToClient,
		&Negotiation.CipherServerToClient
	)) != XSSH_OK) ) {
		return Code;
	}
	if ( xsshKexIndicator(Negotiation.KexAlgorithm) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( !xrtSshCipherIsAead(Negotiation.CipherClientToServer) ) {
		Code = xrtSshNameListFirstMatch(
			pClient->MacClientToServer,
			pServer->MacClientToServer,
			&Negotiation.MacClientToServer
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( !xrtSshCipherIsAead(Negotiation.CipherServerToClient) ) {
		Code = xrtSshNameListFirstMatch(
			pClient->MacServerToClient,
			pServer->MacServerToClient,
			&Negotiation.MacServerToClient
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( ((Code = xrtSshNameListFirstMatch(
		pClient->CompressionClientToServer,
		pServer->CompressionClientToServer,
		&Negotiation.CompressionClientToServer
	)) != XSSH_OK) || ((Code = xrtSshNameListFirstMatch(
		pClient->CompressionServerToClient,
		pServer->CompressionServerToClient,
		&Negotiation.CompressionServerToClient
	)) != XSSH_OK) ) {
		return Code;
	}
	*pNegotiation = Negotiation;
	return XSSH_OK;
}



/* 解析首次 KEX 的角色相关扩展标记。 */
xsshcode xrtSshKexFeatures(
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	xsshrole Role,
	bool bInitial,
	xsshkexfeatures* pFeatures
)
{
	xsshkexfeatures Features;
	xstrview LocalExt;
	xstrview PeerExt;
	xstrview LocalStrict;
	xstrview PeerStrict;
	xstrview LocalStrictPreStandard;
	xstrview PeerStrictPreStandard;

	if ( !xrtMemRangeValid(pLocal, sizeof(*pLocal)) ||
		!xrtMemRangeValid(pPeer, sizeof(*pPeer)) ||
		!xrtMemRangeValid(pFeatures, sizeof(*pFeatures)) ||
		!xsshKexRoleValid(Role) ||
		!xrtSshNameListValid(pLocal->KexAlgorithms) ||
		!xrtSshNameListValid(pPeer->KexAlgorithms) ||
		xrtMemRangesOverlap(
			pFeatures,
			sizeof(*pFeatures),
			pLocal,
			sizeof(*pLocal)
		) || xrtMemRangesOverlap(
			pFeatures,
			sizeof(*pFeatures),
			pPeer,
			sizeof(*pPeer)
		) || xrtMemRangesOverlap(
			pFeatures,
			sizeof(*pFeatures),
			pLocal->KexAlgorithms.Data,
			pLocal->KexAlgorithms.Size
		) || xrtMemRangesOverlap(
			pFeatures,
			sizeof(*pFeatures),
			pPeer->KexAlgorithms.Data,
			pPeer->KexAlgorithms.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	LocalExt = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT) :
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_SERVER);
	PeerExt = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_SERVER) :
		XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT);
	LocalStrict = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER);
	PeerStrict = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT);
	LocalStrictPreStandard = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT_PRE_STANDARD) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER_PRE_STANDARD);
	PeerStrictPreStandard = Role == XSSH_ROLE_CLIENT ?
		XRT_STR_LITERAL(XSSH_KEX_STRICT_SERVER_PRE_STANDARD) :
		XRT_STR_LITERAL(XSSH_KEX_STRICT_CLIENT_PRE_STANDARD);
	if ( bInitial ) {
		if ( xrtSshNameListContains(pLocal->KexAlgorithms, PeerExt) ||
			xrtSshNameListContains(pLocal->KexAlgorithms, PeerStrict) ||
			xrtSshNameListContains(
				pLocal->KexAlgorithms,
				PeerStrictPreStandard
			) ) {
			return XSSH_ERROR_ARGUMENT;
		}
		if ( xrtSshNameListContains(pPeer->KexAlgorithms, LocalExt) ||
			xrtSshNameListContains(pPeer->KexAlgorithms, LocalStrict) ||
			xrtSshNameListContains(
				pPeer->KexAlgorithms,
				LocalStrictPreStandard
			) ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	memset(&Features, 0, sizeof(Features));
	if ( bInitial ) {
		Features.AcceptExtInfo = xrtSshNameListContains(
			pLocal->KexAlgorithms,
			LocalExt
		);
		Features.SendExtInfo = xrtSshNameListContains(
			pPeer->KexAlgorithms,
			PeerExt
		);
		Features.Strict = (xrtSshNameListContains(
			pLocal->KexAlgorithms,
			LocalStrict
		) && xrtSshNameListContains(
			pPeer->KexAlgorithms,
			PeerStrict
		)) || (xrtSshNameListContains(
			pLocal->KexAlgorithms,
			LocalStrictPreStandard
		) && xrtSshNameListContains(
			pPeer->KexAlgorithms,
			PeerStrictPreStandard
		));
	}
	*pFeatures = Features;
	return XSSH_OK;
}



/* first_kex_packet_follows 只在 peer 前两项猜测均正确时保留下一包。 */
xsshcode xrtSshKexGuessSkip(
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	bool* pSkip
)
{
	xstrview FirstKex;
	xstrview FirstHostKey;
	xsshcode Code;

	if ( (pPeer == NULL) || (pNegotiation == NULL) || (pSkip == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !pPeer->FirstKexPacketFollows ) {
		*pSkip = false;
		return XSSH_OK;
	}
	Code = xsshKexFirstAlgorithm(pPeer->KexAlgorithms, &FirstKex);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshKexFirst(
		pPeer->ServerHostKeyAlgorithms,
		&FirstHostKey
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pSkip = !xsshKexEqual(FirstKex, pNegotiation->KexAlgorithm) ||
		!xsshKexEqual(
			FirstHostKey,
			pNegotiation->ServerHostKeyAlgorithm
		);
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_kexinit_random.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEXINIT_RANDOM)



#if defined(XSSH_FEATURE_KEXINIT_RANDOM)

/* 生成一次性 cookie，并在构建后清除栈副本。 */
xsshcode xrtSshKexInitWriteSecure(
	xsshwriter* pWriter,
	const xsshkexinitconfig* pConfig
)
{
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE];
	xsshcode Code;

	if ( (pWriter == NULL) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		((pConfig->Role != XSSH_ROLE_CLIENT) &&
		 (pConfig->Role != XSSH_ROLE_SERVER)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtSecureRandom(arrCookie, sizeof(arrCookie)) ) {
		return XSSH_ERROR_CALLBACK;
	}
	Code = xrtSshKexInitWrite(
		pWriter,
		(xbytesview){ arrCookie, sizeof(arrCookie) },
		pConfig
	);
	xrtSecureZero(arrCookie, sizeof(arrCookie));
	return Code;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_kex_ecdh.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEX_ECDH)



#if defined(XSSH_FEATURE_KEX_ECDH)

/* 验证一个可编码为 SSH string 的借用视图。 */
static bool xsshEcdhViewValid(xbytesview Value)
{
	return !((Value.Data == NULL) && (Value.Size != 0u)) &&
		(Value.Size <= UINT32_MAX);
}



/* 计算一组 SSH string 与消息号的总长度。 */
static xsshcode xsshEcdhMeasure(
	const xbytesview* pValues,
	size_t iCount,
	size_t* pTotalSize
)
{
	size_t iTotalSize = 1u;
	size_t i;

	if ( (pValues == NULL) || (pTotalSize == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xsshEcdhViewValid(pValues[i]) ) {
			return XSSH_ERROR_ARGUMENT;
		}
		if ( (iTotalSize > (SIZE_MAX - 4u)) ||
			(pValues[i].Size > (SIZE_MAX - iTotalSize - 4u)) ) {
			return XSSH_ERROR_OVERFLOW;
		}
		iTotalSize += 4u + pValues[i].Size;
	}
	*pTotalSize = iTotalSize;
	return XSSH_OK;
}



/* 在容量已验证的 writer 副本中写入一组 SSH string。 */
static xsshcode xsshEcdhWrite(
	xsshwriter* pWriter,
	uint8 iMessage,
	const xbytesview* pValues,
	size_t iCount
)
{
	xsshwriter Writer;
	size_t iTotalSize;
	size_t i;
	xsshcode Code;

	if ( pWriter == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshEcdhMeasure(pValues, iCount, &iTotalSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotalSize,
		pValues,
		iCount
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( xrtSshWriteByte(&Writer, iMessage) != XSSH_OK ) {
		return XSSH_ERROR_PROTOCOL;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( xrtSshWriteString(&Writer, pValues[i]) != XSSH_OK ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 初始化 reader 并验证消息号。 */
static xsshcode xsshEcdhReader(
	xbytesview Payload,
	uint8 iExpected,
	xsshreader* pReader
)
{
	uint8 iMessage;
	xsshcode Code;

	if ( (pReader == NULL) ||
		((Payload.Data == NULL) && (Payload.Size != 0u)) ||
		!xrtSshReaderInit(pReader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(pReader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return iMessage == iExpected ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 构建客户端临时公钥消息。 */
xsshcode xrtSshEcdhInitWrite(
	xsshwriter* pWriter,
	xbytesview ClientPublic
)
{
	return xsshEcdhWrite(
		pWriter,
		XSSH_MSG_KEX_ECDH_INIT,
		&ClientPublic,
		1u
	);
}



/* 解析客户端临时公钥，并拒绝尾随数据。 */
xsshcode xrtSshEcdhInitRead(
	xbytesview Payload,
	xsshecdhinit* pMessage
)
{
	xsshreader Reader;
	xsshecdhinit Message;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshEcdhReader(Payload, XSSH_MSG_KEX_ECDH_INIT, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Message.ClientPublic);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pMessage = Message;
	return XSSH_OK;
}



/* 构建服务端 host key、临时公钥和签名消息。 */
xsshcode xrtSshEcdhReplyWrite(
	xsshwriter* pWriter,
	xbytesview ServerHostKey,
	xbytesview ServerPublic,
	xbytesview Signature
)
{
	xbytesview arrValues[3];

	arrValues[0] = ServerHostKey;
	arrValues[1] = ServerPublic;
	arrValues[2] = Signature;
	return xsshEcdhWrite(
		pWriter,
		XSSH_MSG_KEX_ECDH_REPLY,
		arrValues,
		3u
	);
}



/* 解析服务端 ECDH reply，并拒绝尾随数据。 */
xsshcode xrtSshEcdhReplyRead(
	xbytesview Payload,
	xsshecdhreply* pMessage
)
{
	xsshreader Reader;
	xsshecdhreply Message;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshEcdhReader(Payload, XSSH_MSG_KEX_ECDH_REPLY, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadString(
		&Reader,
		&Message.ServerHostKey
	)) != XSSH_OK) || ((Code = xrtSshReadString(
		&Reader,
		&Message.ServerPublic
	)) != XSSH_OK) || ((Code = xrtSshReadString(
		&Reader,
		&Message.Signature
	)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pMessage = Message;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_kex_sha256.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEX_SHA256)
#include <string.h>
#include <stdint.h>




#if defined(XSSH_FEATURE_KEX_SHA256)

/* 验证可编码为 SSH string 的视图。 */
static bool xsshKexHashViewValid(xbytesview Value)
{
	return !((Value.Data == NULL) && (Value.Size != 0u)) &&
		(Value.Size <= UINT32_MAX);
}



/* 以大端序追加一个 uint32。 */
static bool xsshKexHashU32(xsha256* pState, uint32 iValue)
{
	uint8 arrValue[4];

	arrValue[0] = (uint8)(iValue >> 24u);
	arrValue[1] = (uint8)(iValue >> 16u);
	arrValue[2] = (uint8)(iValue >> 8u);
	arrValue[3] = (uint8)iValue;
	return xrtSha256Update(pState, arrValue, sizeof(arrValue));
}



/* 追加一个带 uint32 长度的 SSH string。 */
static bool xsshKexHashString(xsha256* pState, xbytesview Value)
{
	return xsshKexHashViewValid(Value) &&
		xsshKexHashU32(pState, (uint32)Value.Size) &&
		xrtSha256Update(pState, Value.Data, Value.Size);
}



/* 追加由大端 magnitude 规范化得到的非负 SSH mpint。 */
static bool xsshKexHashMpint(xsha256* pState, xbytesview Magnitude)
{
	uint8 iZero = 0u;
	size_t iOffset = 0u;
	bool bPrefix;

	if ( !xsshKexHashViewValid(Magnitude) ) {
		return false;
	}
	while ( (iOffset < Magnitude.Size) &&
		(Magnitude.Data[iOffset] == 0u) ) {
		iOffset++;
	}
	if ( iOffset == Magnitude.Size ) {
		return xsshKexHashU32(pState, 0u);
	}
	Magnitude.Data += iOffset;
	Magnitude.Size -= iOffset;
	bPrefix = (Magnitude.Data[0] & 0x80u) != 0u;
	if ( Magnitude.Size > (UINT32_MAX - (bPrefix ? 1u : 0u)) ) {
		return false;
	}
	if ( !xsshKexHashU32(
		pState,
		(uint32)Magnitude.Size + (bPrefix ? 1u : 0u)
	) ) {
		return false;
	}
	if ( bPrefix && !xrtSha256Update(pState, &iZero, 1u) ) {
		return false;
	}
	return xrtSha256Update(pState, Magnitude.Data, Magnitude.Size);
}



/* 判断共享秘密是否包含非零字节。 */
static bool xsshKexHashNonZero(xbytesview Value)
{
	uint8 iAny = 0u;
	size_t i;

	if ( !xsshKexHashViewValid(Value) || (Value.Size == 0u) ) {
		return false;
	}
	for ( i = 0u; i < Value.Size; ++i ) {
		iAny |= Value.Data[i];
	}
	return iAny != 0u;
}



/* 流式计算 exchange hash，避免构造历史实现中的中间缓冲。 */
xsshcode xrtSshKexHashSha256(
	const xsshkexhashsha256* pInput,
	void* pHash
)
{
	xsha256 State;
	bool bValid;

	if ( (pInput == NULL) || (pHash == NULL) ||
		!xsshKexHashNonZero(pInput->SharedSecret) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	xrtSha256Init(&State);
	bValid = xsshKexHashString(&State, pInput->ClientVersion) &&
		xsshKexHashString(&State, pInput->ServerVersion) &&
		xsshKexHashString(&State, pInput->ClientKexInit) &&
		xsshKexHashString(&State, pInput->ServerKexInit) &&
		xsshKexHashString(&State, pInput->ServerHostKey) &&
		xsshKexHashString(&State, pInput->ClientEphemeral) &&
		xsshKexHashString(&State, pInput->ServerEphemeral) &&
		xsshKexHashMpint(&State, pInput->SharedSecret) &&
		xrtSha256Final(&State, pHash);
	xrtSecureZero(&State, sizeof(State));
	return bValid ? XSSH_OK : XSSH_ERROR_ARGUMENT;
}



/* 逐轮扩展 K || H || X || session_id 或 K || H || 已生成材料。 */
xsshcode xrtSshKexDeriveSha256(
	void* pOutput,
	size_t iOutputSize,
	xbytesview SharedSecret,
	const void* pExchangeHash,
	const void* pSessionId,
	uint8 iLetter
)
{
	uint8 arrDigest[XSSH_SHA256_SIZE];
	xsha256 State;
	bytes pBytes = (bytes)pOutput;
	size_t iDone = 0u;

	if ( ((pOutput == NULL) && (iOutputSize != 0u)) ||
		(pExchangeHash == NULL) || (pSessionId == NULL) ||
		(iLetter < (uint8)'A') || (iLetter > (uint8)'F') ||
		!xsshKexHashNonZero(SharedSecret) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			SharedSecret.Data,
			SharedSecret.Size
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pExchangeHash,
			XSSH_SHA256_SIZE
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pSessionId,
			XSSH_SHA256_SIZE
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	while ( iDone < iOutputSize ) {
		size_t iCopy = iOutputSize - iDone;
		bool bValid;

		xrtSha256Init(&State);
		bValid = xsshKexHashMpint(&State, SharedSecret) &&
			xrtSha256Update(
				&State,
				pExchangeHash,
				XSSH_SHA256_SIZE
			);
		if ( iDone == 0u ) {
			bValid = bValid && xrtSha256Update(&State, &iLetter, 1u) &&
				xrtSha256Update(&State, pSessionId, XSSH_SHA256_SIZE);
		} else {
			bValid = bValid && xrtSha256Update(&State, pOutput, iDone);
		}
		bValid = bValid && xrtSha256Final(&State, arrDigest);
		xrtSecureZero(&State, sizeof(State));
		if ( !bValid ) {
			xrtSecureZero(arrDigest, sizeof(arrDigest));
			return XSSH_ERROR_STATE;
		}
		if ( iCopy > sizeof(arrDigest) ) {
			iCopy = sizeof(arrDigest);
		}
		memcpy(pBytes + iDone, arrDigest, iCopy);
		iDone += iCopy;
	}
	xrtSecureZero(arrDigest, sizeof(arrDigest));
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_kex_curve25519.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEX_CURVE25519)



#if defined(XSSH_FEATURE_KEX_CURVE25519)

/* 复用 XRT X25519 公钥导出，并映射为 SSH 稳定结果码。 */
xsshcode xrtSshCurve25519Public(
	const void* pPrivate,
	void* pPublic
)
{
	if ( (pPrivate == NULL) || (pPublic == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtX25519Public(pPrivate, pPublic) ?
		XSSH_OK : XSSH_ERROR_STATE;
}



/* XRT 已以常量时间完成全零共享秘密拒绝。 */
xsshcode xrtSshCurve25519Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
)
{
	if ( (pPrivate == NULL) || (pPeerPublic == NULL) ||
		(pShared == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtX25519Shared(pPrivate, pPeerPublic, pShared) ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_kex_curve25519_random.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM)



#if defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM)

/* 复用 XRT 的安全随机密钥对生成与输出原子性。 */
xsshcode xrtSshCurve25519KeyPair(
	void* pPrivate,
	void* pPublic
)
{
	if ( (pPrivate == NULL) || (pPublic == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtX25519KeyPair(pPrivate, pPublic) ?
		XSSH_OK : XSSH_ERROR_STATE;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_hostkey.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_HOSTKEY)

#include <string.h>



#if defined(XSSH_FEATURE_HOSTKEY)

/* 比较借用算法名与编译期文本。 */
static bool xsshHostKeyAlgorithmEqual(
	xstrview Algorithm,
	const char* sExpected,
	size_t iExpectedSize
)
{
	return (Algorithm.Size == iExpectedSize) &&
		(memcmp(Algorithm.Data, sExpected, iExpectedSize) == 0);
}



/* 读取并校验一个算法名 string。 */
static xsshcode xsshHostKeyReadAlgorithm(
	xsshreader* pReader,
	xstrview* pAlgorithm
)
{
	xbytesview Value;
	xstrview Algorithm;
	xsshcode Code;

	Code = xrtSshReadString(pReader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Algorithm.Data = (const char*)Value.Data;
	Algorithm.Size = Value.Size;
	if ( !xrtSshNameValid(Algorithm) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pAlgorithm = Algorithm;
	return XSSH_OK;
}



/* 验证 writer、容量和所有输入，随后返回未提交的输出区域。 */
static xsshcode xsshHostKeyWritePrepare(
	xsshwriter* pWriter,
	size_t iWriteSize,
	xbytesview First,
	xbytesview Second,
	xsshwriter* pCopy
)
{
	xsshwriter Writer;
	xbytesview arrInputs[2];
	xsshcode Code;

	if ( (pWriter == NULL) || (pCopy == NULL) ||
		((First.Data == NULL) && (First.Size != 0u)) ||
		((Second.Data == NULL) && (Second.Size != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	arrInputs[0] = First;
	arrInputs[1] = Second;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iWriteSize,
		arrInputs,
		2u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	*pCopy = Writer;
	return XSSH_OK;
}



/* 读取公钥的通用前缀，并把剩余算法参数保持为原始视图。 */
xsshcode xrtSshPublicKeyRead(
	xbytesview Blob,
	xsshpublickey* pPublicKey
)
{
	xsshreader Reader;
	xsshpublickey PublicKey;
	xsshcode Code;

	if ( (pPublicKey == NULL) ||
		!xrtSshReaderInit(&Reader, Blob) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshHostKeyReadAlgorithm(&Reader, &PublicKey.Algorithm);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadBytes(
		&Reader,
		xrtSshReaderRemaining(&Reader),
		&PublicKey.Parameters
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pPublicKey = PublicKey;
	return XSSH_OK;
}



/* 严格读取固定为两个 SSH string 的签名 blob。 */
xsshcode xrtSshSignatureRead(
	xbytesview Blob,
	xsshsignature* pSignature
)
{
	xsshreader Reader;
	xsshsignature Signature;
	xsshcode Code;

	if ( (pSignature == NULL) ||
		!xrtSshReaderInit(&Reader, Blob) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshHostKeyReadAlgorithm(&Reader, &Signature.Algorithm);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Signature.Signature);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pSignature = Signature;
	return XSSH_OK;
}



/* 预计算容量后一次提交通用签名 blob。 */
xsshcode xrtSshSignatureWrite(
	xsshwriter* pWriter,
	xstrview Algorithm,
	xbytesview Signature
)
{
	xsshwriter Writer;
	xbytesview AlgorithmBytes;
	size_t iWriteSize;
	xsshcode Code;

	if ( !xrtSshNameValid(Algorithm) ||
		((Signature.Data == NULL) && (Signature.Size != 0u)) ||
		(Signature.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (Algorithm.Size > (SIZE_MAX - 8u)) ||
		(Signature.Size > (SIZE_MAX - 8u - Algorithm.Size)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iWriteSize = 8u + Algorithm.Size + Signature.Size;
	AlgorithmBytes.Data = (const unsigned char*)Algorithm.Data;
	AlgorithmBytes.Size = Algorithm.Size;
	Code = xsshHostKeyWritePrepare(
		pWriter,
		iWriteSize,
		AlgorithmBytes,
		Signature,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, AlgorithmBytes) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Signature) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 在通用公钥前缀后严格解释 Ed25519 的单一 32 字节字段。 */
xsshcode xrtSshEd25519PublicKeyRead(
	xbytesview Blob,
	xbytesview* pPublicKey
)
{
	xsshpublickey PublicKey;
	xsshreader Reader;
	xbytesview Key;
	xsshcode Code;

	if ( pPublicKey == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPublicKeyRead(Blob, &PublicKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshHostKeyAlgorithmEqual(
		PublicKey.Algorithm,
		XSSH_HOSTKEY_ED25519,
		sizeof(XSSH_HOSTKEY_ED25519) - 1u
	) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( !xrtSshReaderInit(&Reader, PublicKey.Parameters) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshReadString(&Reader, &Key);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (Key.Size != XSSH_ED25519_PUBLIC_SIZE) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPublicKey = Key;
	return XSSH_OK;
}



/* 写入算法名与固定长度 Ed25519 公钥。 */
xsshcode xrtSshEd25519PublicKeyWrite(
	xsshwriter* pWriter,
	xbytesview PublicKey
)
{
	xsshwriter Writer;
	xbytesview Algorithm = XRT_BYTES_LITERAL(XSSH_HOSTKEY_ED25519);
	size_t iWriteSize = 8u + Algorithm.Size + XSSH_ED25519_PUBLIC_SIZE;
	xsshcode Code;

	if ( PublicKey.Size != XSSH_ED25519_PUBLIC_SIZE ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshHostKeyWritePrepare(
		pWriter,
		iWriteSize,
		Algorithm,
		PublicKey,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, Algorithm) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, PublicKey) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 在通用签名结构上收紧算法和固定签名长度。 */
xsshcode xrtSshEd25519SignatureRead(
	xbytesview Blob,
	xbytesview* pSignature
)
{
	xsshsignature Signature;
	xsshcode Code;

	if ( pSignature == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshSignatureRead(Blob, &Signature);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshHostKeyAlgorithmEqual(
		Signature.Algorithm,
		XSSH_HOSTKEY_ED25519,
		sizeof(XSSH_HOSTKEY_ED25519) - 1u
	) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( Signature.Signature.Size != XSSH_ED25519_SIGNATURE_SIZE ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pSignature = Signature.Signature;
	return XSSH_OK;
}



/* 复用通用签名 writer 构建 Ed25519 blob。 */
xsshcode xrtSshEd25519SignatureWrite(
	xsshwriter* pWriter,
	xbytesview Signature
)
{
	if ( Signature.Size != XSSH_ED25519_SIGNATURE_SIZE ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshSignatureWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
		Signature
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_hostkey_ed25519.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_HOSTKEY_ED25519)



#if defined(XSSH_FEATURE_HOSTKEY_ED25519)

/* 先完成 SSH 结构校验，再把可信边界交给 XRT Ed25519。 */
xsshcode xrtSshEd25519HostKeyVerify(
	xbytesview PublicKeyBlob,
	xbytesview SignatureBlob,
	xbytesview Message
)
{
	xbytesview PublicKey;
	xbytesview Signature;
	xsshcode Code;

	if ( (Message.Data == NULL) && (Message.Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshEd25519PublicKeyRead(PublicKeyBlob, &PublicKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshEd25519SignatureRead(SignatureBlob, &Signature);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtEd25519Verify(
		PublicKey.Data,
		Message.Data,
		Message.Size,
		Signature.Data
	) ? XSSH_OK : XSSH_ERROR_AUTHENTICATION;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/key/ssh_key_text.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEY_TEXT)





#if defined(XSSH_FEATURE_KEY_TEXT)

/* 判断结构输出是否覆盖任意借用字段。 */
static bool xsshKeyTextOutputOverlaps(
	const xsshopensshkeyline* pKeyLine,
	const void* pOutput,
	size_t iOutputSize
)
{
	return xrtMemRangesOverlap(
		pKeyLine->Options.Data,
		pKeyLine->Options.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pKeyLine->Algorithm.Data,
		pKeyLine->Algorithm.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pKeyLine->Base64.Data,
		pKeyLine->Base64.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pKeyLine->Comment.Data,
		pKeyLine->Comment.Size,
		pOutput,
		iOutputSize
	);
}



/* 校验调用方保存的公钥文本视图，并重新确认 Base64 容量。 */
static xsshcode xsshKeyTextLineValidate(
	const xsshopensshkeyline* pKeyLine
)
{
	size_t iBlobSize;

	if ( !xrtMemRangeValid(pKeyLine, sizeof(*pKeyLine)) ||
		!xrtMemRangeValid(pKeyLine->Options.Data, pKeyLine->Options.Size) ||
		!xrtMemRangeValid(pKeyLine->Algorithm.Data, pKeyLine->Algorithm.Size) ||
		!xrtMemRangeValid(pKeyLine->Base64.Data, pKeyLine->Base64.Size) ||
		!xrtMemRangeValid(pKeyLine->Comment.Data, pKeyLine->Comment.Size) ||
		!xrtSshNameValid(pKeyLine->Algorithm) ||
		!xsshKeyTextBase64Shape(pKeyLine->Base64) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtBase64Decode(
		pKeyLine->Base64.Data,
		pKeyLine->Base64.Size,
		NULL,
		0u,
		&iBlobSize,
		NULL
	) || (iBlobSize != pKeyLine->BlobSize) ||
		!xsshKeyTextBase64AlgorithmEqual(
			pKeyLine->Base64,
			iBlobSize,
			pKeyLine->Algorithm
		) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return XSSH_OK;
}



/* 解析字段边界，并用 XRT Base64 查询精确 blob 容量。 */
xsshcode xrtSshPublicKeyLineRead(
	xstrview Line,
	xsshopensshkeyline* pKeyLine
)
{
	xsshopensshkeyline KeyLine;
	xstrview Algorithm;
	xstrview Base64;
	size_t iStart;
	size_t iEnd;
	size_t iPosition;
	size_t iLook;
	size_t iOptionsEnd;
	bool bPresent;
	xsshcode Code;

	if ( !xrtMemRangeValid(pKeyLine, sizeof(*pKeyLine)) ||
		!xrtMemRangeValid(Line.Data, Line.Size) ||
		xrtMemRangesOverlap(
			Line.Data,
			Line.Size,
			pKeyLine,
			sizeof(*pKeyLine)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKeyTextBounds(Line, &iStart, &iEnd);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iPosition = iStart;
	while ( iPosition < iEnd ) {
		Code = xsshKeyTextToken(
			Line,
			iEnd,
			&iPosition,
			&Algorithm,
			&bPresent
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( !bPresent ) {
			break;
		}
		iLook = iPosition;
		Code = xsshKeyTextToken(
			Line,
			iEnd,
			&iLook,
			&Base64,
			&bPresent
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( xrtSshNameValid(Algorithm) && bPresent &&
			xsshKeyTextBase64Shape(Base64) ) {
			if ( !xrtBase64Decode(
				Base64.Data,
				Base64.Size,
				NULL,
				0u,
				&KeyLine.BlobSize,
				NULL
			) || !xsshKeyTextBase64AlgorithmEqual(
				Base64,
				KeyLine.BlobSize,
				Algorithm
			) ) {
				return XSSH_ERROR_PROTOCOL;
			}
			break;
		}
	}
	if ( (iPosition >= iEnd) || !bPresent ||
		!xrtSshNameValid(Algorithm) ||
		!xsshKeyTextBase64Shape(Base64) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	iOptionsEnd = (size_t)(Algorithm.Data - Line.Data);
	while ( (iOptionsEnd > iStart) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iOptionsEnd - 1u]
	) ) {
		--iOptionsEnd;
	}
	if ( iOptionsEnd == iStart ) {
		KeyLine.Options.Data = NULL;
		KeyLine.Options.Size = 0u;
	} else {
		KeyLine.Options.Data = Line.Data + iStart;
		KeyLine.Options.Size = iOptionsEnd - iStart;
	}
	KeyLine.Algorithm = Algorithm;
	KeyLine.Base64 = Base64;
	while ( (iLook < iEnd) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iLook]
	) ) {
		++iLook;
	}
	if ( iLook == iEnd ) {
		KeyLine.Comment.Data = NULL;
		KeyLine.Comment.Size = 0u;
	} else {
		KeyLine.Comment.Data = Line.Data + iLook;
		KeyLine.Comment.Size = iEnd - iLook;
	}
	*pKeyLine = KeyLine;
	return XSSH_OK;
}



/* 解码完整 blob 后，才发布算法无关公钥视图。 */
xsshcode xrtSshPublicKeyLineDecode(
	const xsshopensshkeyline* pKeyLine,
	void* pBlob,
	size_t iCapacity,
	xsshpublickey* pPublicKey
)
{
	xsshpublickey PublicKey;
	size_t iBlobSize;
	xsshcode Code;

	if ( !xrtMemRangeValid(pKeyLine, sizeof(*pKeyLine)) ||
		!xrtMemRangeValid(pPublicKey, sizeof(*pPublicKey)) ||
		!xrtMemRangeValid(pBlob, iCapacity) ||
		xsshKeyTextOutputOverlaps(
			pKeyLine,
			pPublicKey,
			sizeof(*pPublicKey)
		) || xrtMemRangesOverlap(
			pKeyLine,
			sizeof(*pKeyLine),
			pPublicKey,
			sizeof(*pPublicKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKeyTextLineValidate(pKeyLine);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iBlobSize = pKeyLine->BlobSize;
	if ( iCapacity < iBlobSize ) {
		return XSSH_ERROR_SPACE;
	}
	if ( xsshKeyTextOutputOverlaps(pKeyLine, pBlob, iBlobSize) ||
		xrtMemRangesOverlap(
			pKeyLine,
			sizeof(*pKeyLine),
			pBlob,
			iBlobSize
		) || xrtMemRangesOverlap(
			pBlob,
			iBlobSize,
			pPublicKey,
			sizeof(*pPublicKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtBase64Decode(
		pKeyLine->Base64.Data,
		pKeyLine->Base64.Size,
		pBlob,
		iCapacity,
		&iBlobSize,
		NULL
	) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshPublicKeyRead(
		(xbytesview){ (const unsigned char*)pBlob, iBlobSize },
		&PublicKey
	);
	if ( Code == XSSH_NEED_MORE ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshKeyTextEqual(PublicKey.Algorithm, pKeyLine->Algorithm) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPublicKey = PublicKey;
	return XSSH_OK;
}



/* 比较规范文本与原始 blob，避免 known_hosts 扫描期间反复分配工作区。 */
xsshcode xrtSshPublicKeyLineMatch(
	const xsshopensshkeyline* pKeyLine,
	xbytesview Blob,
	bool* pMatch
)
{
	xsshpublickey PublicKey;
	xsshcode Code;
	bool bMatch;

	if ( !xrtMemRangeValid(pKeyLine, sizeof(*pKeyLine)) ||
		!xrtMemRangeValid(Blob.Data, Blob.Size) ||
		!xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		xrtMemRangesOverlap(
			pKeyLine,
			sizeof(*pKeyLine),
			pMatch,
			sizeof(*pMatch)
		) || xrtMemRangesOverlap(
			Blob.Data,
			Blob.Size,
			pMatch,
			sizeof(*pMatch)
		) || xsshKeyTextOutputOverlaps(
			pKeyLine,
			pMatch,
			sizeof(*pMatch)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKeyTextLineValidate(pKeyLine);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshPublicKeyRead(Blob, &PublicKey);
	if ( Code == XSSH_NEED_MORE ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	bMatch = (Blob.Size == pKeyLine->BlobSize) &&
		xsshKeyTextEqual(PublicKey.Algorithm, pKeyLine->Algorithm) &&
		xsshKeyTextBase64Equal(pKeyLine->Base64, Blob);
	*pMatch = bMatch;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/key/ssh_known_host.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KNOWN_HOST)





#if defined(XSSH_FEATURE_KNOWN_HOST)

/* 比较 marker 与编译期名称。 */
static bool xsshKnownHostMarkerEqual(
	xstrview Marker,
	const char* sExpected,
	size_t iExpectedSize
)
{
	return (Marker.Size == iExpectedSize) &&
		(memcmp(Marker.Data, sExpected, iExpectedSize) == 0);
}



/* 校验明文 pattern-list 或单个 OpenSSH hashed host token。 */
static bool xsshKnownHostPatternsValid(
	xstrview Patterns,
	bool* pHashed
)
{
	size_t iStart;
	size_t i;

	if ( !xrtMemRangeValid(Patterns.Data, Patterns.Size) ||
		(Patterns.Size == 0u) || (pHashed == NULL) ) {
		return false;
	}
	if ( Patterns.Data[0] == '|' ) {
		for ( i = 0u; i < Patterns.Size; ++i ) {
			unsigned char iCharacter = (unsigned char)Patterns.Data[i];

			if ( (iCharacter <= 0x20u) || (iCharacter == 0x7fu) ||
				(iCharacter == (unsigned char)',') ||
				(iCharacter == (unsigned char)'!') ||
				(iCharacter == (unsigned char)'*') ||
				(iCharacter == (unsigned char)'?') ) {
				return false;
			}
		}
		*pHashed = true;
		return true;
	}
	iStart = 0u;
	for ( i = 0u; i <= Patterns.Size; ++i ) {
		if ( (i == Patterns.Size) || (Patterns.Data[i] == ',') ) {
			size_t iPattern = iStart;

			if ( iPattern == i ) {
				return false;
			}
			if ( Patterns.Data[iPattern] == '!' ) {
				++iPattern;
				if ( iPattern == i ) {
					return false;
				}
			}
			for ( ; iPattern < i; ++iPattern ) {
				unsigned char iCharacter =
					(unsigned char)Patterns.Data[iPattern];

				if ( (iCharacter <= 0x20u) || (iCharacter == 0x7fu) ||
					(iCharacter == (unsigned char)'|') ||
					(iCharacter == (unsigned char)'"') ||
					(iCharacter == (unsigned char)'\\') ) {
					return false;
				}
			}
			iStart = i + 1u;
		}
	}
	*pHashed = false;
	return true;
}



/* ASCII 主机比较大小写不敏感，其他字节保持原值。 */
static unsigned char xsshKnownHostFold(unsigned char iCharacter)
{
	if ( (iCharacter >= (unsigned char)'A') &&
		(iCharacter <= (unsigned char)'Z') ) {
		return (unsigned char)(iCharacter + ('a' - 'A'));
	}
	return iCharacter;
}



/* 用线性回退匹配 * 与 ?，不使用递归或临时主机字符串。 */
static bool xsshKnownHostPatternMatch(
	xstrview Pattern,
	const xsshknownhosttarget* pTarget
)
{
	size_t iPattern = 0u;
	size_t iTarget = 0u;
	size_t iStar = SIZE_MAX;
	size_t iRetry = 0u;

	while ( iTarget < pTarget->Size ) {
		if ( (iPattern < Pattern.Size) &&
			((Pattern.Data[iPattern] == '?') ||
			 (xsshKnownHostFold((unsigned char)Pattern.Data[iPattern]) ==
			  xsshKnownHostFold(xsshKnownHostTargetAt(
				  pTarget,
				  iTarget
			  )))) ) {
			++iPattern;
			++iTarget;
			continue;
		}
		if ( (iPattern < Pattern.Size) &&
			(Pattern.Data[iPattern] == '*') ) {
			iStar = iPattern++;
			iRetry = iTarget;
			continue;
		}
		if ( iStar != SIZE_MAX ) {
			iPattern = iStar + 1u;
			iTarget = ++iRetry;
			continue;
		}
		return false;
	}
	while ( (iPattern < Pattern.Size) &&
		(Pattern.Data[iPattern] == '*') ) {
		++iPattern;
	}
	return iPattern == Pattern.Size;
}



/* 解析 known_hosts 固定字段并保留未知 marker。 */
xsshcode xrtSshKnownHostLineRead(
	xstrview Line,
	xsshknownhostline* pKnownHost
)
{
	xsshknownhostline KnownHost;
	xstrview Token;
	size_t iStart;
	size_t iEnd;
	size_t iPosition;
	bool bPresent;
	xsshcode Code;

	if ( !xrtMemRangeValid(pKnownHost, sizeof(*pKnownHost)) ||
		!xrtMemRangeValid(Line.Data, Line.Size) ||
		xrtMemRangesOverlap(
			Line.Data,
			Line.Size,
			pKnownHost,
			sizeof(*pKnownHost)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKeyTextBounds(Line, &iStart, &iEnd);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iPosition = iStart;
	Code = xsshKeyTextToken(
		Line,
		iEnd,
		&iPosition,
		&Token,
		&bPresent
	);
	if ( (Code != XSSH_OK) || !bPresent ) {
		return Code != XSSH_OK ? Code : XSSH_ERROR_PROTOCOL;
	}
	KnownHost.Marker.Data = NULL;
	KnownHost.Marker.Size = 0u;
	KnownHost.MarkerKind = XSSH_KNOWN_HOST_MARKER_NONE;
	if ( Token.Data[0] == '@' ) {
		xstrview MarkerName = { Token.Data + 1u, Token.Size - 1u };

		if ( (Token.Size <= 1u) || !xrtSshNameValid(MarkerName) ) {
			return XSSH_ERROR_PROTOCOL;
		}
		KnownHost.Marker = Token;
		if ( xsshKnownHostMarkerEqual(
			Token,
			XSSH_KNOWN_HOST_CERT_AUTHORITY,
			sizeof(XSSH_KNOWN_HOST_CERT_AUTHORITY) - 1u
		) ) {
			KnownHost.MarkerKind = XSSH_KNOWN_HOST_MARKER_CERT_AUTHORITY;
		} else if ( xsshKnownHostMarkerEqual(
			Token,
			XSSH_KNOWN_HOST_REVOKED,
			sizeof(XSSH_KNOWN_HOST_REVOKED) - 1u
		) ) {
			KnownHost.MarkerKind = XSSH_KNOWN_HOST_MARKER_REVOKED;
		} else {
			KnownHost.MarkerKind = XSSH_KNOWN_HOST_MARKER_UNKNOWN;
		}
		Code = xsshKeyTextToken(
			Line,
			iEnd,
			&iPosition,
			&Token,
			&bPresent
		);
		if ( (Code != XSSH_OK) || !bPresent ) {
			return Code != XSSH_OK ? Code : XSSH_ERROR_PROTOCOL;
		}
	}
	KnownHost.Hosts = Token;
	if ( !xsshKnownHostPatternsValid(
		KnownHost.Hosts,
		&KnownHost.Hashed
	) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xsshKeyTextToken(
		Line,
		iEnd,
		&iPosition,
		&KnownHost.Algorithm,
		&bPresent
	);
	if ( (Code != XSSH_OK) || !bPresent ||
		!xrtSshNameValid(KnownHost.Algorithm) ) {
		return Code != XSSH_OK ? Code : XSSH_ERROR_PROTOCOL;
	}
	Code = xsshKeyTextToken(
		Line,
		iEnd,
		&iPosition,
		&KnownHost.Base64,
		&bPresent
	);
	if ( (Code != XSSH_OK) || !bPresent ||
		!xsshKeyTextBase64Shape(KnownHost.Base64) ||
		!xrtBase64Decode(
			KnownHost.Base64.Data,
			KnownHost.Base64.Size,
			NULL,
			0u,
			&KnownHost.BlobSize,
			NULL
		) || !xsshKeyTextBase64AlgorithmEqual(
			KnownHost.Base64,
			KnownHost.BlobSize,
			KnownHost.Algorithm
		) ) {
		return Code != XSSH_OK ? Code : XSSH_ERROR_PROTOCOL;
	}
	while ( (iPosition < iEnd) && xsshKeyTextSpace(
		(unsigned char)Line.Data[iPosition]
	) ) {
		++iPosition;
	}
	if ( iPosition == iEnd ) {
		KnownHost.Comment.Data = NULL;
		KnownHost.Comment.Size = 0u;
	} else {
		KnownHost.Comment.Data = Line.Data + iPosition;
		KnownHost.Comment.Size = iEnd - iPosition;
	}
	*pKnownHost = KnownHost;
	return XSSH_OK;
}



/* 复用 public-key 文本的 Base64 与算法一致性检查。 */
xsshcode xrtSshKnownHostLineDecode(
	const xsshknownhostline* pKnownHost,
	void* pBlob,
	size_t iCapacity,
	xsshpublickey* pPublicKey
)
{
	xsshopensshkeyline KeyLine;

	if ( !xrtMemRangeValid(pKnownHost, sizeof(*pKnownHost)) ||
		!xrtMemRangeValid(pBlob, iCapacity) ||
		!xrtMemRangeValid(pPublicKey, sizeof(*pPublicKey)) ||
		xrtMemRangesOverlap(
			pKnownHost,
			sizeof(*pKnownHost),
			pBlob,
			pKnownHost->BlobSize
		) || xrtMemRangesOverlap(
			pKnownHost,
			sizeof(*pKnownHost),
			pPublicKey,
			sizeof(*pPublicKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	KeyLine.Options.Data = NULL;
	KeyLine.Options.Size = 0u;
	KeyLine.Algorithm = pKnownHost->Algorithm;
	KeyLine.Base64 = pKnownHost->Base64;
	KeyLine.Comment = pKnownHost->Comment;
	KeyLine.BlobSize = pKnownHost->BlobSize;
	return xrtSshPublicKeyLineDecode(
		&KeyLine,
		pBlob,
		iCapacity,
		pPublicKey
	);
}



/* 把 known_hosts 字段映射为公共 key-text 视图并复用零分配比较。 */
xsshcode xrtSshKnownHostLineKeyMatch(
	const xsshknownhostline* pKnownHost,
	xbytesview Blob,
	bool* pMatch
)
{
	xsshopensshkeyline KeyLine;

	if ( !xrtMemRangeValid(pKnownHost, sizeof(*pKnownHost)) ||
		!xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		xrtMemRangesOverlap(
			pKnownHost,
			sizeof(*pKnownHost),
			pMatch,
			sizeof(*pMatch)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	KeyLine.Options.Data = NULL;
	KeyLine.Options.Size = 0u;
	KeyLine.Algorithm = pKnownHost->Algorithm;
	KeyLine.Base64 = pKnownHost->Base64;
	KeyLine.Comment = pKnownHost->Comment;
	KeyLine.BlobSize = pKnownHost->BlobSize;
	return xrtSshPublicKeyLineMatch(&KeyLine, Blob, pMatch);
}



/* 匹配整个 pattern-list，并让否定项覆盖正向项。 */
xsshcode xrtSshKnownHostPatternsMatch(
	xstrview Patterns,
	xstrview Host,
	uint32 iPort,
	xsshknownhostmatch* pMatch
)
{
	xsshknownhosttarget Target;
	xsshknownhostmatch Match = XSSH_KNOWN_HOST_NO_MATCH;
	bool bHashed;
	size_t iStart = 0u;
	size_t i;

	if ( !xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		!xsshKnownHostPatternsValid(Patterns, &bHashed) ||
		!xsshKnownHostTargetInit(&Target, Host, iPort) ||
		xrtMemRangesOverlap(
			Patterns.Data,
			Patterns.Size,
			pMatch,
			sizeof(*pMatch)
		) || xrtMemRangesOverlap(
			Host.Data,
			Host.Size,
			pMatch,
			sizeof(*pMatch)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( bHashed ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	for ( i = 0u; i <= Patterns.Size; ++i ) {
		if ( (i == Patterns.Size) || (Patterns.Data[i] == ',') ) {
			xstrview Pattern;
			bool bNegated = Patterns.Data[iStart] == '!';

			Pattern.Data = Patterns.Data + iStart + (bNegated ? 1u : 0u);
			Pattern.Size = i - iStart - (bNegated ? 1u : 0u);
			if ( xsshKnownHostPatternMatch(Pattern, &Target) ) {
				if ( bNegated ) {
					Match = XSSH_KNOWN_HOST_NEGATED;
					break;
				}
				Match = XSSH_KNOWN_HOST_MATCH;
			}
			iStart = i + 1u;
		}
	}
	*pMatch = Match;
	return XSSH_OK;
}



/* 使用已解析的 Hosts 字段执行便利匹配。 */
xsshcode xrtSshKnownHostLineMatch(
	const xsshknownhostline* pKnownHost,
	xstrview Host,
	uint32 iPort,
	xsshknownhostmatch* pMatch
)
{
	if ( !xrtMemRangeValid(pKnownHost, sizeof(*pKnownHost)) ||
		!xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		xrtMemRangesOverlap(
			pKnownHost,
			sizeof(*pKnownHost),
			pMatch,
			sizeof(*pMatch)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pKnownHost->Hashed ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	return xrtSshKnownHostPatternsMatch(
		pKnownHost->Hosts,
		Host,
		iPort,
		pMatch
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/key/ssh_known_host_hash.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KNOWN_HOST_HASH)



#include <string.h>



#if defined(XSSH_FEATURE_KNOWN_HOST_HASH)

/* 以 ASCII 小写追加主机名，不为任意长度输入建立临时副本。 */
static bool xsshKnownHostHashHostUpdate(
	xsha1* pState,
	xstrview Host
)
{
	size_t iStart = 0u;
	size_t i;

	for ( i = 0u; i < Host.Size; ++i ) {
		unsigned char iCharacter = (unsigned char)Host.Data[i];

		if ( (iCharacter >= (unsigned char)'A') &&
			(iCharacter <= (unsigned char)'Z') ) {
			unsigned char iLower =
				(unsigned char)(iCharacter + ('a' - 'A'));

			if ( !xrtSha1Update(
				pState,
				Host.Data + iStart,
				i - iStart
			) || !xrtSha1Update(pState, &iLower, 1u) ) {
				return false;
			}
			iStart = i + 1u;
		}
	}
	return xrtSha1Update(
		pState,
		Host.Data + iStart,
		Host.Size - iStart
	);
}



/* 向摘要追加虚拟 host 或 [host]:port。 */
static bool xsshKnownHostHashTargetUpdate(
	xsha1* pState,
	const xsshknownhosttarget* pTarget
)
{
	static const char sOpen[] = "[";
	static const char sClose[] = "]:";

	if ( !pTarget->Bracketed ) {
		return xsshKnownHostHashHostUpdate(pState, pTarget->Host);
	}
	return xrtSha1Update(pState, sOpen, sizeof(sOpen) - 1u) &&
		xsshKnownHostHashHostUpdate(pState, pTarget->Host) &&
		xrtSha1Update(pState, sClose, sizeof(sClose) - 1u) &&
		xrtSha1Update(pState, pTarget->Port, pTarget->PortSize);
}



/* 使用 XRT SHA-1 组合 OpenSSH 历史格式所需的固定 20 字节 HMAC。 */
static bool xsshKnownHostHmacSha1(
	const xsshknownhosttarget* pTarget,
	xbytesview Salt,
	unsigned char pHash[XSSH_KNOWN_HOST_HASH_SIZE]
)
{
	unsigned char arrInnerPad[XRT_SHA1_BLOCK_SIZE];
	unsigned char arrOuterPad[XRT_SHA1_BLOCK_SIZE];
	unsigned char arrInner[XSSH_KNOWN_HOST_HASH_SIZE];
	xsha1 Inner;
	xsha1 Outer;
	size_t i;
	bool bResult;

	memset(arrInnerPad, 0, sizeof(arrInnerPad));
	memset(arrOuterPad, 0, sizeof(arrOuterPad));
	memcpy(arrInnerPad, Salt.Data, Salt.Size);
	memcpy(arrOuterPad, Salt.Data, Salt.Size);
	for ( i = 0u; i < sizeof(arrInnerPad); ++i ) {
		arrInnerPad[i] ^= 0x36u;
		arrOuterPad[i] ^= 0x5cu;
	}
	xrtSha1Init(&Inner);
	xrtSha1Init(&Outer);
	bResult = xrtSha1Update(&Inner, arrInnerPad, sizeof(arrInnerPad)) &&
		xsshKnownHostHashTargetUpdate(&Inner, pTarget) &&
		xrtSha1Final(&Inner, arrInner) &&
		xrtSha1Update(&Outer, arrOuterPad, sizeof(arrOuterPad)) &&
		xrtSha1Update(&Outer, arrInner, sizeof(arrInner)) &&
		xrtSha1Final(&Outer, pHash);
	xrtSecureZero(arrInnerPad, sizeof(arrInnerPad));
	xrtSecureZero(arrOuterPad, sizeof(arrOuterPad));
	xrtSecureZero(arrInner, sizeof(arrInner));
	xrtSecureZero(&Inner, sizeof(Inner));
	xrtSecureZero(&Outer, sizeof(Outer));
	return bResult;
}



/* 严格拆分 |1|salt|hash，具体 Base64 长度由解码结果约束。 */
static xsshcode xsshKnownHostHashFields(
	xstrview HashedHost,
	xstrview* pSalt,
	xstrview* pHash
)
{
	size_t i;
	size_t iDelimiter = SIZE_MAX;

	if ( !xrtMemRangeValid(HashedHost.Data, HashedHost.Size) ||
		(pSalt == NULL) || (pHash == NULL) ||
		(HashedHost.Size < 5u) ||
		(memcmp(HashedHost.Data, "|1|", 3u) != 0) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	for ( i = 3u; i < HashedHost.Size; ++i ) {
		if ( HashedHost.Data[i] == '|' ) {
			if ( iDelimiter != SIZE_MAX ) {
				return XSSH_ERROR_PROTOCOL;
			}
			iDelimiter = i;
		}
	}
	if ( (iDelimiter == SIZE_MAX) || (iDelimiter == 3u) ||
		((iDelimiter + 1u) == HashedHost.Size) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pSalt->Data = HashedHost.Data + 3u;
	pSalt->Size = iDelimiter - 3u;
	pHash->Data = HashedHost.Data + iDelimiter + 1u;
	pHash->Size = HashedHost.Size - iDelimiter - 1u;
	return XSSH_OK;
}



/* 常量时间比较固定长度摘要。 */
static bool xsshKnownHostHashEqual(
	const unsigned char* pLeft,
	const unsigned char* pRight
)
{
	unsigned int iDifference = 0u;
	size_t i;

	for ( i = 0u; i < XSSH_KNOWN_HOST_HASH_SIZE; ++i ) {
		iDifference |= (unsigned int)(pLeft[i] ^ pRight[i]);
	}
	return iDifference == 0u;
}



/* 公开无文本编码的 HMAC-SHA1 原语，供数据库生成器复用。 */
xsshcode xrtSshKnownHostHash(
	xstrview Host,
	uint32 iPort,
	xbytesview Salt,
	void* pHash
)
{
	xsshknownhosttarget Target;
	unsigned char arrHash[XSSH_KNOWN_HOST_HASH_SIZE];

	if ( (Salt.Size != XSSH_KNOWN_HOST_HASH_SIZE) ||
		!xrtMemRangeValid(Salt.Data, Salt.Size) ||
		!xrtMemRangeValid(pHash, XSSH_KNOWN_HOST_HASH_SIZE) ||
		!xsshKnownHostTargetInit(&Target, Host, iPort) ||
		xrtMemRangesOverlap(
			Host.Data,
			Host.Size,
			pHash,
			XSSH_KNOWN_HOST_HASH_SIZE
		) || xrtMemRangesOverlap(
			Salt.Data,
			Salt.Size,
			pHash,
			XSSH_KNOWN_HOST_HASH_SIZE
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xsshKnownHostHmacSha1(&Target, Salt, arrHash) ) {
		xrtSecureZero(arrHash, sizeof(arrHash));
		return XSSH_ERROR_STATE;
	}
	memcpy(pHash, arrHash, sizeof(arrHash));
	xrtSecureZero(arrHash, sizeof(arrHash));
	return XSSH_OK;
}



/* 预计算完整容量后直接生成 OpenSSH hashed-host token。 */
xsshcode xrtSshKnownHostHashWrite(
	xstrview Host,
	uint32 iPort,
	xbytesview Salt,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	unsigned char arrHash[XSSH_KNOWN_HOST_HASH_SIZE];
	char sSalt[32];
	char sHash[32];
	size_t iSaltSize;
	size_t iHashSize;
	size_t iRequired;
	xsshcode Code;

	if ( !xrtMemRangeValid(Host.Data, Host.Size) ||
		!xrtMemRangeValid(Salt.Data, Salt.Size) ||
		!xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		!xrtMemRangeValid(sOutput, iCapacity) ||
		xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Host.Data,
			Host.Size
		) || xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Salt.Data,
			Salt.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshKnownHostHash(Host, iPort, Salt, arrHash);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtBase64Encode(
		Salt.Data,
		Salt.Size,
		sSalt,
		sizeof(sSalt),
		&iSaltSize,
		NULL
	) || !xrtBase64Encode(
		arrHash,
		sizeof(arrHash),
		sHash,
		sizeof(sHash),
		&iHashSize,
		NULL
	) ) {
		xrtSecureZero(arrHash, sizeof(arrHash));
		return XSSH_ERROR_STATE;
	}
	iRequired = 4u + iSaltSize + iHashSize;
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		xrtSecureZero(arrHash, sizeof(arrHash));
		return XSSH_OK;
	}
	if ( (iCapacity <= iRequired) ||
		xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			Host.Data,
			Host.Size
		) || xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			Salt.Data,
			Salt.Size
		) || xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			pOutputSize,
			sizeof(*pOutputSize)
		) ) {
		xrtSecureZero(arrHash, sizeof(arrHash));
		return iCapacity <= iRequired ?
			XSSH_ERROR_SPACE : XSSH_ERROR_ARGUMENT;
	}
	memcpy(sOutput, "|1|", 3u);
	memcpy(sOutput + 3u, sSalt, iSaltSize);
	sOutput[3u + iSaltSize] = '|';
	memcpy(sOutput + 4u + iSaltSize, sHash, iHashSize);
	sOutput[iRequired] = '\0';
	*pOutputSize = iRequired;
	xrtSecureZero(arrHash, sizeof(arrHash));
	return XSSH_OK;
}



/* 解码 salt/hash，重新计算并在末尾一次发布匹配结果。 */
xsshcode xrtSshKnownHostHashMatch(
	xstrview HashedHost,
	xstrview Host,
	uint32 iPort,
	bool* pMatch
)
{
	unsigned char arrSalt[XSSH_KNOWN_HOST_HASH_SIZE];
	unsigned char arrExpected[XSSH_KNOWN_HOST_HASH_SIZE];
	unsigned char arrActual[XSSH_KNOWN_HOST_HASH_SIZE];
	xstrview SaltText;
	xstrview HashText;
	size_t iSaltSize;
	size_t iHashSize;
	bool bMatch;
	xsshcode Code;

	if ( !xrtMemRangeValid(HashedHost.Data, HashedHost.Size) ||
		!xrtMemRangeValid(Host.Data, Host.Size) ||
		!xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		xrtMemRangesOverlap(
			HashedHost.Data,
			HashedHost.Size,
			pMatch,
			sizeof(*pMatch)
		) || xrtMemRangesOverlap(
			Host.Data,
			Host.Size,
			pMatch,
			sizeof(*pMatch)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKnownHostHashFields(HashedHost, &SaltText, &HashText);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtBase64Decode(
		SaltText.Data,
		SaltText.Size,
		arrSalt,
		sizeof(arrSalt),
		&iSaltSize,
		NULL
	) || !xrtBase64Decode(
		HashText.Data,
		HashText.Size,
		arrExpected,
		sizeof(arrExpected),
		&iHashSize,
		NULL
	) || (iSaltSize != sizeof(arrSalt)) ||
		(iHashSize != sizeof(arrExpected)) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshKnownHostHash(
		Host,
		iPort,
		(xbytesview){ arrSalt, sizeof(arrSalt) },
		arrActual
	);
	if ( Code != XSSH_OK ) {
		xrtSecureZero(arrSalt, sizeof(arrSalt));
		xrtSecureZero(arrExpected, sizeof(arrExpected));
		return Code;
	}
	bMatch = xsshKnownHostHashEqual(arrExpected, arrActual);
	xrtSecureZero(arrSalt, sizeof(arrSalt));
	xrtSecureZero(arrExpected, sizeof(arrExpected));
	xrtSecureZero(arrActual, sizeof(arrActual));
	*pMatch = bMatch;
	return XSSH_OK;
}



/* 验证行类型后复用 hashed-host token matcher。 */
xsshcode xrtSshKnownHostLineHashMatch(
	const xsshknownhostline* pKnownHost,
	xstrview Host,
	uint32 iPort,
	bool* pMatch
)
{
	if ( !xrtMemRangeValid(pKnownHost, sizeof(*pKnownHost)) ||
		!xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		xrtMemRangesOverlap(
			pKnownHost,
			sizeof(*pKnownHost),
			pMatch,
			sizeof(*pMatch)
		) || !pKnownHost->Hashed ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshKnownHostHashMatch(
		pKnownHost->Hosts,
		Host,
		iPort,
		pMatch
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/key/ssh_known_host_db.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KNOWN_HOST_DB)


#include <string.h>



#if defined(XSSH_FEATURE_KNOWN_HOST_DB)

#define XSSH_KNOWN_HOST_DB_VALID_FLAGS \
	((uint32)XSSH_KNOWN_HOST_DB_STRICT)



/* 判断去掉行结束符后的文本是否为空行或注释。 */
static bool xsshKnownHostDbIgnorable(xstrview Line)
{
	size_t i = 0u;

	while ( (i < Line.Size) && xsshKeyTextSpace(
		(unsigned char)Line.Data[i]
	) ) {
		++i;
	}
	return (i == Line.Size) || (Line.Data[i] == '#');
}



/* 校验游标和输出不会在推进时覆盖借用数据库。 */
static bool xsshKnownHostDbStateValid(
	const xsshknownhostdb* pDatabase,
	const xsshknownhostentry* pEntry
)
{
	return xrtMemRangeValid(pDatabase, sizeof(*pDatabase)) &&
		xrtMemRangeValid(pEntry, sizeof(*pEntry)) &&
		xrtMemRangeValid(pDatabase->Source.Data, pDatabase->Source.Size) &&
		(pDatabase->Position <= pDatabase->Source.Size) &&
		((pDatabase->Flags & ~XSSH_KNOWN_HOST_DB_VALID_FLAGS) == 0u) &&
		!xrtMemRangesOverlap(
			pDatabase,
			sizeof(*pDatabase),
			pEntry,
			sizeof(*pEntry)
		) && !xrtMemRangesOverlap(
			pDatabase->Source.Data,
			pDatabase->Source.Size,
			pDatabase,
			sizeof(*pDatabase)
		) && !xrtMemRangesOverlap(
			pDatabase->Source.Data,
			pDatabase->Source.Size,
			pEntry,
			sizeof(*pEntry)
		);
}



/* 返回一行无结束符借用视图，并在局部游标中推进位置和行号。 */
static xsshcode xsshKnownHostDbLine(
	xsshknownhostdb* pDatabase,
	xstrview* pLine
)
{
	size_t iStart = pDatabase->Position;
	size_t iEnd = iStart;

	if ( iStart == pDatabase->Source.Size ) {
		return XSSH_NEED_MORE;
	}
	if ( pDatabase->LineNumber == SIZE_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	while ( (iEnd < pDatabase->Source.Size) &&
		(pDatabase->Source.Data[iEnd] != '\n') ) {
		++iEnd;
	}
	pDatabase->Position = iEnd + (iEnd < pDatabase->Source.Size ? 1u : 0u);
	++pDatabase->LineNumber;
	if ( (iEnd > iStart) &&
		(pDatabase->Source.Data[iEnd - 1u] == '\r') ) {
		--iEnd;
	}
	pLine->Data = pDatabase->Source.Data + iStart;
	pLine->Size = iEnd - iStart;
	return XSSH_OK;
}



/* 匹配明文或 OpenSSH |1| 主机字段，并统一否定项为不匹配。 */
static xsshcode xsshKnownHostDbHostMatch(
	const xsshknownhostline* pKnownHost,
	xstrview Host,
	uint32 iPort,
	bool* pMatch
)
{
	if ( pKnownHost->Hashed ) {
		return xrtSshKnownHostLineHashMatch(
			pKnownHost,
			Host,
			iPort,
			pMatch
		);
	} else {
		xsshknownhostmatch Match;
		xsshcode Code = xrtSshKnownHostLineMatch(
			pKnownHost,
			Host,
			iPort,
			&Match
		);

		if ( Code != XSSH_OK ) {
			return Code;
		}
		*pMatch = Match == XSSH_KNOWN_HOST_MATCH;
		return XSSH_OK;
	}
}



/* 生成没有决定性来源行的 NEW 结果。 */
static void xsshKnownHostDbNew(xsshknownhostcheck* pCheck)
{
	memset(pCheck, 0, sizeof(*pCheck));
	pCheck->Trust = XSSH_KNOWN_HOST_TRUST_NEW;
}



/* 初始化只借用文本的数据库游标。 */
xsshcode xrtSshKnownHostDbInit(
	xsshknownhostdb* pDatabase,
	xstrview Source,
	uint32 iFlags
)
{
	xsshknownhostdb Database;

	if ( !xrtMemRangeValid(pDatabase, sizeof(*pDatabase)) ||
		!xrtMemRangeValid(Source.Data, Source.Size) ||
		((iFlags & ~XSSH_KNOWN_HOST_DB_VALID_FLAGS) != 0u) ||
		xrtMemRangesOverlap(
			Source.Data,
			Source.Size,
			pDatabase,
			sizeof(*pDatabase)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Database.Source = Source;
	Database.Position = 0u;
	Database.LineNumber = 0u;
	Database.Flags = iFlags;
	*pDatabase = Database;
	return XSSH_OK;
}



/* 逐行解析借用文本，严格模式把第一个坏行作为显式条目返回。 */
xsshcode xrtSshKnownHostDbNext(
	xsshknownhostdb* pDatabase,
	xsshknownhostentry* pEntry
)
{
	xsshknownhostdb Database;
	xsshknownhostentry Entry;
	xstrview Line;
	xsshcode Code;

	if ( !xsshKnownHostDbStateValid(pDatabase, pEntry) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Database = *pDatabase;
	for ( ;; ) {
		Code = xsshKnownHostDbLine(&Database, &Line);
		if ( Code != XSSH_OK ) {
			if ( Code == XSSH_NEED_MORE ) {
				*pDatabase = Database;
			}
			return Code;
		}
		if ( xsshKnownHostDbIgnorable(Line) ) {
			continue;
		}
		memset(&Entry, 0, sizeof(Entry));
		Entry.Source = Line;
		Entry.LineNumber = Database.LineNumber;
		Code = xrtSshKnownHostLineRead(Line, &Entry.KnownHost);
		if ( Code == XSSH_OK ) {
			Entry.Valid = true;
			break;
		}
		if ( (Code != XSSH_ERROR_PROTOCOL) &&
			(Code != XSSH_ERROR_UNSUPPORTED) ) {
			return Code;
		}
		if ( (Database.Flags & (uint32)XSSH_KNOWN_HOST_DB_STRICT) != 0u ) {
			break;
		}
	}
	*pDatabase = Database;
	*pEntry = Entry;
	return XSSH_OK;
}



/* 扫描完整文本并按安全优先级汇总普通 key、撤销和 CA 记录。 */
xsshcode xrtSshKnownHostDbCheck(
	xstrview Source,
	xstrview Host,
	uint32 iPort,
	xbytesview KeyBlob,
	uint32 iFlags,
	xsshknownhostcheck* pCheck
)
{
	xsshknownhosttarget Target;
	xsshpublickey PublicKey;
	xsshknownhostdb Database;
	xsshknownhostentry Entry;
	xsshknownhostentry MatchEntry;
	xsshknownhostentry ChangedEntry;
	xsshknownhostentry CaEntry;
	xsshknownhostcheck Check;
	bool bHaveMatch = false;
	bool bHaveChanged = false;
	bool bHaveCa = false;
	xsshcode Code;

	if ( !xrtMemRangeValid(pCheck, sizeof(*pCheck)) ||
		!xrtMemRangeValid(Source.Data, Source.Size) ||
		!xrtMemRangeValid(KeyBlob.Data, KeyBlob.Size) ||
		!xsshKnownHostTargetInit(&Target, Host, iPort) ||
		xrtMemRangesOverlap(
			Source.Data,
			Source.Size,
			pCheck,
			sizeof(*pCheck)
		) || xrtMemRangesOverlap(
			Host.Data,
			Host.Size,
			pCheck,
			sizeof(*pCheck)
		) || xrtMemRangesOverlap(
			KeyBlob.Data,
			KeyBlob.Size,
			pCheck,
			sizeof(*pCheck)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	(void)Target;
	Code = xrtSshPublicKeyRead(KeyBlob, &PublicKey);
	if ( Code == XSSH_NEED_MORE ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshKnownHostDbInit(&Database, Source, iFlags);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	for ( ;; ) {
		bool bHostMatch;
		bool bKeyMatch;

		Code = xrtSshKnownHostDbNext(&Database, &Entry);
		if ( Code == XSSH_NEED_MORE ) {
			break;
		}
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( !Entry.Valid ||
			(Entry.KnownHost.MarkerKind == XSSH_KNOWN_HOST_MARKER_UNKNOWN) ) {
			if ( (iFlags & (uint32)XSSH_KNOWN_HOST_DB_STRICT) != 0u ) {
				Check.Trust = XSSH_KNOWN_HOST_TRUST_INVALID;
				Check.Entry = Entry;
				*pCheck = Check;
				return XSSH_OK;
			}
			continue;
		}
		Code = xsshKnownHostDbHostMatch(
			&Entry.KnownHost,
			Host,
			iPort,
			&bHostMatch
		);
		if ( Code != XSSH_OK ) {
			if ( ((iFlags & (uint32)XSSH_KNOWN_HOST_DB_STRICT) != 0u) &&
				(Code == XSSH_ERROR_PROTOCOL) ) {
				Entry.Valid = false;
				memset(&Entry.KnownHost, 0, sizeof(Entry.KnownHost));
				Check.Trust = XSSH_KNOWN_HOST_TRUST_INVALID;
				Check.Entry = Entry;
				*pCheck = Check;
				return XSSH_OK;
			}
			if ( Code == XSSH_ERROR_PROTOCOL ) {
				continue;
			}
			return Code;
		}
		if ( !bHostMatch ) {
			continue;
		}
		if ( Entry.KnownHost.MarkerKind ==
			XSSH_KNOWN_HOST_MARKER_CERT_AUTHORITY ) {
			if ( !bHaveCa ) {
				CaEntry = Entry;
				bHaveCa = true;
			}
			continue;
		}
		Code = xrtSshKnownHostLineKeyMatch(
			&Entry.KnownHost,
			KeyBlob,
			&bKeyMatch
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( Entry.KnownHost.MarkerKind ==
			XSSH_KNOWN_HOST_MARKER_REVOKED ) {
			if ( bKeyMatch ) {
				Check.Trust = XSSH_KNOWN_HOST_TRUST_REVOKED;
				Check.Entry = Entry;
				*pCheck = Check;
				return XSSH_OK;
			}
			continue;
		}
		if ( bKeyMatch ) {
			if ( !bHaveMatch ) {
				MatchEntry = Entry;
				bHaveMatch = true;
			}
		} else if ( !bHaveChanged ) {
			ChangedEntry = Entry;
			bHaveChanged = true;
		}
	}
	if ( bHaveMatch ) {
		Check.Trust = XSSH_KNOWN_HOST_TRUST_MATCH;
		Check.Entry = MatchEntry;
	} else if ( bHaveCa ) {
		Check.Trust = XSSH_KNOWN_HOST_TRUST_CERT_AUTHORITY;
		Check.Entry = CaEntry;
	} else if ( bHaveChanged ) {
		Check.Trust = XSSH_KNOWN_HOST_TRUST_CHANGED;
		Check.Entry = ChangedEntry;
	} else {
		xsshKnownHostDbNew(&Check);
	}
	*pCheck = Check;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/key/ssh_fingerprint.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_FINGERPRINT)


#include <string.h>



#if defined(XSSH_FEATURE_FINGERPRINT)

/* 先写局部摘要，确保失败不会发布部分结果。 */
xsshcode xrtSshHostKeyDigestSha256(
	xbytesview HostKey,
	void* pDigest
)
{
	unsigned char arrDigest[XSSH_FINGERPRINT_SHA256_SIZE];

	if ( !xrtMemRangeValid(HostKey.Data, HostKey.Size) ||
		!xrtMemRangeValid(pDigest, sizeof(arrDigest)) ||
		xrtMemRangesOverlap(
			HostKey.Data,
			HostKey.Size,
			pDigest,
			sizeof(arrDigest)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtSha256(HostKey.Data, HostKey.Size, arrDigest) ) {
		xrtSecureZero(arrDigest, sizeof(arrDigest));
		return XSSH_ERROR_STATE;
	}
	memcpy(pDigest, arrDigest, sizeof(arrDigest));
	xrtSecureZero(arrDigest, sizeof(arrDigest));
	return XSSH_OK;
}



/* 复用 XRT 无填充 Base64，生成 OpenSSH 标准展示格式。 */
xsshcode xrtSshHostKeyFingerprintSha256(
	xbytesview HostKey,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	static const char sPrefix[] = "SHA256:";
	unsigned char arrDigest[XSSH_FINGERPRINT_SHA256_SIZE];
	char sBase64[48];
	xbase64config Config = { NULL, XBASE64_NO_PADDING };
	size_t iBase64Size;
	size_t iRequired;
	xsshcode Code;

	if ( !xrtMemRangeValid(HostKey.Data, HostKey.Size) ||
		!xrtMemRangeValid(sOutput, iCapacity) ||
		!xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		xrtMemRangesOverlap(
			HostKey.Data,
			HostKey.Size,
			pOutputSize,
			sizeof(*pOutputSize)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshHostKeyDigestSha256(HostKey, arrDigest);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtBase64Encode(
		arrDigest,
		sizeof(arrDigest),
		sBase64,
		sizeof(sBase64),
		&iBase64Size,
		&Config
	) ) {
		xrtSecureZero(arrDigest, sizeof(arrDigest));
		return XSSH_ERROR_STATE;
	}
	iRequired = (sizeof(sPrefix) - 1u) + iBase64Size;
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		xrtSecureZero(arrDigest, sizeof(arrDigest));
		return XSSH_OK;
	}
	if ( (iCapacity <= iRequired) ||
		xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			HostKey.Data,
			HostKey.Size
		) || xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			pOutputSize,
			sizeof(*pOutputSize)
		) ) {
		xrtSecureZero(arrDigest, sizeof(arrDigest));
		return iCapacity <= iRequired ?
			XSSH_ERROR_SPACE : XSSH_ERROR_ARGUMENT;
	}
	memcpy(sOutput, sPrefix, sizeof(sPrefix) - 1u);
	memcpy(sOutput + sizeof(sPrefix) - 1u, sBase64, iBase64Size);
	sOutput[iRequired] = '\0';
	*pOutputSize = iRequired;
	xrtSecureZero(arrDigest, sizeof(arrDigest));
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/key/ssh_private_key.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_PRIVATE_KEY)

#include <string.h>



#if defined(XSSH_FEATURE_PRIVATE_KEY)

/* 完整容器中的短输入都属于协议格式错误，而不是增量等待。 */
static xsshcode xsshPrivateKeyComplete(xsshcode Code)
{
	return Code == XSSH_NEED_MORE ? XSSH_ERROR_PROTOCOL : Code;
}



/* 把 SSH string 字节视图转换为名称文本。 */
static xstrview xsshPrivateKeyText(xbytesview Value)
{
	xstrview Text;

	Text.Data = (const char*)Value.Data;
	Text.Size = Value.Size;
	return Text;
}



/* 比较名称与编译期文本。 */
static bool xsshPrivateKeyNameEqual(
	xstrview Name,
	const char* sExpected,
	size_t iExpectedSize
)
{
	return (Name.Size == iExpectedSize) &&
		(memcmp(Name.Data, sExpected, iExpectedSize) == 0);
}



/* 解析并预验证完整容器，未知 cipher/KDF 只作为元数据保留。 */
xsshcode xrtSshPrivateKeyRead(
	xbytesview Blob,
	xsshopensshprivatekey* pPrivateKey
)
{
	static const unsigned char arrMagic[] = XSSH_PRIVATE_KEY_MAGIC;
	xsshopensshprivatekey PrivateKey;
	xsshreader Reader;
	xbytesview Value;
	xbytesview PublicKeyBlob;
	xsshpublickey PublicKey;
	size_t iPublicStart;
	uint32 i;
	xsshcode Code;
	bool bCipherNone;
	bool bKdfNone;

	if ( !xrtMemRangeValid(pPrivateKey, sizeof(*pPrivateKey)) ||
		!xrtSshReaderInit(&Reader, Blob) ||
		xrtMemRangesOverlap(
			Blob.Data,
			Blob.Size,
			pPrivateKey,
			sizeof(*pPrivateKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPrivateKeyComplete(xrtSshReadBytes(
		&Reader,
		sizeof(arrMagic),
		&Value
	));
	if ( (Code != XSSH_OK) ||
		(memcmp(Value.Data, arrMagic, sizeof(arrMagic)) != 0) ) {
		return Code != XSSH_OK ? Code : XSSH_ERROR_PROTOCOL;
	}
	Code = xsshPrivateKeyComplete(xrtSshReadString(&Reader, &Value));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	PrivateKey.Cipher = xsshPrivateKeyText(Value);
	Code = xsshPrivateKeyComplete(xrtSshReadString(&Reader, &Value));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	PrivateKey.Kdf = xsshPrivateKeyText(Value);
	Code = xsshPrivateKeyComplete(xrtSshReadString(
		&Reader,
		&PrivateKey.KdfOptions
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshPrivateKeyComplete(xrtSshReadU32(
		&Reader,
		&PrivateKey.KeyCount
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshNameValid(PrivateKey.Cipher) ||
		!xrtSshNameValid(PrivateKey.Kdf) || (PrivateKey.KeyCount == 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	bCipherNone = xsshPrivateKeyNameEqual(
		PrivateKey.Cipher,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	);
	bKdfNone = xsshPrivateKeyNameEqual(
		PrivateKey.Kdf,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	);
	if ( (bCipherNone != bKdfNone) ||
		(bCipherNone && (PrivateKey.KdfOptions.Size != 0u)) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	iPublicStart = Reader.Position;
	for ( i = 0u; i < PrivateKey.KeyCount; ++i ) {
		Code = xsshPrivateKeyComplete(xrtSshReadString(
			&Reader,
			&PublicKeyBlob
		));
		if ( Code != XSSH_OK ) {
			return Code;
		}
		Code = xsshPrivateKeyComplete(xrtSshPublicKeyRead(
			PublicKeyBlob,
			&PublicKey
		));
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	PrivateKey.PublicKeys.Data = Blob.Data + iPublicStart;
	PrivateKey.PublicKeys.Size = Reader.Position - iPublicStart;
	Code = xsshPrivateKeyComplete(xrtSshReadString(
		&Reader,
		&PrivateKey.PrivateList
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (PrivateKey.PrivateList.Size == 0u) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	PrivateKey.Blob = Blob;
	*pPrivateKey = PrivateKey;
	return XSSH_OK;
}



/* 未加密容器必须同时使用 cipher=none、kdf=none 和空 options。 */
xsshcode xrtSshPrivateKeyIsEncrypted(
	const xsshopensshprivatekey* pPrivateKey,
	bool* pEncrypted
)
{
	xsshopensshprivatekey PrivateKey;
	bool bEncrypted;
	xsshcode Code;

	if ( !xrtMemRangeValid(pPrivateKey, sizeof(*pPrivateKey)) ||
		!xrtMemRangeValid(pEncrypted, sizeof(*pEncrypted)) ||
		!xrtMemRangeValid(pPrivateKey->Blob.Data, pPrivateKey->Blob.Size) ||
		xrtMemRangesOverlap(
			pPrivateKey,
			sizeof(*pPrivateKey),
			pEncrypted,
			sizeof(*pEncrypted)
		) || xrtMemRangesOverlap(
			pPrivateKey->Blob.Data,
			pPrivateKey->Blob.Size,
			pEncrypted,
			sizeof(*pEncrypted)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPrivateKeyRead(pPrivateKey->Blob, &PrivateKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	bEncrypted = !xsshPrivateKeyNameEqual(
		PrivateKey.Cipher,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	) || !xsshPrivateKeyNameEqual(
		PrivateKey.Kdf,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	) || (PrivateKey.KdfOptions.Size != 0u);
	*pEncrypted = bEncrypted;
	return XSSH_OK;
}



/* 从解析结果建立有界公钥 string 游标。 */
xsshcode xrtSshPrivateKeyPublicsInit(
	const xsshopensshprivatekey* pPrivateKey,
	xsshprivatekeypublics* pPublics
)
{
	xsshopensshprivatekey PrivateKey;
	xsshprivatekeypublics Publics;
	xsshcode Code;

	if ( !xrtMemRangeValid(pPrivateKey, sizeof(*pPrivateKey)) ||
		!xrtMemRangeValid(pPublics, sizeof(*pPublics)) ||
		!xrtMemRangeValid(pPrivateKey->Blob.Data, pPrivateKey->Blob.Size) ||
		xrtMemRangesOverlap(
			pPrivateKey,
			sizeof(*pPrivateKey),
			pPublics,
			sizeof(*pPublics)
		) || xrtMemRangesOverlap(
			pPrivateKey->Blob.Data,
			pPrivateKey->Blob.Size,
			pPublics,
			sizeof(*pPublics)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPrivateKeyRead(pPrivateKey->Blob, &PrivateKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshReaderInit(&Publics.Reader, PrivateKey.PublicKeys) ) {
		return XSSH_ERROR_STATE;
	}
	Publics.Remaining = PrivateKey.KeyCount;
	*pPublics = Publics;
	return XSSH_OK;
}



/* 事务式返回下一把预验证公钥。 */
xsshcode xrtSshPrivateKeyPublicsNext(
	xsshprivatekeypublics* pPublics,
	xbytesview* pPublicKey
)
{
	xsshprivatekeypublics Publics;
	xbytesview PublicKeyBlob;
	xsshpublickey PublicKey;
	xsshcode Code;

	if ( !xrtMemRangeValid(pPublics, sizeof(*pPublics)) ||
		!xrtMemRangeValid(pPublicKey, sizeof(*pPublicKey)) ||
		!xrtMemRangeValid(
			pPublics->Reader.Source.Data,
			pPublics->Reader.Source.Size
		) || (pPublics->Reader.Position > pPublics->Reader.Source.Size) ||
		xrtMemRangesOverlap(
			pPublics,
			sizeof(*pPublics),
			pPublicKey,
			sizeof(*pPublicKey)
		) || xrtMemRangesOverlap(
			pPublics->Reader.Source.Data,
			pPublics->Reader.Source.Size,
			pPublicKey,
			sizeof(*pPublicKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pPublics->Remaining == 0u ) {
		return xrtSshReaderRemaining(&pPublics->Reader) == 0u ?
			XSSH_NEED_MORE : XSSH_ERROR_PROTOCOL;
	}
	Publics = *pPublics;
	Code = xsshPrivateKeyComplete(xrtSshReadString(
		&Publics.Reader,
		&PublicKeyBlob
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshPrivateKeyComplete(xrtSshPublicKeyRead(
		PublicKeyBlob,
		&PublicKey
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	--Publics.Remaining;
	if ( (Publics.Remaining == 0u) &&
		(xrtSshReaderRemaining(&Publics.Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPublics = Publics;
	*pPublicKey = PublicKeyBlob;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/key/ssh_private_key_pem.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_PRIVATE_KEY_PEM)



#if defined(XSSH_FEATURE_PRIVATE_KEY_PEM)

/* 复用 XRT PEM 解码，并让结构化输出只在完整容器成功后发布。 */
xsshcode xrtSshPrivateKeyPemRead(
	xstrview Text,
	void* pBinary,
	size_t iCapacity,
	size_t* pBinarySize,
	xsshopensshprivatekey* pPrivateKey
)
{
	xpemblock Block;
	xsshopensshprivatekey PrivateKey;
	size_t iRequired;
	size_t iDecoded;
	xsshcode Code;

	if ( !xrtMemRangeValid(Text.Data, Text.Size) ||
		!xrtMemRangeValid(pBinarySize, sizeof(*pBinarySize)) ||
		xrtMemRangesOverlap(
			Text.Data,
			Text.Size,
			pBinarySize,
			sizeof(*pBinarySize)
		) || ((pBinary == NULL) &&
		 ((iCapacity != 0u) || (pPrivateKey != NULL))) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtPemFind(
		Text.Data,
		Text.Size,
		XSSH_PRIVATE_KEY_PEM_LABEL,
		&Block
	) || !xrtPemDecode(&Block, NULL, 0u, &iRequired) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( pBinary == NULL ) {
		*pBinarySize = iRequired;
		return XSSH_OK;
	}
	if ( !xrtMemRangeValid(pBinary, iCapacity) ||
		!xrtMemRangeValid(pPrivateKey, sizeof(*pPrivateKey)) ||
		xrtMemRangesOverlap(
			pBinary,
			iCapacity,
			Text.Data,
			Text.Size
		) || xrtMemRangesOverlap(
			pBinary,
			iCapacity,
			pBinarySize,
			sizeof(*pBinarySize)
		) || xrtMemRangesOverlap(
			pBinary,
			iCapacity,
			pPrivateKey,
			sizeof(*pPrivateKey)
		) || xrtMemRangesOverlap(
			Text.Data,
			Text.Size,
			pPrivateKey,
			sizeof(*pPrivateKey)
		) || xrtMemRangesOverlap(
			pBinarySize,
			sizeof(*pBinarySize),
			pPrivateKey,
			sizeof(*pPrivateKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iCapacity < iRequired ) {
		return XSSH_ERROR_SPACE;
	}
	if ( !xrtPemDecode(
		&Block,
		pBinary,
		iCapacity,
		&iDecoded
	) || (iDecoded != iRequired) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshPrivateKeyRead(
		(xbytesview){ (const unsigned char*)pBinary, iDecoded },
		&PrivateKey
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pPrivateKey = PrivateKey;
	*pBinarySize = iDecoded;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/key/ssh_private_key_ed25519.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_PRIVATE_KEY_ED25519)


#include <string.h>



#if defined(XSSH_FEATURE_PRIVATE_KEY_ED25519)

/* 比较借用文本与编译期名称。 */
static bool xsshPrivateEd25519NameEqual(
	xstrview Name,
	const char* sExpected,
	size_t iExpectedSize
)
{
	return (Name.Size == iExpectedSize) &&
		(memcmp(Name.Data, sExpected, iExpectedSize) == 0);
}



/* 完整私钥列表中的短输入统一视为协议错误。 */
static xsshcode xsshPrivateEd25519Complete(xsshcode Code)
{
	return Code == XSSH_NEED_MORE ? XSSH_ERROR_PROTOCOL : Code;
}



/* 校验借用身份字段和 seed/public 对应关系。 */
static xsshcode xsshPrivateEd25519Validate(
	const xsshed25519identity* pIdentity
)
{
	unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE];
	xbytesview BlobPublic;
	xsshcode Code;

	if ( !xrtMemRangeValid(pIdentity, sizeof(*pIdentity)) ||
		!xrtMemRangeValid(
			pIdentity->PublicKeyBlob.Data,
			pIdentity->PublicKeyBlob.Size
		) || !xrtMemRangeValid(pIdentity->Seed.Data, pIdentity->Seed.Size) ||
		!xrtMemRangeValid(
			pIdentity->PublicKey.Data,
			pIdentity->PublicKey.Size
		) || !xrtMemRangeValid(
			pIdentity->Comment.Data,
			pIdentity->Comment.Size
		) || (pIdentity->Seed.Size != XRT_ED25519_SEED_SIZE) ||
		(pIdentity->PublicKey.Size != XSSH_ED25519_PUBLIC_SIZE) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshEd25519PublicKeyRead(
		pIdentity->PublicKeyBlob,
		&BlobPublic
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtEd25519Public(pIdentity->Seed.Data, arrPublic) ) {
		xrtSecureZero(arrPublic, sizeof(arrPublic));
		return XSSH_ERROR_STATE;
	}
	if ( (memcmp(
		arrPublic,
		pIdentity->PublicKey.Data,
		sizeof(arrPublic)
	) != 0) || (memcmp(
		BlobPublic.Data,
		pIdentity->PublicKey.Data,
		sizeof(arrPublic)
	) != 0) ) {
		xrtSecureZero(arrPublic, sizeof(arrPublic));
		return XSSH_ERROR_PROTOCOL;
	}
	xrtSecureZero(arrPublic, sizeof(arrPublic));
	return XSSH_OK;
}



/* 解析 OpenSSH 单 Ed25519 私钥字段并校验 checkint、padding 和公私钥一致性。 */
xsshcode xrtSshPrivateKeyEd25519Read(
	const xsshopensshprivatekey* pPrivateKey,
	xsshed25519identity* pIdentity
)
{
	xsshopensshprivatekey PrivateKey;
	xsshprivatekeypublics Publics;
	xbytesview PublicKeyBlob;
	xbytesview BlobPublic;
	xbytesview KeyType;
	xbytesview PublicRaw;
	xbytesview PrivateRaw;
	xbytesview Comment;
	xsshreader Reader;
	xsshed25519identity Identity;
	unsigned char arrDerived[XSSH_ED25519_PUBLIC_SIZE];
	uint32 iCheckFirst;
	uint32 iCheckSecond;
	size_t iPadding;
	size_t i;
	bool bEncrypted;
	xsshcode Code;

	if ( !xrtMemRangeValid(pPrivateKey, sizeof(*pPrivateKey)) ||
		!xrtMemRangeValid(pIdentity, sizeof(*pIdentity)) ||
		xrtMemRangesOverlap(
			pPrivateKey,
			sizeof(*pPrivateKey),
			pIdentity,
			sizeof(*pIdentity)
		) || xrtMemRangesOverlap(
			pPrivateKey->Blob.Data,
			pPrivateKey->Blob.Size,
			pIdentity,
			sizeof(*pIdentity)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPrivateKeyRead(pPrivateKey->Blob, &PrivateKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	bEncrypted = !xsshPrivateEd25519NameEqual(
		PrivateKey.Cipher,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	) || !xsshPrivateEd25519NameEqual(
		PrivateKey.Kdf,
		XSSH_PRIVATE_KEY_NONE,
		sizeof(XSSH_PRIVATE_KEY_NONE) - 1u
	) || (PrivateKey.KdfOptions.Size != 0u);
	if ( bEncrypted || (PrivateKey.KeyCount != 1u) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( !xrtSshReaderInit(&Publics.Reader, PrivateKey.PublicKeys) ) {
		return XSSH_ERROR_STATE;
	}
	Publics.Remaining = PrivateKey.KeyCount;
	Code = xrtSshPrivateKeyPublicsNext(&Publics, &PublicKeyBlob);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshPrivateEd25519Complete(xrtSshEd25519PublicKeyRead(
		PublicKeyBlob,
		&BlobPublic
	));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshReaderInit(&Reader, PrivateKey.PrivateList) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPrivateEd25519Complete(xrtSshReadU32(
		&Reader,
		&iCheckFirst
	));
	if ( Code == XSSH_OK ) {
		Code = xsshPrivateEd25519Complete(xrtSshReadU32(
			&Reader,
			&iCheckSecond
		));
	}
	if ( Code == XSSH_OK ) {
		Code = xsshPrivateEd25519Complete(xrtSshReadString(
			&Reader,
			&KeyType
		));
	}
	if ( Code == XSSH_OK ) {
		Code = xsshPrivateEd25519Complete(xrtSshReadString(
			&Reader,
			&PublicRaw
		));
	}
	if ( Code == XSSH_OK ) {
		Code = xsshPrivateEd25519Complete(xrtSshReadString(
			&Reader,
			&PrivateRaw
		));
	}
	if ( Code == XSSH_OK ) {
		Code = xsshPrivateEd25519Complete(xrtSshReadString(
			&Reader,
			&Comment
		));
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (iCheckFirst != iCheckSecond) ||
		!xsshPrivateEd25519NameEqual(
			(xstrview){ (const char*)KeyType.Data, KeyType.Size },
			XSSH_HOSTKEY_ED25519,
			sizeof(XSSH_HOSTKEY_ED25519) - 1u
		) || (PublicRaw.Size != XSSH_ED25519_PUBLIC_SIZE) ||
		(PrivateRaw.Size !=
		 (XRT_ED25519_SEED_SIZE + XSSH_ED25519_PUBLIC_SIZE)) ||
		(memcmp(
			PrivateRaw.Data + XRT_ED25519_SEED_SIZE,
			PublicRaw.Data,
			XSSH_ED25519_PUBLIC_SIZE
		) != 0) || (memcmp(
			BlobPublic.Data,
			PublicRaw.Data,
			XSSH_ED25519_PUBLIC_SIZE
		) != 0) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	iPadding = xrtSshReaderRemaining(&Reader);
	if ( iPadding > 255u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	for ( i = 0u; i < iPadding; ++i ) {
		uint8 iByte;

		if ( (xrtSshReadByte(&Reader, &iByte) != XSSH_OK) ||
			(iByte != (uint8)(i + 1u)) ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	if ( !xrtEd25519Public(PrivateRaw.Data, arrDerived) ) {
		xrtSecureZero(arrDerived, sizeof(arrDerived));
		return XSSH_ERROR_STATE;
	}
	if ( memcmp(
		arrDerived,
		PublicRaw.Data,
		sizeof(arrDerived)
	) != 0 ) {
		xrtSecureZero(arrDerived, sizeof(arrDerived));
		return XSSH_ERROR_PROTOCOL;
	}
	xrtSecureZero(arrDerived, sizeof(arrDerived));
	Identity.PublicKeyBlob = PublicKeyBlob;
	Identity.Seed.Data = PrivateRaw.Data;
	Identity.Seed.Size = XRT_ED25519_SEED_SIZE;
	Identity.PublicKey = PublicRaw;
	Identity.Comment = Comment;
	*pIdentity = Identity;
	return XSSH_OK;
}



/* 在局部缓冲完成签名，成功后一次发布固定长度结果。 */
xsshcode xrtSshPrivateKeyEd25519Sign(
	const xsshed25519identity* pIdentity,
	xbytesview Message,
	void* pSignature
)
{
	unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE];
	xsshcode Code;

	if ( !xrtMemRangeValid(Message.Data, Message.Size) ||
		!xrtMemRangeValid(pSignature, XSSH_ED25519_SIGNATURE_SIZE) ||
		xrtMemRangesOverlap(
			pIdentity,
			sizeof(*pIdentity),
			pSignature,
			XSSH_ED25519_SIGNATURE_SIZE
		) || xrtMemRangesOverlap(
			Message.Data,
			Message.Size,
			pSignature,
			XSSH_ED25519_SIGNATURE_SIZE
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPrivateEd25519Validate(pIdentity);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtMemRangesOverlap(
		pIdentity->PublicKeyBlob.Data,
		pIdentity->PublicKeyBlob.Size,
		pSignature,
		XSSH_ED25519_SIGNATURE_SIZE
	) || xrtMemRangesOverlap(
		pIdentity->Seed.Data,
		pIdentity->Seed.Size,
		pSignature,
		XSSH_ED25519_SIGNATURE_SIZE
	) || xrtMemRangesOverlap(
		pIdentity->PublicKey.Data,
		pIdentity->PublicKey.Size,
		pSignature,
		XSSH_ED25519_SIGNATURE_SIZE
	) || xrtMemRangesOverlap(
		pIdentity->Comment.Data,
		pIdentity->Comment.Size,
		pSignature,
		XSSH_ED25519_SIGNATURE_SIZE
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtEd25519Sign(
		pIdentity->Seed.Data,
		Message.Data,
		Message.Size,
		arrSignature
	) ) {
		xrtSecureZero(arrSignature, sizeof(arrSignature));
		return XSSH_ERROR_STATE;
	}
	memcpy(pSignature, arrSignature, sizeof(arrSignature));
	xrtSecureZero(arrSignature, sizeof(arrSignature));
	return XSSH_OK;
}



/* 预留最终输出后签名，并复用通用 signature blob writer。 */
xsshcode xrtSshPrivateKeyEd25519SignatureWrite(
	xsshwriter* pWriter,
	const xsshed25519identity* pIdentity,
	xbytesview Message
)
{
	xbytesview arrInputs[5];
	unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE];
	xsshwriter Writer;
	xsshcode Code;

	if ( !xrtMemRangeValid(pIdentity, sizeof(*pIdentity)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	arrInputs[0] = pIdentity->PublicKeyBlob;
	arrInputs[1] = pIdentity->Seed;
	arrInputs[2] = pIdentity->PublicKey;
	arrInputs[3] = pIdentity->Comment;
	arrInputs[4] = Message;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		8u + (sizeof(XSSH_HOSTKEY_ED25519) - 1u) +
			XSSH_ED25519_SIGNATURE_SIZE,
		arrInputs,
		5u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshPrivateKeyEd25519Sign(
		pIdentity,
		Message,
		arrSignature
	);
	if ( Code != XSSH_OK ) {
		xrtSecureZero(arrSignature, sizeof(arrSignature));
		return Code;
	}
	Writer = *pWriter;
	Code = xrtSshSignatureWrite(
		&Writer,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
		(xbytesview){ arrSignature, sizeof(arrSignature) }
	);
	xrtSecureZero(arrSignature, sizeof(arrSignature));
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/transport/ssh_transport_message.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_TRANSPORT_MESSAGE)



#if defined(XSSH_FEATURE_TRANSPORT_MESSAGE)

/* 转换字节视图为不要求零结尾的文本视图。 */
static xstrview xsshMessageText(xbytesview Value)
{
	xstrview Text;

	Text.Data = (const char*)Value.Data;
	Text.Size = Value.Size;
	return Text;
}



/* 转换文本视图为原始字节视图。 */
static xbytesview xsshMessageBytes(xstrview Value)
{
	xbytesview Bytes;

	Bytes.Data = (const unsigned char*)Value.Data;
	Bytes.Size = Value.Size;
	return Bytes;
}



/* 验证 string 视图并向总长度加入四字节前缀。 */
static xsshcode xsshMessageAddString(xbytesview Value, size_t* pTotal)
{
	if ( (pTotal == NULL) ||
		((Value.Data == NULL) && (Value.Size != 0u)) ||
		(Value.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pTotal > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotal += 4u + Value.Size;
	return XSSH_OK;
}



/* 验证 writer、容量和输入重叠，返回可安全提交的副本。 */
static xsshcode xsshMessagePrepare(
	xsshwriter* pWriter,
	size_t iTotal,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pCopy
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( (pWriter == NULL) || (pCopy == NULL) ||
		((pInputs == NULL) && (iInputCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		pInputs,
		iInputCount
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	*pCopy = Writer;
	return XSSH_OK;
}



/* 初始化 reader 并消费期望的消息号。 */
static xsshcode xsshMessageReader(
	xbytesview Payload,
	uint8 iExpected,
	xsshreader* pReader
)
{
	uint8 iMessage;
	xsshcode Code;

	if ( (pReader == NULL) || !xrtSshReaderInit(pReader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(pReader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return iMessage == iExpected ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 写入只有消息号的 payload。 */
static xsshcode xsshMessageWriteEmpty(
	xsshwriter* pWriter,
	uint8 iMessage
)
{
	xsshwriter Writer;
	xsshcode Code;

	Code = xsshMessagePrepare(pWriter, 1u, NULL, 0u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteByte(&Writer, iMessage) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取只有消息号的 payload。 */
static xsshcode xsshMessageReadEmpty(
	xbytesview Payload,
	uint8 iMessage
)
{
	xsshreader Reader;
	xsshcode Code;

	Code = xsshMessageReader(Payload, iMessage, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtSshReaderRemaining(&Reader) == 0u ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 返回 payload 的第一个消息号。 */
xsshcode xrtSshMessageType(xbytesview Payload, uint8* pMessage)
{
	xsshreader Reader;
	uint8 iMessage;
	xsshcode Code;

	if ( (pMessage == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pMessage = iMessage;
	return XSSH_OK;
}



/* 写入 NEWKEYS。 */
xsshcode xrtSshNewKeysWrite(xsshwriter* pWriter)
{
	return xsshMessageWriteEmpty(pWriter, XSSH_MSG_NEWKEYS);
}



/* 严格读取 NEWKEYS。 */
xsshcode xrtSshNewKeysRead(xbytesview Payload)
{
	return xsshMessageReadEmpty(Payload, XSSH_MSG_NEWKEYS);
}



/* 写入 disconnect reason 与诊断文本。 */
xsshcode xrtSshDisconnectWrite(
	xsshwriter* pWriter,
	uint32 iReason,
	xstrview Description,
	xstrview Language
)
{
	xbytesview arrInputs[2];
	xsshwriter Writer;
	size_t iTotal = 1u + 4u;
	xsshcode Code;

	arrInputs[0] = xsshMessageBytes(Description);
	arrInputs[1] = xsshMessageBytes(Language);
	if ( ((Code = xsshMessageAddString(
		arrInputs[0],
		&iTotal
	)) != XSSH_OK) || ((Code = xsshMessageAddString(
		arrInputs[1],
		&iTotal
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshMessagePrepare(pWriter, iTotal, arrInputs, 2u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_DISCONNECT) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iReason) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 disconnect payload。 */
xsshcode xrtSshDisconnectRead(
	xbytesview Payload,
	xsshdisconnect* pMessage
)
{
	xsshreader Reader;
	xsshdisconnect Message;
	xbytesview Description;
	xbytesview Language;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, XSSH_MSG_DISCONNECT, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(
		&Reader,
		&Message.Reason
	)) != XSSH_OK) || ((Code = xrtSshReadString(
		&Reader,
		&Description
	)) != XSSH_OK) || ((Code = xrtSshReadString(
		&Reader,
		&Language
	)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Message.Description = xsshMessageText(Description);
	Message.Language = xsshMessageText(Language);
	*pMessage = Message;
	return XSSH_OK;
}



/* 写入可忽略的任意二进制数据。 */
xsshcode xrtSshIgnoreWrite(xsshwriter* pWriter, xbytesview Data)
{
	xsshwriter Writer;
	size_t iTotal = 1u;
	xsshcode Code;

	Code = xsshMessageAddString(Data, &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshMessagePrepare(pWriter, iTotal, &Data, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_IGNORE) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Data) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 ignore payload。 */
xsshcode xrtSshIgnoreRead(xbytesview Payload, xsshignore* pMessage)
{
	xsshreader Reader;
	xsshignore Message;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, XSSH_MSG_IGNORE, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Message.Data);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pMessage = Message;
	return XSSH_OK;
}



/* 写入无法实现消息对应的入站序列号。 */
xsshcode xrtSshUnimplementedWrite(
	xsshwriter* pWriter,
	uint32 iSequence
)
{
	xsshwriter Writer;
	xsshcode Code;

	Code = xsshMessagePrepare(pWriter, 5u, NULL, 0u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_UNIMPLEMENTED) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iSequence) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 unimplemented 序列号。 */
xsshcode xrtSshUnimplementedRead(
	xbytesview Payload,
	uint32* pSequence
)
{
	xsshreader Reader;
	uint32 iSequence;
	xsshcode Code;

	if ( pSequence == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, XSSH_MSG_UNIMPLEMENTED, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iSequence);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pSequence = iSequence;
	return XSSH_OK;
}



/* 写入 debug 显示策略与诊断文本。 */
xsshcode xrtSshDebugWrite(
	xsshwriter* pWriter,
	bool bAlwaysDisplay,
	xstrview Message,
	xstrview Language
)
{
	xbytesview arrInputs[2];
	xsshwriter Writer;
	size_t iTotal = 2u;
	xsshcode Code;

	arrInputs[0] = xsshMessageBytes(Message);
	arrInputs[1] = xsshMessageBytes(Language);
	if ( ((Code = xsshMessageAddString(
		arrInputs[0],
		&iTotal
	)) != XSSH_OK) || ((Code = xsshMessageAddString(
		arrInputs[1],
		&iTotal
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshMessagePrepare(pWriter, iTotal, arrInputs, 2u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_DEBUG) != XSSH_OK) ||
		(xrtSshWriteBool(&Writer, bAlwaysDisplay) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 debug payload。 */
xsshcode xrtSshDebugRead(xbytesview Payload, xsshdebug* pMessage)
{
	xsshreader Reader;
	xsshdebug Message;
	xbytesview Text;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, XSSH_MSG_DEBUG, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadBool(&Reader, &Message.AlwaysDisplay);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Text);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Message.Message = xsshMessageText(Text);
	Code = xrtSshReadString(&Reader, &Text);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Message.Language = xsshMessageText(Text);
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pMessage = Message;
	return XSSH_OK;
}



/* 写入 request 或 accept 共用的 service payload。 */
static xsshcode xsshServiceWrite(
	xsshwriter* pWriter,
	uint8 iMessage,
	xstrview Service
)
{
	xbytesview Value = xsshMessageBytes(Service);
	xsshwriter Writer;
	size_t iTotal = 1u;
	xsshcode Code;

	if ( !xrtSshNameValid(Service) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageAddString(Value, &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshMessagePrepare(pWriter, iTotal, &Value, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, iMessage) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Value) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取并校验 request 或 accept 共用的 service payload。 */
static xsshcode xsshServiceRead(
	xbytesview Payload,
	uint8 iMessage,
	xsshservice* pMessage
)
{
	xsshreader Reader;
	xsshservice Message;
	xbytesview Value;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, iMessage, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Message.Name = xsshMessageText(Value);
	if ( !xrtSshNameValid(Message.Name) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pMessage = Message;
	return XSSH_OK;
}



/* 写入 service request。 */
xsshcode xrtSshServiceRequestWrite(
	xsshwriter* pWriter,
	xstrview Service
)
{
	return xsshServiceWrite(pWriter, XSSH_MSG_SERVICE_REQUEST, Service);
}



/* 读取 service request。 */
xsshcode xrtSshServiceRequestRead(
	xbytesview Payload,
	xsshservice* pMessage
)
{
	return xsshServiceRead(Payload, XSSH_MSG_SERVICE_REQUEST, pMessage);
}



/* 写入 service accept。 */
xsshcode xrtSshServiceAcceptWrite(
	xsshwriter* pWriter,
	xstrview Service
)
{
	return xsshServiceWrite(pWriter, XSSH_MSG_SERVICE_ACCEPT, Service);
}



/* 读取 service accept。 */
xsshcode xrtSshServiceAcceptRead(
	xbytesview Payload,
	xsshservice* pMessage
)
{
	return xsshServiceRead(Payload, XSSH_MSG_SERVICE_ACCEPT, pMessage);
}



/* 写入数组形式的 extension-info，值保持任意二进制。 */
xsshcode xrtSshExtInfoWrite(
	xsshwriter* pWriter,
	const xsshextension* pExtensions,
	size_t iCount
)
{
	xsshwriter Writer;
	size_t iTotal = 1u + 4u;
	size_t i;
	xsshcode Code;

	if ( (iCount > UINT32_MAX) ||
		(iCount > (SIZE_MAX / sizeof(*pExtensions))) ||
		((pExtensions == NULL) && (iCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtSshNameValid(pExtensions[i].Name) ) {
			return XSSH_ERROR_ARGUMENT;
		}
		Code = xsshMessageAddString(
			xsshMessageBytes(pExtensions[i].Name),
			&iTotal
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		Code = xsshMessageAddString(pExtensions[i].Value, &iTotal);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Code = xsshMessagePrepare(pWriter, iTotal, NULL, 0u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtMemRangesOverlap(
		Writer.Data + Writer.Size,
		iTotal,
		pExtensions,
		iCount * sizeof(*pExtensions)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( xrtMemRangesOverlap(
				Writer.Data + Writer.Size,
				iTotal,
				pExtensions[i].Name.Data,
				pExtensions[i].Name.Size
			) || xrtMemRangesOverlap(
				Writer.Data + Writer.Size,
				iTotal,
				pExtensions[i].Value.Data,
				pExtensions[i].Value.Size
			) ) {
			return XSSH_ERROR_ARGUMENT;
		}
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_EXT_INFO) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, (uint32)iCount) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( (xrtSshWriteString(
			&Writer,
			xsshMessageBytes(pExtensions[i].Name)
		) != XSSH_OK) || (xrtSshWriteString(
			&Writer,
			pExtensions[i].Value
		) != XSSH_OK) ) {
			return XSSH_ERROR_STATE;
		}
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 预验证完整 EXT_INFO 后初始化无分配迭代器。 */
xsshcode xrtSshExtInfoRead(
	xbytesview Payload,
	xsshextinfo* pExtInfo
)
{
	xsshreader Reader;
	xsshreader Items;
	xsshextinfo ExtInfo;
	xbytesview Name;
	xbytesview Value;
	uint32 i;
	xsshcode Code;

	if ( pExtInfo == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, XSSH_MSG_EXT_INFO, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &ExtInfo.Count);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (size_t)ExtInfo.Count >
		(xrtSshReaderRemaining(&Reader) / 8u) ) {
		return XSSH_NEED_MORE;
	}
	Items = Reader;
	for ( i = 0u; i < ExtInfo.Count; ++i ) {
		Code = xrtSshReadString(&Reader, &Name);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( !xrtSshNameValid(xsshMessageText(Name)) ) {
			return XSSH_ERROR_PROTOCOL;
		}
		Code = xrtSshReadString(&Reader, &Value);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	ExtInfo.Index = 0u;
	ExtInfo.Reader = Items;
	*pExtInfo = ExtInfo;
	return XSSH_OK;
}



/* 从已验证 EXT_INFO 中返回下一项。 */
bool xrtSshExtInfoNext(
	xsshextinfo* pExtInfo,
	xsshextension* pExtension
)
{
	xsshextinfo ExtInfo;
	xsshextension Extension;
	xbytesview Name;

	if ( (pExtInfo == NULL) || (pExtension == NULL) ||
		(pExtInfo->Index >= pExtInfo->Count) ) {
		return false;
	}
	ExtInfo = *pExtInfo;
	if ( (xrtSshReadString(&ExtInfo.Reader, &Name) != XSSH_OK) ||
		(xrtSshReadString(
			&ExtInfo.Reader,
			&Extension.Value
		) != XSSH_OK) ) {
		return false;
	}
	Extension.Name = xsshMessageText(Name);
	++ExtInfo.Index;
	*pExtInfo = ExtInfo;
	*pExtension = Extension;
	return true;
}



/* 写入 delayed compression 激活消息。 */
xsshcode xrtSshNewCompressWrite(xsshwriter* pWriter)
{
	return xsshMessageWriteEmpty(pWriter, XSSH_MSG_NEWCOMPRESS);
}



/* 严格读取 delayed compression 激活消息。 */
xsshcode xrtSshNewCompressRead(xbytesview Payload)
{
	return xsshMessageReadEmpty(Payload, XSSH_MSG_NEWCOMPRESS);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/transport/ssh_transport_rekey.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_TRANSPORT_REKEY)
#include <math.h>
#include <string.h>




#if defined(XSSH_FEATURE_TRANSPORT_REKEY)

/* 验证软包阈值不会超过不可取消的协议硬上限。 */
static bool xsshRekeyPolicyValid(const xsshrekeypolicy* pPolicy)
{
	if ( (pPolicy == NULL) || (pPolicy->HardPacketLimit == 0u) ||
		(pPolicy->HardPacketLimit > XSSH_REKEY_HARD_PACKET_LIMIT) ) {
		return false;
	}
	return ((pPolicy->SendPacketLimit == 0u) ||
		(pPolicy->SendPacketLimit <= pPolicy->HardPacketLimit)) &&
		((pPolicy->ReceivePacketLimit == 0u) ||
		(pPolicy->ReceivePacketLimit <= pPolicy->HardPacketLimit));
}



/* 计数使用饱和加法，异常大的调用值只会提前触发 rekey。 */
static uint64 xsshRekeyAdd(uint64 iLeft, uint64 iRight)
{
	return iRight > (UINT64_MAX - iLeft) ?
		UINT64_MAX : iLeft + iRight;
}



/* 比较启用的单项软阈值。 */
static bool xsshRekeyLimit(uint64 iValue, uint64 iLimit)
{
	return (iLimit != 0u) && (iValue >= iLimit);
}



/* 合并手动、时间和双向计数产生的当前决策。 */
static xsshrekeydecision xsshRekeyCurrent(
	const xsshrekeystate* pState,
	double Timer
)
{
	double iSendElapsed = Timer >= pState->SendStartedTimer ?
		Timer - pState->SendStartedTimer : 0u;
	double iReceiveElapsed = Timer >= pState->ReceiveStartedTimer ?
		Timer - pState->ReceiveStartedTimer : 0u;

	if ( (pState->Sent.Packets >= pState->Policy.HardPacketLimit) ||
		(pState->Received.Packets >= pState->Policy.HardPacketLimit) ) {
		return XSSH_REKEY_REQUIRED;
	}
	if ( pState->Requested ||
		(pState->Policy.TimeLimitMs != 0 && iSendElapsed >= (double)pState->Policy.TimeLimitMs / 1000.0) ||
		(pState->Policy.TimeLimitMs != 0 && iReceiveElapsed >= (double)pState->Policy.TimeLimitMs / 1000.0) ||
		xsshRekeyLimit(pState->Sent.Bytes, pState->Policy.ByteLimit) ||
		xsshRekeyLimit(pState->Received.Bytes, pState->Policy.ByteLimit) ||
		xsshRekeyLimit(pState->Sent.Blocks, pState->Policy.BlockLimit) ||
		xsshRekeyLimit(pState->Received.Blocks, pState->Policy.BlockLimit) ||
		xsshRekeyLimit(
			pState->Sent.Packets,
			pState->Policy.SendPacketLimit
		) || xsshRekeyLimit(
			pState->Received.Packets,
			pState->Policy.ReceivePacketLimit
		) ) {
		return XSSH_REKEY_RECOMMENDED;
	}
	return XSSH_REKEY_NONE;
}



/* 原子预留一个方向的下一包，并返回更新后的策略决策。 */
static xsshcode xsshRekeyReserve(
	xsshrekeystate* pState,
	xsshrekeycounter* pCounter,
	uint64 iWireBytes,
	uint64 iCipherBlocks,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	xsshrekeycounter Counter;
	xsshrekeydecision Decision;

	if ( (pState == NULL) || (pCounter == NULL) || (pDecision == NULL) ||
		!xsshRekeyPolicyValid(&pState->Policy) ||
		xrtMemRangesOverlap(
			pState,
			sizeof(*pState),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xsshRekeyCurrent(pState, Timer) == XSSH_REKEY_REQUIRED ) {
		*pDecision = XSSH_REKEY_REQUIRED;
		return XSSH_OK;
	}
	Counter = *pCounter;
	Counter.Packets += 1u;
	Counter.Bytes = xsshRekeyAdd(Counter.Bytes, iWireBytes);
	Counter.Blocks = xsshRekeyAdd(Counter.Blocks, iCipherBlocks);
	*pCounter = Counter;
	Decision = xsshRekeyCurrent(pState, Timer);
	if ( Decision == XSSH_REKEY_REQUIRED ) {
		Decision = XSSH_REKEY_RECOMMENDED;
	}
	*pDecision = Decision;
	return XSSH_OK;
}



/* 建立兼顾 RFC 推荐值和状态机提前量的默认策略。 */
void xrtSshRekeyPolicyInit(xsshrekeypolicy* pPolicy)
{
	if ( pPolicy == NULL ) {
		return;
	}
	pPolicy->ByteLimit = XSSH_REKEY_DEFAULT_BYTE_LIMIT;
	pPolicy->SendPacketLimit = XSSH_REKEY_DEFAULT_SEND_PACKET_LIMIT;
	pPolicy->ReceivePacketLimit = XSSH_REKEY_DEFAULT_RECEIVE_PACKET_LIMIT;
	pPolicy->BlockLimit = XSSH_REKEY_DEFAULT_BLOCK_LIMIT;
	pPolicy->TimeLimitMs = XSSH_REKEY_DEFAULT_TIME_LIMIT_MS;
	pPolicy->HardPacketLimit = XSSH_REKEY_HARD_PACKET_LIMIT;
}



/* 复制显式策略或默认策略，并开始第一代密钥计数。 */
bool xrtSshRekeyInit(
	xsshrekeystate* pState,
	const xsshrekeypolicy* pPolicy,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return false; }
	xsshrekeypolicy Policy;
	xsshrekeystate State;

	if ( pState == NULL ) {
		return false;
	}
	if ( pPolicy == NULL ) {
		xrtSshRekeyPolicyInit(&Policy);
		pPolicy = &Policy;
	}
	if ( !xsshRekeyPolicyValid(pPolicy) ) {
		return false;
	}
	memset(&State, 0, sizeof(State));
	State.Policy = *pPolicy;
	State.SendStartedTimer = Timer;
	State.ReceiveStartedTimer = Timer;
	*pState = State;
	return true;
}



/* 保留策略并重置新一代密钥的全部运行计数。 */
bool xrtSshRekeyReset(xsshrekeystate* pState, double Timer)
{
	if (!isfinite(Timer) || Timer < 0) { return false; }
	xsshrekeypolicy Policy;

	if ( (pState == NULL) || !xsshRekeyPolicyValid(&pState->Policy) ) {
		return false;
	}
	Policy = pState->Policy;
	memset(pState, 0, sizeof(*pState));
	pState->Policy = Policy;
	pState->SendStartedTimer = Timer;
	pState->ReceiveStartedTimer = Timer;
	return true;
}



/* 写密钥提交只开始新的发送代，不影响已先行收到的新密钥数据。 */
bool xrtSshRekeyResetSend(xsshrekeystate* pState, double Timer)
{
	if (!isfinite(Timer) || Timer < 0) { return false; }
	if ( (pState == NULL) || !xsshRekeyPolicyValid(&pState->Policy) ) {
		return false;
	}
	memset(&pState->Sent, 0, sizeof(pState->Sent));
	pState->SendStartedTimer = Timer;
	return true;
}



/* 读密钥提交只开始新的接收代，不影响已先行发送的新密钥数据。 */
bool xrtSshRekeyResetReceive(xsshrekeystate* pState, double Timer)
{
	if (!isfinite(Timer) || Timer < 0) { return false; }
	if ( (pState == NULL) || !xsshRekeyPolicyValid(&pState->Policy) ) {
		return false;
	}
	memset(&pState->Received, 0, sizeof(pState->Received));
	pState->ReceiveStartedTimer = Timer;
	return true;
}



/* KEX 完成只清除请求标志，不能抹掉已经使用的新代额度。 */
bool xrtSshRekeyComplete(xsshrekeystate* pState)
{
	if ( (pState == NULL) || !xsshRekeyPolicyValid(&pState->Policy) ) {
		return false;
	}
	pState->Requested = false;
	return true;
}



/* 记录应用、算法或对端触发的主动 rekey 请求。 */
bool xrtSshRekeyRequest(xsshrekeystate* pState)
{
	if ( (pState == NULL) || !xsshRekeyPolicyValid(&pState->Policy) ) {
		return false;
	}
	pState->Requested = true;
	return true;
}



/* 读取当前决策，不改变计数状态。 */
xsshcode xrtSshRekeyCheck(
	const xsshrekeystate* pState,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	if ( (pState == NULL) || (pDecision == NULL) ||
		!xsshRekeyPolicyValid(&pState->Policy) ||
		xrtMemRangesOverlap(
			pState,
			sizeof(*pState),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pDecision = xsshRekeyCurrent(pState, Timer);
	return XSSH_OK;
}



/* 预留发送方向下一包。 */
xsshcode xrtSshRekeyReserveSend(
	xsshrekeystate* pState,
	uint64 iWireBytes,
	uint64 iCipherBlocks,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	if ( pState == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xsshRekeyReserve(
		pState,
		&pState->Sent,
		iWireBytes,
		iCipherBlocks,
		Timer,
		pDecision
	);
}



/* 预留接收方向下一包。 */
xsshcode xrtSshRekeyReserveReceive(
	xsshrekeystate* pState,
	uint64 iWireBytes,
	uint64 iCipherBlocks,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	if ( pState == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xsshRekeyReserve(
		pState,
		&pState->Received,
		iWireBytes,
		iCipherBlocks,
		Timer,
		pDecision
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/transport/ssh_transport_state.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_TRANSPORT_STATE)
#include <string.h>




#if defined(XSSH_FEATURE_TRANSPORT_STATE)

#define XSSH_TRANSPORT_RULES_GUARD UINT32_C(0x5853524c)
#define XSSH_TRANSPORT_STATE_GUARD UINT32_C(0x58535354)



/* 判断 endpoint 角色属于公开枚举。 */
static bool xsshTransportRoleValid(xsshrole Role)
{
	return (Role == XSSH_ROLE_CLIENT) || (Role == XSSH_ROLE_SERVER);
}



/* 判断消息方向属于公开枚举。 */
static bool xsshTransportDirectionValid(xsshtransportdirection Direction)
{
	return (Direction == XSSH_TRANSPORT_LOCAL) ||
		(Direction == XSSH_TRANSPORT_PEER);
}



/* 判断 KEX 方法规则已初始化。 */
static bool xsshTransportRulesValid(const xsshtransportkexrules* pRules)
{
	return (pRules != NULL) &&
		(pRules->Guard == XSSH_TRANSPORT_RULES_GUARD);
}



/* 判断 transport 状态可继续使用。 */
static bool xsshTransportStateValid(const xsshtransportstate* pState)
{
	return (pState != NULL) &&
		(pState->Guard == XSSH_TRANSPORT_STATE_GUARD) &&
		xsshTransportRoleValid(pState->Role) &&
		(pState->Phase >= XSSH_TRANSPORT_IDENTIFICATION) &&
		(pState->Phase <= XSSH_TRANSPORT_CLOSED);
}



/* 本端非法调用返回状态错误，对端非法线路状态返回协议错误。 */
static xsshcode xsshTransportDirectionError(
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		XSSH_ERROR_STATE : XSSH_ERROR_PROTOCOL;
}



/* 返回指定方向已经提交的 packet 数。 */
static uint64 xsshTransportPackets(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalPackets : pState->PeerPackets;
}



/* 增加指定方向 packet 数；调用方已经完成溢出检查。 */
static void xsshTransportPacketIncrement(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pState->LocalPackets++;
	} else {
		pState->PeerPackets++;
	}
}



/* 检查计数器以及 strict-kex 初始阶段不得发生 uint32 序列回绕。 */
static xsshcode xsshTransportPacketCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	uint64 iPackets = xsshTransportPackets(pState, Direction);

	if ( iPackets == UINT64_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( pState->Strict && (pState->KexCount == 0u) &&
		(iPackets >= (uint64)UINT32_MAX) ) {
		return xsshTransportDirectionError(Direction);
	}
	return XSSH_OK;
}



/* 判断对应方向已经提交 KEXINIT 但尚未提交 NEWKEYS。 */
static bool xsshTransportDirectionKexActive(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		(pState->LocalKexInit && !pState->LocalNewKeys) :
		(pState->PeerKexInit && !pState->PeerNewKeys);
}



/* 返回指定方向是否已经提交 KEXINIT。 */
static bool xsshTransportDirectionKexInit(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalKexInit : pState->PeerKexInit;
}



/* 返回指定方向是否已经提交 NEWKEYS。 */
static bool xsshTransportDirectionNewKeys(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalNewKeys : pState->PeerNewKeys;
}



/* 判断第二次 EXT_INFO 后是否正在等待紧邻的 USERAUTH_SUCCESS。 */
static bool xsshTransportAuthSuccessPending(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalAuthSuccessPending :
		pState->PeerAuthSuccessPending;
}



/* 判断指定 server 方向已经提交 USERAUTH_SUCCESS。 */
static bool xsshTransportAuthSuccess(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalAuthSuccess : pState->PeerAuthSuccess;
}



/* 返回指定方向的 KEX 方法剩余额度。 */
static const uint8* xsshTransportKexRemainingConst(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalKexRemaining : pState->PeerKexRemaining;
}



/* 返回指定方向可修改的 KEX 方法剩余额度。 */
static uint8* xsshTransportKexRemaining(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalKexRemaining : pState->PeerKexRemaining;
}



/* 判断指定方向的全部 KEX 方法额度均已消费。 */
static bool xsshTransportKexComplete(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	const uint8* pRemaining = xsshTransportKexRemainingConst(
		pState,
		Direction
	);
	size_t i;

	for ( i = 0u; i < XSSH_KEX_METHOD_COUNT; ++i ) {
		if ( pRemaining[i] != 0u ) {
			return false;
		}
	}
	return true;
}



/* 判断指定方向是否仍欠一包 first_kex_packet_follows 猜测消息。 */
static bool xsshTransportGuessPending(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		(pState->LocalGuessExpected && !pState->LocalGuessSeen) :
		(pState->PeerGuessExpected && !pState->PeerGuessSeen);
}



/* 消费一项已经验证存在的 KEX 方法额度。 */
static void xsshTransportKexConsume(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
)
{
	xsshTransportKexRemaining(pState, Direction)[
		(size_t)iMessage - XSSH_KEX_METHOD_MIN
	]--;
}



/* 关闭指定方向 NEWKEYS 后的第一次 EXT_INFO 机会。 */
static void xsshTransportFirstExtClose(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pState->LocalFirstExtOpen = false;
	} else {
		pState->PeerFirstExtOpen = false;
	}
}



/* 判断指定方向是否具备发送或接收应用消息的密钥边界。 */
static bool xsshTransportCanApplicationInternal(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	if ( (pState->Phase != XSSH_TRANSPORT_KEY_EXCHANGE) &&
		(pState->Phase != XSSH_TRANSPORT_OPEN) ) {
		return false;
	}
	if ( pState->Phase == XSSH_TRANSPORT_OPEN ) {
		return true;
	}
	if ( pState->KexCount == 0u ) {
		return xsshTransportDirectionNewKeys(pState, Direction);
	}
	return !xsshTransportDirectionKexInit(pState, Direction) ||
		xsshTransportDirectionNewKeys(pState, Direction);
}



/* 开始下一代 rekey，并保留连接级 strict 与 EXT_INFO 能力。 */
static void xsshTransportBeginRekey(xsshtransportstate* pState)
{
	memset(
		pState->LocalKexRemaining,
		0,
		sizeof(pState->LocalKexRemaining)
	);
	memset(
		pState->PeerKexRemaining,
		0,
		sizeof(pState->PeerKexRemaining)
	);
	pState->LocalKexInit = false;
	pState->PeerKexInit = false;
	pState->LocalNewKeys = false;
	pState->PeerNewKeys = false;
	pState->KexConfigured = false;
	pState->LocalGuessMessage = 0u;
	pState->PeerGuessMessage = 0u;
	pState->LocalGuessExpected = false;
	pState->PeerGuessExpected = false;
	pState->LocalGuessSeen = false;
	pState->PeerGuessSeen = false;
	pState->LocalGuessSkip = false;
	pState->PeerGuessSkip = false;
	pState->LocalStrictViolation = false;
	pState->PeerStrictViolation = false;
	pState->Phase = XSSH_TRANSPORT_KEY_EXCHANGE;
}



/* 检查 EXT_INFO 是否处于首次或 server 认证成功前的第二次机会。 */
static xsshcode xsshTransportExtInfoCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	bool bEnabled = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->SendExtInfo : pState->AcceptExtInfo;
	bool bFirstOpen = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalFirstExtOpen : pState->PeerFirstExtOpen;
	bool bSecondUsed = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalSecondExtUsed : pState->PeerSecondExtUsed;
	bool bServerDirection = Direction == XSSH_TRANSPORT_LOCAL ?
		(pState->Role == XSSH_ROLE_SERVER) :
		(pState->Role == XSSH_ROLE_CLIENT);

	if ( !bEnabled ||
		!xsshTransportCanApplicationInternal(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( bFirstOpen ) {
		return XSSH_OK;
	}
	if ( !bServerDirection || bSecondUsed ||
		(pState->KexCount == 0u) ||
		xsshTransportAuthSuccessPending(pState, Direction) ||
		xsshTransportAuthSuccess(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	return XSSH_OK;
}



/* 提交 EXT_INFO 的首次或第二次机会状态。 */
static void xsshTransportExtInfoCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	bool bFirstOpen = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalFirstExtOpen : pState->PeerFirstExtOpen;

	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		if ( bFirstOpen ) {
			pState->LocalFirstExtOpen = false;
			pState->LocalFirstExtUsed = true;
		} else {
			pState->LocalSecondExtUsed = true;
			pState->LocalAuthSuccessPending = true;
		}
	} else if ( bFirstOpen ) {
		pState->PeerFirstExtOpen = false;
		pState->PeerFirstExtUsed = true;
	} else {
		pState->PeerSecondExtUsed = true;
		pState->PeerAuthSuccessPending = true;
	}
}



/* 初始化空规则并发布 guard。 */
bool xrtSshTransportKexRulesInit(xsshtransportkexrules* pRules)
{
	xsshtransportkexrules Rules;

	if ( !xrtMemRangeValid(pRules, sizeof(*pRules)) ) {
		return false;
	}
	memset(&Rules, 0, sizeof(Rules));
	Rules.Guard = XSSH_TRANSPORT_RULES_GUARD;
	*pRules = Rules;
	return true;
}



/* 设置方法消息的精确方向额度。 */
bool xrtSshTransportKexRuleSet(
	xsshtransportkexrules* pRules,
	xsshtransportdirection Direction,
	uint8 iMessage,
	uint8 iCount
)
{
	size_t iIndex;

	if ( !xsshTransportRulesValid(pRules) ||
		!xsshTransportDirectionValid(Direction) ||
		(iMessage < XSSH_KEX_METHOD_MIN) ||
		(iMessage > XSSH_KEX_METHOD_MAX) ) {
		return false;
	}
	iIndex = (size_t)iMessage - XSSH_KEX_METHOD_MIN;
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pRules->Local[iIndex] = iCount;
	} else {
		pRules->Peer[iIndex] = iCount;
	}
	return true;
}



/* 初始化 identification 阶段。 */
bool xrtSshTransportStateInit(
	xsshtransportstate* pState,
	xsshrole Role
)
{
	xsshtransportstate State;

	if ( !xrtMemRangeValid(pState, sizeof(*pState)) ||
		!xsshTransportRoleValid(Role) ) {
		return false;
	}
	memset(&State, 0, sizeof(State));
	State.Role = Role;
	State.Phase = XSSH_TRANSPORT_IDENTIFICATION;
	State.Guard = XSSH_TRANSPORT_STATE_GUARD;
	*pState = State;
	return true;
}



/* 清除状态对象本身。 */
void xrtSshTransportStateClear(xsshtransportstate* pState)
{
	if ( pState != NULL ) {
		memset(pState, 0, sizeof(*pState));
	}
}



/* 提交一个 identification 方向，双方完成后进入首次 KEX。 */
xsshcode xrtSshTransportIdentificationCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	bool* pCommitted;

	if ( !xsshTransportStateValid(pState) ||
		!xsshTransportDirectionValid(Direction) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pState->Phase != XSSH_TRANSPORT_IDENTIFICATION ) {
		return xsshTransportDirectionError(Direction);
	}
	pCommitted = Direction == XSSH_TRANSPORT_LOCAL ?
		&pState->LocalIdentification : &pState->PeerIdentification;
	if ( *pCommitted ) {
		return xsshTransportDirectionError(Direction);
	}
	*pCommitted = true;
	if ( pState->LocalIdentification && pState->PeerIdentification ) {
		pState->Phase = XSSH_TRANSPORT_KEY_EXCHANGE;
	}
	return XSSH_OK;
}



/* 查询方向性应用消息能力。 */
bool xrtSshTransportCanApplication(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return xsshTransportStateValid(pState) &&
		xsshTransportDirectionValid(Direction) &&
		xsshTransportCanApplicationInternal(pState, Direction);
}



/* 对端先发起 rekey 时提示本端回复。 */
bool xrtSshTransportKexReplyNeeded(const xsshtransportstate* pState)
{
	return xsshTransportStateValid(pState) &&
		(pState->Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		pState->PeerKexInit && !pState->LocalKexInit;
}



/* 检查指定方向 KEXINIT 边界。 */
xsshcode xrtSshTransportKexInitCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	if ( !xsshTransportStateValid(pState) ||
		!xsshTransportDirectionValid(Direction) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((pState->Phase != XSSH_TRANSPORT_KEY_EXCHANGE) &&
		(pState->Phase != XSSH_TRANSPORT_OPEN)) ||
		xsshTransportAuthSuccessPending(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( (pState->Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		(xsshTransportDirectionKexInit(pState, Direction) ||
		xsshTransportDirectionNewKeys(pState, Direction)) ) {
		return xsshTransportDirectionError(Direction);
	}
	return xsshTransportPacketCheck(pState, Direction);
}



/* 提交 KEXINIT，并建立 guessed-packet 期望。 */
xsshcode xrtSshTransportKexInitCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	bool bFirstKexPacketFollows
)
{
	xsshcode Code = xrtSshTransportKexInitCheck(pState, Direction);
	uint64 iOrdinal;

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( pState->Phase == XSSH_TRANSPORT_OPEN ) {
		xsshTransportBeginRekey(pState);
	}
	xsshTransportFirstExtClose(pState, Direction);
	iOrdinal = xsshTransportPackets(pState, Direction);
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pState->LocalKexInit = true;
		pState->LocalGuessExpected = bFirstKexPacketFollows;
		pState->LocalKexInitOrdinal = iOrdinal;
	} else {
		pState->PeerKexInit = true;
		pState->PeerGuessExpected = bFirstKexPacketFollows;
		pState->PeerKexInitOrdinal = iOrdinal;
	}
	xsshTransportPacketIncrement(pState, Direction);
	return XSSH_OK;
}



/* 验证并消费配置前已经提交的猜测包。 */
static xsshcode xsshTransportConfigureGuess(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	bool bSkip
)
{
	bool bSeen = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalGuessSeen : pState->PeerGuessSeen;
	uint8 iMessage = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalGuessMessage : pState->PeerGuessMessage;
	uint8* pRemaining;

	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pState->LocalGuessSkip = bSkip;
	} else {
		pState->PeerGuessSkip = bSkip;
	}
	if ( !bSeen || bSkip ) {
		return XSSH_OK;
	}
	pRemaining = xsshTransportKexRemaining(pState, Direction);
	if ( pRemaining[(size_t)iMessage - XSSH_KEX_METHOD_MIN] == 0u ) {
		return xsshTransportDirectionError(Direction);
	}
	xsshTransportKexConsume(pState, Direction, iMessage);
	return XSSH_OK;
}



/* 比较调用方协商结果与按 client 优先级重新计算的结果。 */
static bool xsshTransportTextEqual(xstrview Left, xstrview Right)
{
	if ( ((Left.Data == NULL) && (Left.Size != 0u)) ||
		((Right.Data == NULL) && (Right.Size != 0u)) ) {
		return false;
	}
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 比较全部双向 KEX 协商字段。 */
static bool xsshTransportNegotiationEqual(
	const xsshkexnegotiation* pLeft,
	const xsshkexnegotiation* pRight
)
{
	return xsshTransportTextEqual(
		pLeft->KexAlgorithm,
		pRight->KexAlgorithm
	) && xsshTransportTextEqual(
		pLeft->ServerHostKeyAlgorithm,
		pRight->ServerHostKeyAlgorithm
	) && xsshTransportTextEqual(
		pLeft->CipherClientToServer,
		pRight->CipherClientToServer
	) && xsshTransportTextEqual(
		pLeft->CipherServerToClient,
		pRight->CipherServerToClient
	) && xsshTransportTextEqual(
		pLeft->MacClientToServer,
		pRight->MacClientToServer
	) && xsshTransportTextEqual(
		pLeft->MacServerToClient,
		pRight->MacServerToClient
	) && xsshTransportTextEqual(
		pLeft->CompressionClientToServer,
		pRight->CompressionClientToServer
	) && xsshTransportTextEqual(
		pLeft->CompressionServerToClient,
		pRight->CompressionServerToClient
	);
}



/* 提交协商、严格模式和本代方法额度。 */
xsshcode xrtSshTransportKexConfigure(
	xsshtransportstate* pState,
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	const xsshtransportkexrules* pRules
)
{
	xsshtransportstate State;
	xsshkexfeatures Features;
	xsshkexnegotiation Expected;
	bool bLocalSkip;
	bool bPeerSkip;
	bool bInitial;
	xsshcode Code;

	if ( !xsshTransportStateValid(pState) ||
		!xrtMemRangeValid(pLocal, sizeof(*pLocal)) ||
		!xrtMemRangeValid(pPeer, sizeof(*pPeer)) ||
		!xrtMemRangeValid(pNegotiation, sizeof(*pNegotiation)) ||
		!xsshTransportRulesValid(pRules) ||
		xrtMemRangesOverlap(pState, sizeof(*pState), pLocal, sizeof(*pLocal)) ||
		xrtMemRangesOverlap(pState, sizeof(*pState), pPeer, sizeof(*pPeer)) ||
		xrtMemRangesOverlap(
			pState,
			sizeof(*pState),
			pNegotiation,
			sizeof(*pNegotiation)
		) || xrtMemRangesOverlap(
			pState,
			sizeof(*pState),
			pRules,
			sizeof(*pRules)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pState->Phase != XSSH_TRANSPORT_KEY_EXCHANGE) ||
		!pState->LocalKexInit || !pState->PeerKexInit ||
		pState->KexConfigured ) {
		return XSSH_ERROR_STATE;
	}
	if ( pLocal->FirstKexPacketFollows != pState->LocalGuessExpected ) {
		return XSSH_ERROR_STATE;
	}
	if ( pPeer->FirstKexPacketFollows != pState->PeerGuessExpected ) {
		return XSSH_ERROR_PROTOCOL;
	}
	bInitial = pState->KexCount == 0u;
	memset(&Features, 0, sizeof(Features));
	if ( bInitial ) {
		Code = xrtSshKexFeatures(
			pLocal,
			pPeer,
			pState->Role,
			true,
			&Features
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	memset(&Expected, 0, sizeof(Expected));
	Code = pState->Role == XSSH_ROLE_CLIENT ?
		xrtSshKexNegotiate(pLocal, pPeer, &Expected) :
		xrtSshKexNegotiate(pPeer, pLocal, &Expected);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshTransportNegotiationEqual(&Expected, pNegotiation) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshKexGuessSkip(pLocal, pNegotiation, &bLocalSkip);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshKexGuessSkip(pPeer, pNegotiation, &bPeerSkip);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Features.Strict &&
		((pState->LocalKexInitOrdinal != 0u) ||
		 (pState->PeerKexInitOrdinal != 0u) ||
		 pState->LocalStrictViolation || pState->PeerStrictViolation ||
		 (pState->LocalPackets > (uint64)UINT32_MAX) ||
		 (pState->PeerPackets > (uint64)UINT32_MAX)) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	State = *pState;
	memcpy(
		State.LocalKexRemaining,
		pRules->Local,
		sizeof(State.LocalKexRemaining)
	);
	memcpy(
		State.PeerKexRemaining,
		pRules->Peer,
		sizeof(State.PeerKexRemaining)
	);
	if ( bInitial ) {
		State.Strict = Features.Strict;
		State.AcceptExtInfo = Features.AcceptExtInfo;
		State.SendExtInfo = Features.SendExtInfo;
	}
	Code = xsshTransportConfigureGuess(
		&State,
		XSSH_TRANSPORT_LOCAL,
		bLocalSkip
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshTransportConfigureGuess(
		&State,
		XSSH_TRANSPORT_PEER,
		bPeerSkip
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	State.KexConfigured = true;
	*pState = State;
	return XSSH_OK;
}



/* 检查 KEX 方法消息或 guessed-packet。 */
static xsshcode xsshTransportKexMessageCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
)
{
	const uint8* pRemaining;

	if ( !xsshTransportDirectionKexActive(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( !pState->KexConfigured ) {
		return xsshTransportGuessPending(pState, Direction) ?
			XSSH_OK : xsshTransportDirectionError(Direction);
	}
	if ( xsshTransportGuessPending(pState, Direction) ) {
		return XSSH_OK;
	}
	pRemaining = xsshTransportKexRemainingConst(pState, Direction);
	return pRemaining[(size_t)iMessage - XSSH_KEX_METHOD_MIN] != 0u ?
		XSSH_OK : xsshTransportDirectionError(Direction);
}



/* 检查普通消息的方向、KEX 范围和 EXT_INFO 时机。 */
xsshcode xrtSshTransportMessageCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
)
{
	bool bKexActive;
	bool bCanApplication;
	xsshcode Code;

	if ( !xsshTransportStateValid(pState) ||
		!xsshTransportDirectionValid(Direction) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pState->Phase != XSSH_TRANSPORT_KEY_EXCHANGE) &&
		(pState->Phase != XSSH_TRANSPORT_OPEN) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( (iMessage == XSSH_MSG_KEXINIT) ||
		(iMessage == XSSH_MSG_NEWKEYS) ) {
		return xsshTransportDirectionError(Direction);
	}
	Code = xsshTransportPacketCheck(pState, Direction);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage == XSSH_MSG_DISCONNECT ) {
		return XSSH_OK;
	}
	if ( xsshTransportAuthSuccessPending(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( iMessage == XSSH_MSG_EXT_INFO ) {
		return xsshTransportExtInfoCheck(pState, Direction);
	}
	bKexActive = xsshTransportDirectionKexActive(pState, Direction);
	bCanApplication = xsshTransportCanApplicationInternal(
		pState,
		Direction
	);
	if ( (iMessage >= XSSH_KEX_METHOD_MIN) &&
		(iMessage <= XSSH_KEX_METHOD_MAX) ) {
		return bKexActive ?
			xsshTransportKexMessageCheck(pState, Direction, iMessage) :
			xsshTransportDirectionError(Direction);
	}
	if ( (iMessage >= 20u) && (iMessage <= 29u) ) {
		if ( bKexActive &&
			!(pState->Strict && (pState->KexCount == 0u)) ) {
			return XSSH_OK;
		}
		return xsshTransportDirectionError(Direction);
	}
	if ( (iMessage == XSSH_MSG_SERVICE_REQUEST) ||
		(iMessage == XSSH_MSG_SERVICE_ACCEPT) ||
		(iMessage == XSSH_MSG_NEWCOMPRESS) ) {
		return bCanApplication ?
			XSSH_OK : xsshTransportDirectionError(Direction);
	}
	if ( bKexActive ) {
		if ( pState->Strict && (pState->KexCount == 0u) ) {
			return xsshTransportDirectionError(Direction);
		}
		return ((iMessage >= 1u) && (iMessage <= 19u)) ?
			XSSH_OK : xsshTransportDirectionError(Direction);
	}
	if ( bCanApplication ) {
		return XSSH_OK;
	}
	return ((iMessage >= 1u) && (iMessage <= 19u)) ?
		XSSH_OK : xsshTransportDirectionError(Direction);
}



/* 提交 KEX 方法消息并区分猜测包是否计入本代额度。 */
static void xsshTransportKexMessageCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
)
{
	bool bGuessPending = xsshTransportGuessPending(pState, Direction);
	bool bSkip = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalGuessSkip : pState->PeerGuessSkip;

	if ( bGuessPending ) {
		if ( Direction == XSSH_TRANSPORT_LOCAL ) {
			pState->LocalGuessSeen = true;
			pState->LocalGuessMessage = iMessage;
		} else {
			pState->PeerGuessSeen = true;
			pState->PeerGuessMessage = iMessage;
		}
		if ( pState->KexConfigured && !bSkip ) {
			xsshTransportKexConsume(pState, Direction, iMessage);
		}
		return;
	}
	xsshTransportKexConsume(pState, Direction, iMessage);
}



/* 提交普通消息并推进机会窗口、猜测包与 packet 计数。 */
xsshcode xrtSshTransportMessageCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
)
{
	xsshcode Code = xrtSshTransportMessageCheck(
		pState,
		Direction,
		iMessage
	);
	bool bKexMethod;

	if ( Code != XSSH_OK ) {
		return Code;
	}
	bKexMethod = (iMessage >= XSSH_KEX_METHOD_MIN) &&
		(iMessage <= XSSH_KEX_METHOD_MAX) &&
		xsshTransportDirectionKexActive(pState, Direction);
	if ( (pState->KexCount == 0u) && !pState->KexConfigured &&
		!bKexMethod && (iMessage != XSSH_MSG_DISCONNECT) ) {
		if ( Direction == XSSH_TRANSPORT_LOCAL ) {
			pState->LocalStrictViolation = true;
		} else {
			pState->PeerStrictViolation = true;
		}
	}
	if ( iMessage == XSSH_MSG_EXT_INFO ) {
		xsshTransportExtInfoCommit(pState, Direction);
	} else {
		xsshTransportFirstExtClose(pState, Direction);
	}
	if ( bKexMethod ) {
		xsshTransportKexMessageCommit(pState, Direction, iMessage);
	}
	xsshTransportPacketIncrement(pState, Direction);
	if ( iMessage == XSSH_MSG_DISCONNECT ) {
		pState->Phase = XSSH_TRANSPORT_CLOSING;
	}
	return XSSH_OK;
}



/* 检查 guessed packet 和全部 KEX 方法额度都已处理。 */
xsshcode xrtSshTransportNewKeysCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	xsshcode Code;

	if ( !xsshTransportStateValid(pState) ||
		!xsshTransportDirectionValid(Direction) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pState->Phase != XSSH_TRANSPORT_KEY_EXCHANGE) ||
		!xsshTransportDirectionKexActive(pState, Direction) ||
		!pState->KexConfigured ||
		xsshTransportGuessPending(pState, Direction) ||
		!xsshTransportKexComplete(pState, Direction) ||
		xsshTransportAuthSuccessPending(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( (pState->KexCount == UINT64_MAX) &&
		(Direction == XSSH_TRANSPORT_LOCAL ?
			pState->PeerNewKeys : pState->LocalNewKeys) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xsshTransportPacketCheck(pState, Direction);
	return Code;
}



/* 提交单方向 NEWKEYS，并在双方完成时结束本代 KEX。 */
xsshcode xrtSshTransportNewKeysCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint32* pActions
)
{
	xsshtransportstate State;
	uint32 iActions = XSSH_TRANSPORT_ACTION_ACTIVATE_KEYS;
	bool bInitial;
	xsshcode Code;

	if ( !xrtMemRangeValid(pActions, sizeof(*pActions)) ||
		(xsshTransportStateValid(pState) && xrtMemRangesOverlap(
			pState,
			sizeof(*pState),
			pActions,
			sizeof(*pActions)
		)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshTransportNewKeysCheck(pState, Direction);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	State = *pState;
	bInitial = State.KexCount == 0u;
	xsshTransportPacketIncrement(&State, Direction);
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		State.LocalNewKeys = true;
		if ( bInitial && State.SendExtInfo ) {
			State.LocalFirstExtOpen = true;
		}
	} else {
		State.PeerNewKeys = true;
		if ( bInitial && State.AcceptExtInfo ) {
			State.PeerFirstExtOpen = true;
		}
	}
	if ( State.Strict ) {
		iActions |= XSSH_TRANSPORT_ACTION_RESET_SEQUENCE;
	}
	if ( State.LocalNewKeys && State.PeerNewKeys ) {
		State.KexCount++;
		State.Phase = XSSH_TRANSPORT_OPEN;
		iActions |= XSSH_TRANSPORT_ACTION_KEX_COMPLETE;
	}
	*pState = State;
	*pActions = iActions;
	return XSSH_OK;
}



/* 检查 server 认证成功方向和当前密钥边界。 */
xsshcode xrtSshTransportAuthSuccessCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	bool bServerDirection;

	if ( !xsshTransportStateValid(pState) ||
		!xsshTransportDirectionValid(Direction) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	bServerDirection = Direction == XSSH_TRANSPORT_LOCAL ?
		(pState->Role == XSSH_ROLE_SERVER) :
		(pState->Role == XSSH_ROLE_CLIENT);
	if ( !bServerDirection ||
		!xsshTransportCanApplicationInternal(pState, Direction) ||
		xsshTransportAuthSuccess(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	return xsshTransportPacketCheck(pState, Direction);
}



/* 提交 USERAUTH_SUCCESS，并完成第二次 EXT_INFO 的邻接约束。 */
xsshcode xrtSshTransportAuthSuccessCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	xsshcode Code = xrtSshTransportAuthSuccessCheck(pState, Direction);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	xsshTransportFirstExtClose(pState, Direction);
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pState->LocalAuthSuccessPending = false;
		pState->LocalAuthSuccess = true;
	} else {
		pState->PeerAuthSuccessPending = false;
		pState->PeerAuthSuccess = true;
	}
	xsshTransportPacketIncrement(pState, Direction);
	return XSSH_OK;
}



/* 标记承载连接已经关闭。 */
void xrtSshTransportClose(xsshtransportstate* pState)
{
	if ( xsshTransportStateValid(pState) ) {
		pState->Phase = XSSH_TRANSPORT_CLOSED;
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/transport/ssh_transport_core.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_TRANSPORT_CORE)
#include <math.h>
#include <string.h>




#if defined(XSSH_FEATURE_TRANSPORT_CORE)

#define XSSH_TRANSPORT_CORE_GUARD UINT32_C(0x53544352)



/* 验证 core 自身和唯一写事务的一致性。 */
static bool xsshTransportCoreValid(const xsshtransportcore* pCore)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		(pCore->Guard == XSSH_TRANSPORT_CORE_GUARD) &&
		(pCore->Write.Active == pCore->Codec.WritePending);
}



/* 清除不拥有外部资源的待提交描述。 */
static void xsshTransportPendingClear(xsshtransportpending* pPending)
{
	memset(pPending, 0, sizeof(*pPending));
}



/* 从完整 payload 分类状态机需要特殊提交的消息。 */
static xsshcode xsshTransportPendingRead(
	xbytesview Payload,
	xsshtransportpending* pPending
)
{
	xsshtransportpending Pending;
	xsshkexinit KexInit;
	xsshcode Code;

	memset(&Pending, 0, sizeof(Pending));
	Code = xrtSshMessageType(Payload, &Pending.Message);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Pending.Message == XSSH_MSG_KEXINIT ) {
		Code = xrtSshKexInitRead(Payload, &KexInit);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		Pending.Kind = XSSH_TRANSPORT_PACKET_KEXINIT;
		Pending.FirstKexPacketFollows = KexInit.FirstKexPacketFollows;
	} else if ( Pending.Message == XSSH_MSG_NEWKEYS ) {
		Code = xrtSshNewKeysRead(Payload);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		Pending.Kind = XSSH_TRANSPORT_PACKET_NEWKEYS;
	} else if ( Pending.Message == XSSH_MSG_USERAUTH_SUCCESS ) {
		if ( Payload.Size != 1u ) {
			return XSSH_ERROR_PROTOCOL;
		}
		Pending.Kind = XSSH_TRANSPORT_PACKET_AUTH_SUCCESS;
	} else {
		Pending.Kind = XSSH_TRANSPORT_PACKET_MESSAGE;
	}
	Pending.Active = true;
	*pPending = Pending;
	return XSSH_OK;
}



/* 按消息类别执行不改变状态的方向检查。 */
static xsshcode xsshTransportCoreStateCheck(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	const xsshtransportpending* pPending
)
{
	switch ( pPending->Kind ) {
		case XSSH_TRANSPORT_PACKET_KEXINIT:
			return xrtSshTransportKexInitCheck(&pCore->State, Direction);
		case XSSH_TRANSPORT_PACKET_NEWKEYS:
			return xrtSshTransportNewKeysCheck(&pCore->State, Direction);
		case XSSH_TRANSPORT_PACKET_AUTH_SUCCESS:
			return xrtSshTransportAuthSuccessCheck(
				&pCore->State,
				Direction
			);
		case XSSH_TRANSPORT_PACKET_MESSAGE:
			return xrtSshTransportMessageCheck(
				&pCore->State,
				Direction,
				pPending->Message
			);
		default:
			return XSSH_ERROR_STATE;
	}
}



/* 在可靠边界按消息类别推进状态，并发布 NEWKEYS 动作。 */
static xsshcode xsshTransportCoreStateCommit(
	xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	const xsshtransportpending* pPending,
	uint32* pActions
)
{
	*pActions = XSSH_TRANSPORT_ACTION_NONE;
	switch ( pPending->Kind ) {
		case XSSH_TRANSPORT_PACKET_KEXINIT:
			return xrtSshTransportKexInitCommit(
				&pCore->State,
				Direction,
				pPending->FirstKexPacketFollows
			);
		case XSSH_TRANSPORT_PACKET_NEWKEYS:
			return xrtSshTransportNewKeysCommit(
				&pCore->State,
				Direction,
				pActions
			);
		case XSSH_TRANSPORT_PACKET_AUTH_SUCCESS:
			return xrtSshTransportAuthSuccessCommit(
				&pCore->State,
				Direction
			);
		case XSSH_TRANSPORT_PACKET_MESSAGE:
			return xrtSshTransportMessageCommit(
				&pCore->State,
				Direction,
				pPending->Message
			);
		default:
			return XSSH_ERROR_STATE;
	}
}



/* AES-GCM 只统计加密 packet body 的十六字节块。 */
static uint64 xsshTransportCoreCipherBlocks(
	xsshpacketmode Mode,
	uint32 iPacketSize
)
{
	return Mode == XSSH_PACKET_MODE_AES_GCM ?
		(uint64)(iPacketSize / XSSH_AES_GCM_BLOCK_SIZE) : 0u;
}



/* 不可恢复错误关闭状态机并解除本地写准备，避免继续复用状态。 */
static void xsshTransportCoreFail(xsshtransportcore* pCore)
{
	if ( pCore->Codec.WritePending ) {
		(void)xrtSshPacketCodecWriteAbort(&pCore->Codec);
	}
	xsshTransportPendingClear(&pCore->Write);
	xsshTransportPendingClear(&pCore->Read);
	pCore->WriteKeyActions = 0u;
	pCore->ReadKeyActions = 0u;
	pCore->KexCompletePending = false;
	xrtSshTransportClose(&pCore->State);
}



/* 记录方向性 NEWKEYS 动作，实际 cipher 激活前保持该方向关闭。 */
static void xsshTransportCoreKeyActions(
	xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	uint32 iActions
)
{
	if ( (iActions & XSSH_TRANSPORT_ACTION_ACTIVATE_KEYS) != 0u ) {
		if ( Direction == XSSH_TRANSPORT_LOCAL ) {
			pCore->WriteKeyActions = iActions;
		} else {
			pCore->ReadKeyActions = iActions;
		}
	}
	if ( (iActions & XSSH_TRANSPORT_ACTION_KEX_COMPLETE) != 0u ) {
		pCore->KexCompletePending = true;
	}
}



/* 只有双向新 cipher 都已提交后才结束本轮 rekey 请求。 */
static bool xsshTransportCoreCompleteReady(xsshtransportcore* pCore)
{
	if ( !pCore->KexCompletePending ||
		(pCore->WriteKeyActions != 0u) ||
		(pCore->ReadKeyActions != 0u) ) {
		return true;
	}
	if ( !xrtSshRekeyComplete(&pCore->Rekey) ) {
		return false;
	}
	pCore->KexCompletePending = false;
	return true;
}



/* 组合三个纯状态层，初始化失败不会发布半成品。 */
bool xrtSshTransportCoreInit(
	xsshtransportcore* pCore,
	xsshrole Role,
	uint32 iMaxPacketSize,
	const xsshrekeypolicy* pRekeyPolicy,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return false; }
	xsshtransportcore Core;

	if ( !xrtMemRangeValid(pCore, sizeof(*pCore)) ) {
		return false;
	}
	memset(&Core, 0, sizeof(Core));
	if ( (xrtSshPacketCodecInit(
		&Core.Codec,
		iMaxPacketSize
	) != XSSH_OK) || !xrtSshTransportStateInit(&Core.State, Role) ||
		!xrtSshRekeyInit(&Core.Rekey, pRekeyPolicy, Timer) ) {
		xrtSshPacketCodecClear(&Core.Codec);
		xrtSecureZero(&Core, sizeof(Core));
		return false;
	}
	Core.Guard = XSSH_TRANSPORT_CORE_GUARD;
	xrtSecureZero(pCore, sizeof(*pCore));
	*pCore = Core;
	xrtSecureZero(&Core, sizeof(Core));
	return true;
}



/* Core 唯一秘密是展开后的 active cipher，统一安全清零。 */
void xrtSshTransportCoreClear(xsshtransportcore* pCore)
{
	if ( pCore == NULL ) {
		return;
	}
	xrtSecureZero(pCore, sizeof(*pCore));
}



/* Identification 不经过 packet codec，但仍属于同一协议状态。 */
xsshcode xrtSshTransportCoreIdentificationCommit(
	xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	if ( !xsshTransportCoreValid(pCore) || pCore->Write.Active ||
		pCore->Read.Active ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshTransportIdentificationCommit(&pCore->State, Direction);
}



/* 状态机开放后仍必须等待对应方向的新 cipher 实际提交。 */
bool xrtSshTransportCoreCanApplication(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	if ( !xsshTransportCoreValid(pCore) ) {
		return false;
	}
	if ( ((Direction == XSSH_TRANSPORT_LOCAL) &&
		 (pCore->WriteKeyActions != 0u)) ||
		((Direction == XSSH_TRANSPORT_PEER) &&
		 (pCore->ReadKeyActions != 0u)) ) {
		return false;
	}
	return xrtSshTransportCanApplication(&pCore->State, Direction);
}



/* KEX 回复判断直接复用顺序状态。 */
bool xrtSshTransportCoreKexReplyNeeded(const xsshtransportcore* pCore)
{
	return xsshTransportCoreValid(pCore) &&
		xrtSshTransportKexReplyNeeded(&pCore->State);
}



/* KEX 配置期间不得存在尚未完成的 packet 或 NEWKEYS 密钥动作。 */
xsshcode xrtSshTransportCoreKexConfigure(
	xsshtransportcore* pCore,
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	const xsshtransportkexrules* pRules
)
{
	if ( !xsshTransportCoreValid(pCore) || pCore->Write.Active ||
		pCore->Read.Active || (pCore->WriteKeyActions != 0u) ||
		(pCore->ReadKeyActions != 0u) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshTransportKexConfigure(
		&pCore->State,
		pLocal,
		pPeer,
		pNegotiation,
		pRules
	);
}



/* 主动请求只改变预算策略状态，不隐式生成 KEXINIT。 */
bool xrtSshTransportCoreRekeyRequest(xsshtransportcore* pCore)
{
	return xsshTransportCoreValid(pCore) &&
		xrtSshRekeyRequest(&pCore->Rekey);
}



/* 查询操作保持 core 不变。 */
xsshcode xrtSshTransportCoreRekeyCheck(
	const xsshtransportcore* pCore,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	if ( !xsshTransportCoreValid(pCore) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pDecision,
		sizeof(*pDecision)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshRekeyCheck(&pCore->Rekey, Timer, pDecision);
}



/* NEWKEYS 未应用时不能用旧方向模式探测下一包。 */
xsshcode xrtSshTransportCoreInspect(
	const xsshtransportcore* pCore,
	const xsshreader* pReader,
	xsshpacketneed* pNeed
)
{
	if ( !xsshTransportCoreValid(pCore) ||
		(pCore->ReadKeyActions != 0u) || pCore->Read.Active ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		!xrtMemRangeValid(pNeed, sizeof(*pNeed)) ||
		xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pReader,
		sizeof(*pReader)
	) || xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pNeed,
		sizeof(*pNeed)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshPacketCodecInspect(&pCore->Codec, pReader, pNeed);
}



/* 状态检查和硬额度检查都发生在生成线路字节之前。 */
xsshcode xrtSshTransportCoreWritePrepareWithPadding(
	xsshtransportcore* pCore,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshtransportpending Pending;
	xsshrekeydecision Decision;
	size_t iStart;
	size_t iWireSize;
	uint32 iPacketSize;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || pCore->Write.Active ||
		(pCore->WriteKeyActions != 0u) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pWriter->Data, pWriter->Capacity) ||
		(pWriter->Size > pWriter->Capacity) ||
		!xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pWriter,
			sizeof(*pWriter)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pWriter->Data,
			pWriter->Capacity
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshTransportPendingRead(Payload, &Pending);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshTransportCoreStateCheck(
		pCore,
		XSSH_TRANSPORT_LOCAL,
		&Pending
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshRekeyCheck(&pCore->Rekey, Timer, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Decision == XSSH_REKEY_REQUIRED ) {
		return XSSH_ERROR_STATE;
	}
	iStart = pWriter->Size;
	Code = xrtSshPacketCodecWritePrepareWithPadding(
		&pCore->Codec,
		pWriter,
		Payload,
		pPadding,
		pUserData
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iWireSize = pWriter->Size - iStart;
	iPacketSize = (uint32)(iWireSize - 4u -
		(pCore->Codec.WriteMode == XSSH_PACKET_MODE_AES_GCM ?
		 XSSH_AES_GCM_TAG_SIZE : 0u));
	Pending.WireBytes = iWireSize;
	Pending.CipherBlocks = xsshTransportCoreCipherBlocks(
		pCore->Codec.WriteMode,
		iPacketSize
	);
	pCore->Write = Pending;
	return XSSH_OK;
}



/* 可靠入队后按 codec、协议和预算三个边界一次提交。 */
xsshcode xrtSshTransportCoreWriteCommit(
	xsshtransportcore* pCore,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshrekeydecision Decision;
	uint32 iActions;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || !pCore->Write.Active ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshTransportCoreStateCheck(
		pCore,
		XSSH_TRANSPORT_LOCAL,
		&pCore->Write
	);
	if ( Code == XSSH_OK ) {
		Code = xrtSshRekeyReserveSend(
			&pCore->Rekey,
			pCore->Write.WireBytes,
			pCore->Write.CipherBlocks,
			Timer,
			&Decision
		);
	}
	if ( (Code != XSSH_OK) || (Decision == XSSH_REKEY_REQUIRED) ) {
		xsshTransportCoreFail(pCore);
		return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
	}
	Code = xsshTransportCoreStateCommit(
		pCore,
		XSSH_TRANSPORT_LOCAL,
		&pCore->Write,
		&iActions
	);
	if ( Code == XSSH_OK ) {
		Code = xrtSshPacketCodecWriteCommit(&pCore->Codec);
	}
	if ( Code != XSSH_OK ) {
		xsshTransportCoreFail(pCore);
		return Code;
	}
	xsshTransportPendingClear(&pCore->Write);
	xsshTransportCoreKeyActions(
		pCore,
		XSSH_TRANSPORT_LOCAL,
		iActions
	);
	*pDecision = Decision;
	return XSSH_OK;
}



/* 未进入网络队列的 packet 可以无损取消。 */
xsshcode xrtSshTransportCoreWriteAbort(xsshtransportcore* pCore)
{
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || !pCore->Write.Active ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshPacketCodecWriteAbort(&pCore->Codec);
	if ( Code != XSSH_OK ) {
		xsshTransportCoreFail(pCore);
		return Code;
	}
	xsshTransportPendingClear(&pCore->Write);
	return XSSH_OK;
}



/* 完整包先认证和分类，状态检查成功后才向调用方发布 view。 */
xsshcode xrtSshTransportCoreReadPrepare(
	xsshtransportcore* pCore,
	xsshreader* pReader,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshtransportpending Pending;
	xsshrekeydecision Decision;
	xsshpacketneed Need;
	xsshpacketview Packet;
	xsshreader Reader;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || pCore->Read.Active ||
		(pCore->ReadKeyActions != 0u) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pReader,
			sizeof(*pReader)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pReader->Source.Data,
			pReader->Source.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPlain,
			iPlainCapacity
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPacketCodecInspect(&pCore->Codec, pReader, &Need);
	if ( Code != XSSH_OK ) {
		if ( (Code == XSSH_ERROR_PROTOCOL) ||
			(Code == XSSH_ERROR_OVERFLOW) ||
			(Code == XSSH_ERROR_AUTHENTICATION) ||
			(Code == XSSH_ERROR_STATE) ) {
			xsshTransportCoreFail(pCore);
		}
		return Code;
	}
	if ( xrtSshReaderRemaining(pReader) < Need.WireSize ) {
		return XSSH_NEED_MORE;
	}
	Code = xrtSshRekeyCheck(&pCore->Rekey, Timer, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Decision == XSSH_REKEY_REQUIRED ) {
		return XSSH_ERROR_STATE;
	}
	Reader = *pReader;
	Code = xrtSshPacketCodecRead(
		&pCore->Codec,
		&Reader,
		&Packet,
		pPlain,
		iPlainCapacity
	);
	if ( Code != XSSH_OK ) {
		if ( (Code == XSSH_ERROR_PROTOCOL) ||
			(Code == XSSH_ERROR_OVERFLOW) ||
			(Code == XSSH_ERROR_AUTHENTICATION) ||
			(Code == XSSH_ERROR_STATE) ) {
			xsshTransportCoreFail(pCore);
		}
		return Code;
	}
	Code = xsshTransportPendingRead(Packet.Payload, &Pending);
	if ( Code == XSSH_OK ) {
		Code = xsshTransportCoreStateCheck(
			pCore,
			XSSH_TRANSPORT_PEER,
			&Pending
		);
	}
	if ( Code != XSSH_OK ) {
		if ( Need.PlainSize != 0u ) {
			xrtSecureZero(pPlain, Need.PlainSize);
		}
		xsshTransportCoreFail(pCore);
		return Code;
	}
	Pending.WireBytes = Need.WireSize;
	Pending.CipherBlocks = xsshTransportCoreCipherBlocks(
		pCore->Codec.ReadMode,
		Need.PacketSize
	);
	pCore->Read = Pending;
	*pReader = Reader;
	*pPacket = Packet;
	return XSSH_OK;
}



/* 已认证包只在上层解析接受后登记预算并推进协议。 */
xsshcode xrtSshTransportCoreReadCommit(
	xsshtransportcore* pCore,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshrekeydecision Decision;
	uint32 iActions;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || !pCore->Read.Active ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshTransportCoreStateCheck(
		pCore,
		XSSH_TRANSPORT_PEER,
		&pCore->Read
	);
	if ( Code == XSSH_OK ) {
		Code = xrtSshRekeyReserveReceive(
			&pCore->Rekey,
			pCore->Read.WireBytes,
			pCore->Read.CipherBlocks,
			Timer,
			&Decision
		);
	}
	if ( (Code != XSSH_OK) || (Decision == XSSH_REKEY_REQUIRED) ) {
		xsshTransportCoreFail(pCore);
		return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
	}
	Code = xsshTransportCoreStateCommit(
		pCore,
		XSSH_TRANSPORT_PEER,
		&pCore->Read,
		&iActions
	);
	if ( Code != XSSH_OK ) {
		xsshTransportCoreFail(pCore);
		return Code;
	}
	xsshTransportPendingClear(&pCore->Read);
	xsshTransportCoreKeyActions(
		pCore,
		XSSH_TRANSPORT_PEER,
		iActions
	);
	*pDecision = Decision;
	return XSSH_OK;
}



/* Codec 已消费并认证该包，放弃只能关闭而不能伪造回滚。 */
xsshcode xrtSshTransportCoreReadAbort(xsshtransportcore* pCore)
{
	if ( !xsshTransportCoreValid(pCore) || !pCore->Read.Active ) {
		return XSSH_ERROR_STATE;
	}
	xsshTransportCoreFail(pCore);
	return XSSH_OK;
}



/* 写方向动作非零表示 codec 仍停留在旧密钥。 */
bool xrtSshTransportCoreWriteKeysPending(const xsshtransportcore* pCore)
{
	return xsshTransportCoreValid(pCore) &&
		(pCore->WriteKeyActions != 0u);
}



/* 读方向动作非零表示 codec 仍停留在旧密钥。 */
bool xrtSshTransportCoreReadKeysPending(const xsshtransportcore* pCore)
{
	return xsshTransportCoreValid(pCore) &&
		(pCore->ReadKeyActions != 0u);
}



/* 新写 cipher、strict sequence 和发送代在同一调用中依次提交。 */
xsshcode xrtSshTransportCoreSetWriteAesGcm(
	xsshtransportcore* pCore,
	xbytesview Key,
	xbytesview InitialIV,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshrekeydecision Decision;
	uint32 iActions;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || pCore->Write.Active ||
		(pCore->WriteKeyActions == 0u) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshRekeyCheck(&pCore->Rekey, Timer, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iActions = pCore->WriteKeyActions;
	Code = xrtSshPacketCodecSetWriteAesGcm(
		&pCore->Codec,
		Key,
		InitialIV
	);
	if ( (Code == XSSH_OK) &&
		((iActions & XSSH_TRANSPORT_ACTION_RESET_SEQUENCE) != 0u) ) {
		Code = xrtSshPacketCodecResetWriteSequence(&pCore->Codec);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshRekeyResetSend(&pCore->Rekey, Timer) ) {
		xsshTransportCoreFail(pCore);
		return XSSH_ERROR_STATE;
	}
	pCore->WriteKeyActions = 0u;
	if ( !xsshTransportCoreCompleteReady(pCore) ) {
		xsshTransportCoreFail(pCore);
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 新读 cipher、strict sequence 和接收代在同一调用中依次提交。 */
xsshcode xrtSshTransportCoreSetReadAesGcm(
	xsshtransportcore* pCore,
	xbytesview Key,
	xbytesview InitialIV,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshrekeydecision Decision;
	uint32 iActions;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || pCore->Read.Active ||
		(pCore->ReadKeyActions == 0u) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshRekeyCheck(&pCore->Rekey, Timer, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iActions = pCore->ReadKeyActions;
	Code = xrtSshPacketCodecSetReadAesGcm(
		&pCore->Codec,
		Key,
		InitialIV
	);
	if ( (Code == XSSH_OK) &&
		((iActions & XSSH_TRANSPORT_ACTION_RESET_SEQUENCE) != 0u) ) {
		Code = xrtSshPacketCodecResetReadSequence(&pCore->Codec);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshRekeyResetReceive(&pCore->Rekey, Timer) ) {
		xsshTransportCoreFail(pCore);
		return XSSH_ERROR_STATE;
	}
	pCore->ReadKeyActions = 0u;
	if ( !xsshTransportCoreCompleteReady(pCore) ) {
		xsshTransportCoreFail(pCore);
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* OPEN 状态不能早于两个方向 cipher 的实际激活。 */
bool xrtSshTransportCoreKexComplete(const xsshtransportcore* pCore)
{
	return xsshTransportCoreValid(pCore) &&
		(pCore->State.KexCount != 0u) &&
		(pCore->State.Phase == XSSH_TRANSPORT_OPEN) &&
		(pCore->WriteKeyActions == 0u) &&
		(pCore->ReadKeyActions == 0u) &&
		!pCore->KexCompletePending;
}



/* Close 保留 cipher 供随后统一 Clear，但禁止任何继续推进。 */
void xrtSshTransportCoreClose(xsshtransportcore* pCore)
{
	if ( xsshTransportCoreValid(pCore) ) {
		xsshTransportCoreFail(pCore);
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_kex_session.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEX_SESSION)
#include <math.h>
#include <string.h>



#if defined(XSSH_FEATURE_KEX_SESSION)

#define XSSH_KEX_SESSION_GUARD UINT32_C(0x4B455853)



/* 校验会话对象的结构哨兵和角色。 */
static bool xsshKexSessionValid(const xsshkexsession* pSession)
{
	return xrtMemRangeValid(pSession, sizeof(*pSession)) &&
		(pSession->Guard == XSSH_KEX_SESSION_GUARD) &&
		((pSession->Role == XSSH_ROLE_CLIENT) ||
		 (pSession->Role == XSSH_ROLE_SERVER));
}



/* 校验 SSH identification 中参与 exchange hash 的无换行版本串。 */
static bool xsshKexVersionValid(xbytesview Version)
{
	char arrLine[XSSH_IDENTIFICATION_MAX];
	xstrview Banner;
	size_t iConsumed;

	if ( !xrtMemRangeValid(Version.Data, Version.Size) ||
		(Version.Size < 8u) ||
		(Version.Size >= XSSH_IDENTIFICATION_MAX) ) {
		return false;
	}
	memcpy(arrLine, Version.Data, Version.Size);
	arrLine[Version.Size] = '\n';
	return (xrtSshBannerRead(
		(xstrview){ arrLine, Version.Size + 1u },
		&Banner,
		&iConsumed
	) == XSSH_OK) && (Banner.Size == Version.Size) &&
		(iConsumed == (Version.Size + 1u)) &&
		(memcmp(Banner.Data, Version.Data, Version.Size) == 0);
}



/* 校验 transcript 及四段借用输入。 */
static bool xsshKexTranscriptValid(const xsshkextranscript* pTranscript)
{
	return xrtMemRangeValid(pTranscript, sizeof(*pTranscript)) &&
		xsshKexVersionValid(pTranscript->ClientVersion) &&
		xsshKexVersionValid(pTranscript->ServerVersion) &&
		xrtMemRangeValid(
			pTranscript->ClientKexInit.Data,
			pTranscript->ClientKexInit.Size
		) && xrtMemRangeValid(
			pTranscript->ServerKexInit.Data,
			pTranscript->ServerKexInit.Size
		) && (pTranscript->ClientKexInit.Size != 0u) &&
		(pTranscript->ServerKexInit.Size != 0u);
}



/* 判断对象范围是否与 transcript 任一借用字节段重叠。 */
static bool xsshKexTranscriptOverlaps(
	const xsshkextranscript* pTranscript,
	const void* pData,
	size_t iSize
)
{
	return xrtMemRangesOverlap(
		pData,
		iSize,
		pTranscript->ClientVersion.Data,
		pTranscript->ClientVersion.Size
	) || xrtMemRangesOverlap(
		pData,
		iSize,
		pTranscript->ServerVersion.Data,
		pTranscript->ServerVersion.Size
	) || xrtMemRangesOverlap(
		pData,
		iSize,
		pTranscript->ClientKexInit.Data,
		pTranscript->ClientKexInit.Size
	) || xrtMemRangesOverlap(
		pData,
		iSize,
		pTranscript->ServerKexInit.Data,
		pTranscript->ServerKexInit.Size
	);
}



/* 比较借用文本与常量算法名。 */
static bool xsshKexTextEqual(xstrview Text, const char* pExpected)
{
	size_t iSize = strlen(pExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, pExpected, iSize) == 0);
}



/* 返回当前实现支持的 AES-GCM 密钥长度。 */
static xsshcode xsshKexCipherKeySize(xstrview Cipher, uint8* pSize)
{
	if ( xsshKexTextEqual(Cipher, "aes128-gcm@openssh.com") ) {
		*pSize = 16u;
		return XSSH_OK;
	}
	if ( xsshKexTextEqual(Cipher, "aes256-gcm@openssh.com") ) {
		*pSize = 32u;
		return XSSH_OK;
	}
	return XSSH_ERROR_UNSUPPORTED;
}



/* 验证本会话能够直接驱动的算法组合。 */
static xsshcode xsshKexNegotiationSupported(
	const xsshkexnegotiation* pNegotiation,
	uint8* pClientToServerKeySize,
	uint8* pServerToClientKeySize
)
{
	xsshcode Code;

	if ( (!xsshKexTextEqual(
		pNegotiation->KexAlgorithm,
		"curve25519-sha256"
	) && !xsshKexTextEqual(
		pNegotiation->KexAlgorithm,
		"curve25519-sha256@libssh.org"
	)) || !xsshKexTextEqual(
		pNegotiation->ServerHostKeyAlgorithm,
		XSSH_HOSTKEY_ED25519
	) || !xsshKexTextEqual(
		pNegotiation->CompressionClientToServer,
		"none"
	) || !xsshKexTextEqual(
		pNegotiation->CompressionServerToClient,
		"none"
	) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	Code = xsshKexCipherKeySize(
		pNegotiation->CipherClientToServer,
		pClientToServerKeySize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xsshKexCipherKeySize(
		pNegotiation->CipherServerToClient,
		pServerToClientKeySize
	);
}



/* 将会话阶段同步到已经提交的 KEX 状态。 */
static void xsshKexSessionUpdatePhase(xsshkexsession* pSession)
{
	if ( !pSession->Active ||
		(pSession->Phase == XSSH_KEX_SESSION_FAILED) ) {
		return;
	}
	if ( pSession->WriteActivated && pSession->ReadActivated ) {
		pSession->Phase = XSSH_KEX_SESSION_COMPLETE;
		return;
	}
	if ( (pSession->Role == XSSH_ROLE_CLIENT) &&
		pSession->MethodReadCommitted &&
		!pSession->HostKeyAccepted ) {
		pSession->Phase = XSSH_KEX_SESSION_HOST_KEY;
		return;
	}
	if ( pSession->MethodWriteCommitted &&
		pSession->MethodReadCommitted &&
		((pSession->Role == XSSH_ROLE_SERVER) ||
		 pSession->HostKeyAccepted) ) {
		pSession->Phase = XSSH_KEX_SESSION_NEW_KEYS;
		return;
	}
	pSession->Phase = XSSH_KEX_SESSION_METHOD;
}



/* 发生不可恢复错误时清除本代及跨代秘密。 */
static void xsshKexSessionSetFailed(xsshkexsession* pSession)
{
	xsshrole Role = pSession->Role;

	xrtSecureZero(pSession, sizeof(*pSession));
	pSession->Role = Role;
	pSession->Phase = XSSH_KEX_SESSION_FAILED;
	pSession->Guard = XSSH_KEX_SESSION_GUARD;
}



/* 计算 exchange hash 并派生本代双向 AES-GCM 材料。 */
static xsshcode xsshKexSessionDerive(xsshkexsession* pSession)
{
	xsshkexhashsha256 Input;
	xbytesview ClientPublic;
	xbytesview ServerPublic;
	xbytesview Shared;
	xbytesview Hash;
	xbytesview SessionId;
	xsshcode Code;

	ClientPublic = pSession->Role == XSSH_ROLE_CLIENT ?
		(xbytesview){ pSession->PublicKey, sizeof(pSession->PublicKey) } :
		(xbytesview){ pSession->PeerPublicKey, sizeof(pSession->PeerPublicKey) };
	ServerPublic = pSession->Role == XSSH_ROLE_SERVER ?
		(xbytesview){ pSession->PublicKey, sizeof(pSession->PublicKey) } :
		(xbytesview){ pSession->PeerPublicKey, sizeof(pSession->PeerPublicKey) };
	Shared = (xbytesview){
		pSession->SharedSecret,
		sizeof(pSession->SharedSecret)
	};
	memset(&Input, 0, sizeof(Input));
	Input.ClientVersion = pSession->Transcript.ClientVersion;
	Input.ServerVersion = pSession->Transcript.ServerVersion;
	Input.ClientKexInit = pSession->Transcript.ClientKexInit;
	Input.ServerKexInit = pSession->Transcript.ServerKexInit;
	Input.ServerHostKey = pSession->ServerHostKey;
	Input.ClientEphemeral = ClientPublic;
	Input.ServerEphemeral = ServerPublic;
	Input.SharedSecret = Shared;
	Code = xrtSshKexHashSha256(&Input, pSession->ExchangeHash);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !pSession->HasSessionId ) {
		memcpy(
			pSession->SessionId,
			pSession->ExchangeHash,
			sizeof(pSession->SessionId)
		);
		pSession->HasSessionId = true;
	}
	Hash = (xbytesview){
		pSession->ExchangeHash,
		sizeof(pSession->ExchangeHash)
	};
	SessionId = (xbytesview){
		pSession->SessionId,
		sizeof(pSession->SessionId)
	};
	Code = xrtSshKexDeriveSha256(
		pSession->ClientToServerIV,
		sizeof(pSession->ClientToServerIV),
		Shared,
		pSession->ExchangeHash,
		pSession->SessionId,
		(uint8)'A'
	);
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexDeriveSha256(
			pSession->ServerToClientIV,
			sizeof(pSession->ServerToClientIV),
			Shared,
			Hash.Data,
			SessionId.Data,
			(uint8)'B'
		);
	}
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexDeriveSha256(
			pSession->ClientToServerKey,
			pSession->ClientToServerKeySize,
			Shared,
			Hash.Data,
			SessionId.Data,
			(uint8)'C'
		);
	}
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexDeriveSha256(
			pSession->ServerToClientKey,
			pSession->ServerToClientKeySize,
			Shared,
			Hash.Data,
			SessionId.Data,
			(uint8)'D'
		);
	}
	if ( Code == XSSH_OK ) {
		pSession->KeysDerived = true;
	} else {
		xrtSecureZero(
			pSession->ClientToServerIV,
			sizeof(pSession->ClientToServerIV)
		);
		xrtSecureZero(
			pSession->ServerToClientIV,
			sizeof(pSession->ServerToClientIV)
		);
		xrtSecureZero(
			pSession->ClientToServerKey,
			sizeof(pSession->ClientToServerKey)
		);
		xrtSecureZero(
			pSession->ServerToClientKey,
			sizeof(pSession->ServerToClientKey)
		);
	}
	return Code;
}



/* 判断当前 peer 方法包是否是协商后必须丢弃的猜测包。 */
static bool xsshKexSessionDiscardGuess(
	const xsshtransportcore* pCore,
	uint8 iMessage
)
{
	return (iMessage >= XSSH_KEX_METHOD_MIN) &&
		(iMessage <= XSSH_KEX_METHOD_MAX) &&
		pCore->State.PeerGuessExpected &&
		!pCore->State.PeerGuessSeen &&
		pCore->State.PeerGuessSkip;
}



/* 校验当前 payload 确实来自 core 的未提交读事务。 */
static bool xsshKexSessionCoreReadMatches(
	const xsshtransportcore* pCore,
	xbytesview Payload,
	uint8 iMessage
)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		pCore->Read.Active &&
		(pCore->Read.Message == iMessage) &&
		(Payload.Size != 0u) && (Payload.Data[0] == iMessage);
}



/* 初始化四段借用 transcript。 */
xsshcode xrtSshKexTranscriptInit(
	xsshkextranscript* pTranscript,
	xbytesview ClientVersion,
	xbytesview ServerVersion,
	xbytesview ClientKexInit,
	xbytesview ServerKexInit
)
{
	xsshkextranscript Transcript;

	if ( !xrtMemRangeValid(pTranscript, sizeof(*pTranscript)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Transcript.ClientVersion = ClientVersion;
	Transcript.ServerVersion = ServerVersion;
	Transcript.ClientKexInit = ClientKexInit;
	Transcript.ServerKexInit = ServerKexInit;
	if ( !xsshKexTranscriptValid(&Transcript) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xsshKexTranscriptOverlaps(
		&Transcript,
		pTranscript,
		sizeof(*pTranscript)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pTranscript = Transcript;
	return XSSH_OK;
}



/* 计算 transcript 四段原始字节总量。 */
xsshcode xrtSshKexTranscriptMeasure(
	const xsshkextranscript* pTranscript,
	size_t* pSize
)
{
	size_t iSize;

	if ( !xsshKexTranscriptValid(pTranscript) ||
		!xrtMemRangeValid(pSize, sizeof(*pSize)) ||
		xrtMemRangesOverlap(
			pTranscript,
			sizeof(*pTranscript),
			pSize,
			sizeof(*pSize)
		) || xsshKexTranscriptOverlaps(
			pTranscript,
			pSize,
			sizeof(*pSize)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iSize = pTranscript->ClientVersion.Size;
	if ( (pTranscript->ServerVersion.Size > (SIZE_MAX - iSize)) ||
		(pTranscript->ClientKexInit.Size >
		 (SIZE_MAX - iSize - pTranscript->ServerVersion.Size)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iSize += pTranscript->ServerVersion.Size;
	iSize += pTranscript->ClientKexInit.Size;
	if ( pTranscript->ServerKexInit.Size > (SIZE_MAX - iSize) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pSize = iSize + pTranscript->ServerKexInit.Size;
	return XSSH_OK;
}



/* 一次预留后复制 transcript，保证失败不推进 writer。 */
xsshcode xrtSshKexTranscriptWrite(
	xsshwriter* pWriter,
	const xsshkextranscript* pInput,
	xsshkextranscript* pOutput
)
{
	xsshkextranscript Input;
	xbytesview arrInputs[4];
	xsshkextranscript Output;
	size_t iSize;
	size_t iStart;
	xsshcode Code;

	if ( !xsshKexTranscriptValid(pInput) ||
		!xrtMemRangeValid(pOutput, sizeof(*pOutput)) ||
		xsshKexTranscriptOverlaps(
			pInput,
			pOutput,
			sizeof(*pOutput)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Input = *pInput;
	Code = xrtSshKexTranscriptMeasure(&Input, &iSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	arrInputs[0] = Input.ClientVersion;
	arrInputs[1] = Input.ServerVersion;
	arrInputs[2] = Input.ClientKexInit;
	arrInputs[3] = Input.ServerKexInit;
	Code = xrtSshWriterReserveInputs(pWriter, iSize, arrInputs, 4u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtMemRangesOverlap(
		pOutput,
		sizeof(*pOutput),
		pWriter->Data + pWriter->Size,
		iSize
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iStart = pWriter->Size;
	memcpy(pWriter->Data + pWriter->Size, Input.ClientVersion.Data,
		Input.ClientVersion.Size);
	pWriter->Size += Input.ClientVersion.Size;
	memcpy(pWriter->Data + pWriter->Size, Input.ServerVersion.Data,
		Input.ServerVersion.Size);
	pWriter->Size += Input.ServerVersion.Size;
	memcpy(pWriter->Data + pWriter->Size, Input.ClientKexInit.Data,
		Input.ClientKexInit.Size);
	pWriter->Size += Input.ClientKexInit.Size;
	memcpy(pWriter->Data + pWriter->Size, Input.ServerKexInit.Data,
		Input.ServerKexInit.Size);
	pWriter->Size += Input.ServerKexInit.Size;
	Output.ClientVersion = (xbytesview){
		pWriter->Data + iStart,
		Input.ClientVersion.Size
	};
	iStart += Input.ClientVersion.Size;
	Output.ServerVersion = (xbytesview){
		pWriter->Data + iStart,
		Input.ServerVersion.Size
	};
	iStart += Input.ServerVersion.Size;
	Output.ClientKexInit = (xbytesview){
		pWriter->Data + iStart,
		Input.ClientKexInit.Size
	};
	iStart += Input.ClientKexInit.Size;
	Output.ServerKexInit = (xbytesview){
		pWriter->Data + iStart,
		Input.ServerKexInit.Size
	};
	*pOutput = Output;
	return XSSH_OK;
}



/* 初始化无外部资源的 KEX 会话。 */
bool xrtSshKexSessionInit(xsshkexsession* pSession, xsshrole Role)
{
	xsshkexsession Session;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	memset(&Session, 0, sizeof(Session));
	Session.Role = Role;
	Session.Phase = XSSH_KEX_SESSION_IDLE;
	Session.Guard = XSSH_KEX_SESSION_GUARD;
	xrtSecureZero(pSession, sizeof(*pSession));
	*pSession = Session;
	xrtSecureZero(&Session, sizeof(Session));
	return true;
}



/* 安全清除完整 KEX 会话。 */
void xrtSshKexSessionClear(xsshkexsession* pSession)
{
	if ( xrtMemRangeValid(pSession, sizeof(*pSession)) ) {
		xrtSecureZero(pSession, sizeof(*pSession));
	}
}



/* 配置一代确定性 Curve25519/Ed25519/AES-GCM KEX。 */
xsshcode xrtSshKexSessionBeginWithPrivate(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	const xsshkextranscript* pTranscript,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
)
{
	xsshkexsession Session;
	xsshkexinit ClientKex;
	xsshkexinit ServerKex;
	xsshkexinit* pLocal;
	xsshkexinit* pPeer;
	xsshtransportkexrules Rules;
	xbytesview PublicKey;
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		!xsshKexTranscriptValid(pTranscript) ||
		!xrtMemRangeValid(PrivateKey.Data, PrivateKey.Size) ||
		(PrivateKey.Size != XSSH_CURVE25519_PRIVATE_SIZE) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pTranscript,
			sizeof(*pTranscript)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pTranscript,
			sizeof(*pTranscript)
		) || xsshKexTranscriptOverlaps(
			pTranscript,
			pSession,
			sizeof(*pSession)
		) || xsshKexTranscriptOverlaps(
			pTranscript,
			pCore,
			sizeof(*pCore)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			ServerHostKey.Data,
			ServerHostKey.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			ServerHostKey.Data,
			ServerHostKey.Size
		) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			PrivateKey.Data,
			PrivateKey.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			PrivateKey.Data,
			PrivateKey.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pSession->Active &&
		(pSession->Phase != XSSH_KEX_SESSION_COMPLETE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pCore->State.Role != pSession->Role) ||
		(pCore->State.Phase != XSSH_TRANSPORT_KEY_EXCHANGE) ||
		!pCore->State.LocalKexInit || !pCore->State.PeerKexInit ||
		pCore->State.KexConfigured || pCore->Write.Active ||
		pCore->Read.Active ) {
		return XSSH_ERROR_STATE;
	}
	if ( ((pCore->State.KexCount == 0u) && pSession->HasSessionId) ||
		((pCore->State.KexCount != 0u) && !pSession->HasSessionId) ) {
		return XSSH_ERROR_STATE;
	}
	memset(&ClientKex, 0, sizeof(ClientKex));
	memset(&ServerKex, 0, sizeof(ServerKex));
	Code = xrtSshKexInitRead(pTranscript->ClientKexInit, &ClientKex);
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexInitRead(pTranscript->ServerKexInit, &ServerKex);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pLocal = pSession->Role == XSSH_ROLE_CLIENT ? &ClientKex : &ServerKex;
	pPeer = pSession->Role == XSSH_ROLE_CLIENT ? &ServerKex : &ClientKex;
	if ( pLocal->FirstKexPacketFollows ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	Session = *pSession;
	Session.Transcript = *pTranscript;
	memset(&Session.Negotiation, 0, sizeof(Session.Negotiation));
	Code = xrtSshKexNegotiate(
		&ClientKex,
		&ServerKex,
		&Session.Negotiation
	);
	if ( Code == XSSH_OK ) {
		Code = xsshKexNegotiationSupported(
			&Session.Negotiation,
			&Session.ClientToServerKeySize,
			&Session.ServerToClientKeySize
		);
	}
	if ( Code != XSSH_OK ) {
		xrtSecureZero(&Session, sizeof(Session));
		return Code;
	}
	Session.ServerHostKey = (xbytesview){ NULL, 0u };
	if ( Session.Role == XSSH_ROLE_SERVER ) {
		Code = xrtSshEd25519PublicKeyRead(ServerHostKey, &PublicKey);
		if ( Code != XSSH_OK ) {
			xrtSecureZero(&Session, sizeof(Session));
			return Code;
		}
		Session.ServerHostKey = ServerHostKey;
		Session.HostKeyVerified = true;
		Session.HostKeyAccepted = true;
	} else if ( !xrtMemRangeValid(ServerHostKey.Data, ServerHostKey.Size) ||
		(ServerHostKey.Size != 0u) ) {
		xrtSecureZero(&Session, sizeof(Session));
		return XSSH_ERROR_ARGUMENT;
	}
	memcpy(Session.PrivateKey, PrivateKey.Data, sizeof(Session.PrivateKey));
	Code = xrtSshCurve25519Public(
		Session.PrivateKey,
		Session.PublicKey
	);
	if ( Code != XSSH_OK ) {
		xrtSecureZero(&Session, sizeof(Session));
		return Code;
	}
	Session.Active = true;
	Session.Phase = XSSH_KEX_SESSION_METHOD;
	Session.WritePending = XSSH_KEX_PACKET_NONE;
	Session.ReadPending = XSSH_KEX_PACKET_NONE;
	Session.KeysDerived = false;
	Session.MethodWriteCommitted = false;
	Session.MethodReadCommitted = false;
	Session.LocalNewKeys = false;
	Session.PeerNewKeys = false;
	Session.WriteActivated = false;
	Session.ReadActivated = false;
	if ( Session.Role == XSSH_ROLE_CLIENT ) {
		Session.HostKeyVerified = false;
		Session.HostKeyAccepted = false;
	}
	xrtSecureZero(Session.PeerPublicKey, sizeof(Session.PeerPublicKey));
	xrtSecureZero(Session.SharedSecret, sizeof(Session.SharedSecret));
	xrtSecureZero(Session.ExchangeHash, sizeof(Session.ExchangeHash));
	xrtSecureZero(Session.ClientToServerIV, sizeof(Session.ClientToServerIV));
	xrtSecureZero(Session.ServerToClientIV, sizeof(Session.ServerToClientIV));
	xrtSecureZero(Session.ClientToServerKey, sizeof(Session.ClientToServerKey));
	xrtSecureZero(Session.ServerToClientKey, sizeof(Session.ServerToClientKey));
	if ( !xrtSshTransportKexRulesInit(&Rules) ||
		!xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_LOCAL,
			Session.Role == XSSH_ROLE_CLIENT ?
				XSSH_MSG_KEX_ECDH_INIT : XSSH_MSG_KEX_ECDH_REPLY,
			1u
		) || !xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_PEER,
			Session.Role == XSSH_ROLE_CLIENT ?
				XSSH_MSG_KEX_ECDH_REPLY : XSSH_MSG_KEX_ECDH_INIT,
			1u
		) ) {
		xrtSecureZero(&Session, sizeof(Session));
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshTransportCoreKexConfigure(
		pCore,
		pLocal,
		pPeer,
		&Session.Negotiation,
		&Rules
	);
	if ( Code != XSSH_OK ) {
		xrtSecureZero(&Session, sizeof(Session));
		return Code;
	}
	*pSession = Session;
	xrtSecureZero(&Session, sizeof(Session));
	return XSSH_OK;
}



/* 计算常见驱动的下一动作。 */
xsshkexsessionevent xrtSshKexSessionEvent(const xsshkexsession* pSession)
{
	if ( !xsshKexSessionValid(pSession) ) {
		return XSSH_KEX_EVENT_NONE;
	}
	if ( pSession->Phase == XSSH_KEX_SESSION_FAILED ) {
		return XSSH_KEX_EVENT_FAILED;
	}
	if ( pSession->Phase == XSSH_KEX_SESSION_COMPLETE ) {
		return XSSH_KEX_EVENT_COMPLETE;
	}
	if ( !pSession->Active ||
		(pSession->WritePending != XSSH_KEX_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_KEX_PACKET_NONE) ) {
		return XSSH_KEX_EVENT_NONE;
	}
	if ( pSession->Role == XSSH_ROLE_CLIENT ) {
		if ( !pSession->MethodWriteCommitted ) {
			return XSSH_KEX_EVENT_WRITE_ECDH_INIT;
		}
		if ( !pSession->MethodReadCommitted ) {
			return XSSH_KEX_EVENT_READ_ECDH_REPLY;
		}
		if ( !pSession->HostKeyAccepted ) {
			return XSSH_KEX_EVENT_VERIFY_HOST_KEY;
		}
	} else {
		if ( !pSession->MethodReadCommitted ) {
			return XSSH_KEX_EVENT_READ_ECDH_INIT;
		}
		if ( !pSession->MethodWriteCommitted ) {
			return XSSH_KEX_EVENT_WRITE_ECDH_REPLY;
		}
	}
	if ( !pSession->LocalNewKeys ) {
		return XSSH_KEX_EVENT_WRITE_NEWKEYS;
	}
	if ( !pSession->WriteActivated ) {
		return XSSH_KEX_EVENT_ACTIVATE_WRITE;
	}
	if ( !pSession->PeerNewKeys ) {
		return XSSH_KEX_EVENT_READ_NEWKEYS;
	}
	if ( !pSession->ReadActivated ) {
		return XSSH_KEX_EVENT_ACTIVATE_READ;
	}
	return XSSH_KEX_EVENT_COMPLETE;
}



/* 返回协商结果快照。 */
xsshcode xrtSshKexSessionNegotiation(
	const xsshkexsession* pSession,
	xsshkexnegotiation* pNegotiation
)
{
	if ( !xsshKexSessionValid(pSession) || !pSession->Active ||
		!xrtMemRangeValid(pNegotiation, sizeof(*pNegotiation)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pNegotiation,
			sizeof(*pNegotiation)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pNegotiation = pSession->Negotiation;
	return XSSH_OK;
}



/* 返回已计算的 exchange hash。 */
xsshcode xrtSshKexSessionExchangeHash(
	const xsshkexsession* pSession,
	xbytesview* pHash
)
{
	if ( !xsshKexSessionValid(pSession) || !pSession->KeysDerived ||
		!xrtMemRangeValid(pHash, sizeof(*pHash)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pHash,
			sizeof(*pHash)
		) ) {
		return XSSH_ERROR_STATE;
	}
	pHash->Data = pSession->ExchangeHash;
	pHash->Size = sizeof(pSession->ExchangeHash);
	return XSSH_OK;
}



/* 返回跨 rekey 保持的 SessionId。 */
xsshcode xrtSshKexSessionId(
	const xsshkexsession* pSession,
	xbytesview* pSessionId
)
{
	if ( !xsshKexSessionValid(pSession) || !pSession->HasSessionId ||
		!xrtMemRangeValid(pSessionId, sizeof(*pSessionId)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pSessionId,
			sizeof(*pSessionId)
		) ) {
		return XSSH_ERROR_STATE;
	}
	pSessionId->Data = pSession->SessionId;
	pSessionId->Size = sizeof(pSession->SessionId);
	return XSSH_OK;
}



/* 返回客户端待确认的主机公钥。 */
xsshcode xrtSshKexSessionHostKey(
	const xsshkexsession* pSession,
	xbytesview* pHostKey
)
{
	if ( !xsshKexSessionValid(pSession) ||
		(pSession->Role != XSSH_ROLE_CLIENT) ||
		!pSession->HostKeyVerified ||
		!xrtMemRangeValid(pHostKey, sizeof(*pHostKey)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pHostKey,
			sizeof(*pHostKey)
		) ) {
		return XSSH_ERROR_STATE;
	}
	*pHostKey = pSession->ServerHostKey;
	return XSSH_OK;
}



/* 提交客户端主机信任决策。 */
xsshcode xrtSshKexSessionHostKeyAccept(xsshkexsession* pSession)
{
	if ( !xsshKexSessionValid(pSession) ||
		(pSession->Role != XSSH_ROLE_CLIENT) ||
		(pSession->ReadPending != XSSH_KEX_PACKET_NONE) ||
		!pSession->MethodReadCommitted || !pSession->HostKeyVerified ) {
		return XSSH_ERROR_STATE;
	}
	pSession->HostKeyAccepted = true;
	xsshKexSessionUpdatePhase(pSession);
	return XSSH_OK;
}



/* 显式终止 KEX 会话。 */
void xrtSshKexSessionFail(xsshkexsession* pSession)
{
	if ( xsshKexSessionValid(pSession) ) {
		xsshKexSessionSetFailed(pSession);
	}
}



/* 准备客户端 Curve25519 方法初始消息。 */
xsshcode xrtSshKexSessionEcdhInitPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter
)
{
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) || !pSession->Active ||
		(pSession->Role != XSSH_ROLE_CLIENT) ||
		pSession->MethodWriteCommitted ||
		(pSession->WritePending != XSSH_KEX_PACKET_NONE) ||
		!xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pWriter,
			sizeof(*pWriter)
		) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshEcdhInitWrite(
		pWriter,
		(xbytesview){ pSession->PublicKey, sizeof(pSession->PublicKey) }
	);
	if ( Code == XSSH_OK ) {
		pSession->WritePending = XSSH_KEX_PACKET_ECDH_INIT;
	}
	return Code;
}



/* 验证外部签名并准备服务端 Curve25519 方法回复。 */
xsshcode xrtSshKexSessionEcdhReplyPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter,
	xbytesview Signature
)
{
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) || !pSession->Active ||
		(pSession->Role != XSSH_ROLE_SERVER) ||
		!pSession->MethodReadCommitted || !pSession->KeysDerived ||
		pSession->MethodWriteCommitted ||
		(pSession->WritePending != XSSH_KEX_PACKET_NONE) ||
		!xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pWriter,
			sizeof(*pWriter)
		) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshEd25519HostKeyVerify(
		pSession->ServerHostKey,
		Signature,
		(xbytesview){
			pSession->ExchangeHash,
			sizeof(pSession->ExchangeHash)
		}
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshEcdhReplyWrite(
		pWriter,
		pSession->ServerHostKey,
		(xbytesview){ pSession->PublicKey, sizeof(pSession->PublicKey) },
		Signature
	);
	if ( Code == XSSH_OK ) {
		pSession->WritePending = XSSH_KEX_PACKET_ECDH_REPLY;
	}
	return Code;
}



/* 准备无字段 NEWKEYS。 */
xsshcode xrtSshKexSessionNewKeysPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter
)
{
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) || !pSession->Active ||
		!pSession->KeysDerived || !pSession->MethodWriteCommitted ||
		!pSession->MethodReadCommitted || pSession->LocalNewKeys ||
		(pSession->WritePending != XSSH_KEX_PACKET_NONE) ||
		!xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pWriter,
			sizeof(*pWriter)
		) ||
		((pSession->Role == XSSH_ROLE_CLIENT) &&
		 !pSession->HostKeyAccepted) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshNewKeysWrite(pWriter);
	if ( Code == XSSH_OK ) {
		pSession->WritePending = XSSH_KEX_PACKET_NEWKEYS;
	}
	return Code;
}



/* 在 transport 提交后推进对应写方法。 */
xsshcode xrtSshKexSessionWriteCommit(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshkexsessionpacket Packet;
	uint8 iMessage;

	if ( !xsshKexSessionValid(pSession) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		(pCore->State.Role != pSession->Role) || pCore->Write.Active ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) ||
		(pSession->WritePending == XSSH_KEX_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	Packet = pSession->WritePending;
	if ( Packet == XSSH_KEX_PACKET_NEWKEYS ) {
		if ( !xrtSshTransportCoreWriteKeysPending(pCore) ) {
			return XSSH_ERROR_STATE;
		}
		pSession->LocalNewKeys = true;
	} else {
		iMessage = Packet == XSSH_KEX_PACKET_ECDH_INIT ?
			XSSH_MSG_KEX_ECDH_INIT : XSSH_MSG_KEX_ECDH_REPLY;
		if ( pCore->State.LocalKexRemaining[
			(size_t)iMessage - XSSH_KEX_METHOD_MIN
		] != 0u ) {
			return XSSH_ERROR_STATE;
		}
		pSession->MethodWriteCommitted = true;
	}
	pSession->WritePending = XSSH_KEX_PACKET_NONE;
	xsshKexSessionUpdatePhase(pSession);
	return XSSH_OK;
}



/* 放弃尚未可靠发送的 KEX payload。 */
xsshcode xrtSshKexSessionWriteAbort(xsshkexsession* pSession)
{
	if ( !xsshKexSessionValid(pSession) ||
		(pSession->WritePending == XSSH_KEX_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	pSession->WritePending = XSSH_KEX_PACKET_NONE;
	return XSSH_OK;
}



/* 服务端准备客户端 ECDH_INIT，并在临时候选状态中派生密钥。 */
static xsshcode xsshKexSessionReadInitPrepare(
	xsshkexsession* pSession,
	xbytesview Payload
)
{
	xsshkexsession Session = *pSession;
	xsshecdhinit Message;
	xsshcode Code;

	memset(&Message, 0, sizeof(Message));
	Code = xrtSshEcdhInitRead(Payload, &Message);
	if ( Code == XSSH_OK &&
		(Message.ClientPublic.Size != XSSH_CURVE25519_PUBLIC_SIZE) ) {
		Code = XSSH_ERROR_PROTOCOL;
	}
	if ( Code == XSSH_OK ) {
		memcpy(
			Session.PeerPublicKey,
			Message.ClientPublic.Data,
			sizeof(Session.PeerPublicKey)
		);
		Code = xrtSshCurve25519Shared(
			Session.PrivateKey,
			Session.PeerPublicKey,
			Session.SharedSecret
		);
	}
	if ( Code == XSSH_OK ) {
		Code = xsshKexSessionDerive(&Session);
	}
	if ( Code == XSSH_OK ) {
		Session.ReadPending = XSSH_KEX_PACKET_ECDH_INIT;
		*pSession = Session;
	}
	xrtSecureZero(&Session, sizeof(Session));
	return Code;
}



/* 客户端准备服务端 ECDH_REPLY、验签并复制待信任主机公钥。 */
static xsshcode xsshKexSessionReadReplyPrepare(
	xsshkexsession* pSession,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize
)
{
	xsshkexsession Session = *pSession;
	xsshecdhreply Message;
	xsshcode Code;

	memset(&Message, 0, sizeof(Message));
	Code = xrtSshEcdhReplyRead(Payload, &Message);
	if ( Code == XSSH_OK &&
		(Message.ServerPublic.Size != XSSH_CURVE25519_PUBLIC_SIZE) ) {
		Code = XSSH_ERROR_PROTOCOL;
	}
	if ( (Code == XSSH_OK) && (pHostKeySize != NULL) ) {
		*pHostKeySize = Message.ServerHostKey.Size;
	}
	if ( Code == XSSH_OK &&
		((iHostKeyCapacity < Message.ServerHostKey.Size) ||
		 !xrtMemRangeValid(pHostKeyStorage, Message.ServerHostKey.Size)) ) {
		Code = XSSH_ERROR_SPACE;
	}
	if ( Code == XSSH_OK &&
		(xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pHostKeyStorage,
			Message.ServerHostKey.Size
		) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pHostKeyStorage,
			Message.ServerHostKey.Size
		)) ) {
		Code = XSSH_ERROR_ARGUMENT;
	}
	if ( Code == XSSH_OK ) {
		memcpy(
			Session.PeerPublicKey,
			Message.ServerPublic.Data,
			sizeof(Session.PeerPublicKey)
		);
		Session.ServerHostKey = Message.ServerHostKey;
		Code = xrtSshCurve25519Shared(
			Session.PrivateKey,
			Session.PeerPublicKey,
			Session.SharedSecret
		);
	}
	if ( Code == XSSH_OK ) {
		Code = xsshKexSessionDerive(&Session);
	}
	if ( Code == XSSH_OK ) {
		Code = xrtSshEd25519HostKeyVerify(
			Message.ServerHostKey,
			Message.Signature,
			(xbytesview){
				Session.ExchangeHash,
				sizeof(Session.ExchangeHash)
			}
		);
	}
	if ( Code == XSSH_OK ) {
		memcpy(
			pHostKeyStorage,
			Message.ServerHostKey.Data,
			Message.ServerHostKey.Size
		);
		Session.ServerHostKey = (xbytesview){
			(const unsigned char*)pHostKeyStorage,
			Message.ServerHostKey.Size
		};
		Session.ReadPending = XSSH_KEX_PACKET_ECDH_REPLY;
		*pSession = Session;
	}
	xrtSecureZero(&Session, sizeof(Session));
	return Code;
}



/* 准备一个 core 已认证的 KEX 方法或 NEWKEYS payload。 */
xsshcode xrtSshKexSessionReadPrepare(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize
)
{
	uint8 iMessage;
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) || !pSession->Active ||
		(pSession->ReadPending != XSSH_KEX_PACKET_NONE) ||
		!xrtMemRangeValid(Payload.Data, Payload.Size) ||
		(Payload.Size == 0u) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pHostKeySize != NULL) &&
		(!xrtMemRangeValid(pHostKeySize, sizeof(*pHostKeySize)) ||
		 xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pHostKeySize,
			sizeof(*pHostKeySize)
		 ) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pHostKeySize,
			sizeof(*pHostKeySize)
		 ) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pHostKeySize,
			sizeof(*pHostKeySize)
		 )) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pHostKeyStorage,
		iHostKeyCapacity
	) || xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pHostKeyStorage,
		iHostKeyCapacity
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pHostKeySize != NULL ) {
		*pHostKeySize = 0u;
	}
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshKexSessionCoreReadMatches(pCore, Payload, iMessage) ) {
		return XSSH_ERROR_STATE;
	}
	if ( xsshKexSessionDiscardGuess(pCore, iMessage) ) {
		pSession->ReadPending = XSSH_KEX_PACKET_DISCARD;
		return XSSH_OK;
	}
	if ( iMessage == XSSH_MSG_NEWKEYS ) {
		if ( pSession->PeerNewKeys ||
			(pSession->Role == XSSH_ROLE_CLIENT ?
			 !pSession->MethodReadCommitted :
			 !pSession->MethodWriteCommitted) ) {
			return XSSH_ERROR_STATE;
		}
		Code = xrtSshNewKeysRead(Payload);
		if ( Code == XSSH_OK ) {
			pSession->ReadPending = XSSH_KEX_PACKET_NEWKEYS;
		}
		return Code;
	}
	if ( pSession->Role == XSSH_ROLE_SERVER ) {
		if ( (iMessage != XSSH_MSG_KEX_ECDH_INIT) ||
			pSession->MethodReadCommitted ) {
			return XSSH_ERROR_PROTOCOL;
		}
		return xsshKexSessionReadInitPrepare(pSession, Payload);
	}
	if ( (iMessage != XSSH_MSG_KEX_ECDH_REPLY) ||
		!pSession->MethodWriteCommitted ||
		pSession->MethodReadCommitted ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return xsshKexSessionReadReplyPrepare(
		pSession,
		Payload,
		pHostKeyStorage,
		iHostKeyCapacity,
		pHostKeySize
	);
}



/* 在 transport 已提交输入后推进 KEX 读事务。 */
xsshcode xrtSshKexSessionReadCommit(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshkexsessionpacket Packet;
	uint8 iMessage;

	if ( !xsshKexSessionValid(pSession) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		(pCore->State.Role != pSession->Role) || pCore->Read.Active ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) ||
		(pSession->ReadPending == XSSH_KEX_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	Packet = pSession->ReadPending;
	if ( Packet == XSSH_KEX_PACKET_DISCARD ) {
		if ( !pCore->State.PeerGuessSeen ) {
			return XSSH_ERROR_STATE;
		}
	} else if ( Packet == XSSH_KEX_PACKET_NEWKEYS ) {
		if ( !xrtSshTransportCoreReadKeysPending(pCore) ) {
			return XSSH_ERROR_STATE;
		}
		pSession->PeerNewKeys = true;
	} else {
		iMessage = Packet == XSSH_KEX_PACKET_ECDH_INIT ?
			XSSH_MSG_KEX_ECDH_INIT : XSSH_MSG_KEX_ECDH_REPLY;
		if ( pCore->State.PeerKexRemaining[
			(size_t)iMessage - XSSH_KEX_METHOD_MIN
		] != 0u ) {
			return XSSH_ERROR_STATE;
		}
		pSession->MethodReadCommitted = true;
		if ( Packet == XSSH_KEX_PACKET_ECDH_REPLY ) {
			pSession->HostKeyVerified = true;
		}
	}
	pSession->ReadPending = XSSH_KEX_PACKET_NONE;
	xsshKexSessionUpdatePhase(pSession);
	return XSSH_OK;
}



/* 已认证输入不可回滚，放弃时终止会话。 */
xsshcode xrtSshKexSessionReadAbort(xsshkexsession* pSession)
{
	if ( !xsshKexSessionValid(pSession) ||
		(pSession->ReadPending == XSSH_KEX_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	xsshKexSessionSetFailed(pSession);
	return XSSH_OK;
}



/* 激活角色对应的本端写方向密钥。 */
xsshcode xrtSshKexSessionActivateWrite(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xbytesview Key;
	xbytesview IV;
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		(pCore->State.Role != pSession->Role) || !pSession->KeysDerived ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) ||
		!pSession->LocalNewKeys || pSession->WriteActivated ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->Role == XSSH_ROLE_CLIENT ) {
		Key = (xbytesview){
			pSession->ClientToServerKey,
			pSession->ClientToServerKeySize
		};
		IV = (xbytesview){
			pSession->ClientToServerIV,
			sizeof(pSession->ClientToServerIV)
		};
	} else {
		Key = (xbytesview){
			pSession->ServerToClientKey,
			pSession->ServerToClientKeySize
		};
		IV = (xbytesview){
			pSession->ServerToClientIV,
			sizeof(pSession->ServerToClientIV)
		};
	}
	Code = xrtSshTransportCoreSetWriteAesGcm(pCore, Key, IV, Timer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	xrtSecureZero((void*)Key.Data, Key.Size);
	xrtSecureZero((void*)IV.Data, IV.Size);
	pSession->WriteActivated = true;
	xsshKexSessionUpdatePhase(pSession);
	if ( pSession->Phase == XSSH_KEX_SESSION_COMPLETE ) {
		xrtSecureZero(pSession->PrivateKey, sizeof(pSession->PrivateKey));
		xrtSecureZero(pSession->SharedSecret, sizeof(pSession->SharedSecret));
	}
	return XSSH_OK;
}



/* 激活角色对应的 peer 读方向密钥。 */
xsshcode xrtSshKexSessionActivateRead(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xbytesview Key;
	xbytesview IV;
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		(pCore->State.Role != pSession->Role) || !pSession->KeysDerived ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) ||
		!pSession->PeerNewKeys || pSession->ReadActivated ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->Role == XSSH_ROLE_CLIENT ) {
		Key = (xbytesview){
			pSession->ServerToClientKey,
			pSession->ServerToClientKeySize
		};
		IV = (xbytesview){
			pSession->ServerToClientIV,
			sizeof(pSession->ServerToClientIV)
		};
	} else {
		Key = (xbytesview){
			pSession->ClientToServerKey,
			pSession->ClientToServerKeySize
		};
		IV = (xbytesview){
			pSession->ClientToServerIV,
			sizeof(pSession->ClientToServerIV)
		};
	}
	Code = xrtSshTransportCoreSetReadAesGcm(pCore, Key, IV, Timer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	xrtSecureZero((void*)Key.Data, Key.Size);
	xrtSecureZero((void*)IV.Data, IV.Size);
	pSession->ReadActivated = true;
	xsshKexSessionUpdatePhase(pSession);
	if ( pSession->Phase == XSSH_KEX_SESSION_COMPLETE ) {
		xrtSecureZero(pSession->PrivateKey, sizeof(pSession->PrivateKey));
		xrtSecureZero(pSession->SharedSecret, sizeof(pSession->SharedSecret));
	}
	return XSSH_OK;
}



/* 校验会话与 transport core 双方都已完成一代 KEX。 */
bool xrtSshKexSessionComplete(
	const xsshkexsession* pSession,
	const xsshtransportcore* pCore
)
{
	return xsshKexSessionValid(pSession) &&
		xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) &&
		(pCore->State.Role == pSession->Role) &&
		(pSession->Phase == XSSH_KEX_SESSION_COMPLETE) &&
		pSession->WriteActivated && pSession->ReadActivated &&
		xrtSshTransportCoreKexComplete(pCore);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_kex_session_random.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEX_SESSION_RANDOM)



#if defined(XSSH_FEATURE_KEX_SESSION_RANDOM)

/* 生成临时密钥，并保证所有退出路径都清除私钥副本。 */
xsshcode xrtSshKexSessionBegin(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	const xsshkextranscript* pTranscript,
	xbytesview ServerHostKey
)
{
	uint8 arrPrivate[XSSH_CURVE25519_PRIVATE_SIZE];
	uint8 arrPublic[XSSH_CURVE25519_PUBLIC_SIZE];
	xsshcode Code;

	Code = xrtSshCurve25519KeyPair(arrPrivate, arrPublic);
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexSessionBeginWithPrivate(
			pSession,
			pCore,
			pTranscript,
			ServerHostKey,
			(xbytesview){ arrPrivate, sizeof(arrPrivate) }
		);
	}
	xrtSecureZero(arrPrivate, sizeof(arrPrivate));
	xrtSecureZero(arrPublic, sizeof(arrPublic));
	return Code;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_kex_exchange.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEX_EXCHANGE)
#include <string.h>




#if defined(XSSH_FEATURE_KEX_EXCHANGE)

#define XSSH_KEX_EXCHANGE_GUARD UINT32_C(0x4b455845)



/* 校验角色和 transport 方向。 */
static bool xsshKexExchangeRoleValid(
	xsshrole Role,
	xsshtransportdirection Direction
)
{
	return ((Role == XSSH_ROLE_CLIENT) || (Role == XSSH_ROLE_SERVER)) &&
		((Direction == XSSH_TRANSPORT_LOCAL) ||
		 (Direction == XSSH_TRANSPORT_PEER));
}



/* 校验对象哨兵、阶段、事务类型和全部缓冲均无尾部预留。 */
static bool xsshKexExchangeValid(const xsshkexexchange* pExchange)
{
	return xrtMemRangeValid(pExchange, sizeof(*pExchange)) &&
		(pExchange->Guard == XSSH_KEX_EXCHANGE_GUARD) &&
		pExchange->Initialized &&
		xsshKexExchangeRoleValid(
			pExchange->Role,
			pExchange->PendingDirection
		) && (pExchange->Session.Role == pExchange->Role) &&
		(pExchange->Phase >= XSSH_KEX_EXCHANGE_IDENTIFICATION) &&
		(pExchange->Phase <= XSSH_KEX_EXCHANGE_FAILED) &&
		(pExchange->Pending >= XSSH_KEX_EXCHANGE_PENDING_NONE) &&
		(pExchange->Pending <= XSSH_KEX_EXCHANGE_PENDING_KEXINIT) &&
		(pExchange->ClientVersion.Reserved == NULL) &&
		(pExchange->ServerVersion.Reserved == NULL) &&
		(pExchange->ClientKexInit.Reserved == NULL) &&
		(pExchange->ServerKexInit.Reserved == NULL) &&
		(pExchange->NextClientKexInit.Reserved == NULL) &&
		(pExchange->NextServerKexInit.Reserved == NULL) &&
		(pExchange->Staging.Reserved == NULL);
}



/* 校验对象没有未决保存事务。 */
static bool xsshKexExchangeStable(const xsshkexexchange* pExchange)
{
	return xsshKexExchangeValid(pExchange) &&
		(pExchange->Pending == XSSH_KEX_EXCHANGE_PENDING_NONE) &&
		xrtNetBufEmpty(&pExchange->Staging);
}



/* 校验 transport core 与交换对象互不重叠且角色一致。 */
static bool xsshKexExchangeCoreValid(
	const xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pExchange,
			sizeof(*pExchange),
			pCore,
			sizeof(*pCore)
		) && (pCore->State.Role == pExchange->Role);
}



/* 判断一个 local/peer 方向在连接两端对应 client 还是 server。 */
static bool xsshKexExchangeIsClient(
	const xsshkexexchange* pExchange,
	xsshtransportdirection Direction
)
{
	return (pExchange->Role == XSSH_ROLE_CLIENT) ==
		(Direction == XSSH_TRANSPORT_LOCAL);
}



/* 返回连接级 client/server 版本缓冲。 */
static xnetbuf* xsshKexExchangeVersionBuffer(
	xsshkexexchange* pExchange,
	xsshtransportdirection Direction
)
{
	return xsshKexExchangeIsClient(pExchange, Direction) ?
		&pExchange->ClientVersion : &pExchange->ServerVersion;
}



/* 返回下一代 client/server KEXINIT 缓冲。 */
static xnetbuf* xsshKexExchangeNextBuffer(
	xsshkexexchange* pExchange,
	xsshtransportdirection Direction
)
{
	return xsshKexExchangeIsClient(pExchange, Direction) ?
		&pExchange->NextClientKexInit :
		&pExchange->NextServerKexInit;
}



/* 返回方向对应的累计 packet 数。 */
static uint64 xsshKexExchangePackets(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pCore->State.LocalPackets : pCore->State.PeerPackets;
}



/* 返回方向对应的本代 KEXINIT 序号。 */
static uint64 xsshKexExchangeOrdinal(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pCore->State.LocalKexInitOrdinal :
		pCore->State.PeerKexInitOrdinal;
}



/* 返回方向是否已经提交本代 KEXINIT。 */
static bool xsshKexExchangeKexInitCommitted(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pCore->State.LocalKexInit : pCore->State.PeerKexInit;
}



/* 返回方向记录的 guessed-packet 标志。 */
static bool xsshKexExchangeGuessExpected(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pCore->State.LocalGuessExpected :
		pCore->State.PeerGuessExpected;
}



/* 返回方向对应的 identification 提交位。 */
static bool xsshKexExchangeIdentificationCommitted(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pCore->State.LocalIdentification :
		pCore->State.PeerIdentification;
}



/* 返回与方向对应的 transport 未决 packet。 */
static const xsshtransportpending* xsshKexExchangeCorePending(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		&pCore->Write : &pCore->Read;
}



/* 清除保存事务元数据，不触碰任何动态块。 */
static void xsshKexExchangePendingClear(xsshkexexchange* pExchange)
{
	pExchange->PendingOrdinal = 0u;
	pExchange->PendingKexCount = 0u;
	pExchange->PendingDirection = XSSH_TRANSPORT_LOCAL;
	pExchange->PendingCorePhase = XSSH_TRANSPORT_IDENTIFICATION;
	pExchange->Pending = XSSH_KEX_EXCHANGE_PENDING_NONE;
	pExchange->PendingFirstKexPacketFollows = false;
}



/* 校验本端或对端无换行 identification。 */
static xsshcode xsshKexExchangeVersionValidate(
	xsshtransportdirection Direction,
	xstrview Version
)
{
	char arrLine[XSSH_IDENTIFICATION_MAX];
	xsshwriter Writer;
	xstrview Parsed;
	size_t iConsumed;
	xsshcode Code;

	if ( !xrtMemRangeValid(Version.Data, Version.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		if ( !xrtSshWriterInit(&Writer, arrLine, sizeof(arrLine)) ) {
			return XSSH_ERROR_STATE;
		}
		return xrtSshBannerWrite(&Writer, Version);
	}
	if ( Version.Size == 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( Version.Size >= sizeof(arrLine) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	memcpy(arrLine, Version.Data, Version.Size);
	arrLine[Version.Size] = '\n';
	Code = xrtSshBannerRead(
		(xstrview){ arrLine, Version.Size + 1u },
		&Parsed,
		&iConsumed
	);
	if ( (Code == XSSH_OK) &&
		((Parsed.Size != Version.Size) ||
		 (iConsumed != (Version.Size + 1u)) ||
		 (memcmp(Parsed.Data, Version.Data, Version.Size) != 0)) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return Code;
}



/* 将一个非空动态链连续化为稳定借用视图。 */
static bool xsshKexExchangeView(xnetbuf* pBuffer, xbytesview* pView)
{
	xnetspan Span;
	size_t iSize = xrtNetBufSize(pBuffer);

	if ( (iSize == 0u) || !xrtNetBufPullup(pBuffer, iSize, &Span) ||
		(Span.Size < iSize) ) {
		return false;
	}
	pView->Data = Span.Data;
	pView->Size = iSize;
	return true;
}



/* 选择 READY 的下一代或 METHOD/COMPLETE 的当前代 KEXINIT。 */
static bool xsshKexExchangeTranscriptBuffers(
	xsshkexexchange* pExchange,
	xnetbuf** ppClient,
	xnetbuf** ppServer
)
{
	if ( pExchange->Phase == XSSH_KEX_EXCHANGE_READY ) {
		*ppClient = &pExchange->NextClientKexInit;
		*ppServer = &pExchange->NextServerKexInit;
		return true;
	}
	if ( (pExchange->Phase == XSSH_KEX_EXCHANGE_METHOD) ||
		(pExchange->Phase == XSSH_KEX_EXCHANGE_COMPLETE) ) {
		*ppClient = &pExchange->ClientKexInit;
		*ppServer = &pExchange->ServerKexInit;
		return true;
	}
	return false;
}



/* 初始化动态 transcript 与可重复 rekey 的 KEX 会话。 */
bool xrtSshKexExchangeInit(
	xsshkexexchange* pExchange,
	xnetbufpool* pPool,
	xsshrole Role
)
{
	xsshkexexchange Exchange;

	if ( !xrtMemRangeValid(pExchange, sizeof(*pExchange)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	memset(&Exchange, 0, sizeof(Exchange));
	if ( !xrtSshKexSessionInit(&Exchange.Session, Role) ||
		!xrtNetBufInit(&Exchange.ClientVersion, pPool) ||
		!xrtNetBufInit(&Exchange.ServerVersion, pPool) ||
		!xrtNetBufInit(&Exchange.ClientKexInit, pPool) ||
		!xrtNetBufInit(&Exchange.ServerKexInit, pPool) ||
		!xrtNetBufInit(&Exchange.NextClientKexInit, pPool) ||
		!xrtNetBufInit(&Exchange.NextServerKexInit, pPool) ||
		!xrtNetBufInit(&Exchange.Staging, pPool) ) {
		xrtSshKexSessionClear(&Exchange.Session);
		return false;
	}
	Exchange.Role = Role;
	Exchange.PendingDirection = XSSH_TRANSPORT_LOCAL;
	Exchange.PendingCorePhase = XSSH_TRANSPORT_IDENTIFICATION;
	Exchange.Phase = XSSH_KEX_EXCHANGE_IDENTIFICATION;
	Exchange.Initialized = true;
	Exchange.Guard = XSSH_KEX_EXCHANGE_GUARD;
	*pExchange = Exchange;
	return true;
}



/* 释放 transcript 并安全清除 KEX 会话。 */
void xrtSshKexExchangeClear(xsshkexexchange* pExchange)
{
	if ( xsshKexExchangeValid(pExchange) ) {
		xrtNetBufClear(&pExchange->ClientVersion);
		xrtNetBufClear(&pExchange->ServerVersion);
		xrtNetBufClear(&pExchange->ClientKexInit);
		xrtNetBufClear(&pExchange->ServerKexInit);
		xrtNetBufClear(&pExchange->NextClientKexInit);
		xrtNetBufClear(&pExchange->NextServerKexInit);
		xrtNetBufClear(&pExchange->Staging);
		xrtSshKexSessionClear(&pExchange->Session);
	}
	if ( xrtMemRangeValid(pExchange, sizeof(*pExchange)) ) {
		memset(pExchange, 0, sizeof(*pExchange));
	}
}



/* 在 transport 提交前保存经过 wire 规则校验的版本串。 */
xsshcode xrtSshKexExchangeVersionPrepare(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xstrview Version
)
{
	xnetbuf* pTarget;
	xsshcode Code;

	if ( !xsshKexExchangeStable(pExchange) ||
		!xsshKexExchangeRoleValid(pExchange->Role, Direction) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		(pExchange->Phase != XSSH_KEX_EXCHANGE_IDENTIFICATION) ) {
		return XSSH_ERROR_STATE;
	}
	if ( xrtMemRangesOverlap(
		pExchange,
		sizeof(*pExchange),
		Version.Data,
		Version.Size
	) || xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		Version.Data,
		Version.Size
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	pTarget = xsshKexExchangeVersionBuffer(pExchange, Direction);
	if ( !xrtNetBufEmpty(pTarget) ||
		xsshKexExchangeIdentificationCommitted(pCore, Direction) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshKexExchangeVersionValidate(Direction, Version);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtNetBufAppend(
		&pExchange->Staging,
		Version.Data,
		Version.Size
	) ) {
		return XSSH_ERROR_SPACE;
	}
	pExchange->PendingDirection = Direction;
	pExchange->PendingCorePhase = pCore->State.Phase;
	pExchange->Pending = XSSH_KEX_EXCHANGE_PENDING_VERSION;
	return XSSH_OK;
}



/* 在 core 提交后发布稳定版本串。 */
xsshcode xrtSshKexExchangeVersionCommit(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	xnetbuf* pTarget;

	if ( !xsshKexExchangeValid(pExchange) ||
		(pExchange->Pending != XSSH_KEX_EXCHANGE_PENDING_VERSION) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		!xsshKexExchangeIdentificationCommitted(
			pCore,
			pExchange->PendingDirection
		) ) {
		return XSSH_ERROR_STATE;
	}
	pTarget = xsshKexExchangeVersionBuffer(
		pExchange,
		pExchange->PendingDirection
	);
	if ( !xrtNetBufEmpty(pTarget) ||
		!xrtNetBufMove(pTarget, &pExchange->Staging) ) {
		return XSSH_ERROR_STATE;
	}
	xsshKexExchangePendingClear(pExchange);
	if ( !xrtNetBufEmpty(&pExchange->ClientVersion) &&
		!xrtNetBufEmpty(&pExchange->ServerVersion) ) {
		pExchange->Phase = XSSH_KEX_EXCHANGE_KEXINIT;
	}
	return XSSH_OK;
}



/* 在 core 尚未提交 identification 时取消保存。 */
xsshcode xrtSshKexExchangeVersionAbort(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	if ( !xsshKexExchangeValid(pExchange) ||
		(pExchange->Pending != XSSH_KEX_EXCHANGE_PENDING_VERSION) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		xsshKexExchangeIdentificationCommitted(
			pCore,
			pExchange->PendingDirection
		) || (pCore->State.Phase != pExchange->PendingCorePhase) ) {
		return XSSH_ERROR_STATE;
	}
	xrtNetBufClear(&pExchange->Staging);
	xsshKexExchangePendingClear(pExchange);
	return XSSH_OK;
}



/* 本端在 core 写事务前保存 KEXINIT，对端关联已经认证的 core 读事务。 */
xsshcode xrtSshKexExchangeKexInitPrepare(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xbytesview Payload
)
{
	xsshkexinit KexInit;
	xnetbuf* pTarget;
	xsshcode Code;

	if ( !xsshKexExchangeStable(pExchange) ||
		!xsshKexExchangeRoleValid(pExchange->Role, Direction) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		((pExchange->Phase != XSSH_KEX_EXCHANGE_KEXINIT) &&
		 (pExchange->Phase != XSSH_KEX_EXCHANGE_COMPLETE)) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			pExchange,
			sizeof(*pExchange),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	pTarget = xsshKexExchangeNextBuffer(pExchange, Direction);
	if ( !xrtNetBufEmpty(pTarget) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshKexInitRead(Payload, &KexInit);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (Direction == XSSH_TRANSPORT_LOCAL) &&
		KexInit.FirstKexPacketFollows ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		if ( pCore->Write.Active || pCore->Read.Active ||
			(xrtSshTransportKexInitCheck(
				&pCore->State,
				XSSH_TRANSPORT_LOCAL
			) != XSSH_OK) ) {
			return XSSH_ERROR_STATE;
		}
	} else if ( !pCore->Read.Active || pCore->Write.Active ||
		(pCore->Read.Kind != XSSH_TRANSPORT_PACKET_KEXINIT) ||
		(pCore->Read.Message != XSSH_MSG_KEXINIT) ||
		(pCore->Read.FirstKexPacketFollows !=
		 KexInit.FirstKexPacketFollows) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtNetBufAppend(
		&pExchange->Staging,
		Payload.Data,
		Payload.Size
	) ) {
		return XSSH_ERROR_SPACE;
	}
	pExchange->PendingOrdinal = xsshKexExchangePackets(
		pCore,
		Direction
	);
	pExchange->PendingKexCount = pCore->State.KexCount;
	pExchange->PendingDirection = Direction;
	pExchange->PendingCorePhase = pCore->State.Phase;
	pExchange->Pending = XSSH_KEX_EXCHANGE_PENDING_KEXINIT;
	pExchange->PendingFirstKexPacketFollows =
		KexInit.FirstKexPacketFollows;
	return XSSH_OK;
}



/* 只在 core 已提交同一 packet 序号后发布下一代 KEXINIT。 */
xsshcode xrtSshKexExchangeKexInitCommit(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	const xsshtransportpending* pPending;
	xnetbuf* pTarget;
	uint64 iPackets;

	if ( !xsshKexExchangeValid(pExchange) ||
		(pExchange->Pending != XSSH_KEX_EXCHANGE_PENDING_KEXINIT) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		(pCore->State.KexCount != pExchange->PendingKexCount) ||
		(pExchange->PendingOrdinal == UINT64_MAX) ) {
		return XSSH_ERROR_STATE;
	}
	pPending = xsshKexExchangeCorePending(
		pCore,
		pExchange->PendingDirection
	);
	iPackets = xsshKexExchangePackets(
		pCore,
		pExchange->PendingDirection
	);
	if ( pPending->Active ||
		(pCore->State.Phase != XSSH_TRANSPORT_KEY_EXCHANGE) ||
		!xsshKexExchangeKexInitCommitted(
			pCore,
			pExchange->PendingDirection
		) || (xsshKexExchangeOrdinal(
			pCore,
			pExchange->PendingDirection
		) != pExchange->PendingOrdinal) ||
		(iPackets != (pExchange->PendingOrdinal + 1u)) ||
		(xsshKexExchangeGuessExpected(
			pCore,
			pExchange->PendingDirection
		) != pExchange->PendingFirstKexPacketFollows) ) {
		return XSSH_ERROR_STATE;
	}
	pTarget = xsshKexExchangeNextBuffer(
		pExchange,
		pExchange->PendingDirection
	);
	if ( !xrtNetBufEmpty(pTarget) ||
		!xrtNetBufMove(pTarget, &pExchange->Staging) ) {
		return XSSH_ERROR_STATE;
	}
	xsshKexExchangePendingClear(pExchange);
	pExchange->Phase = !xrtNetBufEmpty(
		&pExchange->NextClientKexInit
	) && !xrtNetBufEmpty(
		&pExchange->NextServerKexInit
	) ? XSSH_KEX_EXCHANGE_READY : XSSH_KEX_EXCHANGE_KEXINIT;
	return XSSH_OK;
}



/* packet 计数和 KEX 代际未推进时取消暂存副本。 */
xsshcode xrtSshKexExchangeKexInitAbort(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	if ( !xsshKexExchangeValid(pExchange) ||
		(pExchange->Pending != XSSH_KEX_EXCHANGE_PENDING_KEXINIT) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		(pCore->State.KexCount != pExchange->PendingKexCount) ||
		(pCore->State.Phase != pExchange->PendingCorePhase) ||
		(xsshKexExchangePackets(
			pCore,
			pExchange->PendingDirection
		) != pExchange->PendingOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	xrtNetBufClear(&pExchange->Staging);
	xsshKexExchangePendingClear(pExchange);
	return XSSH_OK;
}



/* 校验动态 transcript 和 transport 已到方法交换边界。 */
bool xrtSshKexExchangeReady(
	const xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	return xsshKexExchangeStable(pExchange) &&
		xsshKexExchangeCoreValid(pExchange, pCore) &&
		(pExchange->Phase == XSSH_KEX_EXCHANGE_READY) &&
		!xrtNetBufEmpty(&pExchange->ClientVersion) &&
		!xrtNetBufEmpty(&pExchange->ServerVersion) &&
		!xrtNetBufEmpty(&pExchange->NextClientKexInit) &&
		!xrtNetBufEmpty(&pExchange->NextServerKexInit) &&
		(pCore->State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		pCore->State.LocalKexInit && pCore->State.PeerKexInit &&
		!pCore->State.KexConfigured && !pCore->Write.Active &&
		!pCore->Read.Active;
}



/* 连续化四条动态链并返回不含 packet framing 的 transcript。 */
xsshcode xrtSshKexExchangeTranscript(
	xsshkexexchange* pExchange,
	xsshkextranscript* pTranscript
)
{
	xsshkextranscript Transcript;
	xnetbuf* pClient;
	xnetbuf* pServer;

	if ( !xsshKexExchangeStable(pExchange) ||
		!xrtMemRangeValid(pTranscript, sizeof(*pTranscript)) ||
		xrtMemRangesOverlap(
			pExchange,
			sizeof(*pExchange),
			pTranscript,
			sizeof(*pTranscript)
		) || !xsshKexExchangeTranscriptBuffers(
			pExchange,
			&pClient,
			&pServer
		) ) {
		return XSSH_ERROR_STATE;
	}
	memset(&Transcript, 0, sizeof(Transcript));
	if ( !xsshKexExchangeView(
		&pExchange->ClientVersion,
		&Transcript.ClientVersion
	) || !xsshKexExchangeView(
		&pExchange->ServerVersion,
		&Transcript.ServerVersion
	) || !xsshKexExchangeView(
		pClient,
		&Transcript.ClientKexInit
	) || !xsshKexExchangeView(
		pServer,
		&Transcript.ServerKexInit
	) ) {
		return XSSH_ERROR_SPACE;
	}
	return xrtSshKexTranscriptInit(
		pTranscript,
		Transcript.ClientVersion,
		Transcript.ServerVersion,
		Transcript.ClientKexInit,
		Transcript.ServerKexInit
	);
}



/* 配置本代 KEX，并在成功后释放旧代动态块。 */
xsshcode xrtSshKexExchangeBeginWithPrivate(
	xsshkexexchange* pExchange,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
)
{
	xsshkextranscript Transcript;
	xsshcode Code;

	if ( !xrtSshKexExchangeReady(pExchange, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshKexExchangeTranscript(pExchange, &Transcript);
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexSessionBeginWithPrivate(
			&pExchange->Session,
			pCore,
			&Transcript,
			ServerHostKey,
			PrivateKey
		);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	xrtNetBufClear(&pExchange->ClientKexInit);
	xrtNetBufClear(&pExchange->ServerKexInit);
	if ( !xrtNetBufMove(
		&pExchange->ClientKexInit,
		&pExchange->NextClientKexInit
	) || !xrtNetBufMove(
		&pExchange->ServerKexInit,
		&pExchange->NextServerKexInit
	) ) {
		xrtSshKexExchangeFail(pExchange);
		xrtSshTransportCoreClose(pCore);
		return XSSH_ERROR_STATE;
	}
	pExchange->Phase = XSSH_KEX_EXCHANGE_METHOD;
	return XSSH_OK;
}



/* 借出可推进的 KEX 会话。 */
xsshkexsession* xrtSshKexExchangeSession(xsshkexexchange* pExchange)
{
	return xsshKexExchangeStable(pExchange) &&
		((pExchange->Phase == XSSH_KEX_EXCHANGE_METHOD) ||
		 (pExchange->Phase == XSSH_KEX_EXCHANGE_COMPLETE)) ?
		&pExchange->Session : NULL;
}



/* 借出只读 KEX 会话。 */
const xsshkexsession* xrtSshKexExchangeSessionConst(
	const xsshkexexchange* pExchange
)
{
	return xsshKexExchangeStable(pExchange) &&
		((pExchange->Phase == XSSH_KEX_EXCHANGE_METHOD) ||
		 (pExchange->Phase == XSSH_KEX_EXCHANGE_COMPLETE)) ?
		&pExchange->Session : NULL;
}



/* 确认会话与 transport 都完成本代密钥切换。 */
xsshcode xrtSshKexExchangeComplete(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	if ( !xsshKexExchangeStable(pExchange) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		(pExchange->Phase != XSSH_KEX_EXCHANGE_METHOD) ||
		!xrtSshKexSessionComplete(&pExchange->Session, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	pExchange->Phase = XSSH_KEX_EXCHANGE_COMPLETE;
	return XSSH_OK;
}



/* 终止交换并释放尚未晋升的材料。 */
void xrtSshKexExchangeFail(xsshkexexchange* pExchange)
{
	if ( xsshKexExchangeValid(pExchange) ) {
		xrtNetBufClear(&pExchange->NextClientKexInit);
		xrtNetBufClear(&pExchange->NextServerKexInit);
		xrtNetBufClear(&pExchange->Staging);
		xrtSshKexSessionFail(&pExchange->Session);
		xsshKexExchangePendingClear(pExchange);
		pExchange->Phase = XSSH_KEX_EXCHANGE_FAILED;
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/kex/ssh_kex_exchange_random.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM)



#if defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM)

/* 用安全随机临时密钥委托给确定性交换核心。 */
xsshcode xrtSshKexExchangeBegin(
	xsshkexexchange* pExchange,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey
)
{
	uint8 arrPrivate[XSSH_CURVE25519_PRIVATE_SIZE];
	uint8 arrPublic[XSSH_CURVE25519_PUBLIC_SIZE];
	xsshcode Code;

	if ( !xrtSshKexExchangeReady(pExchange, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshCurve25519KeyPair(arrPrivate, arrPublic);
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexExchangeBeginWithPrivate(
			pExchange,
			pCore,
			ServerHostKey,
			(xbytesview){ arrPrivate, sizeof(arrPrivate) }
		);
	}
	xrtSecureZero(arrPrivate, sizeof(arrPrivate));
	xrtSecureZero(arrPublic, sizeof(arrPublic));
	return Code;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/transport/ssh_transport_tcp.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_TRANSPORT_TCP)
#include <math.h>
#include <string.h>




#if defined(XSSH_FEATURE_TRANSPORT_TCP)

#define XSSH_TRANSPORT_TCP_GUARD UINT32_C(0x53545450)



/* 校验对象以及读写事务与缓冲借用的一致性。 */
static bool xsshTransportTcpValid(const xsshtransporttcp* pTransport)
{
	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		(pTransport->Guard != XSSH_TRANSPORT_TCP_GUARD) ||
		(pTransport->MaxBannerBytes < XSSH_IDENTIFICATION_MAX) ||
		(pTransport->WritePending > XSSH_TRANSPORT_TCP_PENDING_PACKET) ||
		(pTransport->ReadPending > XSSH_TRANSPORT_TCP_PENDING_PACKET) ) {
		return false;
	}
	if ( ((pTransport->ReadPending == XSSH_TRANSPORT_TCP_PENDING_NONE) !=
		 (pTransport->Input == NULL)) ||
		((pTransport->ReadPending == XSSH_TRANSPORT_TCP_PENDING_NONE) !=
		 (pTransport->ReadSize == 0u)) ||
		((pTransport->WritePending == XSSH_TRANSPORT_TCP_PENDING_NONE) !=
		 xrtNetBufEmpty(&pTransport->Output)) ||
		(pTransport->Output.Reserved != NULL) ) {
		return false;
	}
	return true;
}



/* 把适配层内部不变量错误映射到 XRT 结构化错误。 */
static void xsshTransportTcpError(xsshcode Code, cstr sMessage)
{
	xrtSetErrorInfo(
		Code == XSSH_ERROR_PROTOCOL ? XERR_PROTOCOL : XERR_INTERNAL,
		"xrt.ssh",
		(int32)Code,
		sMessage
	);
}



/* 清空读借用描述；底层网络缓冲的所有权始终属于 Stream。 */
static void xsshTransportTcpReadClear(xsshtransporttcp* pTransport)
{
	pTransport->Input = NULL;
	pTransport->ReadSize = 0u;
	pTransport->ReadPending = XSSH_TRANSPORT_TCP_PENDING_NONE;
}



/* 提交已知长度的输出预留，失败时同时回滚 packet 事务。 */
static xsshcode xsshTransportTcpOutputCommit(
	xsshtransporttcp* pTransport,
	size_t iSize,
	xsshtransporttcppending Pending
)
{
	if ( !xrtNetBufCommit(&pTransport->Output, iSize) ) {
		if ( Pending == XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
			(void)xrtSshTransportCoreWriteAbort(&pTransport->Core);
		}
		(void)xrtNetBufCancel(&pTransport->Output);
		return XSSH_ERROR_STATE;
	}
	pTransport->WritePending = Pending;
	return XSSH_OK;
}



/* 查找完整 identification 行，同时限制前置 banner 总量和单行长度。 */
static xsshcode xsshTransportTcpIdentificationEnd(
	const xsshtransporttcp* pTransport,
	const xnetbuf* pInput,
	size_t* pEnd
)
{
	size_t iAvailable = xrtNetBufSize(pInput);
	size_t iLineStart = 0u;

	while ( iLineStart < iAvailable ) {
		size_t iLine = xrtNetBufFind(
			pInput,
			(uint8)'\n',
			iLineStart
		);
		unsigned char arrPrefix[4];

		if ( iLine == XRT_NPOS ) {
			break;
		}
		if ( (iLine + 1u - iLineStart) > XSSH_IDENTIFICATION_MAX ) {
			return XSSH_ERROR_OVERFLOW;
		}
		if ( (iLine + 1u) > pTransport->MaxBannerBytes ) {
			return XSSH_ERROR_OVERFLOW;
		}
		if ( ((iLine - iLineStart) >= sizeof(arrPrefix)) &&
			(xrtNetBufPeek(
				pInput,
				iLineStart,
				arrPrefix,
				sizeof(arrPrefix)
			) == sizeof(arrPrefix)) &&
			(memcmp(arrPrefix, "SSH-", sizeof(arrPrefix)) == 0) ) {
			*pEnd = iLine + 1u;
			return XSSH_OK;
		}
		iLineStart = iLine + 1u;
		if ( iLineStart > pTransport->MaxBannerBytes ) {
			return XSSH_ERROR_OVERFLOW;
		}
	}
	if ( (iAvailable - iLineStart) >= XSSH_IDENTIFICATION_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( iAvailable >= pTransport->MaxBannerBytes ) {
		return XSSH_ERROR_OVERFLOW;
	}
	return XSSH_NEED_MORE;
}



/* 从缓冲链复制公开的四字节长度头，不连续化后续 packet。 */
static xsshcode xsshTransportTcpInspect(
	const xsshtransporttcp* pTransport,
	const xnetbuf* pInput,
	xsshpacketneed* pNeed
)
{
	unsigned char arrHead[4];
	xsshreader Reader;

	if ( xrtNetBufSize(pInput) < sizeof(arrHead) ) {
		return XSSH_NEED_MORE;
	}
	if ( xrtNetBufPeek(
		pInput,
		0u,
		arrHead,
		sizeof(arrHead)
	) != sizeof(arrHead) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrHead, sizeof(arrHead) }
	) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshTransportCoreInspect(
		&pTransport->Core,
		&Reader,
		pNeed
	);
}



/* 默认配置保留协议要求的 packet 能力并给前置 banner 明确硬边界。 */
bool xrtSshTransportTcpConfigInit(
	xsshtransporttcpconfig* pConfig,
	xsshrole Role
)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtSshRekeyPolicyInit(&pConfig->Rekey);
	pConfig->MaxBannerBytes = XSSH_TRANSPORT_TCP_BANNER_LIMIT_DEFAULT;
	pConfig->MaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	pConfig->Role = Role;
	return true;
}



/* 初始化结果先在局部对象闭合，失败不会发布半初始化资源。 */
bool xrtSshTransportTcpInit(
	xsshtransporttcp* pTransport,
	xnetbufpool* pPool,
	const xsshtransporttcpconfig* pConfig,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return false; }
	xsshtransporttcp Transport;
	xsshtransporttcpconfig Config;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		return false;
	}
	Config = *pConfig;
	if ( Config.MaxBannerBytes < XSSH_IDENTIFICATION_MAX ) {
		return false;
	}
	memset(&Transport, 0, sizeof(Transport));
	if ( !xrtNetBufInit(&Transport.Output, pPool) ||
		!xrtSshTransportCoreInit(
			&Transport.Core,
			Config.Role,
			Config.MaxPacketSize,
			&Config.Rekey,
			Timer
		) ) {
		xrtNetBufClear(&Transport.Output);
		xrtSshTransportCoreClear(&Transport.Core);
		return false;
	}
	Transport.MaxBannerBytes = Config.MaxBannerBytes;
	Transport.Guard = XSSH_TRANSPORT_TCP_GUARD;
	xrtSecureZero(pTransport, sizeof(*pTransport));
	*pTransport = Transport;
	xrtSecureZero(&Transport, sizeof(Transport));
	return true;
}



/* 未发送 packet 先回滚 core，再释放输出与 cipher。 */
void xrtSshTransportTcpClear(xsshtransporttcp* pTransport)
{
	if ( pTransport == NULL ) {
		return;
	}
	if ( xsshTransportTcpValid(pTransport) ) {
		if ( pTransport->WritePending ==
			XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
			(void)xrtSshTransportCoreWriteAbort(&pTransport->Core);
		}
		if ( pTransport->ReadPending ==
			XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
			(void)xrtSshTransportCoreReadAbort(&pTransport->Core);
		}
		xrtNetBufClear(&pTransport->Output);
		xrtSshTransportCoreClear(&pTransport->Core);
	}
	xrtSecureZero(pTransport, sizeof(*pTransport));
}



/* Core 借用不会改变 transport 生命周期。 */
xsshtransportcore* xrtSshTransportTcpCore(xsshtransporttcp* pTransport)
{
	return xsshTransportTcpValid(pTransport) ? &pTransport->Core : NULL;
}



/* 只读 Core 借用不会改变 transport 生命周期。 */
const xsshtransportcore* xrtSshTransportTcpCoreConst(
	const xsshtransporttcp* pTransport
)
{
	return xsshTransportTcpValid(pTransport) ? &pTransport->Core : NULL;
}



/* identification 最大只有 255 字节，仍复用池化动态块而不内嵌数组。 */
xsshcode xrtSshTransportTcpIdentificationPrepare(
	xsshtransporttcp* pTransport,
	xstrview Banner
)
{
	xnetwspan Span;
	xsshwriter Writer;
	xsshcode Code;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->WritePending != XSSH_TRANSPORT_TCP_PENDING_NONE) ||
		!xrtNetBufEmpty(&pTransport->Output) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Banner.Data, Banner.Size) ||
		(Banner.Size > (XSSH_IDENTIFICATION_MAX - 2u)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			Banner.Data,
			Banner.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtNetBufReserve(
		&pTransport->Output,
		Banner.Size + 2u,
		&Span
	) ) {
		return XSSH_ERROR_SPACE;
	}
	if ( !xrtSshWriterInit(
		&Writer,
		Span.Data,
		Banner.Size + 2u
	) ) {
		(void)xrtNetBufCancel(&pTransport->Output);
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshBannerWrite(&Writer, Banner);
	if ( Code != XSSH_OK ) {
		(void)xrtNetBufCancel(&pTransport->Output);
		return Code;
	}
	return xsshTransportTcpOutputCommit(
		pTransport,
		Writer.Size,
		XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION
	);
}



/* 精确预留一包最终线长，并让 core 保持网络受理前事务。 */
xsshcode xrtSshTransportTcpWritePrepareWithPadding(
	xsshtransporttcp* pTransport,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshpacketneed Need;
	xnetwspan Span;
	xsshwriter Writer;
	xsshcode Code;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->WritePending != XSSH_TRANSPORT_TCP_PENDING_NONE) ||
		!xrtNetBufEmpty(&pTransport->Output) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			Payload.Data,
			Payload.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPacketCodecWriteMeasure(
		&pTransport->Core.Codec,
		Payload.Size,
		&Need
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtNetBufReserve(
		&pTransport->Output,
		Need.WireSize,
		&Span
	) ) {
		return XSSH_ERROR_SPACE;
	}
	if ( !xrtSshWriterInit(&Writer, Span.Data, Need.WireSize) ) {
		(void)xrtNetBufCancel(&pTransport->Output);
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshTransportCoreWritePrepareWithPadding(
		&pTransport->Core,
		&Writer,
		Payload,
		pPadding,
		pUserData,
		Timer
	);
	if ( Code != XSSH_OK ) {
		(void)xrtNetBufCancel(&pTransport->Output);
		return Code;
	}
	if ( Writer.Size != Need.WireSize ) {
		(void)xrtSshTransportCoreWriteAbort(&pTransport->Core);
		(void)xrtNetBufCancel(&pTransport->Output);
		return XSSH_ERROR_STATE;
	}
	return xsshTransportTcpOutputCommit(
		pTransport,
		Writer.Size,
		XSSH_TRANSPORT_TCP_PENDING_PACKET
	);
}



/* TCP 成功接管动态链就是唯一可靠写提交边界。 */
xnetresult xrtSshTransportTcpWriteSubmit(
	xsshtransporttcp* pTransport,
	xnetstream* pStream,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	xsshtransporttcppending Pending;
	xsshrekeydecision Decision = XSSH_REKEY_NONE;
	xnetresult Result;
	xsshcode Code;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->WritePending == XSSH_TRANSPORT_TCP_PENDING_NONE) ||
		!xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pDecision,
			sizeof(*pDecision)
		) ) {
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.ssh",
			(int32)XSSH_ERROR_ARGUMENT,
			"invalid SSH TCP write submission"
		);
		return XNET_RESULT_ERROR;
	}
	Pending = pTransport->WritePending;
	Result = xrtNetStreamSendBuffer(pStream, &pTransport->Output);
	if ( Result != XNET_RESULT_OK ) {
		return Result;
	}
	if ( Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		Code = xrtSshTransportCoreIdentificationCommit(
			&pTransport->Core,
			XSSH_TRANSPORT_LOCAL
		);
	} else {
		Code = xrtSshTransportCoreWriteCommit(
			&pTransport->Core,
			Timer,
			&Decision
		);
	}
	pTransport->WritePending = XSSH_TRANSPORT_TCP_PENDING_NONE;
	if ( Code != XSSH_OK ) {
		xsshTransportTcpError(
			Code,
			"SSH state rejected TCP-accepted output"
		);
		xrtSshTransportCoreClose(&pTransport->Core);
		(void)xrtNetStreamAbort(pStream);
		return XNET_RESULT_ERROR;
	}
	*pDecision = Decision;
	return XNET_RESULT_OK;
}



/* 未进入 TCP 队列的动态链与 packet 事务同时回滚。 */
xsshcode xrtSshTransportTcpWriteAbort(xsshtransporttcp* pTransport)
{
	xsshcode Code = XSSH_OK;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->WritePending == XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pTransport->WritePending ==
		XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
		Code = xrtSshTransportCoreWriteAbort(&pTransport->Core);
	}
	xrtNetBufClear(&pTransport->Output);
	pTransport->WritePending = XSSH_TRANSPORT_TCP_PENDING_NONE;
	return Code;
}



/* 输出链是唯一待重试 packet 存储。 */
size_t xrtSshTransportTcpWriteSize(
	const xsshtransporttcp* pTransport
)
{
	return xsshTransportTcpValid(pTransport) ?
		xrtNetBufSize(&pTransport->Output) : 0u;
}



/* identification 只连续化最终需要借出的前缀。 */
xsshcode xrtSshTransportTcpIdentificationReadPrepare(
	xsshtransporttcp* pTransport,
	xnetbuf* pInput,
	xstrview* pBanner
)
{
	xnetspan Span;
	xstrview Banner;
	size_t iConsumed;
	size_t iEnd;
	xsshcode Code;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->ReadPending != XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pInput == NULL) ||
		!xrtMemRangeValid(pBanner, sizeof(*pBanner)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pInput,
			sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pInput,
			sizeof(*pInput),
			pBanner,
			sizeof(*pBanner)
		) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pBanner,
			sizeof(*pBanner)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshTransportTcpIdentificationEnd(
		pTransport,
		pInput,
		&iEnd
	);
	if ( Code != XSSH_OK ) {
		if ( (Code == XSSH_ERROR_PROTOCOL) ||
			(Code == XSSH_ERROR_OVERFLOW) ||
			(Code == XSSH_ERROR_UNSUPPORTED) ) {
			xrtSshTransportCoreClose(&pTransport->Core);
		}
		return Code;
	}
	if ( !xrtNetBufPullup(pInput, iEnd, &Span) ) {
		return XSSH_ERROR_SPACE;
	}
	Code = xrtSshBannerRead(
		(xstrview){ (const char*)Span.Data, iEnd },
		&Banner,
		&iConsumed
	);
	if ( (Code != XSSH_OK) || (iConsumed != iEnd) ) {
		if ( (Code == XSSH_ERROR_PROTOCOL) ||
			(Code == XSSH_ERROR_OVERFLOW) ||
			(Code == XSSH_ERROR_UNSUPPORTED) ||
			(Code == XSSH_OK) ) {
			xrtSshTransportCoreClose(&pTransport->Core);
		}
		return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
	}
	pTransport->Input = pInput;
	pTransport->ReadSize = iConsumed;
	pTransport->ReadPending =
		XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION;
	*pBanner = Banner;
	return XSSH_OK;
}



/* Packet 探测不改变输入链或 transport。 */
xsshcode xrtSshTransportTcpReadInspect(
	const xsshtransporttcp* pTransport,
	const xnetbuf* pInput,
	xsshpacketneed* pNeed
)
{
	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->ReadPending != XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pInput == NULL) ||
		!xrtMemRangeValid(pNeed, sizeof(*pNeed)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pInput,
			sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pInput,
			sizeof(*pInput),
			pNeed,
			sizeof(*pNeed)
		) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pNeed,
			sizeof(*pNeed)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xsshTransportTcpInspect(pTransport, pInput, pNeed);
}



/* 完整 packet 只在本次借用期间连续，后续 packet 不参与复制。 */
xsshcode xrtSshTransportTcpReadPrepare(
	xsshtransporttcp* pTransport,
	xnetbuf* pInput,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshpacketneed Need;
	xnetspan Span;
	xsshreader Reader;
	xsshcode Code;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->ReadPending != XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pInput == NULL) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pInput,
			sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pPlain,
			iPlainCapacity
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshTransportTcpInspect(pTransport, pInput, &Need);
	if ( Code != XSSH_OK ) {
		if ( (Code == XSSH_ERROR_PROTOCOL) ||
			(Code == XSSH_ERROR_OVERFLOW) ||
			(Code == XSSH_ERROR_AUTHENTICATION) ||
			(Code == XSSH_ERROR_STATE) ) {
			xrtSshTransportCoreClose(&pTransport->Core);
		}
		return Code;
	}
	if ( xrtNetBufSize(pInput) < Need.WireSize ) {
		return XSSH_NEED_MORE;
	}
	if ( !xrtNetBufPullup(pInput, Need.WireSize, &Span) ) {
		return XSSH_ERROR_SPACE;
	}
	if ( !xrtSshReaderInit(
		&Reader,
		(xbytesview){ Span.Data, Need.WireSize }
	) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshTransportCoreReadPrepare(
		&pTransport->Core,
		&Reader,
		pPacket,
		pPlain,
		iPlainCapacity,
		Timer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Reader.Position != Need.WireSize ) {
		(void)xrtSshTransportCoreReadAbort(&pTransport->Core);
		return XSSH_ERROR_STATE;
	}
	pTransport->Input = pInput;
	pTransport->ReadSize = Need.WireSize;
	pTransport->ReadPending = XSSH_TRANSPORT_TCP_PENDING_PACKET;
	return XSSH_OK;
}



/* Core 先提交，随后底层链必须精确消费同一借用前缀。 */
xsshcode xrtSshTransportTcpReadCommit(
	xsshtransporttcp* pTransport,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshrekeydecision Decision = XSSH_REKEY_NONE;
	xsshcode Code;
	size_t iConsumed;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->ReadPending == XSSH_TRANSPORT_TCP_PENDING_NONE) ||
		!xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pTransport->ReadPending ==
		XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		Code = xrtSshTransportCoreIdentificationCommit(
			&pTransport->Core,
			XSSH_TRANSPORT_PEER
		);
	} else {
		Code = xrtSshTransportCoreReadCommit(
			&pTransport->Core,
			Timer,
			&Decision
		);
	}
	if ( Code != XSSH_OK ) {
		xrtSshTransportCoreClose(&pTransport->Core);
		(void)xrtNetBufConsume(
			pTransport->Input,
			pTransport->ReadSize
		);
		xsshTransportTcpReadClear(pTransport);
		return Code;
	}
	iConsumed = xrtNetBufConsume(
		pTransport->Input,
		pTransport->ReadSize
	);
	if ( iConsumed != pTransport->ReadSize ) {
		xrtSshTransportCoreClose(&pTransport->Core);
		xsshTransportTcpReadClear(pTransport);
		return XSSH_ERROR_STATE;
	}
	xsshTransportTcpReadClear(pTransport);
	*pDecision = Decision;
	return XSSH_OK;
}



/* 上层拒绝借用数据后关闭协议状态，并释放对应网络前缀。 */
xsshcode xrtSshTransportTcpReadAbort(xsshtransporttcp* pTransport)
{
	xsshcode Code = XSSH_OK;
	size_t iConsumed;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->ReadPending == XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pTransport->ReadPending == XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
		Code = xrtSshTransportCoreReadAbort(&pTransport->Core);
	} else {
		xrtSshTransportCoreClose(&pTransport->Core);
	}
	iConsumed = xrtNetBufConsume(
		pTransport->Input,
		pTransport->ReadSize
	);
	if ( iConsumed != pTransport->ReadSize ) {
		Code = XSSH_ERROR_STATE;
	}
	xsshTransportTcpReadClear(pTransport);
	return Code;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/transport/ssh_transport_tcp_random.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM)
#include <math.h>



#if defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM)

/* 默认生产路径直接使用 XRT 系统安全随机源。 */
xsshcode xrtSshTransportTcpWritePrepare(
	xsshtransporttcp* pTransport,
	xbytesview Payload,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	return xrtSshTransportTcpWritePrepareWithPadding(
		pTransport,
		Payload,
		xrtSshSecurePadding,
		NULL,
		Timer
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/auth/ssh_auth_message.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_AUTH_MESSAGE)
#include <string.h>




#if defined(XSSH_FEATURE_AUTH_MESSAGE)

/* 转换文本视图为原始字节视图。 */
static xbytesview xsshAuthBytes(xstrview Text)
{
	xbytesview Value;

	Value.Data = (const unsigned char*)Text.Data;
	Value.Size = Text.Size;
	return Value;
}



/* 转换原始字节视图为不要求零结尾的文本视图。 */
static xstrview xsshAuthText(xbytesview Value)
{
	xstrview Text;

	Text.Data = (const char*)Value.Data;
	Text.Size = Value.Size;
	return Text;
}



/* 判断两个文本视图是否完全相同。 */
static bool xsshAuthTextEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 将一个 SSH string 的编码长度加入总长度。 */
static xsshcode xsshAuthAddString(xbytesview Value, size_t* pTotal)
{
	if ( (pTotal == NULL) ||
		((Value.Data == NULL) && (Value.Size != 0u)) ||
		(Value.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pTotal > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotal += 4u + Value.Size;
	return XSSH_OK;
}



/* 校验整条输出及其输入重叠，再返回可原子提交的 writer 副本。 */
static xsshcode xsshAuthPrepare(
	xsshwriter* pWriter,
	size_t iTotal,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pCopy
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( (pWriter == NULL) || (pCopy == NULL) ||
		((pInputs == NULL) && (iInputCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		pInputs,
		iInputCount
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	*pCopy = Writer;
	return XSSH_OK;
}



/* 初始化 reader 并消费指定 USERAUTH 消息号。 */
static xsshcode xsshAuthReader(
	xbytesview Payload,
	uint8 iExpected,
	xsshreader* pReader
)
{
	uint8 iMessage;
	xsshcode Code;

	if ( (pReader == NULL) || !xrtSshReaderInit(pReader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(pReader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return iMessage == iExpected ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 计算通用认证请求及其原始方法字段的总长度。 */
xsshcode xrtSshAuthRequestSize(
	xstrview User,
	xstrview Service,
	xstrview Method,
	size_t iFieldsSize,
	size_t* pSize
)
{
	xbytesview Values[3];
	size_t iTotal = 1u;
	xsshcode Code;

	Values[0] = xsshAuthBytes(User);
	Values[1] = xsshAuthBytes(Service);
	Values[2] = xsshAuthBytes(Method);
	if ( (pSize == NULL) || !xrtUtf8Valid(User, NULL) ||
		!xrtSshNameValid(Service) || !xrtSshNameValid(Method) ||
		xrtMemRangesOverlap(pSize, sizeof(*pSize), User.Data, User.Size) ||
		xrtMemRangesOverlap(pSize, sizeof(*pSize), Service.Data, Service.Size) ||
		xrtMemRangesOverlap(pSize, sizeof(*pSize), Method.Data, Method.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshAuthAddString(Values[0], &iTotal)) != XSSH_OK) ||
		((Code = xsshAuthAddString(Values[1], &iTotal)) != XSSH_OK) ||
		((Code = xsshAuthAddString(Values[2], &iTotal)) != XSSH_OK) ) {
		return Code;
	}
	if ( iFieldsSize > (SIZE_MAX - iTotal) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pSize = iTotal + iFieldsSize;
	return XSSH_OK;
}



/* 写入可保留未知方法字段的通用认证请求。 */
xsshcode xrtSshAuthRequestWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Service,
	xstrview Method,
	xbytesview Fields
)
{
	xbytesview arrInputs[4];
	xsshwriter Writer;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = xsshAuthBytes(User);
	arrInputs[1] = xsshAuthBytes(Service);
	arrInputs[2] = xsshAuthBytes(Method);
	arrInputs[3] = Fields;
	Code = xrtSshAuthRequestSize(
		User,
		Service,
		Method,
		Fields.Size,
		&iTotal
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshAuthPrepare(pWriter, iTotal, arrInputs, 4u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_USERAUTH_REQUEST) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[2]) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, Fields) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取通用认证请求并借用方法专用字段。 */
xsshcode xrtSshAuthRequestRead(
	xbytesview Payload,
	xsshauthrequest* pRequest
)
{
	xsshreader Reader;
	xsshauthrequest Request;
	xbytesview Value;
	xsshcode Code;

	if ( pRequest == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshAuthReader(Payload, XSSH_MSG_USERAUTH_REQUEST, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Request.User = xsshAuthText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Request.Service = xsshAuthText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Request.Method = xsshAuthText(Value);
	if ( !xrtUtf8Valid(Request.User, NULL) ||
		!xrtSshNameValid(Request.Service) ||
		!xrtSshNameValid(Request.Method) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadBytes(
		&Reader,
		xrtSshReaderRemaining(&Reader),
		&Request.Fields
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pRequest = Request;
	return XSSH_OK;
}



/* 写入标准 ssh-connection none 探测。 */
xsshcode xrtSshAuthNoneWrite(xsshwriter* pWriter, xstrview User)
{
	return xrtSshAuthRequestWrite(
		pWriter,
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_NONE),
		(xbytesview){ NULL, 0u }
	);
}



/* 严格读取标准 ssh-connection none 探测。 */
xsshcode xrtSshAuthNoneRead(xbytesview Payload, xstrview* pUser)
{
	xsshauthrequest Request;
	xsshcode Code;

	if ( pUser == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthRequestRead(Payload, &Request);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshAuthTextEqual(
		Request.Service,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION)
	) || !xsshAuthTextEqual(
		Request.Method,
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_NONE)
	) || (Request.Fields.Size != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pUser = Request.User;
	return XSSH_OK;
}



/* 写入认证失败及后续可用方法。 */
xsshcode xrtSshAuthFailureWrite(
	xsshwriter* pWriter,
	xstrview Methods,
	bool bPartialSuccess
)
{
	xbytesview Value = xsshAuthBytes(Methods);
	xsshwriter Writer;
	size_t iTotal = 2u;
	xsshcode Code;

	if ( !xrtSshNameListValid(Methods) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshAuthAddString(Value, &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshAuthPrepare(pWriter, iTotal, &Value, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_USERAUTH_FAILURE) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Value) != XSSH_OK) ||
		(xrtSshWriteBool(&Writer, bPartialSuccess) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取认证失败及后续可用方法。 */
xsshcode xrtSshAuthFailureRead(
	xbytesview Payload,
	xsshauthfailure* pFailure
)
{
	xsshreader Reader;
	xsshauthfailure Failure;
	xbytesview Methods;
	xsshcode Code;

	if ( pFailure == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshAuthReader(Payload, XSSH_MSG_USERAUTH_FAILURE, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Methods);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Failure.Methods = xsshAuthText(Methods);
	if ( !xrtSshNameListValid(Failure.Methods) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadBool(&Reader, &Failure.PartialSuccess);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pFailure = Failure;
	return XSSH_OK;
}



/* 写入无字段认证成功消息。 */
xsshcode xrtSshAuthSuccessWrite(xsshwriter* pWriter)
{
	xsshwriter Writer;
	xsshcode Code;

	Code = xsshAuthPrepare(pWriter, 1u, NULL, 0u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteByte(&Writer, XSSH_MSG_USERAUTH_SUCCESS) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取无字段认证成功消息。 */
xsshcode xrtSshAuthSuccessRead(xbytesview Payload)
{
	xsshreader Reader;
	xsshcode Code;

	Code = xsshAuthReader(Payload, XSSH_MSG_USERAUTH_SUCCESS, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtSshReaderRemaining(&Reader) == 0u ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 写入 UTF-8 认证横幅和可选 ASCII language tag。 */
xsshcode xrtSshAuthBannerWrite(
	xsshwriter* pWriter,
	xstrview Message,
	xstrview Language
)
{
	xbytesview arrInputs[2];
	xsshwriter Writer;
	size_t iTotal = 1u;
	xsshcode Code;

	arrInputs[0] = xsshAuthBytes(Message);
	arrInputs[1] = xsshAuthBytes(Language);
	if ( !xrtUtf8Valid(Message, NULL) ||
		!xrtSshLanguageValid(Language) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshAuthAddString(arrInputs[0], &iTotal)) != XSSH_OK) ||
		((Code = xsshAuthAddString(arrInputs[1], &iTotal)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshAuthPrepare(pWriter, iTotal, arrInputs, 2u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_USERAUTH_BANNER) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 UTF-8 认证横幅和 ASCII language tag。 */
xsshcode xrtSshAuthBannerRead(
	xbytesview Payload,
	xsshauthbanner* pBanner
)
{
	xsshreader Reader;
	xsshauthbanner Banner;
	xbytesview Value;
	xsshcode Code;

	if ( pBanner == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshAuthReader(Payload, XSSH_MSG_USERAUTH_BANNER, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Banner.Message = xsshAuthText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Banner.Language = xsshAuthText(Value);
	if ( !xrtUtf8Valid(Banner.Message, NULL) ||
		!xrtSshLanguageValid(Banner.Language) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pBanner = Banner;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/auth/ssh_auth_password.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_AUTH_PASSWORD)
#include <string.h>




#if defined(XSSH_FEATURE_AUTH_PASSWORD)

/* 转换文本视图为原始字节视图。 */
static xbytesview xsshPasswordBytes(xstrview Text)
{
	xbytesview Value;

	Value.Data = (const unsigned char*)Text.Data;
	Value.Size = Text.Size;
	return Value;
}



/* 转换原始字节视图为文本视图。 */
static xstrview xsshPasswordText(xbytesview Value)
{
	xstrview Text;

	Text.Data = (const char*)Value.Data;
	Text.Size = Value.Size;
	return Text;
}



/* 比较借用文本与固定协议文本。 */
static bool xsshPasswordTextEqual(xstrview Text, const char* sValue, size_t iSize)
{
	return (Text.Size == iSize) &&
		((iSize == 0u) || (memcmp(Text.Data, sValue, iSize) == 0));
}



/* 将一个 string 字段加入方法字段长度。 */
static xsshcode xsshPasswordAddString(xstrview Text, size_t* pSize)
{
	if ( (pSize == NULL) || !xrtUtf8Valid(Text, NULL) ||
		(Text.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pSize > (SIZE_MAX - 4u)) ||
		(Text.Size > (SIZE_MAX - *pSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pSize += 4u + Text.Size;
	return XSSH_OK;
}



/* 构建普通或更改密码请求，并在最后一次性发布 writer。 */
static xsshcode xsshPasswordWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Password,
	xstrview NewPassword,
	bool bChange
)
{
	xbytesview arrInputs[3];
	xsshwriter Writer;
	size_t iFieldsSize = 1u;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = xsshPasswordBytes(User);
	arrInputs[1] = xsshPasswordBytes(Password);
	arrInputs[2] = xsshPasswordBytes(NewPassword);
	Code = xsshPasswordAddString(Password, &iFieldsSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( bChange ) {
		Code = xsshPasswordAddString(NewPassword, &iFieldsSize);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_PASSWORD),
		iFieldsSize,
		&iTotal
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		3u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	Code = xrtSshAuthRequestWrite(
		&Writer,
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_PASSWORD),
		(xbytesview){ NULL, 0u }
	);
	if ( Code != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	if ( (xrtSshWriteBool(&Writer, bChange) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ||
		(bChange &&
		 (xrtSshWriteString(&Writer, arrInputs[2]) != XSSH_OK)) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 写入普通密码认证请求。 */
xsshcode xrtSshAuthPasswordWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Password
)
{
	return xsshPasswordWrite(
		pWriter,
		User,
		Password,
		XRT_STR_LITERAL(""),
		false
	);
}



/* 写入旧密码与新密码认证请求。 */
xsshcode xrtSshAuthPasswordChangeWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Password,
	xstrview NewPassword
)
{
	return xsshPasswordWrite(
		pWriter,
		User,
		Password,
		NewPassword,
		true
	);
}



/* 严格读取普通或更改密码请求。 */
xsshcode xrtSshAuthPasswordRead(
	xbytesview Payload,
	xsshauthpassword* pPassword
)
{
	xsshauthrequest Request;
	xsshauthpassword Password;
	xsshreader Reader;
	xbytesview Value;
	xsshcode Code;

	if ( pPassword == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthRequestRead(Payload, &Request);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshPasswordTextEqual(
		Request.Service,
		XSSH_SERVICE_CONNECTION,
		sizeof(XSSH_SERVICE_CONNECTION) - 1u
	) || !xsshPasswordTextEqual(
		Request.Method,
		XSSH_AUTH_METHOD_PASSWORD,
		sizeof(XSSH_AUTH_METHOD_PASSWORD) - 1u
	) || !xrtSshReaderInit(&Reader, Request.Fields) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Password.User = Request.User;
	Code = xrtSshReadBool(&Reader, &Password.Change);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Password.Password = xsshPasswordText(Value);
	Password.NewPassword = XRT_STR_LITERAL("");
	if ( Password.Change ) {
		Code = xrtSshReadString(&Reader, &Value);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		Password.NewPassword = xsshPasswordText(Value);
	}
	if ( !xrtUtf8Valid(Password.Password, NULL) ||
		!xrtUtf8Valid(Password.NewPassword, NULL) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPassword = Password;
	return XSSH_OK;
}



/* 写入服务端密码更改提示。 */
xsshcode xrtSshAuthPasswordPromptWrite(
	xsshwriter* pWriter,
	xstrview Prompt,
	xstrview Language
)
{
	xbytesview arrInputs[2];
	xsshwriter Writer;
	size_t iTotal = 1u;
	xsshcode Code;

	arrInputs[0] = xsshPasswordBytes(Prompt);
	arrInputs[1] = xsshPasswordBytes(Language);
	Code = xsshPasswordAddString(Prompt, &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshLanguageValid(Language) ||
		(Language.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (iTotal > (SIZE_MAX - 4u)) ||
		(Language.Size > (SIZE_MAX - iTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal += 4u + Language.Size;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		2u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_USERAUTH_PASSWD_CHANGEREQ
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[0]
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[1]
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取服务端密码更改提示。 */
xsshcode xrtSshAuthPasswordPromptRead(
	xbytesview Payload,
	xsshauthpasswordprompt* pPrompt
)
{
	xsshreader Reader;
	xsshauthpasswordprompt Prompt;
	xbytesview Value;
	uint8 iMessage;
	xsshcode Code;

	if ( (pPrompt == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_USERAUTH_PASSWD_CHANGEREQ ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Prompt.Prompt = xsshPasswordText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Prompt.Language = xsshPasswordText(Value);
	if ( !xrtUtf8Valid(Prompt.Prompt, NULL) ||
		!xrtSshLanguageValid(Prompt.Language) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPrompt = Prompt;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/auth/ssh_auth_publickey.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_AUTH_PUBLICKEY)
#include <string.h>




#if defined(XSSH_FEATURE_AUTH_PUBLICKEY)

/* 转换文本视图为原始字节视图。 */
static xbytesview xsshPublicKeyBytes(xstrview Text)
{
	xbytesview Value;

	Value.Data = (const unsigned char*)Text.Data;
	Value.Size = Text.Size;
	return Value;
}



/* 转换原始字节视图为文本视图。 */
static xstrview xsshPublicKeyText(xbytesview Value)
{
	xstrview Text;

	Text.Data = (const char*)Value.Data;
	Text.Size = Value.Size;
	return Text;
}



/* 比较两个借用算法名。 */
static bool xsshPublicKeyAlgorithmEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 比较借用文本与固定协议文本。 */
static bool xsshPublicKeyTextEqual(
	xstrview Text,
	const char* sValue,
	size_t iSize
)
{
	return (Text.Size == iSize) &&
		((iSize == 0u) || (memcmp(Text.Data, sValue, iSize) == 0));
}



/* 将一个 string 字段加入方法字段长度。 */
static xsshcode xsshPublicKeyAddString(xbytesview Value, size_t* pSize)
{
	if ( (pSize == NULL) ||
		!xrtMemRangeValid(Value.Data, Value.Size) ||
		(Value.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pSize > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pSize += 4u + Value.Size;
	return XSSH_OK;
}



/* 校验算法、公钥 blob 以及可选签名 blob 的关系。 */
static xsshcode xsshPublicKeyValidate(
	xstrview Algorithm,
	xbytesview PublicKey,
	xbytesview Signature,
	bool bSignatureField
)
{
	xsshpublickey Key;
	xsshsignature ParsedSignature;
	xsshcode Code;

	if ( !xrtSshNameValid(Algorithm) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPublicKeyRead(PublicKey, &Key);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? Code : XSSH_ERROR_PROTOCOL;
	}
	if ( !bSignatureField ) {
		return Signature.Size == 0u ? XSSH_OK : XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshSignatureRead(Signature, &ParsedSignature);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? Code : XSSH_ERROR_PROTOCOL;
	}
	return xsshPublicKeyAlgorithmEqual(
		Algorithm,
		ParsedSignature.Algorithm
	) ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 写入已经完整预留过的 publickey 请求字段。 */
static xsshcode xsshPublicKeyBodyWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xbytesview Signature,
	bool bHasSignature,
	bool bSignatureField
)
{
	xsshcode Code;

	Code = xrtSshAuthRequestWrite(
		pWriter,
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_PUBLICKEY),
		(xbytesview){ NULL, 0u }
	);
	if ( Code != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	if ( (xrtSshWriteBool(pWriter, bHasSignature) != XSSH_OK) ||
		(xrtSshWriteString(
			pWriter,
			xsshPublicKeyBytes(Algorithm)
		) != XSSH_OK) || (xrtSshWriteString(
			pWriter,
			PublicKey
		) != XSSH_OK) || (bSignatureField &&
		 (xrtSshWriteString(pWriter, Signature) != XSSH_OK)) ) {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 构建 probe、带签名请求或签名原文中的请求部分。 */
static xsshcode xsshPublicKeyRequestWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xbytesview Signature,
	bool bHasSignature,
	bool bSignatureField
)
{
	xbytesview arrInputs[4];
	xsshwriter Writer;
	size_t iFieldsSize = 1u;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = xsshPublicKeyBytes(User);
	arrInputs[1] = xsshPublicKeyBytes(Algorithm);
	arrInputs[2] = PublicKey;
	arrInputs[3] = Signature;
	Code = xsshPublicKeyValidate(
		Algorithm,
		PublicKey,
		Signature,
		bSignatureField
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xsshPublicKeyAddString(
		arrInputs[1],
		&iFieldsSize
	)) != XSSH_OK) || ((Code = xsshPublicKeyAddString(
		PublicKey,
		&iFieldsSize
	)) != XSSH_OK) ) {
		return Code;
	}
	if ( bSignatureField ) {
		Code = xsshPublicKeyAddString(Signature, &iFieldsSize);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_PUBLICKEY),
		iFieldsSize,
		&iTotal
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		4u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	Code = xsshPublicKeyBodyWrite(
		&Writer,
		User,
		Algorithm,
		PublicKey,
		Signature,
		bHasSignature,
		bSignatureField
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 写入无签名 publickey 探测。 */
xsshcode xrtSshAuthPublicKeyWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey
)
{
	return xsshPublicKeyRequestWrite(
		pWriter,
		User,
		Algorithm,
		PublicKey,
		(xbytesview){ NULL, 0u },
		false,
		false
	);
}



/* 写入带签名 publickey 请求。 */
xsshcode xrtSshAuthPublicKeySignedWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xbytesview Signature
)
{
	return xsshPublicKeyRequestWrite(
		pWriter,
		User,
		Algorithm,
		PublicKey,
		Signature,
		true,
		true
	);
}



/* 严格读取 publickey 探测或带签名请求。 */
xsshcode xrtSshAuthPublicKeyRead(
	xbytesview Payload,
	xsshauthpublickey* pPublicKey
)
{
	xsshauthrequest Request;
	xsshauthpublickey PublicKey;
	xsshreader Reader;
	xbytesview Value;
	xsshcode Code;

	if ( pPublicKey == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthRequestRead(Payload, &Request);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshPublicKeyTextEqual(
		Request.Service,
		XSSH_SERVICE_CONNECTION,
		sizeof(XSSH_SERVICE_CONNECTION) - 1u
	) || !xsshPublicKeyTextEqual(
		Request.Method,
		XSSH_AUTH_METHOD_PUBLICKEY,
		sizeof(XSSH_AUTH_METHOD_PUBLICKEY) - 1u
	) || !xrtSshReaderInit(&Reader, Request.Fields) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	PublicKey.User = Request.User;
	Code = xrtSshReadBool(&Reader, &PublicKey.HasSignature);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	PublicKey.Algorithm = xsshPublicKeyText(Value);
	Code = xrtSshReadString(&Reader, &PublicKey.PublicKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	PublicKey.Signature = (xbytesview){ NULL, 0u };
	if ( PublicKey.HasSignature ) {
		Code = xrtSshReadString(&Reader, &PublicKey.Signature);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xsshPublicKeyValidate(
		PublicKey.Algorithm,
		PublicKey.PublicKey,
		PublicKey.Signature,
		PublicKey.HasSignature
	);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? XSSH_ERROR_PROTOCOL : Code;
	}
	*pPublicKey = PublicKey;
	return XSSH_OK;
}



/* 写入 session identifier 与不含签名字段的已签名请求。 */
xsshcode xrtSshAuthPublicKeySignDataWrite(
	xsshwriter* pWriter,
	xbytesview SessionId,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey
)
{
	xbytesview arrInputs[4];
	xsshwriter Writer;
	size_t iFieldsSize = 1u;
	size_t iRequestSize;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = SessionId;
	arrInputs[1] = xsshPublicKeyBytes(User);
	arrInputs[2] = xsshPublicKeyBytes(Algorithm);
	arrInputs[3] = PublicKey;
	if ( SessionId.Size == 0u ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPublicKeyValidate(
		Algorithm,
		PublicKey,
		(xbytesview){ NULL, 0u },
		false
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xsshPublicKeyAddString(
		arrInputs[2],
		&iFieldsSize
	)) != XSSH_OK) || ((Code = xsshPublicKeyAddString(
		PublicKey,
		&iFieldsSize
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_PUBLICKEY),
		iFieldsSize,
		&iRequestSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (SessionId.Size > UINT32_MAX) ||
		(iRequestSize > (SIZE_MAX - 4u)) ||
		(SessionId.Size > (SIZE_MAX - iRequestSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal = 4u + SessionId.Size + iRequestSize;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		4u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( xrtSshWriteString(&Writer, SessionId) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshPublicKeyBodyWrite(
		&Writer,
		User,
		Algorithm,
		PublicKey,
		(xbytesview){ NULL, 0u },
		true,
		false
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 写入服务端 publickey 探测成功响应。 */
xsshcode xrtSshAuthPublicKeyOkWrite(
	xsshwriter* pWriter,
	xstrview Algorithm,
	xbytesview PublicKey
)
{
	xbytesview arrInputs[2];
	xsshwriter Writer;
	size_t iTotal = 1u;
	xsshcode Code;

	arrInputs[0] = xsshPublicKeyBytes(Algorithm);
	arrInputs[1] = PublicKey;
	Code = xsshPublicKeyValidate(
		Algorithm,
		PublicKey,
		(xbytesview){ NULL, 0u },
		false
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xsshPublicKeyAddString(
		arrInputs[0],
		&iTotal
	)) != XSSH_OK) || ((Code = xsshPublicKeyAddString(
		PublicKey,
		&iTotal
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		2u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_USERAUTH_PK_OK) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, PublicKey) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取服务端 publickey 探测成功响应。 */
xsshcode xrtSshAuthPublicKeyOkRead(
	xbytesview Payload,
	xsshauthpublickeyok* pPublicKey
)
{
	xsshreader Reader;
	xsshauthpublickeyok PublicKey;
	xbytesview Value;
	uint8 iMessage;
	xsshcode Code;

	if ( (pPublicKey == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_USERAUTH_PK_OK ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	PublicKey.Algorithm = xsshPublicKeyText(Value);
	Code = xrtSshReadString(&Reader, &PublicKey.PublicKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xsshPublicKeyValidate(
		PublicKey.Algorithm,
		PublicKey.PublicKey,
		(xbytesview){ NULL, 0u },
		false
	);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? XSSH_ERROR_PROTOCOL : Code;
	}
	*pPublicKey = PublicKey;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/auth/ssh_auth_keyboard.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_AUTH_KEYBOARD)
#include <string.h>




#if defined(XSSH_FEATURE_AUTH_KEYBOARD)

/* 比较借用文本与固定协议文本。 */
static bool xsshKeyboardTextEqual(
	xstrview Text,
	const char* sValue,
	size_t iSize
)
{
	return (Text.Size == iSize) &&
		((iSize == 0u) || (memcmp(Text.Data, sValue, iSize) == 0));
}



/* 将一个 SSH string 的编码长度加入总长度。 */
static xsshcode xsshKeyboardAddString(xbytesview Value, size_t* pTotal)
{
	if ( (pTotal == NULL) ||
		!xrtMemRangeValid(Value.Data, Value.Size) ||
		(Value.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pTotal > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotal += 4u + Value.Size;
	return XSSH_OK;
}



/* 校验 UTF-8 文本并加入 SSH string 编码长度。 */
static xsshcode xsshKeyboardAddText(
	xstrview Text,
	bool bNonempty,
	size_t* pTotal
)
{
	if ( (bNonempty && (Text.Size == 0u)) ||
		!xrtUtf8Valid(Text, NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xsshKeyboardAddString(
		(xbytesview){ (const unsigned char*)Text.Data, Text.Size },
		pTotal
	);
}



/* 校验空值或逗号分隔的 keyboard-interactive 子方法列表。 */
static bool xsshKeyboardSubmethodsValid(xstrview Submethods)
{
	return ((Submethods.Size == 0u) &&
		xrtMemRangeValid(Submethods.Data, Submethods.Size)) ||
		xrtSshNameListValid(Submethods);
}



/* 写入带显式 language tag 的 keyboard-interactive 请求。 */
xsshcode xrtSshAuthKeyboardWriteLanguage(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Language,
	xstrview Submethods
)
{
	xbytesview arrInputs[3];
	xsshwriter Writer;
	size_t iFieldsSize = 0u;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = (xbytesview){
		(const unsigned char*)User.Data,
		User.Size
	};
	arrInputs[1] = (xbytesview){
		(const unsigned char*)Language.Data,
		Language.Size
	};
	arrInputs[2] = (xbytesview){
		(const unsigned char*)Submethods.Data,
		Submethods.Size
	};
	if ( !xrtSshLanguageValid(Language) ||
		!xsshKeyboardSubmethodsValid(Submethods) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshKeyboardAddString(
		arrInputs[1],
		&iFieldsSize
	)) != XSSH_OK) || ((Code = xsshKeyboardAddString(
		arrInputs[2],
		&iFieldsSize
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE),
		iFieldsSize,
		&iTotal
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshWriterReserveInputs(pWriter, iTotal, arrInputs, 3u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	Code = xrtSshAuthRequestWrite(
		&Writer,
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE),
		(xbytesview){ NULL, 0u }
	);
	if ( Code != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	if ( (xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[2]) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 使用规范建议的空 language tag 写入请求。 */
xsshcode xrtSshAuthKeyboardWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Submethods
)
{
	return xrtSshAuthKeyboardWriteLanguage(
		pWriter,
		User,
		XRT_STR_LITERAL(""),
		Submethods
	);
}



/* 严格读取 keyboard-interactive 请求。 */
xsshcode xrtSshAuthKeyboardRead(
	xbytesview Payload,
	xsshauthkeyboard* pKeyboard
)
{
	xsshauthrequest Request;
	xsshauthkeyboard Keyboard;
	xsshreader Reader;
	xbytesview Value;
	xsshcode Code;

	if ( pKeyboard == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthRequestRead(Payload, &Request);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshKeyboardTextEqual(
		Request.Service,
		XSSH_SERVICE_CONNECTION,
		sizeof(XSSH_SERVICE_CONNECTION) - 1u
	) || !xsshKeyboardTextEqual(
		Request.Method,
		XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE,
		sizeof(XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE) - 1u
	) || !xrtSshReaderInit(&Reader, Request.Fields) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Keyboard.User = Request.User;
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Keyboard.Language = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Keyboard.Submethods = (xstrview){ (const char*)Value.Data, Value.Size };
	if ( !xrtSshLanguageValid(Keyboard.Language) ||
		!xsshKeyboardSubmethodsValid(Keyboard.Submethods) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pKeyboard = Keyboard;
	return XSSH_OK;
}



/* 写入任意数量的 keyboard-interactive 提示。 */
xsshcode xrtSshAuthKeyboardChallengeWrite(
	xsshwriter* pWriter,
	xstrview Name,
	xstrview Instruction,
	xstrview Language,
	const xsshauthkeyboardprompt* pPrompts,
	size_t iCount
)
{
	xbytesview arrInputs[4];
	xbytesview Input;
	xsshwriter Writer;
	size_t iPromptsSize;
	size_t iTotal = 1u + 4u;
	size_t i;
	xsshcode Code;

	if ( (iCount > UINT32_MAX) ||
		(iCount > (SIZE_MAX / sizeof(*pPrompts))) ||
		((pPrompts == NULL) && (iCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iPromptsSize = iCount * sizeof(*pPrompts);
	if ( !xrtMemRangeValid(pPrompts, iPromptsSize) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	arrInputs[0] = (xbytesview){
		(const unsigned char*)Name.Data,
		Name.Size
	};
	arrInputs[1] = (xbytesview){
		(const unsigned char*)Instruction.Data,
		Instruction.Size
	};
	arrInputs[2] = (xbytesview){
		(const unsigned char*)Language.Data,
		Language.Size
	};
	arrInputs[3] = (xbytesview){
		(const unsigned char*)pPrompts,
		iPromptsSize
	};
	if ( ((Code = xsshKeyboardAddText(Name, false, &iTotal)) != XSSH_OK) ||
		((Code = xsshKeyboardAddText(
			Instruction,
			false,
			&iTotal
		)) != XSSH_OK) ) {
		return Code;
	}
	if ( !xrtSshLanguageValid(Language) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKeyboardAddString(arrInputs[2], &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	for ( i = 0u; i < iCount; ++i ) {
		Code = xsshKeyboardAddText(pPrompts[i].Prompt, true, &iTotal);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( iTotal == SIZE_MAX ) {
			return XSSH_ERROR_OVERFLOW;
		}
		++iTotal;
	}
	Code = xrtSshWriterReserveInputs(pWriter, iTotal, arrInputs, 4u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	for ( i = 0u; i < iCount; ++i ) {
		Input = (xbytesview){
			(const unsigned char*)pPrompts[i].Prompt.Data,
			pPrompts[i].Prompt.Size
		};
		Code = xrtSshWriterReserveInputs(pWriter, iTotal, &Input, 1u);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_USERAUTH_INFO_REQUEST
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[0]
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[1]
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[2]
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		(uint32)iCount
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	for ( i = 0u; i < iCount; ++i ) {
		Input = (xbytesview){
			(const unsigned char*)pPrompts[i].Prompt.Data,
			pPrompts[i].Prompt.Size
		};
		if ( (xrtSshWriteString(&Writer, Input) != XSSH_OK) ||
			(xrtSshWriteBool(&Writer, pPrompts[i].Echo) != XSSH_OK) ) {
			return XSSH_ERROR_STATE;
		}
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 预验证完整 challenge 后初始化无分配提示迭代器。 */
xsshcode xrtSshAuthKeyboardChallengeRead(
	xbytesview Payload,
	xsshauthkeyboardchallenge* pChallenge
)
{
	xsshauthkeyboardchallenge Challenge;
	xsshreader Reader;
	xsshreader Prompts;
	xbytesview Value;
	bool bEcho;
	uint8 iMessage;
	uint32 i;
	xsshcode Code;

	if ( (pChallenge == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_USERAUTH_INFO_REQUEST ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Challenge.Name = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Challenge.Instruction = (xstrview){
		(const char*)Value.Data,
		Value.Size
	};
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Challenge.Language = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadU32(&Reader, &Challenge.Count);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtUtf8Valid(Challenge.Name, NULL) ||
		!xrtUtf8Valid(Challenge.Instruction, NULL) ||
		!xrtSshLanguageValid(Challenge.Language) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( (size_t)Challenge.Count >
		(xrtSshReaderRemaining(&Reader) / 5u) ) {
		return XSSH_NEED_MORE;
	}
	Prompts = Reader;
	for ( i = 0u; i < Challenge.Count; ++i ) {
		Code = xrtSshReadString(&Reader, &Value);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( (Value.Size == 0u) || !xrtUtf8Valid(
			(xstrview){ (const char*)Value.Data, Value.Size },
			NULL
		) ) {
			return XSSH_ERROR_PROTOCOL;
		}
		Code = xrtSshReadBool(&Reader, &bEcho);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Challenge.Index = 0u;
	Challenge.Reader = Prompts;
	*pChallenge = Challenge;
	return XSSH_OK;
}



/* 从已验证 challenge 中返回下一项提示。 */
bool xrtSshAuthKeyboardChallengeNext(
	xsshauthkeyboardchallenge* pChallenge,
	xsshauthkeyboardprompt* pPrompt
)
{
	xsshauthkeyboardchallenge Challenge;
	xsshauthkeyboardprompt Prompt;
	xbytesview Value;

	if ( (pChallenge == NULL) || (pPrompt == NULL) ||
		(pChallenge->Index >= pChallenge->Count) ) {
		return false;
	}
	Challenge = *pChallenge;
	if ( (xrtSshReadString(&Challenge.Reader, &Value) != XSSH_OK) ||
		(xrtSshReadBool(&Challenge.Reader, &Prompt.Echo) != XSSH_OK) ||
		(Value.Size == 0u) ) {
		return false;
	}
	Prompt.Prompt = (xstrview){ (const char*)Value.Data, Value.Size };
	if ( !xrtUtf8Valid(Prompt.Prompt, NULL) ) {
		return false;
	}
	++Challenge.Index;
	*pChallenge = Challenge;
	*pPrompt = Prompt;
	return true;
}



/* 写入任意数量的 keyboard-interactive 响应。 */
xsshcode xrtSshAuthKeyboardResponseWrite(
	xsshwriter* pWriter,
	const xstrview* pResponses,
	size_t iCount
)
{
	xbytesview Inputs;
	xbytesview Input;
	xsshwriter Writer;
	size_t iResponsesSize;
	size_t iTotal = 1u + 4u;
	size_t i;
	xsshcode Code;

	if ( (iCount > UINT32_MAX) ||
		(iCount > (SIZE_MAX / sizeof(*pResponses))) ||
		((pResponses == NULL) && (iCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iResponsesSize = iCount * sizeof(*pResponses);
	if ( !xrtMemRangeValid(pResponses, iResponsesSize) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Inputs = (xbytesview){
		(const unsigned char*)pResponses,
		iResponsesSize
	};
	for ( i = 0u; i < iCount; ++i ) {
		Code = xsshKeyboardAddText(pResponses[i], false, &iTotal);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Code = xrtSshWriterReserveInputs(pWriter, iTotal, &Inputs, 1u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	for ( i = 0u; i < iCount; ++i ) {
		Input = (xbytesview){
			(const unsigned char*)pResponses[i].Data,
			pResponses[i].Size
		};
		Code = xrtSshWriterReserveInputs(pWriter, iTotal, &Input, 1u);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_USERAUTH_INFO_RESPONSE
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		(uint32)iCount
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	for ( i = 0u; i < iCount; ++i ) {
		Input = (xbytesview){
			(const unsigned char*)pResponses[i].Data,
			pResponses[i].Size
		};
		if ( xrtSshWriteString(&Writer, Input) != XSSH_OK ) {
			return XSSH_ERROR_STATE;
		}
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 预验证完整 response 后初始化无分配响应迭代器。 */
xsshcode xrtSshAuthKeyboardResponseRead(
	xbytesview Payload,
	xsshauthkeyboardresponses* pResponses
)
{
	xsshauthkeyboardresponses Responses;
	xsshreader Reader;
	xsshreader Items;
	xbytesview Value;
	uint8 iMessage;
	uint32 i;
	xsshcode Code;

	if ( (pResponses == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_USERAUTH_INFO_RESPONSE ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadU32(&Reader, &Responses.Count);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (size_t)Responses.Count >
		(xrtSshReaderRemaining(&Reader) / 4u) ) {
		return XSSH_NEED_MORE;
	}
	Items = Reader;
	for ( i = 0u; i < Responses.Count; ++i ) {
		Code = xrtSshReadString(&Reader, &Value);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( !xrtUtf8Valid(
			(xstrview){ (const char*)Value.Data, Value.Size },
			NULL
		) ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Responses.Index = 0u;
	Responses.Reader = Items;
	*pResponses = Responses;
	return XSSH_OK;
}



/* 从已验证 response 中返回下一项响应。 */
bool xrtSshAuthKeyboardResponseNext(
	xsshauthkeyboardresponses* pResponses,
	xstrview* pResponse
)
{
	xsshauthkeyboardresponses Responses;
	xstrview Response;
	xbytesview Value;

	if ( (pResponses == NULL) || (pResponse == NULL) ||
		(pResponses->Index >= pResponses->Count) ) {
		return false;
	}
	Responses = *pResponses;
	if ( xrtSshReadString(&Responses.Reader, &Value) != XSSH_OK ) {
		return false;
	}
	Response = (xstrview){ (const char*)Value.Data, Value.Size };
	if ( !xrtUtf8Valid(Response, NULL) ) {
		return false;
	}
	++Responses.Index;
	*pResponses = Responses;
	*pResponse = Response;
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/auth/ssh_auth_hostbased.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_AUTH_HOSTBASED)
#include <string.h>




#if defined(XSSH_FEATURE_AUTH_HOSTBASED)

/* 比较两个借用算法名。 */
static bool xsshHostBasedAlgorithmEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 比较借用文本与固定协议文本。 */
static bool xsshHostBasedTextEqual(
	xstrview Text,
	const char* sValue,
	size_t iSize
)
{
	return (Text.Size == iSize) &&
		((iSize == 0u) || (memcmp(Text.Data, sValue, iSize) == 0));
}



/* 将一个 SSH string 的编码长度加入总长度。 */
static xsshcode xsshHostBasedAddString(xbytesview Value, size_t* pTotal)
{
	if ( (pTotal == NULL) ||
		!xrtMemRangeValid(Value.Data, Value.Size) ||
		(Value.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pTotal > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotal += 4u + Value.Size;
	return XSSH_OK;
}



/* 校验 US-ASCII DNS 主机名和标签边界。 */
bool xrtSshAuthHostNameValid(xstrview HostName)
{
	size_t iLabel = 0u;
	size_t i;
	bool bRooted;

	if ( !xrtMemRangeValid(HostName.Data, HostName.Size) ||
		(HostName.Size == 0u) ) {
		return false;
	}
	bRooted = HostName.Data[HostName.Size - 1u] == '.';
	if ( HostName.Size > (bRooted ? XSSH_AUTH_HOST_NAME_MAX :
		(XSSH_AUTH_HOST_NAME_MAX - 1u)) ) {
		return false;
	}
	for ( i = 0u; i < HostName.Size; ++i ) {
		unsigned char iByte = (unsigned char)HostName.Data[i];

		if ( iByte == '.' ) {
			if ( (iLabel == 0u) || (iLabel > 63u) ||
				(HostName.Data[i - 1u] == '-') ) {
				return false;
			}
			iLabel = 0u;
			continue;
		}
		if ( !(((iByte >= 'a') && (iByte <= 'z')) ||
			((iByte >= 'A') && (iByte <= 'Z')) ||
			((iByte >= '0') && (iByte <= '9')) ||
			((iByte == '-') && (iLabel != 0u))) ) {
			return false;
		}
		++iLabel;
	}
	return bRooted || ((iLabel != 0u) && (iLabel <= 63u) &&
		(HostName.Data[HostName.Size - 1u] != '-'));
}



/* 校验算法、公钥、主机身份以及可选签名的关系。 */
static xsshcode xsshHostBasedValidate(
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser,
	xbytesview Signature,
	bool bSignature
)
{
	xsshpublickey Key;
	xsshsignature ParsedSignature;
	xsshcode Code;

	if ( !xrtSshNameValid(Algorithm) ||
		!xrtSshAuthHostNameValid(HostName) ||
		!xrtUtf8Valid(ClientUser, NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPublicKeyRead(PublicKey, &Key);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? Code : XSSH_ERROR_PROTOCOL;
	}
	if ( !bSignature ) {
		return Signature.Size == 0u ? XSSH_OK : XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshSignatureRead(Signature, &ParsedSignature);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? Code : XSSH_ERROR_PROTOCOL;
	}
	return xsshHostBasedAlgorithmEqual(
		Algorithm,
		ParsedSignature.Algorithm
	) ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 计算 hostbased 方法字段长度并完成全部字段校验。 */
static xsshcode xsshHostBasedFieldsSize(
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser,
	xbytesview Signature,
	bool bSignature,
	size_t* pSize
)
{
	xbytesview arrValues[5];
	size_t iTotal = 0u;
	size_t iCount = bSignature ? 5u : 4u;
	size_t i;
	xsshcode Code;

	if ( pSize == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshHostBasedValidate(
		Algorithm,
		PublicKey,
		HostName,
		ClientUser,
		Signature,
		bSignature
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	arrValues[0] = (xbytesview){
		(const unsigned char*)Algorithm.Data,
		Algorithm.Size
	};
	arrValues[1] = PublicKey;
	arrValues[2] = (xbytesview){
		(const unsigned char*)HostName.Data,
		HostName.Size
	};
	arrValues[3] = (xbytesview){
		(const unsigned char*)ClientUser.Data,
		ClientUser.Size
	};
	arrValues[4] = Signature;
	for ( i = 0u; i < iCount; ++i ) {
		Code = xsshHostBasedAddString(arrValues[i], &iTotal);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	*pSize = iTotal;
	return XSSH_OK;
}



/* 写入已经完整预留过的 hostbased 请求字段。 */
static xsshcode xsshHostBasedBodyWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser,
	xbytesview Signature,
	bool bSignature
)
{
	xsshcode Code;

	Code = xrtSshAuthRequestWrite(
		pWriter,
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_HOSTBASED),
		(xbytesview){ NULL, 0u }
	);
	if ( Code != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	if ( (xrtSshWriteString(
		pWriter,
		(xbytesview){
			(const unsigned char*)Algorithm.Data,
			Algorithm.Size
		}
	) != XSSH_OK) || (xrtSshWriteString(
		pWriter,
		PublicKey
	) != XSSH_OK) || (xrtSshWriteString(
		pWriter,
		(xbytesview){
			(const unsigned char*)HostName.Data,
			HostName.Size
		}
	) != XSSH_OK) || (xrtSshWriteString(
		pWriter,
		(xbytesview){
			(const unsigned char*)ClientUser.Data,
			ClientUser.Size
		}
	) != XSSH_OK) || (bSignature &&
		(xrtSshWriteString(pWriter, Signature) != XSSH_OK)) ) {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 写入完整 hostbased 认证请求。 */
xsshcode xrtSshAuthHostBasedWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser,
	xbytesview Signature
)
{
	xbytesview arrInputs[6];
	xsshwriter Writer;
	size_t iFieldsSize;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = (xbytesview){
		(const unsigned char*)User.Data,
		User.Size
	};
	arrInputs[1] = (xbytesview){
		(const unsigned char*)Algorithm.Data,
		Algorithm.Size
	};
	arrInputs[2] = PublicKey;
	arrInputs[3] = (xbytesview){
		(const unsigned char*)HostName.Data,
		HostName.Size
	};
	arrInputs[4] = (xbytesview){
		(const unsigned char*)ClientUser.Data,
		ClientUser.Size
	};
	arrInputs[5] = Signature;
	Code = xsshHostBasedFieldsSize(
		Algorithm,
		PublicKey,
		HostName,
		ClientUser,
		Signature,
		true,
		&iFieldsSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_HOSTBASED),
		iFieldsSize,
		&iTotal
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshWriterReserveInputs(pWriter, iTotal, arrInputs, 6u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	Code = xsshHostBasedBodyWrite(
		&Writer,
		User,
		Algorithm,
		PublicKey,
		HostName,
		ClientUser,
		Signature,
		true
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取完整 hostbased 请求。 */
xsshcode xrtSshAuthHostBasedRead(
	xbytesview Payload,
	xsshauthhostbased* pHostBased
)
{
	xsshauthrequest Request;
	xsshauthhostbased HostBased;
	xsshreader Reader;
	xbytesview Value;
	xsshcode Code;

	if ( pHostBased == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthRequestRead(Payload, &Request);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshHostBasedTextEqual(
		Request.Service,
		XSSH_SERVICE_CONNECTION,
		sizeof(XSSH_SERVICE_CONNECTION) - 1u
	) || !xsshHostBasedTextEqual(
		Request.Method,
		XSSH_AUTH_METHOD_HOSTBASED,
		sizeof(XSSH_AUTH_METHOD_HOSTBASED) - 1u
	) || !xrtSshReaderInit(&Reader, Request.Fields) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	HostBased.User = Request.User;
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	HostBased.Algorithm = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadString(&Reader, &HostBased.PublicKey);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	HostBased.HostName = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	HostBased.ClientUser = (xstrview){ (const char*)Value.Data, Value.Size };
	Code = xrtSshReadString(&Reader, &HostBased.Signature);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xsshHostBasedValidate(
		HostBased.Algorithm,
		HostBased.PublicKey,
		HostBased.HostName,
		HostBased.ClientUser,
		HostBased.Signature,
		true
	);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? XSSH_ERROR_PROTOCOL : Code;
	}
	*pHostBased = HostBased;
	return XSSH_OK;
}



/* 写入 session identifier 与不含签名字段的 hostbased 请求。 */
xsshcode xrtSshAuthHostBasedSignDataWrite(
	xsshwriter* pWriter,
	xbytesview SessionId,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser
)
{
	xbytesview arrInputs[6];
	xsshwriter Writer;
	size_t iFieldsSize;
	size_t iRequestSize;
	size_t iTotal;
	xsshcode Code;

	arrInputs[0] = SessionId;
	arrInputs[1] = (xbytesview){
		(const unsigned char*)User.Data,
		User.Size
	};
	arrInputs[2] = (xbytesview){
		(const unsigned char*)Algorithm.Data,
		Algorithm.Size
	};
	arrInputs[3] = PublicKey;
	arrInputs[4] = (xbytesview){
		(const unsigned char*)HostName.Data,
		HostName.Size
	};
	arrInputs[5] = (xbytesview){
		(const unsigned char*)ClientUser.Data,
		ClientUser.Size
	};
	if ( SessionId.Size == 0u ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshHostBasedFieldsSize(
		Algorithm,
		PublicKey,
		HostName,
		ClientUser,
		(xbytesview){ NULL, 0u },
		false,
		&iFieldsSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshAuthRequestSize(
		User,
		XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
		XRT_STR_LITERAL(XSSH_AUTH_METHOD_HOSTBASED),
		iFieldsSize,
		&iRequestSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (SessionId.Size > UINT32_MAX) ||
		(iRequestSize > (SIZE_MAX - 4u)) ||
		(SessionId.Size > (SIZE_MAX - iRequestSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal = 4u + SessionId.Size + iRequestSize;
	Code = xrtSshWriterReserveInputs(pWriter, iTotal, arrInputs, 6u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( xrtSshWriteString(&Writer, SessionId) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshHostBasedBodyWrite(
		&Writer,
		User,
		Algorithm,
		PublicKey,
		HostName,
		ClientUser,
		(xbytesview){ NULL, 0u },
		false
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/auth/ssh_auth_guard.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_AUTH_GUARD)
#include <math.h>
#include <string.h>




#if defined(XSSH_FEATURE_AUTH_GUARD)

/* 校验公开状态没有被未初始化或越界枚举伪造。 */
static bool xsshAuthGuardValid(const xsshauthguard* pGuard)
{
	return (pGuard != NULL) && pGuard->Initialized &&
		(pGuard->Exhaustion >= XSSH_AUTH_EXHAUST_NONE) &&
		(pGuard->Exhaustion <= XSSH_AUTH_EXHAUST_BYTES);
}



/* 无符号计数使用饱和加法，永不回绕。 */
static uint64 xsshAuthGuardAdd64(uint64 iLeft, uint64 iRight)
{
	return iRight > (UINT64_MAX - iLeft) ?
		UINT64_MAX : iLeft + iRight;
}



/* 32 位计数增加一项时饱和。 */
static uint32 xsshAuthGuardIncrement(uint32 iValue)
{
	return iValue == UINT32_MAX ? UINT32_MAX : iValue + 1u;
}



/* 判断启用的 64 位上限是否已经被超过。 */
static bool xsshAuthGuardLimit64(uint64 iValue, uint64 iLimit)
{
	return (iLimit != 0u) && (iValue > iLimit);
}



/* 判断启用的 32 位上限是否已经被超过。 */
static bool xsshAuthGuardLimit32(uint32 iValue, uint32 iLimit)
{
	return (iLimit != 0u) && (iValue > iLimit);
}



/* 按稳定优先级返回当前首个资源耗尽原因。 */
static xsshauthexhaustion xsshAuthGuardCurrent(
	const xsshauthguard* pGuard,
	double Timer
)
{
	double iElapsed = Timer >= pGuard->StartedTimer ?
		Timer - pGuard->StartedTimer : 0u;

	if ( pGuard->Exhaustion != XSSH_AUTH_EXHAUST_NONE ) {
		return pGuard->Exhaustion;
	}
	if ( (pGuard->Policy.TimeoutMs != 0u) &&
		(iElapsed >= (double)pGuard->Policy.TimeoutMs / 1000.0) ) {
		return XSSH_AUTH_EXHAUST_TIMEOUT;
	}
	if ( xsshAuthGuardLimit32(
		pGuard->Attempts,
		pGuard->Policy.AttemptLimit
	) ) {
		return XSSH_AUTH_EXHAUST_ATTEMPTS;
	}
	if ( xsshAuthGuardLimit32(
		pGuard->Rounds,
		pGuard->Policy.RoundLimit
	) ) {
		return XSSH_AUTH_EXHAUST_ROUNDS;
	}
	if ( xsshAuthGuardLimit32(
		pGuard->Messages,
		pGuard->Policy.MessageLimit
	) ) {
		return XSSH_AUTH_EXHAUST_MESSAGES;
	}
	if ( xsshAuthGuardLimit64(
		pGuard->Bytes,
		pGuard->Policy.ByteLimit
	) ) {
		return XSSH_AUTH_EXHAUST_BYTES;
	}
	return XSSH_AUTH_EXHAUST_NONE;
}



/* 初始化 RFC 推荐超时、尝试数和保守资源预算。 */
void xrtSshAuthGuardPolicyInit(xsshauthguardpolicy* pPolicy)
{
	if ( pPolicy == NULL ) {
		return;
	}
	pPolicy->TimeoutMs = XSSH_AUTH_DEFAULT_TIMEOUT_MS;
	pPolicy->ByteLimit = XSSH_AUTH_DEFAULT_BYTE_LIMIT;
	pPolicy->AttemptLimit = XSSH_AUTH_DEFAULT_ATTEMPT_LIMIT;
	pPolicy->RoundLimit = XSSH_AUTH_DEFAULT_ROUND_LIMIT;
	pPolicy->MessageLimit = XSSH_AUTH_DEFAULT_MESSAGE_LIMIT;
}



/* 开始一个不拥有时钟的认证预算会话。 */
bool xrtSshAuthGuardInit(
	xsshauthguard* pGuard,
	const xsshauthguardpolicy* pPolicy,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return false; }
	xsshauthguardpolicy Policy;
	xsshauthguard Guard;

	if ( pGuard == NULL || (pPolicy != NULL && pPolicy->TimeoutMs < 0) ) {
		return false;
	}
	if ( pPolicy == NULL ) {
		xrtSshAuthGuardPolicyInit(&Policy);
		pPolicy = &Policy;
	}
	if ( xrtMemRangesOverlap(
		pGuard,
		sizeof(*pGuard),
		pPolicy,
		sizeof(*pPolicy)
	) ) {
		return false;
	}
	memset(&Guard, 0, sizeof(Guard));
	Guard.Policy = *pPolicy;
	Guard.StartedTimer = Timer;
	Guard.Initialized = true;
	*pGuard = Guard;
	return true;
}



/* 查询当前认证预算，不增加任何计数。 */
xsshcode xrtSshAuthGuardCheck(
	xsshauthguard* pGuard,
	double Timer,
	xsshauthguarddecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshauthexhaustion Exhaustion;

	if ( !xsshAuthGuardValid(pGuard) || (pDecision == NULL) ||
		xrtMemRangesOverlap(
			pGuard,
			sizeof(*pGuard),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pGuard->Complete ) {
		*pDecision = XSSH_AUTH_GUARD_IGNORE;
		return XSSH_OK;
	}
	Exhaustion = xsshAuthGuardCurrent(pGuard, Timer);
	if ( Exhaustion != XSSH_AUTH_EXHAUST_NONE ) {
		pGuard->Exhaustion = Exhaustion;
		*pDecision = XSSH_AUTH_GUARD_DISCONNECT;
		return XSSH_OK;
	}
	*pDecision = XSSH_AUTH_GUARD_ALLOW;
	return XSSH_OK;
}



/* 原子计入一条消息并在超限时冻结断开原因。 */
xsshcode xrtSshAuthGuardReserve(
	xsshauthguard* pGuard,
	xsshauthevent Event,
	uint64 iMessageBytes,
	double Timer,
	xsshauthguarddecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshauthguard Guard;
	xsshauthguarddecision Decision;
	xsshcode Code;

	if ( !xsshAuthGuardValid(pGuard) || (pDecision == NULL) ||
		(Event < XSSH_AUTH_EVENT_MESSAGE) ||
		(Event > XSSH_AUTH_EVENT_ROUND) ||
		xrtMemRangesOverlap(
			pGuard,
			sizeof(*pGuard),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthGuardCheck(pGuard, Timer, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Decision != XSSH_AUTH_GUARD_ALLOW ) {
		*pDecision = Decision;
		return XSSH_OK;
	}
	Guard = *pGuard;
	Guard.Messages = xsshAuthGuardIncrement(Guard.Messages);
	Guard.Bytes = xsshAuthGuardAdd64(Guard.Bytes, iMessageBytes);
	if ( Event == XSSH_AUTH_EVENT_ATTEMPT ) {
		Guard.Attempts = xsshAuthGuardIncrement(Guard.Attempts);
	} else if ( Event == XSSH_AUTH_EVENT_ROUND ) {
		Guard.Rounds = xsshAuthGuardIncrement(Guard.Rounds);
	}
	Guard.Exhaustion = xsshAuthGuardCurrent(&Guard, Timer);
	*pGuard = Guard;
	*pDecision = Guard.Exhaustion == XSSH_AUTH_EXHAUST_NONE ?
		XSSH_AUTH_GUARD_ALLOW : XSSH_AUTH_GUARD_DISCONNECT;
	return XSSH_OK;
}



/* 成功状态只能从仍可用的认证预算进入。 */
bool xrtSshAuthGuardComplete(xsshauthguard* pGuard)
{
	if ( !xsshAuthGuardValid(pGuard) ||
		(pGuard->Exhaustion != XSSH_AUTH_EXHAUST_NONE) ) {
		return false;
	}
	pGuard->Complete = true;
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/auth/ssh_auth_session.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_AUTH_SESSION)
#include <math.h>
#include <string.h>




#if defined(XSSH_FEATURE_AUTH_SESSION)

#define XSSH_AUTH_SESSION_GUARD UINT32_C(0x41555448)



/* 校验会话公开状态，避免未初始化对象进入事务路径。 */
static bool xsshAuthSessionValid(const xsshauthsession* pSession)
{
	return xrtMemRangeValid(pSession, sizeof(*pSession)) &&
		(pSession->ObjectGuard == XSSH_AUTH_SESSION_GUARD) &&
		((pSession->Role == XSSH_ROLE_CLIENT) ||
		 (pSession->Role == XSSH_ROLE_SERVER)) &&
		(pSession->Phase >= XSSH_AUTH_SESSION_IDLE) &&
		(pSession->Phase <= XSSH_AUTH_SESSION_FAILED) &&
		(pSession->Event >= XSSH_AUTH_SESSION_EVENT_NONE) &&
		(pSession->Event <= XSSH_AUTH_SESSION_EVENT_FAILED) &&
		(pSession->WritePending >= XSSH_AUTH_SESSION_PACKET_NONE) &&
		(pSession->WritePending <= XSSH_AUTH_SESSION_PACKET_METHOD) &&
		(pSession->ReadPending >= XSSH_AUTH_SESSION_PACKET_NONE) &&
		(pSession->ReadPending <= XSSH_AUTH_SESSION_PACKET_METHOD);
}



/* 比较不以零结尾的协议文本与编译期常量。 */
static bool xsshAuthSessionTextEqual(xstrview Text, const char* sValue)
{
	size_t iSize = strlen(sValue);

	return (Text.Size == iSize) &&
		((iSize == 0u) || (memcmp(Text.Data, sValue, iSize) == 0));
}



/* 清除当前待读消息借用视图。 */
static void xsshAuthSessionReadViewsClear(xsshauthsession* pSession)
{
	memset(&pSession->Request, 0, sizeof(pSession->Request));
	memset(&pSession->Failure, 0, sizeof(pSession->Failure));
	memset(&pSession->Banner, 0, sizeof(pSession->Banner));
	pSession->Method = (xbytesview){ NULL, 0u };
}



/* 清除写事务描述，不推进主状态。 */
static void xsshAuthSessionWriteClear(xsshauthsession* pSession)
{
	pSession->WritePending = XSSH_AUTH_SESSION_PACKET_NONE;
	pSession->WriteOrdinal = 0u;
	memset(&pSession->PendingBudget, 0, sizeof(pSession->PendingBudget));
}



/* 清除读事务描述和全部借用视图。 */
static void xsshAuthSessionReadClear(xsshauthsession* pSession)
{
	pSession->ReadPending = XSSH_AUTH_SESSION_PACKET_NONE;
	pSession->ReadOrdinal = 0u;
	xsshAuthSessionReadViewsClear(pSession);
	memset(&pSession->PendingBudget, 0, sizeof(pSession->PendingBudget));
}



/* 失败状态解除所有认证事务，transport 的关闭仍由驱动执行。 */
static void xsshAuthSessionSetFailed(xsshauthsession* pSession)
{
	xsshAuthSessionWriteClear(pSession);
	xsshAuthSessionReadClear(pSession);
	pSession->ContinueAllowed = false;
	pSession->Phase = XSSH_AUTH_SESSION_FAILED;
	pSession->Event = XSSH_AUTH_SESSION_EVENT_FAILED;
}



/* 检查 core 与认证会话的角色、地址和首轮密钥边界。 */
static bool xsshAuthSessionCoreReady(
	const xsshauthsession* pSession,
	const xsshtransportcore* pCore
)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) && (pCore->State.Role == pSession->Role) &&
		!pCore->Write.Active && !pCore->Read.Active &&
		xrtSshTransportCoreKexComplete(pCore);
}



/* 返回当前角色应观察的 server USERAUTH_SUCCESS 方向。 */
static bool xsshAuthSessionCoreSuccess(
	const xsshauthsession* pSession,
	const xsshtransportcore* pCore
)
{
	return pSession->Role == XSSH_ROLE_SERVER ?
		pCore->State.LocalAuthSuccess : pCore->State.PeerAuthSuccess;
}



/* 将完整 payload 严格分类，并解析通用认证视图。 */
static xsshcode xsshAuthSessionPacketRead(
	xbytesview Payload,
	xsshauthsessionpacket* pPacket,
	xsshauthrequest* pRequest,
	xsshauthfailure* pFailure,
	xsshauthbanner* pBanner
)
{
	xsshservice Service;
	uint8 iMessage;
	xsshcode Code;

	memset(pRequest, 0, sizeof(*pRequest));
	memset(pFailure, 0, sizeof(*pFailure));
	memset(pBanner, 0, sizeof(*pBanner));
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage == XSSH_MSG_SERVICE_REQUEST ) {
		Code = xrtSshServiceRequestRead(Payload, &Service);
		*pPacket = XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST;
	} else if ( iMessage == XSSH_MSG_SERVICE_ACCEPT ) {
		Code = xrtSshServiceAcceptRead(Payload, &Service);
		*pPacket = XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT;
	} else if ( iMessage == XSSH_MSG_USERAUTH_REQUEST ) {
		Code = xrtSshAuthRequestRead(Payload, pRequest);
		*pPacket = XSSH_AUTH_SESSION_PACKET_REQUEST;
	} else if ( iMessage == XSSH_MSG_USERAUTH_FAILURE ) {
		Code = xrtSshAuthFailureRead(Payload, pFailure);
		*pPacket = XSSH_AUTH_SESSION_PACKET_FAILURE;
	} else if ( iMessage == XSSH_MSG_USERAUTH_SUCCESS ) {
		Code = xrtSshAuthSuccessRead(Payload);
		*pPacket = XSSH_AUTH_SESSION_PACKET_SUCCESS;
	} else if ( iMessage == XSSH_MSG_USERAUTH_BANNER ) {
		Code = xrtSshAuthBannerRead(Payload, pBanner);
		*pPacket = XSSH_AUTH_SESSION_PACKET_BANNER;
	} else if ( (iMessage >= 60u) && (iMessage <= 79u) ) {
		Code = XSSH_OK;
		*pPacket = XSSH_AUTH_SESSION_PACKET_METHOD;
	} else {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((*pPacket == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST) ||
		 (*pPacket == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT)) &&
		!xsshAuthSessionTextEqual(Service.Name, XSSH_SERVICE_USERAUTH) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( (*pPacket == XSSH_AUTH_SESSION_PACKET_REQUEST) &&
		!xsshAuthSessionTextEqual(
			pRequest->Service,
			XSSH_SERVICE_CONNECTION
		) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return XSSH_OK;
}



/* 判断当前驱动事件是否允许发送指定分类。 */
static bool xsshAuthSessionWriteAllowed(
	const xsshauthsession* pSession,
	xsshauthsessionpacket Packet
)
{
	if ( (Packet == XSSH_AUTH_SESSION_PACKET_BANNER) &&
		(pSession->Role == XSSH_ROLE_SERVER) &&
		pSession->ServiceAccepted &&
		(pSession->Phase == XSSH_AUTH_SESSION_AUTHENTICATION) ) {
		return true;
	}
	if ( pSession->Role == XSSH_ROLE_CLIENT ) {
		if ( pSession->Event ==
			XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST ) {
			return Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST;
		}
		if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST ) {
			return (Packet == XSSH_AUTH_SESSION_PACKET_REQUEST) ||
				(pSession->ContinueAllowed &&
				 (Packet == XSSH_AUTH_SESSION_PACKET_METHOD));
		}
		return false;
	}
	if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_ACCEPT ) {
		return Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT;
	}
	if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_WRITE_RESULT ) {
		return (Packet == XSSH_AUTH_SESSION_PACKET_FAILURE) ||
			(Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS) ||
			(Packet == XSSH_AUTH_SESSION_PACKET_METHOD);
	}
	return false;
}



/* 判断当前驱动事件是否允许接收指定分类。 */
static bool xsshAuthSessionReadAllowed(
	const xsshauthsession* pSession,
	xsshauthsessionpacket Packet
)
{
	if ( (Packet == XSSH_AUTH_SESSION_PACKET_BANNER) &&
		(pSession->Role == XSSH_ROLE_CLIENT) &&
		pSession->ServiceAccepted &&
		(pSession->Phase == XSSH_AUTH_SESSION_AUTHENTICATION) ) {
		return true;
	}
	if ( pSession->Role == XSSH_ROLE_SERVER ) {
		if ( pSession->Event ==
			XSSH_AUTH_SESSION_EVENT_READ_SERVICE_REQUEST ) {
			return Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST;
		}
		if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_READ_REQUEST ) {
			return (Packet == XSSH_AUTH_SESSION_PACKET_REQUEST) ||
				(pSession->ContinueAllowed &&
				 (Packet == XSSH_AUTH_SESSION_PACKET_METHOD));
		}
		return false;
	}
	if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_READ_SERVICE_ACCEPT ) {
		return Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT;
	}
	if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_READ_RESULT ) {
		return (Packet == XSSH_AUTH_SESSION_PACKET_FAILURE) ||
			(Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS) ||
			(Packet == XSSH_AUTH_SESSION_PACKET_METHOD);
	}
	return false;
}



/* 把消息分类转换成认证预算事件。 */
static xsshauthevent xsshAuthSessionBudgetEvent(
	xsshauthsessionpacket Packet,
	bool bServerMessage
)
{
	if ( Packet == XSSH_AUTH_SESSION_PACKET_REQUEST ) {
		return XSSH_AUTH_EVENT_ATTEMPT;
	}
	if ( (Packet == XSSH_AUTH_SESSION_PACKET_METHOD) && bServerMessage ) {
		return XSSH_AUTH_EVENT_ROUND;
	}
	return XSSH_AUTH_EVENT_MESSAGE;
}



/* 在临时副本中预留预算，事务提交前不修改正式计数。 */
static xsshcode xsshAuthSessionBudgetPrepare(
	xsshauthsession* pSession,
	xsshauthsessionpacket Packet,
	bool bServerMessage,
	size_t iPayloadSize,
	double Timer
)
{
	xsshauthguarddecision Decision;
	xsshauthguard Budget = pSession->Budget;
	xsshcode Code;

	if ( (Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST) ||
		(Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT) ) {
		pSession->PendingBudget = Budget;
		return XSSH_OK;
	}
	Code = xrtSshAuthGuardReserve(
		&Budget,
		xsshAuthSessionBudgetEvent(Packet, bServerMessage),
		(uint64)iPayloadSize,
		Timer,
		&Decision
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Decision != XSSH_AUTH_GUARD_ALLOW ) {
		pSession->Budget = Budget;
		xsshAuthSessionSetFailed(pSession);
		return XSSH_ERROR_AUTHENTICATION;
	}
	pSession->PendingBudget = Budget;
	return XSSH_OK;
}



/* 提交成功分类时同时冻结认证预算。 */
static bool xsshAuthSessionBudgetCommit(
	xsshauthsession* pSession,
	xsshauthsessionpacket Packet
)
{
	if ( (Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS) &&
		!xrtSshAuthGuardComplete(&pSession->PendingBudget) ) {
		return false;
	}
	pSession->Budget = pSession->PendingBudget;
	return true;
}



/* 按可靠写边界推进 client/server 认证步骤。 */
static void xsshAuthSessionWriteAdvance(
	xsshauthsession* pSession,
	xsshauthsessionpacket Packet
)
{
	if ( Packet == XSSH_AUTH_SESSION_PACKET_BANNER ) {
		return;
	}
	if ( pSession->Role == XSSH_ROLE_CLIENT ) {
		if ( Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST ) {
			pSession->Event = XSSH_AUTH_SESSION_EVENT_READ_SERVICE_ACCEPT;
		} else {
			pSession->ContinueAllowed = false;
			pSession->Event = XSSH_AUTH_SESSION_EVENT_READ_RESULT;
		}
		return;
	}
	if ( Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT ) {
		pSession->ServiceAccepted = true;
		pSession->ContinueAllowed = false;
		pSession->Phase = XSSH_AUTH_SESSION_AUTHENTICATION;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_READ_REQUEST;
	} else if ( Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS ) {
		pSession->ContinueAllowed = false;
		pSession->Phase = XSSH_AUTH_SESSION_COMPLETE;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_COMPLETE;
	} else {
		pSession->ContinueAllowed =
			Packet == XSSH_AUTH_SESSION_PACKET_METHOD;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_READ_REQUEST;
	}
}



/* 按认证输入提交边界推进 client/server 认证步骤。 */
static void xsshAuthSessionReadAdvance(
	xsshauthsession* pSession,
	xsshauthsessionpacket Packet
)
{
	if ( Packet == XSSH_AUTH_SESSION_PACKET_BANNER ) {
		return;
	}
	if ( pSession->Role == XSSH_ROLE_SERVER ) {
		if ( Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST ) {
			pSession->Event = XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_ACCEPT;
		} else {
			pSession->ContinueAllowed = false;
			pSession->Event = XSSH_AUTH_SESSION_EVENT_WRITE_RESULT;
		}
		return;
	}
	if ( Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT ) {
		pSession->ServiceAccepted = true;
		pSession->ContinueAllowed = false;
		pSession->Phase = XSSH_AUTH_SESSION_AUTHENTICATION;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST;
	} else if ( Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS ) {
		pSession->ContinueAllowed = false;
		pSession->Phase = XSSH_AUTH_SESSION_COMPLETE;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_COMPLETE;
	} else {
		pSession->ContinueAllowed =
			Packet == XSSH_AUTH_SESSION_PACKET_METHOD;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST;
	}
}



/* 判断输出对象是否会覆盖会话或当前借用输入。 */
static bool xsshAuthSessionOutputOverlap(
	const xsshauthsession* pSession,
	const void* pOutput,
	size_t iOutputSize
)
{
	return xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Request.User.Data,
		pSession->Request.User.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Request.Service.Data,
		pSession->Request.Service.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Request.Method.Data,
		pSession->Request.Method.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Request.Fields.Data,
		pSession->Request.Fields.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Failure.Methods.Data,
		pSession->Failure.Methods.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Banner.Message.Data,
		pSession->Banner.Message.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Banner.Language.Data,
		pSession->Banner.Language.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Method.Data,
		pSession->Method.Size,
		pOutput,
		iOutputSize
	);
}



/* 初始化空认证会话。 */
bool xrtSshAuthSessionInit(xsshauthsession* pSession, xsshrole Role)
{
	xsshauthsession Session;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	memset(&Session, 0, sizeof(Session));
	Session.Role = Role;
	Session.Phase = XSSH_AUTH_SESSION_IDLE;
	Session.ObjectGuard = XSSH_AUTH_SESSION_GUARD;
	*pSession = Session;
	return true;
}



/* 清除认证状态和借用视图。 */
void xrtSshAuthSessionClear(xsshauthsession* pSession)
{
	if ( xrtMemRangeValid(pSession, sizeof(*pSession)) ) {
		memset(pSession, 0, sizeof(*pSession));
	}
}



/* 从已经完成首轮 KEX 的 transport 开始认证。 */
xsshcode xrtSshAuthSessionBegin(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	const xsshauthguardpolicy* pPolicy,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshauthsession Session;

	if ( !xsshAuthSessionValid(pSession) ||
		!xsshAuthSessionCoreReady(pSession, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pPolicy != NULL) &&
		(!xrtMemRangeValid(pPolicy, sizeof(*pPolicy)) ||
		 xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pPolicy,
			sizeof(*pPolicy)
		 ) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPolicy,
			sizeof(*pPolicy)
		 )) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pSession->Active ||
		(pSession->Phase != XSSH_AUTH_SESSION_IDLE) ||
		xsshAuthSessionCoreSuccess(pSession, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	Session = *pSession;
	if ( !xrtSshAuthGuardInit(&Session.Budget, pPolicy, Timer) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Session.Active = true;
	Session.Phase = XSSH_AUTH_SESSION_SERVICE;
	Session.Event = Session.Role == XSSH_ROLE_CLIENT ?
		XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST :
		XSSH_AUTH_SESSION_EVENT_READ_SERVICE_REQUEST;
	*pSession = Session;
	return XSSH_OK;
}



/* 查询下一认证动作。 */
xsshauthsessionevent xrtSshAuthSessionEvent(
	const xsshauthsession* pSession
)
{
	return xsshAuthSessionValid(pSession) ?
		pSession->Event : XSSH_AUTH_SESSION_EVENT_NONE;
}



/* 复制资源预算快照。 */
xsshcode xrtSshAuthSessionBudget(
	const xsshauthsession* pSession,
	xsshauthguard* pBudget
)
{
	if ( !xsshAuthSessionValid(pSession) || !pSession->Active ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pBudget, sizeof(*pBudget)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pBudget,
			sizeof(*pBudget)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pBudget = pSession->Budget;
	return XSSH_OK;
}



/* 检查无外部时钟依赖的认证预算。 */
xsshcode xrtSshAuthSessionCheck(
	xsshauthsession* pSession,
	double Timer,
	xsshauthguarddecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshauthguarddecision Decision;
	xsshcode Code;

	if ( !xsshAuthSessionValid(pSession) || !pSession->Active ||
		(pSession->Phase == XSSH_AUTH_SESSION_FAILED) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthGuardCheck(&pSession->Budget, Timer, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pDecision = Decision;
	if ( Decision == XSSH_AUTH_GUARD_DISCONNECT ) {
		xsshAuthSessionSetFailed(pSession);
	}
	return XSSH_OK;
}



/* 准备不复制 payload 的认证写事务。 */
xsshcode xrtSshAuthSessionWritePrepare(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshauthsessionpacket Packet = XSSH_AUTH_SESSION_PACKET_NONE;
	xsshauthrequest Request;
	xsshauthfailure Failure;
	xsshauthbanner Banner;
	xsshcode Code;

	if ( !xsshAuthSessionValid(pSession) || !pSession->Active ||
		(pSession->Phase == XSSH_AUTH_SESSION_COMPLETE) ||
		(pSession->Phase == XSSH_AUTH_SESSION_FAILED) ||
		(pSession->WritePending != XSSH_AUTH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_NONE) ||
		!xsshAuthSessionCoreReady(pSession, pCore) ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_LOCAL
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshAuthSessionPacketRead(
		Payload,
		&Packet,
		&Request,
		&Failure,
		&Banner
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshAuthSessionWriteAllowed(pSession, Packet) ||
		(pCore->State.LocalPackets == UINT64_MAX) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshAuthSessionBudgetPrepare(
		pSession,
		Packet,
		pSession->Role == XSSH_ROLE_SERVER,
		Payload.Size,
		Timer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->WritePending = Packet;
	pSession->WriteOrdinal = pCore->State.LocalPackets + 1u;
	return XSSH_OK;
}



/* 提交 transport 已可靠发送的认证消息。 */
xsshcode xrtSshAuthSessionWriteCommit(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshauthsessionpacket Packet;

	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->WritePending == XSSH_AUTH_SESSION_PACKET_NONE) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) || (pCore->State.Role != pSession->Role) ||
		pCore->Write.Active ||
		(pCore->State.LocalPackets != pSession->WriteOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	Packet = pSession->WritePending;
	if ( ((Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS) &&
		 !xsshAuthSessionCoreSuccess(pSession, pCore)) ||
		!xsshAuthSessionBudgetCommit(pSession, Packet) ) {
		xsshAuthSessionSetFailed(pSession);
		return XSSH_ERROR_STATE;
	}
	xsshAuthSessionWriteAdvance(pSession, Packet);
	xsshAuthSessionWriteClear(pSession);
	return XSSH_OK;
}



/* 无损放弃未提交的认证输出。 */
xsshcode xrtSshAuthSessionWriteAbort(xsshauthsession* pSession)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->WritePending == XSSH_AUTH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	xsshAuthSessionWriteClear(pSession);
	return XSSH_OK;
}



/* 准备一个已经由 transport core 认证的输入事务。 */
xsshcode xrtSshAuthSessionReadPrepare(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	double Timer,
	xsshauthsessionpacket* pPacket
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshauthsessionpacket Packet = XSSH_AUTH_SESSION_PACKET_NONE;
	xsshauthrequest Request;
	xsshauthfailure Failure;
	xsshauthbanner Banner;
	uint8 iMessage;
	xsshcode Code;

	if ( !xsshAuthSessionValid(pSession) || !pSession->Active ||
		(pSession->Phase == XSSH_AUTH_SESSION_COMPLETE) ||
		(pSession->Phase == XSSH_AUTH_SESSION_FAILED) ||
		(pSession->WritePending != XSSH_AUTH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_NONE) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) || (pCore->State.Role != pSession->Role) ||
		!pCore->Read.Active ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_PEER
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pPacket,
			sizeof(*pPacket)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( (Code != XSSH_OK) || (iMessage != pCore->Read.Message) ) {
		return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
	}
	Code = xsshAuthSessionPacketRead(
		Payload,
		&Packet,
		&Request,
		&Failure,
		&Banner
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshAuthSessionReadAllowed(pSession, Packet) ||
		(pCore->State.PeerPackets == UINT64_MAX) ) {
		xsshAuthSessionSetFailed(pSession);
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xsshAuthSessionBudgetPrepare(
		pSession,
		Packet,
		pSession->Role == XSSH_ROLE_CLIENT,
		Payload.Size,
		Timer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->Request = Request;
	pSession->Failure = Failure;
	pSession->Banner = Banner;
	if ( Packet == XSSH_AUTH_SESSION_PACKET_METHOD ) {
		pSession->Method = Payload;
	}
	pSession->ReadPending = Packet;
	pSession->ReadOrdinal = pCore->State.PeerPackets + 1u;
	*pPacket = Packet;
	return XSSH_OK;
}



/* 返回当前通用认证请求。 */
xsshcode xrtSshAuthSessionRequest(
	const xsshauthsession* pSession,
	xsshauthrequest* pRequest
)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_REQUEST) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pRequest, sizeof(*pRequest)) ||
		xsshAuthSessionOutputOverlap(
			pSession,
			pRequest,
			sizeof(*pRequest)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pRequest = pSession->Request;
	return XSSH_OK;
}



/* 返回当前认证失败方法列表。 */
xsshcode xrtSshAuthSessionFailure(
	const xsshauthsession* pSession,
	xsshauthfailure* pFailure
)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_FAILURE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pFailure, sizeof(*pFailure)) ||
		xsshAuthSessionOutputOverlap(
			pSession,
			pFailure,
			sizeof(*pFailure)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pFailure = pSession->Failure;
	return XSSH_OK;
}



/* 返回当前认证横幅。 */
xsshcode xrtSshAuthSessionBanner(
	const xsshauthsession* pSession,
	xsshauthbanner* pBanner
)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_BANNER) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pBanner, sizeof(*pBanner)) ||
		xsshAuthSessionOutputOverlap(
			pSession,
			pBanner,
			sizeof(*pBanner)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pBanner = pSession->Banner;
	return XSSH_OK;
}



/* 返回当前方法专用 payload。 */
xsshcode xrtSshAuthSessionMethod(
	const xsshauthsession* pSession,
	xbytesview* pPayload
)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_METHOD) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pPayload, sizeof(*pPayload)) ||
		xsshAuthSessionOutputOverlap(
			pSession,
			pPayload,
			sizeof(*pPayload)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pPayload = pSession->Method;
	return XSSH_OK;
}



/* 提交 transport 已接受的认证输入。 */
xsshcode xrtSshAuthSessionReadCommit(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshauthsessionpacket Packet;

	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending == XSSH_AUTH_SESSION_PACKET_NONE) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) || (pCore->State.Role != pSession->Role) ||
		pCore->Read.Active ||
		(pCore->State.PeerPackets != pSession->ReadOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	Packet = pSession->ReadPending;
	if ( ((Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS) &&
		 !xsshAuthSessionCoreSuccess(pSession, pCore)) ||
		!xsshAuthSessionBudgetCommit(pSession, Packet) ) {
		xsshAuthSessionSetFailed(pSession);
		return XSSH_ERROR_STATE;
	}
	xsshAuthSessionReadAdvance(pSession, Packet);
	xsshAuthSessionReadClear(pSession);
	return XSSH_OK;
}



/* 已认证输入不可回滚，放弃时终止会话。 */
xsshcode xrtSshAuthSessionReadAbort(xsshauthsession* pSession)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending == XSSH_AUTH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	xsshAuthSessionSetFailed(pSession);
	return XSSH_OK;
}



/* 显式终止认证状态机。 */
void xrtSshAuthSessionFail(xsshauthsession* pSession)
{
	if ( xsshAuthSessionValid(pSession) && pSession->Active &&
		(pSession->Phase != XSSH_AUTH_SESSION_COMPLETE) ) {
		xsshAuthSessionSetFailed(pSession);
	}
}



/* 校验认证会话与 transport 的 server 成功方向一致。 */
bool xrtSshAuthSessionComplete(
	const xsshauthsession* pSession,
	const xsshtransportcore* pCore
)
{
	return xsshAuthSessionValid(pSession) && pSession->Active &&
		xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) && (pCore->State.Role == pSession->Role) &&
		(pSession->Phase == XSSH_AUTH_SESSION_COMPLETE) &&
		(pSession->Event == XSSH_AUTH_SESSION_EVENT_COMPLETE) &&
		(pSession->WritePending == XSSH_AUTH_SESSION_PACKET_NONE) &&
		(pSession->ReadPending == XSSH_AUTH_SESSION_PACKET_NONE) &&
		pSession->Budget.Complete &&
		xsshAuthSessionCoreSuccess(pSession, pCore) &&
		xrtSshTransportCoreKexComplete(pCore);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_connection_message.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CONNECTION_MESSAGE)



#if defined(XSSH_FEATURE_CONNECTION_MESSAGE)

/* 写入可扩展全局请求并一次发布 writer。 */
xsshcode xrtSshGlobalRequestWrite(
	xsshwriter* pWriter,
	xstrview Name,
	bool bWantReply,
	xbytesview Fields
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( !xrtMemRangeValid(Fields.Data, Fields.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshGlobalRequestWriteBegin(
		pWriter,
		Name,
		bWantReply,
		Fields.Size,
		&Fields,
		1u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteBytes(&Writer, Fields) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取请求前缀并把剩余专用字段完整借出。 */
xsshcode xrtSshGlobalRequestRead(
	xbytesview Payload,
	xsshglobalrequest* pRequest
)
{
	xsshreader Reader;
	xsshglobalrequest Request;
	xbytesview Value;
	uint8 iMessage;
	xsshcode Code;

	if ( (pRequest == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_GLOBAL_REQUEST ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Request.Name = (xstrview){ (const char*)Value.Data, Value.Size };
	if ( !xrtSshNameValid(Request.Name) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadBool(&Reader, &Request.WantReply);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadBytes(
		&Reader,
		xrtSshReaderRemaining(&Reader),
		&Request.Fields
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pRequest = Request;
	return XSSH_OK;
}



/* 写入带任意专用数据的全局请求成功响应。 */
xsshcode xrtSshGlobalSuccessWrite(
	xsshwriter* pWriter,
	xbytesview Fields
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( Fields.Size == SIZE_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshWriterReserveInputs(pWriter, 1u + Fields.Size, &Fields, 1u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_REQUEST_SUCCESS
	) != XSSH_OK) || (xrtSshWriteBytes(
		&Writer,
		Fields
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取成功响应并借用全部请求专用数据。 */
xsshcode xrtSshGlobalSuccessRead(
	xbytesview Payload,
	xbytesview* pFields
)
{
	xsshreader Reader;
	xbytesview Fields;
	uint8 iMessage;
	xsshcode Code;

	if ( (pFields == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_REQUEST_SUCCESS ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadBytes(
		&Reader,
		xrtSshReaderRemaining(&Reader),
		&Fields
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pFields = Fields;
	return XSSH_OK;
}



/* 写入无字段全局请求失败响应。 */
xsshcode xrtSshGlobalFailureWrite(xsshwriter* pWriter)
{
	xsshwriter Writer;
	xsshcode Code = xrtSshWriterReserve(pWriter, 1u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( xrtSshWriteByte(&Writer, XSSH_MSG_REQUEST_FAILURE) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取无字段全局请求失败响应。 */
xsshcode xrtSshGlobalFailureRead(xbytesview Payload)
{
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Payload.Size == 0u ) {
		return XSSH_NEED_MORE;
	}
	return (Payload.Size == 1u) &&
		(Payload.Data[0] == XSSH_MSG_REQUEST_FAILURE) ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_channel_message.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CHANNEL_MESSAGE)



#if defined(XSSH_FEATURE_CHANNEL_MESSAGE)

/* 初始化局部 reader，并拒绝输出对象覆盖输入 payload。 */
static xsshcode xsshChannelReadBegin(
	xbytesview Payload,
	void* pOutput,
	size_t iOutputSize,
	uint8 iExpected,
	xsshreader* pReader
)
{
	uint8 iMessage;
	xsshcode Code;

	if ( (pOutput == NULL) || (pReader == NULL) ||
		!xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pOutput,
			iOutputSize
		) || !xrtSshReaderInit(pReader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(pReader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return iMessage == iExpected ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 借出 reader 的全部剩余字段。 */
static xsshcode xsshChannelReadFields(
	xsshreader* pReader,
	xbytesview* pFields
)
{
	return xrtSshReadBytes(
		pReader,
		xrtSshReaderRemaining(pReader),
		pFields
	);
}



/* 写入只包含消息号和 recipient channel 的固定消息。 */
static xsshcode xsshChannelSimpleWrite(
	xsshwriter* pWriter,
	uint8 iMessage,
	uint32 iRecipient
)
{
	xsshwriter Writer;
	xsshcode Code = xsshChannelPrepare(
		pWriter,
		5u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, iMessage) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iRecipient) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取只包含消息号和 recipient channel 的固定消息。 */
static xsshcode xsshChannelSimpleRead(
	xbytesview Payload,
	uint8 iMessage,
	uint32* pRecipient
)
{
	xsshreader Reader;
	uint32 iRecipient;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pRecipient,
		sizeof(*pRecipient),
		iMessage,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iRecipient);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pRecipient = iRecipient;
	return XSSH_OK;
}



/* 写入可扩展 channel open。 */
xsshcode xrtSshChannelOpenWrite(
	xsshwriter* pWriter,
	xstrview Type,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Fields
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( !xrtMemRangeValid(Fields.Data, Fields.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshChannelOpenWriteBegin(
		pWriter,
		Type,
		iSender,
		iWindow,
		iMaxPacket,
		Fields.Size,
		&Fields,
		1u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteBytes(&Writer, Fields) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取 channel open，并保留未知类型专用字段。 */
xsshcode xrtSshChannelOpenRead(
	xbytesview Payload,
	xsshchannelopen* pOpen
)
{
	xsshreader Reader;
	xsshchannelopen Open;
	xbytesview Value;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pOpen,
		sizeof(*pOpen),
		XSSH_MSG_CHANNEL_OPEN,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Open.Type = xsshChannelText(Value);
	if ( !xrtSshNameValid(Open.Type) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Open.Sender)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Open.Window)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Open.MaxPacket)) != XSSH_OK) ||
		((Code = xsshChannelReadFields(&Reader, &Open.Fields)) != XSSH_OK) ) {
		return Code;
	}
	if ( Open.MaxPacket == 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pOpen = Open;
	return XSSH_OK;
}



/* 写入 channel open confirmation。 */
xsshcode xrtSshChannelOpenConfirmationWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Fields
)
{
	xsshwriter Writer;
	size_t iTotal;
	xsshcode Code;

	if ( iMaxPacket == 0u ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtMemRangeValid(Fields.Data, Fields.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Fields.Size > (SIZE_MAX - 17u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal = 17u + Fields.Size;
	Code = xsshChannelPrepare(pWriter, iTotal, &Fields, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_CHANNEL_OPEN_CONFIRMATION
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iRecipient
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iSender
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iWindow
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iMaxPacket
	) != XSSH_OK) || (xrtSshWriteBytes(
		&Writer,
		Fields
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取 channel open confirmation 与专用字段。 */
xsshcode xrtSshChannelOpenConfirmationRead(
	xbytesview Payload,
	xsshchannelconfirmation* pConfirmation
)
{
	xsshreader Reader;
	xsshchannelconfirmation Confirmation;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pConfirmation,
		sizeof(*pConfirmation),
		XSSH_MSG_CHANNEL_OPEN_CONFIRMATION,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Confirmation.Recipient)) !=
		XSSH_OK) || ((Code = xrtSshReadU32(
		&Reader,
		&Confirmation.Sender
	)) != XSSH_OK) || ((Code = xrtSshReadU32(
		&Reader,
		&Confirmation.Window
	)) != XSSH_OK) || ((Code = xrtSshReadU32(
		&Reader,
		&Confirmation.MaxPacket
	)) != XSSH_OK) || ((Code = xsshChannelReadFields(
		&Reader,
		&Confirmation.Fields
	)) != XSSH_OK) ) {
		return Code;
	}
	if ( Confirmation.MaxPacket == 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pConfirmation = Confirmation;
	return XSSH_OK;
}



/* 写入 UTF-8 channel open failure。 */
xsshcode xrtSshChannelOpenFailureWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iReason,
	xstrview Description,
	xstrview Language
)
{
	xbytesview arrInputs[2] = {
		xsshChannelBytes(Description),
		xsshChannelBytes(Language)
	};
	xsshwriter Writer;
	size_t iTotal = 9u;
	xsshcode Code;

	if ( !xrtUtf8Valid(Description, NULL) ||
		!xrtSshLanguageValid(Language) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshChannelAddString(arrInputs[0], &iTotal)) != XSSH_OK) ||
		((Code = xsshChannelAddString(arrInputs[1], &iTotal)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshChannelPrepare(pWriter, iTotal, arrInputs, 2u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_CHANNEL_OPEN_FAILURE
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iRecipient
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iReason
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[0]
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[1]
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取并校验 channel open failure 文本字段。 */
xsshcode xrtSshChannelOpenFailureRead(
	xbytesview Payload,
	xsshchannelopenfailure* pFailure
)
{
	xsshreader Reader;
	xsshchannelopenfailure Failure;
	xbytesview Value;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pFailure,
		sizeof(*pFailure),
		XSSH_MSG_CHANNEL_OPEN_FAILURE,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Failure.Recipient)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Failure.Reason)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Value)) != XSSH_OK) ) {
		return Code;
	}
	Failure.Description = xsshChannelText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Failure.Language = xsshChannelText(Value);
	if ( !xrtUtf8Valid(Failure.Description, NULL) ||
		!xrtSshLanguageValid(Failure.Language) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pFailure = Failure;
	return XSSH_OK;
}



/* 写入 channel window adjust。 */
xsshcode xrtSshChannelWindowAdjustWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iBytes
)
{
	xsshwriter Writer;
	xsshcode Code = xsshChannelPrepare(
		pWriter,
		9u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_CHANNEL_WINDOW_ADJUST
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iRecipient
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iBytes
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 channel window adjust。 */
xsshcode xrtSshChannelWindowAdjustRead(
	xbytesview Payload,
	xsshchanneladjust* pAdjust
)
{
	xsshreader Reader;
	xsshchanneladjust Adjust;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pAdjust,
		sizeof(*pAdjust),
		XSSH_MSG_CHANNEL_WINDOW_ADJUST,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Adjust.Recipient)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Adjust.Bytes)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pAdjust = Adjust;
	return XSSH_OK;
}



/* 写入普通 channel data。 */
xsshcode xrtSshChannelDataWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xbytesview Data
)
{
	xsshwriter Writer;
	size_t iTotal = 5u;
	xsshcode Code = xsshChannelAddString(Data, &iTotal);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshChannelPrepare(pWriter, iTotal, &Data, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_CHANNEL_DATA) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iRecipient) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Data) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取普通 channel data。 */
xsshcode xrtSshChannelDataRead(
	xbytesview Payload,
	xsshchanneldata* pData
)
{
	xsshreader Reader;
	xsshchanneldata Data;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pData,
		sizeof(*pData),
		XSSH_MSG_CHANNEL_DATA,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Data.Recipient)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Data.Data)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pData = Data;
	return XSSH_OK;
}



/* 写入 channel extended data。 */
xsshcode xrtSshChannelExtendedDataWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iType,
	xbytesview Data
)
{
	xsshwriter Writer;
	size_t iTotal = 9u;
	xsshcode Code = xsshChannelAddString(Data, &iTotal);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshChannelPrepare(pWriter, iTotal, &Data, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_CHANNEL_EXTENDED_DATA
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iRecipient
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iType
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		Data
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 channel extended data。 */
xsshcode xrtSshChannelExtendedDataRead(
	xbytesview Payload,
	xsshchannelextendeddata* pData
)
{
	xsshreader Reader;
	xsshchannelextendeddata Data;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pData,
		sizeof(*pData),
		XSSH_MSG_CHANNEL_EXTENDED_DATA,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Data.Recipient)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Data.Type)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Data.Data)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pData = Data;
	return XSSH_OK;
}



/* 写入 channel EOF。 */
xsshcode xrtSshChannelEofWrite(xsshwriter* pWriter, uint32 iRecipient)
{
	return xsshChannelSimpleWrite(
		pWriter,
		XSSH_MSG_CHANNEL_EOF,
		iRecipient
	);
}



/* 严格读取 channel EOF。 */
xsshcode xrtSshChannelEofRead(xbytesview Payload, uint32* pRecipient)
{
	return xsshChannelSimpleRead(
		Payload,
		XSSH_MSG_CHANNEL_EOF,
		pRecipient
	);
}



/* 写入 channel close。 */
xsshcode xrtSshChannelCloseWrite(xsshwriter* pWriter, uint32 iRecipient)
{
	return xsshChannelSimpleWrite(
		pWriter,
		XSSH_MSG_CHANNEL_CLOSE,
		iRecipient
	);
}



/* 严格读取 channel close。 */
xsshcode xrtSshChannelCloseRead(xbytesview Payload, uint32* pRecipient)
{
	return xsshChannelSimpleRead(
		Payload,
		XSSH_MSG_CHANNEL_CLOSE,
		pRecipient
	);
}



/* 写入保留未知专用字段的 channel request。 */
xsshcode xrtSshChannelRequestWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Type,
	bool bWantReply,
	xbytesview Fields
)
{
	xbytesview arrInputs[2] = { xsshChannelBytes(Type), Fields };
	xsshwriter Writer;
	size_t iTotal = 6u;
	xsshcode Code;

	if ( !xrtSshNameValid(Type) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshChannelAddString(arrInputs[0], &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtMemRangeValid(Fields.Data, Fields.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Fields.Size > (SIZE_MAX - iTotal) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal += Fields.Size;
	Code = xsshChannelPrepare(pWriter, iTotal, arrInputs, 2u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_CHANNEL_REQUEST) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iRecipient) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteBool(&Writer, bWantReply) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, Fields) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取 channel request，并借出全部未知专用字段。 */
xsshcode xrtSshChannelRequestRead(
	xbytesview Payload,
	xsshchannelrequest* pRequest
)
{
	xsshreader Reader;
	xsshchannelrequest Request;
	xbytesview Value;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pRequest,
		sizeof(*pRequest),
		XSSH_MSG_CHANNEL_REQUEST,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Request.Recipient)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Value)) != XSSH_OK) ) {
		return Code;
	}
	Request.Type = xsshChannelText(Value);
	if ( !xrtSshNameValid(Request.Type) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( ((Code = xrtSshReadBool(&Reader, &Request.WantReply)) != XSSH_OK) ||
		((Code = xsshChannelReadFields(&Reader, &Request.Fields)) != XSSH_OK) ) {
		return Code;
	}
	*pRequest = Request;
	return XSSH_OK;
}



/* 写入 channel request success。 */
xsshcode xrtSshChannelSuccessWrite(xsshwriter* pWriter, uint32 iRecipient)
{
	return xsshChannelSimpleWrite(
		pWriter,
		XSSH_MSG_CHANNEL_SUCCESS,
		iRecipient
	);
}



/* 严格读取 channel request success。 */
xsshcode xrtSshChannelSuccessRead(xbytesview Payload, uint32* pRecipient)
{
	return xsshChannelSimpleRead(
		Payload,
		XSSH_MSG_CHANNEL_SUCCESS,
		pRecipient
	);
}



/* 写入 channel request failure。 */
xsshcode xrtSshChannelFailureWrite(xsshwriter* pWriter, uint32 iRecipient)
{
	return xsshChannelSimpleWrite(
		pWriter,
		XSSH_MSG_CHANNEL_FAILURE,
		iRecipient
	);
}



/* 严格读取 channel request failure。 */
xsshcode xrtSshChannelFailureRead(xbytesview Payload, uint32* pRecipient)
{
	return xsshChannelSimpleRead(
		Payload,
		XSSH_MSG_CHANNEL_FAILURE,
		pRecipient
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_channel_window.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CHANNEL_WINDOW)



#if defined(XSSH_FEATURE_CHANNEL_WINDOW)

/* 校验不会被调用方直接修改破坏的固定窗口约束。 */
static bool xsshChannelWindowValid(const xsshchannelwindow* pWindow)
{
	return (pWindow != NULL) && (pWindow->SendMaxPacket != 0u) &&
		(pWindow->ReceiveMaxPacket != 0u) &&
		(pWindow->AdjustThreshold != 0u);
}



/* 初始化纯数值窗口状态。 */
bool xrtSshChannelWindowInit(
	xsshchannelwindow* pWindow,
	uint32 iSendWindow,
	uint32 iSendMaxPacket,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
)
{
	xsshchannelwindow Window;

	if ( (pWindow == NULL) || (iSendMaxPacket == 0u) ||
		(iReceiveMaxPacket == 0u) || (iAdjustThreshold == 0u) ) {
		return false;
	}
	Window.SendWindow = iSendWindow;
	Window.SendMaxPacket = iSendMaxPacket;
	Window.ReceiveWindow = iReceiveWindow;
	Window.ReceiveMaxPacket = iReceiveMaxPacket;
	Window.AdjustThreshold = iAdjustThreshold;
	Window.ReceiveBuffered = 0u;
	Window.ReceivePending = 0u;
	*pWindow = Window;
	return true;
}



/* 返回同时受远端窗口和最大包限制的发送上限。 */
uint32 xrtSshChannelSendLimit(const xsshchannelwindow* pWindow)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return 0u;
	}
	return pWindow->SendWindow < pWindow->SendMaxPacket ?
		pWindow->SendWindow : pWindow->SendMaxPacket;
}



/* 扣减已经可靠排队的发送字节。 */
xsshcode xrtSshChannelSendCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iBytes > pWindow->SendMaxPacket ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iBytes > pWindow->SendWindow ) {
		return XSSH_ERROR_SPACE;
	}
	pWindow->SendWindow -= iBytes;
	return XSSH_OK;
}



/* 增加远端发送窗口，拒绝 RFC 4254 禁止的 uint32 回绕。 */
xsshcode xrtSshChannelSendAdjust(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iBytes > (UINT32_MAX - pWindow->SendWindow) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pWindow->SendWindow += iBytes;
	return XSSH_OK;
}



/* 校验远端数据并从已通告窗口转入应用缓冲计数。 */
xsshcode xrtSshChannelReceiveCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (iBytes > pWindow->ReceiveMaxPacket) ||
		(iBytes > pWindow->ReceiveWindow) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( (uint64)iBytes > (UINT64_MAX - pWindow->ReceiveBuffered) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	pWindow->ReceiveWindow -= iBytes;
	pWindow->ReceiveBuffered += (uint64)iBytes;
	return XSSH_OK;
}



/* 将应用消费量从缓冲计数转入待返还计数。 */
xsshcode xrtSshChannelReceiveConsume(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (uint64)iBytes > pWindow->ReceiveBuffered ) {
		return XSSH_ERROR_STATE;
	}
	if ( (uint64)iBytes > (UINT64_MAX - pWindow->ReceivePending) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	pWindow->ReceiveBuffered -= (uint64)iBytes;
	pWindow->ReceivePending += (uint64)iBytes;
	return XSSH_OK;
}



/* 判断返还额度是否已值得产生一条 WINDOW_ADJUST。 */
bool xrtSshChannelReceiveAdjustReady(const xsshchannelwindow* pWindow)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return false;
	}
	return (pWindow->ReceivePending >=
		(uint64)pWindow->AdjustThreshold) ||
		((pWindow->ReceiveWindow == 0u) &&
		 (pWindow->ReceivePending != 0u));
}



/* 计算不会令当前 uint32 接收窗口回绕的返还额度。 */
uint32 xrtSshChannelReceiveAdjustLimit(
	const xsshchannelwindow* pWindow
)
{
	uint64 iLimit;

	if ( !xsshChannelWindowValid(pWindow) ) {
		return 0u;
	}
	iLimit = (uint64)(UINT32_MAX - pWindow->ReceiveWindow);
	if ( pWindow->ReceivePending < iLimit ) {
		iLimit = pWindow->ReceivePending;
	}
	return (uint32)iLimit;
}



/* 提交已经可靠排队的消费额度返还。 */
xsshcode xrtSshChannelReceiveAdjustCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((uint64)iBytes > pWindow->ReceivePending) ||
		(iBytes > (UINT32_MAX - pWindow->ReceiveWindow)) ) {
		return XSSH_ERROR_STATE;
	}
	pWindow->ReceivePending -= (uint64)iBytes;
	pWindow->ReceiveWindow += iBytes;
	return XSSH_OK;
}



/* 提交新获得的独立接收容量，不消耗待返还计数。 */
xsshcode xrtSshChannelReceiveGrantCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iBytes > (UINT32_MAX - pWindow->ReceiveWindow) ) {
		return XSSH_ERROR_STATE;
	}
	pWindow->ReceiveWindow += iBytes;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_channel_request.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CHANNEL_REQUEST)



#if defined(XSSH_FEATURE_CHANNEL_REQUEST)

/* 写入只含一个 SSH string 的 request。 */
static xsshcode xsshRequestOneStringWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Type,
	bool bWantReply,
	xbytesview Value
)
{
	xsshwriter Writer;
	size_t iFieldsSize = 0u;
	xsshcode Code = xsshRequestAddString(Value, &iFieldsSize);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		Type,
		bWantReply,
		iFieldsSize,
		&Value,
		1u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteString(&Writer, Value) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取只含一个 SSH string 的 request。 */
static xsshcode xsshRequestOneStringRead(
	const xsshchannelrequest* pRequest,
	const char* pType,
	size_t iTypeSize,
	int iWantReply,
	xbytesview* pValue
)
{
	xsshreader Reader;
	xbytesview Value;
	xsshcode Code;

	Code = xsshRequestReadBegin(
		pRequest,
		pType,
		iTypeSize,
		iWantReply,
		pValue,
		sizeof(*pValue),
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pValue = Value;
	return XSSH_OK;
}



/* 校验信号名称并拒绝 RFC 明确省略的 SIG 前缀。 */
bool xrtSshChannelSignalValid(xstrview Signal)
{
	return xrtSshNameValid(Signal) &&
		!((Signal.Size >= 3u) &&
		  (memcmp(Signal.Data, "SIG", 3u) == 0));
}



/* 写入无专用字段的 shell request。 */
xsshcode xrtSshChannelShellWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply
)
{
	xsshwriter Writer;
	xsshcode Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_SHELL),
		bWantReply,
		0u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取无专用字段的 shell request。 */
xsshcode xrtSshChannelShellRead(const xsshchannelrequest* pRequest)
{
	xsshreader Reader;
	xsshcode Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_SHELL,
		sizeof(XSSH_CHANNEL_REQUEST_SHELL) - 1u,
		-1,
		NULL,
		0u,
		&Reader
	);

	return Code == XSSH_OK ? xsshRequestReadEnd(&Reader) : Code;
}



/* 写入不限定编码的 exec command。 */
xsshcode xrtSshChannelExecWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Command
)
{
	return xsshRequestOneStringWrite(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_EXEC),
		bWantReply,
		Command
	);
}



/* 严格读取不限定编码的 exec command。 */
xsshcode xrtSshChannelExecRead(
	const xsshchannelrequest* pRequest,
	xbytesview* pCommand
)
{
	return xsshRequestOneStringRead(
		pRequest,
		XSSH_CHANNEL_REQUEST_EXEC,
		sizeof(XSSH_CHANNEL_REQUEST_EXEC) - 1u,
		-1,
		pCommand
	);
}



/* 写入不限定编码的 subsystem 名称。 */
xsshcode xrtSshChannelSubsystemWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Subsystem
)
{
	return xsshRequestOneStringWrite(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_SUBSYSTEM),
		bWantReply,
		Subsystem
	);
}



/* 严格读取不限定编码的 subsystem 名称。 */
xsshcode xrtSshChannelSubsystemRead(
	const xsshchannelrequest* pRequest,
	xbytesview* pSubsystem
)
{
	return xsshRequestOneStringRead(
		pRequest,
		XSSH_CHANNEL_REQUEST_SUBSYSTEM,
		sizeof(XSSH_CHANNEL_REQUEST_SUBSYSTEM) - 1u,
		-1,
		pSubsystem
	);
}



/* 写入 env 名称和值。 */
xsshcode xrtSshChannelEnvWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Name,
	xbytesview Value
)
{
	xbytesview arrInputs[2] = { Name, Value };
	xsshwriter Writer;
	size_t iFieldsSize = 0u;
	xsshcode Code;

	if ( ((Code = xsshRequestAddString(Name, &iFieldsSize)) != XSSH_OK) ||
		((Code = xsshRequestAddString(Value, &iFieldsSize)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_ENV),
		bWantReply,
		iFieldsSize,
		arrInputs,
		2u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, Name) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Value) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 env 名称和值。 */
xsshcode xrtSshChannelEnvRead(
	const xsshchannelrequest* pRequest,
	xsshchannelenv* pEnv
)
{
	xsshreader Reader;
	xsshchannelenv Env;
	xsshcode Code;

	Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_ENV,
		sizeof(XSSH_CHANNEL_REQUEST_ENV) - 1u,
		-1,
		pEnv,
		sizeof(*pEnv),
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadString(&Reader, &Env.Name)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Env.Value)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pEnv = Env;
	return XSSH_OK;
}



/* 写入不要求回复的 xon-xoff 通知。 */
xsshcode xrtSshChannelXonXoffWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bClientCanDo
)
{
	xsshwriter Writer;
	xsshcode Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_XON_XOFF),
		false,
		1u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteBool(&Writer, bClientCanDo) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取不要求回复的 xon-xoff 通知。 */
xsshcode xrtSshChannelXonXoffRead(
	const xsshchannelrequest* pRequest,
	bool* pClientCanDo
)
{
	xsshreader Reader;
	bool bClientCanDo;
	xsshcode Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_XON_XOFF,
		sizeof(XSSH_CHANNEL_REQUEST_XON_XOFF) - 1u,
		0,
		pClientCanDo,
		sizeof(*pClientCanDo),
		&Reader
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadBool(&Reader, &bClientCanDo);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pClientCanDo = bClientCanDo;
	return XSSH_OK;
}



/* 写入不要求回复的终端尺寸变更。 */
xsshcode xrtSshChannelWindowChangeWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight
)
{
	xsshwriter Writer;
	xsshcode Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_WINDOW_CHANGE),
		false,
		16u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteU32(&Writer, iColumns) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iRows) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPixelWidth) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPixelHeight) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取不要求回复的终端尺寸变更。 */
xsshcode xrtSshChannelWindowChangeRead(
	const xsshchannelrequest* pRequest,
	xsshchannelwindowchange* pChange
)
{
	xsshreader Reader;
	xsshchannelwindowchange Change;
	xsshcode Code;

	Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_WINDOW_CHANGE,
		sizeof(XSSH_CHANNEL_REQUEST_WINDOW_CHANGE) - 1u,
		0,
		pChange,
		sizeof(*pChange),
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Change.Columns)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Change.Rows)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Change.PixelWidth)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Change.PixelHeight)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pChange = Change;
	return XSSH_OK;
}



/* 写入不要求回复的信号通知。 */
xsshcode xrtSshChannelSignalWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Signal
)
{
	if ( !xrtSshChannelSignalValid(Signal) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xsshRequestOneStringWrite(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_SIGNAL),
		false,
		xsshRequestBytes(Signal)
	);
}



/* 严格读取不要求回复的信号通知。 */
xsshcode xrtSshChannelSignalRead(
	const xsshchannelrequest* pRequest,
	xstrview* pSignal
)
{
	xbytesview Value;
	xstrview Signal;
	xsshcode Code;

	if ( (pSignal == NULL) || (pRequest == NULL) ||
		xrtMemRangesOverlap(
			pRequest->Fields.Data,
			pRequest->Fields.Size,
			pSignal,
			sizeof(*pSignal)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshRequestOneStringRead(
		pRequest,
		XSSH_CHANNEL_REQUEST_SIGNAL,
		sizeof(XSSH_CHANNEL_REQUEST_SIGNAL) - 1u,
		0,
		&Value
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Signal = xsshRequestText(Value);
	if ( !xrtSshChannelSignalValid(Signal) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pSignal = Signal;
	return XSSH_OK;
}



/* 写入 RFC 4335 break request。 */
xsshcode xrtSshChannelBreakWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	uint32 iLengthMs
)
{
	xsshwriter Writer;
	xsshcode Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_BREAK),
		bWantReply,
		4u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteU32(&Writer, iLengthMs) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 RFC 4335 break request。 */
xsshcode xrtSshChannelBreakRead(
	const xsshchannelrequest* pRequest,
	uint32* pLengthMs
)
{
	xsshreader Reader;
	uint32 iLengthMs;
	xsshcode Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_BREAK,
		sizeof(XSSH_CHANNEL_REQUEST_BREAK) - 1u,
		-1,
		pLengthMs,
		sizeof(*pLengthMs),
		&Reader
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iLengthMs);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pLengthMs = iLengthMs;
	return XSSH_OK;
}



/* 写入不要求回复的进程退出状态。 */
xsshcode xrtSshChannelExitStatusWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iStatus
)
{
	xsshwriter Writer;
	xsshcode Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_EXIT_STATUS),
		false,
		4u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteU32(&Writer, iStatus) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取不要求回复的进程退出状态。 */
xsshcode xrtSshChannelExitStatusRead(
	const xsshchannelrequest* pRequest,
	uint32* pStatus
)
{
	xsshreader Reader;
	uint32 iStatus;
	xsshcode Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_EXIT_STATUS,
		sizeof(XSSH_CHANNEL_REQUEST_EXIT_STATUS) - 1u,
		0,
		pStatus,
		sizeof(*pStatus),
		&Reader
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iStatus);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pStatus = iStatus;
	return XSSH_OK;
}



/* 写入不要求回复的进程退出信号。 */
xsshcode xrtSshChannelExitSignalWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Signal,
	bool bCoreDumped,
	xstrview Message,
	xstrview Language
)
{
	xbytesview arrInputs[3] = {
		xsshRequestBytes(Signal),
		xsshRequestBytes(Message),
		xsshRequestBytes(Language)
	};
	xsshwriter Writer;
	size_t iFieldsSize = 1u;
	xsshcode Code;

	if ( !xrtSshChannelSignalValid(Signal) ||
		!xrtUtf8Valid(Message, NULL) ||
		!xrtSshLanguageValid(Language) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshRequestAddString(arrInputs[0], &iFieldsSize)) !=
		XSSH_OK) || ((Code = xsshRequestAddString(
		arrInputs[1],
		&iFieldsSize
	)) != XSSH_OK) || ((Code = xsshRequestAddString(
		arrInputs[2],
		&iFieldsSize
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_EXIT_SIGNAL),
		false,
		iFieldsSize,
		arrInputs,
		3u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteBool(&Writer, bCoreDumped) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[2]) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取不要求回复的进程退出信号。 */
xsshcode xrtSshChannelExitSignalRead(
	const xsshchannelrequest* pRequest,
	xsshchannelexitsignal* pSignal
)
{
	xsshreader Reader;
	xsshchannelexitsignal Signal;
	xbytesview Value;
	xsshcode Code;

	Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_EXIT_SIGNAL,
		sizeof(XSSH_CHANNEL_REQUEST_EXIT_SIGNAL) - 1u,
		0,
		pSignal,
		sizeof(*pSignal),
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Signal.Signal = xsshRequestText(Value);
	if ( ((Code = xrtSshReadBool(&Reader, &Signal.CoreDumped)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Value)) != XSSH_OK) ) {
		return Code;
	}
	Signal.Message = xsshRequestText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Signal.Language = xsshRequestText(Value);
	if ( !xrtSshChannelSignalValid(Signal.Signal) ||
		!xrtUtf8Valid(Signal.Message, NULL) ||
		!xrtSshLanguageValid(Signal.Language) ||
		(xsshRequestReadEnd(&Reader) != XSSH_OK) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pSignal = Signal;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_channel_pty.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CHANNEL_PTY)




#if defined(XSSH_FEATURE_CHANNEL_PTY)

/* 写入一个有参数的 terminal mode。 */
xsshcode xrtSshTerminalModeWrite(
	xsshwriter* pWriter,
	uint8 iOpcode,
	uint32 iValue
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( (iOpcode == XSSH_TTY_OP_END) ||
		(iOpcode >= XSSH_TTY_OP_UNSUPPORTED_MIN) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshWriterReserveInputs(pWriter, 5u, NULL, 0u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(&Writer, iOpcode) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iValue) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 写入 terminal mode 结束标记。 */
xsshcode xrtSshTerminalModeEnd(xsshwriter* pWriter)
{
	xsshwriter Writer;
	xsshcode Code = xrtSshWriterReserveInputs(pWriter, 1u, NULL, 0u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( xrtSshWriteByte(&Writer, XSSH_TTY_OP_END) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 完整预验证 mode stream，未知新 opcode 按 RFC 停止解析。 */
xsshcode xrtSshTerminalModesRead(
	xbytesview Modes,
	xsshterminalmodes* pModes
)
{
	xsshterminalmodes Result;
	xsshreader Reader;
	uint32 iValue;
	uint8 iOpcode;
	xsshcode Code;

	if ( (pModes == NULL) || !xrtMemRangeValid(Modes.Data, Modes.Size) ||
		xrtMemRangesOverlap(
			Modes.Data,
			Modes.Size,
			pModes,
			sizeof(*pModes)
		) || !xrtSshReaderInit(&Reader, Modes) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Result.Reader = Reader;
	Result.Count = 0u;
	Result.Index = 0u;
	Result.Unsupported = false;
	if ( Modes.Size == 0u ) {
		*pModes = Result;
		return XSSH_OK;
	}
	while ( xrtSshReaderRemaining(&Reader) != 0u ) {
		Code = xrtSshReadByte(&Reader, &iOpcode);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( iOpcode == XSSH_TTY_OP_END ) {
			if ( xrtSshReaderRemaining(&Reader) != 0u ) {
				return XSSH_ERROR_PROTOCOL;
			}
			*pModes = Result;
			return XSSH_OK;
		}
		if ( iOpcode >= XSSH_TTY_OP_UNSUPPORTED_MIN ) {
			Result.Unsupported = true;
			*pModes = Result;
			return XSSH_OK;
		}
		Code = xrtSshReadU32(&Reader, &iValue);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		(void)iValue;
		++Result.Count;
	}
	return XSSH_ERROR_PROTOCOL;
}



/* 迭代一项已经完整验证的 terminal mode。 */
bool xrtSshTerminalModesNext(
	xsshterminalmodes* pModes,
	xsshterminalmode* pMode
)
{
	xsshterminalmodes Modes;
	xsshterminalmode Mode;

	if ( (pModes == NULL) || (pMode == NULL) ||
		(pModes->Index >= pModes->Count) ) {
		return false;
	}
	Modes = *pModes;
	if ( (xrtSshReadByte(&Modes.Reader, &Mode.Opcode) != XSSH_OK) ||
		(xrtSshReadU32(&Modes.Reader, &Mode.Value) != XSSH_OK) ) {
		return false;
	}
	++Modes.Index;
	*pModes = Modes;
	*pMode = Mode;
	return true;
}



/* 写入不复制 mode stream 的 PTY request。 */
xsshcode xrtSshChannelPtyWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Terminal,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight,
	xbytesview Modes
)
{
	xbytesview arrInputs[2] = { Terminal, Modes };
	xsshterminalmodes Parsed;
	xsshwriter Writer;
	size_t iFieldsSize = 16u;
	xsshcode Code;

	Code = xrtSshTerminalModesRead(Modes, &Parsed);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_PROTOCOL ? XSSH_ERROR_ARGUMENT : Code;
	}
	if ( ((Code = xsshRequestAddString(Terminal, &iFieldsSize)) != XSSH_OK) ||
		((Code = xsshRequestAddString(Modes, &iFieldsSize)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_PTY),
		bWantReply,
		iFieldsSize,
		arrInputs,
		2u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, Terminal) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iColumns) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iRows) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPixelWidth) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPixelHeight) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Modes) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 PTY request 并验证 mode stream。 */
xsshcode xrtSshChannelPtyRead(
	const xsshchannelrequest* pRequest,
	xsshchannelpty* pPty
)
{
	xsshterminalmodes Modes;
	xsshchannelpty Pty;
	xsshreader Reader;
	xsshcode Code;

	Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_PTY,
		sizeof(XSSH_CHANNEL_REQUEST_PTY) - 1u,
		-1,
		pPty,
		sizeof(*pPty),
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadString(&Reader, &Pty.Terminal)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Pty.Columns)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Pty.Rows)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Pty.PixelWidth)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Pty.PixelHeight)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Pty.Modes)) != XSSH_OK) ) {
		return Code;
	}
	if ( xsshRequestReadEnd(&Reader) != XSSH_OK ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshTerminalModesRead(Pty.Modes, &Modes);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? Code : XSSH_ERROR_PROTOCOL;
	}
	*pPty = Pty;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_channel_state.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CHANNEL_STATE)



#if defined(XSSH_FEATURE_CHANNEL_STATE)

/* 判断生命周期对象已经初始化。 */
static bool xsshChannelStateValid(const xsshchannelstate* pState)
{
	return (pState != NULL) && pState->Initialized;
}



/* 初始化打开状态。 */
bool xrtSshChannelStateInit(xsshchannelstate* pState)
{
	xsshchannelstate State;

	if ( pState == NULL ) {
		return false;
	}
	State.LocalEof = false;
	State.RemoteEof = false;
	State.LocalClose = false;
	State.RemoteClose = false;
	State.Initialized = true;
	*pState = State;
	return true;
}



/* EOF 只停止本方向 data；任一 close 都停止新增发送。 */
bool xrtSshChannelCanSendData(const xsshchannelstate* pState)
{
	return xsshChannelStateValid(pState) && !pState->LocalEof &&
		!pState->LocalClose && !pState->RemoteClose;
}



/* 本端 close 后仍允许处理此前在途数据，直到远端 EOF/CLOSE。 */
bool xrtSshChannelCanReceiveData(const xsshchannelstate* pState)
{
	return xsshChannelStateValid(pState) && !pState->RemoteEof &&
		!pState->RemoteClose;
}



/* EOF 不禁止控制 request，close 才终止新的 request。 */
bool xrtSshChannelCanSendRequest(const xsshchannelstate* pState)
{
	return xsshChannelStateValid(pState) && !pState->LocalClose &&
		!pState->RemoteClose;
}



/* 远端先 close 时必须补发一次本端 close。 */
bool xrtSshChannelCloseReplyNeeded(const xsshchannelstate* pState)
{
	return xsshChannelStateValid(pState) && pState->RemoteClose &&
		!pState->LocalClose;
}



/* 只有双向 close 都已提交才能回收 channel。 */
bool xrtSshChannelClosed(const xsshchannelstate* pState)
{
	return xsshChannelStateValid(pState) && pState->LocalClose &&
		pState->RemoteClose;
}



/* 提交本端 EOF。 */
xsshcode xrtSshChannelLocalEofCommit(xsshchannelstate* pState)
{
	if ( !xsshChannelStateValid(pState) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pState->LocalEof || pState->LocalClose || pState->RemoteClose ) {
		return XSSH_ERROR_STATE;
	}
	pState->LocalEof = true;
	return XSSH_OK;
}



/* 提交远端 EOF。 */
xsshcode xrtSshChannelRemoteEofCommit(xsshchannelstate* pState)
{
	if ( !xsshChannelStateValid(pState) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pState->RemoteEof || pState->RemoteClose ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pState->RemoteEof = true;
	return XSSH_OK;
}



/* 提交本端 close；close 隐含本方向不再发送任何数据。 */
xsshcode xrtSshChannelLocalCloseCommit(xsshchannelstate* pState)
{
	if ( !xsshChannelStateValid(pState) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pState->LocalClose ) {
		return XSSH_ERROR_STATE;
	}
	pState->LocalClose = true;
	return XSSH_OK;
}



/* 提交远端 close；重复 close 是线路协议错误。 */
xsshcode xrtSshChannelRemoteCloseCommit(xsshchannelstate* pState)
{
	if ( !xsshChannelStateValid(pState) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pState->RemoteClose ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pState->RemoteClose = true;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_channel_core.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CHANNEL_CORE)
#include <string.h>




#if defined(XSSH_FEATURE_CHANNEL_CORE)

/* 校验公开状态没有被未初始化对象或越界阶段伪造。 */
static bool xsshChannelCoreValid(const xsshchannelcore* pChannel)
{
	return xrtMemRangeValid(pChannel, sizeof(*pChannel)) &&
		pChannel->Initialized &&
		(pChannel->Phase >= XSSH_CHANNEL_CORE_OPENING) &&
		(pChannel->Phase <= XSSH_CHANNEL_CORE_CLOSED);
}



/* 打开数据面的阶段必须同时具有有效生命周期状态。 */
static bool xsshChannelCoreDataValid(const xsshchannelcore* pChannel)
{
	return xsshChannelCoreValid(pChannel) &&
		((pChannel->Phase == XSSH_CHANNEL_CORE_OPEN) ||
		 (pChannel->Phase == XSSH_CHANNEL_CORE_CLOSED)) &&
		pChannel->State.Initialized;
}



/* 判断输入结构与 channel 对象互不覆盖。 */
static bool xsshChannelCoreInputValid(
	const xsshchannelcore* pChannel,
	const void* pInput,
	size_t iInputSize
)
{
	return xrtMemRangeValid(pInput, iInputSize) &&
		!xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pInput,
			iInputSize
		);
}



/* 双向 close 完成后冻结协议状态，但保留窗口中的应用缓冲计数。 */
static void xsshChannelCoreCloseUpdate(xsshchannelcore* pChannel)
{
	if ( xrtSshChannelClosed(&pChannel->State) ) {
		pChannel->Phase = XSSH_CHANNEL_CORE_CLOSED;
	}
}



/* 初始化等待 confirmation 的本端 open。 */
bool xrtSshChannelCoreOpenInit(
	xsshchannelcore* pChannel,
	uint32 iLocal,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
)
{
	xsshchannelcore Channel;

	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ) {
		return false;
	}
	memset(&Channel, 0, sizeof(Channel));
	if ( !xrtSshChannelWindowInit(
		&Channel.Window,
		0u,
		1u,
		iReceiveWindow,
		iReceiveMaxPacket,
		iAdjustThreshold
	) ) {
		return false;
	}
	Channel.Local = iLocal;
	Channel.Phase = XSSH_CHANNEL_CORE_OPENING;
	Channel.Initialized = true;
	*pChannel = Channel;
	return true;
}



/* 初始化等待本端确认的 peer open。 */
bool xrtSshChannelCoreAcceptInit(
	xsshchannelcore* pChannel,
	uint32 iLocal,
	const xsshchannelopen* pOpen,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
)
{
	xsshchannelcore Channel;

	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!xrtMemRangeValid(pOpen, sizeof(*pOpen)) ||
		xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pOpen,
			sizeof(*pOpen)
		) || (pOpen->MaxPacket == 0u) ) {
		return false;
	}
	memset(&Channel, 0, sizeof(Channel));
	if ( !xrtSshChannelWindowInit(
		&Channel.Window,
		pOpen->Window,
		pOpen->MaxPacket,
		iReceiveWindow,
		iReceiveMaxPacket,
		iAdjustThreshold
	) ) {
		return false;
	}
	Channel.Local = iLocal;
	Channel.Remote = pOpen->Sender;
	Channel.Phase = XSSH_CHANNEL_CORE_ACCEPTING;
	Channel.Initialized = true;
	*pChannel = Channel;
	return true;
}



/* 清除不拥有外部资源的 channel core。 */
void xrtSshChannelCoreClear(xsshchannelcore* pChannel)
{
	if ( xrtMemRangeValid(pChannel, sizeof(*pChannel)) ) {
		memset(pChannel, 0, sizeof(*pChannel));
	}
}



/* 查询公开阶段。 */
xsshchannelcorephase xrtSshChannelCorePhase(
	const xsshchannelcore* pChannel
)
{
	return xsshChannelCoreValid(pChannel) ?
		pChannel->Phase : XSSH_CHANNEL_CORE_FAILED;
}



/* 复制 channel 两个方向的线路编号。 */
bool xrtSshChannelCoreIds(
	const xsshchannelcore* pChannel,
	uint32* pLocal,
	uint32* pRemote
)
{
	if ( !xsshChannelCoreValid(pChannel) ||
		((pChannel->Phase == XSSH_CHANNEL_CORE_OPENING) ||
		 (pChannel->Phase == XSSH_CHANNEL_CORE_FAILED)) ||
		!xrtMemRangeValid(pLocal, sizeof(*pLocal)) ||
		!xrtMemRangeValid(pRemote, sizeof(*pRemote)) ||
		xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pLocal,
			sizeof(*pLocal)
		) || xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pRemote,
			sizeof(*pRemote)
		) || xrtMemRangesOverlap(
			pLocal,
			sizeof(*pLocal),
			pRemote,
			sizeof(*pRemote)
		) ) {
		return false;
	}
	*pLocal = pChannel->Local;
	*pRemote = pChannel->Remote;
	return true;
}



/* 原子应用 peer confirmation。 */
xsshcode xrtSshChannelCoreConfirmationCommit(
	xsshchannelcore* pChannel,
	const xsshchannelconfirmation* pConfirmation
)
{
	xsshchannelcore Channel;

	if ( !xsshChannelCoreValid(pChannel) ||
		(pChannel->Phase != XSSH_CHANNEL_CORE_OPENING) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xsshChannelCoreInputValid(
		pChannel,
		pConfirmation,
		sizeof(*pConfirmation)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pConfirmation->Recipient != pChannel->Local) ||
		(pConfirmation->MaxPacket == 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Channel = *pChannel;
	if ( !xrtSshChannelWindowInit(
		&Channel.Window,
		pConfirmation->Window,
		pConfirmation->MaxPacket,
		pChannel->Window.ReceiveWindow,
		pChannel->Window.ReceiveMaxPacket,
		pChannel->Window.AdjustThreshold
	) || !xrtSshChannelStateInit(&Channel.State) ) {
		return XSSH_ERROR_STATE;
	}
	Channel.Remote = pConfirmation->Sender;
	Channel.Phase = XSSH_CHANNEL_CORE_OPEN;
	*pChannel = Channel;
	return XSSH_OK;
}



/* 原子应用 peer open failure。 */
xsshcode xrtSshChannelCoreFailureCommit(
	xsshchannelcore* pChannel,
	const xsshchannelopenfailure* pFailure
)
{
	if ( !xsshChannelCoreValid(pChannel) ||
		(pChannel->Phase != XSSH_CHANNEL_CORE_OPENING) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xsshChannelCoreInputValid(
		pChannel,
		pFailure,
		sizeof(*pFailure)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pFailure->Recipient != pChannel->Local ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pChannel->FailureReason = pFailure->Reason;
	pChannel->Phase = XSSH_CHANNEL_CORE_FAILED;
	return XSSH_OK;
}



/* 本端 confirmation 可靠提交后开放输入 channel。 */
xsshcode xrtSshChannelCoreAcceptCommit(xsshchannelcore* pChannel)
{
	if ( !xsshChannelCoreValid(pChannel) ||
		(pChannel->Phase != XSSH_CHANNEL_CORE_ACCEPTING) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtSshChannelStateInit(&pChannel->State) ) {
		return XSSH_ERROR_STATE;
	}
	pChannel->Phase = XSSH_CHANNEL_CORE_OPEN;
	return XSSH_OK;
}



/* 本端 failure 可靠提交后冻结拒绝结果。 */
xsshcode xrtSshChannelCoreRejectCommit(
	xsshchannelcore* pChannel,
	uint32 iReason
)
{
	if ( !xsshChannelCoreValid(pChannel) ||
		(pChannel->Phase != XSSH_CHANNEL_CORE_ACCEPTING) ) {
		return XSSH_ERROR_STATE;
	}
	pChannel->FailureReason = iReason;
	pChannel->Phase = XSSH_CHANNEL_CORE_FAILED;
	return XSSH_OK;
}



/* 判断 channel 数据面已开放。 */
bool xrtSshChannelCoreOpen(const xsshchannelcore* pChannel)
{
	return xsshChannelCoreDataValid(pChannel) &&
		(pChannel->Phase == XSSH_CHANNEL_CORE_OPEN);
}



/* 判断双向 close 已完成。 */
bool xrtSshChannelCoreClosed(const xsshchannelcore* pChannel)
{
	return xsshChannelCoreDataValid(pChannel) &&
		(pChannel->Phase == XSSH_CHANNEL_CORE_CLOSED) &&
		xrtSshChannelClosed(&pChannel->State);
}



/* 查询当前数据发送上限。 */
uint32 xrtSshChannelCoreSendLimit(const xsshchannelcore* pChannel)
{
	if ( !xrtSshChannelCoreOpen(pChannel) ||
		!xrtSshChannelCanSendData(&pChannel->State) ) {
		return 0u;
	}
	return xrtSshChannelSendLimit(&pChannel->Window);
}



/* 提交本端 data 的窗口消费。 */
xsshcode xrtSshChannelCoreDataSendCommit(
	xsshchannelcore* pChannel,
	uint32 iBytes
)
{
	if ( !xrtSshChannelCoreOpen(pChannel) ||
		!xrtSshChannelCanSendData(&pChannel->State) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshChannelSendCommit(&pChannel->Window, iBytes);
}



/* 提交 peer data 的生命周期与窗口消费。 */
xsshcode xrtSshChannelCoreDataReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient,
	uint32 iBytes
)
{
	xsshcode Code = xrtSshChannelCoreRecipientCheck(
		pChannel,
		iRecipient
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshChannelCanReceiveData(&pChannel->State) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return xrtSshChannelReceiveCommit(&pChannel->Window, iBytes);
}



/* 消费调用方已经处理的数据。 */
xsshcode xrtSshChannelCoreDataConsume(
	xsshchannelcore* pChannel,
	uint32 iBytes
)
{
	if ( !xsshChannelCoreDataValid(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshChannelReceiveConsume(&pChannel->Window, iBytes);
}



/* 判断仍可发送窗口更新且返还额度达到阈值。 */
bool xrtSshChannelCoreAdjustReady(const xsshchannelcore* pChannel)
{
	return xrtSshChannelCoreCanSendRequest(pChannel) &&
		xrtSshChannelReceiveAdjustReady(&pChannel->Window);
}



/* 返回当前可返还额度。 */
uint32 xrtSshChannelCoreAdjustLimit(const xsshchannelcore* pChannel)
{
	return xrtSshChannelCoreCanSendRequest(pChannel) ?
		xrtSshChannelReceiveAdjustLimit(&pChannel->Window) : 0u;
}



/* 提交本端已可靠发送的窗口更新。 */
xsshcode xrtSshChannelCoreAdjustSendCommit(
	xsshchannelcore* pChannel,
	uint32 iBytes
)
{
	if ( !xrtSshChannelCoreCanSendRequest(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshChannelReceiveAdjustCommit(&pChannel->Window, iBytes);
}



/* 提交 peer 窗口更新。 */
xsshcode xrtSshChannelCoreAdjustReceiveCommit(
	xsshchannelcore* pChannel,
	const xsshchanneladjust* pAdjust
)
{
	xsshcode Code;

	if ( !xsshChannelCoreDataValid(pChannel) ||
		!xsshChannelCoreInputValid(
			pChannel,
			pAdjust,
			sizeof(*pAdjust)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshChannelCoreRecipientCheck(
		pChannel,
		pAdjust->Recipient
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( pChannel->State.LocalClose || pChannel->State.RemoteClose ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return xrtSshChannelSendAdjust(&pChannel->Window, pAdjust->Bytes);
}



/* 判断本端仍可发送 request。 */
bool xrtSshChannelCoreCanSendRequest(
	const xsshchannelcore* pChannel
)
{
	return xrtSshChannelCoreOpen(pChannel) &&
		xrtSshChannelCanSendRequest(&pChannel->State);
}



/* 判断 peer 方向仍可到达 request。 */
bool xrtSshChannelCoreCanReceiveRequest(
	const xsshchannelcore* pChannel
)
{
	return xrtSshChannelCoreOpen(pChannel) &&
		!pChannel->State.RemoteClose;
}



/* 校验 recipient 指向当前已打开 channel。 */
xsshcode xrtSshChannelCoreRecipientCheck(
	const xsshchannelcore* pChannel,
	uint32 iRecipient
)
{
	if ( !xrtSshChannelCoreOpen(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	return iRecipient == pChannel->Local ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 提交本端 EOF。 */
xsshcode xrtSshChannelCoreEofSendCommit(xsshchannelcore* pChannel)
{
	if ( !xrtSshChannelCoreOpen(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshChannelLocalEofCommit(&pChannel->State);
}



/* 提交 peer EOF。 */
xsshcode xrtSshChannelCoreEofReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient
)
{
	xsshcode Code = xrtSshChannelCoreRecipientCheck(
		pChannel,
		iRecipient
	);

	return Code == XSSH_OK ?
		xrtSshChannelRemoteEofCommit(&pChannel->State) : Code;
}



/* 提交本端 CLOSE，并在握手完成时冻结 channel。 */
xsshcode xrtSshChannelCoreCloseSendCommit(xsshchannelcore* pChannel)
{
	xsshcode Code;

	if ( !xrtSshChannelCoreOpen(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshChannelLocalCloseCommit(&pChannel->State);
	if ( Code == XSSH_OK ) {
		xsshChannelCoreCloseUpdate(pChannel);
	}
	return Code;
}



/* 提交 peer CLOSE，并在握手完成时冻结 channel。 */
xsshcode xrtSshChannelCoreCloseReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient
)
{
	xsshcode Code = xrtSshChannelCoreRecipientCheck(
		pChannel,
		iRecipient
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshChannelRemoteCloseCommit(&pChannel->State);
	if ( Code == XSSH_OK ) {
		xsshChannelCoreCloseUpdate(pChannel);
	}
	return Code;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_channel_io.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CHANNEL_IO)
#include <string.h>




#if defined(XSSH_FEATURE_CHANNEL_IO)

#define XSSH_CHANNEL_IO_GUARD UINT32_C(0x4348494f)



/* 校验公开流类型。 */
static bool xsshChannelIoStreamValid(xsshchanneliostream Stream)
{
	return (Stream == XSSH_CHANNEL_IO_DATA) ||
		(Stream == XSSH_CHANNEL_IO_STDERR);
}



/* 返回指定接收流的内部缓冲。 */
static xnetbuf* xsshChannelIoReceiveBuffer(
	xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	return Stream == XSSH_CHANNEL_IO_DATA ?
		&pIo->ReceiveData : &pIo->ReceiveError;
}



/* 返回指定发送流的内部缓冲。 */
static xnetbuf* xsshChannelIoSendBuffer(
	xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	return Stream == XSSH_CHANNEL_IO_DATA ?
		&pIo->SendData : &pIo->SendError;
}



/* 安全相加两条缓冲长度。 */
static bool xsshChannelIoPairSize(
	const xnetbuf* pFirst,
	const xnetbuf* pSecond,
	size_t* pSize
)
{
	size_t iFirst = xrtNetBufSize(pFirst);
	size_t iSecond = xrtNetBufSize(pSecond);

	if ( iFirst > (SIZE_MAX - iSecond) ) {
		return false;
	}
	*pSize = iFirst + iSecond;
	return true;
}



/* 校验对象固定字段和内部缓冲没有活动写预留。 */
static bool xsshChannelIoValid(const xsshchannelio* pIo)
{
	return xrtMemRangeValid(pIo, sizeof(*pIo)) &&
		(pIo->Guard == XSSH_CHANNEL_IO_GUARD) && pIo->Initialized &&
		xrtMemRangeValid(pIo->Channel, sizeof(*pIo->Channel)) &&
		pIo->Channel->Initialized && !xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pIo->Channel,
			sizeof(*pIo->Channel)
		) && (pIo->ReceiveLimit <= UINT32_MAX) &&
		(pIo->Pending >= XSSH_CHANNEL_IO_PENDING_NONE) &&
		(pIo->Pending <= XSSH_CHANNEL_IO_PENDING_SEND) &&
		xsshChannelIoStreamValid(pIo->PendingStream) &&
		(pIo->ReceiveData.Reserved == NULL) &&
		(pIo->ReceiveError.Reserved == NULL) &&
		(pIo->SendData.Reserved == NULL) &&
		(pIo->SendError.Reserved == NULL) &&
		(pIo->ReceiveStaging.Reserved == NULL);
}



/* 校验没有在途事务，且动态接收量与 channel 窗口计数一致。 */
static bool xsshChannelIoStable(const xsshchannelio* pIo)
{
	size_t iReadable;

	return xsshChannelIoValid(pIo) &&
		(pIo->Pending == XSSH_CHANNEL_IO_PENDING_NONE) &&
		xsshChannelIoPairSize(
			&pIo->ReceiveData,
			&pIo->ReceiveError,
			&iReadable
		) && ((uint64)iReadable == pIo->Channel->Window.ReceiveBuffered);
}



/* 判断一段外部数据不会覆盖 I/O 对象或绑定 channel。 */
static bool xsshChannelIoDataValid(
	const xsshchannelio* pIo,
	const void* pData,
	size_t iSize
)
{
	return xrtMemRangeValid(pData, iSize) && !xrtMemRangesOverlap(
		pIo,
		sizeof(*pIo),
		pData,
		iSize
	) && !xrtMemRangesOverlap(
		pIo->Channel,
		sizeof(*pIo->Channel),
		pData,
		iSize
	);
}



/* 清除短事务快照，不触碰任何动态块。 */
static void xsshChannelIoPendingClear(xsshchannelio* pIo)
{
	memset(&pIo->ChannelBefore, 0, sizeof(pIo->ChannelBefore));
	memset(&pIo->ChannelAfter, 0, sizeof(pIo->ChannelAfter));
	pIo->SendHead = NULL;
	pIo->PendingBytes = 0u;
	pIo->PendingStream = XSSH_CHANNEL_IO_DATA;
	pIo->Pending = XSSH_CHANNEL_IO_PENDING_NONE;
}



/* 返回共享发送预算，结构损坏时返回零。 */
static size_t xsshChannelIoWriteAvailable(const xsshchannelio* pIo)
{
	size_t iQueued;

	if ( !xsshChannelIoPairSize(
		&pIo->SendData,
		&pIo->SendError,
		&iQueued
	) || (iQueued >= pIo->SendLimit) ) {
		return 0u;
	}
	return pIo->SendLimit - iQueued;
}



/* 校验一次发送追加，并返回目标缓冲。 */
static xsshcode xsshChannelIoWriteCheck(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize,
	xnetbuf** ppBuffer
)
{
	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xsshChannelIoDataValid(pIo, pData, iSize) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iSize > xsshChannelIoWriteAvailable(pIo) ) {
		return XSSH_ERROR_SPACE;
	}
	*ppBuffer = xsshChannelIoSendBuffer(pIo, Stream);
	return XSSH_OK;
}



/* 写入默认动态缓冲预算。 */
void xrtSshChannelIoConfigInit(xsshchannelioconfig* pConfig)
{
	if ( xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		pConfig->ReceiveLimit = XSSH_CHANNEL_IO_LIMIT_DEFAULT;
		pConfig->SendLimit = XSSH_CHANNEL_IO_LIMIT_DEFAULT;
	}
}



/* 初始化绑定单个 channel 的动态收发缓冲。 */
bool xrtSshChannelIoInit(
	xsshchannelio* pIo,
	xnetbufpool* pPool,
	xsshchannelcore* pChannel,
	const xsshchannelioconfig* pConfig
)
{
	xsshchannelioconfig Config;
	xsshchannelio Io;

	if ( !xrtMemRangeValid(pIo, sizeof(*pIo)) ||
		!xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!pChannel->Initialized || xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pChannel,
			sizeof(*pChannel)
		) || (pChannel->Window.ReceiveBuffered != 0u) ||
		(pChannel->Window.ReceivePending != 0u) ) {
		return false;
	}
	xrtSshChannelIoConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
			xrtMemRangesOverlap(
				pIo,
				sizeof(*pIo),
				pConfig,
				sizeof(*pConfig)
			) || xrtMemRangesOverlap(
				pChannel,
				sizeof(*pChannel),
				pConfig,
				sizeof(*pConfig)
			) ) {
			return false;
		}
		Config = *pConfig;
	}
	if ( (Config.ReceiveLimit > UINT32_MAX) ||
		(Config.ReceiveLimit < pChannel->Window.ReceiveWindow) ) {
		return false;
	}
	memset(&Io, 0, sizeof(Io));
	if ( !xrtNetBufInit(&Io.ReceiveData, pPool) ||
		!xrtNetBufInit(&Io.ReceiveError, pPool) ||
		!xrtNetBufInit(&Io.SendData, pPool) ||
		!xrtNetBufInit(&Io.SendError, pPool) ||
		!xrtNetBufInit(&Io.ReceiveStaging, pPool) ) {
		return false;
	}
	Io.Channel = pChannel;
	Io.ReceiveLimit = Config.ReceiveLimit;
	Io.SendLimit = Config.SendLimit;
	Io.PendingStream = XSSH_CHANNEL_IO_DATA;
	Io.Initialized = true;
	Io.Guard = XSSH_CHANNEL_IO_GUARD;
	*pIo = Io;
	return true;
}



/* 丢弃全部队列，并把已经计入窗口的数据视为已消费。 */
void xrtSshChannelIoClear(xsshchannelio* pIo)
{
	if ( xsshChannelIoValid(pIo) ) {
		size_t iReadable;
		size_t iStaged = xrtNetBufSize(&pIo->ReceiveStaging);

		if ( xsshChannelIoPairSize(
			&pIo->ReceiveData,
			&pIo->ReceiveError,
			&iReadable
		) ) {
			size_t iCounted = iReadable;

			if ( (pIo->Pending == XSSH_CHANNEL_IO_PENDING_RECEIVE) &&
				(iCounted <= (SIZE_MAX - iStaged)) &&
				((uint64)(iCounted + iStaged) ==
				 pIo->Channel->Window.ReceiveBuffered) ) {
				iCounted += iStaged;
			}
			if ( ((uint64)iCounted ==
				pIo->Channel->Window.ReceiveBuffered) &&
				(iCounted <= UINT32_MAX) ) {
				(void)xrtSshChannelCoreDataConsume(
					pIo->Channel,
					(uint32)iCounted
				);
			}
		}
		xrtNetBufClear(&pIo->ReceiveData);
		xrtNetBufClear(&pIo->ReceiveError);
		xrtNetBufClear(&pIo->SendData);
		xrtNetBufClear(&pIo->SendError);
		xrtNetBufClear(&pIo->ReceiveStaging);
	}
	if ( xrtMemRangeValid(pIo, sizeof(*pIo)) ) {
		memset(pIo, 0, sizeof(*pIo));
	}
}



/* 查询已可靠接收的数据量。 */
size_t xrtSshChannelIoReadable(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	if ( !xsshChannelIoValid(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return 0u;
	}
	return xrtNetBufSize(xsshChannelIoReceiveBuffer(
		(xsshchannelio*)pIo,
		Stream
	));
}



/* 借出只读接收缓冲。 */
const xnetbuf* xrtSshChannelIoReadBuffer(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	if ( !xsshChannelIoValid(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return NULL;
	}
	return xsshChannelIoReceiveBuffer((xsshchannelio*)pIo, Stream);
}



/* 原子消费接收前缀和对应窗口计数。 */
xsshcode xrtSshChannelIoConsume(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	size_t iSize
)
{
	xsshchannelcore Channel;
	xnetbuf* pBuffer;
	xsshcode Code;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	pBuffer = xsshChannelIoReceiveBuffer(pIo, Stream);
	if ( (iSize > UINT32_MAX) || (iSize > xrtNetBufSize(pBuffer)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Channel = *pIo->Channel;
	Code = xrtSshChannelCoreDataConsume(&Channel, (uint32)iSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtNetBufConsume(pBuffer, iSize) != iSize ) {
		return XSSH_ERROR_STATE;
	}
	*pIo->Channel = Channel;
	return XSSH_OK;
}



/* 复制并消费接收数据。 */
xsshcode xrtSshChannelIoRead(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	void* pOutput,
	size_t iCapacity,
	size_t* pRead
)
{
	xnetbuf* pBuffer;
	size_t iRead;
	xsshcode Code;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pOutput, iCapacity) ||
		!xrtMemRangeValid(pRead, sizeof(*pRead)) ||
		xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pOutput,
			iCapacity
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pOutput,
			iCapacity
		) || xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pRead,
			sizeof(*pRead)
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pRead,
			sizeof(*pRead)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	pBuffer = xsshChannelIoReceiveBuffer(pIo, Stream);
	iRead = xrtNetBufSize(pBuffer);
	if ( iRead > iCapacity ) {
		iRead = iCapacity;
	}
	if ( xrtNetBufPeek(pBuffer, 0u, pOutput, iRead) != iRead ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshChannelIoConsume(pIo, Stream, iRead);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pRead = iRead;
	return XSSH_OK;
}



/* 查询单条发送流排队量。 */
size_t xrtSshChannelIoQueued(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	if ( !xsshChannelIoValid(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return 0u;
	}
	return xrtNetBufSize(xsshChannelIoSendBuffer(
		(xsshchannelio*)pIo,
		Stream
	));
}



/* 查询共享发送预算。 */
size_t xrtSshChannelIoWritable(const xsshchannelio* pIo)
{
	return xsshChannelIoValid(pIo) ?
		xsshChannelIoWriteAvailable(pIo) : 0u;
}



/* 查询下一条 payload 可直接使用的连续队首。 */
size_t xrtSshChannelIoSendLimit(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	xnetspan Span;
	uint32 iLimit;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) || !xrtNetBufFront(
			xsshChannelIoSendBuffer((xsshchannelio*)pIo, Stream),
			&Span
		) ) {
		return 0u;
	}
	iLimit = xrtSshChannelCoreSendLimit(pIo->Channel);
	return Span.Size < (size_t)iLimit ? Span.Size : (size_t)iLimit;
}



/* 复制追加发送数据。 */
xsshcode xrtSshChannelIoWrite(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize
)
{
	xnetbuf* pBuffer;
	xsshcode Code = xsshChannelIoWriteCheck(
		pIo,
		Stream,
		pData,
		iSize,
		&pBuffer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtNetBufAppend(pBuffer, pData, iSize) ?
		XSSH_OK : XSSH_ERROR_SPACE;
}



/* 追加借用发送数据。 */
xsshcode xrtSshChannelIoWriteBorrow(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize
)
{
	xnetbuf* pBuffer;
	xsshcode Code = xsshChannelIoWriteCheck(
		pIo,
		Stream,
		pData,
		iSize,
		&pBuffer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iSize == 0u ) {
		return XSSH_OK;
	}
	return xrtNetBufAppendBorrow(pBuffer, pData, iSize) ?
		XSSH_OK : XSSH_ERROR_SPACE;
}



/* 接管 XRT 分配的发送数据。 */
xsshcode xrtSshChannelIoWriteTake(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	void* pData,
	size_t iSize
)
{
	xnetbuf* pBuffer;
	xsshcode Code = xsshChannelIoWriteCheck(
		pIo,
		Stream,
		pData,
		iSize,
		&pBuffer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iSize == 0u ) {
		return XSSH_OK;
	}
	return xrtNetBufAppendTake(pBuffer, pData, iSize) ?
		XSSH_OK : XSSH_ERROR_SPACE;
}



/* 接管自定义释放的发送数据。 */
xsshcode xrtSshChannelIoWriteRef(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
)
{
	xnetbuf* pBuffer;
	xsshcode Code;

	if ( (pRelease == NULL) && (iSize != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshChannelIoWriteCheck(
		pIo,
		Stream,
		pData,
		iSize,
		&pBuffer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iSize == 0u ) {
		return XSSH_OK;
	}
	return xrtNetBufAppendRef(
		pBuffer,
		pData,
		iSize,
		pRelease,
		pContext
	) ? XSSH_OK : XSSH_ERROR_SPACE;
}



/* 移动调用方发送缓冲。 */
xsshcode xrtSshChannelIoWriteBuffer(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	xnetbuf* pBuffer
)
{
	xnetbuf* pTarget;
	size_t iSize;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pBuffer, sizeof(*pBuffer)) ||
		xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pBuffer,
			sizeof(*pBuffer)
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pBuffer,
			sizeof(*pBuffer)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iSize = xrtNetBufSize(pBuffer);
	if ( iSize > xsshChannelIoWriteAvailable(pIo) ) {
		return XSSH_ERROR_SPACE;
	}
	if ( iSize == 0u ) {
		return XSSH_OK;
	}
	pTarget = xsshChannelIoSendBuffer(pIo, Stream);
	return xrtNetBufMove(pTarget, pBuffer) ?
		XSSH_OK : XSSH_ERROR_STATE;
}



/* 为认证后的 peer data 预分配动态接收块。 */
xsshcode xrtSshChannelIoReceivePrepare(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	uint32 iRecipient,
	xbytesview Data
)
{
	xsshchannelcore Channel;
	size_t iReadable;
	xsshcode Code;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xsshChannelIoDataValid(pIo, Data.Data, Data.Size) ||
		(Data.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xsshChannelIoPairSize(
		&pIo->ReceiveData,
		&pIo->ReceiveError,
		&iReadable
	) || (iReadable > pIo->ReceiveLimit) ||
		(Data.Size > (pIo->ReceiveLimit - iReadable)) ) {
		return XSSH_ERROR_SPACE;
	}
	Channel = *pIo->Channel;
	Code = xrtSshChannelCoreDataReceiveCommit(
		&Channel,
		iRecipient,
		(uint32)Data.Size
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtNetBufAppend(&pIo->ReceiveStaging, Data.Data, Data.Size) ) {
		return XSSH_ERROR_SPACE;
	}
	pIo->ChannelBefore = *pIo->Channel;
	pIo->ChannelAfter = Channel;
	pIo->PendingBytes = Data.Size;
	pIo->PendingStream = Stream;
	pIo->Pending = XSSH_CHANNEL_IO_PENDING_RECEIVE;
	return XSSH_OK;
}



/* 在 channel 状态提交后发布预分配接收数据。 */
xsshcode xrtSshChannelIoReceiveCommit(xsshchannelio* pIo)
{
	xnetbuf* pTarget;
	size_t iReadable;

	if ( !xsshChannelIoValid(pIo) ||
		(pIo->Pending != XSSH_CHANNEL_IO_PENDING_RECEIVE) ||
		(memcmp(
			pIo->Channel,
			&pIo->ChannelAfter,
			sizeof(pIo->ChannelAfter)
		) != 0) || !xsshChannelIoPairSize(
			&pIo->ReceiveData,
			&pIo->ReceiveError,
			&iReadable
		) || (iReadable > (SIZE_MAX - pIo->PendingBytes)) ||
		((uint64)(iReadable + pIo->PendingBytes) !=
		 pIo->Channel->Window.ReceiveBuffered) ) {
		return XSSH_ERROR_STATE;
	}
	pTarget = xsshChannelIoReceiveBuffer(pIo, pIo->PendingStream);
	if ( !xrtNetBufMove(pTarget, &pIo->ReceiveStaging) ) {
		return XSSH_ERROR_STATE;
	}
	xsshChannelIoPendingClear(pIo);
	return XSSH_OK;
}



/* 无损放弃尚未提交的接收数据。 */
xsshcode xrtSshChannelIoReceiveAbort(xsshchannelio* pIo)
{
	if ( !xsshChannelIoValid(pIo) ||
		(pIo->Pending != XSSH_CHANNEL_IO_PENDING_RECEIVE) ||
		(memcmp(
			pIo->Channel,
			&pIo->ChannelBefore,
			sizeof(pIo->ChannelBefore)
		) != 0) ) {
		return XSSH_ERROR_STATE;
	}
	xrtNetBufClear(&pIo->ReceiveStaging);
	xsshChannelIoPendingClear(pIo);
	return XSSH_OK;
}



/* 从动态发送队首直接构建一条最终 channel payload。 */
xsshcode xrtSshChannelIoSendPrepare(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	xsshwriter* pWriter,
	xbytesview* pPayload
)
{
	xsshchannelcore Channel;
	xsshwriter Writer;
	xnetbuf* pBuffer;
	xnetspan Span;
	xbytesview Data;
	size_t iHeader;
	size_t iLimit;
	size_t iBegin;
	uint32 iLocal;
	uint32 iRemote;
	xsshcode Code;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pPayload, sizeof(*pPayload)) ||
		xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pWriter,
			sizeof(*pWriter)
		) || xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pPayload,
			sizeof(*pPayload)
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pWriter,
			sizeof(*pWriter)
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pPayload,
			sizeof(*pPayload)
		) || !xrtMemRangeValid(pWriter->Data, pWriter->Capacity) ||
		(pWriter->Size > pWriter->Capacity) || xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pWriter->Data,
			pWriter->Capacity
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pWriter->Data,
			pWriter->Capacity
		) || xrtMemRangesOverlap(
			pPayload,
			sizeof(*pPayload),
			pWriter->Data,
			pWriter->Capacity
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtSshChannelCoreIds(pIo->Channel, &iLocal, &iRemote) ) {
		return XSSH_ERROR_STATE;
	}
	(void)iLocal;
	pBuffer = xsshChannelIoSendBuffer(pIo, Stream);
	if ( !xrtNetBufFront(pBuffer, &Span) ) {
		return XSSH_NEED_MORE;
	}
	iLimit = (size_t)xrtSshChannelCoreSendLimit(pIo->Channel);
	if ( iLimit == 0u ) {
		return XSSH_NEED_MORE;
	}
	iHeader = Stream == XSSH_CHANNEL_IO_DATA ? 9u : 13u;
	if ( xrtSshWriterRemaining(pWriter) <= iHeader ) {
		return XSSH_ERROR_SPACE;
	}
	if ( iLimit > Span.Size ) {
		iLimit = Span.Size;
	}
	if ( iLimit > (xrtSshWriterRemaining(pWriter) - iHeader) ) {
		iLimit = xrtSshWriterRemaining(pWriter) - iHeader;
	}
	if ( iLimit == 0u ) {
		return XSSH_ERROR_SPACE;
	}
	Data = (xbytesview){ Span.Data, iLimit };
	Channel = *pIo->Channel;
	Code = xrtSshChannelCoreDataSendCommit(
		&Channel,
		(uint32)iLimit
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	iBegin = Writer.Size;
	Code = Stream == XSSH_CHANNEL_IO_DATA ?
		xrtSshChannelDataWrite(&Writer, iRemote, Data) :
		xrtSshChannelExtendedDataWrite(
			&Writer,
			iRemote,
			XSSH_CHANNEL_EXTENDED_DATA_STDERR,
			Data
		);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pIo->ChannelBefore = *pIo->Channel;
	pIo->ChannelAfter = Channel;
	pIo->SendHead = Span.Data;
	pIo->PendingBytes = iLimit;
	pIo->PendingStream = Stream;
	pIo->Pending = XSSH_CHANNEL_IO_PENDING_SEND;
	*pWriter = Writer;
	pPayload->Data = Writer.Data + iBegin;
	pPayload->Size = Writer.Size - iBegin;
	return XSSH_OK;
}



/* 在 channel 发送额度已经提交后消费队首。 */
xsshcode xrtSshChannelIoSendCommit(xsshchannelio* pIo)
{
	xnetbuf* pBuffer;
	xnetspan Span;

	if ( !xsshChannelIoValid(pIo) ||
		(pIo->Pending != XSSH_CHANNEL_IO_PENDING_SEND) ||
		(memcmp(
			pIo->Channel,
			&pIo->ChannelAfter,
			sizeof(pIo->ChannelAfter)
		) != 0) ) {
		return XSSH_ERROR_STATE;
	}
	pBuffer = xsshChannelIoSendBuffer(pIo, pIo->PendingStream);
	if ( !xrtNetBufFront(pBuffer, &Span) ||
		(Span.Data != pIo->SendHead) ||
		(Span.Size < pIo->PendingBytes) ) {
		return XSSH_ERROR_STATE;
	}
	if ( xrtNetBufConsume(pBuffer, pIo->PendingBytes) !=
		pIo->PendingBytes ) {
		return XSSH_ERROR_STATE;
	}
	xsshChannelIoPendingClear(pIo);
	return XSSH_OK;
}



/* 无损放弃尚未提交的发送队首。 */
xsshcode xrtSshChannelIoSendAbort(xsshchannelio* pIo)
{
	if ( !xsshChannelIoValid(pIo) ||
		(pIo->Pending != XSSH_CHANNEL_IO_PENDING_SEND) ||
		(memcmp(
			pIo->Channel,
			&pIo->ChannelBefore,
			sizeof(pIo->ChannelBefore)
		) != 0) ) {
		return XSSH_ERROR_STATE;
	}
	xsshChannelIoPendingClear(pIo);
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_forward_message.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_FORWARD_MESSAGE)
#include <string.h>





#if defined(XSSH_FEATURE_FORWARD_MESSAGE)

/* TCP/IP forwarding 线路字段只接受操作系统端口范围。 */
static bool xsshForwardPortValid(uint32 iPort)
{
	return iPort <= UINT16_MAX;
}



/* 比较借用名称与编译期 request/channel 类型。 */
static bool xsshForwardNameEqual(
	xstrview Name,
	const char* pExpected,
	size_t iExpected
)
{
	return (Name.Size == iExpected) &&
		(memcmp(Name.Data, pExpected, iExpected) == 0);
}



/* 写入包含地址和端口的 remote forwarding 全局请求。 */
static xsshcode xsshForwardGlobalWrite(
	xsshwriter* pWriter,
	xstrview Name,
	xbytesview Address,
	uint32 iPort
)
{
	xsshwriter Writer;
	size_t iFieldsSize = 4u;
	xsshcode Code;

	if ( !xsshForwardPortValid(iPort) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshChannelAddString(Address, &iFieldsSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshGlobalRequestWriteBegin(
		pWriter,
		Name,
		true,
		iFieldsSize,
		&Address,
		1u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, Address) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPort) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 remote forwarding 全局请求字段。 */
static xsshcode xsshForwardGlobalRead(
	const xsshglobalrequest* pRequest,
	const char* pName,
	size_t iNameSize,
	xsshtcpipforward* pForward
)
{
	xsshreader Reader;
	xsshtcpipforward Forward;
	xsshcode Code;

	if ( (pRequest == NULL) || (pForward == NULL) ||
		!pRequest->WantReply || !xrtSshNameValid(pRequest->Name) ||
		!xsshForwardNameEqual(pRequest->Name, pName, iNameSize) ||
		!xrtMemRangeValid(pRequest->Fields.Data, pRequest->Fields.Size) ||
		xrtMemRangesOverlap(
			pRequest->Fields.Data,
			pRequest->Fields.Size,
			pForward,
			sizeof(*pForward)
		) || !xrtSshReaderInit(&Reader, pRequest->Fields) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xrtSshReadString(&Reader, &Forward.Address)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Forward.Port)) != XSSH_OK) ) {
		return Code;
	}
	if ( (xrtSshReaderRemaining(&Reader) != 0u) ||
		 !xsshForwardPortValid(Forward.Port) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pForward = Forward;
	return XSSH_OK;
}



/* 写入 direct/forwarded 共用的 TCP/IP channel open。 */
static xsshcode xsshForwardOpenWrite(
	xsshwriter* pWriter,
	xstrview Type,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort
)
{
	xbytesview arrInputs[2] = { Host, Originator };
	xsshwriter Writer;
	size_t iFieldsSize = 8u;
	xsshcode Code;

	if ( !xsshForwardPortValid(iPort) ||
		 !xsshForwardPortValid(iOriginatorPort) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshChannelAddString(Host, &iFieldsSize)) != XSSH_OK) ||
		((Code = xsshChannelAddString(Originator, &iFieldsSize)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshChannelOpenWriteBegin(
		pWriter,
		Type,
		iSender,
		iWindow,
		iMaxPacket,
		iFieldsSize,
		arrInputs,
		2u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, Host) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPort) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Originator) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iOriginatorPort) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 direct/forwarded 共用的 TCP/IP 专用字段。 */
static xsshcode xsshForwardOpenRead(
	const xsshchannelopen* pOpen,
	const char* pType,
	size_t iTypeSize,
	xsshtcpipopen* pTcpip
)
{
	xsshreader Reader;
	xsshtcpipopen Tcpip;
	xsshcode Code;

	if ( (pOpen == NULL) || (pTcpip == NULL) ||
		!xrtSshNameValid(pOpen->Type) ||
		!xsshForwardNameEqual(pOpen->Type, pType, iTypeSize) ||
		!xrtMemRangeValid(pOpen->Fields.Data, pOpen->Fields.Size) ||
		xrtMemRangesOverlap(
			pOpen->Fields.Data,
			pOpen->Fields.Size,
			pTcpip,
			sizeof(*pTcpip)
		) || !xrtSshReaderInit(&Reader, pOpen->Fields) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xrtSshReadString(&Reader, &Tcpip.Host)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Tcpip.Port)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Tcpip.Originator)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Tcpip.OriginatorPort)) != XSSH_OK) ) {
		return Code;
	}
	if ( (xrtSshReaderRemaining(&Reader) != 0u) ||
		 !xsshForwardPortValid(Tcpip.Port) ||
		 !xsshForwardPortValid(Tcpip.OriginatorPort) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pTcpip = Tcpip;
	return XSSH_OK;
}



/* 写入要求回复的 tcpip-forward。 */
xsshcode xrtSshTcpipForwardWrite(
	xsshwriter* pWriter,
	xbytesview Address,
	uint32 iPort
)
{
	return xsshForwardGlobalWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_GLOBAL_REQUEST_TCPIP_FORWARD),
		Address,
		iPort
	);
}



/* 严格读取 tcpip-forward。 */
xsshcode xrtSshTcpipForwardRead(
	const xsshglobalrequest* pRequest,
	xsshtcpipforward* pForward
)
{
	return xsshForwardGlobalRead(
		pRequest,
		XSSH_GLOBAL_REQUEST_TCPIP_FORWARD,
		sizeof(XSSH_GLOBAL_REQUEST_TCPIP_FORWARD) - 1u,
		pForward
	);
}



/* 写入要求回复的 cancel-tcpip-forward。 */
xsshcode xrtSshTcpipForwardCancelWrite(
	xsshwriter* pWriter,
	xbytesview Address,
	uint32 iPort
)
{
	return xsshForwardGlobalWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD),
		Address,
		iPort
	);
}



/* 严格读取 cancel-tcpip-forward。 */
xsshcode xrtSshTcpipForwardCancelRead(
	const xsshglobalrequest* pRequest,
	xsshtcpipforward* pForward
)
{
	return xsshForwardGlobalRead(
		pRequest,
		XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD,
		sizeof(XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD) - 1u,
		pForward
	);
}



/* 写入动态端口分配成功响应。 */
xsshcode xrtSshTcpipForwardSuccessWrite(
	xsshwriter* pWriter,
	uint32 iPort
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( !xsshForwardPortValid(iPort) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshWriterReserveInputs(pWriter, 5u, NULL, 0u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_REQUEST_SUCCESS) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPort) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取动态端口分配成功响应。 */
xsshcode xrtSshTcpipForwardSuccessRead(
	xbytesview Payload,
	uint32* pPort
)
{
	xsshreader Reader;
	uint32 iPort;
	uint8 iMessage;
	xsshcode Code;

	if ( (pPort == NULL) || !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pPort,
			sizeof(*pPort)
		) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xrtSshReadByte(&Reader, &iMessage)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &iPort)) != XSSH_OK) ) {
		return Code;
	}
	if ( (iMessage != XSSH_MSG_REQUEST_SUCCESS) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ||
		!xsshForwardPortValid(iPort) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPort = iPort;
	return XSSH_OK;
}



/* 写入 direct-tcpip channel open。 */
xsshcode xrtSshDirectTcpipOpenWrite(
	xsshwriter* pWriter,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort
)
{
	return xsshForwardOpenWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_CHANNEL_TYPE_DIRECT_TCPIP),
		iSender,
		iWindow,
		iMaxPacket,
		Host,
		iPort,
		Originator,
		iOriginatorPort
	);
}



/* 严格读取 direct-tcpip 专用字段。 */
xsshcode xrtSshDirectTcpipOpenRead(
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip
)
{
	return xsshForwardOpenRead(
		pOpen,
		XSSH_CHANNEL_TYPE_DIRECT_TCPIP,
		sizeof(XSSH_CHANNEL_TYPE_DIRECT_TCPIP) - 1u,
		pTcpip
	);
}



/* 写入 forwarded-tcpip channel open。 */
xsshcode xrtSshForwardedTcpipOpenWrite(
	xsshwriter* pWriter,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort
)
{
	return xsshForwardOpenWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_CHANNEL_TYPE_FORWARDED_TCPIP),
		iSender,
		iWindow,
		iMaxPacket,
		Host,
		iPort,
		Originator,
		iOriginatorPort
	);
}



/* 严格读取 forwarded-tcpip 专用字段。 */
xsshcode xrtSshForwardedTcpipOpenRead(
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip
)
{
	return xsshForwardOpenRead(
		pOpen,
		XSSH_CHANNEL_TYPE_FORWARDED_TCPIP,
		sizeof(XSSH_CHANNEL_TYPE_FORWARDED_TCPIP) - 1u,
		pTcpip
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_reply_queue.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_REPLY_QUEUE)



#if defined(XSSH_FEATURE_REPLY_QUEUE)

/* 计算 token 存储字节数并拒绝乘法溢出。 */
static bool xsshReplyStorageSize(size_t iCapacity, size_t* pSize)
{
	if ( (pSize == NULL) ||
		(iCapacity > (SIZE_MAX / sizeof(uint64))) ) {
		return false;
	}
	*pSize = iCapacity * sizeof(uint64);
	return true;
}



/* 校验 ring 状态与调用方存储范围。 */
static bool xsshReplyQueueValid(const xsshreplyqueue* pQueue)
{
	size_t iStorageSize;

	if ( (pQueue == NULL) || !pQueue->Initialized ||
		!xsshReplyStorageSize(pQueue->Capacity, &iStorageSize) ||
		!xrtMemRangeValid(pQueue->Tokens, iStorageSize) ||
		xrtMemRangesOverlap(
			pQueue->Tokens,
			iStorageSize,
			pQueue,
			sizeof(*pQueue)
		) || (pQueue->Count > pQueue->Capacity) ) {
		return false;
	}
	return pQueue->Capacity == 0u ? pQueue->Head == 0u :
		pQueue->Head < pQueue->Capacity;
}



/* 计算不发生 size_t 回绕的 ring 下标。 */
static size_t xsshReplyIndex(
	const xsshreplyqueue* pQueue,
	size_t iOffset
)
{
	size_t iUntilEnd = pQueue->Capacity - pQueue->Head;

	return iOffset >= iUntilEnd ?
		iOffset - iUntilEnd : pQueue->Head + iOffset;
}



/* 初始化调用方提供的 token ring。 */
bool xrtSshReplyQueueInit(
	xsshreplyqueue* pQueue,
	uint64* pTokens,
	size_t iCapacity
)
{
	xsshreplyqueue Queue;
	size_t iStorageSize;

	if ( (pQueue == NULL) ||
		!xsshReplyStorageSize(iCapacity, &iStorageSize) ||
		!xrtMemRangeValid(pTokens, iStorageSize) ||
		xrtMemRangesOverlap(
			pTokens,
			iStorageSize,
			pQueue,
			sizeof(*pQueue)
		) ) {
		return false;
	}
	Queue.Tokens = pTokens;
	Queue.Capacity = iCapacity;
	Queue.Head = 0u;
	Queue.Count = 0u;
	Queue.Initialized = true;
	*pQueue = Queue;
	return true;
}



/* 返回等待回复数量。 */
size_t xrtSshReplyQueueCount(const xsshreplyqueue* pQueue)
{
	return xsshReplyQueueValid(pQueue) ? pQueue->Count : 0u;
}



/* 追加已经可靠发送的 want-reply 请求 token。 */
xsshcode xrtSshReplyQueuePush(
	xsshreplyqueue* pQueue,
	uint64 iToken
)
{
	size_t iTail;

	if ( !xsshReplyQueueValid(pQueue) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pQueue->Count == pQueue->Capacity ) {
		return XSSH_ERROR_SPACE;
	}
	iTail = xsshReplyIndex(pQueue, pQueue->Count);
	pQueue->Tokens[iTail] = iToken;
	++pQueue->Count;
	return XSSH_OK;
}



/* 查看但不消费队首 token。 */
xsshcode xrtSshReplyQueueFront(
	const xsshreplyqueue* pQueue,
	uint64* pToken
)
{
	uint64 iToken;
	size_t iStorageSize;

	if ( !xsshReplyQueueValid(pQueue) || (pToken == NULL) ||
		!xsshReplyStorageSize(pQueue->Capacity, &iStorageSize) ||
		xrtMemRangesOverlap(
			pQueue->Tokens,
			iStorageSize,
			pToken,
			sizeof(*pToken)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pQueue->Count == 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	iToken = pQueue->Tokens[pQueue->Head];
	*pToken = iToken;
	return XSSH_OK;
}



/* 按线路顺序消费队首 token。 */
xsshcode xrtSshReplyQueuePop(
	xsshreplyqueue* pQueue,
	uint64* pToken
)
{
	uint64 iToken;
	size_t iStorageSize;

	if ( !xsshReplyQueueValid(pQueue) || (pToken == NULL) ||
		!xsshReplyStorageSize(pQueue->Capacity, &iStorageSize) ||
		xrtMemRangesOverlap(
			pQueue->Tokens,
			iStorageSize,
			pToken,
			sizeof(*pToken)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pQueue->Count == 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	iToken = pQueue->Tokens[pQueue->Head];
	if ( pQueue->Head == (pQueue->Capacity - 1u) ) {
		pQueue->Head = 0u;
	} else {
		++pQueue->Head;
	}
	--pQueue->Count;
	*pToken = iToken;
	return XSSH_OK;
}



/* 将 ring 按逻辑顺序迁移到新的不重叠存储。 */
xsshcode xrtSshReplyQueueRebind(
	xsshreplyqueue* pQueue,
	uint64* pTokens,
	size_t iCapacity
)
{
	size_t iOldSize;
	size_t iNewSize;
	size_t i;

	if ( !xsshReplyQueueValid(pQueue) ||
		!xsshReplyStorageSize(pQueue->Capacity, &iOldSize) ||
		!xsshReplyStorageSize(iCapacity, &iNewSize) ||
		(iCapacity < pQueue->Count) ||
		!xrtMemRangeValid(pTokens, iNewSize) ||
		xrtMemRangesOverlap(
			pTokens,
			iNewSize,
			pQueue,
			sizeof(*pQueue)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pTokens == pQueue->Tokens) && (iCapacity == pQueue->Capacity) ) {
		return XSSH_OK;
	}
	if ( xrtMemRangesOverlap(
		pQueue->Tokens,
		iOldSize,
		pTokens,
		iNewSize
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < pQueue->Count; ++i ) {
		pTokens[i] = pQueue->Tokens[xsshReplyIndex(pQueue, i)];
	}
	pQueue->Tokens = pTokens;
	pQueue->Capacity = iCapacity;
	pQueue->Head = 0u;
	return XSSH_OK;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_connection_session.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CONNECTION_SESSION)
#include <string.h>




#if defined(XSSH_FEATURE_CONNECTION_SESSION)

#define XSSH_CONNECTION_SESSION_GUARD UINT32_C(0x434f4e4e)



/* 校验调用方 reply FIFO 的公开环形状态。 */
static bool xsshConnectionQueueValid(const xsshreplyqueue* pQueue)
{
	size_t iBytes;

	if ( !xrtMemRangeValid(pQueue, sizeof(*pQueue)) ||
		!pQueue->Initialized || (pQueue->Count > pQueue->Capacity) ||
		(pQueue->Capacity > (SIZE_MAX / sizeof(uint64))) ) {
		return false;
	}
	iBytes = pQueue->Capacity * sizeof(uint64);
	if ( (pQueue->Capacity == 0u) &&
		((pQueue->Head != 0u) || (pQueue->Count != 0u)) ) {
		return false;
	}
	if ( (pQueue->Capacity != 0u) &&
		(pQueue->Head >= pQueue->Capacity) ) {
		return false;
	}
	return xrtMemRangeValid(pQueue->Tokens, iBytes) &&
		!xrtMemRangesOverlap(
			pQueue,
			sizeof(*pQueue),
			pQueue->Tokens,
			iBytes
		);
}



/* 校验会话固定状态，外部 channel 与 FIFO 在事务入口单独检查。 */
static bool xsshConnectionSessionValid(
	const xsshconnectionsession* pSession
)
{
	return xrtMemRangeValid(pSession, sizeof(*pSession)) &&
		(pSession->ObjectGuard == XSSH_CONNECTION_SESSION_GUARD) &&
		((pSession->Role == XSSH_ROLE_CLIENT) ||
		 (pSession->Role == XSSH_ROLE_SERVER)) &&
		(pSession->WritePending >= XSSH_CONNECTION_PACKET_NONE) &&
		(pSession->WritePending <=
		 XSSH_CONNECTION_PACKET_CHANNEL_FAILURE) &&
		(pSession->ReadPending >= XSSH_CONNECTION_PACKET_NONE) &&
		(pSession->ReadPending <=
		 XSSH_CONNECTION_PACKET_CHANNEL_FAILURE) &&
		(pSession->QueueAction >= XSSH_CONNECTION_QUEUE_NONE) &&
		(pSession->QueueAction <= XSSH_CONNECTION_QUEUE_POP);
}



/* 清除单个待提交事务，不修改调用方对象。 */
static void xsshConnectionSessionPendingClear(
	xsshconnectionsession* pSession
)
{
	memset(&pSession->ChannelBefore, 0, sizeof(pSession->ChannelBefore));
	memset(&pSession->ChannelPending, 0, sizeof(pSession->ChannelPending));
	memset(&pSession->QueueBefore, 0, sizeof(pSession->QueueBefore));
	pSession->Channel = NULL;
	pSession->Queue = NULL;
	pSession->QueueToken = 0u;
	pSession->QueueAction = XSSH_CONNECTION_QUEUE_NONE;
}



/* 失败会话丢弃全部内部事务快照，外部 channel 与 FIFO 保持原状。 */
static void xsshConnectionSessionSetFailed(
	xsshconnectionsession* pSession
)
{
	xsshConnectionSessionPendingClear(pSession);
	pSession->WritePending = XSSH_CONNECTION_PACKET_NONE;
	pSession->ReadPending = XSSH_CONNECTION_PACKET_NONE;
	pSession->WriteOrdinal = 0u;
	pSession->ReadOrdinal = 0u;
	pSession->Failed = true;
}



/* 返回当前角色应观察的 server USERAUTH_SUCCESS 方向。 */
static bool xsshConnectionCoreAuthenticated(
	const xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
)
{
	return pSession->Role == XSSH_ROLE_SERVER ?
		pCore->State.LocalAuthSuccess : pCore->State.PeerAuthSuccess;
}



/* 校验 transport 地址、角色和无并行 packet 事务边界。 */
static bool xsshConnectionCoreValid(
	const xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) && (pCore->State.Role == pSession->Role);
}



/* 严格分类一条 RFC 4254 connection payload。 */
static xsshcode xsshConnectionPacketRead(
	xbytesview Payload,
	xsshconnectionpacket* pPacket,
	bool* pRecognized
)
{
	xsshconnectionpacket Packet;
	uint8 iMessage;
	xsshcode Code;

	memset(&Packet, 0, sizeof(Packet));
	if ( pRecognized != NULL ) {
		*pRecognized = false;
	}
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage == XSSH_MSG_GLOBAL_REQUEST ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_GLOBAL_REQUEST;
		Code = xrtSshGlobalRequestRead(
			Payload,
			&Packet.Message.GlobalRequest
		);
	} else if ( iMessage == XSSH_MSG_REQUEST_SUCCESS ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_GLOBAL_SUCCESS;
		Code = xrtSshGlobalSuccessRead(
			Payload,
			&Packet.Message.GlobalSuccess
		);
	} else if ( iMessage == XSSH_MSG_REQUEST_FAILURE ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_GLOBAL_FAILURE;
		Code = xrtSshGlobalFailureRead(Payload);
	} else if ( iMessage == XSSH_MSG_CHANNEL_OPEN ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_OPEN;
		Code = xrtSshChannelOpenRead(
			Payload,
			&Packet.Message.ChannelOpen
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_OPEN_CONFIRMATION ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION;
		Code = xrtSshChannelOpenConfirmationRead(
			Payload,
			&Packet.Message.ChannelConfirmation
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_OPEN_FAILURE ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE;
		Code = xrtSshChannelOpenFailureRead(
			Payload,
			&Packet.Message.ChannelOpenFailure
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_WINDOW_ADJUST ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_ADJUST;
		Code = xrtSshChannelWindowAdjustRead(
			Payload,
			&Packet.Message.ChannelAdjust
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_DATA ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_DATA;
		Code = xrtSshChannelDataRead(
			Payload,
			&Packet.Message.ChannelData
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_EXTENDED_DATA ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA;
		Code = xrtSshChannelExtendedDataRead(
			Payload,
			&Packet.Message.ChannelExtendedData
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_EOF ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_EOF;
		Code = xrtSshChannelEofRead(
			Payload,
			&Packet.Message.Recipient
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_CLOSE ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_CLOSE;
		Code = xrtSshChannelCloseRead(
			Payload,
			&Packet.Message.Recipient
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_REQUEST ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_REQUEST;
		Code = xrtSshChannelRequestRead(
			Payload,
			&Packet.Message.ChannelRequest
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_SUCCESS ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS;
		Code = xrtSshChannelSuccessRead(
			Payload,
			&Packet.Message.Recipient
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_FAILURE ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_FAILURE;
		Code = xrtSshChannelFailureRead(
			Payload,
			&Packet.Message.Recipient
		);
	} else {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( pRecognized != NULL ) {
		*pRecognized = true;
	}
	if ( Code == XSSH_OK ) {
		*pPacket = Packet;
	}
	return Code;
}



/* 判断分类是否需要一个已经存在的 channel。 */
static bool xsshConnectionPacketHasChannel(
	xsshconnectionpacketkind Kind
)
{
	return (Kind >= XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION) &&
		(Kind <= XSSH_CONNECTION_PACKET_CHANNEL_FAILURE);
}



/* 返回 channel 消息中的本地或远端 recipient。 */
static uint32 xsshConnectionPacketRecipient(
	const xsshconnectionpacket* pPacket
)
{
	switch ( pPacket->Kind ) {
		case XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION:
			return pPacket->Message.ChannelConfirmation.Recipient;
		case XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE:
			return pPacket->Message.ChannelOpenFailure.Recipient;
		case XSSH_CONNECTION_PACKET_CHANNEL_ADJUST:
			return pPacket->Message.ChannelAdjust.Recipient;
		case XSSH_CONNECTION_PACKET_CHANNEL_DATA:
			return pPacket->Message.ChannelData.Recipient;
		case XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA:
			return pPacket->Message.ChannelExtendedData.Recipient;
		case XSSH_CONNECTION_PACKET_CHANNEL_REQUEST:
			return pPacket->Message.ChannelRequest.Recipient;
		default:
			return pPacket->Message.Recipient;
	}
}



/* 校验短事务借用的 channel 不覆盖其他状态或 payload。 */
static bool xsshConnectionChannelValid(
	const xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	const xsshchannelcore* pChannel
)
{
	return xrtMemRangeValid(pChannel, sizeof(*pChannel)) &&
		pChannel->Initialized &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pChannel,
			sizeof(*pChannel)
		) && !xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pChannel,
			sizeof(*pChannel)
		) && !xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pChannel,
			sizeof(*pChannel)
		);
}



/* 校验短事务借用的 reply FIFO 及 token 存储不覆盖协议状态。 */
static bool xsshConnectionQueueBorrowValid(
	const xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	const xsshchannelcore* pChannel,
	const xsshreplyqueue* pQueue
)
{
	size_t iBytes;

	if ( !xsshConnectionQueueValid(pQueue) ) {
		return false;
	}
	if ( (pChannel != NULL) && (pQueue == pSession->GlobalReplies) ) {
		return false;
	}
	iBytes = pQueue->Capacity * sizeof(uint64);
	return !xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pQueue,
		sizeof(*pQueue)
	) && !xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pQueue,
		sizeof(*pQueue)
	) && !xrtMemRangesOverlap(
		Payload.Data,
		Payload.Size,
		pQueue,
		sizeof(*pQueue)
	) && ((pChannel == NULL) ||
		!xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pQueue,
			sizeof(*pQueue)
		)) && !xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pQueue->Tokens,
		iBytes
	) && !xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pQueue->Tokens,
		iBytes
	) && !xrtMemRangesOverlap(
		Payload.Data,
		Payload.Size,
		pQueue->Tokens,
		iBytes
	) && ((pChannel == NULL) ||
		!xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pQueue->Tokens,
			iBytes
		));
}



/* 预留一次可靠提交后执行的 FIFO push。 */
static xsshcode xsshConnectionQueuePushPrepare(
	xsshconnectionsession* pSession,
	xsshreplyqueue* pQueue,
	uint64 iToken
)
{
	if ( pQueue->Count >= pQueue->Capacity ) {
		return XSSH_ERROR_SPACE;
	}
	pSession->Queue = pQueue;
	pSession->QueueBefore = *pQueue;
	pSession->QueueToken = iToken;
	pSession->QueueAction = XSSH_CONNECTION_QUEUE_PUSH;
	return XSSH_OK;
}



/* 预留一次可靠提交后执行的 FIFO pop，并借出当前关联 token。 */
static xsshcode xsshConnectionQueuePopPrepare(
	xsshconnectionsession* pSession,
	xsshreplyqueue* pQueue,
	uint64* pToken
)
{
	xsshcode Code = xrtSshReplyQueueFront(pQueue, pToken);

	if ( Code != XSSH_OK ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pSession->Queue = pQueue;
	pSession->QueueBefore = *pQueue;
	pSession->QueueToken = *pToken;
	pSession->QueueAction = XSSH_CONNECTION_QUEUE_POP;
	return XSSH_OK;
}



/* 在副本中准备一条本端 channel 输出。 */
static xsshcode xsshConnectionWriteChannelPrepare(
	xsshconnectionsession* pSession,
	const xsshconnectionpacket* pPacket,
	xsshchannelcore* pChannel
)
{
	xsshchannelcore Channel = *pChannel;
	uint32 iLocal;
	uint32 iRemote;
	size_t iSize;
	xsshcode Code = XSSH_OK;

	switch ( pPacket->Kind ) {
		case XSSH_CONNECTION_PACKET_CHANNEL_OPEN:
			if ( (xrtSshChannelCorePhase(pChannel) !=
				XSSH_CHANNEL_CORE_OPENING) ||
				(pPacket->Message.ChannelOpen.Sender != pChannel->Local) ||
				(pPacket->Message.ChannelOpen.Window !=
				 pChannel->Window.ReceiveWindow) ||
				(pPacket->Message.ChannelOpen.MaxPacket !=
				 pChannel->Window.ReceiveMaxPacket) ) {
				return XSSH_ERROR_STATE;
			}
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION:
			if ( (xrtSshChannelCorePhase(pChannel) !=
				XSSH_CHANNEL_CORE_ACCEPTING) ||
				!xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelConfirmation.Recipient != iRemote) ||
				(pPacket->Message.ChannelConfirmation.Sender != iLocal) ||
				(pPacket->Message.ChannelConfirmation.Window !=
				 pChannel->Window.ReceiveWindow) ||
				(pPacket->Message.ChannelConfirmation.MaxPacket !=
				 pChannel->Window.ReceiveMaxPacket) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreAcceptCommit(&Channel);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE:
			if ( (xrtSshChannelCorePhase(pChannel) !=
				XSSH_CHANNEL_CORE_ACCEPTING) ||
				!xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelOpenFailure.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreRejectCommit(
				&Channel,
				pPacket->Message.ChannelOpenFailure.Reason
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_ADJUST:
			if ( !xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelAdjust.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreAdjustSendCommit(
				&Channel,
				pPacket->Message.ChannelAdjust.Bytes
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_DATA:
			iSize = pPacket->Message.ChannelData.Data.Size;
			if ( (iSize > UINT32_MAX) ||
				!xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelData.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreDataSendCommit(
				&Channel,
				(uint32)iSize
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA:
			iSize = pPacket->Message.ChannelExtendedData.Data.Size;
			if ( (iSize > UINT32_MAX) ||
				!xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelExtendedData.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreDataSendCommit(
				&Channel,
				(uint32)iSize
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_EOF:
			if ( !xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreEofSendCommit(&Channel);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_CLOSE:
			if ( !xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreCloseSendCommit(&Channel);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_REQUEST:
			if ( !xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelRequest.Recipient != iRemote) ||
				!xrtSshChannelCoreCanSendRequest(pChannel) ) {
				return XSSH_ERROR_STATE;
			}
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS:
		case XSSH_CONNECTION_PACKET_CHANNEL_FAILURE:
			if ( !xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.Recipient != iRemote) ||
				!xrtSshChannelCoreCanSendRequest(pChannel) ) {
				return XSSH_ERROR_STATE;
			}
			break;
		default:
			return XSSH_ERROR_STATE;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->Channel = pChannel;
	pSession->ChannelBefore = *pChannel;
	pSession->ChannelPending = Channel;
	return XSSH_OK;
}



/* 通过调用方 resolver 取得当前本地 recipient 的状态对象。 */
static xsshcode xsshConnectionResolve(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	uint32 iLocal,
	xsshchannelcore** ppChannel,
	xsshreplyqueue** ppReplies
)
{
	xsshchannelcore* pChannel = NULL;
	xsshreplyqueue* pReplies = NULL;

	if ( (pSession->Resolve == NULL) || !pSession->Resolve(
		pSession->UserData,
		iLocal,
		&pChannel,
		&pReplies
	) || !xsshConnectionChannelValid(
		pSession,
		pCore,
		Payload,
		pChannel
	) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*ppChannel = pChannel;
	*ppReplies = pReplies;
	return XSSH_OK;
}



/* 在副本中准备一条 peer channel 输入。 */
static xsshcode xsshConnectionReadChannelPrepare(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshconnectionpacket* pPacket
)
{
	xsshchannelcore* pChannel;
	xsshreplyqueue* pReplies;
	xsshchannelcore Channel;
	uint32 iRecipient = xsshConnectionPacketRecipient(pPacket);
	size_t iSize;
	xsshcode Code;

	Code = xsshConnectionResolve(
		pSession,
		pCore,
		Payload,
		iRecipient,
		&pChannel,
		&pReplies
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Channel = *pChannel;
	switch ( pPacket->Kind ) {
		case XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION:
			Code = xrtSshChannelCoreConfirmationCommit(
				&Channel,
				&pPacket->Message.ChannelConfirmation
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE:
			Code = xrtSshChannelCoreFailureCommit(
				&Channel,
				&pPacket->Message.ChannelOpenFailure
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_ADJUST:
			Code = xrtSshChannelCoreAdjustReceiveCommit(
				&Channel,
				&pPacket->Message.ChannelAdjust
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_DATA:
			iSize = pPacket->Message.ChannelData.Data.Size;
			Code = iSize > UINT32_MAX ? XSSH_ERROR_PROTOCOL :
				xrtSshChannelCoreDataReceiveCommit(
					&Channel,
					iRecipient,
					(uint32)iSize
				);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA:
			iSize = pPacket->Message.ChannelExtendedData.Data.Size;
			Code = iSize > UINT32_MAX ? XSSH_ERROR_PROTOCOL :
				xrtSshChannelCoreDataReceiveCommit(
					&Channel,
					iRecipient,
					(uint32)iSize
				);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_EOF:
			Code = xrtSshChannelCoreEofReceiveCommit(
				&Channel,
				iRecipient
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_CLOSE:
			Code = xrtSshChannelCoreCloseReceiveCommit(
				&Channel,
				iRecipient
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_REQUEST:
			Code = xrtSshChannelCoreRecipientCheck(
				pChannel,
				iRecipient
			);
			if ( (Code == XSSH_OK) &&
				!xrtSshChannelCoreCanReceiveRequest(pChannel) ) {
				Code = XSSH_ERROR_PROTOCOL;
			}
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS:
		case XSSH_CONNECTION_PACKET_CHANNEL_FAILURE:
			Code = xrtSshChannelCoreRecipientCheck(
				pChannel,
				iRecipient
			);
			if ( Code == XSSH_OK ) {
				if ( !xsshConnectionQueueBorrowValid(
					pSession,
					pCore,
					Payload,
					pChannel,
					pReplies
				) ) {
					Code = XSSH_ERROR_PROTOCOL;
				} else {
					Code = xsshConnectionQueuePopPrepare(
						pSession,
						pReplies,
						&pPacket->ReplyToken
					);
					pPacket->HasReplyToken = Code == XSSH_OK;
				}
			}
			break;
		default:
			return XSSH_ERROR_STATE;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->Channel = pChannel;
	pSession->ChannelBefore = *pChannel;
	pSession->ChannelPending = Channel;
	return XSSH_OK;
}



/* 校验外部对象未在短事务期间被其他执行流修改。 */
static bool xsshConnectionPendingStable(
	const xsshconnectionsession* pSession
)
{
	if ( (pSession->Channel != NULL) &&
		(memcmp(
			pSession->Channel,
			&pSession->ChannelBefore,
			sizeof(pSession->ChannelBefore)
		) != 0) ) {
		return false;
	}
	return (pSession->Queue == NULL) ||
		(memcmp(
			pSession->Queue,
			&pSession->QueueBefore,
			sizeof(pSession->QueueBefore)
		) == 0);
}



/* 在 transport 之后提交 FIFO 与 channel 候选状态。 */
static xsshcode xsshConnectionPendingCommit(
	xsshconnectionsession* pSession
)
{
	uint64 iToken;
	xsshcode Code = XSSH_OK;

	if ( !xsshConnectionPendingStable(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->QueueAction == XSSH_CONNECTION_QUEUE_POP ) {
		Code = xrtSshReplyQueueFront(pSession->Queue, &iToken);
		if ( (Code != XSSH_OK) || (iToken != pSession->QueueToken) ) {
			return XSSH_ERROR_STATE;
		}
	}
	if ( pSession->QueueAction == XSSH_CONNECTION_QUEUE_PUSH ) {
		Code = xrtSshReplyQueuePush(
			pSession->Queue,
			pSession->QueueToken
		);
	} else if ( pSession->QueueAction == XSSH_CONNECTION_QUEUE_POP ) {
		Code = xrtSshReplyQueuePop(pSession->Queue, &iToken);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( pSession->Channel != NULL ) {
		*pSession->Channel = pSession->ChannelPending;
	}
	xsshConnectionSessionPendingClear(pSession);
	return XSSH_OK;
}



/* 初始化调用方存储路由的 connection 会话。 */
bool xrtSshConnectionSessionInit(
	xsshconnectionsession* pSession,
	xsshrole Role,
	xsshchannelresolveproc pResolve,
	ptr pUserData,
	xsshreplyqueue* pGlobalReplies
)
{
	xsshconnectionsession Session;
	size_t iTokenBytes;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	if ( pGlobalReplies != NULL ) {
		if ( !xsshConnectionQueueValid(pGlobalReplies) ||
			(pGlobalReplies->Count != 0u) ) {
			return false;
		}
		iTokenBytes = pGlobalReplies->Capacity * sizeof(uint64);
		if ( xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pGlobalReplies,
			sizeof(*pGlobalReplies)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pGlobalReplies->Tokens,
			iTokenBytes
		) ) {
			return false;
		}
	}
	memset(&Session, 0, sizeof(Session));
	Session.Role = Role;
	Session.Resolve = pResolve;
	Session.UserData = pUserData;
	Session.GlobalReplies = pGlobalReplies;
	Session.ObjectGuard = XSSH_CONNECTION_SESSION_GUARD;
	*pSession = Session;
	return true;
}



/* 清除 connection 会话本身。 */
void xrtSshConnectionSessionClear(xsshconnectionsession* pSession)
{
	if ( xrtMemRangeValid(pSession, sizeof(*pSession)) ) {
		memset(pSession, 0, sizeof(*pSession));
	}
}



/* 在认证成功的 transport 上开始 connection 编排。 */
xsshcode xrtSshConnectionSessionBegin(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
)
{
	if ( !xsshConnectionSessionValid(pSession) ||
		!xsshConnectionCoreValid(pSession, pCore) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pSession->Active || pSession->Failed ||
		pCore->Write.Active || pCore->Read.Active ||
		((pSession->GlobalReplies != NULL) &&
		 (!xsshConnectionQueueValid(pSession->GlobalReplies) ||
		  (pSession->GlobalReplies->Count != 0u))) ||
		!xrtSshTransportCoreKexComplete(pCore) ||
		!xsshConnectionCoreAuthenticated(pSession, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	pSession->Active = true;
	return XSSH_OK;
}



/* 查询会话可用状态。 */
bool xrtSshConnectionSessionActive(
	const xsshconnectionsession* pSession
)
{
	return xsshConnectionSessionValid(pSession) &&
		pSession->Active && !pSession->Failed;
}



/* 准备本端 connection 输出及其外部状态候选。 */
xsshcode xrtSshConnectionSessionWritePrepare(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
)
{
	xsshconnectionpacket Packet;
	xsshreplyqueue* pQueue = NULL;
	bool bWantReply = false;
	xsshcode Code;

	if ( !xrtSshConnectionSessionActive(pSession) ||
		!xsshConnectionCoreValid(pSession, pCore) ||
		pCore->Write.Active || pCore->Read.Active ||
		(pSession->WritePending != XSSH_CONNECTION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_CONNECTION_PACKET_NONE) ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_LOCAL
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshConnectionPacketRead(Payload, &Packet, NULL);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_OPEN ) {
		if ( !xsshConnectionChannelValid(
			pSession,
			pCore,
			Payload,
			pChannel
		) ) {
			return XSSH_ERROR_ARGUMENT;
		}
		Code = xsshConnectionWriteChannelPrepare(
			pSession,
			&Packet,
			pChannel
		);
	} else if ( xsshConnectionPacketHasChannel(Packet.Kind) ) {
		if ( !xsshConnectionChannelValid(
			pSession,
			pCore,
			Payload,
			pChannel
		) ) {
			return XSSH_ERROR_ARGUMENT;
		}
		Code = xsshConnectionWriteChannelPrepare(
			pSession,
			&Packet,
			pChannel
		);
	} else if ( pChannel != NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Code != XSSH_OK ) {
		xsshConnectionSessionPendingClear(pSession);
		return Code;
	}
	if ( Packet.Kind == XSSH_CONNECTION_PACKET_GLOBAL_REQUEST ) {
		if ( pReplies != NULL ) {
			xsshConnectionSessionPendingClear(pSession);
			return XSSH_ERROR_ARGUMENT;
		}
		bWantReply = Packet.Message.GlobalRequest.WantReply;
		pQueue = pSession->GlobalReplies;
	} else if ( Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_REQUEST ) {
		bWantReply = Packet.Message.ChannelRequest.WantReply;
		pQueue = pReplies;
	}
	if ( bWantReply ) {
		if ( !xsshConnectionQueueBorrowValid(
			pSession,
			pCore,
			Payload,
			pChannel,
			pQueue
		) ) {
			xsshConnectionSessionPendingClear(pSession);
			return XSSH_ERROR_ARGUMENT;
		}
		Code = xsshConnectionQueuePushPrepare(
			pSession,
			pQueue,
			iReplyToken
		);
		if ( Code != XSSH_OK ) {
			xsshConnectionSessionPendingClear(pSession);
			return Code;
		}
	} else if ( pReplies != NULL ) {
		xsshConnectionSessionPendingClear(pSession);
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pCore->State.LocalPackets == UINT64_MAX ) {
		xsshConnectionSessionPendingClear(pSession);
		return XSSH_ERROR_STATE;
	}
	pSession->WritePending = Packet.Kind;
	pSession->WriteOrdinal = pCore->State.LocalPackets + 1u;
	return XSSH_OK;
}



/* 提交本端 connection 输出。 */
xsshcode xrtSshConnectionSessionWriteCommit(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshcode Code;

	if ( !xrtSshConnectionSessionActive(pSession) ||
		(pSession->WritePending == XSSH_CONNECTION_PACKET_NONE) ||
		!xsshConnectionCoreValid(pSession, pCore) || pCore->Write.Active ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_LOCAL
		) ||
		(pCore->State.LocalPackets != pSession->WriteOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshConnectionPendingCommit(pSession);
	if ( Code != XSSH_OK ) {
		xsshConnectionSessionSetFailed(pSession);
		return Code;
	}
	pSession->WritePending = XSSH_CONNECTION_PACKET_NONE;
	pSession->WriteOrdinal = 0u;
	return XSSH_OK;
}



/* 无损放弃本端 connection 输出候选。 */
xsshcode xrtSshConnectionSessionWriteAbort(
	xsshconnectionsession* pSession
)
{
	if ( !xrtSshConnectionSessionActive(pSession) ||
		(pSession->WritePending == XSSH_CONNECTION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	xsshConnectionSessionPendingClear(pSession);
	pSession->WritePending = XSSH_CONNECTION_PACKET_NONE;
	pSession->WriteOrdinal = 0u;
	return XSSH_OK;
}



/* 准备 peer connection 输入及其外部状态候选。 */
xsshcode xrtSshConnectionSessionReadPrepare(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshconnectionpacket* pPacket
)
{
	xsshconnectionpacket Packet;
	uint8 iMessage;
	bool bRecognized;
	xsshcode Code;

	if ( !xrtSshConnectionSessionActive(pSession) ||
		!xsshConnectionCoreValid(pSession, pCore) ||
		!pCore->Read.Active || pCore->Write.Active ||
		(pSession->WritePending != XSSH_CONNECTION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_CONNECTION_PACKET_NONE) ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_PEER
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pPacket,
			sizeof(*pPacket)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( Code != XSSH_OK ) {
		xsshConnectionSessionSetFailed(pSession);
		return XSSH_ERROR_PROTOCOL;
	}
	if ( iMessage != pCore->Read.Message ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshConnectionPacketRead(Payload, &Packet, &bRecognized);
	if ( Code != XSSH_OK ) {
		if ( bRecognized ) {
			xsshConnectionSessionSetFailed(pSession);
			return XSSH_ERROR_PROTOCOL;
		}
		return Code;
	}
	if ( xsshConnectionPacketHasChannel(Packet.Kind) ) {
		Code = xsshConnectionReadChannelPrepare(
			pSession,
			pCore,
			Payload,
			&Packet
		);
	} else if ( (Packet.Kind == XSSH_CONNECTION_PACKET_GLOBAL_SUCCESS) ||
		(Packet.Kind == XSSH_CONNECTION_PACKET_GLOBAL_FAILURE) ) {
		if ( !xsshConnectionQueueBorrowValid(
			pSession,
			pCore,
			Payload,
			NULL,
			pSession->GlobalReplies
		) ) {
			Code = XSSH_ERROR_PROTOCOL;
		} else {
			Code = xsshConnectionQueuePopPrepare(
				pSession,
				pSession->GlobalReplies,
				&Packet.ReplyToken
			);
			Packet.HasReplyToken = Code == XSSH_OK;
		}
	}
	if ( Code != XSSH_OK ) {
		xsshConnectionSessionSetFailed(pSession);
		return Code;
	}
	if ( pCore->State.PeerPackets == UINT64_MAX ) {
		xsshConnectionSessionSetFailed(pSession);
		return XSSH_ERROR_STATE;
	}
	pSession->ReadPending = Packet.Kind;
	pSession->ReadOrdinal = pCore->State.PeerPackets + 1u;
	*pPacket = Packet;
	return XSSH_OK;
}



/* 提交 peer connection 输入。 */
xsshcode xrtSshConnectionSessionReadCommit(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshcode Code;

	if ( !xrtSshConnectionSessionActive(pSession) ||
		(pSession->ReadPending == XSSH_CONNECTION_PACKET_NONE) ||
		!xsshConnectionCoreValid(pSession, pCore) || pCore->Read.Active ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_PEER
		) ||
		(pCore->State.PeerPackets != pSession->ReadOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshConnectionPendingCommit(pSession);
	if ( Code != XSSH_OK ) {
		xsshConnectionSessionSetFailed(pSession);
		return Code;
	}
	pSession->ReadPending = XSSH_CONNECTION_PACKET_NONE;
	pSession->ReadOrdinal = 0u;
	return XSSH_OK;
}



/* 放弃不可回滚的 peer 输入并终止会话。 */
xsshcode xrtSshConnectionSessionReadAbort(
	xsshconnectionsession* pSession
)
{
	if ( !xrtSshConnectionSessionActive(pSession) ||
		(pSession->ReadPending == XSSH_CONNECTION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	xsshConnectionSessionSetFailed(pSession);
	return XSSH_OK;
}



/* 显式终止 connection 会话。 */
void xrtSshConnectionSessionFail(xsshconnectionsession* pSession)
{
	if ( xsshConnectionSessionValid(pSession) && pSession->Active ) {
		xsshConnectionSessionSetFailed(pSession);
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/connection/ssh_channels.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CHANNELS)
#include <string.h>




#if defined(XSSH_FEATURE_CHANNELS)

#define XSSH_CHANNEL_GUARD UINT32_C(0x43484e4c)
#define XSSH_CHANNELS_GUARD UINT32_C(0x43484e53)



/* 设置当前执行上下文的参数错误。 */
static xsshcode xsshChannelsArgument(void)
{
	xrtSetErrorKind(XERR_ARGUMENT);
	return XSSH_ERROR_ARGUMENT;
}



/* 设置当前执行上下文的状态错误。 */
static xsshcode xsshChannelsState(void)
{
	xrtSetErrorKind(XERR_STATE);
	return XSSH_ERROR_STATE;
}



/* 把底层已经设置的容量或分配错误收窄为 SSH 空间错误。 */
static xsshcode xsshChannelsSpace(void)
{
	return XSSH_ERROR_SPACE;
}



/* 校验单个动态 channel 的拥有关系和固定字段。 */
static bool xsshChannelValid(const xsshchannel* pChannel)
{
	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (pChannel->Guard != XSSH_CHANNEL_GUARD) ||
		 !pChannel->Initialized || !pChannel->Core.Initialized ||
		 !pChannel->Io.Initialized ||
		 (pChannel->Io.Channel != &pChannel->Core) ||
		 (pChannel->Replies.Capacity != pChannel->ReplyCapacity) ||
		 (pChannel->Replies.Tokens != pChannel->ReplyTokens) ||
		 (pChannel->ReplyCapacity > pChannel->ReplyLimit) ||
		 ((pChannel->ReplyCapacity == 0u) !=
		  (pChannel->ReplyTokens == NULL)) ) {
		xrtSetErrorKind(XERR_STATE);
		return false;
	}
	return true;
}



/* 校验集合和默认配置没有被外部破坏。 */
static bool xsshChannelsValid(const xsshchannels* pChannels)
{
	if ( !xrtMemRangeValid(pChannels, sizeof(*pChannels)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (pChannels->Guard != XSSH_CHANNELS_GUARD) ||
		 !pChannels->Initialized ||
		 (pChannels->Config.MaxChannels == 0u) ||
		 (pChannels->Config.MaxChannels > UINT32_MAX) ||
		 (pChannels->Config.ReceiveWindow == 0u) ||
		 (pChannels->Config.ReceiveMaxPacket == 0u) ||
		 (pChannels->Config.AdjustThreshold == 0u) ||
		 (pChannels->Config.AdjustThreshold >
		  pChannels->Config.ReceiveWindow) ||
		 (pChannels->Config.Io.ReceiveLimit <
		  pChannels->Config.ReceiveWindow) ) {
		xrtSetErrorKind(XERR_STATE);
		return false;
	}
	return true;
}



/* 释放整数映射内联 channel 拥有的全部资源。 */
static void xsshChannelsDrop(int64 iKey, ptr pValue, ptr pUserData)
{
	xsshchannel* pChannel = (xsshchannel*)pValue;

	(void)iKey;
	(void)pUserData;
	if ( pChannel->Initialized ) {
		xrtSshChannelIoClear(&pChannel->Io);
		xrtSshChannelCoreClear(&pChannel->Core);
		xrtFree(pChannel->ReplyTokens);
	}
	memset(pChannel, 0, sizeof(*pChannel));
}



/* 从单调游标开始寻找当前未占用的 uint32 channel id。 */
static bool xsshChannelsLocal(
	xsshchannels* pChannels,
	uint32* pLocal
)
{
	size_t i;
	uint32 iLocal = pChannels->NextLocal;
	size_t iCount = xrtIntMapCount(&pChannels->Map);

	for ( i = 0u; i <= iCount; ++i ) {
		if ( xrtIntMapGet(&pChannels->Map, (int64)iLocal) == NULL ) {
			*pLocal = iLocal;
			pChannels->NextLocal = iLocal + 1u;
			return true;
		}
		iLocal++;
	}
	xrtSetErrorKind(XERR_RANGE);
	return false;
}



/* 在已经清零的映射值槽中建立 core、I/O 和空回复队列。 */
static xsshcode xsshChannelsChannelInit(
	xsshchannels* pChannels,
	xsshchannel* pChannel,
	uint32 iLocal,
	const xsshchannelopen* pOpen
)
{
	bool bCore;

	if ( pOpen == NULL ) {
		bCore = xrtSshChannelCoreOpenInit(
			&pChannel->Core,
			iLocal,
			pChannels->Config.ReceiveWindow,
			pChannels->Config.ReceiveMaxPacket,
			pChannels->Config.AdjustThreshold
		);
	} else {
		bCore = xrtSshChannelCoreAcceptInit(
			&pChannel->Core,
			iLocal,
			pOpen,
			pChannels->Config.ReceiveWindow,
			pChannels->Config.ReceiveMaxPacket,
			pChannels->Config.AdjustThreshold
		);
	}
	if ( !bCore ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtSshChannelIoInit(
		&pChannel->Io,
		pChannels->Pool,
		&pChannel->Core,
		&pChannels->Config.Io
	) ) {
		xrtSshChannelCoreClear(&pChannel->Core);
		return xsshChannelsSpace();
	}
	if ( !xrtSshReplyQueueInit(&pChannel->Replies, NULL, 0u) ) {
		xrtSshChannelIoClear(&pChannel->Io);
		xrtSshChannelCoreClear(&pChannel->Core);
		return XSSH_ERROR_STATE;
	}
	pChannel->Incoming = pOpen != NULL;
	pChannel->ReplyLimit = pChannels->Config.ReplyLimit;
	pChannel->Initialized = true;
	pChannel->Guard = XSSH_CHANNEL_GUARD;
	return XSSH_OK;
}



/* 创建本端或对端发起的 channel，并保持失败时映射不留空槽。 */
static xsshcode xsshChannelsAdd(
	xsshchannels* pChannels,
	const xsshchannelopen* pOpen,
	xsshchannel** ppChannel
)
{
	xsshchannel* pChannel;
	xsshcode Code;
	uint32 iLocal;
	bool bNew = false;

	if ( ppChannel == NULL ) {
		return xsshChannelsArgument();
	}
	*ppChannel = NULL;
	if ( !xsshChannelsValid(pChannels) ) {
		return XSSH_ERROR_STATE;
	}
	if ( xrtIntMapCount(&pChannels->Map) >=
		 pChannels->Config.MaxChannels ) {
		xrtSetErrorKind(XERR_RANGE);
		return XSSH_ERROR_SPACE;
	}
	if ( !xsshChannelsLocal(pChannels, &iLocal) ) {
		return XSSH_ERROR_SPACE;
	}
	pChannel = (xsshchannel*)xrtIntMapGetOrAdd(
		&pChannels->Map,
		(int64)iLocal,
		&bNew
	);
	if ( (pChannel == NULL) || !bNew ) {
		return xsshChannelsSpace();
	}
	Code = xsshChannelsChannelInit(
		pChannels,
		pChannel,
		iLocal,
		pOpen
	);
	if ( Code != XSSH_OK ) {
		(void)xrtIntMapRemove(&pChannels->Map, (int64)iLocal);
		return Code;
	}
	*ppChannel = pChannel;
	return XSSH_OK;
}



/* 写入不会预分配 channel 或 reply token 的默认预算。 */
XRT_API void xrtSshChannelsConfigInit(xsshchannelsconfig* pConfig)
{
	if ( pConfig == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->MaxChannels = XSSH_CHANNELS_MAX_DEFAULT;
	pConfig->ReplyLimit = XSSH_CHANNELS_REPLY_LIMIT_DEFAULT;
	pConfig->ReceiveWindow = XSSH_CHANNELS_WINDOW_DEFAULT;
	pConfig->ReceiveMaxPacket = XSSH_CHANNELS_PACKET_DEFAULT;
	pConfig->AdjustThreshold = XSSH_CHANNELS_ADJUST_DEFAULT;
	xrtSshChannelIoConfigInit(&pConfig->Io);
}



/* 初始化拥有式整数映射，channel 节点地址在删除前保持稳定。 */
XRT_API bool xrtSshChannelsInit(
	xsshchannels* pChannels,
	xnetbufpool* pPool,
	const xsshchannelsconfig* pConfig
)
{
	xsshchannelsconfig Config;

	if ( pChannels == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pConfig == NULL ) {
		xrtSshChannelsConfigInit(&Config);
		pConfig = &Config;
	}
	if ( (pConfig->MaxChannels == 0u) ||
		 (pConfig->MaxChannels > UINT32_MAX) ||
		 (pConfig->ReceiveWindow == 0u) ||
		 (pConfig->ReceiveMaxPacket == 0u) ||
		 (pConfig->AdjustThreshold == 0u) ||
		 (pConfig->AdjustThreshold > pConfig->ReceiveWindow) ||
		 (pConfig->Io.ReceiveLimit < pConfig->ReceiveWindow) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memset(pChannels, 0, sizeof(*pChannels));
	if ( !xrtIntMapInit(&pChannels->Map, sizeof(xsshchannel)) ) {
		return false;
	}
	if ( !xrtIntMapSetDrop(&pChannels->Map, xsshChannelsDrop, NULL) ) {
		xrtIntMapUnit(&pChannels->Map);
		memset(pChannels, 0, sizeof(*pChannels));
		return false;
	}
	pChannels->Config = *pConfig;
	pChannels->Pool = pPool;
	pChannels->Initialized = true;
	pChannels->Guard = XSSH_CHANNELS_GUARD;
	return true;
}



/* 删除观察器属于组合层钩子，不改变集合自身的所有权规则。 */
XRT_API bool xrtSshChannelsOnRemoved(
	xsshchannels* pChannels,
	xsshchannelsremovedproc pRemoved,
	ptr pUserData
)
{
	if ( !xsshChannelsValid(pChannels) ) {
		return false;
	}
	pChannels->Removed = pRemoved;
	pChannels->RemovedData = pRemoved != NULL ? pUserData : NULL;
	return true;
}



/* 清理映射会逐项释放动态 I/O 和 reply token。 */
XRT_API void xrtSshChannelsClear(xsshchannels* pChannels)
{
	if ( !xsshChannelsValid(pChannels) ) {
		return;
	}
	xrtIntMapUnit(&pChannels->Map);
	memset(pChannels, 0, sizeof(*pChannels));
}



/* 返回活动节点数。 */
XRT_API size_t xrtSshChannelsCount(const xsshchannels* pChannels)
{
	if ( !xsshChannelsValid(pChannels) ) {
		return 0u;
	}
	return xrtIntMapCount(&pChannels->Map);
}



/* 创建本端发起的 channel。 */
XRT_API xsshcode xrtSshChannelsOpen(
	xsshchannels* pChannels,
	xsshchannel** ppChannel
)
{
	return xsshChannelsAdd(pChannels, NULL, ppChannel);
}



/* 创建对端发起的 channel。 */
XRT_API xsshcode xrtSshChannelsAccept(
	xsshchannels* pChannels,
	const xsshchannelopen* pOpen,
	xsshchannel** ppChannel
)
{
	if ( pOpen == NULL ) {
		if ( ppChannel != NULL ) {
			*ppChannel = NULL;
		}
		return xsshChannelsArgument();
	}
	return xsshChannelsAdd(pChannels, pOpen, ppChannel);
}



/* 按线路 recipient 查询可写 channel。 */
XRT_API xsshchannel* xrtSshChannelsGet(
	xsshchannels* pChannels,
	uint32 iLocal
)
{
	if ( !xsshChannelsValid(pChannels) ) {
		return NULL;
	}
	return (xsshchannel*)xrtIntMapGet(&pChannels->Map, (int64)iLocal);
}



/* 按线路 recipient 查询只读 channel。 */
XRT_API const xsshchannel* xrtSshChannelsConstGet(
	const xsshchannels* pChannels,
	uint32 iLocal
)
{
	if ( !xsshChannelsValid(pChannels) ) {
		return NULL;
	}
	return (const xsshchannel*)xrtIntMapConstGet(
		&pChannels->Map,
		(int64)iLocal
	);
}



/* 使用非重叠新存储迁移回复队列，避免 realloc 后留下悬空借用。 */
XRT_API xsshcode xrtSshChannelReplyReserve(
	xsshchannel* pChannel,
	size_t iCapacity
)
{
	uint64* pTokens;
	size_t iNewCapacity;
	xsshcode Code;

	if ( !xsshChannelValid(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	if ( iCapacity > pChannel->ReplyLimit ) {
		xrtSetErrorKind(XERR_RANGE);
		return XSSH_ERROR_SPACE;
	}
	if ( iCapacity <= pChannel->ReplyCapacity ) {
		return XSSH_OK;
	}
	iNewCapacity = pChannel->ReplyCapacity != 0u ?
		pChannel->ReplyCapacity : 4u;
	while ( iNewCapacity < iCapacity ) {
		if ( iNewCapacity > (SIZE_MAX / 2u) ) {
			iNewCapacity = iCapacity;
			break;
		}
		iNewCapacity *= 2u;
	}
	if ( iNewCapacity > pChannel->ReplyLimit ) {
		iNewCapacity = pChannel->ReplyLimit;
	}
	if ( iNewCapacity > (SIZE_MAX / sizeof(uint64)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return XSSH_ERROR_OVERFLOW;
	}
	pTokens = (uint64*)xrtMalloc(iNewCapacity * sizeof(uint64));
	if ( pTokens == NULL ) {
		return xsshChannelsSpace();
	}
	Code = xrtSshReplyQueueRebind(
		&pChannel->Replies,
		pTokens,
		iNewCapacity
	);
	if ( Code != XSSH_OK ) {
		xrtFree(pTokens);
		return Code;
	}
	xrtFree(pChannel->ReplyTokens);
	pChannel->ReplyTokens = pTokens;
	pChannel->ReplyCapacity = iNewCapacity;
	return XSSH_OK;
}



/* 检查结束状态和所有可观察数据都已消费。 */
XRT_API bool xrtSshChannelsRemove(
	xsshchannels* pChannels,
	uint32 iLocal
)
{
	xsshchannel* pChannel;
	xsshchannelsremovedproc pRemoved;
	ptr pRemovedData;
	xsshchannelcorephase Phase;

	if ( !xsshChannelsValid(pChannels) ) {
		return false;
	}
	pChannel = (xsshchannel*)xrtIntMapGet(
		&pChannels->Map,
		(int64)iLocal
	);
	if ( pChannel == NULL ) {
		xrtSetErrorKind(XERR_NOT_FOUND);
		return false;
	}
	if ( !xsshChannelValid(pChannel) ) {
		return false;
	}
	Phase = xrtSshChannelCorePhase(&pChannel->Core);
	if ( ((Phase != XSSH_CHANNEL_CORE_FAILED) &&
		  (Phase != XSSH_CHANNEL_CORE_CLOSED)) ||
		 (pChannel->Io.Pending != XSSH_CHANNEL_IO_PENDING_NONE) ||
		 (xrtSshReplyQueueCount(&pChannel->Replies) != 0u) ||
		 (xrtSshChannelIoReadable(
			&pChannel->Io,
			XSSH_CHANNEL_IO_DATA
		 ) != 0u) ||
		 (xrtSshChannelIoReadable(
			&pChannel->Io,
			XSSH_CHANNEL_IO_STDERR
		 ) != 0u) ||
		 (xrtSshChannelIoQueued(
			&pChannel->Io,
			XSSH_CHANNEL_IO_DATA
		 ) != 0u) ||
		 (xrtSshChannelIoQueued(
			&pChannel->Io,
			XSSH_CHANNEL_IO_STDERR
		 ) != 0u) ) {
		(void)xsshChannelsState();
		return false;
	}
	pRemoved = pChannels->Removed;
	pRemovedData = pChannels->RemovedData;
	if ( !xrtIntMapRemove(&pChannels->Map, (int64)iLocal) ) {
		return false;
	}
	if ( pRemoved != NULL ) {
		pRemoved(pChannels, iLocal, pRemovedData);
	}
	return true;
}



/* 强制删除仍活动或仍有应用数据的 channel。 */
XRT_API bool xrtSshChannelsDiscard(
	xsshchannels* pChannels,
	uint32 iLocal
)
{
	xsshchannelsremovedproc pRemoved;
	ptr pRemovedData;

	if ( !xsshChannelsValid(pChannels) ) {
		return false;
	}
	if ( !xrtIntMapHas(&pChannels->Map, (int64)iLocal) ) {
		xrtSetErrorKind(XERR_NOT_FOUND);
		return false;
	}
	pRemoved = pChannels->Removed;
	pRemovedData = pChannels->RemovedData;
	if ( !xrtIntMapRemove(&pChannels->Map, (int64)iLocal) ) {
		return false;
	}
	if ( pRemoved != NULL ) {
		pRemoved(pChannels, iLocal, pRemovedData);
	}
	return true;
}



/* 把集合查询适配为 connection session 的无拥有权 resolver。 */
XRT_API bool xrtSshChannelsResolve(
	ptr pUserData,
	uint32 iLocal,
	xsshchannelcore** ppChannel,
	xsshreplyqueue** ppReplies
)
{
	xsshchannel* pChannel;

	if ( (ppChannel == NULL) || (ppReplies == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	*ppChannel = NULL;
	*ppReplies = NULL;
	pChannel = xrtSshChannelsGet((xsshchannels*)pUserData, iLocal);
	if ( (pChannel == NULL) || !xsshChannelValid(pChannel) ) {
		return false;
	}
	*ppChannel = &pChannel->Core;
	*ppReplies = &pChannel->Replies;
	return true;
}



/* 启动稳定地址节点的升序遍历。 */
XRT_API bool xrtSshChannelsIterBegin(
	xsshchannels* pChannels,
	xsshchannelsiter* pIterator
)
{
	if ( !xsshChannelsValid(pChannels) || (pIterator == NULL) ) {
		if ( pIterator == NULL ) {
			xrtSetErrorKind(XERR_ARGUMENT);
		}
		return false;
	}
	memset(pIterator, 0, sizeof(*pIterator));
	if ( !xrtIntMapIterBegin(&pChannels->Map, &pIterator->Base) ) {
		return false;
	}
	pIterator->Active = true;
	return true;
}



/* 读取下一节点并安全收窄非负 uint32 键。 */
XRT_API xsshchannel* xrtSshChannelsIterNext(
	xsshchannelsiter* pIterator,
	uint32* pLocal
)
{
	xsshchannel* pChannel;
	int64 iKey = 0;

	if ( (pIterator == NULL) || !pIterator->Active ) {
		xrtSetErrorKind(XERR_STATE);
		return NULL;
	}
	pChannel = (xsshchannel*)xrtIntMapIterNext(
		&pIterator->Base,
		&iKey
	);
	if ( pChannel == NULL ) {
		return NULL;
	}
	if ( (iKey < 0) || (iKey > UINT32_MAX) ||
		 !xsshChannelValid(pChannel) ) {
		(void)xsshChannelsState();
		return NULL;
	}
	if ( pLocal != NULL ) {
		*pLocal = (uint32)iKey;
	}
	return pChannel;
}



/* 结束外置迭代。 */
XRT_API void xrtSshChannelsIterEnd(xsshchannelsiter* pIterator)
{
	if ( (pIterator == NULL) || !pIterator->Active ) {
		xrtSetErrorKind(XERR_STATE);
		return;
	}
	xrtIntMapIterEnd(&pIterator->Base);
	memset(pIterator, 0, sizeof(*pIterator));
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_session_core.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_SESSION_CORE)
#include <math.h>
#include <string.h>




#if defined(XSSH_FEATURE_SESSION_CORE)

#define XSSH_SESSION_CORE_GUARD UINT32_C(0x53455353)



/* 校验连接级对象的固定字段和未决事务枚举。 */
static bool xsshSessionCoreValid(const xsshsessioncore* pSession)
{
	return xrtMemRangeValid(pSession, sizeof(*pSession)) &&
		pSession->Initialized &&
		(pSession->Guard == XSSH_SESSION_CORE_GUARD) &&
		((pSession->Role == XSSH_ROLE_CLIENT) ||
		 (pSession->Role == XSSH_ROLE_SERVER)) &&
		(pSession->WritePending >= XSSH_SESSION_PACKET_NONE) &&
		(pSession->WritePending <= XSSH_SESSION_PACKET_EXTENSION) &&
		(pSession->ReadPending >= XSSH_SESSION_PACKET_NONE) &&
		(pSession->ReadPending <= XSSH_SESSION_PACKET_EXTENSION);
}



/* 校验 transport 的角色和地址边界。 */
static bool xsshSessionCoreTransportValid(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) && (pCore->State.Role == pSession->Role) &&
		(pCore->State.Phase >= XSSH_TRANSPORT_IDENTIFICATION) &&
		(pCore->State.Phase <= XSSH_TRANSPORT_CLOSED);
}



/* 判断任一子状态已经进入不可继续状态。 */
static bool xsshSessionCoreChildFailed(const xsshsessioncore* pSession)
{
	return (pSession->Kex.Phase == XSSH_KEX_EXCHANGE_FAILED) ||
		(pSession->Auth.Phase == XSSH_AUTH_SESSION_FAILED) ||
		pSession->Connection.Failed;
}



/* 清除本端未决事务描述。 */
static void xsshSessionCoreWriteClear(xsshsessioncore* pSession)
{
	pSession->WritePayload = (xbytesview){ NULL, 0u };
	pSession->WriteOrdinal = 0u;
	pSession->WritePending = XSSH_SESSION_PACKET_NONE;
	pSession->WriteMessage = 0u;
	pSession->WriteBound = false;
}



/* 清除对端未决事务描述。 */
static void xsshSessionCoreReadClear(xsshsessioncore* pSession)
{
	pSession->ReadOrdinal = 0u;
	pSession->ReadPending = XSSH_SESSION_PACKET_NONE;
	pSession->ReadMessage = 0u;
}



/* 将已知 transport 控制 payload 严格解析为借用视图。 */
static xsshcode xsshSessionCoreTransportPacket(
	xbytesview Payload,
	uint8 iMessage,
	xsshsessionpacket* pPacket,
	bool* pRecognized
)
{
	xsshcode Code;

	*pRecognized = true;
	if ( iMessage == XSSH_MSG_DISCONNECT ) {
		pPacket->Kind = XSSH_SESSION_PACKET_DISCONNECT;
		Code = xrtSshDisconnectRead(
			Payload,
			&pPacket->Message.Disconnect
		);
	} else if ( iMessage == XSSH_MSG_IGNORE ) {
		pPacket->Kind = XSSH_SESSION_PACKET_IGNORE;
		Code = xrtSshIgnoreRead(Payload, &pPacket->Message.Ignore);
	} else if ( iMessage == XSSH_MSG_UNIMPLEMENTED ) {
		pPacket->Kind = XSSH_SESSION_PACKET_UNIMPLEMENTED;
		Code = xrtSshUnimplementedRead(
			Payload,
			&pPacket->Message.UnimplementedSequence
		);
	} else if ( iMessage == XSSH_MSG_DEBUG ) {
		pPacket->Kind = XSSH_SESSION_PACKET_DEBUG;
		Code = xrtSshDebugRead(Payload, &pPacket->Message.Debug);
	} else if ( iMessage == XSSH_MSG_EXT_INFO ) {
		pPacket->Kind = XSSH_SESSION_PACKET_EXT_INFO;
		Code = xrtSshExtInfoRead(Payload, &pPacket->Message.ExtInfo);
	} else if ( iMessage == XSSH_MSG_NEWCOMPRESS ) {
		pPacket->Kind = XSSH_SESSION_PACKET_NEWCOMPRESS;
		Code = xrtSshNewCompressRead(Payload);
	} else {
		*pRecognized = false;
		pPacket->Kind = XSSH_SESSION_PACKET_EXTENSION;
		Code = XSSH_OK;
	}
	return Code;
}



/* 判断消息号属于标准认证层范围。 */
static bool xsshSessionCoreAuthNumber(uint8 iMessage)
{
	return (iMessage == XSSH_MSG_SERVICE_REQUEST) ||
		(iMessage == XSSH_MSG_SERVICE_ACCEPT) ||
		((iMessage >= XSSH_MSG_USERAUTH_REQUEST) && (iMessage <= 79u));
}



/* 判断消息号属于 RFC 4254 已分配的 connection 范围。 */
static bool xsshSessionCoreConnectionNumber(uint8 iMessage)
{
	return ((iMessage >= XSSH_MSG_GLOBAL_REQUEST) &&
		(iMessage <= XSSH_MSG_REQUEST_FAILURE)) ||
		((iMessage >= XSSH_MSG_CHANNEL_OPEN) &&
		 (iMessage <= XSSH_MSG_CHANNEL_FAILURE));
}



/* 校验 KEX 方法构建器的未决输出与最终 payload 一致。 */
static xsshcode xsshSessionCoreKexWriteCheck(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	uint8 iMessage
)
{
	const xsshkexsession* pKex = xrtSshKexExchangeSessionConst(
		&pSession->Kex
	);
	xsshkexsessionpacket Packet;
	xsshcode Code;

	if ( pKex == NULL || !pKex->Active ) {
		return XSSH_ERROR_STATE;
	}
	Packet = pKex->WritePending;
	if ( ((Packet == XSSH_KEX_PACKET_ECDH_INIT) &&
		 (iMessage != XSSH_MSG_KEX_ECDH_INIT)) ||
		((Packet == XSSH_KEX_PACKET_ECDH_REPLY) &&
		 (iMessage != XSSH_MSG_KEX_ECDH_REPLY)) ||
		((Packet == XSSH_KEX_PACKET_NEWKEYS) &&
		 (iMessage != XSSH_MSG_NEWKEYS)) ||
		((Packet != XSSH_KEX_PACKET_ECDH_INIT) &&
		 (Packet != XSSH_KEX_PACKET_ECDH_REPLY) &&
		 (Packet != XSSH_KEX_PACKET_NEWKEYS)) ) {
		return XSSH_ERROR_STATE;
	}
	Code = iMessage == XSSH_MSG_NEWKEYS ?
		xrtSshTransportNewKeysCheck(
			&pCore->State,
			XSSH_TRANSPORT_LOCAL
		) : xrtSshTransportMessageCheck(
			&pCore->State,
			XSSH_TRANSPORT_LOCAL,
			iMessage
		);
	return Code;
}



/* 完成方向性密钥激活后收口本代动态 KEX transcript。 */
static xsshcode xsshSessionCoreKexFinish(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore
)
{
	if ( !xrtSshKexSessionComplete(&pSession->Kex.Session, pCore) ) {
		return XSSH_OK;
	}
	return xrtSshKexExchangeComplete(&pSession->Kex, pCore);
}



/* 认证完成时自动开放 connection 层，避免两个状态机之间存在空窗。 */
static xsshcode xsshSessionCoreConnectionStart(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	if ( !xrtSshAuthSessionComplete(&pSession->Auth, pCore) ||
		xrtSshConnectionSessionActive(&pSession->Connection) ) {
		return XSSH_OK;
	}
	return xrtSshConnectionSessionBegin(&pSession->Connection, pCore);
}



/* 把 KEX 子状态的动作映射到连接级动作。 */
static xsshsessionaction xsshSessionCoreKexAction(
	const xsshkexsession* pKex
)
{
	switch ( xrtSshKexSessionEvent(pKex) ) {
		case XSSH_KEX_EVENT_WRITE_ECDH_INIT:
			return XSSH_SESSION_ACTION_WRITE_ECDH_INIT;
		case XSSH_KEX_EVENT_READ_ECDH_INIT:
			return XSSH_SESSION_ACTION_READ_ECDH_INIT;
		case XSSH_KEX_EVENT_WRITE_ECDH_REPLY:
			return XSSH_SESSION_ACTION_WRITE_ECDH_REPLY;
		case XSSH_KEX_EVENT_READ_ECDH_REPLY:
			return XSSH_SESSION_ACTION_READ_ECDH_REPLY;
		case XSSH_KEX_EVENT_VERIFY_HOST_KEY:
			return XSSH_SESSION_ACTION_VERIFY_HOST_KEY;
		case XSSH_KEX_EVENT_WRITE_NEWKEYS:
			return XSSH_SESSION_ACTION_WRITE_NEWKEYS;
		case XSSH_KEX_EVENT_READ_NEWKEYS:
			return XSSH_SESSION_ACTION_READ_NEWKEYS;
		case XSSH_KEX_EVENT_ACTIVATE_WRITE:
			return XSSH_SESSION_ACTION_ACTIVATE_WRITE_KEYS;
		case XSSH_KEX_EVENT_ACTIVATE_READ:
			return XSSH_SESSION_ACTION_ACTIVATE_READ_KEYS;
		case XSSH_KEX_EVENT_COMPLETE:
			return XSSH_SESSION_ACTION_COMPLETE_KEX;
		case XSSH_KEX_EVENT_FAILED:
			return XSSH_SESSION_ACTION_FAILED;
		default:
			return XSSH_SESSION_ACTION_NONE;
	}
}



/* 把 USERAUTH 子状态的动作映射到连接级动作。 */
static xsshsessionaction xsshSessionCoreAuthAction(
	const xsshauthsession* pAuth
)
{
	switch ( xrtSshAuthSessionEvent(pAuth) ) {
		case XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST:
			return XSSH_SESSION_ACTION_WRITE_SERVICE_REQUEST;
		case XSSH_AUTH_SESSION_EVENT_READ_SERVICE_REQUEST:
			return XSSH_SESSION_ACTION_READ_SERVICE_REQUEST;
		case XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_ACCEPT:
			return XSSH_SESSION_ACTION_WRITE_SERVICE_ACCEPT;
		case XSSH_AUTH_SESSION_EVENT_READ_SERVICE_ACCEPT:
			return XSSH_SESSION_ACTION_READ_SERVICE_ACCEPT;
		case XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST:
			return XSSH_SESSION_ACTION_WRITE_AUTH_REQUEST;
		case XSSH_AUTH_SESSION_EVENT_READ_REQUEST:
			return XSSH_SESSION_ACTION_READ_AUTH_REQUEST;
		case XSSH_AUTH_SESSION_EVENT_WRITE_RESULT:
			return XSSH_SESSION_ACTION_WRITE_AUTH_RESULT;
		case XSSH_AUTH_SESSION_EVENT_READ_RESULT:
			return XSSH_SESSION_ACTION_READ_AUTH_RESULT;
		case XSSH_AUTH_SESSION_EVENT_COMPLETE:
			return XSSH_SESSION_ACTION_COMPLETE_AUTH;
		case XSSH_AUTH_SESSION_EVENT_FAILED:
			return XSSH_SESSION_ACTION_FAILED;
		default:
			return XSSH_SESSION_ACTION_NONE;
	}
}



/* 初始化连接级协议所有权。 */
bool xrtSshSessionCoreInit(
	xsshsessioncore* pSession,
	xnetbufpool* pPool,
	xsshrole Role,
	xsshchannelresolveproc pResolve,
	ptr pUserData,
	xsshreplyqueue* pGlobalReplies
)
{
	xsshsessioncore Session;
	size_t iTokenBytes = 0u;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	if ( pGlobalReplies != NULL ) {
		if ( !xrtMemRangeValid(
			pGlobalReplies,
			sizeof(*pGlobalReplies)
		) || (pGlobalReplies->Capacity >
			(SIZE_MAX / sizeof(uint64))) ) {
			return false;
		}
		iTokenBytes = pGlobalReplies->Capacity * sizeof(uint64);
		if ( xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pGlobalReplies,
			sizeof(*pGlobalReplies)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pGlobalReplies->Tokens,
			iTokenBytes
		) ) {
			return false;
		}
	}
	memset(&Session, 0, sizeof(Session));
	if ( !xrtSshKexExchangeInit(&Session.Kex, pPool, Role) ) {
		return false;
	}
	if ( !xrtSshAuthSessionInit(&Session.Auth, Role) ||
		!xrtSshConnectionSessionInit(
			&Session.Connection,
			Role,
			pResolve,
			pUserData,
			pGlobalReplies
		) ) {
		xrtSshKexExchangeClear(&Session.Kex);
		return false;
	}
	Session.Role = Role;
	Session.Initialized = true;
	Session.Guard = XSSH_SESSION_CORE_GUARD;
	*pSession = Session;
	return true;
}



/* 清除全部内部状态和动态 transcript。 */
void xrtSshSessionCoreClear(xsshsessioncore* pSession)
{
	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ) {
		return;
	}
	if ( xsshSessionCoreValid(pSession) ) {
		xrtSshKexExchangeClear(&pSession->Kex);
		xrtSshAuthSessionClear(&pSession->Auth);
		xrtSshConnectionSessionClear(&pSession->Connection);
	}
	memset(pSession, 0, sizeof(*pSession));
}



/* 从子状态和 transport 推导连接级阶段。 */
xsshsessionphase xrtSshSessionCorePhase(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	if ( !xsshSessionCoreValid(pSession) ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		pSession->Failed || xsshSessionCoreChildFailed(pSession) ) {
		return XSSH_SESSION_FAILED;
	}
	if ( (pCore->State.Phase == XSSH_TRANSPORT_CLOSING) ||
		(pCore->State.Phase == XSSH_TRANSPORT_CLOSED) ) {
		return XSSH_SESSION_CLOSING;
	}
	if ( pSession->Kex.Phase == XSSH_KEX_EXCHANGE_IDENTIFICATION ) {
		return XSSH_SESSION_IDENTIFICATION;
	}
	if ( (pSession->Kex.Phase != XSSH_KEX_EXCHANGE_COMPLETE) ||
		(pCore->State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE) ) {
		return pSession->Auth.Active ?
			XSSH_SESSION_REKEY : XSSH_SESSION_KEY_EXCHANGE;
	}
	if ( !xrtSshAuthSessionComplete(&pSession->Auth, pCore) ) {
		return XSSH_SESSION_AUTHENTICATION;
	}
	return XSSH_SESSION_CONNECTION;
}



/* 从所有子层推导唯一的常见驱动动作，不推进任何事务。 */
xsshsessionaction xrtSshSessionCoreAction(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	xsshsessionphase Phase;

	Phase = xrtSshSessionCorePhase(pSession, pCore);
	if ( Phase == XSSH_SESSION_FAILED ) {
		return XSSH_SESSION_ACTION_FAILED;
	}
	if ( Phase == XSSH_SESSION_CLOSING ) {
		return XSSH_SESSION_ACTION_CLOSING;
	}
	if ( (pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		((pSession->Kex.Pending != XSSH_KEX_EXCHANGE_PENDING_NONE) &&
		 (pSession->Kex.PendingDirection == XSSH_TRANSPORT_LOCAL)) ) {
		return XSSH_SESSION_ACTION_WRITE_PENDING;
	}
	if ( (pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ||
		((pSession->Kex.Pending != XSSH_KEX_EXCHANGE_PENDING_NONE) &&
		 (pSession->Kex.PendingDirection == XSSH_TRANSPORT_PEER)) ) {
		return XSSH_SESSION_ACTION_READ_PENDING;
	}
	if ( Phase == XSSH_SESSION_IDENTIFICATION ) {
		return !pCore->State.LocalIdentification ?
			XSSH_SESSION_ACTION_WRITE_IDENTIFICATION :
			XSSH_SESSION_ACTION_READ_IDENTIFICATION;
	}
	if ( (pSession->Kex.Phase == XSSH_KEX_EXCHANGE_KEXINIT) ||
		((pSession->Kex.Phase == XSSH_KEX_EXCHANGE_COMPLETE) &&
		 (pCore->State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE)) ) {
		return !pCore->State.LocalKexInit ?
			XSSH_SESSION_ACTION_WRITE_KEXINIT :
			XSSH_SESSION_ACTION_READ_KEXINIT;
	}
	if ( pSession->Kex.Phase == XSSH_KEX_EXCHANGE_READY ) {
		return XSSH_SESSION_ACTION_BEGIN_KEX;
	}
	if ( pSession->Kex.Phase == XSSH_KEX_EXCHANGE_METHOD ) {
		return xsshSessionCoreKexAction(&pSession->Kex.Session);
	}
	if ( !pSession->Auth.Active ) {
		return XSSH_SESSION_ACTION_BEGIN_AUTH;
	}
	if ( !xrtSshAuthSessionComplete(&pSession->Auth, pCore) ) {
		return xsshSessionCoreAuthAction(&pSession->Auth);
	}
	if ( xrtSshConnectionSessionActive(&pSession->Connection) ) {
		return XSSH_SESSION_ACTION_CONNECTION;
	}
	return XSSH_SESSION_ACTION_NONE;
}



/* 返回可变 KEX 子对象。 */
xsshkexexchange* xrtSshSessionCoreKex(xsshsessioncore* pSession)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Kex : NULL;
}



/* 返回只读 KEX 子对象。 */
const xsshkexexchange* xrtSshSessionCoreKexConst(
	const xsshsessioncore* pSession
)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Kex : NULL;
}



/* 返回可变认证子对象。 */
xsshauthsession* xrtSshSessionCoreAuth(xsshsessioncore* pSession)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Auth : NULL;
}



/* 返回只读认证子对象。 */
const xsshauthsession* xrtSshSessionCoreAuthConst(
	const xsshsessioncore* pSession
)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Auth : NULL;
}



/* 返回可变 connection 子对象。 */
xsshconnectionsession* xrtSshSessionCoreConnection(
	xsshsessioncore* pSession
)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Connection : NULL;
}



/* 返回只读 connection 子对象。 */
const xsshconnectionsession* xrtSshSessionCoreConnectionConst(
	const xsshsessioncore* pSession
)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Connection : NULL;
}



/* 保存一条尚未提交的 identification。 */
xsshcode xrtSshSessionCoreVersionPrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xstrview Version
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshKexExchangeVersionPrepare(
		&pSession->Kex,
		pCore,
		Direction,
		Version
	);
}



/* 发布 transport 已提交的 identification。 */
xsshcode xrtSshSessionCoreVersionCommit(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshKexExchangeVersionCommit(&pSession->Kex, pCore);
}



/* 放弃尚未提交的 identification。 */
xsshcode xrtSshSessionCoreVersionAbort(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshKexExchangeVersionAbort(&pSession->Kex, pCore);
}



/* 以确定性私钥开始当前 KEX。 */
xsshcode xrtSshSessionCoreKexBeginWithPrivate(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshKexExchangeBeginWithPrivate(
		&pSession->Kex,
		pCore,
		ServerHostKey,
		PrivateKey
	);
}



/* 首轮密钥交换完成后开始认证。 */
xsshcode xrtSshSessionCoreAuthBegin(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	const xsshauthguardpolicy* pPolicy,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->Kex.Phase != XSSH_KEX_EXCHANGE_COMPLETE) ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshAuthSessionBegin(
		&pSession->Auth,
		pCore,
		pPolicy,
		Timer
	);
}



/* 在 transport 之前准备本端协议事务。 */
xsshcode xrtSshSessionCoreWritePrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	double Timer,
	xsshsessionpacketkind* pKind
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshsessionpacket Packet;
	bool bRecognized;
	uint8 iMessage;
	xsshcode Code;

	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		pCore->Write.Active || pCore->Read.Active ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ||
		(pCore->State.LocalPackets == UINT64_MAX) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		!xrtMemRangeValid(pKind, sizeof(*pKind)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pKind,
			sizeof(*pKind)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pKind,
			sizeof(*pKind)
		) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pKind,
			sizeof(*pKind)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	memset(&Packet, 0, sizeof(Packet));
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((pChannel != NULL) || (pReplies != NULL)) &&
		!xsshSessionCoreConnectionNumber(iMessage) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iMessage == XSSH_MSG_KEXINIT ) {
		Code = xrtSshKexExchangeKexInitPrepare(
			&pSession->Kex,
			pCore,
			XSSH_TRANSPORT_LOCAL,
			Payload
		);
		Packet.Kind = XSSH_SESSION_PACKET_KEXINIT;
	} else if ( (pCore->State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		((iMessage == XSSH_MSG_NEWKEYS) ||
		 ((iMessage >= XSSH_KEX_METHOD_MIN) &&
		  (iMessage <= XSSH_KEX_METHOD_MAX))) ) {
		Code = xsshSessionCoreKexWriteCheck(pSession, pCore, iMessage);
		Packet.Kind = XSSH_SESSION_PACKET_KEX;
	} else {
		Code = xsshSessionCoreTransportPacket(
			Payload,
			iMessage,
			&Packet,
			&bRecognized
		);
		if ( (Code == XSSH_OK) && bRecognized ) {
			Code = xrtSshTransportMessageCheck(
				&pCore->State,
				XSSH_TRANSPORT_LOCAL,
				iMessage
			);
		} else if ( Code == XSSH_OK && pSession->Auth.Active &&
			(pSession->Auth.Phase != XSSH_AUTH_SESSION_COMPLETE) ) {
			Code = xrtSshAuthSessionWritePrepare(
				&pSession->Auth,
				pCore,
				Payload,
				Timer
			);
			if ( Code == XSSH_OK ) {
				Packet.Kind = XSSH_SESSION_PACKET_AUTH;
			} else if ( Code == XSSH_ERROR_UNSUPPORTED ) {
				Code = XSSH_OK;
			}
		} else if ( Code == XSSH_OK &&
			xrtSshConnectionSessionActive(&pSession->Connection) ) {
			Code = xrtSshConnectionSessionWritePrepare(
				&pSession->Connection,
				pCore,
				Payload,
				pChannel,
				pReplies,
				iReplyToken
			);
			if ( Code == XSSH_OK ) {
				Packet.Kind = XSSH_SESSION_PACKET_CONNECTION;
			} else if ( Code == XSSH_ERROR_UNSUPPORTED ) {
				Code = XSSH_OK;
			}
		}
		if ( (Code == XSSH_OK) &&
			(Packet.Kind == XSSH_SESSION_PACKET_EXTENSION) ) {
			if ( xsshSessionCoreAuthNumber(iMessage) ||
				xsshSessionCoreConnectionNumber(iMessage) ) {
				return XSSH_ERROR_STATE;
			}
			if ( (pChannel != NULL) || (pReplies != NULL) ) {
				return XSSH_ERROR_ARGUMENT;
			}
			Code = xrtSshTransportMessageCheck(
				&pCore->State,
				XSSH_TRANSPORT_LOCAL,
				iMessage
			);
		}
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->WriteOrdinal = pCore->State.LocalPackets + 1u;
	pSession->WritePayload = Payload;
	pSession->WritePending = Packet.Kind;
	pSession->WriteMessage = iMessage;
	*pKind = Packet.Kind;
	return XSSH_OK;
}



/* 把上层候选绑定到 transport 当前未决 packet。 */
xsshcode xrtSshSessionCoreWriteBind(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->WritePending == XSSH_SESSION_PACKET_NONE) ||
		pSession->WriteBound || !pCore->Write.Active || pCore->Read.Active ||
		(pCore->State.LocalPackets >= pSession->WriteOrdinal) ||
		(pCore->Write.Message != pSession->WriteMessage) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		(Payload.Data != pSession->WritePayload.Data) ||
		(Payload.Size != pSession->WritePayload.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((pSession->WritePending == XSSH_SESSION_PACKET_KEXINIT) &&
		 (pCore->Write.Kind != XSSH_TRANSPORT_PACKET_KEXINIT)) ||
		((pSession->WritePending == XSSH_SESSION_PACKET_KEX) &&
		 (pCore->Write.Kind == XSSH_TRANSPORT_PACKET_KEXINIT)) ) {
		return XSSH_ERROR_STATE;
	}
	pSession->WriteBound = true;
	return XSSH_OK;
}



/* 提交 transport 已可靠接受的本端 payload。 */
xsshcode xrtSshSessionCoreWriteCommit(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshcode Code = XSSH_OK;

	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->WritePending == XSSH_SESSION_PACKET_NONE) ||
		!pSession->WriteBound || pCore->Write.Active ||
		(pCore->State.LocalPackets != pSession->WriteOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->WritePending == XSSH_SESSION_PACKET_KEXINIT ) {
		Code = xrtSshKexExchangeKexInitCommit(&pSession->Kex, pCore);
	} else if ( pSession->WritePending == XSSH_SESSION_PACKET_KEX ) {
		Code = xrtSshKexSessionWriteCommit(&pSession->Kex.Session, pCore);
		if ( (Code == XSSH_OK) && pSession->Kex.Session.LocalNewKeys &&
			!pSession->Kex.Session.WriteActivated ) {
			Code = xrtSshKexSessionActivateWrite(
				&pSession->Kex.Session,
				pCore,
				Timer
			);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshSessionCoreKexFinish(pSession, pCore);
		}
	} else if ( pSession->WritePending == XSSH_SESSION_PACKET_AUTH ) {
		Code = xrtSshAuthSessionWriteCommit(&pSession->Auth, pCore);
		if ( Code == XSSH_OK ) {
			Code = xsshSessionCoreConnectionStart(pSession, pCore);
		}
	} else if ( pSession->WritePending ==
		XSSH_SESSION_PACKET_CONNECTION ) {
		Code = xrtSshConnectionSessionWriteCommit(
			&pSession->Connection,
			pCore
		);
	}
	if ( Code != XSSH_OK ) {
		xrtSshSessionCoreFail(pSession);
		return Code;
	}
	xsshSessionCoreWriteClear(pSession);
	return XSSH_OK;
}



/* 放弃尚未可靠提交的本端 payload。 */
xsshcode xrtSshSessionCoreWriteAbort(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	xsshcode Code = XSSH_OK;

	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->WritePending == XSSH_SESSION_PACKET_NONE) ||
		(pCore->State.LocalPackets >= pSession->WriteOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->WritePending == XSSH_SESSION_PACKET_KEXINIT ) {
		Code = xrtSshKexExchangeKexInitAbort(&pSession->Kex, pCore);
	} else if ( pSession->WritePending == XSSH_SESSION_PACKET_KEX ) {
		Code = xrtSshKexSessionWriteAbort(&pSession->Kex.Session);
	} else if ( pSession->WritePending == XSSH_SESSION_PACKET_AUTH ) {
		Code = xrtSshAuthSessionWriteAbort(&pSession->Auth);
	} else if ( pSession->WritePending ==
		XSSH_SESSION_PACKET_CONNECTION ) {
		Code = xrtSshConnectionSessionWriteAbort(&pSession->Connection);
	}
	if ( Code == XSSH_OK ) {
		xsshSessionCoreWriteClear(pSession);
	}
	return Code;
}



/* 准备 transport 已认证的 peer payload。 */
xsshcode xrtSshSessionCoreReadPrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	double Timer,
	xsshsessionpacket* pPacket
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshsessionpacket Packet;
	xsshauthsessionpacket AuthPacket;
	bool bRecognized;
	uint8 iMessage;
	xsshcode Code;

	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		!pCore->Read.Active || pCore->Write.Active ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ||
		(pCore->State.PeerPackets == UINT64_MAX) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		!xrtMemRangeValid(pHostKeyStorage, iHostKeyCapacity) ||
		((pHostKeySize != NULL) &&
		 !xrtMemRangeValid(pHostKeySize, sizeof(*pHostKeySize))) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pPacket,
			sizeof(*pPacket)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pHostKeySize != NULL ) {
		*pHostKeySize = 0u;
	}
	memset(&Packet, 0, sizeof(Packet));
	Packet.Payload = Payload;
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( (Code != XSSH_OK) || (iMessage != pCore->Read.Message) ) {
		return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
	}
	Packet.Number = iMessage;
	if ( iMessage == XSSH_MSG_KEXINIT ) {
		Code = xrtSshKexExchangeKexInitPrepare(
			&pSession->Kex,
			pCore,
			XSSH_TRANSPORT_PEER,
			Payload
		);
		Packet.Kind = XSSH_SESSION_PACKET_KEXINIT;
	} else if ( (pCore->State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		((iMessage == XSSH_MSG_NEWKEYS) ||
		 ((iMessage >= XSSH_KEX_METHOD_MIN) &&
		  (iMessage <= XSSH_KEX_METHOD_MAX))) ) {
		Code = xrtSshKexSessionReadPrepare(
			&pSession->Kex.Session,
			pCore,
			Payload,
			pHostKeyStorage,
			iHostKeyCapacity,
			pHostKeySize
		);
		Packet.Kind = XSSH_SESSION_PACKET_KEX;
		Packet.Message.Kex = pSession->Kex.Session.ReadPending;
	} else {
		Code = xsshSessionCoreTransportPacket(
			Payload,
			iMessage,
			&Packet,
			&bRecognized
		);
		if ( (Code == XSSH_OK) && !bRecognized &&
			pSession->Auth.Active &&
			(pSession->Auth.Phase != XSSH_AUTH_SESSION_COMPLETE) ) {
			Code = xrtSshAuthSessionReadPrepare(
				&pSession->Auth,
				pCore,
				Payload,
				Timer,
				&AuthPacket
			);
			if ( Code == XSSH_OK ) {
				Packet.Kind = XSSH_SESSION_PACKET_AUTH;
				Packet.Message.Auth = AuthPacket;
			} else if ( Code == XSSH_ERROR_UNSUPPORTED ) {
				Code = XSSH_OK;
			}
		} else if ( (Code == XSSH_OK) && !bRecognized &&
			xrtSshConnectionSessionActive(&pSession->Connection) ) {
			Code = xrtSshConnectionSessionReadPrepare(
				&pSession->Connection,
				pCore,
				Payload,
				&Packet.Message.Connection
			);
			if ( Code == XSSH_OK ) {
				Packet.Kind = XSSH_SESSION_PACKET_CONNECTION;
			} else if ( Code == XSSH_ERROR_UNSUPPORTED ) {
				Code = XSSH_OK;
			}
		}
		if ( (Code == XSSH_OK) &&
			(Packet.Kind == XSSH_SESSION_PACKET_EXTENSION) &&
			(xsshSessionCoreAuthNumber(iMessage) ||
			 xsshSessionCoreConnectionNumber(iMessage)) ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->ReadOrdinal = pCore->State.PeerPackets + 1u;
	pSession->ReadPending = Packet.Kind;
	pSession->ReadMessage = iMessage;
	*pPacket = Packet;
	return XSSH_OK;
}



/* 提交已经由 transport 消费的 peer payload。 */
xsshcode xrtSshSessionCoreReadCommit(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshcode Code = XSSH_OK;

	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->ReadPending == XSSH_SESSION_PACKET_NONE) ||
		pCore->Read.Active ||
		(pCore->State.PeerPackets != pSession->ReadOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->ReadPending == XSSH_SESSION_PACKET_KEXINIT ) {
		Code = xrtSshKexExchangeKexInitCommit(&pSession->Kex, pCore);
	} else if ( pSession->ReadPending == XSSH_SESSION_PACKET_KEX ) {
		Code = xrtSshKexSessionReadCommit(&pSession->Kex.Session, pCore);
		if ( (Code == XSSH_OK) && pSession->Kex.Session.PeerNewKeys &&
			!pSession->Kex.Session.ReadActivated ) {
			Code = xrtSshKexSessionActivateRead(
				&pSession->Kex.Session,
				pCore,
				Timer
			);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshSessionCoreKexFinish(pSession, pCore);
		}
	} else if ( pSession->ReadPending == XSSH_SESSION_PACKET_AUTH ) {
		Code = xrtSshAuthSessionReadCommit(&pSession->Auth, pCore);
		if ( Code == XSSH_OK ) {
			Code = xsshSessionCoreConnectionStart(pSession, pCore);
		}
	} else if ( pSession->ReadPending ==
		XSSH_SESSION_PACKET_CONNECTION ) {
		Code = xrtSshConnectionSessionReadCommit(
			&pSession->Connection,
			pCore
		);
	}
	if ( Code != XSSH_OK ) {
		xrtSshSessionCoreFail(pSession);
		return Code;
	}
	xsshSessionCoreReadClear(pSession);
	return XSSH_OK;
}



/* 放弃不可回滚的 peer payload 并终止全部子状态。 */
xsshcode xrtSshSessionCoreReadAbort(xsshsessioncore* pSession)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		(pSession->ReadPending == XSSH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->ReadPending == XSSH_SESSION_PACKET_KEXINIT ) {
		xrtNetBufClear(&pSession->Kex.Staging);
	} else if ( pSession->ReadPending == XSSH_SESSION_PACKET_KEX ) {
		(void)xrtSshKexSessionReadAbort(&pSession->Kex.Session);
	} else if ( pSession->ReadPending == XSSH_SESSION_PACKET_AUTH ) {
		(void)xrtSshAuthSessionReadAbort(&pSession->Auth);
	} else if ( pSession->ReadPending ==
		XSSH_SESSION_PACKET_CONNECTION ) {
		(void)xrtSshConnectionSessionReadAbort(&pSession->Connection);
	}
	xrtSshSessionCoreFail(pSession);
	return XSSH_OK;
}



/* 终止协议核心但保留外部资源所有权。 */
void xrtSshSessionCoreFail(xsshsessioncore* pSession)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ) {
		return;
	}
	xrtSshKexExchangeFail(&pSession->Kex);
	xrtSshAuthSessionFail(&pSession->Auth);
	xrtSshConnectionSessionFail(&pSession->Connection);
	xsshSessionCoreWriteClear(pSession);
	xsshSessionCoreReadClear(pSession);
	pSession->Failed = true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_session_core_random.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_SESSION_CORE_RANDOM)



#if defined(XSSH_FEATURE_SESSION_CORE_RANDOM)

/* 使用系统安全随机临时密钥开始当前 KEX。 */
xsshcode xrtSshSessionCoreKexBegin(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey
)
{
	xsshkexexchange* pKex = xrtSshSessionCoreKex(pSession);

	if ( (pKex == NULL) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		(pCore->State.Role != pSession->Role) || pCore->Write.Active ||
		pCore->Read.Active ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshKexExchangeBegin(pKex, pCore, ServerHostKey);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_session_tcp.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_SESSION_TCP)
#include <math.h>
#include <string.h>




#if defined(XSSH_FEATURE_SESSION_TCP)

#define XSSH_SESSION_TCP_GUARD UINT32_C(0x53535450)



/* 校验组合对象以及两个子层的角色一致性。 */
static bool xsshSessionTcpValid(const xsshsessiontcp* pSession)
{
	const xsshtransporttcp* pTransport;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		(pSession->Guard != XSSH_SESSION_TCP_GUARD) ||
		!pSession->Session.Initialized ) {
		return false;
	}
	pTransport = xrtSshTransportTcpCoreConst(&pSession->Transport) != NULL ?
		&pSession->Transport : NULL;
	if ( (pTransport == NULL) ||
		(pTransport->Core.State.Role != pSession->Session.Role) ) {
		return false;
	}
	return true;
}



/* 清空只在未决 packet 读取期间有效的外部缓冲借用。 */
static void xsshSessionTcpReadClear(xsshsessiontcp* pSession)
{
	memset(&pSession->ReadPacket, 0, sizeof(pSession->ReadPacket));
	pSession->ReadVersion = (xstrview){ NULL, 0u };
	pSession->ReadPlain = NULL;
	pSession->ReadPlainCapacity = 0u;
}



/* 把无法恢复的跨层提交错误发布到 XRT 并终止整条链路。 */
static void xsshSessionTcpFail(
	xsshsessiontcp* pSession,
	xnetstream* pStream,
	xsshcode Code,
	cstr sMessage
)
{
	xrtSetErrorInfo(
		Code == XSSH_ERROR_PROTOCOL ? XERR_PROTOCOL : XERR_INTERNAL,
		"xrt.ssh",
		(int32)Code,
		sMessage
	);
	xrtSshSessionCoreFail(&pSession->Session);
	xrtSshTransportCoreClose(&pSession->Transport.Core);
	xsshSessionTcpReadClear(pSession);
	if ( pStream != NULL ) {
		(void)xrtNetStreamAbort(pStream);
	}
}



/* 校验 packet 读取的外部对象，避免成功后写回破坏未消费线路数据。 */
static bool xsshSessionTcpReadArguments(
	const xsshsessiontcp* pSession,
	const xnetbuf* pInput,
	void* pPlain,
	size_t iPlainCapacity,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	xsshsessiontcppacket* pPacket
)
{
	if ( (pInput == NULL) ||
		!xrtMemRangeValid(pPlain, iPlainCapacity) ||
		!xrtMemRangeValid(pHostKeyStorage, iHostKeyCapacity) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		((pHostKeySize != NULL) &&
		 !xrtMemRangeValid(pHostKeySize, sizeof(*pHostKeySize))) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pInput,
		sizeof(*pInput)
	) || xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pPlain,
		iPlainCapacity
	) || xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pHostKeyStorage,
		iHostKeyCapacity
	) || xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pPacket,
		sizeof(*pPacket)
	) || xrtMemRangesOverlap(
		pInput,
		sizeof(*pInput),
		pPacket,
		sizeof(*pPacket)
	) || xrtMemRangesOverlap(
		pPlain,
		iPlainCapacity,
		pPacket,
		sizeof(*pPacket)
	) || xrtMemRangesOverlap(
		pHostKeyStorage,
		iHostKeyCapacity,
		pPacket,
		sizeof(*pPacket)
	) ) {
		return false;
	}
	if ( (pHostKeySize != NULL) &&
		(xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pHostKeySize,
			sizeof(*pHostKeySize)
		) || xrtMemRangesOverlap(
			pInput,
			sizeof(*pInput),
			pHostKeySize,
			sizeof(*pHostKeySize)
		) || xrtMemRangesOverlap(
			pPlain,
			iPlainCapacity,
			pHostKeySize,
			sizeof(*pHostKeySize)
		) || xrtMemRangesOverlap(
			pHostKeyStorage,
			iHostKeyCapacity,
			pHostKeySize,
			sizeof(*pHostKeySize)
		) || xrtMemRangesOverlap(
			pPacket,
			sizeof(*pPacket),
			pHostKeySize,
			sizeof(*pHostKeySize)
		)) ) {
		return false;
	}
	return true;
}



/* 默认配置直接复用 transport 的稳定预算。 */
bool xrtSshSessionTcpConfigInit(
	xsshsessiontcpconfig* pConfig,
	xsshrole Role
)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		return false;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	return xrtSshTransportTcpConfigInit(&pConfig->Transport, Role);
}



/* 两个子对象先在局部结构中完整初始化，再一次发布。 */
bool xrtSshSessionTcpInit(
	xsshsessiontcp* pSession,
	xnetbufpool* pPool,
	const xsshsessiontcpconfig* pConfig,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return false; }
	xsshsessiontcp Session;
	xsshsessiontcpconfig Config;
	size_t iReplyBytes = 0u;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pConfig,
			sizeof(*pConfig)
		) ) {
		return false;
	}
	Config = *pConfig;
	if ( Config.GlobalReplies != NULL ) {
		if ( !xrtMemRangeValid(
			Config.GlobalReplies,
			sizeof(*Config.GlobalReplies)
		) || (Config.GlobalReplies->Capacity >
			(SIZE_MAX / sizeof(uint64))) ) {
			return false;
		}
		iReplyBytes = Config.GlobalReplies->Capacity * sizeof(uint64);
		if ( xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Config.GlobalReplies,
			sizeof(*Config.GlobalReplies)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Config.GlobalReplies->Tokens,
			iReplyBytes
		) ) {
			return false;
		}
	}
	memset(&Session, 0, sizeof(Session));
	if ( !xrtSshTransportTcpInit(
		&Session.Transport,
		pPool,
		&Config.Transport,
		Timer
	) || !xrtSshSessionCoreInit(
		&Session.Session,
		pPool,
		Config.Transport.Role,
		Config.ChannelResolve,
		Config.ChannelUserData,
		Config.GlobalReplies
	) ) {
		xrtSshSessionCoreClear(&Session.Session);
		xrtSshTransportTcpClear(&Session.Transport);
		return false;
	}
	Session.Guard = XSSH_SESSION_TCP_GUARD;
	xrtSecureZero(pSession, sizeof(*pSession));
	*pSession = Session;
	xrtSecureZero(&Session, sizeof(Session));
	return true;
}



/* 协议层先释放 transcript，transport 随后回滚 packet 并清除 cipher。 */
void xrtSshSessionTcpClear(xsshsessiontcp* pSession)
{
	if ( pSession == NULL ) {
		return;
	}
	if ( xsshSessionTcpValid(pSession) ) {
		xrtSshSessionCoreClear(&pSession->Session);
		xrtSshTransportTcpClear(&pSession->Transport);
	}
	xrtSecureZero(pSession, sizeof(*pSession));
}



/* 返回组合对象持有的 TCP transport。 */
xsshtransporttcp* xrtSshSessionTcpTransport(xsshsessiontcp* pSession)
{
	return xsshSessionTcpValid(pSession) ? &pSession->Transport : NULL;
}



/* 返回只读 TCP transport。 */
const xsshtransporttcp* xrtSshSessionTcpTransportConst(
	const xsshsessiontcp* pSession
)
{
	return xsshSessionTcpValid(pSession) ? &pSession->Transport : NULL;
}



/* 返回组合对象持有的连接级协议核心。 */
xsshsessioncore* xrtSshSessionTcpCore(xsshsessiontcp* pSession)
{
	return xsshSessionTcpValid(pSession) ? &pSession->Session : NULL;
}



/* 返回只读连接级协议核心。 */
const xsshsessioncore* xrtSshSessionTcpCoreConst(
	const xsshsessiontcp* pSession
)
{
	return xsshSessionTcpValid(pSession) ? &pSession->Session : NULL;
}



/* 连接阶段统一从协议核心和同一 transport 推导。 */
xsshsessionphase xrtSshSessionTcpPhase(const xsshsessiontcp* pSession)
{
	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_SESSION_FAILED;
	}
	return xrtSshSessionCorePhase(
		&pSession->Session,
		&pSession->Transport.Core
	);
}



/* TCP 组合对象直接复用连接级统一动作推导。 */
xsshsessionaction xrtSshSessionTcpAction(const xsshsessiontcp* pSession)
{
	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_SESSION_ACTION_FAILED;
	}
	return xrtSshSessionCoreAction(
		&pSession->Session,
		&pSession->Transport.Core
	);
}



/* 确定性 KEX 便利入口只消除两个子对象访问样板。 */
xsshcode xrtSshSessionTcpKexBeginWithPrivate(
	xsshsessiontcp* pSession,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
)
{
	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(ServerHostKey.Data, ServerHostKey.Size) ||
		!xrtMemRangeValid(PrivateKey.Data, PrivateKey.Size) ||
		xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		ServerHostKey.Data,
		ServerHostKey.Size
	) || xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		PrivateKey.Data,
		PrivateKey.Size
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshSessionCoreKexBeginWithPrivate(
		&pSession->Session,
		&pSession->Transport.Core,
		ServerHostKey,
		PrivateKey
	);
}



/* 认证便利入口保留调用方策略和时钟所有权。 */
xsshcode xrtSshSessionTcpAuthBegin(
	xsshsessiontcp* pSession,
	const xsshauthguardpolicy* pPolicy,
	double Timer
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pPolicy != NULL) &&
		(!xrtMemRangeValid(pPolicy, sizeof(*pPolicy)) ||
		 xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pPolicy,
			sizeof(*pPolicy)
		)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshSessionCoreAuthBegin(
		&pSession->Session,
		&pSession->Transport.Core,
		pPolicy,
		Timer
	);
}



/* 版本先进入 transcript 暂存，再准备可重试的动态线路输出。 */
xsshcode xrtSshSessionTcpIdentificationWritePrepare(
	xsshsessiontcp* pSession,
	xstrview Version
)
{
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Version.Data, Version.Size) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Version.Data,
			Version.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshSessionCoreVersionPrepare(
		&pSession->Session,
		&pSession->Transport.Core,
		XSSH_TRANSPORT_LOCAL,
		Version
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshTransportTcpIdentificationPrepare(
		&pSession->Transport,
		Version
	);
	if ( Code != XSSH_OK ) {
		(void)xrtSshSessionCoreVersionAbort(
			&pSession->Session,
			&pSession->Transport.Core
		);
	}
	return Code;
}



/* 协议候选、线路编码和事务绑定在一次调用中完整闭合。 */
xsshcode xrtSshSessionTcpWritePrepareWithPadding(
	xsshsessiontcp* pSession,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	xsshpaddingproc pPadding,
	ptr pPaddingData,
	double Timer,
	xsshsessionpacketkind* pKind
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		!xrtMemRangeValid(pKind, sizeof(*pKind)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pKind,
			sizeof(*pKind)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshSessionCoreWritePrepare(
		&pSession->Session,
		&pSession->Transport.Core,
		Payload,
		pChannel,
		pReplies,
		iReplyToken,
		Timer,
		pKind
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshTransportTcpWritePrepareWithPadding(
		&pSession->Transport,
		Payload,
		pPadding,
		pPaddingData,
		Timer
	);
	if ( Code != XSSH_OK ) {
		(void)xrtSshSessionCoreWriteAbort(
			&pSession->Session,
			&pSession->Transport.Core
		);
		return Code;
	}
	Code = xrtSshSessionCoreWriteBind(
		&pSession->Session,
		&pSession->Transport.Core,
		Payload
	);
	if ( Code != XSSH_OK ) {
		(void)xrtSshSessionCoreWriteAbort(
			&pSession->Session,
			&pSession->Transport.Core
		);
		(void)xrtSshTransportTcpWriteAbort(&pSession->Transport);
	}
	return Code;
}



/* Stream 接管后 transport 先提交，协议层再发布相同事务。 */
xnetresult xrtSshSessionTcpWriteSubmit(
	xsshsessiontcp* pSession,
	xnetstream* pStream,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	xsshtransporttcppending Pending;
	xnetresult Result;
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) ||
		!xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pDecision,
			sizeof(*pDecision)
		) ) {
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.ssh",
			(int32)XSSH_ERROR_ARGUMENT,
			"invalid SSH TCP session write submission"
		);
		return XNET_RESULT_ERROR;
	}
	Pending = pSession->Transport.WritePending;
	if ( ((Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION) &&
		 ((pSession->Session.Kex.Pending !=
		   XSSH_KEX_EXCHANGE_PENDING_VERSION) ||
		  (pSession->Session.Kex.PendingDirection !=
		   XSSH_TRANSPORT_LOCAL))) ||
		((Pending == XSSH_TRANSPORT_TCP_PENDING_PACKET) &&
		 ((pSession->Session.WritePending ==
		   XSSH_SESSION_PACKET_NONE) ||
		  !pSession->Session.WriteBound)) ||
		(Pending == XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		xsshSessionTcpFail(
			pSession,
			NULL,
			XSSH_ERROR_STATE,
			"SSH TCP session write transactions diverged"
		);
		return XNET_RESULT_ERROR;
	}
	Result = xrtSshTransportTcpWriteSubmit(
		&pSession->Transport,
		pStream,
		Timer,
		pDecision
	);
	if ( Result != XNET_RESULT_OK ) {
		return Result;
	}
	if ( Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		Code = xrtSshSessionCoreVersionCommit(
			&pSession->Session,
			&pSession->Transport.Core
		);
	} else {
		Code = xrtSshSessionCoreWriteCommit(
			&pSession->Session,
			&pSession->Transport.Core,
			Timer
		);
	}
	if ( Code != XSSH_OK ) {
		xsshSessionTcpFail(
			pSession,
			pStream,
			Code,
			"SSH session rejected TCP-accepted output"
		);
		return XNET_RESULT_ERROR;
	}
	return XNET_RESULT_OK;
}



/* 两个写事务按上层到 transport 的逆提交顺序回滚。 */
xsshcode xrtSshSessionTcpWriteAbort(xsshsessiontcp* pSession)
{
	xsshtransporttcppending Pending;
	xsshcode SessionCode;
	xsshcode TransportCode;

	if ( !xsshSessionTcpValid(pSession) ||
		(pSession->Transport.WritePending ==
		 XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	Pending = pSession->Transport.WritePending;
	if ( Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		SessionCode = xrtSshSessionCoreVersionAbort(
			&pSession->Session,
			&pSession->Transport.Core
		);
	} else {
		SessionCode = xrtSshSessionCoreWriteAbort(
			&pSession->Session,
			&pSession->Transport.Core
		);
	}
	TransportCode = xrtSshTransportTcpWriteAbort(&pSession->Transport);
	if ( (SessionCode != XSSH_OK) || (TransportCode != XSSH_OK) ) {
		xsshSessionTcpFail(
			pSession,
			NULL,
			SessionCode != XSSH_OK ? SessionCode : TransportCode,
			"SSH TCP session write rollback diverged"
		);
	}
	return SessionCode != XSSH_OK ? SessionCode : TransportCode;
}



/* 动态输出大小直接由 transport 持有。 */
size_t xrtSshSessionTcpWriteSize(const xsshsessiontcp* pSession)
{
	return xsshSessionTcpValid(pSession) ?
		xrtSshTransportTcpWriteSize(&pSession->Transport) : 0u;
}



/* transport 借出完整版本行后，协议层复制不含 CRLF 的 transcript。 */
xsshcode xrtSshSessionTcpIdentificationReadPrepare(
	xsshsessiontcp* pSession,
	xnetbuf* pInput,
	xstrview* pVersion
)
{
	xstrview Version;
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) || (pInput == NULL) ||
		!xrtMemRangeValid(pVersion, sizeof(*pVersion)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pInput,
			sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pVersion,
			sizeof(*pVersion)
		) || xrtMemRangesOverlap(
			pInput,
			sizeof(*pInput),
			pVersion,
			sizeof(*pVersion)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pSession->Transport.ReadPending ==
		XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		if ( pSession->Transport.Input != pInput ) {
			return XSSH_ERROR_ARGUMENT;
		}
		Version = pSession->ReadVersion;
	} else {
		Code = xrtSshTransportTcpIdentificationReadPrepare(
			&pSession->Transport,
			pInput,
			&Version
		);
		if ( Code != XSSH_OK ) {
			if ( (pSession->Transport.Core.State.Phase ==
				XSSH_TRANSPORT_CLOSING) ||
				(pSession->Transport.Core.State.Phase ==
				 XSSH_TRANSPORT_CLOSED) ) {
				xrtSshSessionCoreFail(&pSession->Session);
				xsshSessionTcpReadClear(pSession);
			}
			return Code;
		}
		pSession->ReadVersion = Version;
	}
	Code = xrtSshSessionCoreVersionPrepare(
		&pSession->Session,
		&pSession->Transport.Core,
		XSSH_TRANSPORT_PEER,
		Version
	);
	if ( Code == XSSH_OK ) {
		*pVersion = Version;
	} else if ( Code != XSSH_ERROR_SPACE ) {
		(void)xrtSshTransportTcpReadAbort(&pSession->Transport);
		xsshSessionTcpFail(
			pSession,
			NULL,
			Code,
			"SSH peer identification was rejected"
		);
	}
	return Code;
}



/* packet 探测不触碰协议核心事务。 */
xsshcode xrtSshSessionTcpReadInspect(
	const xsshsessiontcp* pSession,
	const xnetbuf* pInput,
	xsshpacketneed* pNeed
)
{
	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pNeed, sizeof(*pNeed)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pNeed,
			sizeof(*pNeed)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshTransportTcpReadInspect(
		&pSession->Transport,
		pInput,
		pNeed
	);
}



/* transport 认证与会话路由共享同一个未消费线路前缀。 */
xsshcode xrtSshSessionTcpReadPrepare(
	xsshsessiontcp* pSession,
	xnetbuf* pInput,
	void* pPlain,
	size_t iPlainCapacity,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	double Timer,
	xsshsessiontcppacket* pPacket
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshsessionpacket SessionPacket;
	xsshpacketview TransportPacket;
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) ||
		!xsshSessionTcpReadArguments(
			pSession,
			pInput,
			pPlain,
			iPlainCapacity,
			pHostKeyStorage,
			iHostKeyCapacity,
			pHostKeySize,
			pPacket
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pSession->Transport.ReadPending ==
		XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
		if ( (pSession->Transport.Input != pInput) ||
			(pSession->ReadPlain != pPlain) ||
			(pSession->ReadPlainCapacity != iPlainCapacity) ||
			(pSession->Session.ReadPending !=
			 XSSH_SESSION_PACKET_NONE) ) {
			return XSSH_ERROR_STATE;
		}
		TransportPacket = pSession->ReadPacket;
	} else {
		Code = xrtSshTransportTcpReadPrepare(
			&pSession->Transport,
			pInput,
			&TransportPacket,
			pPlain,
			iPlainCapacity,
			Timer
		);
		if ( Code != XSSH_OK ) {
			if ( (pSession->Transport.Core.State.Phase ==
				XSSH_TRANSPORT_CLOSING) ||
				(pSession->Transport.Core.State.Phase ==
				 XSSH_TRANSPORT_CLOSED) ) {
				xrtSshSessionCoreFail(&pSession->Session);
				xsshSessionTcpReadClear(pSession);
			}
			return Code;
		}
		pSession->ReadPacket = TransportPacket;
		pSession->ReadPlain = pPlain;
		pSession->ReadPlainCapacity = iPlainCapacity;
	}
	Code = xrtSshSessionCoreReadPrepare(
		&pSession->Session,
		&pSession->Transport.Core,
		TransportPacket.Payload,
		pHostKeyStorage,
		iHostKeyCapacity,
		pHostKeySize,
		Timer,
		&SessionPacket
	);
	if ( Code == XSSH_OK ) {
		pPacket->Transport = TransportPacket;
		pPacket->Session = SessionPacket;
	} else if ( (Code != XSSH_ERROR_SPACE) &&
		(Code != XSSH_ERROR_ARGUMENT) ) {
		(void)xrtSshTransportTcpReadAbort(&pSession->Transport);
		xsshSessionTcpFail(
			pSession,
			NULL,
			Code,
			"SSH authenticated packet was rejected"
		);
	}
	return Code;
}



/* transport 消费成功后才发布版本或连接级协议状态。 */
xsshcode xrtSshSessionTcpReadCommit(
	xsshsessiontcp* pSession,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshtransporttcppending Pending;
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) ||
		!xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Pending = pSession->Transport.ReadPending;
	if ( ((Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION) &&
		 ((pSession->Session.Kex.Pending !=
		   XSSH_KEX_EXCHANGE_PENDING_VERSION) ||
		  (pSession->Session.Kex.PendingDirection !=
		   XSSH_TRANSPORT_PEER))) ||
		((Pending == XSSH_TRANSPORT_TCP_PENDING_PACKET) &&
		 (pSession->Session.ReadPending ==
		  XSSH_SESSION_PACKET_NONE)) ||
		(Pending == XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshTransportTcpReadCommit(
		&pSession->Transport,
		Timer,
		pDecision
	);
	if ( Code != XSSH_OK ) {
		xrtSshSessionCoreFail(&pSession->Session);
		xsshSessionTcpReadClear(pSession);
		return Code;
	}
	if ( Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		Code = xrtSshSessionCoreVersionCommit(
			&pSession->Session,
			&pSession->Transport.Core
		);
	} else {
		Code = xrtSshSessionCoreReadCommit(
			&pSession->Session,
			&pSession->Transport.Core,
			Timer
		);
	}
	xsshSessionTcpReadClear(pSession);
	if ( Code != XSSH_OK ) {
		xsshSessionTcpFail(
			pSession,
			NULL,
			Code,
			"SSH session rejected consumed TCP input"
		);
	}
	return Code;
}



/* 拒绝输入时两层都进入终止态，线路前缀只由 transport 消费一次。 */
xsshcode xrtSshSessionTcpReadAbort(xsshsessiontcp* pSession)
{
	xsshtransporttcppending Pending;
	xsshcode SessionCode = XSSH_OK;
	xsshcode TransportCode;

	if ( !xsshSessionTcpValid(pSession) ||
		(pSession->Transport.ReadPending ==
		 XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	Pending = pSession->Transport.ReadPending;
	if ( Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		if ( pSession->Session.Kex.Pending ==
			XSSH_KEX_EXCHANGE_PENDING_VERSION ) {
			SessionCode = xrtSshSessionCoreVersionAbort(
				&pSession->Session,
				&pSession->Transport.Core
			);
		}
	} else if ( pSession->Session.ReadPending !=
		XSSH_SESSION_PACKET_NONE ) {
		SessionCode = xrtSshSessionCoreReadAbort(&pSession->Session);
	}
	TransportCode = xrtSshTransportTcpReadAbort(&pSession->Transport);
	xrtSshSessionCoreFail(&pSession->Session);
	xsshSessionTcpReadClear(pSession);
	return SessionCode != XSSH_OK ? SessionCode : TransportCode;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_session_reader.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_SESSION_READER)
#include <math.h>
#include <string.h>




#if defined(XSSH_FEATURE_SESSION_READER)

#define XSSH_SESSION_READER_GUARD UINT32_C(0x53535244)



/* 校验读取器、动态链和借用会话仍属于同一个缓冲池。 */
static bool xsshSessionReaderValid(const xsshsessionreader* pReader)
{
	const xsshtransporttcp* pTransport;

	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		(pReader->Guard != XSSH_SESSION_READER_GUARD) ||
		(pReader->Session == NULL) ||
		(pReader->State < XSSH_SESSION_READER_IDLE) ||
		(pReader->State > XSSH_SESSION_READER_READY) ) {
		return false;
	}
	pTransport = xrtSshSessionTcpTransportConst(pReader->Session);
	return (pTransport != NULL) &&
		(pReader->Plain.Pool == pTransport->Output.Pool) &&
		(pReader->HostKey.Pool == pTransport->Output.Pool);
}



/* 放弃尚未提交的动态尾部，并清空 packet 借用元数据。 */
static bool xsshSessionReaderReset(xsshsessionreader* pReader)
{
	bool bSuccess = true;

	if ( pReader->Plain.Reserved != NULL ) {
		bSuccess = xrtNetBufCancel(&pReader->Plain) && bSuccess;
	}
	if ( pReader->HostKey.Reserved != NULL ) {
		bSuccess = xrtNetBufCancel(&pReader->HostKey) && bSuccess;
	}
	pReader->Input = NULL;
	pReader->PlainSpan = (xnetwspan){ NULL, 0u };
	pReader->HostKeySpan = (xnetwspan){ NULL, 0u };
	memset(&pReader->Need, 0, sizeof(pReader->Need));
	pReader->HostKeyOldSize = 0u;
	pReader->HostKeySize = 0u;
	pReader->State = XSSH_SESSION_READER_IDLE;
	return bSuccess;
}



/* 线路或动态工作区错误终止读取器绑定的完整会话。 */
static xsshcode xsshSessionReaderFail(
	xsshsessionreader* pReader,
	xsshcode Code,
	cstr sMessage
)
{
	xrtSetErrorInfo(
		XERR_INTERNAL,
		"xrt.ssh",
		(int32)Code,
		sMessage
	);
	xrtSshSessionCoreFail(&pReader->Session->Session);
	xrtSshTransportCoreClose(&pReader->Session->Transport.Core);
	(void)xsshSessionReaderReset(pReader);
	return Code;
}



/* 校验 Prepare 的输入和输出不会覆盖读取器或绑定会话。 */
static bool xsshSessionReaderPrepareArguments(
	const xsshsessionreader* pReader,
	const xnetbuf* pInput,
	const xsshsessiontcppacket* pPacket
)
{
	return (pInput != NULL) &&
		xrtMemRangeValid(pPacket, sizeof(*pPacket)) &&
		!xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pInput,
			sizeof(*pInput)
		) && !xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pPacket,
			sizeof(*pPacket)
		) && !xrtMemRangesOverlap(
			pReader->Session,
			sizeof(*pReader->Session),
			pPacket,
			sizeof(*pPacket)
		) && !xrtMemRangesOverlap(
			pInput,
			sizeof(*pInput),
			pPacket,
			sizeof(*pPacket)
		);
}



/* 首次探测后让明文包借用输入，加密包只申请本次解密需要的连续空间。 */
static xsshcode xsshSessionReaderPlainPrepare(
	xsshsessionreader* pReader,
	xnetbuf* pInput
)
{
	xsshcode Code;

	Code = xrtSshSessionTcpReadInspect(
		pReader->Session,
		pInput,
		&pReader->Need
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtNetBufSize(pInput) < pReader->Need.WireSize ) {
		return XSSH_NEED_MORE;
	}
	if ( pReader->Need.PlainSize != 0u ) {
		if ( !xrtNetBufReserve(
			&pReader->Plain,
			pReader->Need.PlainSize,
			&pReader->PlainSpan
		) ) {
			return XSSH_ERROR_SPACE;
		}
	} else {
		pReader->PlainSpan = (xnetwspan){ NULL, 0u };
	}
	pReader->Input = pInput;
	return XSSH_OK;
}



/* 按 ECDH_REPLY 给出的精确长度申请新主机公钥尾部。 */
static xsshcode xsshSessionReaderHostKeyPrepare(
	xsshsessionreader* pReader
)
{
	if ( pReader->HostKeySize == 0u ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pReader->HostKey.Reserved != NULL) &&
		(pReader->HostKeySpan.Size < pReader->HostKeySize) ) {
		if ( !xrtNetBufCancel(&pReader->HostKey) ) {
			return XSSH_ERROR_STATE;
		}
		pReader->HostKeySpan = (xnetwspan){ NULL, 0u };
	}
	if ( pReader->HostKey.Reserved == NULL ) {
		pReader->HostKeyOldSize = xrtNetBufSize(&pReader->HostKey);
		if ( !xrtNetBufReserve(
			&pReader->HostKey,
			pReader->HostKeySize,
			&pReader->HostKeySpan
		) ) {
			return XSSH_ERROR_SPACE;
		}
	}
	return XSSH_OK;
}



/* 使用当前动态空间重试同一未消费 packet 的上层解析。 */
static xsshcode xsshSessionReaderPacketPrepare(
	xsshsessionreader* pReader,
	double Timer,
	xsshsessiontcppacket* pPacket
)
{
	xsshsessiontcppacket Packet;
	size_t iHostKeySize = pReader->HostKeySize;
	xsshcode Code;

	memset(&Packet, 0, sizeof(Packet));
	Code = xrtSshSessionTcpReadPrepare(
		pReader->Session,
		pReader->Input,
		pReader->PlainSpan.Data,
		pReader->PlainSpan.Size,
		pReader->HostKey.Reserved != NULL ?
			pReader->HostKeySpan.Data : NULL,
		pReader->HostKey.Reserved != NULL ?
			pReader->HostKeySpan.Size : 0u,
		&iHostKeySize,
		Timer,
		&Packet
	);
	pReader->HostKeySize = iHostKeySize;
	if ( Code == XSSH_OK ) {
		pReader->State = XSSH_SESSION_READER_READY;
		*pPacket = Packet;
	} else if ( Code == XSSH_ERROR_SPACE ) {
		pReader->State = (pReader->HostKeySize >
			pReader->HostKeySpan.Size) ?
			XSSH_SESSION_READER_HOST_KEY :
			XSSH_SESSION_READER_RETRY;
	}
	return Code;
}



/* 初始化两个空动态链并绑定已经有效的 TCP 会话。 */
bool xrtSshSessionReaderInit(
	xsshsessionreader* pReader,
	xnetbufpool* pPool,
	xsshsessiontcp* pSession
)
{
	xsshsessionreader Reader;
	const xsshtransporttcp* pTransport;

	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		!xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pSession,
			sizeof(*pSession)
		) ) {
		return false;
	}
	pTransport = xrtSshSessionTcpTransportConst(pSession);
	if ( (pTransport == NULL) || (pTransport->Output.Pool != pPool) ) {
		return false;
	}
	memset(&Reader, 0, sizeof(Reader));
	if ( !xrtNetBufInit(&Reader.Plain, pPool) ||
		!xrtNetBufInit(&Reader.HostKey, pPool) ) {
		xrtNetBufClear(&Reader.Plain);
		xrtNetBufClear(&Reader.HostKey);
		return false;
	}
	Reader.Session = pSession;
	Reader.State = XSSH_SESSION_READER_IDLE;
	Reader.Guard = XSSH_SESSION_READER_GUARD;
	*pReader = Reader;
	return true;
}



/* 清理前必须终止仍借用输入或动态空间的 packet 事务。 */
void xrtSshSessionReaderClear(xsshsessionreader* pReader)
{
	if ( pReader == NULL ) {
		return;
	}
	if ( xsshSessionReaderValid(pReader) ) {
		if ( pReader->State != XSSH_SESSION_READER_IDLE ) {
			(void)xrtSshSessionTcpReadAbort(pReader->Session);
		}
		(void)xsshSessionReaderReset(pReader);
		xrtNetBufClear(&pReader->Plain);
		xrtNetBufClear(&pReader->HostKey);
	}
	memset(pReader, 0, sizeof(*pReader));
}



/* 返回借用的可变 TCP 会话。 */
xsshsessiontcp* xrtSshSessionReaderSession(xsshsessionreader* pReader)
{
	return xsshSessionReaderValid(pReader) ? pReader->Session : NULL;
}



/* 返回借用的只读 TCP 会话。 */
const xsshsessiontcp* xrtSshSessionReaderSessionConst(
	const xsshsessionreader* pReader
)
{
	return xsshSessionReaderValid(pReader) ? pReader->Session : NULL;
}



/* 无效对象返回不会与任何可提交事务混淆的独立状态。 */
xsshsessionreaderstate xrtSshSessionReaderState(
	const xsshsessionreader* pReader
)
{
	return xsshSessionReaderValid(pReader) ?
		pReader->State : XSSH_SESSION_READER_INVALID;
}



/* 首次探测明文，空间不足时在同一调用中按精确主机公钥长度重试。 */
xsshcode xrtSshSessionReaderPrepare(
	xsshsessionreader* pReader,
	xnetbuf* pInput,
	double Timer,
	xsshsessiontcppacket* pPacket
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshcode Code;

	if ( !xsshSessionReaderValid(pReader) ||
		!xsshSessionReaderPrepareArguments(pReader, pInput, pPacket) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pReader->State == XSSH_SESSION_READER_READY ) {
		return XSSH_ERROR_STATE;
	}
	if ( pReader->State == XSSH_SESSION_READER_IDLE ) {
		Code = xsshSessionReaderPlainPrepare(pReader, pInput);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	} else if ( pReader->Input != pInput ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for (;;) {
		if ( pReader->State == XSSH_SESSION_READER_HOST_KEY ) {
			Code = xsshSessionReaderHostKeyPrepare(pReader);
			if ( Code != XSSH_OK ) {
				return Code;
			}
		}
		Code = xsshSessionReaderPacketPrepare(
			pReader,
			Timer,
			pPacket
		);
		if ( (Code == XSSH_ERROR_SPACE) &&
			(pReader->State == XSSH_SESSION_READER_HOST_KEY) ) {
			continue;
		}
		if ( (Code != XSSH_OK) && (Code != XSSH_ERROR_SPACE) ) {
			(void)xsshSessionReaderReset(pReader);
		}
		return Code;
	}
}



/* 发布可持久借用的主机公钥，再提交唯一 packet 并释放明文工作区。 */
xsshcode xrtSshSessionReaderCommit(
	xsshsessionreader* pReader,
	double Timer,
	xsshrekeydecision* pDecision
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	bool bHostKey;
	xsshcode Code;
	xnetbufpool* pPool;

	if ( !xsshSessionReaderValid(pReader) ||
		(pReader->State != XSSH_SESSION_READER_READY) ||
		!xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	pPool = pReader->Plain.Pool;
	bHostKey = pReader->HostKey.Reserved != NULL;
	if ( bHostKey && !xrtNetBufCommit(
		&pReader->HostKey,
		pReader->HostKeySize
	) ) {
		return xsshSessionReaderFail(
			pReader,
			XSSH_ERROR_STATE,
			"SSH host key workspace commit failed"
		);
	}
	Code = xrtSshSessionTcpReadCommit(
		pReader->Session,
		Timer,
		pDecision
	);
	if ( Code != XSSH_OK ) {
		xrtNetBufClear(&pReader->HostKey);
		if ( !xrtNetBufInit(
			&pReader->HostKey,
			pPool
		) ) {
			return xsshSessionReaderFail(
				pReader,
				XSSH_ERROR_STATE,
				"SSH host key buffer reset failed"
			);
		}
		(void)xsshSessionReaderReset(pReader);
		return Code;
	}
	if ( bHostKey && (xrtNetBufConsume(
		&pReader->HostKey,
		pReader->HostKeyOldSize
	) != pReader->HostKeyOldSize) ) {
		return xsshSessionReaderFail(
			pReader,
			XSSH_ERROR_STATE,
			"SSH previous host key release failed"
		);
	}
	if ( !xsshSessionReaderReset(pReader) ) {
		return xsshSessionReaderFail(
			pReader,
			XSSH_ERROR_STATE,
			"SSH packet workspace release failed"
		);
	}
	return XSSH_OK;
}



/* transport 消费由基础组合层负责，读取器只释放自己的动态工作区。 */
xsshcode xrtSshSessionReaderAbort(xsshsessionreader* pReader)
{
	xsshcode Code;

	if ( !xsshSessionReaderValid(pReader) ||
		(pReader->State == XSSH_SESSION_READER_IDLE) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshSessionTcpReadAbort(pReader->Session);
	if ( !xsshSessionReaderReset(pReader) && (Code == XSSH_OK) ) {
		Code = XSSH_ERROR_STATE;
	}
	return Code;
}



/* 未提交新 key 优先返回 staging，其他状态返回最近提交的连续 key。 */
xbytesview xrtSshSessionReaderHostKey(
	const xsshsessionreader* pReader
)
{
	xnetspan Span;

	if ( !xsshSessionReaderValid(pReader) ) {
		return (xbytesview){ NULL, 0u };
	}
	if ( pReader->HostKey.Reserved != NULL ) {
		return (xbytesview){
			pReader->HostKeySpan.Data,
			pReader->HostKeySize
		};
	}
	if ( !xrtNetBufFront(&pReader->HostKey, &Span) ||
		(Span.Size < xrtNetBufSize(&pReader->HostKey)) ) {
		return (xbytesview){ NULL, 0u };
	}
	return (xbytesview){ Span.Data, xrtNetBufSize(&pReader->HostKey) };
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_session_stream.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_SESSION_STREAM)
#include <string.h>




#if defined(XSSH_FEATURE_SESSION_STREAM)

#define XSSH_SESSION_STREAM_GUARD UINT32_C(0x53535354)



static void xsshSessionStreamOpen(xnetstream* pStream, ptr pData);
static void xsshSessionStreamRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
);
static void xsshSessionStreamEnd(xnetstream* pStream, ptr pData);
static void xsshSessionStreamHighWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
);
static void xsshSessionStreamLowWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
);
static void xsshSessionStreamDrain(xnetstream* pStream, ptr pData);
static void xsshSessionStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
);



static const xnetstreamevents xsshSessionStreamEvents = {
	xsshSessionStreamOpen,
	xsshSessionStreamRead,
	xsshSessionStreamEnd,
	xsshSessionStreamHighWater,
	xsshSessionStreamLowWater,
	xsshSessionStreamDrain,
	xsshSessionStreamClose
};



/* 验证驱动哨兵和公开状态，不触碰尚未初始化的会话对象。 */
static bool xsshSessionStreamValid(const xsshsessionstream* pSession)
{
	return xrtMemRangeValid(pSession, sizeof(*pSession)) &&
		(pSession->Guard == XSSH_SESSION_STREAM_GUARD) &&
		(pSession->State >= XSSH_SESSION_STREAM_CREATED) &&
		(pSession->State <= XSSH_SESSION_STREAM_CLOSED);
}



/* 验证活动驱动仍在绑定 Stream 的所属 Worker 上执行。 */
static bool xsshSessionStreamCurrent(const xsshsessionstream* pSession)
{
	xnetworker* pWorker;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->Stream == NULL) ) {
		return false;
	}
	pWorker = xrtNetStreamWorker(pSession->Stream);
	return (pWorker != NULL) && xrtNetWorkerIsCurrent(pWorker);
}



/* SSH rekey 和等待统一使用 Timer 的 double 秒数。 */
static double xsshSessionStreamNow(void)
{
	return xrtTimer();
}



/* 暂停新的 TCP 读取；已经到达的 completion 仍由同一缓冲链承接。 */
static void xsshSessionStreamPause(xsshsessionstream* pSession)
{
	if ( !pSession->Paused && (pSession->Stream != NULL) ) {
		xrtNetStreamPause(pSession->Stream);
		pSession->Paused = true;
	}
}



/* 仅在没有 HOLD、OOM 或写背压原因时恢复读取。 */
static bool xsshSessionStreamResume(xsshsessionstream* pSession)
{
	if ( !pSession->Paused || pSession->WritePaused ||
		(pSession->State != XSSH_SESSION_STREAM_OPEN) ) {
		return true;
	}
	if ( pSession->ReadEnded ) {
		pSession->Paused = false;
		return true;
	}
	if ( !xrtNetStreamResume(pSession->Stream) ) {
		return false;
	}
	pSession->Paused = false;
	return true;
}



/* 把协议错误补成结构化错误并同步通知应用。 */
static void xsshSessionStreamErrorNotify(
	xsshsessionstream* pSession,
	xsshcode Code,
	xerrkind Kind,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();
	bool bRelevant = (pError != NULL) && (
		(xrtErrorFind(pError, "xrt.ssh", (int32)Code) != NULL) ||
		(((Kind == XERR_IO) || (Kind == XERR_MEMORY)) &&
		 (xrtErrorIs(pError, Kind) != NULL))
	);

	if ( !bRelevant ) {
		xrtSetErrorInfo(Kind, "xrt.ssh", (int32)Code, sMessage);
		pError = xrtGetError();
	}
	if ( pSession->Events.Error != NULL ) {
		pSession->Events.Error(
			pSession,
			Code,
			pError,
			pSession->UserData
		);
	}
}



/* 尽力回滚未接管事务，随后让 Stream 进入唯一异常关闭路径。 */
static void xsshSessionStreamStop(xsshsessionstream* pSession)
{
	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->State >= XSSH_SESSION_STREAM_CLOSING) ) {
		return;
	}
	if ( pSession->SessionReady ) {
		xsshsessionreaderstate ReaderState =
			xrtSshSessionReaderState(&pSession->Reader);
		xsshsessionaction Action =
			xrtSshSessionTcpAction(&pSession->Session);

		if ( (ReaderState != XSSH_SESSION_READER_IDLE) &&
			(ReaderState != XSSH_SESSION_READER_INVALID) ) {
			(void)xrtSshSessionReaderAbort(&pSession->Reader);
		} else if ( Action == XSSH_SESSION_ACTION_READ_PENDING ) {
			(void)xrtSshSessionTcpReadAbort(&pSession->Session);
		}
		if ( xrtSshSessionTcpAction(&pSession->Session) ==
			XSSH_SESSION_ACTION_WRITE_PENDING ) {
			(void)xrtSshSessionTcpWriteAbort(&pSession->Session);
		}
	}
	pSession->State = XSSH_SESSION_STREAM_CLOSING;
	if ( pSession->Stream != NULL ) {
		(void)xrtNetStreamAbort(pSession->Stream);
	}
}



/* 通知非空 rekey 建议；是否开始 KEXINIT 仍由应用决定。 */
static void xsshSessionStreamRekey(
	xsshsessionstream* pSession,
	xsshrekeydecision Decision
)
{
	if ( (Decision != XSSH_REKEY_NONE) &&
		(pSession->Events.Rekey != NULL) ) {
		pSession->Events.Rekey(
			pSession,
			Decision,
			pSession->UserData
		);
	}
}



/* 判断当前动作是否允许从 TCP 缓冲准备一个 SSH packet。 */
static bool xsshSessionStreamPacketAction(xsshsessionaction Action)
{
	return (Action == XSSH_SESSION_ACTION_READ_KEXINIT) ||
		(Action == XSSH_SESSION_ACTION_READ_ECDH_INIT) ||
		(Action == XSSH_SESSION_ACTION_READ_ECDH_REPLY) ||
		(Action == XSSH_SESSION_ACTION_READ_NEWKEYS) ||
		(Action == XSSH_SESSION_ACTION_READ_SERVICE_REQUEST) ||
		(Action == XSSH_SESSION_ACTION_READ_SERVICE_ACCEPT) ||
		(Action == XSSH_SESSION_ACTION_READ_AUTH_REQUEST) ||
		(Action == XSSH_SESSION_ACTION_READ_AUTH_RESULT) ||
		(Action == XSSH_SESSION_ACTION_CONNECTION);
}



/* 只在动作变化时通知，避免未处理动作形成忙循环。 */
static bool xsshSessionStreamActionNotify(
	xsshsessionstream* pSession,
	xsshsessionaction Action
)
{
	xsshsessionaction After;

	if ( pSession->NotifiedAction == Action ) {
		return false;
	}
	pSession->NotifiedAction = Action;
	if ( pSession->Events.Action != NULL ) {
		pSession->Events.Action(
			pSession,
			Action,
			pSession->UserData
		);
	}
	if ( (pSession->State != XSSH_SESSION_STREAM_OPEN) ||
		(xrtNetStreamState(pSession->Stream) != XNET_STREAM_OPEN) ) {
		return true;
	}
	After = xrtSshSessionTcpAction(&pSession->Session);
	return After != Action;
}



/* 自动把上层已经准备好的唯一输出事务交给有界 TCP 队列。 */
static xsshcode xsshSessionStreamWrite(
	xsshsessionstream* pSession,
	bool* pProgress
)
{
	xsshrekeydecision Decision = XSSH_REKEY_NONE;
	xnetresult Result;

	Result = xrtSshSessionTcpWriteSubmit(
		&pSession->Session,
		pSession->Stream,
		xsshSessionStreamNow(),
		&Decision
	);
	if ( Result == XNET_RESULT_AGAIN ) {
		pSession->WritePaused = true;
		xsshSessionStreamPause(pSession);
		return XSSH_OK;
	}
	if ( Result != XNET_RESULT_OK ) {
		xsshSessionStreamErrorNotify(
			pSession,
			XSSH_ERROR_STATE,
			XERR_IO,
			"SSH output could not enter the TCP queue"
		);
		xsshSessionStreamStop(pSession);
		return XSSH_ERROR_STATE;
	}
	pSession->WritePaused = false;
	pSession->NotifiedAction = XSSH_SESSION_ACTION_NONE;
	*pProgress = true;
	xsshSessionStreamRekey(pSession, Decision);
	if ( !xsshSessionStreamResume(pSession) ) {
		xsshSessionStreamErrorNotify(
			pSession,
			XSSH_ERROR_STATE,
			XERR_IO,
			"SSH input could not resume after TCP backpressure"
		);
		xsshSessionStreamStop(pSession);
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 按回调决定提交、保留或拒绝 peer identification。 */
static xsshcode xsshSessionStreamIdentification(
	xsshsessionstream* pSession,
	bool* pProgress
)
{
	xsshsessionstreamdecision Decision = XSSH_SESSION_STREAM_ACCEPT;
	xsshrekeydecision Rekey = XSSH_REKEY_NONE;
	xstrview Version;
	xsshcode Code;

	if ( pSession->Input == NULL ) {
		return XSSH_NEED_MORE;
	}
	Code = xrtSshSessionTcpIdentificationReadPrepare(
		&pSession->Session,
		pSession->Input,
		&Version
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->Version = Version;
	if ( pSession->Events.Identification != NULL ) {
		Decision = pSession->Events.Identification(
			pSession,
			Version,
			pSession->UserData
		);
	}
	if ( Decision == XSSH_SESSION_STREAM_HOLD ) {
		pSession->State = XSSH_SESSION_STREAM_HOLD_IDENTIFICATION;
		xsshSessionStreamPause(pSession);
		return XSSH_OK;
	}
	if ( Decision != XSSH_SESSION_STREAM_ACCEPT ) {
		(void)xrtSshSessionTcpReadAbort(&pSession->Session);
		pSession->State = XSSH_SESSION_STREAM_CLOSING;
		(void)xrtNetStreamAbort(pSession->Stream);
		return Decision == XSSH_SESSION_STREAM_ABORT ?
			XSSH_OK : XSSH_ERROR_CALLBACK;
	}
	Code = xrtSshSessionTcpReadCommit(
		&pSession->Session,
		xsshSessionStreamNow(),
		&Rekey
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->Version = (xstrview){ NULL, 0u };
	pSession->NotifiedAction = XSSH_SESSION_ACTION_NONE;
	*pProgress = true;
	xsshSessionStreamRekey(pSession, Rekey);
	return XSSH_OK;
}



/* 按回调决定提交、保留或拒绝一个已经认证的 SSH packet。 */
static xsshcode xsshSessionStreamPacketRead(
	xsshsessionstream* pSession,
	bool* pProgress
)
{
	xsshsessionstreamdecision Decision = XSSH_SESSION_STREAM_ACCEPT;
	xsshrekeydecision Rekey = XSSH_REKEY_NONE;
	xsshsessiontcppacket Packet;
	xsshcode Code;

	if ( (pSession->Input == NULL) ||
		xrtNetBufEmpty(pSession->Input) ) {
		return XSSH_NEED_MORE;
	}
	Code = xrtSshSessionReaderPrepare(
		&pSession->Reader,
		pSession->Input,
		xsshSessionStreamNow(),
		&Packet
	);
	if ( Code != XSSH_OK ) {
		if ( Code == XSSH_ERROR_SPACE ) {
			pSession->State = XSSH_SESSION_STREAM_RETRY;
			xsshSessionStreamPause(pSession);
		}
		return Code;
	}
	pSession->Packet = Packet;
	if ( pSession->Events.Packet != NULL ) {
		Decision = pSession->Events.Packet(
			pSession,
			&pSession->Packet,
			pSession->UserData
		);
	}
	if ( Decision == XSSH_SESSION_STREAM_HOLD ) {
		pSession->State = XSSH_SESSION_STREAM_HOLD_PACKET;
		xsshSessionStreamPause(pSession);
		return XSSH_OK;
	}
	if ( Decision != XSSH_SESSION_STREAM_ACCEPT ) {
		(void)xrtSshSessionReaderAbort(&pSession->Reader);
		pSession->State = XSSH_SESSION_STREAM_CLOSING;
		(void)xrtNetStreamAbort(pSession->Stream);
		return Decision == XSSH_SESSION_STREAM_ABORT ?
			XSSH_OK : XSSH_ERROR_CALLBACK;
	}
	Code = xrtSshSessionReaderCommit(
		&pSession->Reader,
		xsshSessionStreamNow(),
		&Rekey
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	memset(&pSession->Packet, 0, sizeof(pSession->Packet));
	pSession->NotifiedAction = XSSH_SESSION_ACTION_NONE;
	*pProgress = true;
	xsshSessionStreamRekey(pSession, Rekey);
	return XSSH_OK;
}



/* 推进一步，返回是否发生了可继续循环的状态推进。 */
static xsshcode xsshSessionStreamStep(
	xsshsessionstream* pSession,
	bool* pProgress
)
{
	xsshsessionaction Action;
	xsshcode Code;

	*pProgress = false;
	Action = xrtSshSessionTcpAction(&pSession->Session);
	if ( Action == XSSH_SESSION_ACTION_FAILED ) {
		return XSSH_ERROR_STATE;
	}
	if ( Action == XSSH_SESSION_ACTION_CLOSING ) {
		pSession->State = XSSH_SESSION_STREAM_CLOSING;
		(void)xrtNetStreamClose(pSession->Stream);
		return XSSH_OK;
	}
	if ( Action == XSSH_SESSION_ACTION_READ_PENDING ) {
		return XSSH_ERROR_STATE;
	}
	if ( xsshSessionStreamActionNotify(pSession, Action) ) {
		*pProgress = pSession->State == XSSH_SESSION_STREAM_OPEN;
		return XSSH_OK;
	}
	if ( Action == XSSH_SESSION_ACTION_WRITE_PENDING ) {
		return xsshSessionStreamWrite(pSession, pProgress);
	}
	if ( Action == XSSH_SESSION_ACTION_READ_IDENTIFICATION ) {
		Code = xsshSessionStreamIdentification(pSession, pProgress);
	} else if ( xsshSessionStreamPacketAction(Action) ) {
		Code = xsshSessionStreamPacketRead(pSession, pProgress);
	} else {
		return XSSH_OK;
	}
	if ( (Code != XSSH_OK) && (Code != XSSH_NEED_MORE) ) {
		xsshSessionStreamErrorNotify(
			pSession,
			Code,
			Code == XSSH_ERROR_SPACE ? XERR_MEMORY : XERR_PROTOCOL,
			Code == XSSH_ERROR_SPACE ?
				"SSH stream needs memory before retrying the same input" :
				"SSH stream rejected peer input"
		);
		if ( Code != XSSH_ERROR_SPACE ) {
			xsshSessionStreamStop(pSession);
		}
	}
	return Code;
}



/* 串行推进并合并回调中的递归 Drive 请求。 */
static xsshcode xsshSessionStreamRun(xsshsessionstream* pSession)
{
	xsshcode Result = XSSH_OK;

	if ( pSession->Driving ) {
		pSession->DriveAgain = true;
		return XSSH_OK;
	}
	pSession->Driving = true;
	do {
		bool bProgress;
		xsshcode Code;

		pSession->DriveAgain = false;
		for ( ;; ) {
			if ( pSession->State != XSSH_SESSION_STREAM_OPEN ) {
				break;
			}
			Code = xsshSessionStreamStep(pSession, &bProgress);
			if ( (Code != XSSH_OK) || !bProgress ) {
				if ( Code != XSSH_OK ) {
					Result = Code;
				}
				break;
			}
		}
	} while ( pSession->DriveAgain &&
		(pSession->State == XSSH_SESSION_STREAM_OPEN) );
	pSession->Driving = false;
	return Result;
}



/* EOF 后等待未入队输出完成；空输入正常关闭，残留输入按截断消息拒绝。 */
static xsshcode xsshSessionStreamFinish(
	xsshsessionstream* pSession,
	xsshcode Code
)
{
	xsshsessionaction Action;

	if ( !pSession->ReadEnded ||
		(pSession->State != XSSH_SESSION_STREAM_OPEN) ) {
		return Code;
	}
	if ( (pSession->Input != NULL) &&
		!xrtNetBufEmpty(pSession->Input) ) {
		xsshSessionStreamErrorNotify(
			pSession,
			XSSH_ERROR_PROTOCOL,
			XERR_PROTOCOL,
			"SSH stream ended with a truncated message"
		);
		xsshSessionStreamStop(pSession);
		return XSSH_ERROR_PROTOCOL;
	}
	Action = xrtSshSessionTcpAction(&pSession->Session);
	if ( pSession->WritePaused ||
		(Action == XSSH_SESSION_ACTION_WRITE_PENDING) ) {
		return Code;
	}
	pSession->State = XSSH_SESSION_STREAM_CLOSING;
	(void)xrtNetStreamClose(pSession->Stream);
	return Code;
}



/* 在 Open 或已打开 Attach 路径上绑定 Worker 池并创建唯一会话。 */
static bool xsshSessionStreamStart(
	xsshsessionstream* pSession,
	xnetstream* pStream
)
{
	xnetworker* pWorker;
	xnetbufpool* pPool;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->State != XSSH_SESSION_STREAM_CREATED) ||
		((pSession->Stream != NULL) && (pSession->Stream != pStream)) ) {
		return false;
	}
	pWorker = xrtNetStreamWorker(pStream);
	if ( (pWorker == NULL) || !xrtNetWorkerIsCurrent(pWorker) ) {
		return false;
	}
	pPool = xrtNetWorkerBufPool(pWorker);
	if ( (pPool == NULL) || !xrtSshSessionTcpInit(
		&pSession->Session,
		pPool,
		&pSession->Config,
		xsshSessionStreamNow()
	) ) {
		return false;
	}
	if ( !xrtSshSessionReaderInit(
		&pSession->Reader,
		pPool,
		&pSession->Session
	) ) {
		xrtSshSessionTcpClear(&pSession->Session);
		return false;
	}
	pSession->Stream = pStream;
	pSession->SessionReady = true;
	pSession->State = XSSH_SESSION_STREAM_OPEN;
	if ( pSession->Events.Open != NULL ) {
		pSession->Events.Open(pSession, pSession->UserData);
	}
	if ( pSession->State == XSSH_SESSION_STREAM_OPEN ) {
		(void)xsshSessionStreamRun(pSession);
	}
	return true;
}



/* 复制配置和用户事件，不提前占用 Worker 或网络资源。 */
bool xrtSshSessionStreamInit(
	xsshsessionstream* pSession,
	const xsshsessiontcpconfig* pConfig,
	const xsshsessionstreamevents* pEvents,
	ptr pData
)
{
	xsshsessionstream Session;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pConfig,
			sizeof(*pConfig)
		) || ((pEvents != NULL) && (
			!xrtMemRangeValid(pEvents, sizeof(*pEvents)) ||
			xrtMemRangesOverlap(
				pSession,
				sizeof(*pSession),
				pEvents,
				sizeof(*pEvents)
		))) ) {
		return false;
	}
	memset(&Session, 0, sizeof(Session));
	Session.Config = *pConfig;
	if ( pEvents != NULL ) {
		Session.Events = *pEvents;
	}
	Session.UserData = pData;
	Session.State = XSSH_SESSION_STREAM_CREATED;
	Session.Guard = XSSH_SESSION_STREAM_GUARD;
	*pSession = Session;
	return true;
}



/* 清理未附着或已经完成 Close 清理的驱动。 */
bool xrtSshSessionStreamClear(xsshsessionstream* pSession)
{
	if ( !xsshSessionStreamValid(pSession) ||
		((pSession->State != XSSH_SESSION_STREAM_CREATED) &&
		 (pSession->State != XSSH_SESSION_STREAM_CLOSED)) ) {
		return false;
	}
	memset(pSession, 0, sizeof(*pSession));
	return true;
}



/* 返回可由 TCP client 和 server 共同使用的内部事件表。 */
const xnetstreamevents* xrtSshSessionStreamNetEvents(void)
{
	return &xsshSessionStreamEvents;
}



/* 在 Worker 上替换事件；Open 尚未发布时由网络层稍后完成启动。 */
bool xrtSshSessionStreamAttach(
	xsshsessionstream* pSession,
	xnetstream* pStream
)
{
	xnetstreamstate State;
	xnetworker* pWorker;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->State != XSSH_SESSION_STREAM_CREATED) ||
		(pSession->Stream != NULL) || (pStream == NULL) ) {
		return false;
	}
	pWorker = xrtNetStreamWorker(pStream);
	State = xrtNetStreamState(pStream);
	if ( (pWorker == NULL) || !xrtNetWorkerIsCurrent(pWorker) ||
		(State >= XNET_STREAM_CLOSING) ||
		((State == XNET_STREAM_OPEN) &&
		 (xrtNetStreamAvailable(pStream) != 0u)) ||
		!xrtNetStreamSetEvents(
			pStream,
			&xsshSessionStreamEvents,
			pSession
		) ) {
		return false;
	}
	pSession->Stream = pStream;
	if ( State != XNET_STREAM_OPEN ) {
		return true;
	}
	if ( xsshSessionStreamStart(pSession, pStream) ) {
		return true;
	}
	xsshSessionStreamErrorNotify(
		pSession,
		XSSH_ERROR_STATE,
		XERR_STATE,
		"SSH stream could not bind its Worker buffer pool"
	);
	pSession->State = XSSH_SESSION_STREAM_CLOSING;
	(void)xrtNetStreamAbort(pStream);
	return false;
}



/* 无效对象返回不会与活动状态混淆的独立值。 */
xsshsessionstreamstate xrtSshSessionStreamState(
	const xsshsessionstream* pSession
)
{
	return xsshSessionStreamValid(pSession) ?
		pSession->State : XSSH_SESSION_STREAM_INVALID;
}



/* 借出仍处于连接生命周期中的 TCP Stream。 */
xnetstream* xrtSshSessionStreamTcp(xsshsessionstream* pSession)
{
	return xsshSessionStreamValid(pSession) &&
		(pSession->State != XSSH_SESSION_STREAM_CLOSED) ?
		pSession->Stream : NULL;
}



/* 借出已经绑定 Worker 池的 SSH TCP 会话。 */
xsshsessiontcp* xrtSshSessionStreamSession(
	xsshsessionstream* pSession
)
{
	return xsshSessionStreamValid(pSession) && pSession->SessionReady ?
		&pSession->Session : NULL;
}



/* 借出已经绑定同一 Worker 池的动态 Reader。 */
xsshsessionreader* xrtSshSessionStreamReader(
	xsshsessionstream* pSession
)
{
	return xsshSessionStreamValid(pSession) && pSession->SessionReady ?
		&pSession->Reader : NULL;
}



/* HOLD identification 以外的状态不暴露陈旧视图。 */
xstrview xrtSshSessionStreamVersion(
	const xsshsessionstream* pSession
)
{
	return xsshSessionStreamValid(pSession) &&
		(pSession->State == XSSH_SESSION_STREAM_HOLD_IDENTIFICATION) ?
		pSession->Version : (xstrview){ NULL, 0u };
}



/* HOLD packet 以外的状态不暴露陈旧解析结果。 */
const xsshsessiontcppacket* xrtSshSessionStreamPacket(
	const xsshsessionstream* pSession
)
{
	return xsshSessionStreamValid(pSession) &&
		(pSession->State == XSSH_SESSION_STREAM_HOLD_PACKET) ?
		&pSession->Packet : NULL;
}



/* 显式推进会重新通知当前动作，并允许 RETRY 再次申请内存。 */
xsshcode xrtSshSessionStreamDrive(xsshsessionstream* pSession)
{
	xsshcode Code;

	if ( !xsshSessionStreamCurrent(pSession) ||
		!pSession->SessionReady ||
		((pSession->State != XSSH_SESSION_STREAM_OPEN) &&
		 (pSession->State != XSSH_SESSION_STREAM_RETRY)) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->State == XSSH_SESSION_STREAM_RETRY ) {
		pSession->State = XSSH_SESSION_STREAM_OPEN;
	}
	pSession->NotifiedAction = XSSH_SESSION_ACTION_NONE;
	Code = xsshSessionStreamRun(pSession);
	return xsshSessionStreamFinish(pSession, Code);
}



/* 提交 HOLD 事务，再恢复由驱动暂停的读取。 */
xsshcode xrtSshSessionStreamAccept(xsshsessionstream* pSession)
{
	xsshrekeydecision Decision = XSSH_REKEY_NONE;
	xsshcode Code;

	if ( !xsshSessionStreamCurrent(pSession) ||
		!pSession->SessionReady ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->State == XSSH_SESSION_STREAM_HOLD_IDENTIFICATION ) {
		Code = xrtSshSessionTcpReadCommit(
			&pSession->Session,
			xsshSessionStreamNow(),
			&Decision
		);
	} else if ( pSession->State == XSSH_SESSION_STREAM_HOLD_PACKET ) {
		Code = xrtSshSessionReaderCommit(
			&pSession->Reader,
			xsshSessionStreamNow(),
			&Decision
		);
	} else {
		return XSSH_ERROR_STATE;
	}
	if ( Code != XSSH_OK ) {
		xsshSessionStreamErrorNotify(
			pSession,
			Code,
			XERR_PROTOCOL,
			"SSH held input could not be committed"
		);
		xsshSessionStreamStop(pSession);
		return Code;
	}
	pSession->Version = (xstrview){ NULL, 0u };
	memset(&pSession->Packet, 0, sizeof(pSession->Packet));
	pSession->State = XSSH_SESSION_STREAM_OPEN;
	pSession->NotifiedAction = XSSH_SESSION_ACTION_NONE;
	xsshSessionStreamRekey(pSession, Decision);
	if ( !xsshSessionStreamResume(pSession) ) {
		xsshSessionStreamStop(pSession);
		return XSSH_ERROR_STATE;
	}
	Code = xsshSessionStreamRun(pSession);
	return xsshSessionStreamFinish(pSession, Code);
}



/* 拒绝当前 HOLD，并把底层认证后输入连同连接一起终止。 */
xsshcode xrtSshSessionStreamReject(xsshsessionstream* pSession)
{
	xsshcode Code;

	if ( !xsshSessionStreamCurrent(pSession) ||
		!pSession->SessionReady ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->State == XSSH_SESSION_STREAM_HOLD_IDENTIFICATION ) {
		Code = xrtSshSessionTcpReadAbort(&pSession->Session);
	} else if ( pSession->State == XSSH_SESSION_STREAM_HOLD_PACKET ) {
		Code = xrtSshSessionReaderAbort(&pSession->Reader);
	} else {
		return XSSH_ERROR_STATE;
	}
	pSession->State = XSSH_SESSION_STREAM_CLOSING;
	(void)xrtNetStreamAbort(pSession->Stream);
	return Code;
}



/* 在所属 Worker 上从任意活动状态请求唯一的异常关闭。 */
bool xrtSshSessionStreamAbort(xsshsessionstream* pSession)
{
	if ( !xsshSessionStreamCurrent(pSession) ||
		(pSession->State >= XSSH_SESSION_STREAM_CLOSING) ) {
		return false;
	}
	xsshSessionStreamStop(pSession);
	return true;
}



/* 客户端直连与服务端 Attach 最终都从 Open 绑定 Worker 缓冲池。 */
static void xsshSessionStreamOpen(xnetstream* pStream, ptr pData)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;

	/* Accept 内 Attach 已经完成启动时，忽略网络层随后发布的正式 Open。 */
	if ( xsshSessionStreamValid(pSession) &&
		pSession->SessionReady &&
		(pSession->Stream == pStream) &&
		(pSession->State == XSSH_SESSION_STREAM_OPEN) ) {
		return;
	}
	if ( !xsshSessionStreamStart(pSession, pStream) ) {
		if ( xsshSessionStreamValid(pSession) ) {
			xsshSessionStreamErrorNotify(
				pSession,
				XSSH_ERROR_STATE,
				XERR_STATE,
				"SSH stream could not bind its Worker buffer pool"
			);
			pSession->Stream = pStream;
			pSession->State = XSSH_SESSION_STREAM_CLOSING;
		}
		(void)xrtNetStreamAbort(pStream);
	}
}



/* 借用 Stream 的可变接收链并增量处理所有当前可推进输入。 */
static void xsshSessionStreamRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->Stream != pStream) || !pSession->SessionReady ) {
		(void)xrtNetStreamAbort(pStream);
		return;
	}
	pSession->Input = pBuffer;
	if ( pSession->State == XSSH_SESSION_STREAM_OPEN ) {
		(void)xsshSessionStreamRun(pSession);
	}
}



/* EOF 后先发布事件，再处理已缓存尾部并关闭或拒绝截断报文。 */
static void xsshSessionStreamEnd(xnetstream* pStream, ptr pData)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->Stream != pStream) ) {
		(void)xrtNetStreamAbort(pStream);
		return;
	}
	pSession->ReadEnded = true;
	if ( pSession->Events.End != NULL ) {
		pSession->Events.End(pSession, pSession->UserData);
	}
	if ( pSession->State == XSSH_SESSION_STREAM_OPEN ) {
		xsshcode Code = xsshSessionStreamRun(pSession);

		(void)xsshSessionStreamFinish(pSession, Code);
	}
}



/* 转发首次高水位通知，不改变已被 TCP 接管的输出。 */
static void xsshSessionStreamHighWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;

	if ( xsshSessionStreamValid(pSession) &&
		(pSession->Stream == pStream) &&
		(pSession->Events.HighWater != NULL) ) {
		pSession->Events.HighWater(
			pSession,
			iQueued,
			pSession->UserData
		);
	}
}



/* 低水位允许重试仍由 SSH transport 持有的完整输出事务。 */
static void xsshSessionStreamLowWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;
	xsshcode Code = XSSH_OK;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->Stream != pStream) ) {
		return;
	}
	if ( (pSession->State == XSSH_SESSION_STREAM_OPEN) &&
		pSession->WritePaused ) {
		Code = xsshSessionStreamRun(pSession);
		(void)xsshSessionStreamFinish(pSession, Code);
	}
	if ( (Code == XSSH_OK) && !pSession->WritePaused &&
		(pSession->State == XSSH_SESSION_STREAM_OPEN) &&
		(pSession->Events.LowWater != NULL) ) {
		pSession->Events.LowWater(
			pSession,
			xrtNetStreamPending(pStream),
			pSession->UserData
		);
	}
	(void)iQueued;
}



/* 排空通知与低水位使用相同写重试契约。 */
static void xsshSessionStreamDrain(xnetstream* pStream, ptr pData)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;
	xsshcode Code = XSSH_OK;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->Stream != pStream) ) {
		return;
	}
	if ( (pSession->State == XSSH_SESSION_STREAM_OPEN) &&
		pSession->WritePaused ) {
		Code = xsshSessionStreamRun(pSession);
		(void)xsshSessionStreamFinish(pSession, Code);
	}
	if ( (Code == XSSH_OK) && !pSession->WritePaused &&
		(pSession->State == XSSH_SESSION_STREAM_OPEN) &&
		(xrtNetStreamPending(pStream) == 0u) &&
		(pSession->Events.Drain != NULL) ) {
		pSession->Events.Drain(pSession, pSession->UserData);
	}
}



/* Close 回调期间底层对象仍可检查，回调返回后统一释放 Worker 池动态块。 */
static void xsshSessionStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;

	if ( !xsshSessionStreamValid(pSession) ||
		((pSession->Stream != NULL) &&
		 (pSession->Stream != pStream)) ) {
		return;
	}
	pSession->Stream = pStream;
	pSession->State = XSSH_SESSION_STREAM_CLOSING;
	if ( pSession->Events.Close != NULL ) {
		pSession->Events.Close(
			pSession,
			Result,
			pError,
			pSession->UserData
		);
	}
	if ( pSession->SessionReady ) {
		xrtSshSessionReaderClear(&pSession->Reader);
		xrtSshSessionTcpClear(&pSession->Session);
	}
	pSession->SessionReady = false;
	pSession->Driving = false;
	pSession->DriveAgain = false;
	pSession->Paused = false;
	pSession->WritePaused = false;
	pSession->Input = NULL;
	pSession->Stream = NULL;
	pSession->Version = (xstrview){ NULL, 0u };
	memset(&pSession->Packet, 0, sizeof(pSession->Packet));
	pSession->State = XSSH_SESSION_STREAM_CLOSED;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_session_tcp_random.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_SESSION_TCP_RANDOM)
#include <math.h>



#if defined(XSSH_FEATURE_SESSION_TCP_RANDOM)

/* 随机 KEX 只组合已经分离的会话随机便利层。 */
xsshcode xrtSshSessionTcpKexBegin(
	xsshsessiontcp* pSession,
	xbytesview ServerHostKey
)
{
	xsshsessioncore* pCore = xrtSshSessionTcpCore(pSession);
	xsshtransporttcp* pTransport = xrtSshSessionTcpTransport(pSession);

	if ( (pCore == NULL) || (pTransport == NULL) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(ServerHostKey.Data, ServerHostKey.Size) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			ServerHostKey.Data,
			ServerHostKey.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshSessionCoreKexBegin(
		pCore,
		&pTransport->Core,
		ServerHostKey
	);
}



/* 安全随机 padding 不改变基础闭包的协议事务顺序。 */
xsshcode xrtSshSessionTcpWritePrepare(
	xsshsessiontcp* pSession,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	double Timer,
	xsshsessionpacketkind* pKind
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	return xrtSshSessionTcpWritePrepareWithPadding(
		pSession,
		Payload,
		pChannel,
		pReplies,
		iReplyToken,
		xrtSshSecurePadding,
		NULL,
		Timer,
		pKind
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_client_core.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CLIENT_CORE)
#include <math.h>
#include <string.h>




#if defined(XSSH_FEATURE_CLIENT_CORE)

#define XSSH_CLIENT_CORE_GUARD UINT32_C(0x5343434f)



typedef xsshcode (*xsshclientbuildproc)(xsshwriter* pWriter, ptr pData);



typedef struct xsshclientauthbuild {
	xsshclientcore* Client;
	xsshclientauth Auth;
} xsshclientauthbuild;



/* 验证核心哨兵和动态输出边界。 */
static bool xsshClientCoreValid(const xsshclientcore* pClient)
{
	return xrtMemRangeValid(pClient, sizeof(*pClient)) &&
		(pClient->Guard == XSSH_CLIENT_CORE_GUARD) &&
		pClient->Initialized &&
		(((pClient->OutputCapacity == 0u) &&
		  (pClient->Output == NULL)) ||
		 ((pClient->OutputCapacity != 0u) &&
		  (pClient->Output != NULL))) &&
		(pClient->OutputCapacity <= pClient->Config.OutputLimit);
}



/* 在不改变上限的前提下扩展敏感输出，失败时保留已清零的旧缓冲。 */
static bool xsshClientCoreGrow(xsshclientcore* pClient)
{
	bytes pOutput;
	size_t iCapacity;

	if ( pClient->OutputCapacity >= pClient->Config.OutputLimit ) {
		return false;
	}
	iCapacity = pClient->OutputCapacity;
	if ( iCapacity == 0u ) {
		iCapacity = pClient->Config.OutputInitial;
	} else if ( iCapacity <= (pClient->Config.OutputLimit / 2u) ) {
		iCapacity *= 2u;
	} else {
		iCapacity = pClient->Config.OutputLimit;
	}
	if ( pClient->Output != NULL ) {
		xrtSecureZero(pClient->Output, pClient->OutputCapacity);
		pOutput = (bytes)xrtRealloc(pClient->Output, iCapacity);
	} else {
		pOutput = (bytes)xrtMalloc(iCapacity);
	}
	if ( pOutput == NULL ) {
		return false;
	}
	pClient->Output = pOutput;
	pClient->OutputCapacity = iCapacity;
	return true;
}



/* 对事务式构建器执行有界扩容重试，并发布唯一稳定 payload。 */
static xsshcode xsshClientCoreBuild(
	xsshclientcore* pClient,
	xsshclientbuildproc pBuild,
	ptr pData,
	xbytesview* pPayload
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( (pClient->OutputCapacity == 0u) &&
		!xsshClientCoreGrow(pClient) ) {
		return XSSH_ERROR_SPACE;
	}
	for ( ;; ) {
		xrtSecureZero(pClient->Output, pClient->OutputCapacity);
		if ( !xrtSshWriterInit(
			&Writer,
			pClient->Output,
			pClient->OutputCapacity
		) ) {
			return XSSH_ERROR_STATE;
		}
		Code = pBuild(&Writer, pData);
		if ( Code != XSSH_ERROR_SPACE ) {
			break;
		}
		if ( !xsshClientCoreGrow(pClient) ) {
			return XSSH_ERROR_SPACE;
		}
	}
	if ( Code != XSSH_OK ) {
		xrtSecureZero(pClient->Output, pClient->OutputCapacity);
		return Code;
	}
	*pPayload = (xbytesview){ pClient->Output, Writer.Size };
	return XSSH_OK;
}



/* 构建本轮初始或 rekey KEXINIT。 */
static xsshcode xsshClientCoreKexInitBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	return xrtSshKexInitWriteSecure(
		pWriter,
		(const xsshkexinitconfig*)pData
	);
}



/* 构建客户端 Curve25519 方法首包。 */
static xsshcode xsshClientCoreEcdhBuild(xsshwriter* pWriter, ptr pData)
{
	return xrtSshKexSessionEcdhInitPrepare(
		(xsshkexsession*)pData,
		pWriter
	);
}



/* 构建当前 KEX 的 NEWKEYS。 */
static xsshcode xsshClientCoreNewKeysBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	return xrtSshKexSessionNewKeysPrepare(
		(xsshkexsession*)pData,
		pWriter
	);
}



/* 构建 ssh-userauth service request。 */
static xsshcode xsshClientCoreServiceBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	(void)pData;
	return xrtSshServiceRequestWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
	);
}



/* 首次 none 探测只用于取得服务端支持的方法列表。 */
static xsshcode xsshClientCoreNoneBuild(xsshwriter* pWriter, ptr pData)
{
	return xrtSshAuthNoneWrite(pWriter, *(const xstrview*)pData);
}



/* 把自定义认证器适配到统一的动态构建循环。 */
static xsshcode xsshClientCoreAuthBuild(xsshwriter* pWriter, ptr pData)
{
	xsshclientauthbuild* pBuild = (xsshclientauthbuild*)pData;

	return pBuild->Client->Config.Authenticate(
		pBuild->Client,
		pWriter,
		&pBuild->Auth,
		pBuild->Client->Config.AuthenticateData
	);
}



/* 返回当前 KEX 方法会话。 */
static xsshkexsession* xsshClientCoreKex(xsshsessiontcp* pSession)
{
	xsshsessioncore* pCore = xrtSshSessionTcpCore(pSession);
	xsshkexexchange* pExchange;

	if ( pCore == NULL ) {
		return NULL;
	}
	pExchange = xrtSshSessionCoreKex(pCore);
	return pExchange != NULL ?
		xrtSshKexExchangeSession(pExchange) : NULL;
}



/* 把服务端方法列表复制成独立稳定文本，旧结果只在成功后替换。 */
static xsshcode xsshClientCoreMethodsSet(
	xsshclientcore* pClient,
	xstrview Methods,
	bool bPartialSuccess
)
{
	char* sMethods = NULL;

	if ( Methods.Size == SIZE_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( Methods.Size != 0u ) {
		sMethods = (char*)xrtMalloc(Methods.Size + 1u);
		if ( sMethods == NULL ) {
			return XSSH_ERROR_SPACE;
		}
		memcpy(sMethods, Methods.Data, Methods.Size);
		sMethods[Methods.Size] = '\0';
	}
	xrtFree(pClient->AuthMethods);
	pClient->AuthMethods = sMethods;
	pClient->AuthMethodsSize = Methods.Size;
	pClient->AuthPartialSuccess = bPartialSuccess;
	return XSSH_OK;
}



/* 写入客户端安全默认配置。 */
bool xrtSshClientCoreConfigInit(xsshclientcoreconfig* pConfig)
{
	xsshclientcoreconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		return false;
	}
	memset(&Config, 0, sizeof(Config));
	if ( !xrtSshKexInitConfigInit(
		&Config.Kex,
		XSSH_ROLE_CLIENT,
		true
	) ) {
		return false;
	}
	xrtSshAuthGuardPolicyInit(&Config.AuthGuard);
	Config.Version = XRT_STR_LITERAL(XSSH_CLIENT_VERSION_DEFAULT);
	Config.OutputInitial = XSSH_CLIENT_OUTPUT_INITIAL_DEFAULT;
	Config.OutputLimit = XSSH_CLIENT_OUTPUT_LIMIT_DEFAULT;
	Config.ProbeNone = true;
	*pConfig = Config;
	return true;
}



/* 初始化不拥有会话和 Reader 的客户端动作核心。 */
bool xrtSshClientCoreInit(
	xsshclientcore* pClient,
	const xsshclientcoreconfig* pConfig
)
{
	char arrBanner[XSSH_IDENTIFICATION_MAX + 2u];
	xsshclientcore Client;
	xsshwriter Writer;

	if ( !xrtMemRangeValid(pClient, sizeof(*pClient)) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		xrtMemRangesOverlap(
			pClient,
			sizeof(*pClient),
			pConfig,
			sizeof(*pConfig)
		) || (pConfig->Kex.Role != XSSH_ROLE_CLIENT) ||
		(pConfig->OutputInitial == 0u) ||
		(pConfig->OutputInitial > pConfig->OutputLimit) ||
		!xrtMemRangeValid(pConfig->Version.Data, pConfig->Version.Size) ||
		!xrtMemRangeValid(pConfig->User.Data, pConfig->User.Size) ) {
		return false;
	}
	if ( !xrtSshWriterInit(&Writer, arrBanner, sizeof(arrBanner)) ||
		(xrtSshBannerWrite(&Writer, pConfig->Version) != XSSH_OK) ) {
		return false;
	}
	memset(&Client, 0, sizeof(Client));
	Client.Config = *pConfig;
	Client.Initialized = true;
	Client.Guard = XSSH_CLIENT_CORE_GUARD;
	*pClient = Client;
	return true;
}



/* 清理认证敏感输出与方法副本。 */
void xrtSshClientCoreClear(xsshclientcore* pClient)
{
	if ( xsshClientCoreValid(pClient) ) {
		if ( pClient->Output != NULL ) {
			xrtSecureZero(pClient->Output, pClient->OutputCapacity);
			xrtFree(pClient->Output);
		}
		xrtFree(pClient->AuthMethods);
	}
	if ( xrtMemRangeValid(pClient, sizeof(*pClient)) ) {
		xrtSecureZero(pClient, sizeof(*pClient));
	}
}



/* 推进客户端动作直到出现外部边界。 */
xsshcode xrtSshClientCoreNext(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession,
	const xsshsessionreader* pReader,
	double Timer,
	xsshclientnext* pNext
)
{
	if (!isfinite(Timer) || Timer < 0) { return XSSH_ERROR_ARGUMENT; }
	xsshclientnext Next;
	uint32 iSteps;

	if ( !xsshClientCoreValid(pClient) ||
		!xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		!xrtMemRangeValid(pNext, sizeof(*pNext)) ||
		xrtMemRangesOverlap(
			pClient,
			sizeof(*pClient),
			pNext,
			sizeof(*pNext)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pNext,
			sizeof(*pNext)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	memset(&Next, 0, sizeof(Next));
	for ( iSteps = 0u; iSteps < 32u; ++iSteps ) {
		xsshsessionaction Action = xrtSshSessionTcpAction(pSession);
		xsshcode Code;

		if ( Action == XSSH_SESSION_ACTION_WRITE_IDENTIFICATION ) {
			Next.Kind = XSSH_CLIENT_NEXT_IDENTIFICATION;
			Next.Text = pClient->Config.Version;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_WRITE_KEXINIT ) {
			xsshkexinitconfig Kex = pClient->Config.Kex;
			xsshsessioncore* pCore = xrtSshSessionTcpCore(pSession);
			const xsshkexexchange* pExchange = pCore != NULL ?
				xrtSshSessionCoreKexConst(pCore) : NULL;

			if ( pExchange == NULL ) {
				return XSSH_ERROR_STATE;
			}
			Kex.Initial = !pExchange->Session.HasSessionId;
			Code = xsshClientCoreBuild(
				pClient,
				xsshClientCoreKexInitBuild,
				&Kex,
				&Next.Data
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			Next.Kind = XSSH_CLIENT_NEXT_PAYLOAD;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_BEGIN_KEX ) {
			Code = xrtSshSessionTcpKexBegin(
				pSession,
				(xbytesview){ NULL, 0u }
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			continue;
		}
		if ( Action == XSSH_SESSION_ACTION_WRITE_ECDH_INIT ) {
			xsshkexsession* pKex = xsshClientCoreKex(pSession);

			if ( pKex == NULL ) {
				return XSSH_ERROR_STATE;
			}
			Code = xsshClientCoreBuild(
				pClient,
				xsshClientCoreEcdhBuild,
				pKex,
				&Next.Data
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			Next.Kind = XSSH_CLIENT_NEXT_PAYLOAD;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_VERIFY_HOST_KEY ) {
			xsshkexsession* pKex = xsshClientCoreKex(pSession);
			xsshclienthost Host;
			xsshclienthostdecision Decision;

			if ( (pKex == NULL) ||
				!xrtMemRangeValid(pReader, sizeof(*pReader)) ) {
				return XSSH_ERROR_STATE;
			}
			memset(&Host, 0, sizeof(Host));
			Host.Key = xrtSshSessionReaderHostKey(pReader);
			Code = xrtSshKexSessionNegotiation(
				pKex,
				&Host.Negotiation
			);
			if ( (Code != XSSH_OK) || (Host.Key.Size == 0u) ) {
				return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
			}
			Decision = pClient->Config.HostKey != NULL ?
				pClient->Config.HostKey(
					pClient,
					&Host,
					pClient->Config.HostKeyData
				) : XSSH_CLIENT_HOST_REJECT;
			if ( Decision == XSSH_CLIENT_HOST_DEFER ) {
				Next.Kind = XSSH_CLIENT_NEXT_HOST_KEY;
				*pNext = Next;
				return XSSH_OK;
			}
			if ( Decision != XSSH_CLIENT_HOST_ACCEPT ) {
				xrtSshKexSessionFail(pKex);
				return XSSH_ERROR_AUTHENTICATION;
			}
			Code = xrtSshKexSessionHostKeyAccept(pKex);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			continue;
		}
		if ( Action == XSSH_SESSION_ACTION_WRITE_NEWKEYS ) {
			xsshkexsession* pKex = xsshClientCoreKex(pSession);

			if ( pKex == NULL ) {
				return XSSH_ERROR_STATE;
			}
			Code = xsshClientCoreBuild(
				pClient,
				xsshClientCoreNewKeysBuild,
				pKex,
				&Next.Data
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			Next.Kind = XSSH_CLIENT_NEXT_PAYLOAD;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_BEGIN_AUTH ) {
			Code = xrtSshSessionTcpAuthBegin(
				pSession,
				&pClient->Config.AuthGuard,
				Timer
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			continue;
		}
		if ( Action == XSSH_SESSION_ACTION_WRITE_SERVICE_REQUEST ) {
			Code = xsshClientCoreBuild(
				pClient,
				xsshClientCoreServiceBuild,
				NULL,
				&Next.Data
			);
			if ( Code != XSSH_OK ) {
				return Code;
			}
			Next.Kind = XSSH_CLIENT_NEXT_PAYLOAD;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_WRITE_AUTH_REQUEST ) {
			xsshsessioncore* pCore = xrtSshSessionTcpCore(pSession);
			xsshauthsession* pAuthSession = pCore != NULL ?
				xrtSshSessionCoreAuth(pCore) : NULL;
			xsshkexsession* pKex = xsshClientCoreKex(pSession);
			xsshauthguard Budget;
			xsshclientauthbuild Build;

			if ( (pAuthSession == NULL) || (pKex == NULL) ||
				(xrtSshAuthSessionBudget(
					pAuthSession,
					&Budget
				) != XSSH_OK) ) {
				return XSSH_ERROR_STATE;
			}
			if ( pClient->Config.ProbeNone && (Budget.Attempts == 0u) ) {
				Code = xsshClientCoreBuild(
					pClient,
					xsshClientCoreNoneBuild,
					&pClient->Config.User,
					&Next.Data
				);
			} else if ( pClient->Config.Authenticate == NULL ) {
				Next.Kind = XSSH_CLIENT_NEXT_AUTH;
				*pNext = Next;
				return XSSH_OK;
			} else {
				memset(&Build, 0, sizeof(Build));
				Build.Client = pClient;
				Build.Auth.User = pClient->Config.User;
				Build.Auth.Methods = (xstrview){
					pClient->AuthMethods,
					pClient->AuthMethodsSize
				};
				Build.Auth.Attempts = Budget.Attempts;
				Build.Auth.PartialSuccess =
					pClient->AuthPartialSuccess;
				Code = xrtSshKexSessionId(
					pKex,
					&Build.Auth.SessionId
				);
				if ( Code == XSSH_OK ) {
					Code = xsshClientCoreBuild(
						pClient,
						xsshClientCoreAuthBuild,
						&Build,
						&Next.Data
					);
				}
			}
			if ( Code == XSSH_NEED_MORE ) {
				Next.Kind = XSSH_CLIENT_NEXT_AUTH;
				*pNext = Next;
				return XSSH_OK;
			}
			if ( Code != XSSH_OK ) {
				return Code;
			}
			Next.Kind = XSSH_CLIENT_NEXT_PAYLOAD;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_CONNECTION ) {
			Next.Kind = XSSH_CLIENT_NEXT_READY;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( (Action == XSSH_SESSION_ACTION_READ_IDENTIFICATION) ||
			(Action == XSSH_SESSION_ACTION_READ_KEXINIT) ||
			(Action == XSSH_SESSION_ACTION_READ_ECDH_REPLY) ||
			(Action == XSSH_SESSION_ACTION_READ_NEWKEYS) ||
			(Action == XSSH_SESSION_ACTION_READ_SERVICE_ACCEPT) ||
			(Action == XSSH_SESSION_ACTION_READ_AUTH_RESULT) ) {
			Next.Kind = XSSH_CLIENT_NEXT_INPUT;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( (Action == XSSH_SESSION_ACTION_WRITE_PENDING) ||
			(Action == XSSH_SESSION_ACTION_READ_PENDING) ) {
			Next.Kind = XSSH_CLIENT_NEXT_TRANSACTION;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_CLOSING ) {
			Next.Kind = XSSH_CLIENT_NEXT_CLOSING;
			*pNext = Next;
			return XSSH_OK;
		}
		if ( Action == XSSH_SESSION_ACTION_FAILED ) {
			return XSSH_ERROR_STATE;
		}
		return XSSH_ERROR_STATE;
	}
	return XSSH_ERROR_STATE;
}



/* 保存认证失败方法，使 packet 提交后的认证器不再借用输入。 */
xsshcode xrtSshClientCoreObserve(
	xsshclientcore* pClient,
	const xsshsessiontcp* pSession,
	const xsshsessiontcppacket* pPacket
)
{
	const xsshsessioncore* pCore;
	const xsshauthsession* pAuth;
	xsshauthfailure Failure;

	if ( !xsshClientCoreValid(pClient) ||
		!xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pPacket->Session.Kind != XSSH_SESSION_PACKET_AUTH) ||
		(pPacket->Session.Message.Auth !=
		 XSSH_AUTH_SESSION_PACKET_FAILURE) ) {
		return XSSH_OK;
	}
	pCore = xrtSshSessionTcpCoreConst(pSession);
	pAuth = pCore != NULL ? xrtSshSessionCoreAuthConst(pCore) : NULL;
	if ( (pAuth == NULL) ||
		(xrtSshAuthSessionFailure(pAuth, &Failure) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	return xsshClientCoreMethodsSet(
		pClient,
		Failure.Methods,
		Failure.PartialSuccess
	);
}



/* 接受已完成密码学验证的当前主机密钥。 */
xsshcode xrtSshClientCoreHostKeyAccept(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession
)
{
	xsshkexsession* pKex;

	if ( !xsshClientCoreValid(pClient) ||
		!xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		(xrtSshSessionTcpAction(pSession) !=
		 XSSH_SESSION_ACTION_VERIFY_HOST_KEY) ) {
		return XSSH_ERROR_STATE;
	}
	pKex = xsshClientCoreKex(pSession);
	return pKex != NULL ?
		xrtSshKexSessionHostKeyAccept(pKex) : XSSH_ERROR_STATE;
}



/* 拒绝当前主机密钥并终止本轮 KEX。 */
xsshcode xrtSshClientCoreHostKeyReject(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession
)
{
	xsshkexsession* pKex;

	if ( !xsshClientCoreValid(pClient) ||
		!xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		(xrtSshSessionTcpAction(pSession) !=
		 XSSH_SESSION_ACTION_VERIFY_HOST_KEY) ) {
		return XSSH_ERROR_STATE;
	}
	pKex = xsshClientCoreKex(pSession);
	if ( pKex == NULL ) {
		return XSSH_ERROR_STATE;
	}
	xrtSshKexSessionFail(pKex);
	return XSSH_OK;
}



/* 返回稳定认证方法副本。 */
xstrview xrtSshClientCoreAuthMethods(
	const xsshclientcore* pClient,
	bool* pPartialSuccess
)
{
	if ( pPartialSuccess != NULL ) {
		*pPartialSuccess = false;
	}
	if ( !xsshClientCoreValid(pClient) ) {
		return (xstrview){ NULL, 0u };
	}
	if ( pPartialSuccess != NULL ) {
		*pPartialSuccess = pClient->AuthPartialSuccess;
	}
	return (xstrview){
		pClient->AuthMethods,
		pClient->AuthMethodsSize
	};
}



/* 使用显式借用口令构建 password 认证。 */
xsshcode xrtSshClientPasswordAuth(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pUserData
)
{
	const xstrview* pPassword = (const xstrview*)pUserData;

	(void)pClient;
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pAuth, sizeof(*pAuth)) ||
		!xrtMemRangeValid(pPassword, sizeof(*pPassword)) ||
		!xrtMemRangeValid(pPassword->Data, pPassword->Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pAuth->Methods.Size != 0u) &&
		!xrtSshNameListContains(
			pAuth->Methods,
			XRT_STR_LITERAL(XSSH_AUTH_METHOD_PASSWORD)
		) ) {
		return XSSH_ERROR_AUTHENTICATION;
	}
	return xrtSshAuthPasswordWrite(
		pWriter,
		pAuth->User,
		*pPassword
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_client_auth_ed25519.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CLIENT_AUTH_ED25519)



#if defined(XSSH_FEATURE_CLIENT_AUTH_ED25519)

#define XSSH_CLIENT_ED25519_SIGNATURE_CAPACITY \
	(8u + (sizeof(XSSH_HOSTKEY_ED25519) - 1u) + \
	 XSSH_ED25519_SIGNATURE_SIZE)



/* 使用客户端动态输出暂存签名原文，再把固定签名原子写成最终认证报文。 */
xsshcode xrtSshClientEd25519Auth(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pUserData
)
{
	const xsshed25519identity* pIdentity =
		(const xsshed25519identity*)pUserData;
	unsigned char arrSignature[XSSH_CLIENT_ED25519_SIGNATURE_CAPACITY];
	xsshwriter SignData;
	xsshwriter Signature;
	xsshwriter Writer;
	xsshcode Code;

	(void)pClient;
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pAuth, sizeof(*pAuth)) ||
		!xrtMemRangeValid(pIdentity, sizeof(*pIdentity)) ||
		(pWriter->Size != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pAuth->Methods.Size != 0u) &&
		!xrtSshNameListContains(
			pAuth->Methods,
			XRT_STR_LITERAL(XSSH_AUTH_METHOD_PUBLICKEY)
		) ) {
		return XSSH_ERROR_AUTHENTICATION;
	}
	SignData = *pWriter;
	Code = xrtSshAuthPublicKeySignDataWrite(
		&SignData,
		pAuth->SessionId,
		pAuth->User,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
		pIdentity->PublicKeyBlob
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshWriterInit(
		&Signature,
		arrSignature,
		sizeof(arrSignature)
	) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshPrivateKeyEd25519SignatureWrite(
		&Signature,
		pIdentity,
		(xbytesview){ SignData.Data, SignData.Size }
	);
	if ( Code == XSSH_OK ) {
		Writer = *pWriter;
		Code = xrtSshAuthPublicKeySignedWrite(
			&Writer,
			pAuth->User,
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			pIdentity->PublicKeyBlob,
			(xbytesview){ arrSignature, Signature.Size }
		);
		if ( Code == XSSH_OK ) {
			*pWriter = Writer;
		}
	}
	xrtSecureZero(arrSignature, sizeof(arrSignature));
	return Code;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_client.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CLIENT)
#include <string.h>


#if defined(XSSH_FEATURE_CLIENT_FUTURE)
#endif



#if defined(XSSH_FEATURE_CLIENT)

#define XSSH_CLIENT_GUARD UINT32_C(0x53434c54)



static bool xsshClientValid(const xsshclient* pClient);
static void xsshClientOpen(xsshsessionstream* pStream, ptr pData);
static void xsshClientAction(
	xsshsessionstream* pStream,
	xsshsessionaction Action,
	ptr pData
);
static xsshsessionstreamdecision xsshClientIdentification(
	xsshsessionstream* pStream,
	xstrview Version,
	ptr pData
);
static xsshsessionstreamdecision xsshClientPacket(
	xsshsessionstream* pStream,
	const xsshsessiontcppacket* pPacket,
	ptr pData
);
static void xsshClientRekey(
	xsshsessionstream* pStream,
	xsshrekeydecision Decision,
	ptr pData
);
static void xsshClientError(
	xsshsessionstream* pStream,
	xsshcode Code,
	const xerror* pError,
	ptr pData
);
static void xsshClientEnd(xsshsessionstream* pStream, ptr pData);
static void xsshClientHighWater(
	xsshsessionstream* pStream,
	size_t iQueued,
	ptr pData
);
static void xsshClientLowWater(
	xsshsessionstream* pStream,
	size_t iQueued,
	ptr pData
);
static void xsshClientDrain(xsshsessionstream* pStream, ptr pData);
static void xsshClientClose(
	xsshsessionstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
);
static void xsshClientReadyTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
);

#if defined(XSSH_FEATURE_CLIENT_FUTURE)
static void xsshClientChannelRemoved(
	xsshchannels* pChannels,
	uint32 iLocal,
	ptr pData
);
#endif



static const xsshsessionstreamevents xsshClientEvents = {
	xsshClientOpen,
	xsshClientAction,
	xsshClientIdentification,
	xsshClientPacket,
	xsshClientRekey,
	xsshClientError,
	xsshClientEnd,
	xsshClientHighWater,
	xsshClientLowWater,
	xsshClientDrain,
	xsshClientClose
};



#if defined(XSSH_FEATURE_CLIENT_FUTURE)
/* 集合删除后关闭对应等待，并清除尚未发布的可写借用。 */
static void xsshClientChannelRemoved(
	xsshchannels* pChannels,
	uint32 iLocal,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;
	xsshclientfuturenotice Notice;

	if ( !xsshClientValid(pClient) ||
		(pChannels != &pClient->Channels) ) {
		return;
	}
	if ( (pClient->FutureWritableChannel != NULL) &&
		(pClient->FutureWritableLocal == iLocal) ) {
		pClient->FutureWritableChannel = NULL;
		pClient->FutureWritableLocal = 0u;
	}
	memset(&Notice, 0, sizeof(Notice));
	Notice.Signal = XSSH_CLIENT_FUTURE_CHANNEL_REMOVED;
	Notice.ChannelLocal = iLocal;
	Notice.HasChannelLocal = true;
	__xrtSshClientFutureNotify(pClient, &Notice);
}
#endif



/* Channel open 回调上下文只在同步构建调用期间借用。 */
typedef struct xsshclientopenbuild {
	xsshclientchannelopenproc Open;
	xsshchannel* Channel;
	ptr UserData;
} xsshclientopenbuild;



/* Channel 数据构建上下文只保存稳定 channel 和流方向。 */
typedef struct xsshclientflushbuild {
	xsshchannel* Channel;
	xsshchanneliostream Stream;
} xsshclientflushbuild;



/* Peer open 响应只在读提交后的同步构建期间存在。 */
typedef struct xsshclientopenresponse {
	xsshchannel* Channel;
	xsshclientchanneldecision Decision;
	uint32 Reason;
} xsshclientopenresponse;



/* 验证客户端哨兵和资源生命周期，不触碰尚未初始化的动态对象。 */
static bool xsshClientValid(const xsshclient* pClient)
{
	return xrtMemRangeValid(pClient, sizeof(*pClient)) &&
		(pClient->Guard == XSSH_CLIENT_GUARD) &&
		(pClient->State >= XSSH_CLIENT_CREATED) &&
		(pClient->State <= XSSH_CLIENT_CLOSED);
}



/* 验证调用发生在已附着 Stream 的所属 Worker。 */
static bool xsshClientCurrent(const xsshclient* pClient)
{
	xnetstream* pStream;
	xnetworker* pWorker;

	if ( !xsshClientValid(pClient) ) {
		return false;
	}
	pStream = pClient->Stream.Stream;
	if ( pStream == NULL ) {
		return false;
	}
	pWorker = xrtNetStreamWorker(pStream);
	return (pWorker != NULL) && xrtNetWorkerIsCurrent(pWorker);
}



/* 验证 channel 来自当前客户端的动态集合。 */
static bool xsshClientChannelOwned(
	const xsshclient* pClient,
	const xsshchannel* pChannel
)
{
	if ( !xsshClientValid(pClient) || !pClient->ResourcesReady ||
		!xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!pChannel->Initialized ) {
		return false;
	}
	return xrtSshChannelsConstGet(
		&pClient->Channels,
		pChannel->Core.Local
	) == pChannel;
}



/* 返回当前未提交 peer CHANNEL_OPEN，其他 packet 不可伪造决定。 */
static const xsshchannelopen* xsshClientCurrentOpen(
	xsshclient* pClient
)
{
	const xsshsessiontcppacket* pPacket;

	if ( !xsshClientCurrent(pClient) ) {
		return NULL;
	}
	if ( pClient->OpenCurrent != NULL ) {
		return pClient->OpenCurrent;
	}
	pPacket = xrtSshSessionStreamPacket(&pClient->Stream);
	return (pPacket != NULL) &&
		(pPacket->Session.Kind == XSSH_SESSION_PACKET_CONNECTION) &&
		(pPacket->Session.Message.Connection.Kind ==
		 XSSH_CONNECTION_PACKET_CHANNEL_OPEN) ?
		&pPacket->Session.Message.Connection.Message.ChannelOpen : NULL;
}



/* 丢弃尚未进入写事务的 peer open 决定。 */
static void xsshClientOpenPendingDiscard(xsshclient* pClient)
{
	xsshchannel* pChannel = pClient->OpenPendingChannel;

	pClient->OpenPendingChannel = NULL;
	pClient->OpenDecision = XSSH_CLIENT_CHANNEL_NONE;
	pClient->OpenReason = 0u;
	if ( pChannel != NULL ) {
		(void)xrtSshChannelsDiscard(
			&pClient->Channels,
			pChannel->Core.Local
		);
	}
}



/* 暂存 peer open 决定，并为 confirmation/failure 保留稳定 channel。 */
static xsshcode xsshClientOpenStage(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	xsshclientchanneldecision Decision,
	uint32 iReason,
	xsshchannel** ppChannel
)
{
	const xsshchannelopen* pCurrent = xsshClientCurrentOpen(pClient);
	xsshchannel* pChannel;
	xsshcode Code;

	if ( (pCurrent == NULL) || (pOpen != pCurrent) ||
		(pClient->OpenDecision != XSSH_CLIENT_CHANNEL_NONE) ||
		(pClient->OpenPendingChannel != NULL) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshChannelsAccept(
		&pClient->Channels,
		pOpen,
		&pChannel
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pClient->OpenPendingChannel = pChannel;
	pClient->OpenDecision = Decision;
	pClient->OpenReason = iReason;
	if ( ppChannel != NULL ) {
		*ppChannel = pChannel;
	}
	return XSSH_OK;
}



/* 把公开类型化 open 回调适配为通用控制报文构建器。 */
static xsshcode xsshClientChannelOpenBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshclientopenbuild* pBuild = (xsshclientopenbuild*)pData;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		(pBuild->Open == NULL) ||
		!xrtMemRangeValid(pBuild->Channel, sizeof(*pBuild->Channel)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return pBuild->Open(
		pWriter,
		&pBuild->Channel->Core,
		pBuild->UserData
	);
}



/* 从 channel I/O 队首构建一条暂存发送事务。 */
static xsshcode xsshClientChannelFlushBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshclientflushbuild* pBuild = (xsshclientflushbuild*)pData;
	xbytesview Payload;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		!xrtMemRangeValid(pBuild->Channel, sizeof(*pBuild->Channel)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshChannelIoSendPrepare(
		&pBuild->Channel->Io,
		pBuild->Stream,
		pWriter,
		&Payload
	);
}



/* 从 channel 当前远端编号构建窗口返还。 */
static xsshcode xsshClientChannelAdjustBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshchannel* pChannel = (xsshchannel*)pData;
	uint32 iLocal;
	uint32 iRemote;
	uint32 iBytes;

	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!xrtSshChannelCoreIds(&pChannel->Core, &iLocal, &iRemote) ) {
		return XSSH_ERROR_STATE;
	}
	(void)iLocal;
	iBytes = xrtSshChannelCoreAdjustLimit(&pChannel->Core);
	return iBytes != 0u ? xrtSshChannelWindowAdjustWrite(
		pWriter,
		iRemote,
		iBytes
	) : XSSH_NEED_MORE;
}



/* 从 channel 当前远端编号构建 EOF。 */
static xsshcode xsshClientChannelEofBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshchannel* pChannel = (xsshchannel*)pData;
	uint32 iLocal;
	uint32 iRemote;

	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!xrtSshChannelCoreIds(&pChannel->Core, &iLocal, &iRemote) ) {
		return XSSH_ERROR_STATE;
	}
	(void)iLocal;
	return xrtSshChannelEofWrite(pWriter, iRemote);
}



/* 从 channel 当前远端编号构建 CLOSE。 */
static xsshcode xsshClientChannelCloseBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshchannel* pChannel = (xsshchannel*)pData;
	uint32 iLocal;
	uint32 iRemote;

	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!xrtSshChannelCoreIds(&pChannel->Core, &iLocal, &iRemote) ) {
		return XSSH_ERROR_STATE;
	}
	(void)iLocal;
	return xrtSshChannelCloseWrite(pWriter, iRemote);
}



/* 在 peer open 读事务完成后构建 confirmation 或 failure。 */
static xsshcode xsshClientOpenResponseBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshclientopenresponse* pBuild = (xsshclientopenresponse*)pData;
	uint32 iLocal;
	uint32 iRemote;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		!xrtMemRangeValid(pBuild->Channel, sizeof(*pBuild->Channel)) ||
		!xrtSshChannelCoreIds(
			&pBuild->Channel->Core,
			&iLocal,
			&iRemote
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pBuild->Decision == XSSH_CLIENT_CHANNEL_ACCEPT ) {
		return xrtSshChannelOpenConfirmationWrite(
			pWriter,
			iRemote,
			iLocal,
			pBuild->Channel->Core.Window.ReceiveWindow,
			pBuild->Channel->Core.Window.ReceiveMaxPacket,
			(xbytesview){ NULL, 0u }
		);
	}
	if ( pBuild->Decision == XSSH_CLIENT_CHANNEL_REJECT ) {
		return xrtSshChannelOpenFailureWrite(
			pWriter,
			iRemote,
			pBuild->Reason,
			XRT_STR_LITERAL(""),
			XRT_STR_LITERAL("")
		);
	}
	return XSSH_ERROR_STATE;
}



/* 写事务提交后开放被接受 channel，或回收已拒绝 channel。 */
static xsshcode xsshClientOpenSendCommit(xsshclient* pClient)
{
	xsshchannel* pChannel = pClient->OpenSendChannel;
	xsshclientchannelnotice Notice;
	xsshchannelcorephase Phase;
	uint32 iLocal;

	if ( pChannel == NULL ) {
		return XSSH_OK;
	}
	iLocal = pChannel->Core.Local;
	Phase = xrtSshChannelCorePhase(&pChannel->Core);
	if ( Phase == XSSH_CHANNEL_CORE_ACCEPTING ) {
		return XSSH_OK;
	}
	pClient->OpenSendChannel = NULL;
	if ( Phase == XSSH_CHANNEL_CORE_OPEN ) {
		memset(&Notice, 0, sizeof(Notice));
		Notice.Channel = pChannel;
		Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_OPENED;
		Notice.Incoming = true;
		if ( pClient->Events.Channel != NULL ) {
			pClient->Events.Channel(
				pClient,
				&Notice,
				pClient->UserData
			);
		}
		return XSSH_OK;
	}
	if ( Phase == XSSH_CHANNEL_CORE_FAILED ) {
		return xrtSshChannelsDiscard(
			&pClient->Channels,
			iLocal
		) ? XSSH_OK : XSSH_ERROR_STATE;
	}
	return XSSH_ERROR_STATE;
}



/* 把已提交 peer open 决定转为唯一线路写事务。 */
static xsshcode xsshClientOpenResponse(xsshclient* pClient)
{
	xsshclientopenresponse Build;
	xsshchannel* pChannel;
	xsshcode Code;

	if ( pClient->OpenDecision == XSSH_CLIENT_CHANNEL_NONE ) {
		return XSSH_OK;
	}
	pChannel = pClient->OpenPendingChannel;
	if ( pChannel == NULL ) {
		return XSSH_ERROR_STATE;
	}
	Build.Channel = pChannel;
	Build.Decision = pClient->OpenDecision;
	Build.Reason = pClient->OpenReason;
	pClient->OpenPendingChannel = NULL;
	pClient->OpenDecision = XSSH_CLIENT_CHANNEL_NONE;
	pClient->OpenReason = 0u;
	pClient->OpenSendChannel = pChannel;
	Code = xrtSshClientBuild(
		pClient,
		xsshClientOpenResponseBuild,
		&Build,
		pChannel,
		NULL,
		0u
	);
	if ( Code != XSSH_OK ) {
		pClient->OpenSendChannel = NULL;
		(void)xrtSshChannelsDiscard(
			&pClient->Channels,
			pChannel->Core.Local
		);
	}
	return Code;
}



/* 把客户端层错误发布给用户，但不覆盖底层已经存在的结构化错误。 */
static void xsshClientErrorReport(
	xsshclient* pClient,
	xsshcode Code,
	xerrkind Kind,
	cstr sMessage,
	bool bTerminal
)
{
	const xerror* pError = xrtGetError();
	bool bRelevant = (pError != NULL) && (
		(xrtErrorFind(pError, "xrt.ssh", (int32)Code) != NULL) ||
		(((Kind == XERR_IO) || (Kind == XERR_MEMORY)) &&
		 (xrtErrorIs(pError, Kind) != NULL))
	);

	if ( !bRelevant ) {
		xrtSetErrorInfo(Kind, "xrt.ssh.client", (int32)Code, sMessage);
		pError = xrtGetError();
	}
	if ( bTerminal && xsshClientValid(pClient) &&
		(pClient->TerminalError == NULL) && (pError != NULL) ) {
		pClient->TerminalError = xrtErrorRef(pError);
	}
	if ( xsshClientValid(pClient) && (pClient->Events.Error != NULL) ) {
		pClient->Events.Error(
			pClient,
			Code,
			pError,
			pClient->UserData
		);
	}
}



/* 发布会终结连接或其全部未决操作的错误。 */
static void xsshClientErrorNotify(
	xsshclient* pClient,
	xsshcode Code,
	xerrkind Kind,
	cstr sMessage
)
{
	xsshClientErrorReport(pClient, Code, Kind, sMessage, true);
}



/* 发布可在同一输入事务上显式重试的错误，不污染未来关闭终态。 */
static void xsshClientRetryNotify(
	xsshclient* pClient,
	xsshcode Code,
	xerrkind Kind,
	cstr sMessage
)
{
	xsshClientErrorReport(pClient, Code, Kind, sMessage, false);
}



/* 取消就绪截止时间；Timer 的取消回调会因 ID 已清零而成为空操作。 */
static void xsshClientReadyTimerCancel(xsshclient* pClient)
{
	xnetstream* pStream;
	xnetworker* pWorker;
	xnetengine* pEngine;
	uint64 Id;

	if ( pClient->ReadyTimer == 0u ) {
		return;
	}
	pStream = xrtSshSessionStreamTcp(&pClient->Stream);
	pWorker = pStream != NULL ? xrtNetStreamWorker(pStream) : NULL;
	pEngine = pWorker != NULL ? xrtNetWorkerEngine(pWorker) : NULL;
	Id = pClient->ReadyTimer;
	pClient->ReadyTimer = 0u;
	if ( pEngine != NULL ) {
		(void)xrtNetEngineTimerCancelCurrent(pEngine, Id);
	}
}



/* 为 TCP 建连后的 SSH identification、KEX 和认证建立统一截止时间。 */
static bool xsshClientReadyTimerStart(xsshclient* pClient)
{
	xnetstream* pStream;
	xnetworker* pWorker;
	xnetengine* pEngine;
	uint64 Id;

	if ( pClient->Config.ReadyTimeout == 0u ) {
		return true;
	}
	pStream = xrtSshSessionStreamTcp(&pClient->Stream);
	pWorker = pStream != NULL ? xrtNetStreamWorker(pStream) : NULL;
	pEngine = pWorker != NULL ? xrtNetWorkerEngine(pWorker) : NULL;
	if ( pEngine == NULL ) {
		return false;
	}
	Id = xrtNetEngineAfter(
		pEngine,
		xrtNetWorkerIndex(pWorker),
		pClient->Config.ReadyTimeout,
		xsshClientReadyTimer,
		pClient
	);
	if ( Id == 0u ) {
		return false;
	}
	pClient->ReadyTimer = Id;
	return true;
}



/* 就绪截止时间只终结仍处于握手阶段的同一客户端生命周期。 */
static void xsshClientReadyTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pWorker;
	if ( !xsshClientValid(pClient) ||
		(pClient->ReadyTimer != Id) ) {
		return;
	}
	pClient->ReadyTimer = 0u;
	if ( (Result == XNET_RESULT_CANCELLED) ||
		(Result == XNET_RESULT_CLOSED) ||
		(pClient->State != XSSH_CLIENT_HANDSHAKE) ) {
		return;
	}
	if ( Result == XNET_RESULT_OK ) {
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_TIMEOUT,
			XERR_TIMEOUT,
			"SSH client did not become ready before its deadline"
		);
	} else {
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_STATE,
			XERR_STATE,
			"SSH client ready timer terminated unexpectedly"
		);
	}
	pClient->State = XSSH_CLIENT_CLOSING;
	(void)xrtSshSessionStreamAbort(&pClient->Stream);
}



/* 把正常等待输入的 Drive 返回值收敛为调用成功。 */
static xsshcode xsshClientDrive(xsshclient* pClient)
{
	xsshcode Code = xrtSshSessionStreamDrive(&pClient->Stream);

	return Code == XSSH_NEED_MORE ? XSSH_OK : Code;
}



/* 在 packet 提交后原子发布 channel I/O，再通知应用读取。 */
static xsshcode xsshClientReceiveCommit(xsshclient* pClient)
{
	xsshchannel* pChannel;
	xsshchanneliostream Stream;
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		uint32 iLocal;
	#endif

	if ( !pClient->ReceivePending ) {
		return XSSH_OK;
	}
	pChannel = pClient->ReceiveChannel;
	Stream = pClient->ReceiveStream;
	pClient->ReceiveChannel = NULL;
	pClient->ReceivePending = false;
	if ( (pChannel == NULL) ||
		(xrtSshChannelIoReceiveCommit(&pChannel->Io) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		iLocal = pChannel->Core.Local;
	#endif
	if ( pClient->Events.Data != NULL ) {
		pClient->Events.Data(
			pClient,
			pChannel,
			Stream,
			pClient->UserData
		);
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	{
		xsshclientfuturenotice FutureNotice;

		memset(&FutureNotice, 0, sizeof(FutureNotice));
		FutureNotice.Signal = XSSH_CLIENT_FUTURE_DATA;
		FutureNotice.Channel = pChannel;
		FutureNotice.Stream = Stream;
		FutureNotice.ChannelLocal = iLocal;
		FutureNotice.HasChannelLocal = true;
		__xrtSshClientFutureNotify(pClient, &FutureNotice);
	}
	#endif
	return XSSH_OK;
}



/* 把已提交 connection 状态转换为不借用 packet 的稳定通知。 */
static xsshcode xsshClientChannelNoticeCommit(xsshclient* pClient)
{
	xsshclientchannelnotice Notice;
	xsshchannelcorephase Phase;
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		uint32 iLocal;
	#endif

	if ( !pClient->ChannelNoticePending ) {
		return XSSH_OK;
	}
	Notice = pClient->ChannelNotice;
	memset(&pClient->ChannelNotice, 0, sizeof(pClient->ChannelNotice));
	pClient->ChannelNoticePending = false;
	if ( !xsshClientChannelOwned(pClient, Notice.Channel) ) {
		return XSSH_ERROR_STATE;
	}
	Phase = xrtSshChannelCorePhase(&Notice.Channel->Core);
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		iLocal = Notice.Channel->Core.Local;
	#endif
	if ( ((Notice.Event == XSSH_CLIENT_CHANNEL_EVENT_OPENED) &&
		 (Phase != XSSH_CHANNEL_CORE_OPEN)) ||
		((Notice.Event == XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED) &&
		 (Phase != XSSH_CHANNEL_CORE_FAILED)) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pClient->Events.Channel != NULL ) {
		pClient->Events.Channel(
			pClient,
			&Notice,
			pClient->UserData
		);
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	{
		xsshclientfuturenotice FutureNotice;

		memset(&FutureNotice, 0, sizeof(FutureNotice));
		FutureNotice.Signal = XSSH_CLIENT_FUTURE_CHANNEL;
		FutureNotice.Channel = Notice.Channel;
		FutureNotice.ChannelNotice = &Notice;
		FutureNotice.ChannelLocal = iLocal;
		FutureNotice.HasChannelLocal = true;
		__xrtSshClientFutureNotify(pClient, &FutureNotice);
	}
	#endif
	return XSSH_OK;
}



/* 在全局回复和对应 FIFO 出队提交后发布稳定 token。 */
static xsshcode xsshClientGlobalNoticeCommit(xsshclient* pClient)
{
	xsshclientglobalnotice Notice;

	if ( !pClient->GlobalNoticePending ) {
		return XSSH_OK;
	}
	Notice = pClient->GlobalNotice;
	memset(&pClient->GlobalNotice, 0, sizeof(pClient->GlobalNotice));
	pClient->GlobalNoticePending = false;
	if ( pClient->Events.Global != NULL ) {
		pClient->Events.Global(
			pClient,
			&Notice,
			pClient->UserData
		);
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	{
		xsshclientfuturenotice FutureNotice;

		memset(&FutureNotice, 0, sizeof(FutureNotice));
		FutureNotice.Signal = XSSH_CLIENT_FUTURE_GLOBAL;
		FutureNotice.GlobalNotice = &Notice;
		__xrtSshClientFutureNotify(pClient, &FutureNotice);
	}
	#endif
	return XSSH_OK;
}



/* 在 SSH 与 transport 写事务提交后消费 channel I/O 队首。 */
static xsshcode xsshClientSendCommit(xsshclient* pClient)
{
	xsshchannel* pChannel;
	xsshcode Code;

	if ( !pClient->SendPending ) {
		return XSSH_OK;
	}
	pChannel = pClient->SendChannel;
	pClient->SendChannel = NULL;
	pClient->SendPending = false;
	Code = pChannel != NULL ?
		xrtSshChannelIoSendCommit(&pChannel->Io) : XSSH_ERROR_STATE;
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	if ( Code == XSSH_OK ) {
		pClient->FutureWritableChannel = pChannel;
		pClient->FutureWritableLocal = pChannel->Core.Local;
	}
	#endif
	return Code;
}



/* 为需要提交后发布的 channel 状态保存稳定对象和标量。 */
static xsshcode xsshClientChannelNoticePrepare(
	xsshclient* pClient,
	const xsshsessiontcppacket* pPacket
)
{
	const xsshconnectionpacket* pConnection;
	xsshclientchannelnotice Notice;
	uint32 iRecipient;

	if ( pPacket->Session.Kind != XSSH_SESSION_PACKET_CONNECTION ) {
		return XSSH_OK;
	}
	pConnection = &pPacket->Session.Message.Connection;
	memset(&Notice, 0, sizeof(Notice));
	switch ( pConnection->Kind ) {
		case XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION:
			iRecipient = pConnection->Message.ChannelConfirmation.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_OPENED;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE:
			iRecipient = pConnection->Message.ChannelOpenFailure.Recipient;
			Notice.Reason = pConnection->Message.ChannelOpenFailure.Reason;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_ADJUST:
			iRecipient = pConnection->Message.ChannelAdjust.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_WRITABLE;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS:
			iRecipient = pConnection->Message.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS;
			Notice.ReplyToken = pConnection->ReplyToken;
			Notice.HasReplyToken = pConnection->HasReplyToken;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_FAILURE:
			iRecipient = pConnection->Message.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE;
			Notice.ReplyToken = pConnection->ReplyToken;
			Notice.HasReplyToken = pConnection->HasReplyToken;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_EOF:
			iRecipient = pConnection->Message.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_EOF;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_CLOSE:
			iRecipient = pConnection->Message.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_CLOSED;
			break;
		default:
			return XSSH_OK;
	}
	if ( pClient->ChannelNoticePending ) {
		return XSSH_ERROR_STATE;
	}
	Notice.Channel = xrtSshChannelsGet(&pClient->Channels, iRecipient);
	if ( (Notice.Channel == NULL) ||
		(((Notice.Event == XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS) ||
		  (Notice.Event == XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE)) &&
		 !Notice.HasReplyToken) ) {
		return XSSH_ERROR_STATE;
	}
	pClient->ChannelNotice = Notice;
	pClient->ChannelNoticePending = true;
	return XSSH_OK;
}



/* 为全局 success/failure 保存已关联但尚未提交的回复 token。 */
static xsshcode xsshClientGlobalNoticePrepare(
	xsshclient* pClient,
	const xsshsessiontcppacket* pPacket
)
{
	const xsshconnectionpacket* pConnection;
	xsshclientglobalnotice Notice;

	if ( pPacket->Session.Kind != XSSH_SESSION_PACKET_CONNECTION ) {
		return XSSH_OK;
	}
	pConnection = &pPacket->Session.Message.Connection;
	if ( pConnection->Kind == XSSH_CONNECTION_PACKET_GLOBAL_SUCCESS ) {
		Notice.Event = XSSH_CLIENT_GLOBAL_EVENT_REQUEST_SUCCESS;
	} else if ( pConnection->Kind ==
		XSSH_CONNECTION_PACKET_GLOBAL_FAILURE ) {
		Notice.Event = XSSH_CLIENT_GLOBAL_EVENT_REQUEST_FAILURE;
	} else {
		return XSSH_OK;
	}
	if ( pClient->GlobalNoticePending || !pConnection->HasReplyToken ) {
		return XSSH_ERROR_STATE;
	}
	Notice.ReplyToken = pConnection->ReplyToken;
	pClient->GlobalNotice = Notice;
	pClient->GlobalNoticePending = true;
	return XSSH_OK;
}



/* 为 DATA 或标准 stderr 准备动态接收，未知扩展仍保留给 Packet 回调。 */
static xsshcode xsshClientReceivePrepare(
	xsshclient* pClient,
	const xsshsessiontcppacket* pPacket
)
{
	const xsshconnectionpacket* pConnection =
		&pPacket->Session.Message.Connection;
	xsshchanneliostream Stream;
	xbytesview Data;
	uint32 iRecipient;
	xsshchannel* pChannel;

	if ( pPacket->Session.Kind != XSSH_SESSION_PACKET_CONNECTION ) {
		return XSSH_OK;
	}
	if ( pConnection->Kind == XSSH_CONNECTION_PACKET_CHANNEL_DATA ) {
		iRecipient = pConnection->Message.ChannelData.Recipient;
		Data = pConnection->Message.ChannelData.Data;
		Stream = XSSH_CHANNEL_IO_DATA;
	} else if ( (pConnection->Kind ==
		XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA) &&
		(pConnection->Message.ChannelExtendedData.Type ==
		 XSSH_CHANNEL_EXTENDED_DATA_STDERR) ) {
		iRecipient = pConnection->Message.ChannelExtendedData.Recipient;
		Data = pConnection->Message.ChannelExtendedData.Data;
		Stream = XSSH_CHANNEL_IO_STDERR;
	} else {
		return XSSH_OK;
	}
	pChannel = xrtSshChannelsGet(&pClient->Channels, iRecipient);
	if ( pChannel == NULL ) {
		return XSSH_ERROR_STATE;
	}
	pClient->ReceiveChannel = pChannel;
	pClient->ReceiveStream = Stream;
	return xrtSshChannelIoReceivePrepare(
		&pChannel->Io,
		Stream,
		iRecipient,
		Data
	);
}



/* 回滚当前 packet 建立的全部内部暂存，保持拒绝和异常路径一致。 */
static void xsshClientPacketStageAbort(xsshclient* pClient)
{
	if ( (pClient->ReceiveChannel != NULL) &&
		(pClient->ReceiveChannel->Io.Pending ==
		 XSSH_CHANNEL_IO_PENDING_RECEIVE) ) {
		(void)xrtSshChannelIoReceiveAbort(&pClient->ReceiveChannel->Io);
	}
	pClient->ReceiveChannel = NULL;
	pClient->ReceivePending = false;
	memset(&pClient->ChannelNotice, 0, sizeof(pClient->ChannelNotice));
	pClient->ChannelNoticePending = false;
	memset(&pClient->GlobalNotice, 0, sizeof(pClient->GlobalNotice));
	pClient->GlobalNoticePending = false;
	pClient->OpenCurrent = NULL;
	xsshClientOpenPendingDiscard(pClient);
}



/* 处理一个已认证 packet 的内部准备和用户提交决定。 */
static xsshsessionstreamdecision xsshClientPacketDecide(
	xsshclient* pClient,
	const xsshsessiontcppacket* pPacket
)
{
	xsshsessionstreamdecision Decision = XSSH_SESSION_STREAM_ACCEPT;
	bool bChannelOpen;
	xsshcode Code;

	bChannelOpen = (pPacket->Session.Kind ==
		XSSH_SESSION_PACKET_CONNECTION) &&
		(pPacket->Session.Message.Connection.Kind ==
		 XSSH_CONNECTION_PACKET_CHANNEL_OPEN);
	pClient->OpenCurrent = bChannelOpen ?
		&pPacket->Session.Message.Connection.Message.ChannelOpen : NULL;
	Code = xrtSshClientCoreObserve(&pClient->Core, &pClient->Stream.Session,
		pPacket);
	if ( Code == XSSH_OK ) {
		Code = xsshClientReceivePrepare(pClient, pPacket);
	}
	if ( Code == XSSH_OK ) {
		Code = xsshClientChannelNoticePrepare(pClient, pPacket);
	}
	if ( Code == XSSH_OK ) {
		Code = xsshClientGlobalNoticePrepare(pClient, pPacket);
	}
	if ( Code == XSSH_ERROR_SPACE ) {
		pClient->OpenCurrent = NULL;
		pClient->ReceiveRetry = true;
		xsshClientRetryNotify(
			pClient,
			Code,
			XERR_MEMORY,
			"SSH client needs memory before retrying channel input"
		);
		return XSSH_SESSION_STREAM_HOLD;
	}
	if ( Code != XSSH_OK ) {
		xsshClientPacketStageAbort(pClient);
		xsshClientErrorNotify(
			pClient,
			Code,
			XERR_PROTOCOL,
			"SSH client rejected peer packet"
		);
		return XSSH_SESSION_STREAM_ABORT;
	}
	pClient->ReceivePending = pClient->ReceiveChannel != NULL;
	if ( pClient->Events.Packet != NULL ) {
		Decision = pClient->Events.Packet(
			pClient,
			pPacket,
			pClient->UserData
		);
	}
	if ( (Decision == XSSH_SESSION_STREAM_ACCEPT) && bChannelOpen &&
		(pClient->OpenDecision == XSSH_CLIENT_CHANNEL_NONE) ) {
		Code = xsshClientOpenStage(
			pClient,
			&pPacket->Session.Message.Connection.Message.ChannelOpen,
			XSSH_CLIENT_CHANNEL_REJECT,
			XSSH_CHANNEL_OPEN_UNKNOWN_CHANNEL_TYPE,
			NULL
		);
		if ( Code == XSSH_ERROR_SPACE ) {
			pClient->OpenCurrent = NULL;
			pClient->ReceiveRetry = true;
			pClient->ReceiveRetryOpen = true;
			xsshClientRetryNotify(
				pClient,
				Code,
				XERR_MEMORY,
				"SSH client needs memory before rejecting peer channel"
			);
			return XSSH_SESSION_STREAM_HOLD;
		}
		if ( Code != XSSH_OK ) {
			pClient->OpenCurrent = NULL;
			xsshClientErrorNotify(
				pClient,
				Code,
				XERR_STATE,
				"SSH client could not stage peer channel decision"
			);
			return XSSH_SESSION_STREAM_ABORT;
		}
	}
	pClient->OpenCurrent = NULL;
	if ( (Decision != XSSH_SESSION_STREAM_ACCEPT) &&
		(Decision != XSSH_SESSION_STREAM_HOLD) ) {
		xsshClientPacketStageAbort(pClient);
		return XSSH_SESSION_STREAM_ABORT;
	}
	return Decision;
}



/* 推进握手核心并把唯一输出事务交给 SessionStream。 */
static xsshcode xsshClientAdvance(xsshclient* pClient)
{
	xsshsessiontcp* pSession = xrtSshSessionStreamSession(&pClient->Stream);
	xsshsessionreader* pReader = xrtSshSessionStreamReader(&pClient->Stream);
	xsshsessionpacketkind Kind;
	xsshclientnext Next;
	xsshcode Code;

	if ( (pSession == NULL) || (pReader == NULL) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshClientCoreNext(
		&pClient->Core,
		pSession,
		pReader,
		xrtTimer(),
		&Next
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Next.Kind == XSSH_CLIENT_NEXT_IDENTIFICATION ) {
		return xrtSshSessionTcpIdentificationWritePrepare(
			pSession,
			Next.Text
		);
	}
	if ( Next.Kind == XSSH_CLIENT_NEXT_PAYLOAD ) {
		return xrtSshSessionTcpWritePrepare(
			pSession,
			Next.Data,
			NULL,
			NULL,
			0u,
			xrtTimer(),
			&Kind
		);
	}
	if ( Next.Kind == XSSH_CLIENT_NEXT_HOST_KEY ) {
		if ( !pClient->HostNotified &&
			(pClient->Events.HostKey != NULL) ) {
			pClient->HostNotified = true;
			pClient->Events.HostKey(pClient, pClient->UserData);
		}
		return XSSH_OK;
	}
	if ( Next.Kind == XSSH_CLIENT_NEXT_AUTH ) {
		if ( !pClient->AuthNotified &&
			(pClient->Events.Authenticate != NULL) ) {
			pClient->AuthNotified = true;
			pClient->Events.Authenticate(pClient, pClient->UserData);
		}
		return XSSH_OK;
	}
	if ( Next.Kind == XSSH_CLIENT_NEXT_READY ) {
		xsshClientReadyTimerCancel(pClient);
		pClient->State = XSSH_CLIENT_READY;
		if ( !pClient->ReadyNotified ) {
			pClient->ReadyNotified = true;
			if ( pClient->Events.Ready != NULL ) {
				pClient->Events.Ready(pClient, pClient->UserData);
			}
			#if defined(XSSH_FEATURE_CLIENT_FUTURE)
			{
				xsshclientfuturenotice FutureNotice;

				memset(&FutureNotice, 0, sizeof(FutureNotice));
				FutureNotice.Signal = XSSH_CLIENT_FUTURE_READY;
				__xrtSshClientFutureNotify(pClient, &FutureNotice);
			}
			#endif
		}
	} else if ( Next.Kind == XSSH_CLIENT_NEXT_CLOSING ) {
		xsshClientReadyTimerCancel(pClient);
		pClient->State = XSSH_CLIENT_CLOSING;
	}
	return XSSH_OK;
}



/* 初始化 Worker 关联资源后发布连接打开。 */
static void xsshClientOpen(xsshsessionstream* pStream, ptr pData)
{
	xsshclient* pClient = (xsshclient*)pData;
	xnetstream* pTcp;
	xnetworker* pWorker;
	xnetbufpool* pPool;

	if ( !xsshClientValid(pClient) || (pStream != &pClient->Stream) ) {
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	pTcp = xrtSshSessionStreamTcp(pStream);
	pWorker = pTcp != NULL ? xrtNetStreamWorker(pTcp) : NULL;
	pPool = pWorker != NULL ? xrtNetWorkerBufPool(pWorker) : NULL;

	if ( pPool == NULL ) {
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_SPACE,
			XERR_MEMORY,
			"SSH client Worker resources could not initialize"
		);
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	if ( !xrtSshChannelsInit(
		&pClient->Channels,
		pPool,
		&pClient->Config.Channels
	) ) {
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_SPACE,
			XERR_MEMORY,
			"SSH client channels could not initialize"
		);
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	if ( !xrtSshChannelsOnRemoved(
		&pClient->Channels,
		xsshClientChannelRemoved,
		pClient
	) ) {
		xrtSshChannelsClear(&pClient->Channels);
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_STATE,
			XERR_STATE,
			"SSH client channel observer could not initialize"
		);
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	#endif
	if ( !xrtNetBufInit(&pClient->Control, pPool) ) {
		xrtSshChannelsClear(&pClient->Channels);
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_SPACE,
			XERR_MEMORY,
			"SSH client control buffer could not initialize"
		);
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	pClient->ResourcesReady = true;
	pClient->State = XSSH_CLIENT_HANDSHAKE;
	if ( !xsshClientReadyTimerStart(pClient) ) {
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_SPACE,
			XERR_MEMORY,
			"SSH client ready timer could not initialize"
		);
		pClient->State = XSSH_CLIENT_CLOSING;
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	if ( pClient->Events.Open != NULL ) {
		pClient->Events.Open(pClient, pClient->UserData);
	}
}



/* 在 Action 边界提交 staged DATA，再推进客户端握手动作。 */
static void xsshClientAction(
	xsshsessionstream* pStream,
	xsshsessionaction Action,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;
	xsshcode Code;

	if ( !xsshClientValid(pClient) || (pStream != &pClient->Stream) ) {
		return;
	}
	if ( Action == XSSH_SESSION_ACTION_CONNECTION ) {
		Code = xsshClientSendCommit(pClient);
		if ( Code == XSSH_OK ) {
			Code = xsshClientOpenSendCommit(pClient);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshClientOpenResponse(pClient);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshClientReceiveCommit(pClient);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshClientChannelNoticeCommit(pClient);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshClientGlobalNoticeCommit(pClient);
		}
		#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		if ( (Code == XSSH_OK) &&
			(pClient->FutureWritableChannel != NULL) ) {
			xsshclientfuturenotice FutureNotice;

			memset(&FutureNotice, 0, sizeof(FutureNotice));
			FutureNotice.Signal = XSSH_CLIENT_FUTURE_WRITABLE;
			FutureNotice.Channel = pClient->FutureWritableChannel;
			FutureNotice.ChannelLocal = pClient->FutureWritableLocal;
			FutureNotice.HasChannelLocal = true;
			pClient->FutureWritableChannel = NULL;
			pClient->FutureWritableLocal = 0u;
			__xrtSshClientFutureNotify(pClient, &FutureNotice);
		}
		#endif
		if ( Code != XSSH_OK ) {
			#if defined(XSSH_FEATURE_CLIENT_FUTURE)
				pClient->FutureWritableChannel = NULL;
				pClient->FutureWritableLocal = 0u;
			#endif
			xsshClientErrorNotify(
				pClient,
				Code,
				XERR_STATE,
				"SSH client channel transaction diverged"
			);
			(void)xrtSshSessionStreamAbort(pStream);
			return;
		}
	}
	Code = xsshClientAdvance(pClient);
	if ( Code != XSSH_OK ) {
		xsshClientErrorNotify(
			pClient,
			Code,
			Code == XSSH_ERROR_SPACE ? XERR_MEMORY : XERR_PROTOCOL,
			"SSH client action failed"
		);
		(void)xrtSshSessionStreamAbort(pStream);
	}
}



/* 客户端接受所有已经通过底层格式检查的 SSH-2.0 identification。 */
static xsshsessionstreamdecision xsshClientIdentification(
	xsshsessionstream* pStream,
	xstrview Version,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)Version;
	return xsshClientValid(pClient) && (pStream == &pClient->Stream) ?
		XSSH_SESSION_STREAM_ACCEPT : XSSH_SESSION_STREAM_ABORT;
}



/* 在底层提交前完成认证观察、channel DATA 预留和用户决策。 */
static xsshsessionstreamdecision xsshClientPacket(
	xsshsessionstream* pStream,
	const xsshsessiontcppacket* pPacket,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	if ( !xsshClientValid(pClient) || (pStream != &pClient->Stream) ) {
		return XSSH_SESSION_STREAM_ABORT;
	}
	return xsshClientPacketDecide(pClient, pPacket);
}



/* 转发非空 rekey 建议。 */
static void xsshClientRekey(
	xsshsessionstream* pStream,
	xsshrekeydecision Decision,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) && (pClient->Events.Rekey != NULL) ) {
		pClient->Events.Rekey(
			pClient,
			Decision,
			pClient->UserData
		);
	}
}



/* 转发底层 SessionStream 的结构化错误。 */
static void xsshClientError(
	xsshsessionstream* pStream,
	xsshcode Code,
	const xerror* pError,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) && (pClient->Events.Error != NULL) ) {
		pClient->Events.Error(
			pClient,
			Code,
			pError,
			pClient->UserData
		);
	}
}



/* 转发 peer TCP EOF，channel EOF 仍由协议 packet 表达。 */
static void xsshClientEnd(xsshsessionstream* pStream, ptr pData)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) && (pClient->Events.End != NULL) ) {
		pClient->Events.End(pClient, pClient->UserData);
	}
}



/* 转发 TCP 高水位通知。 */
static void xsshClientHighWater(
	xsshsessionstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) &&
		(pClient->Events.HighWater != NULL) ) {
		pClient->Events.HighWater(
			pClient,
			iQueued,
			pClient->UserData
		);
	}
}



/* 转发 TCP 低水位通知。 */
static void xsshClientLowWater(
	xsshsessionstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) &&
		(pClient->Events.LowWater != NULL) ) {
		pClient->Events.LowWater(
			pClient,
			iQueued,
			pClient->UserData
		);
	}
}



/* 转发 TCP 发送队列排空通知。 */
static void xsshClientDrain(xsshsessionstream* pStream, ptr pData)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) && (pClient->Events.Drain != NULL) ) {
		pClient->Events.Drain(pClient, pClient->UserData);
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	if ( xsshClientValid(pClient) ) {
		xsshclientfuturenotice FutureNotice;

		memset(&FutureNotice, 0, sizeof(FutureNotice));
		FutureNotice.Signal = XSSH_CLIENT_FUTURE_DRAIN;
		__xrtSshClientFutureNotify(pClient, &FutureNotice);
	}
	#endif
}



/* 发布唯一关闭终态，动态资源保留到显式 Clear。 */
static void xsshClientClose(
	xsshsessionstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;
	const xerror* pTerminal;

	if ( !xsshClientValid(pClient) || (pStream != &pClient->Stream) ) {
		return;
	}
	xsshClientReadyTimerCancel(pClient);
	pTerminal = pError != NULL ? pError : pClient->TerminalError;
	pClient->State = XSSH_CLIENT_CLOSED;
	if ( pClient->Events.Close != NULL ) {
		pClient->Events.Close(
			pClient,
			Result,
			pTerminal,
			pClient->UserData
		);
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	{
		xsshclientfuturenotice FutureNotice;

		memset(&FutureNotice, 0, sizeof(FutureNotice));
		FutureNotice.Signal = XSSH_CLIENT_FUTURE_CLOSE;
		FutureNotice.Error = pTerminal;
		__xrtSshClientFutureNotify(pClient, &FutureNotice);
	}
	#endif
}



/* 写入安全、无隐藏运行时的客户端默认配置。 */
bool xrtSshClientConfigInit(xsshclientconfig* pConfig)
{
	xsshclientconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		return false;
	}
	memset(&Config, 0, sizeof(Config));
	if ( !xrtSshClientCoreConfigInit(&Config.Core) ) {
		return false;
	}
	xrtSshChannelsConfigInit(&Config.Channels);
	Config.ReadyTimeout = XSSH_CLIENT_READY_TIMEOUT_DEFAULT;
	Config.ControlInitial = XSSH_CLIENT_CONTROL_INITIAL_DEFAULT;
	Config.ControlLimit = XSSH_CLIENT_CONTROL_LIMIT_DEFAULT;
	Config.GlobalReplyLimit = XSSH_CLIENT_GLOBAL_REPLY_LIMIT_DEFAULT;
	*pConfig = Config;
	return true;
}



/* 初始化客户端组合对象，不创建网络或动态 channel 块。 */
bool xrtSshClientInit(
	xsshclient* pClient,
	const xsshclientconfig* pConfig,
	const xsshclientevents* pEvents,
	ptr pData
)
{
	xsshsessiontcpconfig SessionConfig;
	xsshclientevents Events;

	if ( !xrtMemRangeValid(pClient, sizeof(*pClient)) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		xrtMemRangesOverlap(
			pClient,
			sizeof(*pClient),
			pConfig,
			sizeof(*pConfig)
		) || (pConfig->Core.Kex.Role != XSSH_ROLE_CLIENT) ||
		(pConfig->ControlInitial == 0u) ||
		(pConfig->ControlInitial > pConfig->ControlLimit) ||
		(pConfig->GlobalReplyLimit == 0u) ) {
		return false;
	}
	memset(&Events, 0, sizeof(Events));
	if ( pEvents != NULL ) {
		if ( !xrtMemRangeValid(pEvents, sizeof(*pEvents)) ||
			xrtMemRangesOverlap(
				pClient,
				sizeof(*pClient),
				pEvents,
				sizeof(*pEvents)
			) ) {
			return false;
		}
		Events = *pEvents;
	}
	memset(pClient, 0, sizeof(*pClient));
	pClient->Config = *pConfig;
	pClient->Events = Events;
	pClient->UserData = pData;
	pClient->ControlTarget = pConfig->ControlInitial;
	pClient->State = XSSH_CLIENT_CREATED;
	pClient->Guard = XSSH_CLIENT_GUARD;
	if ( !xrtSshReplyQueueInit(&pClient->GlobalReplies, NULL, 0u) ||
		!xrtSshClientCoreInit(&pClient->Core, &pConfig->Core) ||
		!xrtSshSessionTcpConfigInit(
			&SessionConfig,
			XSSH_ROLE_CLIENT
		) ) {
		xrtSshClientCoreClear(&pClient->Core);
		memset(pClient, 0, sizeof(*pClient));
		return false;
	}
	SessionConfig.ChannelResolve = xrtSshChannelsResolve;
	SessionConfig.ChannelUserData = &pClient->Channels;
	SessionConfig.GlobalReplies = &pClient->GlobalReplies;
	if ( !xrtSshSessionStreamInit(
		&pClient->Stream,
		&SessionConfig,
		&xsshClientEvents,
		pClient
	) ) {
		xrtSshClientCoreClear(&pClient->Core);
		memset(pClient, 0, sizeof(*pClient));
		return false;
	}
	return true;
}



/* 清理关闭客户端的动态数据和协议状态。 */
bool xrtSshClientClear(xsshclient* pClient)
{
	if ( !xsshClientValid(pClient) ||
		((pClient->State != XSSH_CLIENT_CREATED) &&
		 (pClient->State != XSSH_CLIENT_CLOSED)) ) {
		return false;
	}
	if ( !xrtSshSessionStreamClear(&pClient->Stream) ) {
		return false;
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		__xrtSshClientFutureClear(pClient);
	#endif
	if ( pClient->ResourcesReady ) {
		xrtNetBufClear(&pClient->Control);
		xrtSshChannelsClear(&pClient->Channels);
	}
	xrtFree(pClient->GlobalReplyTokens);
	xrtErrorFree(pClient->TerminalError);
	xrtSshClientCoreClear(&pClient->Core);
	memset(pClient, 0, sizeof(*pClient));
	return true;
}



/* 复用 SessionStream 唯一网络适配器。 */
const xnetstreamevents* xrtSshClientNetEvents(void)
{
	return xrtSshSessionStreamNetEvents();
}



/* 返回 NetEvents 要求的稳定 SessionStream 地址。 */
ptr xrtSshClientNetData(xsshclient* pClient)
{
	return xsshClientValid(pClient) ? (ptr)&pClient->Stream : NULL;
}



/* 把已连接 Stream 附着到组合客户端。 */
bool xrtSshClientAttach(xsshclient* pClient, xnetstream* pStream)
{
	return xsshClientValid(pClient) &&
		(pClient->State == XSSH_CLIENT_CREATED) &&
		xrtSshSessionStreamAttach(&pClient->Stream, pStream);
}



/* 返回公开客户端状态。 */
xsshclientstate xrtSshClientState(const xsshclient* pClient)
{
	return xsshClientValid(pClient) ? pClient->State : XSSH_CLIENT_INVALID;
}



/* 返回客户端是否位于其唯一 Stream 所属 Worker。 */
bool xrtSshClientIsCurrent(const xsshclient* pClient)
{
	return xsshClientCurrent(pClient);
}



/* 返回完整 Stream 驱动。 */
xsshsessionstream* xrtSshClientStream(xsshclient* pClient)
{
	return xsshClientValid(pClient) ? &pClient->Stream : NULL;
}



/* 返回底层 TCP 会话。 */
xsshsessiontcp* xrtSshClientSession(xsshclient* pClient)
{
	return xsshClientValid(pClient) ?
		xrtSshSessionStreamSession(&pClient->Stream) : NULL;
}



/* 返回动态 packet Reader。 */
xsshsessionreader* xrtSshClientReader(xsshclient* pClient)
{
	return xsshClientValid(pClient) ?
		xrtSshSessionStreamReader(&pClient->Stream) : NULL;
}



/* 返回动态 channel 所有者。 */
xsshchannels* xrtSshClientChannels(xsshclient* pClient)
{
	return xsshClientValid(pClient) && pClient->ResourcesReady ?
		&pClient->Channels : NULL;
}



/* 查询 channel 地址与本地编号是否仍映射到当前客户端。 */
bool xrtSshClientOwnsChannel(
	const xsshclient* pClient,
	const xsshchannel* pChannel
)
{
	return xsshClientChannelOwned(pClient, pChannel);
}



/* 暂存接受当前 peer channel open 的决定。 */
xsshcode xrtSshClientChannelAccept(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	xsshchannel** ppChannel
)
{
	if ( !xrtMemRangeValid(ppChannel, sizeof(*ppChannel)) ||
		xrtMemRangesOverlap(
			pClient,
			sizeof(*pClient),
			ppChannel,
			sizeof(*ppChannel)
		) || xrtMemRangesOverlap(
			pOpen,
			sizeof(*pOpen),
			ppChannel,
			sizeof(*ppChannel)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*ppChannel = NULL;
	return xsshClientOpenStage(
		pClient,
		pOpen,
		XSSH_CLIENT_CHANNEL_ACCEPT,
		0u,
		ppChannel
	);
}



/* 暂存拒绝当前 peer channel open 的决定。 */
xsshcode xrtSshClientChannelReject(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	uint32 iReason
)
{
	return xsshClientOpenStage(
		pClient,
		pOpen,
		XSSH_CLIENT_CHANNEL_REJECT,
		iReason,
		NULL
	);
}



/* 返回连接级 request reply FIFO。 */
xsshreplyqueue* xrtSshClientGlobalReplies(xsshclient* pClient)
{
	return xsshClientValid(pClient) ? &pClient->GlobalReplies : NULL;
}



/* 按需扩展连接级 token ring，失败时保留原队列。 */
xsshcode xrtSshClientGlobalReplyReserve(
	xsshclient* pClient,
	size_t iCapacity
)
{
	uint64* pTokens;
	size_t iNewCapacity;
	xsshcode Code;

	if ( !xsshClientValid(pClient) ||
		((pClient->State != XSSH_CLIENT_CREATED) &&
		 !xsshClientCurrent(pClient)) ) {
		return XSSH_ERROR_STATE;
	}
	if ( iCapacity > pClient->Config.GlobalReplyLimit ) {
		xrtSetErrorKind(XERR_RANGE);
		return XSSH_ERROR_SPACE;
	}
	if ( iCapacity <= pClient->GlobalReplyCapacity ) {
		return XSSH_OK;
	}
	iNewCapacity = pClient->GlobalReplyCapacity != 0u ?
		pClient->GlobalReplyCapacity : 4u;
	while ( iNewCapacity < iCapacity ) {
		if ( iNewCapacity > (SIZE_MAX / 2u) ) {
			iNewCapacity = iCapacity;
			break;
		}
		iNewCapacity *= 2u;
	}
	if ( iNewCapacity > pClient->Config.GlobalReplyLimit ) {
		iNewCapacity = pClient->Config.GlobalReplyLimit;
	}
	if ( iNewCapacity > (SIZE_MAX / sizeof(uint64)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	pTokens = (uint64*)xrtMalloc(iNewCapacity * sizeof(uint64));
	if ( pTokens == NULL ) {
		return XSSH_ERROR_SPACE;
	}
	Code = xrtSshReplyQueueRebind(
		&pClient->GlobalReplies,
		pTokens,
		iNewCapacity
	);
	if ( Code != XSSH_OK ) {
		xrtFree(pTokens);
		return Code;
	}
	xrtFree(pClient->GlobalReplyTokens);
	pClient->GlobalReplyTokens = pTokens;
	pClient->GlobalReplyCapacity = iNewCapacity;
	return XSSH_OK;
}



/* 在 Worker 中重试外部凭据动作并继续驱动线路。 */
xsshcode xrtSshClientContinue(xsshclient* pClient)
{
	xsshcode Code;

	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_HANDSHAKE) ) {
		return XSSH_ERROR_STATE;
	}
	pClient->AuthNotified = false;
	Code = xsshClientAdvance(pClient);
	return Code == XSSH_OK ? xsshClientDrive(pClient) : Code;
}



/* 接受延迟主机密钥并继续 KEX。 */
xsshcode xrtSshClientHostKeyAccept(xsshclient* pClient)
{
	xsshcode Code;

	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_HANDSHAKE) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshClientCoreHostKeyAccept(
		&pClient->Core,
		&pClient->Stream.Session
	);
	pClient->HostNotified = false;
	return Code == XSSH_OK ? xsshClientDrive(pClient) : Code;
}



/* 拒绝延迟主机密钥并异常终止连接。 */
xsshcode xrtSshClientHostKeyReject(xsshclient* pClient)
{
	xsshcode Code;

	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_HANDSHAKE) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshClientCoreHostKeyReject(
		&pClient->Core,
		&pClient->Stream.Session
	);
	if ( Code == XSSH_OK ) {
		pClient->State = XSSH_CLIENT_CLOSING;
		(void)xrtSshSessionStreamAbort(&pClient->Stream);
	}
	return Code;
}



/* 提交用户保留且已经完成内部准备的 packet。 */
xsshcode xrtSshClientPacketAccept(xsshclient* pClient)
{
	const xsshchannelopen* pOpen;
	xsshcode Code;

	if ( !xsshClientCurrent(pClient) || pClient->ReceiveRetry ) {
		return XSSH_ERROR_STATE;
	}
	pOpen = xsshClientCurrentOpen(pClient);
	if ( (pOpen != NULL) &&
		(pClient->OpenDecision == XSSH_CLIENT_CHANNEL_NONE) ) {
		Code = xsshClientOpenStage(
			pClient,
			pOpen,
			XSSH_CLIENT_CHANNEL_REJECT,
			XSSH_CHANNEL_OPEN_UNKNOWN_CHANNEL_TYPE,
			NULL
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Code = xrtSshSessionStreamAccept(&pClient->Stream);
	return Code == XSSH_NEED_MORE ? XSSH_OK : Code;
}



/* 回滚 staged DATA 并拒绝当前 packet。 */
xsshcode xrtSshClientPacketReject(xsshclient* pClient)
{
	xsshcode Code = XSSH_OK;

	if ( !xsshClientCurrent(pClient) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pClient->ReceiveChannel != NULL) &&
		(pClient->ReceiveChannel->Io.Pending ==
		 XSSH_CHANNEL_IO_PENDING_RECEIVE) ) {
		Code = xrtSshChannelIoReceiveAbort(&pClient->ReceiveChannel->Io);
	}
	xsshClientPacketStageAbort(pClient);
	pClient->ReceiveRetry = false;
	pClient->ReceiveRetryOpen = false;
	if ( xrtSshSessionStreamReject(&pClient->Stream) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	return Code;
}



/* 在 OOM 恢复后重跑内部 DATA 准备和用户 Packet 决策。 */
xsshcode xrtSshClientPacketRetry(xsshclient* pClient)
{
	const xsshsessiontcppacket* pPacket;
	xsshsessionstreamdecision Decision;
	xsshcode Code;

	if ( !xsshClientCurrent(pClient) || !pClient->ReceiveRetry ) {
		return XSSH_ERROR_STATE;
	}
	pPacket = xrtSshSessionStreamPacket(&pClient->Stream);
	if ( pPacket == NULL ) {
		return XSSH_ERROR_STATE;
	}
	if ( pClient->ReceiveRetryOpen ) {
		const xsshchannelopen* pOpen =
			&pPacket->Session.Message.Connection.Message.ChannelOpen;

		Code = xsshClientOpenStage(
			pClient,
			pOpen,
			XSSH_CLIENT_CHANNEL_REJECT,
			XSSH_CHANNEL_OPEN_UNKNOWN_CHANNEL_TYPE,
			NULL
		);

		if ( Code == XSSH_ERROR_SPACE ) {
			return Code;
		}
		pClient->ReceiveRetry = false;
		pClient->ReceiveRetryOpen = false;
		if ( Code != XSSH_OK ) {
			(void)xrtSshSessionStreamReject(&pClient->Stream);
			return Code;
		}
		Code = xrtSshSessionStreamAccept(&pClient->Stream);
		return Code == XSSH_NEED_MORE ? XSSH_OK : Code;
	}
	pClient->ReceiveRetry = false;
	pClient->ReceiveRetryOpen = false;
	pClient->ReceiveChannel = NULL;
	Decision = xsshClientPacketDecide(pClient, pPacket);
	if ( Decision == XSSH_SESSION_STREAM_ACCEPT ) {
		Code = xrtSshSessionStreamAccept(&pClient->Stream);
		return Code == XSSH_NEED_MORE ? XSSH_OK : Code;
	}
	if ( Decision == XSSH_SESSION_STREAM_HOLD ) {
		return XSSH_OK;
	}
	(void)xrtSshSessionStreamReject(&pClient->Stream);
	return XSSH_ERROR_STATE;
}



/* 直接编码并提交一个已有 payload。 */
xsshcode xrtSshClientSend(
	xsshclient* pClient,
	xbytesview Payload,
	xsshchannel* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
)
{
	xsshsessionpacketkind Kind;
	xsshcode Code;
	bool bChannelIo;

	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_READY) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pChannel != NULL) &&
		(!xsshClientChannelOwned(pClient, pChannel) ||
		 ((pReplies != NULL) &&
		  (pReplies != &pChannel->Replies))) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pChannel == NULL) && (pReplies != NULL) &&
		(pReplies != &pClient->GlobalReplies) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	bChannelIo = (pChannel != NULL) &&
		(pChannel->Io.Pending == XSSH_CHANNEL_IO_PENDING_SEND);
	if ( bChannelIo ) {
		if ( pClient->SendPending ) {
			return XSSH_ERROR_STATE;
		}
		pClient->SendChannel = pChannel;
		pClient->SendPending = true;
	}
	Code = xrtSshSessionTcpWritePrepare(
		&pClient->Stream.Session,
		Payload,
		pChannel != NULL ? &pChannel->Core : NULL,
		pReplies,
		iReplyToken,
		xrtTimer(),
		&Kind
	);
	if ( Code != XSSH_OK ) {
		if ( bChannelIo ) {
			(void)xrtSshChannelIoSendAbort(&pChannel->Io);
			pClient->SendChannel = NULL;
			pClient->SendPending = false;
		}
		return Code;
	}
	Code = xsshClientDrive(pClient);
	if ( Code != XSSH_OK ) {
		(void)xrtSshSessionTcpWriteAbort(&pClient->Stream.Session);
		if ( pClient->SendPending ) {
			(void)xrtSshChannelIoSendAbort(&pChannel->Io);
			pClient->SendChannel = NULL;
			pClient->SendPending = false;
		}
	}
	return Code;
}



/* 在 Worker 缓冲池中按需扩展连续 scratch 并发送构建结果。 */
xsshcode xrtSshClientBuild(
	xsshclient* pClient,
	xsshclientbuildproc pBuild,
	ptr pBuildData,
	xsshchannel* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
)
{
	xnetwspan Span;
	xsshwriter Writer;
	xsshcode Code;
	size_t iCapacity;

	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_READY) ||
		(pBuild == NULL) ) {
		return XSSH_ERROR_STATE;
	}
	for ( ;; ) {
		if ( !xrtNetBufReserve(
			&pClient->Control,
			pClient->ControlTarget,
			&Span
		) ) {
			return XSSH_ERROR_SPACE;
		}
		iCapacity = Span.Size < pClient->Config.ControlLimit ?
			Span.Size : pClient->Config.ControlLimit;
		xrtSecureZero(Span.Data, Span.Size);
		if ( !xrtSshWriterInit(&Writer, Span.Data, iCapacity) ) {
			(void)xrtNetBufCancel(&pClient->Control);
			return XSSH_ERROR_STATE;
		}
		Code = pBuild(&Writer, pBuildData);
		if ( Code != XSSH_ERROR_SPACE ) {
			break;
		}
		xrtSecureZero(Span.Data, Span.Size);
		if ( !xrtNetBufCancel(&pClient->Control) ||
			(iCapacity >= pClient->Config.ControlLimit) ) {
			return XSSH_ERROR_SPACE;
		}
		if ( pClient->ControlTarget <=
			(pClient->Config.ControlLimit / 2u) ) {
			pClient->ControlTarget *= 2u;
		} else {
			pClient->ControlTarget = pClient->Config.ControlLimit;
		}
	}
	if ( Code == XSSH_OK ) {
		Code = xrtSshClientSend(
			pClient,
			(xbytesview){ Span.Data, Writer.Size },
			pChannel,
			pReplies,
			iReplyToken
		);
	}
	xrtSecureZero(Span.Data, Span.Size);
	if ( !xrtNetBufCancel(&pClient->Control) ) {
		return XSSH_ERROR_STATE;
	}
	return Code;
}



/* 创建 channel 并用调用方类型构建器发送唯一 open。 */
xsshcode xrtSshClientChannelOpen(
	xsshclient* pClient,
	xsshclientchannelopenproc pOpen,
	ptr pOpenData,
	xsshchannel** ppChannel
)
{
	xsshclientopenbuild Build;
	xsshchannel* pChannel;
	xsshchannels* pChannels;
	xsshcode Code;
	uint32 iLocal;

	if ( (pOpen == NULL) ||
		!xrtMemRangeValid(ppChannel, sizeof(*ppChannel)) ||
		xrtMemRangesOverlap(
			pClient,
			sizeof(*pClient),
			ppChannel,
			sizeof(*ppChannel)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*ppChannel = NULL;
	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_READY) ) {
		return XSSH_ERROR_STATE;
	}
	pChannels = &pClient->Channels;
	Code = xrtSshChannelsOpen(pChannels, &pChannel);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iLocal = pChannel->Core.Local;
	Build.Open = pOpen;
	Build.Channel = pChannel;
	Build.UserData = pOpenData;
	Code = xrtSshClientBuild(
		pClient,
		xsshClientChannelOpenBuild,
		&Build,
		pChannel,
		NULL,
		0u
	);
	if ( Code != XSSH_OK ) {
		if ( !xrtSshChannelsDiscard(pChannels, iLocal) ) {
			return XSSH_ERROR_STATE;
		}
		return Code;
	}
	*ppChannel = pChannel;
	return XSSH_OK;
}



/* 把一条 channel 数据流的连续队首提交给客户端。 */
xsshcode xrtSshClientChannelFlush(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream
)
{
	xsshclientflushbuild Build;
	xsshcode Code;

	if ( !xsshClientChannelOwned(pClient, pChannel) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Build.Channel = pChannel;
	Build.Stream = Stream;
	Code = xrtSshClientBuild(
		pClient,
		xsshClientChannelFlushBuild,
		&Build,
		pChannel,
		NULL,
		0u
	);
	if ( (Code != XSSH_OK) &&
		(pChannel->Io.Pending == XSSH_CHANNEL_IO_PENDING_SEND) ) {
		(void)xrtSshChannelIoSendAbort(&pChannel->Io);
	}
	return Code;
}



/* 返还当前 channel 已消费的接收额度。 */
xsshcode xrtSshClientChannelAdjust(
	xsshclient* pClient,
	xsshchannel* pChannel
)
{
	if ( !xsshClientChannelOwned(pClient, pChannel) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtSshChannelCoreAdjustReady(&pChannel->Core) ) {
		return XSSH_NEED_MORE;
	}
	return xrtSshClientBuild(
		pClient,
		xsshClientChannelAdjustBuild,
		pChannel,
		pChannel,
		NULL,
		0u
	);
}



/* 半关闭 channel 的本端写方向。 */
xsshcode xrtSshClientChannelEof(
	xsshclient* pClient,
	xsshchannel* pChannel
)
{
	return xsshClientChannelOwned(pClient, pChannel) ?
		xrtSshClientBuild(
			pClient,
			xsshClientChannelEofBuild,
			pChannel,
			pChannel,
			NULL,
			0u
		) : XSSH_ERROR_ARGUMENT;
}



/* 发起或响应 channel 双向关闭。 */
xsshcode xrtSshClientChannelClose(
	xsshclient* pClient,
	xsshchannel* pChannel
)
{
	return xsshClientChannelOwned(pClient, pChannel) ?
		xrtSshClientBuild(
			pClient,
			xsshClientChannelCloseBuild,
			pChannel,
			pChannel,
			NULL,
			0u
		) : XSSH_ERROR_ARGUMENT;
}



/* 委托 SessionStream 回滚并异常关闭。 */
bool xrtSshClientAbort(xsshclient* pClient)
{
	if ( !xsshClientCurrent(pClient) ||
		(pClient->State >= XSSH_CLIENT_CLOSING) ) {
		return false;
	}
	pClient->State = XSSH_CLIENT_CLOSING;
	return xrtSshSessionStreamAbort(&pClient->Stream);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_client_session.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CLIENT_SESSION)



#if defined(XSSH_FEATURE_CLIENT_SESSION)

/* 单一构建上下文覆盖 session request 的全部经典类型。 */
typedef enum xsshclientsessionbuildkind {
	XSSH_CLIENT_SESSION_BUILD_REQUEST = 0,
	XSSH_CLIENT_SESSION_BUILD_ENV = 1,
	XSSH_CLIENT_SESSION_BUILD_SHELL = 2,
	XSSH_CLIENT_SESSION_BUILD_EXEC = 3,
	XSSH_CLIENT_SESSION_BUILD_SUBSYSTEM = 4,
	XSSH_CLIENT_SESSION_BUILD_SIGNAL = 5,
	XSSH_CLIENT_SESSION_BUILD_BREAK = 6
} xsshclientsessionbuildkind;



/* 构建期间只借用输入视图，发送返回后不保留调用方文本。 */
typedef struct xsshclientsessionbuild {
	xsshchannel* Channel;
	xstrview Type;
	xbytesview First;
	xbytesview Second;
	uint32 Value;
	bool WantReply;
	xsshclientsessionbuildkind Kind;
} xsshclientsessionbuild;



/* 判断调用发生在 READY 客户端 Worker 且 channel 归属正确。 */
static bool xsshClientSessionReady(
	xsshclient* pClient,
	xsshchannel* pChannel
)
{
	return (xrtSshClientState(pClient) == XSSH_CLIENT_READY) &&
		xrtSshClientIsCurrent(pClient) &&
		xrtSshClientOwnsChannel(pClient, pChannel);
}



/* 为 want-reply 请求预留一个 FIFO 位置，失败不改变队列内容。 */
static xsshcode xsshClientSessionReplyReserve(
	xsshchannel* pChannel,
	bool bWantReply
)
{
	size_t iCount;

	if ( !bWantReply ) {
		return XSSH_OK;
	}
	iCount = xrtSshReplyQueueCount(&pChannel->Replies);
	if ( iCount == SIZE_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	return xrtSshChannelReplyReserve(pChannel, iCount + 1u);
}



/* 构建固定 session channel open。 */
static xsshcode xsshClientSessionOpenBuild(
	xsshwriter* pWriter,
	const xsshchannelcore* pChannel,
	ptr pData
)
{
	(void)pData;
	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		(xrtSshChannelCorePhase(pChannel) !=
		 XSSH_CHANNEL_CORE_OPENING) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshChannelOpenWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_CHANNEL_TYPE_SESSION),
		pChannel->Local,
		pChannel->Window.ReceiveWindow,
		pChannel->Window.ReceiveMaxPacket,
		(xbytesview){ NULL, 0u }
	);
}



/* 根据类型写出一个 session request，并统一使用远端 channel id。 */
static xsshcode xsshClientSessionRequestBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshclientsessionbuild* pBuild = (xsshclientsessionbuild*)pData;
	uint32 iLocal;
	uint32 iRemote;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		!xrtMemRangeValid(pBuild->Channel, sizeof(*pBuild->Channel)) ||
		!xrtSshChannelCoreIds(
			&pBuild->Channel->Core,
			&iLocal,
			&iRemote
		) ) {
		return XSSH_ERROR_STATE;
	}
	(void)iLocal;
	switch ( pBuild->Kind ) {
		case XSSH_CLIENT_SESSION_BUILD_REQUEST:
			return xrtSshChannelRequestWrite(
				pWriter,
				iRemote,
				pBuild->Type,
				pBuild->WantReply,
				pBuild->First
			);
		case XSSH_CLIENT_SESSION_BUILD_ENV:
			return xrtSshChannelEnvWrite(
				pWriter,
				iRemote,
				pBuild->WantReply,
				pBuild->First,
				pBuild->Second
			);
		case XSSH_CLIENT_SESSION_BUILD_SHELL:
			return xrtSshChannelShellWrite(
				pWriter,
				iRemote,
				pBuild->WantReply
			);
		case XSSH_CLIENT_SESSION_BUILD_EXEC:
			return xrtSshChannelExecWrite(
				pWriter,
				iRemote,
				pBuild->WantReply,
				pBuild->First
			);
		case XSSH_CLIENT_SESSION_BUILD_SUBSYSTEM:
			return xrtSshChannelSubsystemWrite(
				pWriter,
				iRemote,
				pBuild->WantReply,
				pBuild->First
			);
		case XSSH_CLIENT_SESSION_BUILD_SIGNAL:
			return xrtSshChannelSignalWrite(
				pWriter,
				iRemote,
				pBuild->Type
			);
		case XSSH_CLIENT_SESSION_BUILD_BREAK:
			return xrtSshChannelBreakWrite(
				pWriter,
				iRemote,
				pBuild->WantReply,
				pBuild->Value
			);
		default:
			return XSSH_ERROR_ARGUMENT;
	}
}



/* 校验并发送一个已经填充的 session request。 */
static xsshcode xsshClientSessionSend(
	xsshclient* pClient,
	xsshclientsessionbuild* pBuild,
	uint64 iReplyToken
)
{
	xsshreplyqueue* pReplies;
	xsshcode Code;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		!xsshClientSessionReady(pClient, pBuild->Channel) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshClientSessionReplyReserve(
		pBuild->Channel,
		pBuild->WantReply
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pReplies = pBuild->WantReply ? &pBuild->Channel->Replies : NULL;
	return xrtSshClientBuild(
		pClient,
		xsshClientSessionRequestBuild,
		pBuild,
		pBuild->Channel,
		pReplies,
		iReplyToken
	);
}



/* 打开不带类型专用字段的 session channel。 */
xsshcode xrtSshClientSessionOpen(
	xsshclient* pClient,
	xsshchannel** ppChannel
)
{
	return xrtSshClientChannelOpen(
		pClient,
		xsshClientSessionOpenBuild,
		NULL,
		ppChannel
	);
}



/* 发送调用方提供类型与已编码字段的扩展请求。 */
xsshcode xrtSshClientSessionRequest(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xstrview Type,
	xbytesview Fields,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		Type,
		Fields,
		{ NULL, 0u },
		0u,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_REQUEST
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}



/* 发送 env request。 */
xsshcode xrtSshClientSessionEnv(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Name,
	xbytesview Value,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		{ NULL, 0u },
		Name,
		Value,
		0u,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_ENV
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}



/* 发送 shell request。 */
xsshcode xrtSshClientSessionShell(
	xsshclient* pClient,
	xsshchannel* pChannel,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		{ NULL, 0u },
		{ NULL, 0u },
		{ NULL, 0u },
		0u,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_SHELL
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}



/* 发送 exec request。 */
xsshcode xrtSshClientSessionExec(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Command,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		{ NULL, 0u },
		Command,
		{ NULL, 0u },
		0u,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_EXEC
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}



/* 发送 subsystem request。 */
xsshcode xrtSshClientSessionSubsystem(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Subsystem,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		{ NULL, 0u },
		Subsystem,
		{ NULL, 0u },
		0u,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_SUBSYSTEM
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}



/* 发送不要求回复的 signal request。 */
xsshcode xrtSshClientSessionSignal(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xstrview Signal
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		Signal,
		{ NULL, 0u },
		{ NULL, 0u },
		0u,
		false,
		XSSH_CLIENT_SESSION_BUILD_SIGNAL
	};

	return xsshClientSessionSend(pClient, &Build, 0u);
}



/* 发送 break request。 */
xsshcode xrtSshClientSessionBreak(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint32 iLengthMs,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		{ NULL, 0u },
		{ NULL, 0u },
		{ NULL, 0u },
		iLengthMs,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_BREAK
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_client_dial.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CLIENT_DIAL)



#if defined(XSSH_FEATURE_CLIENT_DIAL)

/* 设置当前执行上下文的客户端 Dial 参数或状态错误。 */
static xnetdial* xsshClientDialError(xerrkind Kind, xsshcode Code)
{
	xrtSetErrorInfo(
		Kind,
		"xrt.ssh.client",
		(int32)Code,
		Kind == XERR_ARGUMENT ?
			"invalid SSH client dial arguments" :
			"SSH client dial requires a created client"
	);
	return NULL;
}



/* 直接复用 XRT Dial 的解析、地址竞速、截止时间、取消和所有权契约。 */
xnetdial* xrtSshClientDial(
	xsshclient* pClient,
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	xnetdialproc pDone,
	ptr pData
)
{
	xsshclientstate State = xrtSshClientState(pClient);
	ptr pStreamData;

	if ( State == XSSH_CLIENT_INVALID ) {
		return xsshClientDialError(
			XERR_ARGUMENT,
			XSSH_ERROR_ARGUMENT
		);
	}
	if ( State != XSSH_CLIENT_CREATED ) {
		return xsshClientDialError(XERR_STATE, XSSH_ERROR_STATE);
	}
	pStreamData = xrtSshClientNetData(pClient);
	if ( pStreamData == NULL ) {
		return xsshClientDialError(XERR_STATE, XSSH_ERROR_STATE);
	}
	return xrtNetDial(
		pEngine,
		pResolver,
		sHost,
		iPort,
		pConfig,
		xrtSshClientNetEvents(),
		pStreamData,
		pDone,
		pData
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_client_forward.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CLIENT_FORWARD)



#if defined(XSSH_FEATURE_CLIENT_FORWARD)

/* Direct channel 构建上下文只在同步 open 构建期间借用地址。 */
typedef struct xsshclientdirectbuild {
	xbytesview Host;
	xbytesview Originator;
	uint32 Port;
	uint32 OriginatorPort;
} xsshclientdirectbuild;



/* 全局 forwarding 构建上下文区分申请和取消。 */
typedef struct xsshclientforwardbuild {
	xbytesview Address;
	uint32 Port;
	bool Cancel;
} xsshclientforwardbuild;



/* 高级 TCP helper 只接受线路端口范围；forward 请求可用零值动态监听。 */
static bool xsshClientForwardPort(uint32 iPort)
{
	if ( iPort > UINT16_MAX ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	return true;
}



/* 写出 direct-tcpip open，并沿用动态 channel 的窗口配置。 */
static xsshcode xsshClientDirectBuild(
	xsshwriter* pWriter,
	const xsshchannelcore* pChannel,
	ptr pData
)
{
	xsshclientdirectbuild* pBuild = (xsshclientdirectbuild*)pData;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		!xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		(xrtSshChannelCorePhase(pChannel) !=
		 XSSH_CHANNEL_CORE_OPENING) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshDirectTcpipOpenWrite(
		pWriter,
		pChannel->Local,
		pChannel->Window.ReceiveWindow,
		pChannel->Window.ReceiveMaxPacket,
		pBuild->Host,
		pBuild->Port,
		pBuild->Originator,
		pBuild->OriginatorPort
	);
}



/* 写出要求回复的 forwarding 全局请求。 */
static xsshcode xsshClientForwardBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshclientforwardbuild* pBuild = (xsshclientforwardbuild*)pData;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return pBuild->Cancel ? xrtSshTcpipForwardCancelWrite(
		pWriter,
		pBuild->Address,
		pBuild->Port
	) : xrtSshTcpipForwardWrite(
		pWriter,
		pBuild->Address,
		pBuild->Port
	);
}



/* 为全局请求预留 reply token 后提交唯一事务。 */
static xsshcode xsshClientForwardSend(
	xsshclient* pClient,
	xsshclientforwardbuild* pBuild,
	uint64 iReplyToken
)
{
	xsshreplyqueue* pReplies;
	size_t iCount;
	xsshcode Code;

	if ( (xrtSshClientState(pClient) != XSSH_CLIENT_READY) ||
		!xrtSshClientIsCurrent(pClient) ||
		!xrtMemRangeValid(pBuild, sizeof(*pBuild)) ) {
		return XSSH_ERROR_STATE;
	}
	pReplies = xrtSshClientGlobalReplies(pClient);
	iCount = xrtSshReplyQueueCount(pReplies);
	if ( iCount == SIZE_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshClientGlobalReplyReserve(pClient, iCount + 1u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtSshClientBuild(
		pClient,
		xsshClientForwardBuild,
		pBuild,
		NULL,
		NULL,
		iReplyToken
	);
}



/* 打开一个动态 direct-tcpip channel。 */
xsshcode xrtSshClientDirectTcpipOpen(
	xsshclient* pClient,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort,
	xsshchannel** ppChannel
)
{
	xsshclientdirectbuild Build = {
		Host,
		Originator,
		iPort,
		iOriginatorPort
	};

	if ( !xsshClientForwardPort(iPort) ||
		!xsshClientForwardPort(iOriginatorPort) ) {
		if ( xrtMemRangeValid(ppChannel, sizeof(*ppChannel)) ) {
			*ppChannel = NULL;
		}
		return XSSH_ERROR_ARGUMENT;
	}

	return xrtSshClientChannelOpen(
		pClient,
		xsshClientDirectBuild,
		&Build,
		ppChannel
	);
}



/* 解析 peer forwarding 字段并暂存读提交后的 confirmation。 */
xsshcode xrtSshClientForwardedTcpipAccept(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip,
	xsshchannel** ppChannel
)
{
	xsshtcpipopen Tcpip;
	xsshcode Code;

	if ( !xrtMemRangeValid(pTcpip, sizeof(*pTcpip)) ||
		!xrtMemRangeValid(ppChannel, sizeof(*ppChannel)) ||
		xrtMemRangesOverlap(
			pTcpip,
			sizeof(*pTcpip),
			ppChannel,
			sizeof(*ppChannel)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshForwardedTcpipOpenRead(pOpen, &Tcpip);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshClientChannelAccept(pClient, pOpen, ppChannel);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pTcpip = Tcpip;
	return XSSH_OK;
}



/* 请求服务端创建 remote forwarding 监听。 */
xsshcode xrtSshClientTcpipForward(
	xsshclient* pClient,
	xbytesview Address,
	uint32 iPort,
	uint64 iReplyToken
)
{
	xsshclientforwardbuild Build = { Address, iPort, false };

	if ( !xsshClientForwardPort(iPort) ) {
		return XSSH_ERROR_ARGUMENT;
	}

	return xsshClientForwardSend(pClient, &Build, iReplyToken);
}



/* 请求服务端撤销 remote forwarding 监听。 */
xsshcode xrtSshClientTcpipForwardCancel(
	xsshclient* pClient,
	xbytesview Address,
	uint32 iPort,
	uint64 iReplyToken
)
{
	xsshclientforwardbuild Build = { Address, iPort, true };

	if ( !xsshClientForwardPort(iPort) ) {
		return XSSH_ERROR_ARGUMENT;
	}

	return xsshClientForwardSend(pClient, &Build, iReplyToken);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_client_future.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CLIENT_FUTURE)
#include <string.h>





#if defined(XSSH_FEATURE_CLIENT_FUTURE)

/* 等待节点类型保持公共 API 参数精确，不使用带无关字段的万能入口。 */
typedef enum xsshclientfuturekind {
	XSSH_CLIENT_FUTURE_WAIT_CLIENT = 0,
	XSSH_CLIENT_FUTURE_WAIT_CHANNEL = 1,
	XSSH_CLIENT_FUTURE_WAIT_READ = 2,
	XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY = 3,
	XSSH_CLIENT_FUTURE_WAIT_GLOBAL_REPLY = 4
} xsshclientfuturekind;



/* 内部完成状态直接映射到 XRT Future 的四种终态。 */
typedef enum xsshclientfuturecompletion {
	XSSH_CLIENT_FUTURE_PENDING = 0,
	XSSH_CLIENT_FUTURE_RESOLVE = 1,
	XSSH_CLIENT_FUTURE_REJECT = 2,
	XSSH_CLIENT_FUTURE_CANCEL = 3,
	XSSH_CLIENT_FUTURE_CLOSED = 4
} xsshclientfuturecompletion;



typedef struct xsshclientfuturestate xsshclientfuturestate;
typedef struct xsshclientfuturewaiter xsshclientfuturewaiter;



/* 每个 Future 只占一个链表节点、Promise 和取消监听。 */
struct xsshclientfuturewaiter {
	xsshclientfuturewaiter* Next;
	xsshclientfuturestate* State;
	xpromise* Promise;
	xcancelwatch* CancelWatch;
	xsshchannel* Channel;
	uint64 ReplyToken;
	uint32 ChannelLocal;
	xsshclientfuturekind Kind;
	xsshclientwait ClientWait;
	xsshclientchannelwait ChannelWait;
	xsshchanneliostream Stream;
	bool Linked;
};



/* 管理器按客户端懒创建；引用保护 Close 回调中的同步 Future continuation。 */
struct xsshclientfuturestate {
	xspinlock Lock;
	xatomic32 References;
	xsshclientfuturewaiter* Head;
	xsshclientfuturewaiter* Tail;
	bool Closed;
};



/* 增加管理器内部引用。 */
static void xsshClientFutureStateRef(xsshclientfuturestate* pState)
{
	(void)xrtAtomic32FetchAdd(
		&pState->References,
		1u,
		XMEMORY_RELAXED
	);
}



/* 释放最后一个管理器引用及其关闭错误。 */
static void xsshClientFutureStateDestroy(xsshclientfuturestate* pState)
{
	if ( xrtAtomic32FetchAdd(
		&pState->References,
		UINT32_MAX,
		XMEMORY_ACQ_REL
	) != 1u ) {
		return;
	}
	(void)xrtSpinUnit(&pState->Lock);
	xrtFree(pState);
}



/* 在客户端 Worker 上按需建立唯一等待管理器。 */
static xsshclientfuturestate* xsshClientFutureState(
	xsshclient* pClient
)
{
	xsshclientfuturestate* pState =
		(xsshclientfuturestate*)pClient->FutureState;

	if ( pState != NULL ) {
		return pState;
	}
	pState = (xsshclientfuturestate*)xrtCalloc(1u, sizeof(*pState));
	if ( pState == NULL ) {
		return NULL;
	}
	if ( !xrtSpinInit(&pState->Lock) ) {
		xrtFree(pState);
		return NULL;
	}
	xrtAtomic32Init(&pState->References, 1u);
	pClient->FutureState = pState;
	return pState;
}



/* 从等待链中摘除一个已知节点。 */
static bool xsshClientFutureRemove(
	xsshclientfuturestate* pState,
	xsshclientfuturewaiter* pWaiter
)
{
	xsshclientfuturewaiter** ppCurrent = &pState->Head;
	xsshclientfuturewaiter* pPrevious = NULL;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pState->Tail == pWaiter ) {
		pState->Tail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	return true;
}



/* 把一个摘除节点发布到唯一 Future 终态并释放生产资源。 */
static void xsshClientFutureFinish(
	xsshclientfuturewaiter* pWaiter,
	xsshclientfuturecompletion Completion,
	const xerror* pError
)
{
	xpromise* pPromise = pWaiter->Promise;
	xsshclientfuturestate* pState = pWaiter->State;

	pWaiter->Promise = NULL;
	xrtCancelUnwatch(pWaiter->CancelWatch);
	pWaiter->CancelWatch = NULL;
	if ( Completion == XSSH_CLIENT_FUTURE_RESOLVE ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( (Completion == XSSH_CLIENT_FUTURE_REJECT) &&
		(pError != NULL) ) {
		(void)xrtPromiseReject(pPromise, pError);
	} else if ( Completion == XSSH_CLIENT_FUTURE_CANCEL ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtPromiseDestroy(pPromise);
	xsshClientFutureStateDestroy(pState);
	xrtFree(pWaiter);
}



/* Future 取消只摘除本次等待，不改变 SSH channel 或连接。 */
static void xsshClientFutureCancel(ptr pData)
{
	xsshclientfuturewaiter* pWaiter =
		(xsshclientfuturewaiter*)pData;
	xsshclientfuturestate* pState = pWaiter->State;
	bool bRemoved;

	(void)xrtSpinLock(&pState->Lock);
	bRemoved = pWaiter->Linked &&
		xsshClientFutureRemove(pState, pWaiter);
	(void)xrtSpinUnlock(&pState->Lock);
	if ( bRemoved ) {
		xsshClientFutureFinish(
			pWaiter,
			XSSH_CLIENT_FUTURE_CANCEL,
			NULL
		);
	}
}



/* 创建一个可立即完成或进入管理器链表的 Future 节点。 */
static xsshclientfuturewaiter* xsshClientFutureCreate(
	xsshclient* pClient,
	xfuture** ppFuture
)
{
	xsshclientfuturestate* pState;
	xsshclientfuturewaiter* pWaiter;
	xcancel* pCancel;
	xerror* pError;

	*ppFuture = NULL;
	if ( !xrtSshClientIsCurrent(pClient) ) {
		xrtSetErrorInfo(
			XERR_STATE,
			"xrt.ssh.client.future",
			(int32)XSSH_ERROR_STATE,
			"SSH client Future must be created on its network Worker"
		);
		return NULL;
	}
	pState = xsshClientFutureState(pClient);
	if ( pState == NULL ) {
		return NULL;
	}
	pWaiter = (xsshclientfuturewaiter*)xrtCalloc(
		1u,
		sizeof(*pWaiter)
	);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Promise = xrtPromiseCreate(ppFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		xrtFree(pWaiter);
		return NULL;
	}
	pWaiter->State = pState;
	xsshClientFutureStateRef(pState);
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->CancelWatch = xrtCancelWatch(
			pCancel,
			xsshClientFutureCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->CancelWatch != NULL ) {
		return pWaiter;
	}
	pError = xrtTakeError();
	xrtFutureDestroy(*ppFuture);
	*ppFuture = NULL;
	xrtPromiseDestroy(pWaiter->Promise);
	xsshClientFutureStateDestroy(pState);
	xrtFree(pWaiter);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return NULL;
}



/* 把节点链接到管理器尾部，保持同条件 waiter 的 FIFO 完成顺序。 */
static void xsshClientFutureLink(
	xsshclientfuturestate* pState,
	xsshclientfuturewaiter* pWaiter
)
{
	pWaiter->Linked = true;
	if ( pState->Tail != NULL ) {
		pState->Tail->Next = pWaiter;
	} else {
		pState->Head = pWaiter;
	}
	pState->Tail = pWaiter;
}



/* 返回客户端水平条件的当前结果。 */
static xsshclientfuturecompletion xsshClientFutureClientResult(
	xsshclient* pClient,
	xsshclientwait Wait
)
{
	xsshclientstate State = xrtSshClientState(pClient);
	xnetstream* pStream;

	if ( (Wait == XSSH_CLIENT_WAIT_READY) &&
		(State == XSSH_CLIENT_READY) ) {
		return XSSH_CLIENT_FUTURE_RESOLVE;
	}
	if ( Wait == XSSH_CLIENT_WAIT_DRAIN ) {
		pStream = xrtSshSessionStreamTcp(&pClient->Stream);
		if ( (pStream != NULL) && (xrtNetStreamPending(pStream) == 0u) ) {
			return XSSH_CLIENT_FUTURE_RESOLVE;
		}
	}
	if ( (Wait == XSSH_CLIENT_WAIT_CLOSE) &&
		(State == XSSH_CLIENT_CLOSED) ) {
		return XSSH_CLIENT_FUTURE_RESOLVE;
	}
	return State >= XSSH_CLIENT_CLOSING ?
		XSSH_CLIENT_FUTURE_CLOSED : XSSH_CLIENT_FUTURE_PENDING;
}



/* 返回 channel 水平条件的当前结果。 */
static xsshclientfuturecompletion xsshClientFutureChannelResult(
	const xsshchannel* pChannel,
	xsshclientchannelwait Wait
)
{
	xsshchannelcorephase Phase = xrtSshChannelCorePhase(&pChannel->Core);

	if ( Wait == XSSH_CLIENT_CHANNEL_WAIT_OPEN ) {
		if ( Phase == XSSH_CHANNEL_CORE_OPEN ) {
			return XSSH_CLIENT_FUTURE_RESOLVE;
		}
		if ( Phase == XSSH_CHANNEL_CORE_FAILED ) {
			return XSSH_CLIENT_FUTURE_REJECT;
		}
	}
	if ( (Wait == XSSH_CLIENT_CHANNEL_WAIT_WRITE) &&
		xrtSshChannelCanSendData(&pChannel->Core.State) &&
		(xrtSshChannelIoWritable(&pChannel->Io) != 0u) ) {
		return XSSH_CLIENT_FUTURE_RESOLVE;
	}
	if ( (Wait == XSSH_CLIENT_CHANNEL_WAIT_EOF) &&
		(pChannel->Core.State.RemoteEof ||
		 pChannel->Core.State.RemoteClose) ) {
		return XSSH_CLIENT_FUTURE_RESOLVE;
	}
	if ( (Wait == XSSH_CLIENT_CHANNEL_WAIT_CLOSE) &&
		pChannel->Core.State.RemoteClose ) {
		return XSSH_CLIENT_FUTURE_RESOLVE;
	}
	return (Phase == XSSH_CHANNEL_CORE_FAILED) ||
		(Phase == XSSH_CHANNEL_CORE_CLOSED) ?
		XSSH_CLIENT_FUTURE_CLOSED : XSSH_CLIENT_FUTURE_PENDING;
}



/* 为远端 channel open 拒绝创建稳定结构化错误。 */
static xerror* xsshClientFutureOpenError(
	const xsshchannel* pChannel
)
{
	return xrtErrorCreate(
		XERR_IO,
		"xrt.ssh.channel.open",
		(int32)pChannel->Core.FailureReason,
		"SSH channel open was rejected"
	);
}



/* 提交一个普通客户端等待。 */
xfuture* xrtSshClientWaitAsync(
	xsshclient* pClient,
	xsshclientwait Wait
)
{
	xsshclientfuturecompletion Completion;
	xsshclientfuturewaiter* pWaiter;
	xfuture* pFuture;

	if ( (Wait < XSSH_CLIENT_WAIT_READY) ||
		(Wait > XSSH_CLIENT_WAIT_CLOSE) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pWaiter = xsshClientFutureCreate(pClient, &pFuture);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Kind = XSSH_CLIENT_FUTURE_WAIT_CLIENT;
	pWaiter->ClientWait = Wait;
	(void)xrtSpinLock(&pWaiter->State->Lock);
	Completion = pWaiter->State->Closed ?
		(Wait == XSSH_CLIENT_WAIT_CLOSE ?
		 XSSH_CLIENT_FUTURE_RESOLVE : XSSH_CLIENT_FUTURE_CLOSED) :
		xsshClientFutureClientResult(pClient, Wait);
	if ( Completion == XSSH_CLIENT_FUTURE_PENDING ) {
		xsshClientFutureLink(pWaiter->State, pWaiter);
	}
	(void)xrtSpinUnlock(&pWaiter->State->Lock);
	if ( Completion != XSSH_CLIENT_FUTURE_PENDING ) {
		xsshClientFutureFinish(pWaiter, Completion, NULL);
	}
	return pFuture;
}



/* 提交一个 channel 生命周期或写预算等待。 */
xfuture* xrtSshClientChannelWaitAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshclientchannelwait Wait
)
{
	xsshclientfuturecompletion Completion;
	xsshclientfuturewaiter* pWaiter;
	xfuture* pFuture;
	xerror* pError = NULL;

	if ( (Wait < XSSH_CLIENT_CHANNEL_WAIT_OPEN) ||
		(Wait > XSSH_CLIENT_CHANNEL_WAIT_CLOSE) ||
		!xrtSshClientOwnsChannel(pClient, pChannel) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pWaiter = xsshClientFutureCreate(pClient, &pFuture);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Kind = XSSH_CLIENT_FUTURE_WAIT_CHANNEL;
	pWaiter->Channel = pChannel;
	pWaiter->ChannelLocal = pChannel->Core.Local;
	pWaiter->ChannelWait = Wait;
	(void)xrtSpinLock(&pWaiter->State->Lock);
	Completion = pWaiter->State->Closed ?
		XSSH_CLIENT_FUTURE_CLOSED :
		xsshClientFutureChannelResult(pChannel, Wait);
	if ( Completion == XSSH_CLIENT_FUTURE_PENDING ) {
		xsshClientFutureLink(pWaiter->State, pWaiter);
	}
	(void)xrtSpinUnlock(&pWaiter->State->Lock);
	if ( Completion == XSSH_CLIENT_FUTURE_REJECT ) {
		pError = xsshClientFutureOpenError(pChannel);
	}
	if ( Completion != XSSH_CLIENT_FUTURE_PENDING ) {
		xsshClientFutureFinish(pWaiter, Completion, pError);
	}
	xrtErrorFree(pError);
	return pFuture;
}



/* 提交一个不会消费 channel 数据的可读等待。 */
xfuture* xrtSshClientChannelReadAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream
)
{
	xsshclientfuturecompletion Completion;
	xsshclientfuturewaiter* pWaiter;
	xfuture* pFuture;

	if ( ((Stream != XSSH_CHANNEL_IO_DATA) &&
		 (Stream != XSSH_CHANNEL_IO_STDERR)) ||
		!xrtSshClientOwnsChannel(pClient, pChannel) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pWaiter = xsshClientFutureCreate(pClient, &pFuture);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Kind = XSSH_CLIENT_FUTURE_WAIT_READ;
	pWaiter->Channel = pChannel;
	pWaiter->ChannelLocal = pChannel->Core.Local;
	pWaiter->Stream = Stream;
	(void)xrtSpinLock(&pWaiter->State->Lock);
	if ( pWaiter->State->Closed ) {
		Completion = XSSH_CLIENT_FUTURE_CLOSED;
	} else if ( xrtSshChannelIoReadable(&pChannel->Io, Stream) != 0u ) {
		Completion = XSSH_CLIENT_FUTURE_RESOLVE;
	} else if ( pChannel->Core.State.RemoteEof ||
		pChannel->Core.State.RemoteClose ) {
		Completion = XSSH_CLIENT_FUTURE_CLOSED;
	} else {
		Completion = XSSH_CLIENT_FUTURE_PENDING;
		xsshClientFutureLink(pWaiter->State, pWaiter);
	}
	(void)xrtSpinUnlock(&pWaiter->State->Lock);
	if ( Completion != XSSH_CLIENT_FUTURE_PENDING ) {
		xsshClientFutureFinish(pWaiter, Completion, NULL);
	}
	return pFuture;
}



/* 提交一个按稳定 token 精确关联的 channel request 等待。 */
xfuture* xrtSshClientChannelReplyAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint64 iReplyToken
)
{
	xsshclientfuturewaiter* pWaiter;
	xfuture* pFuture;

	if ( !xrtSshClientOwnsChannel(pClient, pChannel) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pWaiter = xsshClientFutureCreate(pClient, &pFuture);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Kind = XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY;
	pWaiter->Channel = pChannel;
	pWaiter->ChannelLocal = pChannel->Core.Local;
	pWaiter->ReplyToken = iReplyToken;
	(void)xrtSpinLock(&pWaiter->State->Lock);
	if ( pWaiter->State->Closed ) {
		(void)xrtSpinUnlock(&pWaiter->State->Lock);
		xsshClientFutureFinish(
			pWaiter,
			XSSH_CLIENT_FUTURE_CLOSED,
			NULL
		);
	} else {
		xsshClientFutureLink(pWaiter->State, pWaiter);
		(void)xrtSpinUnlock(&pWaiter->State->Lock);
	}
	return pFuture;
}



/* 提交一个按稳定 token 精确关联的 global request 等待。 */
xfuture* xrtSshClientGlobalReplyAsync(
	xsshclient* pClient,
	uint64 iReplyToken
)
{
	xsshclientfuturewaiter* pWaiter;
	xfuture* pFuture;

	pWaiter = xsshClientFutureCreate(pClient, &pFuture);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Kind = XSSH_CLIENT_FUTURE_WAIT_GLOBAL_REPLY;
	pWaiter->ReplyToken = iReplyToken;
	(void)xrtSpinLock(&pWaiter->State->Lock);
	if ( pWaiter->State->Closed ) {
		(void)xrtSpinUnlock(&pWaiter->State->Lock);
		xsshClientFutureFinish(
			pWaiter,
			XSSH_CLIENT_FUTURE_CLOSED,
			NULL
		);
	} else {
		xsshClientFutureLink(pWaiter->State, pWaiter);
		(void)xrtSpinUnlock(&pWaiter->State->Lock);
	}
	return pFuture;
}



/* 判断一个等待节点是否由本次已提交信号满足。 */
static bool xsshClientFutureMatches(
	const xsshclientfuturewaiter* pWaiter,
	const xsshclientfuturenotice* pNotice
)
{
	const xsshclientchannelnotice* pChannel = pNotice->ChannelNotice;
	const xsshclientglobalnotice* pGlobal = pNotice->GlobalNotice;
	bool bChannelMatches = (pWaiter->Channel == pNotice->Channel) &&
		(!pNotice->HasChannelLocal ||
		 (pWaiter->ChannelLocal == pNotice->ChannelLocal));

	if ( pNotice->Signal == XSSH_CLIENT_FUTURE_CLOSE ) {
		return true;
	}
	if ( pNotice->Signal == XSSH_CLIENT_FUTURE_CHANNEL_REMOVED ) {
		return pNotice->HasChannelLocal &&
			(pWaiter->ChannelLocal == pNotice->ChannelLocal) &&
			((pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL) ||
			 (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_READ) ||
			 (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY));
	}
	if ( pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CLIENT ) {
		return ((pWaiter->ClientWait == XSSH_CLIENT_WAIT_READY) &&
			(pNotice->Signal == XSSH_CLIENT_FUTURE_READY)) ||
			((pWaiter->ClientWait == XSSH_CLIENT_WAIT_DRAIN) &&
			 (pNotice->Signal == XSSH_CLIENT_FUTURE_DRAIN));
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_READ) &&
		(pNotice->Signal == XSSH_CLIENT_FUTURE_DATA) ) {
		return bChannelMatches &&
			(pWaiter->Stream == pNotice->Stream) &&
			(xrtSshChannelIoReadable(
				&pWaiter->Channel->Io,
				pWaiter->Stream
			) != 0u);
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_READ) &&
		(pNotice->Signal == XSSH_CLIENT_FUTURE_CHANNEL) ) {
		return (pChannel != NULL) &&
			bChannelMatches &&
			((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_EOF) ||
			 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED));
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_GLOBAL_REPLY) &&
		(pNotice->Signal == XSSH_CLIENT_FUTURE_GLOBAL) ) {
		return (pGlobal != NULL) &&
			(pWaiter->ReplyToken == pGlobal->ReplyToken);
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY) &&
		(pNotice->Signal == XSSH_CLIENT_FUTURE_CHANNEL) ) {
		return (pChannel != NULL) &&
			bChannelMatches &&
			(((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED)) ||
			 (pChannel->HasReplyToken &&
			  (pWaiter->ReplyToken == pChannel->ReplyToken) &&
			  ((pChannel->Event ==
			    XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS) ||
			   (pChannel->Event ==
			    XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE))));
	}
	if ( pWaiter->Kind != XSSH_CLIENT_FUTURE_WAIT_CHANNEL ) {
		return false;
	}
	if ( (pWaiter->ChannelWait == XSSH_CLIENT_CHANNEL_WAIT_WRITE) &&
		(pNotice->Signal == XSSH_CLIENT_FUTURE_WRITABLE) ) {
		return bChannelMatches;
	}
	if ( (pNotice->Signal != XSSH_CLIENT_FUTURE_CHANNEL) ||
		(pChannel == NULL) ||
		!bChannelMatches ) {
		return false;
	}
	return ((pWaiter->ChannelWait == XSSH_CLIENT_CHANNEL_WAIT_OPEN) &&
		((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_OPENED) ||
		 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED) ||
		 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED))) ||
		((pWaiter->ChannelWait == XSSH_CLIENT_CHANNEL_WAIT_WRITE) &&
		((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_WRITABLE) ||
		 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED) ||
		 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED) ||
		 ((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_OPENED) &&
		  xrtSshChannelCanSendData(&pChannel->Channel->Core.State) &&
		  (xrtSshChannelIoWritable(&pChannel->Channel->Io) != 0u)))) ||
		((pWaiter->ChannelWait == XSSH_CLIENT_CHANNEL_WAIT_EOF) &&
		 ((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_EOF) ||
		  (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED))) ||
		((pWaiter->ChannelWait == XSSH_CLIENT_CHANNEL_WAIT_CLOSE) &&
		 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED));
}



/* 把匹配信号转换为成功、远端拒绝或连接关闭。 */
static xsshclientfuturecompletion xsshClientFutureCompletion(
	const xsshclientfuturewaiter* pWaiter,
	const xsshclientfuturenotice* pNotice
)
{
	if ( pNotice->Signal == XSSH_CLIENT_FUTURE_CLOSE ) {
		return (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CLIENT) &&
			(pWaiter->ClientWait == XSSH_CLIENT_WAIT_CLOSE) ?
			XSSH_CLIENT_FUTURE_RESOLVE :
			(pNotice->Error != NULL ?
			 XSSH_CLIENT_FUTURE_REJECT : XSSH_CLIENT_FUTURE_CLOSED);
	}
	if ( pNotice->Signal == XSSH_CLIENT_FUTURE_CHANNEL_REMOVED ) {
		return XSSH_CLIENT_FUTURE_CLOSED;
	}
	if ( (pNotice->Signal == XSSH_CLIENT_FUTURE_CHANNEL) &&
		(pNotice->ChannelNotice != NULL) &&
		((pNotice->ChannelNotice->Event ==
		  XSSH_CLIENT_CHANNEL_EVENT_EOF) ||
		 (pNotice->ChannelNotice->Event ==
		  XSSH_CLIENT_CHANNEL_EVENT_CLOSED)) &&
		((pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_READ) ||
		 (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY) ||
		 ((pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL) &&
		  (pWaiter->ChannelWait != XSSH_CLIENT_CHANNEL_WAIT_EOF) &&
		  (pWaiter->ChannelWait != XSSH_CLIENT_CHANNEL_WAIT_CLOSE))) ) {
		return XSSH_CLIENT_FUTURE_CLOSED;
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL) &&
		(pNotice->ChannelNotice != NULL) &&
		(pNotice->ChannelNotice->Event ==
		 XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED) ) {
		return XSSH_CLIENT_FUTURE_REJECT;
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY) &&
		(pNotice->ChannelNotice->Event ==
		 XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE) ) {
		return XSSH_CLIENT_FUTURE_REJECT;
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_GLOBAL_REPLY) &&
		(pNotice->GlobalNotice->Event ==
		 XSSH_CLIENT_GLOBAL_EVENT_REQUEST_FAILURE) ) {
		return XSSH_CLIENT_FUTURE_REJECT;
	}
	return XSSH_CLIENT_FUTURE_RESOLVE;
}



/* 为远端语义拒绝创建不依赖输入 packet 的错误。 */
static xerror* xsshClientFutureNoticeError(
	const xsshclientfuturewaiter* pWaiter,
	const xsshclientfuturenotice* pNotice
)
{
	if ( pNotice->Signal == XSSH_CLIENT_FUTURE_CLOSE ) {
		return xrtErrorRef(pNotice->Error);
	}
	if ( pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL ) {
		return xrtErrorCreate(
			XERR_IO,
			"xrt.ssh.channel.open",
			(int32)pNotice->ChannelNotice->Reason,
			"SSH channel open was rejected"
		);
	}
	return xrtErrorCreate(
		XERR_IO,
		pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_GLOBAL_REPLY ?
			"xrt.ssh.global.request" : "xrt.ssh.channel.request",
		(int32)XSSH_ERROR_STATE,
		"SSH request was rejected"
	);
}



/* 发布提交后信号，先从锁内摘除，再在线程安全的 Promise 路径完成。 */
void __xrtSshClientFutureNotify(
	xsshclient* pClient,
	const xsshclientfuturenotice* pNotice
)
{
	xsshclientfuturestate* pState =
		(xsshclientfuturestate*)pClient->FutureState;
	xsshclientfuturewaiter* pReadyHead = NULL;
	xsshclientfuturewaiter* pReadyTail = NULL;
	xsshclientfuturewaiter* pWaiter;
	xsshclientfuturewaiter* pNext;

	if ( pState == NULL ) {
		return;
	}
	xsshClientFutureStateRef(pState);
	(void)xrtSpinLock(&pState->Lock);
	if ( (pNotice->Signal == XSSH_CLIENT_FUTURE_CLOSE) &&
		!pState->Closed ) {
		pState->Closed = true;
	}
	pWaiter = pState->Head;
	while ( pWaiter != NULL ) {
		pNext = pWaiter->Next;
		if ( xsshClientFutureMatches(pWaiter, pNotice) ) {
			(void)xsshClientFutureRemove(pState, pWaiter);
			if ( pReadyTail != NULL ) {
				pReadyTail->Next = pWaiter;
			} else {
				pReadyHead = pWaiter;
			}
			pReadyTail = pWaiter;
		}
		pWaiter = pNext;
	}
	(void)xrtSpinUnlock(&pState->Lock);
	for ( pWaiter = pReadyHead; pWaiter != NULL; pWaiter = pNext ) {
		xsshclientfuturecompletion Completion =
			xsshClientFutureCompletion(pWaiter, pNotice);
		xerror* pError = Completion == XSSH_CLIENT_FUTURE_REJECT ?
			xsshClientFutureNoticeError(pWaiter, pNotice) : NULL;

		pNext = pWaiter->Next;
		pWaiter->Next = NULL;
		xsshClientFutureFinish(pWaiter, Completion, pError);
		xrtErrorFree(pError);
	}
	xsshClientFutureStateDestroy(pState);
}



/* Clear 先关闭全部等待，再分离客户端持有的管理器引用。 */
void __xrtSshClientFutureClear(xsshclient* pClient)
{
	xsshclientfuturestate* pState =
		(xsshclientfuturestate*)pClient->FutureState;
	xsshclientfuturewaiter* pHead;
	xsshclientfuturewaiter* pNext;

	if ( pState == NULL ) {
		return;
	}
	pClient->FutureState = NULL;
	(void)xrtSpinLock(&pState->Lock);
	pState->Closed = true;
	pHead = pState->Head;
	pState->Head = NULL;
	pState->Tail = NULL;
	for ( pNext = pHead; pNext != NULL; pNext = pNext->Next ) {
		pNext->Linked = false;
	}
	(void)xrtSpinUnlock(&pState->Lock);
	while ( pHead != NULL ) {
		pNext = pHead->Next;
		pHead->Next = NULL;
		xsshClientFutureFinish(
			pHead,
			XSSH_CLIENT_FUTURE_CLOSED,
			NULL
		);
		pHead = pNext;
	}
	xsshClientFutureStateDestroy(pState);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xssh/src/session/ssh_client_pty.c */
/* ========================================================================== */

#if defined(XSSH_FEATURE_CLIENT_PTY)



#if defined(XSSH_FEATURE_CLIENT_PTY)

/* PTY 与 resize 共用尺寸字段，只有 PTY 借用 Terminal 和 Modes。 */
typedef struct xsshclientptybuild {
	xsshchannel* Channel;
	xbytesview Terminal;
	xbytesview Modes;
	uint32 Columns;
	uint32 Rows;
	uint32 PixelWidth;
	uint32 PixelHeight;
	bool WantReply;
	bool Resize;
} xsshclientptybuild;



/* 写出 PTY 或 window-change，并统一使用远端 channel id。 */
static xsshcode xsshClientPtyBuild(xsshwriter* pWriter, ptr pData)
{
	xsshclientptybuild* pBuild = (xsshclientptybuild*)pData;
	uint32 iLocal;
	uint32 iRemote;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		!xrtMemRangeValid(pBuild->Channel, sizeof(*pBuild->Channel)) ||
		!xrtSshChannelCoreIds(
			&pBuild->Channel->Core,
			&iLocal,
			&iRemote
		) ) {
		return XSSH_ERROR_STATE;
	}
	(void)iLocal;
	if ( pBuild->Resize ) {
		return xrtSshChannelWindowChangeWrite(
			pWriter,
			iRemote,
			pBuild->Columns,
			pBuild->Rows,
			pBuild->PixelWidth,
			pBuild->PixelHeight
		);
	}
	return xrtSshChannelPtyWrite(
		pWriter,
		iRemote,
		pBuild->WantReply,
		pBuild->Terminal,
		pBuild->Columns,
		pBuild->Rows,
		pBuild->PixelWidth,
		pBuild->PixelHeight,
		pBuild->Modes
	);
}



/* 发送 PTY 请求，并按需预留 channel reply token。 */
xsshcode xrtSshClientSessionPty(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Terminal,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight,
	xbytesview Modes,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientptybuild Build = {
		pChannel,
		Terminal,
		Modes,
		iColumns,
		iRows,
		iPixelWidth,
		iPixelHeight,
		bWantReply,
		false
	};
	xsshreplyqueue* pReplies = NULL;
	size_t iCount;
	xsshcode Code;

	if ( (xrtSshClientState(pClient) != XSSH_CLIENT_READY) ||
		!xrtSshClientIsCurrent(pClient) ||
		!xrtSshClientOwnsChannel(pClient, pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	if ( bWantReply ) {
		iCount = xrtSshReplyQueueCount(&pChannel->Replies);
		if ( iCount == SIZE_MAX ) {
			return XSSH_ERROR_OVERFLOW;
		}
		Code = xrtSshChannelReplyReserve(pChannel, iCount + 1u);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		pReplies = &pChannel->Replies;
	}
	return xrtSshClientBuild(
		pClient,
		xsshClientPtyBuild,
		&Build,
		pChannel,
		pReplies,
		iReplyToken
	);
}



/* 发送 window-change 通知。 */
xsshcode xrtSshClientSessionResize(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight
)
{
	xsshclientptybuild Build = {
		pChannel,
		{ NULL, 0u },
		{ NULL, 0u },
		iColumns,
		iRows,
		iPixelWidth,
		iPixelHeight,
		false,
		true
	};

	if ( (xrtSshClientState(pClient) != XSSH_CLIENT_READY) ||
		!xrtSshClientIsCurrent(pClient) ||
		!xrtSshClientOwnsChannel(pClient, pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshClientBuild(
		pClient,
		xsshClientPtyBuild,
		&Build,
		pChannel,
		NULL,
		0u
	);
}

#endif
#endif

#endif
