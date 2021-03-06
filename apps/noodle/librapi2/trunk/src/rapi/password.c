/* $Id: password.c 2355 2006-04-07 18:47:20Z voc $ */
#undef __STRICT_ANSI__
#define _GNU_SOURCE
#include "rapi_internal.h"
#include "rapi_api.h"
#include "rapi_buffer.h"
#include "rapi_context.h"
#include <stdlib.h>


BOOL _CeCheckPassword( /*{{{*/
		LPWSTR lpszPassword)
{
	RapiContext* context = rapi_context_current();
	BOOL return_value = 0;

	rapi_context_begin_command(context, 0x34);
	rapi_buffer_write_optional_string(context->send_buffer, lpszPassword);

	if ( !rapi_context_call(context) )
		return false;

	rapi_buffer_read_uint32(context->recv_buffer, &context->last_error);
	rapi_buffer_read_uint32(context->recv_buffer, &return_value);

	return return_value;
}/*}}}*/

