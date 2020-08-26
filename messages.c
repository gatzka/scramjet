/*
 * SPDX-License-Identifier: MIT
 *
 * The MIT License (MIT)
 *
 * Copyright (c) <2019> <Stephan Gatzka>
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

#include <stddef.h>

#include "cio/buffered_stream.h"
#include "cio/compiler.h"
#include "cio/error_code.h"

#include "jet_client.h"
#include "messages.h"
#include "sj_log.h"

static void handle_request(struct jet_client *client)
{
	(void)client;
}

static void handle_response(struct jet_client *client)
{
	(void)client;
}

static void message_type_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by client!");
		cio_buffered_stream_close(bs);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading message type failed!");
		cio_buffered_stream_close(bs);
		return;
	}

	uint8_t message_type;
	memcpy(&message_type, cio_read_buffer_get_read_ptr(buffer), sizeof(message_type));
	cio_read_buffer_consume(buffer, sizeof(message_type));

	struct jet_client *client = (struct jet_client *)handler_context;

	switch ((enum jet_message)message_type) {
	case MESSAGE_REQUEST:
		handle_request(client);
		break;

	case MESSAGE_RESPONSE:
		handle_response(client);
		break;

	default:
		sclog_message(&sj_log, SCLOG_ERROR, "unknown message type sent by client!");
		cio_buffered_stream_close(bs);
		break;
	}
}

void read_jet_message(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err)
{
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "write before reading message type failed!");
		cio_buffered_stream_close(bs);
		return;
	}

	struct jet_client *client = (struct jet_client *)handler_context;
	err = cio_buffered_stream_read_at_least(bs, &client->rb, 1, message_type_read, client);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading message type!");
		cio_buffered_stream_close(bs);
	}
}
