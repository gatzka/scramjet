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

#include "cio/buffered_stream.h"
#include "cio/eventloop.h"
#include "cio/socket.h"
#include "cio/util.h"

#include "hs_hash/hs_hash.h"

#include "sclog/sclog.h"
#include "sclog/stderr_sink.h"

#include "jet_peer.h"
#include "messages.h"
#include "protocol_version.h"
#include "sj_log.h"

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
		goto err;
	}

	if (signal(SIGTERM, sighandler) == SIG_ERR) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR,
		              "Could not install signal handler for SIGTERM!");
		goto err;
	}

	if (signal(SIGINT, sighandler) == SIG_ERR) {
		signal(SIGTERM, SIG_DFL);
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR,
		              "Could not install signal handler for SIGINT!");
		goto err;
	}

	sclog_message(&sj_log, SCLOG_INFO, "Starting eventloop!");

	err = cio_eventloop_run(&loop);
	if (err != CIO_SUCCESS) {
		ret = EXIT_FAILURE;
		sclog_message(&sj_log, SCLOG_ERROR, "Could not run eventloop!");
	}

destroy_loop:
	cio_eventloop_destroy(&loop);
err:
	sclog_close(&sj_log);

	return ret;
}
