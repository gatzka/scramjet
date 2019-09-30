/*
 * SPDX-License-Identifier: MIT
 *
 *The MIT License (MIT)
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

#include "cio_eventloop.h"
#include "cio_server_socket.h"

static const uint64_t close_timeout_ns = UINT64_C(1) * UINT64_C(1000) * UINT64_C(1000) * UINT64_C(1000);
static const unsigned int SERVERSOCKET_BACKLOG = 5;
static const uint16_t SERVERSOCKET_LISTEN_PORT = 12345;

static struct cio_eventloop loop;

static void sighandler(int signum)
{
	(void)signum;
	cio_eventloop_cancel(&loop);
}

static struct cio_socket *alloc_jet_client(void)
{
	//struct echo_client *client = malloc(sizeof(*client));
	//if (cio_unlikely(client == NULL)) {
	//	return NULL;
	//}

	//return &client->socket;
	return NULL;
}

static void free_jet_client(struct cio_socket *socket)
{
	(void)socket;
	//struct echo_client *client = cio_container_of(socket, struct echo_client, socket);
	//free(client);
}

static void handle_accept(struct cio_server_socket *ss, void *handler_context, enum cio_error err, struct cio_socket *socket)
{
	(void)handler_context;
	(void)ss;
	(void)err;
	(void)socket;
}

int main(void)
{
	int ret = EXIT_SUCCESS;

	enum cio_error err = cio_eventloop_init(&loop);
	if (err != CIO_SUCCESS) {
		return EXIT_FAILURE;
	}

	if (signal(SIGTERM, sighandler) == SIG_ERR) {
		return EXIT_FAILURE;
	}

	if (signal(SIGINT, sighandler) == SIG_ERR) {
		signal(SIGTERM, SIG_DFL);
		return EXIT_FAILURE;
	}

	struct cio_server_socket ss;
	err = cio_server_socket_init(&ss, &loop, SERVERSOCKET_BACKLOG, alloc_jet_client, free_jet_client, close_timeout_ns, NULL);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		goto destroy_loop;
	}

	err = cio_server_socket_set_reuse_address(&ss, true);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		goto close_socket;
	}

	err = cio_server_socket_bind(&ss, NULL, SERVERSOCKET_LISTEN_PORT);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		goto close_socket;
	}

	err = cio_server_socket_accept(&ss, handle_accept, NULL);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		goto close_socket;
	}

	err = cio_eventloop_run(&loop);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
	}

close_socket:
	cio_server_socket_close(&ss);
destroy_loop:
	cio_eventloop_destroy(&loop);
	return ret;


	return ret;
}
