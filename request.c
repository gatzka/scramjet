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
	(void)bs;
	(void)num_bytes;

	struct jet_peer *peer = (struct jet_peer *)handler_context;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by peer!");
		peer->shutdown_peer(peer);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading value failed!");
		peer->shutdown_peer(peer);
		return;
	}

	char *value = peer->key + peer->key_length + 4;

	enum jet_error jet_error = add_state(peer, peer->key_length, peer->key, peer->value_length, value);

	cio_read_buffer_consume(buffer, peer->key_length);
	cio_read_buffer_consume(buffer, 4);
	cio_read_buffer_consume(buffer, peer->value_length);

	send_response(peer, jet_error);
}

static void value_length_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	struct jet_peer *peer = (struct jet_peer *)handler_context;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by peer!");
		peer->shutdown_peer(peer);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading key failed!");
		peer->shutdown_peer(peer);
		return;
	}

	uint32_t value_length;
	memcpy(&value_length, cio_read_buffer_get_read_ptr(buffer), sizeof(value_length));
	value_length = cio_le32toh(value_length);

	peer->value_length = value_length;

	err = cio_buffered_stream_read_at_least(bs, &peer->rb, value_length, value_read, peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading value!");
		cio_buffered_stream_close(&peer->bs);
	}
}

static void key_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	struct jet_peer *peer = (struct jet_peer *)handler_context;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by peer!");
		peer->shutdown_peer(peer);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading key failed!");
		peer->shutdown_peer(peer);
		return;
	}

	peer->key = (char *)cio_read_buffer_get_read_ptr(buffer);

	err = cio_buffered_stream_read_at_least(bs, &peer->rb, 4, value_length_read, peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading value length!");
		cio_buffered_stream_close(&peer->bs);
	}
}

static void key_length_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	struct jet_peer *peer = (struct jet_peer *)handler_context;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by peer!");
		peer->shutdown_peer(peer);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading key length failed!");
		peer->shutdown_peer(peer);
		return;
	}

	uint16_t key_length;
	memcpy(&key_length, cio_read_buffer_get_read_ptr(buffer), sizeof(key_length));
	key_length = cio_le16toh(key_length);

	peer->key_length = key_length;
	cio_read_buffer_consume(buffer, sizeof(key_length));

	err = cio_buffered_stream_read_at_least(bs, &peer->rb, key_length, key_read, peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading key!");
		peer->shutdown_peer(peer);
	}
}

static void handle_add_state(struct jet_peer *peer, struct cio_buffered_stream *bs)
{
	enum cio_error err = cio_buffered_stream_read_at_least(bs, &peer->rb, 2, key_length_read, peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading key length!");
		peer->shutdown_peer(peer);
	}
}

static void handle_remove_state(struct jet_peer *peer, struct cio_buffered_stream *bs)
{
	(void)peer;
	(void)bs;
}

static void jet_function_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)bs;
	(void)num_bytes;

	struct jet_peer *peer = (struct jet_peer *)handler_context;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by peer!");
		peer->shutdown_peer(peer);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading jet function failed!");
		peer->shutdown_peer(peer);
		return;
	}

	uint8_t jet_function;
	memcpy(&jet_function, cio_read_buffer_get_read_ptr(buffer), sizeof(jet_function));
	cio_read_buffer_consume(buffer, sizeof(jet_function));

	switch ((enum jet_function)jet_function) {
	case ADD_STATE:
		handle_add_state(peer, bs);
		break;

	case REMOVE_STATE:
		handle_remove_state(peer, bs);
		break;

	default:
		sclog_message(&sj_log, SCLOG_ERROR, "unknown jet function sent by peer!");
		break;
	}
}

static void request_id_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	struct jet_peer *peer = (struct jet_peer *)handler_context;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "connection closed by peer!");
		peer->shutdown_peer(peer);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "reading message id failed!");
		peer->shutdown_peer(peer);
		return;
	}

	uint32_t request_id;
	memcpy(&request_id, cio_read_buffer_get_read_ptr(buffer), sizeof(request_id));
	cio_read_buffer_consume(buffer, sizeof(request_id));

	peer->request_id = request_id;

	err = cio_buffered_stream_read_at_least(bs, buffer, 1, jet_function_read, peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading jet function");
		peer->shutdown_peer(peer);
	}
}

void handle_request(struct jet_peer *peer)
{
	enum cio_error err = cio_buffered_stream_read_at_least(&peer->bs, &peer->rb, 4, request_id_read, peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start reading request id!");
		peer->shutdown_peer(peer);
	}
}
