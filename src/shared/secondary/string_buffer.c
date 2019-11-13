/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_log.h>
#include <rte_branch_prediction.h>

#include "string_buffer.h"

#define RTE_LOGTYPE_SPP_STRING_BUFF RTE_LOGTYPE_USER1

/* get message buffer capacity */
static inline size_t
strbuf_get_capacity(const char *strbuf)
{
	return *((const size_t *)(strbuf - sizeof(size_t)));
}

/* re-allocate message buffer */
static inline char*
strbuf_reallocate(char *strbuf, size_t required_len)
{
	size_t new_cap = strbuf_get_capacity(strbuf) * 2;
	char *new_strbuf = NULL;

	while (unlikely(new_cap <= required_len))
		new_cap *= 2;

	new_strbuf = spp_strbuf_allocate(new_cap);
	if (unlikely(new_strbuf == NULL))
		return NULL;

	strcpy(new_strbuf, strbuf);
	spp_strbuf_free(strbuf);

	return new_strbuf;
}

/* allocate message buffer */
char*
spp_strbuf_allocate(size_t capacity)
{
	char *buf = (char *)malloc(capacity + sizeof(size_t));
	if (unlikely(buf == NULL))
		return NULL;

	memset(buf, 0x00, capacity + sizeof(size_t));
	*((size_t *)buf) = capacity;
	RTE_LOG(DEBUG, SPP_STRING_BUFF,
			";alloc  ; addr=%p; size=%lu; str= ; len=0;\n",
			buf + sizeof(size_t), capacity);

	return buf + sizeof(size_t);
}

/* free message buffer */
void
spp_strbuf_free(char *strbuf)
{
	if (likely(strbuf != NULL)) {
		RTE_LOG(DEBUG, SPP_STRING_BUFF,
				";free   ; addr=%p; size=%lu; "
				"str=%s; len=%lu;\n",
				strbuf, strbuf_get_capacity(strbuf),
				strbuf, strlen(strbuf));
		free(strbuf - sizeof(size_t));
	}
}

/* append message to buffer */
char*
spp_strbuf_append(char *strbuf, const char *append, size_t append_len)
{
	size_t cap = strbuf_get_capacity(strbuf);
	size_t len = strlen(strbuf);
	char *new_strbuf = strbuf;

	if (unlikely(len + append_len >= cap)) {
		new_strbuf = strbuf_reallocate(strbuf, len + append_len);
		if (unlikely(new_strbuf == NULL))
			return NULL;
	}

	memcpy(new_strbuf + len, append, append_len);
	*(new_strbuf + len + append_len) = '\0';
	RTE_LOG(DEBUG, SPP_STRING_BUFF,
			";append ; addr=%p; size=%lu; str=%s; len=%lu;\n",
			new_strbuf, strbuf_get_capacity(new_strbuf),
			new_strbuf, strlen(new_strbuf));

	return new_strbuf;
}

/* remove message from front */
char*
spp_strbuf_remove_front(char *strbuf, size_t remove_len)
{
	size_t len = strlen(strbuf);
	size_t new_len = len - remove_len;

	if (likely(new_len == 0)) {
		*strbuf = '\0';
		return strbuf;
	}

	return memmove(strbuf, strbuf + remove_len, new_len + 1);
}
