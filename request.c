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

#include <stddef.h>
#include <stdint.h>

#include "cio/buffered_stream.h"
#include "cio/compiler.h"
#include "cio/endian.h"
#include "cio/error_code.h"
#include "cio/read_buffer.h"

#include "jet_error.h"
#include "jet_function.h"
#include "request.h"
#include "response.h"
#include "sj_log.h"
#include "state.h"

static void value_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by client!");
		cio_buffered_stream_close(bs);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading value failed!");
		cio_buffered_stream_close(bs);
		return;
	}

	struct jet_client *client = (struct jet_client *)handler_context;
	char *value = client->key + client->key_length + 4;

	enum jet_error jet_error = add_state(client, client->key_length, client->key, client->value_length, value);

	cio_read_buffer_consume(buffer, client->key_length);
	cio_read_buffer_consume(buffer, 4);
	cio_read_buffer_consume(buffer, client->value_length);

	send_response(client, jet_error);
}

static void value_length_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by client!");
		cio_buffered_stream_close(bs);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading key failed!");
		cio_buffered_stream_close(bs);
		return;
	}

	uint32_t value_length;
	memcpy(&value_length, cio_read_buffer_get_read_ptr(buffer), sizeof(value_length));
	value_length = cio_le32toh(value_length);

	struct jet_client *client = (struct jet_client *)handler_context;
	client->value_length = value_length;

	err = cio_buffered_stream_read_at_least(&client->bs, &client->rb, value_length, value_read, client);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading value!");
		cio_buffered_stream_close(&client->bs);
	}
}

static void key_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by client!");
		cio_buffered_stream_close(bs);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading key failed!");
		cio_buffered_stream_close(bs);
		return;
	}

	struct jet_client *client = (struct jet_client *)handler_context;
	client->key = (char *)cio_read_buffer_get_read_ptr(buffer);

	err = cio_buffered_stream_read_at_least(&client->bs, &client->rb, 4, value_length_read, client);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading value length!");
		cio_buffered_stream_close(&client->bs);
	}
}

static void key_length_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by client!");
		cio_buffered_stream_close(bs);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading key length failed!");
		cio_buffered_stream_close(bs);
		return;
	}

	uint16_t key_length;
	memcpy(&key_length, cio_read_buffer_get_read_ptr(buffer), sizeof(key_length));
	key_length = cio_le16toh(key_length);

	struct jet_client *client = (struct jet_client *)handler_context;
	client->key_length = key_length;
	cio_read_buffer_consume(buffer, sizeof(key_length));

	err = cio_buffered_stream_read_at_least(&client->bs, &client->rb, key_length, key_read, client);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading key!");
		cio_buffered_stream_close(&client->bs);
	}
}

static void handle_add_state(struct jet_client *client)
{
	enum cio_error err = cio_buffered_stream_read_at_least(&client->bs, &client->rb, 2, key_length_read, client);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading key length!");
		cio_buffered_stream_close(&client->bs);
	}
}

static void handle_remove_state(struct jet_client *client)
{
	(void)client;
}

static void jet_function_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by client!");
		cio_buffered_stream_close(bs);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading jet function failed!");
		cio_buffered_stream_close(bs);
		return;
	}

	uint8_t jet_function;
	memcpy(&jet_function, cio_read_buffer_get_read_ptr(buffer), sizeof(jet_function));
	cio_read_buffer_consume(buffer, sizeof(jet_function));

	struct jet_client *client = (struct jet_client *)handler_context;

	switch ((enum jet_function)jet_function) {
	case ADD_STATE:
		handle_add_state(client);
		break;

	case REMOVE_STATE:
		handle_remove_state(client);
		break;

	default:
		sclog_message(&sj_log, SCLOG_ERROR, "unknown jet function sent by client!");
		cio_buffered_stream_close(bs);
		break;
	}
}

static void request_id_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by client!");
		cio_buffered_stream_close(bs);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading message id failed!");
		cio_buffered_stream_close(bs);
		return;
	}

	uint32_t request_id;
	memcpy(&request_id, cio_read_buffer_get_read_ptr(buffer), sizeof(request_id));
	cio_read_buffer_consume(buffer, sizeof(request_id));

	struct jet_client *client = (struct jet_client *)handler_context;
	client->request_id = request_id;

	err = cio_buffered_stream_read_at_least(bs, buffer, 1, jet_function_read, client);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading jet function");
		cio_buffered_stream_close(bs);
	}
}

void handle_request(struct jet_client *client)
{
	enum cio_error err = cio_buffered_stream_read_at_least(&client->bs, &client->rb, 4, request_id_read, client);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading request id!");
		cio_buffered_stream_close(&client->bs);
	}
}
