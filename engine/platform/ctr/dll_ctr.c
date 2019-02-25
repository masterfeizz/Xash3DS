/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "dll_ctr.h"

typedef struct dll_s
{
	const char *name;
	int refcnt;
	dllexport_t *exp;
	struct dll_s *next;
} dll_t;

static dll_t *dll_list;
static char *dll_err = NULL;

static void *dlfind( const char *name )
{
	dll_t *d = NULL;
	for( d = dll_list; d; d = d->next )
		if( !Q_strcmp( d->name, name ) )
			break;
	return d;
}

static const char *dlname( void *handle )
{
	dll_t *d = NULL;
	// iterate through all dll_ts to check if the handle is actually in the list
	// and not some bogus pointer from god knows where
	for( d = dll_list; d; d = d->next ) if( d == handle ) break;
	return d ? d->name : NULL;
}

void *dlopen( const char *name, int flag )
{
	printf("dlopen: %s\n", name);
	dll_t *d = dlfind( name );
	if( d ) d->refcnt++;
	else dll_err = "dlopen(): unknown dll name"; 
	return d;
}

void *dlsym( void *handle, const char *symbol )
{
	if( !handle || !symbol ) { dll_err = "dlsym(): NULL args"; return NULL; }
	if( !dlname( handle ) ) { dll_err = "dlsym(): unknown handle"; return NULL; }
	dll_t *d = handle;
	if( !d->refcnt ) { dll_err = "dlsym(): call dlopen() first"; return NULL; }
	dllexport_t *f = NULL;
	for( f = d->exp; f && f->func; f++ )
		if( !Q_strcmp( f->name, symbol ) )
			break;

	if( f && f->func )
	{
		return f->func;
	}
	else
	{
		dll_err = "dlsym(): symbol not found in dll";
		return NULL;
	}
}

int dlclose( void *handle )
{
	if( !handle ) { dll_err = "dlclose(): NULL arg"; return -1; }
	if( !dlname( handle ) ) { dll_err = "dlclose(): unknown handle"; return -2; }
	dll_t *d = handle;
	if( !d->refcnt ) { dll_err = "dlclose(): call dlopen() first"; return -3; }
	d->refcnt--;
	return 0;
}

char *dlerror( void )
{
	char *err = dll_err;
	dll_err = NULL;
	return err;
}

int dladdr( const void *addr, Dl_info *info )
{
	dll_t *d = NULL;
	dllexport_t *f = NULL;
	for( d = dll_list; d; d = d->next )
		for( f = d->exp; f && f->func; f++ )
			if( f->func == addr ) goto for_end;
for_end:
	if( d && f && f->func )
	{
		if( info )
		{
			info->dli_fhandle = d;
			info->dli_sname = f->name;
			info->dli_saddr = addr;
		}
		return 1;
	}
	return 0;
}

// export registering api for all dlls //

int dll_register( const char *name, dllexport_t *exports )
{
	if( !name || !exports ) return -1;
	if( dlfind( name ) ) return -2; // already registered
	dll_t *new = calloc( 1, sizeof( dll_t ) );
	if( !new ) return -3;
	new->name = name;
	new->exp = exports;
	new->next = dll_list;
	dll_list = new;
}
