/* $Id: registry.c 2355 2006-04-07 18:47:20Z voc $ */
#include "rapi2_api.h"
#include "rapi_context.h"
#include <stdlib.h>

LONG _CeRegCreateKeyEx2(
		HKEY hKey,
		LPCWSTR lpszSubKey,
		DWORD Reserved,
		LPWSTR lpszClass,
		DWORD ulOptions,
		REGSAM samDesired,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		PHKEY phkResult,
		LPDWORD lpdwDisposition)
{
	RapiContext* context = rapi_context_current();
	LONG return_value = 0;
	HKEY result = 0;
	DWORD disposition = 0;

	rapi_context_begin_command(context, 0x31);
	rapi_buffer_write_uint32(context->send_buffer, hKey);
	rapi2_buffer_write_string(context->send_buffer, lpszSubKey);
	rapi2_buffer_write_string(context->send_buffer, lpszClass);

	if ( !rapi2_context_call(context) )
		return false;

	rapi_buffer_read_uint32(context->recv_buffer, &context->last_error);
	rapi_buffer_read_int32(context->recv_buffer, &return_value);

	if (ERROR_SUCCESS == return_value)
	{
		rapi_buffer_read_int32(context->recv_buffer, &result);
		rapi_buffer_read_uint32(context->recv_buffer, &disposition);

		if (phkResult)
			*phkResult = result;

		if (lpdwDisposition)
			*lpdwDisposition = disposition;
	}

	return return_value;
}

LONG _CeRegOpenKeyEx2(
		HKEY hKey,
		LPCWSTR lpszSubKey,
		DWORD ulOptions,
		REGSAM samDesired,
		PHKEY phkResult)
{
	RapiContext* context = rapi_context_current();
	LONG return_value = 0;

	rapi_context_begin_command(context, 0x2f);
	rapi_buffer_write_uint32(context->send_buffer, hKey);
	rapi2_buffer_write_string(context->send_buffer, lpszSubKey);

	if ( !rapi2_context_call(context) )
		return false;

	rapi_buffer_read_uint32(context->recv_buffer, &context->last_error);
	rapi_buffer_read_int32(context->recv_buffer, &return_value);

	if (ERROR_SUCCESS == return_value)
	{
		if (phkResult)
			rapi_buffer_read_int32(context->recv_buffer, phkResult);
	}

	return return_value;
}


LONG _CeRegQueryValueEx2(
		HKEY hKey,
		LPCWSTR lpValueName,
		LPDWORD lpReserved,
		LPDWORD lpType,
		LPBYTE lpData,
		LPDWORD lpcbData)
{
	RapiContext* context = rapi_context_current();
	LONG return_value = -1;
	rapi_context_begin_command(context, 0x37);
	rapi_buffer_write_uint32(context->send_buffer, hKey);
	rapi2_buffer_write_string(context->send_buffer, lpValueName);
    rapi_buffer_write_uint32(context->send_buffer, *lpcbData);

	if ( !rapi2_context_call(context) )
	{
		synce_trace("rapi2_context_call failed");
		return return_value;
	}

	if (!rapi_buffer_read_uint32(context->recv_buffer, &context->last_error))
	{
		synce_trace("rapi_buffer_read_uint32 failed");
		return return_value;
	}
	rapi_buffer_read_int32(context->recv_buffer, &return_value);

	if (ERROR_SUCCESS == return_value)
	{
        DWORD type = 0;

        rapi_buffer_read_uint32(context->recv_buffer, &type);
        rapi_buffer_read_uint32(context->recv_buffer, (uint32_t*)lpcbData);

        if (lpType)
            *lpType = type;

        if (lpData) {
            if (REG_DWORD == type) {
                rapi_buffer_read_uint32(context->recv_buffer, (uint32_t *) lpData);
            } else {
                rapi_buffer_read_data(context->recv_buffer, lpData, *lpcbData);
            }
        }
    }
#if 0
	else
	{
		synce_trace("CeRegQueryValueEx returning %i", return_value);
	}
#endif

	return return_value;
}


