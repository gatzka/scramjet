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

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>

#include "cio/cio_eventloop.h"
#include "cio/cio_server_socket.h"
#include "cio/cio_socket.h"
#include "cio/cio_socket_address.h"
#include "cio/cio_util.h"
#include "hs_hash.h"
#include "sclog.h"
#include "sclog_stderr_sink.h"
#include "sj_log.h"

static const uint64_t close_timeout_ns = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
enum { SERVERSOCKET_BACKLOG = 5 };
enum { SERVERSOCKET_LISTEN_PORT = 12345 };

struct jet_client {
	struct cio_socket socket;
};

static struct cio_eventloop loop;

static void sighandler(int signum)
{
	(void)signum;
	cio_eventloop_cancel(&loop);
}

static struct cio_socket *alloc_jet_client(void)
{
	struct jet_client *client = malloc(sizeof(*client));
	if (cio_unlikely(client == NULL)) {
		return NULL;
	}

	return &client->socket;
}

static void free_jet_client(struct cio_socket *socket)
{
	struct jet_client *client = cio_container_of(socket, struct jet_client, socket);
	free(client);
}

static void handle_accept(struct cio_server_socket *ss, void *handler_context, enum cio_error err, struct cio_socket *socket)
{
	(void)handler_context;
	(void)socket;

	if (err != CIO_SUCCESS) {
		sclog_message(&sj_log, SCLOG_ERROR, "Error in handle_accept!");
		cio_server_socket_close(ss);
		cio_eventloop_cancel(ss->impl.loop);
		return;
	}
}

int main(void)
{
	int ret = EXIT_SUCCESS;

	struct sclog_sink stderr_sink;
	if (sclog_stderr_sink_init(&stderr_sink) != 0) {
		return EXIT_FAILURE;
	}

	if (sclog_init(&sj_log, "scramjet", SCLOG_INFO, &stderr_sink) != 0) {
		return EXIT_FAILURE;
	}

	enum cio_error err = cio_eventloop_init(&loop);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not init eventloop!");
		goto err;
	}

	if (signal(SIGTERM, sighandler) == SIG_ERR) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not install signal handler for SIGTERM!");
		goto err;
	}

	if (signal(SIGINT, sighandler) == SIG_ERR) {
		signal(SIGTERM, SIG_DFL);
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not install signal handler for SIGINT!");
		goto err;
	}

	struct cio_socket_address endpoint;
	err = cio_init_inet_socket_address(&endpoint, cio_get_inet_address_any4(), SERVERSOCKET_LISTEN_PORT);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not init listen socket address!");
		goto err;
	}

	struct cio_server_socket ss;
	err = cio_server_socket_init(&ss, &loop, SERVERSOCKET_BACKLOG, cio_socket_address_get_family(&endpoint), alloc_jet_client, free_jet_client, close_timeout_ns, NULL);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not init server socket!");
		goto destroy_loop;
	}

	err = cio_server_socket_set_tcp_fast_open(&ss, true);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not set TCP NODELAY!");
		goto close_socket;
	}

	err = cio_server_socket_set_reuse_address(&ss, true);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not set reuse address socket option!");
		goto close_socket;
	}

	err = cio_server_socket_bind(&ss, &endpoint);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not bind to socket endpoint!");
		goto close_socket;
	}

	err = cio_server_socket_accept(&ss, handle_accept, NULL);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not accept on server socket!");
		goto close_socket;
	}

	sclog_message(&sj_log, SCLOG_INFO, "Starting eventloop!");

	err = cio_eventloop_run(&loop);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not run eventloop!");
	}

close_socket:
	cio_server_socket_close(&ss);
destroy_loop:
	cio_eventloop_destroy(&loop);
err:
	sclog_close(&sj_log);

	return ret;
}
