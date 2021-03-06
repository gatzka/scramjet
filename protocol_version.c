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

#include "cio/compiler.h"
#include "cio/error_code.h"

#include "messages.h"
#include "peer.h"
#include "protocol_version.h"
#include "sj_log.h"

static const uint32_t PROTOCOL_VERSION_MAJOR = UINT32_C(1);
static const uint32_t PROTOCOL_VERSION_MINOR = UINT32_C(0);
static const uint32_t PROTOCOL_VERSION_PATCH = UINT32_C(0);

static const uint8_t PROTOCOL_VERSION[13] = {
    (uint8_t)MESSAGE_API_VERSION,
    (uint8_t)(PROTOCOL_VERSION_MAJOR & 0xFFU),
    (uint8_t)((PROTOCOL_VERSION_MAJOR >> 8U) & 0xFFU),
    (uint8_t)((PROTOCOL_VERSION_MAJOR >> 16U) & 0xFFU),
    (uint8_t)((PROTOCOL_VERSION_MAJOR >> 24U) & 0xFFU),

    (uint8_t)(PROTOCOL_VERSION_MINOR & 0xFFU),
    (uint8_t)((PROTOCOL_VERSION_MINOR >> 8U) & 0xFFU),
    (uint8_t)((PROTOCOL_VERSION_MINOR >> 16U) & 0xFFU),
    (uint8_t)((PROTOCOL_VERSION_MINOR >> 24U) & 0xFFU),

    (uint8_t)(PROTOCOL_VERSION_PATCH & 0xFFU),
    (uint8_t)((PROTOCOL_VERSION_PATCH >> 8U) & 0xFFU),
    (uint8_t)((PROTOCOL_VERSION_PATCH >> 16U) & 0xFFU),
    (uint8_t)((PROTOCOL_VERSION_PATCH >> 24U) & 0xFFU),
};

void send_protocol_version(struct peer *peer, peer_message_sent_t sent_handler)
{
	cio_write_buffer_head_init(&peer->wbh);
	cio_write_buffer_const_element_init(&peer->wb, PROTOCOL_VERSION, sizeof(PROTOCOL_VERSION));
	cio_write_buffer_queue_tail(&peer->wbh, &peer->wb);

	peer->send_message(peer, sent_handler);
}
