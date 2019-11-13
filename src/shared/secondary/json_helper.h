/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPPWK_JSON_HELPER_H_
#define _SPPWK_JSON_HELPER_H_

#include <string.h>
#include <rte_branch_prediction.h>
#include <rte_log.h>
#include "return_codes.h"
#include "string_buffer.h"

/* TODO(yasufum) revise name considering the usage. */
#define JSON_APPEND_LEN 16

/* Add comma at the end of JSON statement, or do nothing. */
#define JSON_APPEND_COMMA(flg)    ((flg)?", ":"")

/**
 * Defines of macro for JSON formatter. There are two ro three strings, the
 * first one is placehodler for JSON msg, second one is key and third one is
 * its value. Key and value are appended at the end of the placeholder.
 */
#define JSON_APPEND_VALUE(format) "%s\"%s\": "format
#define JSON_APPEND_ARRAY         "%s\"%s\": [ %s ]"
#define JSON_APPEND_BLOCK_NONAME  "%s%s{ %s }"
#define JSON_APPEND_BLOCK         "%s\"%s\": { %s }"

/**
 * Add a comma to given JSON string, or nothing if the end of the statement.
 *
 * @param[in,out] output Placeholder of JSON msg.
 */
int append_json_comma(char **output);

/**
 * Add a uint value to given JSON string.
 *
 * @param[in,out] output Placeholder of JSON msg.
 * @param[in] name Name as a key.
 * @param[in] val Uint value of the key.
 * @retval SPPWK_RET_OK if succeeded.
 * @retval SPPWK_RET_NG if failed.
 */
int append_json_uint_value(char **output, const char *name, unsigned int val);

/**
 * Add an int value to given JSON string.
 *
 * @param[in,out] output Placeholder of JSON msg.
 * @param[in] name Name as a key.
 * @param[in] val Int value of the key.
 * @retval SPPWK_RET_OK if succeeded.
 * @retval SPPWK_RET_NG if failed.
 */
int append_json_int_value(char **output, const char *name, int val);

/**
 * Add a string value to given JSON string.
 *
 * @param[in,out] output Placeholder of JSON msg.
 * @param[in] name Name as a key.
 * @param[in] val String value of the key.
 * @retval SPPWK_RET_OK if succeeded.
 * @retval SPPWK_RET_NG if failed.
 */
int append_json_str_value(char **output, const char *name, const char *val);

/**
 * Add an entry of array by surrounding given value with '[' and ']' to make
 * it an array entry. The added entry `"key": [ val ]"` is defined as macro
 * `JSON_APPEND_ARRAY`.
 *
 * @param[in,out] output Placeholder of JSON msg.
 * @param[in] name Name as a key.
 * @param[in] val String value of the key.
 * @retval SPPWK_RET_OK if succeeded.
 * @retval SPPWK_RET_NG if failed.
 */
int append_json_array_brackets(char **output, const char *name,
		const char *val);

/**
 * Add an entry of hash by surrounding given value with '{' and '}' to make
 * it a hash entry. The added entry `"key": { val }"` is defined as macro
 * `JSON_APPEND_BLOCK`.
 *
 * This function is also used to make a block without key `{ val }` if given
 * key is `""`.
 *
 * @param[in,out] output Placeholder of JSON msg.
 * @param[in] name Name as a key.
 * @param[in] val String value of the key.
 * @retval SPPWK_RET_OK if succeeded.
 * @retval SPPWK_RET_NG if failed.
 */
int append_json_block_brackets(char **output, const char *name,
		const char *val);

#endif
