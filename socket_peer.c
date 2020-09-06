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

#include <stdint.h>
#include <stdlib.h>

#include "cio/buffered_stream.h"
#include "cio/endian.h"
#include "cio/error_code.h"
#include "cio/eventloop.h"
#include "cio/server_socket.h"
#include "cio/socket_address.h"
#include "cio/util.h"

#include "sclog/sclog.h"

#include "messages.h"
#include "protocol_version.h"
#include "sj_log.h"
#include "socket_peer.h"

//TODO(gatzka): make these constants configurable via cmake
static const uint64_t CLOSE_TIMEOUT_NS =
    UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
enum { SERVERSOCKET_BACKLOG = 5 };
static const unsigned int KEEP_ALIVE_IDLE_S = 12;
static const unsigned int KEEP_ALIVE_INTERVAL_S = 3;
static const unsigned int KEEP_ALIVE_CNT = 2;

static void message_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)bs;
	(void)num_bytes;

	struct socket_peer *peer = (struct socket_peer *)handler_context;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "Connection closed by socket peer!");
		close_peer(&peer->peer);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Receiving message over socket failed!");
		close_peer(&peer->peer);
		return;
	}

	uint8_t *message = cio_read_buffer_get_read_ptr(buffer);
	peer->peer.recvd_hander(&peer->peer, message, peer->message_length);
	cio_read_buffer_consume(buffer, peer->message_length);
}

static void message_length_read(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err, struct cio_read_buffer *buffer, size_t num_bytes)
{
	(void)num_bytes;

	struct socket_peer *peer = (struct socket_peer *)handler_context;

	if (cio_unlikely(err == CIO_EOF)) {
		sclog_message(&sj_log, SCLOG_INFO, "Connection closed by socket peer!");
		close_peer(&peer->peer);
		return;
	}

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Receiving message length over socket failed!");
		close_peer(&peer->peer);
		return;
	}

	uint32_t message_length;
	memcpy(&message_length, cio_read_buffer_get_read_ptr(buffer), sizeof(message_length));
	cio_read_buffer_consume(buffer, sizeof(message_length));
	message_length = cio_le32toh(message_length);
	peer->message_length = message_length;

	err = cio_buffered_stream_read_at_least(bs, &peer->rb, message_length, message_read, peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Start receiving message over socket failed!");
		close_peer(&peer->peer);
	}
}

static void shutdown_socket_peer(struct peer *jet_peer)
{
	struct socket_peer *peer =
	    cio_container_of(jet_peer, struct socket_peer, peer);
	cio_buffered_stream_close(&peer->bs);
}

static void sent_comnplete(struct cio_buffered_stream *bs, void *handler_context, enum cio_error err)
{
	(void)bs;
	struct peer *peer = (struct peer *)handler_context;

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Sending message over socket failed!");
		close_peer(peer);
		return;
	}

	peer->sent_handler(peer);
}

static void send_message_socket_peer(struct peer *jet_peer, peer_message_sent_t handler)
{
	jet_peer->sent_handler = handler;

	struct socket_peer *peer =
	    cio_container_of(jet_peer, struct socket_peer, peer);
	peer->write_message_length = (uint32_t)jet_peer->wbh.data.head.total_length;
	cio_write_buffer_const_element_init(&peer->wb, &peer->write_message_length, sizeof(peer->write_message_length));
	cio_write_buffer_queue_head(&jet_peer->wbh, &peer->wb);
	enum cio_error err =  cio_buffered_stream_write(&peer->bs, &jet_peer->wbh, sent_comnplete, jet_peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Start sending message over socket failed!");
		close_peer(&peer->peer);
	}
}

static void receive_message_socket_peer(struct peer *jet_peer, peer_message_received_t handler)
{
	jet_peer->recvd_hander = handler;

	struct socket_peer *peer =
	    cio_container_of(jet_peer, struct socket_peer, peer);

	enum cio_error err = cio_buffered_stream_read_at_least(&peer->bs, &peer->rb, 4, message_length_read, peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Start receiving message length over socket failed!");
		close_peer(&peer->peer);
	}
}

static struct cio_socket *alloc_socket_jet_peer(void)
{
	struct socket_peer *peer = malloc(sizeof(*peer));
	if (cio_unlikely(peer == NULL)) {
		return NULL;
	}

