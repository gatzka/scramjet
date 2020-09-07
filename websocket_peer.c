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
#include <stdlib.h>

#include "cio/compiler.h"
#include "cio/endian.h"
#include "cio/error_code.h"
#include "cio/eventloop.h"
#include "cio/http_client.h"
#include "cio/http_location.h"
#include "cio/http_server.h"
#include "cio/socket_address.h"
#include "cio/util.h"

#include "sclog/sclog.h"

#include "protocol_version.h"
#include "sj_log.h"
#include "websocket_peer.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

// TODO(gatzka): Make constants configuratble via cmake.
enum { HTTPSERVER_LISTEN_PORT = 8080 };
enum { READ_BUFFER_SIZE = 2000 };
static const uint64_t HEADER_READ_TIMEOUT = UINT64_C(5) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t BODY_READ_TIMEOUT = UINT64_C(5) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t RESPONSE_TIMEOUT = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t CLOSE_TIMEOUT_NS = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);

static void serve_error(struct cio_http_server *s, const char *reason)
{
	(void)s;
	//TODO(gatzka): close all peers/websocket_peers?
	sclog_message(&sj_log, SCLOG_ERROR, "http server error %s!", reason);
}

static struct cio_socket *alloc_http_client(void)
{
	struct cio_http_client *client = malloc(sizeof(*client) + READ_BUFFER_SIZE);
	if (cio_unlikely(client == NULL)) {
		return NULL;
	}

	client->buffer_size = READ_BUFFER_SIZE;
	return &client->socket;
}

static void free_http_client(struct cio_socket *socket)
{
	struct cio_http_client *client = cio_container_of(socket, struct cio_http_client, socket);
	free(client);
}

static void on_connect(struct cio_websocket *ws)
{
	struct cio_websocket_location_handler *handler = cio_container_of(ws, struct cio_websocket_location_handler, websocket);
	struct websocket_peer *ws_peer = cio_container_of(handler, struct websocket_peer, ws_handler);
	start_peer(&ws_peer->peer);
}

static void free_websocket_handler(struct cio_websocket_location_handler *wslh)
{
	struct websocket_peer *ws_peer = cio_container_of(wslh, struct websocket_peer, ws_handler);
	free(ws_peer);
}

static void shutdown_websocket_peer(struct peer *peer)
{
	(void)peer;
	// Do nothing here. The websocket layer itself closes the websocket connection.
}

static void sent_complete(struct cio_websocket *ws, void *handler_context, enum cio_error err)
{
	(void)ws;
	struct peer *peer = (struct peer *)handler_context;

	if (err != CIO_SUCCESS) {
		struct websocket_peer *ws_peer = cio_container_of(peer, struct websocket_peer, peer);
		sclog_message(&sj_log, SCLOG_ERROR, "Sending message over websocket failed!");
		close_peer(&ws_peer->peer);
	}

	peer->sent_handler(peer);
}

static void send_message_websocket_peer(struct peer *peer, peer_message_sent_t handler)
{
	peer->sent_handler = handler;

	struct websocket_peer *ws_peer = cio_container_of(peer, struct websocket_peer, peer);
	ws_peer->write_message_length = (uint32_t)peer->wbh.data.head.total_length;
	cio_write_buffer_const_element_init(&ws_peer->wb, &ws_peer->write_message_length, sizeof(ws_peer->write_message_length));
	cio_write_buffer_queue_head(&peer->wbh, &ws_peer->wb);

	enum cio_error err = cio_websocket_write_message_first_chunk(&ws_peer->ws_handler.websocket, peer->wbh.data.head.total_length, &peer->wbh, true, true, sent_complete, peer);
	if (err != CIO_SUCCESS) {
		sclog_message(&sj_log, SCLOG_ERROR, "Start sending message over websocket failed!");
		close_peer(&ws_peer->peer);
	}
}

