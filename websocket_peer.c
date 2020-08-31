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

#include "cio/error_code.h"
#include "cio/eventloop.h"
#include "cio/http_client.h"
#include "cio/http_server.h"
#include "cio/socket_address.h"
#include "cio/util.h"

#include "sclog/sclog.h"

#include "sj_log.h"
#include "websocket_peer.h"

// TODO(gatzka): Make constants configuratble via cmake.
enum { HTTPSERVER_LISTEN_PORT = 8080 };
enum { READ_BUFFER_SIZE = 2000 };
static const uint64_t HEADER_READ_TIMEOUT = UINT64_C(5) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t BODY_READ_TIMEOUT = UINT64_C(5) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t RESPONSE_TIMEOUT = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const uint64_t CLOSE_TIMEOUT_NS = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);

static void serve_error(struct cio_http_server *s, const char *reason)
{
	//TODO(gatzka): close all peers/websocket_peers?
	sclog_message(&sj_log, SCLOG_ERROR, "http server error %s!", reason);
	cio_http_server_shutdown(s, NULL);
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

enum cio_error prepare_websocket_peer_connection(struct cio_inet_address *address, struct cio_eventloop *loop)
{
	(void)loop;

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
	if (err != CIO_SUCCESS) {
		sclog_message(&sj_log, SCLOG_ERROR, "Could not init server socket address for websocket!");
		return err;
	}

	return CIO_SUCCESS;
}