	peer->peer.shutdown_peer = shutdown_socket_peer;
	peer->peer.send_message = send_message_socket_peer;
	peer->peer.receive_message = receive_message_socket_peer;

	return &peer->socket;
}

static void free_socket_jet_peer(struct cio_socket *socket)
{
	struct socket_peer *peer =
	    cio_container_of(socket, struct socket_peer, socket);
	free(peer);
}

static enum cio_error init_socket_peer(struct socket_peer *peer)
{
	enum cio_error err = cio_read_buffer_init(&peer->rb, peer->buffer, sizeof(peer->buffer));
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Failed to initialize read buffer!");
		goto error;
	}

	struct cio_io_stream *stream = cio_socket_get_io_stream(&peer->socket);

	struct cio_buffered_stream *bs = &peer->bs;
	err = cio_buffered_stream_init(bs, stream);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Failed to initialize buffered stream!");
		goto error;
	}

	return CIO_SUCCESS;

error:
	cio_socket_close(&peer->socket);
	return err;
}

static void handle_accept(struct cio_server_socket *ss, void *handler_context,
                          enum cio_error err, struct cio_socket *socket)
{
	(void)handler_context;

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Error in handle_accept!");
		cio_server_socket_close(ss);
		cio_eventloop_cancel(ss->impl.loop);
	}

	enum cio_address_family family = cio_socket_get_address_family(socket);
	if ((family == CIO_ADDRESS_FAMILY_INET4) || (family == CIO_ADDRESS_FAMILY_INET6)) {
		err = cio_socket_set_tcp_no_delay(socket, true);
		if (cio_unlikely(err != CIO_SUCCESS)) {
			sclog_message(&sj_log, SCLOG_ERROR, "Failed to set tcp nodelay!");
			cio_socket_close(socket);
			return;
		}

		err = cio_socket_set_keep_alive(socket, true, KEEP_ALIVE_IDLE_S, KEEP_ALIVE_INTERVAL_S, KEEP_ALIVE_CNT);
		if (cio_unlikely(err != CIO_SUCCESS)) {
			sclog_message(&sj_log, SCLOG_ERROR, "Failed to set socket keep alive!");
			cio_socket_close(socket);
			return;
		}
	}

	struct socket_peer *peer =
	    cio_container_of(socket, struct socket_peer, socket);

	err = init_socket_peer(peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		cio_socket_close(socket);
		return;
	}

	start_peer(&peer->peer);
}

enum cio_error prepare_socket_peer_connection(struct cio_server_socket *ss, struct cio_socket_address *endpoint, struct cio_eventloop *loop)
{
	enum cio_error err = cio_server_socket_init(ss, loop, SERVERSOCKET_BACKLOG,
	                                            cio_socket_address_get_family(endpoint),
	                                            alloc_socket_jet_peer, free_socket_jet_peer,
	                                            CLOSE_TIMEOUT_NS, NULL);
	if (err != CIO_SUCCESS) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not init server socket!");
		return err;
	}

	enum cio_address_family family = cio_socket_address_get_family(endpoint);
	if ((family == CIO_ADDRESS_FAMILY_INET4) || (family == CIO_ADDRESS_FAMILY_INET6)) {
		err = cio_server_socket_set_tcp_fast_open(ss, true);
		if (cio_unlikely(err != CIO_SUCCESS)) {
			sclog_message(&sj_log, SCLOG_ERROR, "Could not set TCP FASTOPEN!");
			goto close_socket;
		}
	}

	err = cio_server_socket_set_reuse_address(ss, true);
	if (err != CIO_SUCCESS) {
		sclog_message(&sj_log, SCLOG_ERROR,
		              "Could not set reuse address socket option!");
		goto close_socket;
	}

	err = cio_server_socket_bind(ss, endpoint);
	if (err != CIO_SUCCESS) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not bind to socket endpoint!");
		goto close_socket;
	}

	err = cio_server_socket_accept(ss, handle_accept, NULL);
	if (err != CIO_SUCCESS) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not accept on server socket!");
		goto close_socket;
	}

	return CIO_SUCCESS;

close_socket:
	cio_server_socket_close(ss);

	return err;
}
