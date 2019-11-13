/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef _STRING_BUFFER_H_
#define _STRING_BUFFER_H_

#include <stdlib.h>

/**
 * @file
 * SPP String buffer management
 *
 * Management features of string buffer which is used for communicating
 * between spp_vf and controller.
 */

/**
 * allocate string buffer from heap memory.
 *
 * @attention allocated memory must free by spp_strbuf_free function.
 *
 * @param capacity
 *  initial buffer size (include null char).
 *
 * @retval not-NULL pointer to the allocated memory.
 * @retval NULL     error.
 */
char *spp_strbuf_allocate(size_t capacity);

/**
 * free string buffer.
 *
 * @param strbuf
 *  spp_strbuf_allocate/spp_strbuf_append return value.
 */
void spp_strbuf_free(char *strbuf);

/**
 * append string to buffer.
 *
 * @param strbuf
 *  destination string buffer.
 *  spp_strbuf_allocate/spp_strbuf_append return value.
 * @param append
 *  string to append. normal c-string.
 * @param append_len
 *  length of append string.
 *
 * @return if "strbuf" has enough space to append, returns "strbuf"
 *         else returns a new pointer to the allocated memory.
 */
char *spp_strbuf_append(char *strbuf, const char *append, size_t append_len);

/**
 * remove string from front.
 *
 * @param strbuf
 *  target string buffer.
 *  spp_strbuf_allocate/spp_strbuf_append return value.
 * @param remove_len
 *  length of remove.
 *
 * @return
 *  The pointer to removed string.
 */
char *spp_strbuf_remove_front(char *strbuf, size_t remove_len);

#endif /* _STRING_BUFFER_H_ */