LONG _CeRegCloseKey2(
		HKEY hKey)
{
	RapiContext* context = rapi_context_current();
	LONG return_value = 0;

	rapi_context_begin_command(context, 0x32);
	rapi_buffer_write_uint32(context->send_buffer, hKey);

	if ( !rapi2_context_call(context) )
		return false;

	rapi_buffer_read_uint32(context->recv_buffer, &context->last_error);
	rapi_buffer_read_int32(context->recv_buffer, &return_value);

	return return_value;
}


LONG _CeRegDeleteKey2(
        HKEY hKey,
        LPCWSTR lpszSubKey)
{
    RapiContext* context = rapi_context_current();
    LONG return_value = 0;

    rapi_context_begin_command(context, 0x33);
    rapi_buffer_write_uint32(context->send_buffer, hKey);
    rapi2_buffer_write_string(context->send_buffer, lpszSubKey);

    if ( !rapi2_context_call(context) )
      return false;

    rapi_buffer_read_uint32(context->recv_buffer, &context->last_error);
    rapi_buffer_read_int32(context->recv_buffer, &return_value);

    return return_value;
}


LONG _CeRegDeleteValue2(
        HKEY hKey,
        LPCWSTR lpszValueName)
{
    RapiContext* context = rapi_context_current();
    LONG return_value = 0;

    rapi_context_begin_command(context, 0x35);
    rapi_buffer_write_uint32(context->send_buffer, hKey);
    rapi2_buffer_write_string(context->send_buffer, lpszValueName);

    if ( !rapi2_context_call(context) )
      return false;

    rapi_buffer_read_uint32(context->recv_buffer, &context->last_error);
    rapi_buffer_read_int32(context->recv_buffer, &return_value);

    return return_value;
}


/*
LONG _CeRegQueryInfoKey2(
		HKEY hKey,
		LPWSTR lpClass,
		LPDWORD lpcbClass,
		LPDWORD lpReserved,
		LPDWORD lpcSubKeys,
		LPDWORD lpcbMaxSubKeyLen,
		LPDWORD lpcbMaxClassLen,
		LPDWORD lpcValues,
		LPDWORD lpcbMaxValueNameLen,
		LPDWORD lpcbMaxValueLen,
		LPDWORD lpcbSecurityDescriptor,
		PFILETIME lpftLastWriteTime)
{
	RapiContext* context = rapi_context_current();
	LONG return_value = 0;

	rapi_context_begin_command(context, 0x25);
	rapi_buffer_write_uint32         (context->send_buffer, hKey);
	rapi_buffer_write_optional       (context->send_buffer, lpClass, lpcbClass ? *lpcbClass * sizeof(WCHAR) : 0, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpcbClass, true);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpReserved, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpcSubKeys, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpcbMaxSubKeyLen, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpcbMaxClassLen, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpcValues, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpcbMaxValueNameLen, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpcbMaxValueLen, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpcbSecurityDescriptor, false);
	rapi_buffer_write_optional       (context->send_buffer, lpftLastWriteTime, sizeof(FILETIME), false);

	if ( !rapi2_context_call(context) )
		return false;

	rapi_buffer_read_uint32(context->recv_buffer, &context->last_error);
	rapi_buffer_read_int32(context->recv_buffer, &return_value);

	if (ERROR_SUCCESS == return_value)
	{
		rapi_buffer_read_optional       (context->recv_buffer, lpClass, lpcbClass ? *lpcbClass * sizeof(WCHAR) : 0);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpcbClass);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpReserved);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpcSubKeys);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpcbMaxSubKeyLen);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpcbMaxClassLen);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpcValues);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpcbMaxValueNameLen);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpcbMaxValueLen);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpcbSecurityDescriptor);
		rapi_buffer_read_optional_filetime(context->recv_buffer, lpftLastWriteTime);
	}

	return return_value;
}

LONG _CeRegEnumValue2(
		HKEY hKey,
		DWORD dwIndex,
		LPWSTR lpszValueName,
		LPDWORD lpcbValueName,
		LPDWORD lpReserved,
		LPDWORD lpType,
		LPBYTE lpData,
		LPDWORD lpcbData)
{
	RapiContext* context = rapi_context_current();
	LONG return_value = 0;

	rapi_context_begin_command(context, 0x23);
	rapi_buffer_write_uint32         (context->send_buffer, hKey);
	rapi_buffer_write_uint32         (context->send_buffer, dwIndex);
	rapi_buffer_write_optional       (context->send_buffer, lpszValueName, lpcbValueName ? *lpcbValueName * sizeof(WCHAR) : 0, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpcbValueName, true);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpReserved, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpType, false);
	rapi_buffer_write_optional       (context->send_buffer, lpData, lpcbData ? *lpcbData : 0, false);
	rapi_buffer_write_optional_uint32(context->send_buffer, lpcbData, true);

	if ( !rapi2_context_call(context) )
		return false;

	rapi_buffer_read_uint32(context->recv_buffer, &context->last_error);
	rapi_buffer_read_int32(context->recv_buffer, &return_value);

	if (ERROR_SUCCESS == return_value)
	{
		rapi_buffer_read_optional       (context->recv_buffer, lpszValueName, lpcbValueName ? *lpcbValueName * sizeof(WCHAR) : 0);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpcbValueName);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpReserved);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpType);
		rapi_buffer_read_optional       (context->recv_buffer, lpData, lpcbData ? *lpcbData : 0);
		rapi_buffer_read_optional_uint32(context->recv_buffer, lpcbData);
	}

	return return_value;
}
*/

