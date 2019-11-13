/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include "string_buffer.h"
#include "json_helper.h"

#define RTE_LOGTYPE_WK_JSON_HELPER RTE_LOGTYPE_USER1

/* Add a comma to given JSON string, or nothing if the end of the msg. */
int
append_json_comma(char **output)
{
	*output = spp_strbuf_append(*output, ", ", strlen(", "));
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER, "Failed to add comma.\n");
		return SPPWK_RET_NG;
	}

	return SPPWK_RET_OK;
}

/* Add a uint value to given JSON string. */
int
append_json_uint_value(char **output, const char *name, unsigned int value)
{
	int len = strlen(*output);

	*output = spp_strbuf_append(*output, "",
			strlen(name) + JSON_APPEND_LEN*2);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's numeric format failed to add. "
				"(name = %s, uint = %u)\n", name, value);
		return SPPWK_RET_NG;
	}

	sprintf(&(*output)[len], JSON_APPEND_VALUE("%u"),
			JSON_APPEND_COMMA(len), name, value);
	return SPPWK_RET_OK;
}

/* Add an int value to given JSON string. */
int
append_json_int_value(char **output, const char *name, int value)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + JSON_APPEND_LEN*2);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's numeric format failed to add. "
				"(name = %s, int = %d)\n", name, value);
		return SPPWK_RET_NG;
	}

	sprintf(&(*output)[len], JSON_APPEND_VALUE("%d"),
			JSON_APPEND_COMMA(len), name, value);
	return SPPWK_RET_OK;
}

/* Add a string value to given JSON string. */
int
append_json_str_value(char **output, const char *name, const char *val)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + strlen(val) + JSON_APPEND_LEN);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's string format failed to add. "
				"(name = %s, val= %s)\n", name, val);
		return SPPWK_RET_NG;
	}

	sprintf(&(*output)[len], JSON_APPEND_VALUE("\"%s\""),
			JSON_APPEND_COMMA(len), name, val);
	return SPPWK_RET_OK;
}

/**
 * Add an entry of array by surrounding given value with '[' and ']' to make
 * it an array entry. The added entry `"key": [ val ]"` is defined as macro
 * `JSON_APPEND_ARRAY`.
 */
int
append_json_array_brackets(char **output, const char *name, const char *val)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + strlen(val) + JSON_APPEND_LEN);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's square bracket failed to add. "
				"(name = %s, val= %s)\n", name, val);
		return SPPWK_RET_NG;
	}

	sprintf(&(*output)[len], JSON_APPEND_ARRAY,
			JSON_APPEND_COMMA(len), name, val);
	return SPPWK_RET_OK;
}

/**
 * Add an entry of hash by surrounding given value with '{' and '}' to make
 * it a hash entry. The added entry `"key": { val }"` is defined as macro
 * `JSON_APPEND_BLOCK`.
 *
 * This function is also used to make a block without key `{ val }` if given
 * key is `""`.
 */
int
append_json_block_brackets(char **output, const char *name, const char *val)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + strlen(val) + JSON_APPEND_LEN);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's curly bracket failed to add. "
				"(name = %s, val= %s)\n", name, val);
		return SPPWK_RET_NG;
	}

	if (name[0] == '\0')
		sprintf(&(*output)[len], JSON_APPEND_BLOCK_NONAME,
				JSON_APPEND_COMMA(len), name, val);
	else
		sprintf(&(*output)[len], JSON_APPEND_BLOCK,
				JSON_APPEND_COMMA(len), name, val);
	return SPPWK_RET_OK;
}