static void message_read(struct cio_websocket *ws, void *handler_context, enum cio_error err, size_t frame_length, uint8_t *data, size_t chunk_length, bool last_chunk, bool last_frame, bool is_binary)
{
	(void)ws;
	(void)chunk_length;

	struct websocket_peer *ws_peer = (struct websocket_peer *)handler_context;

	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Receiving message over websocket failed!");
		close_peer(&ws_peer->peer);
		return;
	}

	if (!last_chunk || !is_binary || !last_frame) {
		sclog_message(&sj_log, SCLOG_ERROR, "Receiving websocket message in wrong format!");
		close_peer(&ws_peer->peer);
		return;
	}

	uint32_t message_length;
	memcpy(&message_length, data, sizeof(message_length));
	message_length = cio_le32toh(message_length);

	if (cio_unlikely(frame_length != message_length + sizeof(message_length))) {
		sclog_message(&sj_log, SCLOG_ERROR, "Websocket frame length does not correspond with jet message length!");
		close_peer(&ws_peer->peer);
		return;
	}

	uint8_t *message = data + sizeof(message_length);
	ws_peer->peer.recvd_hander(&ws_peer->peer, message, message_length);
}

static void receive_message_websocket_peer(struct peer *peer, peer_message_received_t handler)
{
	peer->recvd_hander = handler;

	struct websocket_peer *ws_peer =
	    cio_container_of(peer, struct websocket_peer, peer);

	enum cio_error err = cio_websocket_read_message(&ws_peer->ws_handler.websocket, message_read, ws_peer);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Start receiving message over websocket failed!");
		close_peer(&ws_peer->peer);
	}
}

static struct cio_http_location_handler *alloc_websocket_handler(const void *config)
{
	(void)config;
	struct websocket_peer *ws_peer = malloc(sizeof(*ws_peer));
	if (cio_unlikely(ws_peer == NULL)) {
		return NULL;
	}

	static const char *subprotocols[] = {"jet"};
	enum cio_error err = cio_websocket_location_handler_init(&ws_peer->ws_handler, subprotocols, ARRAY_SIZE(subprotocols), on_connect, free_websocket_handler);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		free(ws_peer);
		return NULL;
	}

	ws_peer->peer.shutdown_peer = shutdown_websocket_peer;
	ws_peer->peer.send_message = send_message_websocket_peer;
	ws_peer->peer.receive_message = receive_message_websocket_peer;

	return &ws_peer->ws_handler.http_location;
}

enum cio_error prepare_websocket_peer_connection(struct cio_http_server *server, const struct cio_inet_address *address, struct cio_http_location *target_jet, struct cio_eventloop *loop)
{
	struct cio_http_server_configuration config = {
	    .on_error = serve_error,
	    .read_header_timeout_ns = HEADER_READ_TIMEOUT,
	    .read_body_timeout_ns = BODY_READ_TIMEOUT,
	    .response_timeout_ns = RESPONSE_TIMEOUT,
	    .close_timeout_ns = CLOSE_TIMEOUT_NS,
	    .use_tcp_fastopen = false,
	    .alloc_client = alloc_http_client,
	    .free_client = free_http_client};

	enum cio_error err = cio_init_inet_socket_address(&config.endpoint, address, HTTPSERVER_LISTEN_PORT);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not init server socket address for websocket!");
		return err;
	}

	err = cio_http_server_init(server, loop, &config);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not init http server!");
		return err;
	}

	err = cio_http_location_init(target_jet, "/api/scramjet/1.0/", NULL, alloc_websocket_handler);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not init jet service location!");
		goto shutdown_server;
	}

	err = cio_http_server_register_location(server, target_jet);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not register jet service location!");
		goto shutdown_server;
	}

	err = cio_http_server_serve(server);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not start serving http!");
		goto shutdown_server;
	}

	return CIO_SUCCESS;

shutdown_server:
	cio_http_server_shutdown(server, NULL);
	return err;
}
