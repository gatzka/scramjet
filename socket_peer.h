/*
 * SPDX-License-Identifier: MIT
 *
 * The MIT License (MIT)
 *
 * Copyright (c) <2020> <Stephan Gatzka>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SCRAMJET_SOCKET_PEER_H
#define SCRAMJET_SOCKET_PEER_H

#include <stddef.h>

#include "cio/buffered_stream.h"
#include "cio/eventloop.h"
#include "cio/read_buffer.h"
#include "cio/server_socket.h"
#include "cio/socket.h"
#include "cio/write_buffer.h"

#include "jet_peer.h"

// TODO(gatzka): make BUFFER_SIZE configurable via cmake.
enum { BUFFER_SIZE = 128 };

struct socket_peer {
	struct jet_peer peer;

	uint32_t message_length;
	struct cio_socket socket;
	struct cio_read_buffer rb;
	struct cio_write_buffer wb;
	uint32_t write_message_length;
	struct cio_buffered_stream bs;
	uint8_t buffer[BUFFER_SIZE];
};

enum cio_error prepare_socket_peer_connection(struct cio_server_socket *ss, struct cio_eventloop *loop);

#endif
