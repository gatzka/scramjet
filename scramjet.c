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

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>

#include "cio/eventloop.h"
#include "cio/server_socket.h"
#include "cio/socket_address.h"
#include "cio/unix_address.h"

#include "sclog/sclog.h"
#include "sclog/stderr_sink.h"

#include "sj_log.h"
#include "socket_peer.h"

enum { SERVERSOCKET_LISTEN_PORT = 12345 };

static struct cio_eventloop loop;

static void sighandler(int signum)
{
	(void)signum;
	cio_eventloop_cancel(&loop);
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
		goto close_log;
	}

	if (signal(SIGTERM, sighandler) == SIG_ERR) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR,
		              "Could not install signal handler for SIGTERM!");
		goto destroy_loop;
	}

	if (signal(SIGINT, sighandler) == SIG_ERR) {
		signal(SIGTERM, SIG_DFL);
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR,
		              "Could not install signal handler for SIGINT!");
		goto destroy_loop;
	}

	struct cio_socket_address ipv4_endpoint;
	err = cio_init_inet_socket_address(&ipv4_endpoint, cio_get_inet_address_any4(),
	                                   SERVERSOCKET_LISTEN_PORT);
	if (err != CIO_SUCCESS) {
		sclog_message(&sj_log, SCLOG_ERROR,
		              "Could not init IPv4 listen socket address!");
		goto destroy_loop;
	}

	struct cio_server_socket ipv4_ss;
	err = prepare_socket_peer_connection(&ipv4_ss, &ipv4_endpoint, &loop);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not setup IPv4 server socket!");
		goto destroy_loop;
	}

	struct cio_socket_address ipv6_endpoint;
	err = cio_init_inet_socket_address(&ipv6_endpoint, cio_get_inet_address_any6(),
	                                   SERVERSOCKET_LISTEN_PORT);
	if (err != CIO_SUCCESS) {
		sclog_message(&sj_log, SCLOG_ERROR,
		              "Could not init IPv6 listen socket address!");
		goto close_ipv4_ss;
	}

	struct cio_server_socket ipv6_ss;
	err = prepare_socket_peer_connection(&ipv6_ss, &ipv6_endpoint, &loop);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not setup IPv6 server socket!");
		goto close_ipv4_ss;
	}

	struct cio_socket_address uds_endpoint;
	const char path[] = {"\0/tmp/scramjet"};
	err = cio_init_uds_socket_address(&uds_endpoint, path);
	if (cio_unlikely(err != CIO_SUCCESS)) {
		sclog_message(&sj_log, SCLOG_ERROR,
		              "Could not init UDS listen socket address!");
		goto close_ipv6_ss;
	}

	struct cio_server_socket uds_ss;
	err = prepare_socket_peer_connection(&uds_ss, &uds_endpoint, &loop);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not setup UDS server socket!");
		goto close_ipv4_ss;
	}

	sclog_message(&sj_log, SCLOG_INFO, "Starting eventloop!");

	err = cio_eventloop_run(&loop);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not run eventloop!");
	}

	// TODO(gatzka): implement that
	//destroy_all_peers();

close_ipv6_ss:
	cio_server_socket_close(&ipv6_ss);

close_ipv4_ss:
	cio_server_socket_close(&ipv4_ss);

destroy_loop:
	cio_eventloop_destroy(&loop);

close_log:
	sclog_close(&sj_log);

	return ret;
}