LONG _CeRegEnumKeyEx2(
		HKEY hKey,
		DWORD dwIndex,
		LPWSTR lpName,
		LPDWORD lpcbName,
		LPDWORD lpReserved,
		LPWSTR lpClass,
		LPDWORD lpcbClass,
		PFILETIME lpftLastWriteTime)
{
	RapiContext* context = rapi_context_current();
	LONG return_value = 0;

	rapi_context_begin_command(context, 0x30);
	rapi_buffer_write_uint32         (context->send_buffer, hKey);
	rapi_buffer_write_uint32         (context->send_buffer, dwIndex);

    rapi_buffer_write_uint32(context->send_buffer, *lpcbName);
    rapi_buffer_write_uint32(context->send_buffer,  0);
    rapi_buffer_write_uint32(context->send_buffer, 0);


	if ( !rapi2_context_call(context) )
		return false;

	rapi_buffer_read_uint32(context->recv_buffer, &context->last_error);
	rapi_buffer_read_int32(context->recv_buffer, &return_value);

	if (ERROR_SUCCESS == return_value)
    {
        rapi_buffer_read_string(context->recv_buffer, lpName, lpcbName );
    }

	return return_value;
}

LONG _CeRegSetValueEx2(
		HKEY hKey,
		LPCWSTR lpValueName,
		DWORD Reserved,
		DWORD dwType,
		const BYTE *lpData,
		DWORD cbData)
{
	RapiContext* context = rapi_context_current();
	LONG return_value = 0;

	rapi_context_begin_command(context, 0x38);
	rapi_buffer_write_uint32(context->send_buffer, hKey);
	rapi2_buffer_write_string(context->send_buffer, lpValueName);
	rapi_buffer_write_uint32(context->send_buffer, dwType);
        rapi_buffer_write_uint32(context->send_buffer, cbData);
        rapi_buffer_write_data(context->send_buffer, lpData, cbData);

	if ( !rapi2_context_call(context) )
		return false;

	if (!rapi_buffer_read_uint32(context->recv_buffer, &context->last_error))
		return false;
	if (!rapi_buffer_read_int32(context->recv_buffer, &return_value))
		return false;

	return return_value;
}

