/*
 * Protocol-level socket functions
 *
 * Copyright (C) 1993,1994,1996,1997 John Brezak, Erik Bos, Alex Korobka.
 * Copyright (C) 2001 Stefan Leichter
 * Copyright (C) 2004 Hans Leidekker
 * Copyright (C) 2005 Marcus Meissner
 * Copyright (C) 2006-2008 Kai Blin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "ws2_32_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(winsock);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

unixlib_handle_t ws_unix_handle = 0;

#define WS_CALL(func, params) __wine_unix_call( ws_unix_handle, ws_unix_ ## func, params )

static char *get_fqdn(void)
{
    char *ret;
    DWORD size = 0;

    GetComputerNameExA( ComputerNamePhysicalDnsFullyQualified, NULL, &size );
    if (GetLastError() != ERROR_MORE_DATA) return NULL;
    if (!(ret = HeapAlloc( GetProcessHeap(), 0, size ))) return NULL;
    if (!GetComputerNameExA( ComputerNamePhysicalDnsFullyQualified, ret, &size ))
    {
        HeapFree( GetProcessHeap(), 0, ret );
        return NULL;
    }
    return ret;
}

/* call Unix getaddrinfo, allocating a large enough buffer */
static int do_getaddrinfo( const char *node, const char *service,
                           const struct addrinfo *hints, struct addrinfo **info )
{
    unsigned int size = 1024;
    struct getaddrinfo_params params = { node, service, hints, NULL, &size };
    int ret;

    for (;;)
    {
        if (!(params.info = HeapAlloc( GetProcessHeap(), 0, size )))
            return WSA_NOT_ENOUGH_MEMORY;
        if (!(ret = WS_CALL( getaddrinfo, &params )))
        {
            *info = params.info;
            return ret;
        }
        HeapFree( GetProcessHeap(), 0, params.info );
        if (ret != ERROR_INSUFFICIENT_BUFFER) return ret;
    }
}


/***********************************************************************
 *      getaddrinfo   (ws2_32.@)
 */
int WINAPI getaddrinfo( const char *node, const char *service,
                        const struct addrinfo *hints, struct addrinfo **info )
{
    char *nodev6 = NULL, *fqdn = NULL;
    int ret;

    TRACE( "node %s, service %s, hints %p\n", debugstr_a(node), debugstr_a(service), hints );

    *info = NULL;

    if (!node && !service)
    {
        SetLastError( WSAHOST_NOT_FOUND );
        return WSAHOST_NOT_FOUND;
    }

    if (node)
    {
        if (!node[0])
        {
            if (!(fqdn = get_fqdn())) return WSA_NOT_ENOUGH_MEMORY;
            node = fqdn;
        }
        else if (!hints || hints->ai_family == AF_UNSPEC || hints->ai_family == AF_INET6)
        {
            /* [ipv6] or [ipv6]:portnumber are supported by Windows */
            char *close_bracket;

            if (node[0] == '[' && (close_bracket = strchr(node + 1, ']')))
            {
                nodev6 = HeapAlloc( GetProcessHeap(), 0, close_bracket - node );
                if (!nodev6) return WSA_NOT_ENOUGH_MEMORY;
                lstrcpynA( nodev6, node + 1, close_bracket - node );
                node = nodev6;
            }
        }
    }

    ret = do_getaddrinfo( node, service, hints, info );

    if (ret && (!hints || !(hints->ai_flags & AI_NUMERICHOST)) && node)
    {
        if (!fqdn && !(fqdn = get_fqdn()))
        {
            HeapFree( GetProcessHeap(), 0, nodev6 );
            return WSA_NOT_ENOUGH_MEMORY;
        }
        if (!strcmp( fqdn, node ) || (!strncmp( fqdn, node, strlen( node ) ) && fqdn[strlen( node )] == '.'))
        {
            /* If it didn't work it means the host name IP is not in /etc/hosts, try again
             * by sending a NULL host and avoid sending a NULL servname too because that
             * is invalid */
            ERR_(winediag)( "Failed to resolve your host name IP\n" );
            ret = do_getaddrinfo( NULL, service, hints, info );
            if (!ret && hints && (hints->ai_flags & AI_CANONNAME) && *info && !(*info)->ai_canonname)
            {
                freeaddrinfo( *info );
                *info = NULL;
                return EAI_NONAME;
            }
        }
    }

    HeapFree( GetProcessHeap(), 0, fqdn );
    HeapFree( GetProcessHeap(), 0, nodev6 );

    if (!ret && TRACE_ON(winsock))
    {
        struct addrinfo *ai;

        for (ai = *info; ai != NULL; ai = ai->ai_next)
        {
            TRACE( "=> %p, flags %#x, family %d, type %d, protocol %d, len %ld, name %s, addr %s\n",
                   ai, ai->ai_flags, ai->ai_family, ai->ai_socktype, ai->ai_protocol, ai->ai_addrlen,
                   ai->ai_canonname, debugstr_sockaddr(ai->ai_addr) );
        }
    }

    SetLastError( ret );
    return ret;
}


static ADDRINFOEXW *addrinfo_AtoW( const struct addrinfo *ai )
{
    ADDRINFOEXW *ret;

    if (!(ret = HeapAlloc( GetProcessHeap(), 0, sizeof(ADDRINFOEXW) ))) return NULL;
    ret->ai_flags     = ai->ai_flags;
    ret->ai_family    = ai->ai_family;
    ret->ai_socktype  = ai->ai_socktype;
    ret->ai_protocol  = ai->ai_protocol;
    ret->ai_addrlen   = ai->ai_addrlen;
    ret->ai_canonname = NULL;
    ret->ai_addr      = NULL;
    ret->ai_blob      = NULL;
    ret->ai_bloblen   = 0;
    ret->ai_provider  = NULL;
    ret->ai_next      = NULL;
    if (ai->ai_canonname)
    {
        int len = MultiByteToWideChar( CP_ACP, 0, ai->ai_canonname, -1, NULL, 0 );
        if (!(ret->ai_canonname = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) )))
        {
            HeapFree( GetProcessHeap(), 0, ret );
            return NULL;
        }
        MultiByteToWideChar( CP_ACP, 0, ai->ai_canonname, -1, ret->ai_canonname, len );
    }
    if (ai->ai_addr)
    {
        if (!(ret->ai_addr = HeapAlloc( GetProcessHeap(), 0, ai->ai_addrlen )))
        {
            HeapFree( GetProcessHeap(), 0, ret->ai_canonname );
            HeapFree( GetProcessHeap(), 0, ret );
            return NULL;
        }
        memcpy( ret->ai_addr, ai->ai_addr, ai->ai_addrlen );
    }
    return ret;
}

static ADDRINFOEXW *addrinfo_list_AtoW( const struct addrinfo *info )
{
    ADDRINFOEXW *ret, *infoW;

    if (!(ret = infoW = addrinfo_AtoW( info ))) return NULL;
    while (info->ai_next)
    {
        if (!(infoW->ai_next = addrinfo_AtoW( info->ai_next )))
        {
            FreeAddrInfoExW( ret );
            return NULL;
        }
        infoW = infoW->ai_next;
        info = info->ai_next;
    }
    return ret;
}

static struct addrinfo *addrinfo_WtoA( const struct addrinfoW *ai )
{
    struct addrinfo *ret;

    if (!(ret = HeapAlloc( GetProcessHeap(), 0, sizeof(struct addrinfo) ))) return NULL;
    ret->ai_flags     = ai->ai_flags;
    ret->ai_family    = ai->ai_family;
    ret->ai_socktype  = ai->ai_socktype;
    ret->ai_protocol  = ai->ai_protocol;
    ret->ai_addrlen   = ai->ai_addrlen;
    ret->ai_canonname = NULL;
    ret->ai_addr      = NULL;
    ret->ai_next      = NULL;
    if (ai->ai_canonname)
    {
        int len = WideCharToMultiByte( CP_ACP, 0, ai->ai_canonname, -1, NULL, 0, NULL, NULL );
        if (!(ret->ai_canonname = HeapAlloc( GetProcessHeap(), 0, len )))
        {
            HeapFree( GetProcessHeap(), 0, ret );
            return NULL;
        }
        WideCharToMultiByte( CP_ACP, 0, ai->ai_canonname, -1, ret->ai_canonname, len, NULL, NULL );
    }
    if (ai->ai_addr)
    {
        if (!(ret->ai_addr = HeapAlloc( GetProcessHeap(), 0, sizeof(struct sockaddr) )))
        {
            HeapFree( GetProcessHeap(), 0, ret->ai_canonname );
            HeapFree( GetProcessHeap(), 0, ret );
            return NULL;
        }
        memcpy( ret->ai_addr, ai->ai_addr, sizeof(struct sockaddr) );
    }
    return ret;
}

struct getaddrinfo_args
{
    OVERLAPPED *overlapped;
    LPLOOKUPSERVICE_COMPLETION_ROUTINE completion_routine;
    ADDRINFOEXW **result;
    char *nodename;
    char *servname;
    struct addrinfo *hints;
};

static void WINAPI getaddrinfo_callback(TP_CALLBACK_INSTANCE *instance, void *context)
{
    struct getaddrinfo_args *args = context;
    OVERLAPPED *overlapped = args->overlapped;
    HANDLE event = overlapped->hEvent;
    LPLOOKUPSERVICE_COMPLETION_ROUTINE completion_routine = args->completion_routine;
    struct addrinfo *res;
    int ret;

    ret = getaddrinfo( args->nodename, args->servname, args->hints, &res );
    if (res)
    {
        *args->result = addrinfo_list_AtoW(res);
        overlapped->u.Pointer = args->result;
        freeaddrinfo(res);
    }

    HeapFree( GetProcessHeap(), 0, args->nodename );
    HeapFree( GetProcessHeap(), 0, args->servname );
    HeapFree( GetProcessHeap(), 0, args );

    overlapped->Internal = ret;
    if (completion_routine) completion_routine( ret, 0, overlapped );
    if (event) SetEvent( event );
}

static int getaddrinfoW( const WCHAR *nodename, const WCHAR *servname,
                            const struct addrinfo *hints, ADDRINFOEXW **res, OVERLAPPED *overlapped,
                            LPLOOKUPSERVICE_COMPLETION_ROUTINE completion_routine )
{
    int ret = EAI_MEMORY, len, i;
    char *nodenameA = NULL, *servnameA = NULL;
    struct addrinfo *resA;
    WCHAR *local_nodenameW = (WCHAR *)nodename;

    *res = NULL;
    if (nodename)
    {
        /* Is this an IDN? Most likely if any char is above the Ascii table, this
         * is the simplest validation possible, further validation will be done by
         * the native getaddrinfo() */
        for (i = 0; nodename[i]; i++)
        {
            if (nodename[i] > 'z')
                break;
        }
        if (nodename[i])
        {
            if (hints && (hints->ai_flags & AI_DISABLE_IDN_ENCODING))
            {
                /* Name requires conversion but it was disabled */
                ret = WSAHOST_NOT_FOUND;
                SetLastError( ret );
                goto end;
            }

            len = IdnToAscii( 0, nodename, -1, NULL, 0 );
            if (!len)
            {
                ERR("Failed to convert %s to punycode\n", debugstr_w(nodename));
                ret = EAI_FAIL;
                goto end;
            }
            if (!(local_nodenameW = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) ))) goto end;
            IdnToAscii( 0, nodename, -1, local_nodenameW, len );
        }
    }
    if (local_nodenameW)
    {
        len = WideCharToMultiByte( CP_ACP, 0, local_nodenameW, -1, NULL, 0, NULL, NULL );
        if (!(nodenameA = HeapAlloc( GetProcessHeap(), 0, len ))) goto end;
        WideCharToMultiByte( CP_ACP, 0, local_nodenameW, -1, nodenameA, len, NULL, NULL );
    }
    if (servname)
    {
        len = WideCharToMultiByte( CP_ACP, 0, servname, -1, NULL, 0, NULL, NULL );
        if (!(servnameA = HeapAlloc( GetProcessHeap(), 0, len ))) goto end;
        WideCharToMultiByte( CP_ACP, 0, servname, -1, servnameA, len, NULL, NULL );
    }

    if (overlapped)
    {
        struct getaddrinfo_args *args;

        if (overlapped->hEvent && completion_routine)
        {
            ret = WSAEINVAL;
            goto end;
        }

        if (!(args = HeapAlloc( GetProcessHeap(), 0, sizeof(*args) + sizeof(*args->hints) ))) goto end;
        args->overlapped = overlapped;
        args->completion_routine = completion_routine;
        args->result = res;
        args->nodename = nodenameA;
        args->servname = servnameA;
        if (hints)
        {
            args->hints = (struct addrinfo *)(args + 1);
            args->hints->ai_flags    = hints->ai_flags;
            args->hints->ai_family   = hints->ai_family;
            args->hints->ai_socktype = hints->ai_socktype;
            args->hints->ai_protocol = hints->ai_protocol;
        }
        else args->hints = NULL;

        overlapped->Internal = WSAEINPROGRESS;
        if (!TrySubmitThreadpoolCallback( getaddrinfo_callback, args, NULL ))
        {
            HeapFree( GetProcessHeap(), 0, args );
            ret = GetLastError();
            goto end;
        }

        if (local_nodenameW != nodename)
            HeapFree( GetProcessHeap(), 0, local_nodenameW );
        SetLastError( ERROR_IO_PENDING );
        return ERROR_IO_PENDING;
    }

    ret = getaddrinfo( nodenameA, servnameA, hints, &resA );
    if (!ret)
    {
        *res = addrinfo_list_AtoW( resA );
        freeaddrinfo( resA );
    }

end:
    if (local_nodenameW != nodename)
        HeapFree( GetProcessHeap(), 0, local_nodenameW );
    HeapFree( GetProcessHeap(), 0, nodenameA );
    HeapFree( GetProcessHeap(), 0, servnameA );
    return ret;
}


/***********************************************************************
 *      GetAddrInfoExW   (ws2_32.@)
 */
int WINAPI GetAddrInfoExW( const WCHAR *name, const WCHAR *servname, DWORD namespace,
                           GUID *namespace_id, const ADDRINFOEXW *hints, ADDRINFOEXW **result,
                           struct timeval *timeout, OVERLAPPED *overlapped,
                           LPLOOKUPSERVICE_COMPLETION_ROUTINE completion_routine, HANDLE *handle )
{
    int ret;

    TRACE( "name %s, servname %s, namespace %u, namespace_id %s)\n",
           debugstr_w(name), debugstr_w(servname), namespace, debugstr_guid(namespace_id) );

    if (namespace != NS_DNS)
        FIXME( "Unsupported namespace %u\n", namespace );
    if (namespace_id)
        FIXME( "Unsupported namespace_id %s\n", debugstr_guid(namespace_id) );
    if (timeout)
        FIXME( "Unsupported timeout\n" );
    if (handle)
        FIXME( "Unsupported cancel handle\n" );

    ret = getaddrinfoW( name, servname, (struct addrinfo *)hints, result, overlapped, completion_routine );
    if (ret) return ret;
    if (handle) *handle = (HANDLE)0xdeadbeef;
    return 0;
}


/***********************************************************************
 *      GetAddrInfoExOverlappedResult   (ws2_32.@)
 */
int WINAPI GetAddrInfoExOverlappedResult( OVERLAPPED *overlapped )
{
    TRACE( "(%p)\n", overlapped );
    return overlapped->Internal;
}


/***********************************************************************
 *      GetAddrInfoExCancel   (ws2_32.@)
 */
int WINAPI GetAddrInfoExCancel( HANDLE *handle )
{
    FIXME( "(%p)\n", handle );
    return WSA_INVALID_HANDLE;
}


/***********************************************************************
 *      GetAddrInfoW   (ws2_32.@)
 */
int WINAPI GetAddrInfoW(const WCHAR *nodename, const WCHAR *servname, const ADDRINFOW *hints, ADDRINFOW **res)
{
    struct addrinfo *hintsA = NULL;
    ADDRINFOEXW *resex;
    int ret = EAI_MEMORY;

    TRACE( "nodename %s, servname %s, hints %p, result %p\n",
           debugstr_w(nodename), debugstr_w(servname), hints, res );

    *res = NULL;
    if (hints) hintsA = addrinfo_WtoA( hints );
    ret = getaddrinfoW( nodename, servname, hintsA, &resex, NULL, NULL );
    freeaddrinfo( hintsA );
    if (ret) return ret;

    if (resex)
    {
        /* ADDRINFOEXW has a layout compatible with ADDRINFOW except for the
         * ai_next field, so we may convert it in place */
        *res = (ADDRINFOW *)resex;
        do
        {
            ((ADDRINFOW *)resex)->ai_next = (ADDRINFOW *)resex->ai_next;
            resex = resex->ai_next;
        } while (resex);
    }
    return 0;
}


/***********************************************************************
 *      freeaddrinfo   (ws2_32.@)
 */
void WINAPI freeaddrinfo( struct addrinfo *info )
{
    TRACE( "%p\n", info );

    HeapFree( GetProcessHeap(), 0, info );
}


/***********************************************************************
 *      FreeAddrInfoW   (ws2_32.@)
 */
void WINAPI FreeAddrInfoW( ADDRINFOW *ai )
{
    while (ai)
    {
        ADDRINFOW *next;
        HeapFree( GetProcessHeap(), 0, ai->ai_canonname );
        HeapFree( GetProcessHeap(), 0, ai->ai_addr );
        next = ai->ai_next;
        HeapFree( GetProcessHeap(), 0, ai );
        ai = next;
    }
}


/***********************************************************************
 *      FreeAddrInfoEx   (ws2_32.@)
 */
void WINAPI FreeAddrInfoEx( ADDRINFOEXA *ai )
{
    TRACE( "(%p)\n", ai );

    while (ai)
    {
        ADDRINFOEXA *next;
        HeapFree( GetProcessHeap(), 0, ai->ai_canonname );
        HeapFree( GetProcessHeap(), 0, ai->ai_addr );
        next = ai->ai_next;
        HeapFree( GetProcessHeap(), 0, ai );
        ai = next;
    }
}


/***********************************************************************
 *      FreeAddrInfoExW   (ws2_32.@)
 */
void WINAPI FreeAddrInfoExW( ADDRINFOEXW *ai )
{
    TRACE( "(%p)\n", ai );

    while (ai)
    {
        ADDRINFOEXW *next;
        HeapFree( GetProcessHeap(), 0, ai->ai_canonname );
        HeapFree( GetProcessHeap(), 0, ai->ai_addr );
        next = ai->ai_next;
        HeapFree( GetProcessHeap(), 0, ai );
        ai = next;
    }
}


/***********************************************************************
 *      getnameinfo   (ws2_32.@)
 */
int WINAPI getnameinfo( const SOCKADDR *addr, socklen_t addr_len, char *host,
                           DWORD host_len, char *serv, DWORD serv_len, int flags )
{
    struct getnameinfo_params params = { addr, addr_len, host, host_len, serv, serv_len, flags };

    TRACE( "addr %s, addr_len %d, host %p, host_len %u, serv %p, serv_len %d, flags %#x\n",
           debugstr_sockaddr(addr), addr_len, host, host_len, serv, serv_len, flags );

    return WS_CALL( getnameinfo, &params );
}


/***********************************************************************
 *      GetNameInfoW   (ws2_32.@)
 */
int WINAPI GetNameInfoW( const SOCKADDR *addr, socklen_t addr_len, WCHAR *host,
                         DWORD host_len, WCHAR *serv, DWORD serv_len, int flags )
{
    int ret;
    char *hostA = NULL, *servA = NULL;

    if (host && (!(hostA = HeapAlloc( GetProcessHeap(), 0, host_len ))))
        return EAI_MEMORY;
    if (serv && (!(servA = HeapAlloc( GetProcessHeap(), 0, serv_len ))))
    {
        HeapFree( GetProcessHeap(), 0, hostA );
        return EAI_MEMORY;
    }

    ret = getnameinfo( addr, addr_len, hostA, host_len, servA, serv_len, flags );
    if (!ret)
    {
        if (host) MultiByteToWideChar( CP_ACP, 0, hostA, -1, host, host_len );
        if (serv) MultiByteToWideChar( CP_ACP, 0, servA, -1, serv, serv_len );
    }

    HeapFree( GetProcessHeap(), 0, hostA );
    HeapFree( GetProcessHeap(), 0, servA );
    return ret;
}


static struct hostent *get_hostent_buffer( unsigned int size )
{
    struct per_thread_data *data = get_per_thread_data();
    if (data->he_buffer)
    {
        if (data->he_len >= size) return data->he_buffer;
        HeapFree( GetProcessHeap(), 0, data->he_buffer );
    }
    data->he_buffer = HeapAlloc( GetProcessHeap(), 0, (data->he_len = size) );
    if (!data->he_buffer) SetLastError(WSAENOBUFS);
    return data->he_buffer;
}

/* create a hostent entry
 *
 * Creates the entry with enough memory for the name, aliases
 * addresses, and the address pointers.  Also copies the name
 * and sets up all the pointers.
 *
 * NOTE: The alias and address lists must be allocated with room
 * for the NULL item terminating the list.  This is true even if
 * the list has no items ("aliases" and "addresses" must be
 * at least "1", a truly empty list is invalid).
 */
static struct hostent *create_hostent( char *name, int alias_count, int aliases_size,
                                       int address_count, int address_length )
{
    struct hostent *p_to;
    char *p;
    unsigned int size = sizeof(struct hostent), i;

    size += strlen(name) + 1;
    size += alias_count * sizeof(char *);
    size += aliases_size;
    size += address_count * sizeof(char *);
    size += (address_count - 1) * address_length;

    if (!(p_to = get_hostent_buffer( size ))) return NULL;
    memset( p_to, 0, size );

    /* Use the memory in the same way winsock does.
     * First set the pointer for aliases, second set the pointers for addresses.
     * Third fill the addresses indexes, fourth jump aliases names size.
     * Fifth fill the hostname.
     * NOTE: This method is valid for OS versions >= XP.
     */
    p = (char *)(p_to + 1);
    p_to->h_aliases = (char **)p;
    p += alias_count * sizeof(char *);

    p_to->h_addr_list = (char **)p;
    p += address_count * sizeof(char *);

    for (i = 0, address_count--; i < address_count; i++, p += address_length)
        p_to->h_addr_list[i] = p;

    /* h_aliases must be filled in manually because we don't know each string
     * size. Leave these pointers NULL (already set to NULL by memset earlier).
     */
    p += aliases_size;

    p_to->h_name = p;
    strcpy( p, name );

    return p_to;
}


/***********************************************************************
 *      gethostbyaddr   (ws2_32.51)
 */
struct hostent * WINAPI gethostbyaddr( const char *addr, int len, int family )
{
    unsigned int size = 1024;
    struct gethostbyaddr_params params = { addr, len, family, NULL, &size };
    int ret;

    for (;;)
    {
        if (!(params.host = get_hostent_buffer( size )))
            return NULL;

        if ((ret = WS_CALL( gethostbyaddr, &params )) != ERROR_INSUFFICIENT_BUFFER)
            break;
    }

    SetLastError( ret );
    return ret ? NULL : params.host;
}


struct route
{
    struct in_addr addr;
    IF_INDEX interface;
    DWORD metric, default_route;
};

static int __cdecl compare_routes_by_metric_asc( const void *left, const void *right )
{
    const struct route *a = left, *b = right;
    if (a->default_route && b->default_route)
        return a->default_route - b->default_route;
    if (a->default_route && !b->default_route)
        return -1;
    if (b->default_route && !a->default_route)
        return 1;
    return a->metric - b->metric;
}

/* Returns the list of local IP addresses by going through the network
 * adapters and using the local routing table to sort the addresses
 * from highest routing priority to lowest routing priority. This
 * functionality is inferred from the description for obtaining local
 * IP addresses given in the Knowledge Base Article Q160215.
 *
 * Please note that the returned hostent is only freed when the thread
 * closes and is replaced if another hostent is requested.
 */
static struct hostent *get_local_ips( char *hostname )
{
    int numroutes = 0, i, j, default_routes = 0;
    IP_ADAPTER_INFO *adapters = NULL, *k;
    struct hostent *hostlist = NULL;
    MIB_IPFORWARDTABLE *routes = NULL;
    struct route *route_addrs = NULL;
    DWORD adap_size, route_size, n;

    /* Obtain the size of the adapter list and routing table, also allocate memory */
    if (GetAdaptersInfo( NULL, &adap_size ) != ERROR_BUFFER_OVERFLOW)
        return NULL;
    if (GetIpForwardTable( NULL, &route_size, FALSE ) != ERROR_INSUFFICIENT_BUFFER)
        return NULL;

    adapters = HeapAlloc( GetProcessHeap(), 0, adap_size );
    routes = HeapAlloc( GetProcessHeap(), 0, route_size );
    if (!adapters || !routes)
        goto cleanup;

    /* Obtain the adapter list and the full routing table */
    if (GetAdaptersInfo( adapters, &adap_size ) != NO_ERROR)
        goto cleanup;
    if (GetIpForwardTable( routes, &route_size, FALSE ) != NO_ERROR)
        goto cleanup;

    /* Store the interface associated with each route */
    for (n = 0; n < routes->dwNumEntries; n++)
    {
        IF_INDEX ifindex;
        DWORD ifmetric, ifdefault = 0;
        BOOL exists = FALSE;

        /* Check if this is a default route (there may be more than one) */
        if (!routes->table[n].dwForwardDest)
            ifdefault = ++default_routes;
        else if (routes->table[n].u1.ForwardType != MIB_IPROUTE_TYPE_DIRECT)
            continue;
        ifindex = routes->table[n].dwForwardIfIndex;
        ifmetric = routes->table[n].dwForwardMetric1;
        /* Only store the lowest valued metric for an interface */
        for (j = 0; j < numroutes; j++)
        {
            if (route_addrs[j].interface == ifindex)
            {
                if (route_addrs[j].metric > ifmetric)
                    route_addrs[j].metric = ifmetric;
                exists = TRUE;
            }
        }
        if (exists)
            continue;
        route_addrs = heap_realloc( route_addrs, (numroutes + 1) * sizeof(struct route) );
        if (!route_addrs)
            goto cleanup;
        route_addrs[numroutes].interface = ifindex;
        route_addrs[numroutes].metric = ifmetric;
        route_addrs[numroutes].default_route = ifdefault;
        /* If no IP is found in the next step (for whatever reason)
         * then fall back to the magic loopback address.
         */
        memcpy( &route_addrs[numroutes].addr.s_addr, magic_loopback_addr, 4 );
        numroutes++;
    }
    if (numroutes == 0)
       goto cleanup; /* No routes, fall back to the Magic IP */

    /* Find the IP address associated with each found interface */
    for (i = 0; i < numroutes; i++)
    {
        for (k = adapters; k != NULL; k = k->Next)
        {
            char *ip = k->IpAddressList.IpAddress.String;

            if (route_addrs[i].interface == k->Index)
                route_addrs[i].addr.s_addr = inet_addr(ip);
        }
    }

    /* Allocate a hostent and enough memory for all the IPs,
     * including the NULL at the end of the list.
     */
    hostlist = create_hostent( hostname, 1, 0, numroutes+1, sizeof(struct in_addr) );
    if (hostlist == NULL)
        goto cleanup;
    hostlist->h_addr_list[numroutes] = NULL;
    hostlist->h_aliases[0] = NULL;
    hostlist->h_addrtype = AF_INET;
    hostlist->h_length = sizeof(struct in_addr);

    /* Reorder the entries before placing them in the host list. Windows expects
     * the IP list in order from highest priority to lowest (the critical thing
     * is that most applications expect the first IP to be the default route).
     */
    if (numroutes > 1)
        qsort( route_addrs, numroutes, sizeof(struct route), compare_routes_by_metric_asc );

    for (i = 0; i < numroutes; i++)
        *(struct in_addr *)hostlist->h_addr_list[i] = route_addrs[i].addr;

cleanup:
    HeapFree( GetProcessHeap(), 0, route_addrs );
    HeapFree( GetProcessHeap(), 0, adapters );
    HeapFree( GetProcessHeap(), 0, routes );
    return hostlist;
}


/***********************************************************************
 *      gethostbyname   (ws2_32.52)
 */
struct hostent * WINAPI gethostbyname( const char *name )
{
    struct hostent *host = NULL;
    char hostname[100];
    struct gethostname_params params = { hostname, sizeof(hostname) };
    int ret;

    TRACE( "%s\n", debugstr_a(name) );

    if (!num_startup)
    {
        SetLastError( WSANOTINITIALISED );
        return NULL;
    }

    if ((ret = WS_CALL( gethostname, &params )))
    {
        SetLastError( ret );
        return NULL;
    }

    if (!name || !name[0])
        name = hostname;

    /* If the hostname of the local machine is requested then return the
     * complete list of local IP addresses */
    if (!strcmp( name, hostname ))
        host = get_local_ips( hostname );

    /* If any other hostname was requested (or the routing table lookup failed)
     * then return the IP found by the host OS */
    if (!host)
    {
        unsigned int size = 1024;
        struct gethostbyname_params params = { name, NULL, &size };
        int ret;

        for (;;)
        {
            if (!(params.host = get_hostent_buffer( size )))
                return NULL;

            if ((ret = WS_CALL( gethostbyname, &params )) != ERROR_INSUFFICIENT_BUFFER)
                break;
        }

        SetLastError( ret );
        return ret ? NULL : params.host;
    }

    if (host && host->h_addr_list[0][0] == 127 && strcmp( name, "localhost" ))
    {
        /* hostname != "localhost" but has loopback address. replace by our
         * special address.*/
        memcpy( host->h_addr_list[0], magic_loopback_addr, 4 );
    }

    return host;
}


/***********************************************************************
 *      gethostname   (ws2_32.57)
 */
int WINAPI gethostname( char *name, int namelen )
{
    char buf[256];
    struct gethostname_params params = { buf, sizeof(buf) };
    int len, ret;

    TRACE( "name %p, len %d\n", name, namelen );

    if (!name)
    {
        SetLastError( WSAEFAULT );
        return -1;
    }

    if ((ret = WS_CALL( gethostname, &params )))
    {
        SetLastError( ret );
        return -1;
    }

    TRACE( "<- %s\n", debugstr_a(buf) );
    len = strlen( buf );
    if (len > 15)
        WARN( "Windows supports NetBIOS name length up to 15 bytes!\n" );
    if (namelen <= len)
    {
        SetLastError( WSAEFAULT );
        return -1;
    }
    strcpy( name, buf );
    return 0;
}


/***********************************************************************
 *      GetHostNameW   (ws2_32.@)
 */
int WINAPI GetHostNameW( WCHAR *name, int namelen )
{
    char buf[256];
    struct gethostname_params params = { buf, sizeof(buf) };
    int ret;

    TRACE( "name %p, len %d\n", name, namelen );

    if (!name)
    {
        SetLastError( WSAEFAULT );
        return -1;
    }

    if ((ret = WS_CALL( gethostname, &params )))
    {
        SetLastError( ret );
        return -1;
    }

    if (MultiByteToWideChar( CP_ACP, 0, buf, -1, NULL, 0 ) > namelen)
    {
        SetLastError( WSAEFAULT );
        return -1;
    }
    MultiByteToWideChar( CP_ACP, 0, buf, -1, name, namelen );
    return 0;
}


static char *read_etc_file( const WCHAR *filename, DWORD *ret_size )
{
    WCHAR path[MAX_PATH];
    DWORD size = sizeof(path);
    HANDLE file;
    char *data;
    LONG ret;

    if ((ret = RegGetValueW( HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\tcpip\\Parameters",
                             L"DatabasePath", RRF_RT_REG_SZ, NULL, path, &size )))
    {
        ERR( "failed to get database path, error %u\n", ret );
        return NULL;
    }
    wcscat( path, L"\\" );
    wcscat( path, filename );

    file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    if (file == INVALID_HANDLE_VALUE)
    {
        ERR( "failed to open %s, error %u\n", debugstr_w( path ), GetLastError() );
        return NULL;
    }

    size = GetFileSize( file, NULL );
    if (!(data = HeapAlloc( GetProcessHeap(), 0, size )) ||
        !ReadFile( file, data, size, ret_size, NULL ))
    {
        WARN( "failed to read file, error %u\n", GetLastError() );
        HeapFree( GetProcessHeap(), 0, data );
        data = NULL;
    }
    CloseHandle( file );
    return data;
}

/* returns "end" if there was no space */
static char *next_space( const char *p, const char *end )
{
    while (p < end && !isspace( *p ))
        ++p;
    return (char *)p;
}

/* returns "end" if there was no non-space */
static char *next_non_space( const char *p, const char *end )
{
    while (p < end && isspace( *p ))
        ++p;
    return (char *)p;
}


static struct protoent *get_protoent_buffer( unsigned int size )
{
    struct per_thread_data *data = get_per_thread_data();

    if (data->pe_buffer)
    {
        if (data->pe_len >= size) return data->pe_buffer;
        HeapFree( GetProcessHeap(), 0, data->pe_buffer );
    }
    data->pe_len = size;
    data->pe_buffer = HeapAlloc( GetProcessHeap(), 0, size );
    if (!data->pe_buffer) SetLastError( WSAENOBUFS );
    return data->pe_buffer;
}

/* Parse the first valid line into a protoent structure, returning NULL if
 * there is no valid line. Updates cursor to point to the start of the next
 * line or the end of the file. */
static struct protoent *get_next_protocol( const char **cursor, const char *end )
{
    const char *p = *cursor;

    while (p < end)
    {
        const char *line_end, *next_line;
        size_t needed_size, line_len;
        unsigned int alias_count = 0;
        struct protoent *proto;
        const char *name;
        int number;
        char *q;

        for (line_end = p; line_end < end && *line_end != '\n' && *line_end != '#'; ++line_end)
            ;
        TRACE( "parsing line %s\n", debugstr_an(p, line_end - p) );

        for (next_line = line_end; next_line < end && *next_line != '\n'; ++next_line)
            ;
        if (next_line < end)
            ++next_line; /* skip over the newline */

        p = next_non_space( p, line_end );
        if (p == line_end)
        {
            p = next_line;
            continue;
        }

        /* parse the name */

        name = p;
        line_len = line_end - name;

        p = next_space( p, line_end );
        if (p == line_end)
        {
            p = next_line;
            continue;
        }

        p = next_non_space( p, line_end );

        /* parse the number */

        number = atoi( p );

        p = next_space( p, line_end );
        p = next_non_space( p, line_end );

        /* we will copy the entire line after the protoent structure, then
         * replace spaces with null bytes as necessary */

        while (p < line_end)
        {
            ++alias_count;

            p = next_space( p, line_end );
            p = next_non_space( p, line_end );
        }
        needed_size = sizeof(*proto) + line_len + 1 + (alias_count + 1) * sizeof(char *);

        if (!(proto = get_protoent_buffer( needed_size )))
        {
            SetLastError( WSAENOBUFS );
            return NULL;
        }

        proto->p_proto = number;
        proto->p_aliases = (char **)(proto + 1);
        proto->p_name = (char *)(proto->p_aliases + alias_count + 1);

        memcpy( proto->p_name, name, line_len );
        proto->p_name[line_len] = 0;

        line_end = proto->p_name + line_len;

        q = proto->p_name;
        q = next_space( q, line_end );
        *q++ = 0;
        q = next_non_space( q, line_end );
        /* skip over the number */
        q = next_space( q, line_end );
        q = next_non_space( q, line_end );

        alias_count = 0;
        while (q < line_end)
        {
            proto->p_aliases[alias_count++] = q;
            q = next_space( q, line_end );
            if (q < line_end) *q++ = 0;
            q = next_non_space( q, line_end );
        }
        proto->p_aliases[alias_count] = NULL;

        *cursor = next_line;
        return proto;
    }

    SetLastError( WSANO_DATA );
    return NULL;
}


/***********************************************************************
 *      getprotobyname   (ws2_32.53)
 */
struct protoent * WINAPI getprotobyname( const char *name )
{
    struct protoent *proto;
    const char *cursor;
    char *file;
    DWORD size;

    TRACE( "%s\n", debugstr_a(name) );

    if (!(file = read_etc_file( L"protocol", &size )))
    {
        SetLastError( WSANO_DATA );
        return NULL;
    }

    cursor = file;
    while ((proto = get_next_protocol( &cursor, file + size )))
    {
        if (!strcasecmp( proto->p_name, name ))
            break;
    }

    HeapFree( GetProcessHeap(), 0, file );
    return proto;
}


/***********************************************************************
 *      getprotobynumber   (ws2_32.54)
 */
struct protoent * WINAPI getprotobynumber( int number )
{
    struct protoent *proto;
    const char *cursor;
    char *file;
    DWORD size;

    TRACE( "%d\n", number );

    if (!(file = read_etc_file( L"protocol", &size )))
    {
        SetLastError( WSANO_DATA );
        return NULL;
    }

    cursor = file;
    while ((proto = get_next_protocol( &cursor, file + size )))
    {
        if (proto->p_proto == number)
            break;
    }

    HeapFree( GetProcessHeap(), 0, file );
    return proto;
}


static struct servent *get_servent_buffer( int size )
{
    struct per_thread_data *data = get_per_thread_data();
    if (data->se_buffer)
    {
        if (data->se_len >= size) return data->se_buffer;
        HeapFree( GetProcessHeap(), 0, data->se_buffer );
    }
    data->se_len = size;
    data->se_buffer = HeapAlloc( GetProcessHeap(), 0, size );
    if (!data->se_buffer) SetLastError( WSAENOBUFS );
    return data->se_buffer;
}

/* Parse the first valid line into a servent structure, returning NULL if
 * there is no valid line. Updates cursor to point to the start of the next
 * line or the end of the file. */
static struct servent *get_next_service( const char **cursor, const char *end )
{
    const char *p = *cursor;

    while (p < end)
    {
        const char *line_end, *next_line;
        size_t needed_size, line_len;
        unsigned int alias_count = 0;
        struct servent *serv;
        const char *name;
        int port;
        char *q;

        for (line_end = p; line_end < end && *line_end != '\n' && *line_end != '#'; ++line_end)
            ;
        TRACE( "parsing line %s\n", debugstr_an(p, line_end - p) );

        for (next_line = line_end; next_line < end && *next_line != '\n'; ++next_line)
            ;
        if (next_line < end)
            ++next_line; /* skip over the newline */

        p = next_non_space( p, line_end );
        if (p == line_end)
        {
            p = next_line;
            continue;
        }

        /* parse the name */

        name = p;
        line_len = line_end - name;

        p = next_space( p, line_end );
        if (p == line_end)
        {
            p = next_line;
            continue;
        }

        p = next_non_space( p, line_end );

        /* parse the port */

        port = atoi( p );
        p = memchr( p, '/', line_end - p );
        if (!p)
        {
            p = next_line;
            continue;
        }

        p = next_space( p, line_end );
        p = next_non_space( p, line_end );

        /* we will copy the entire line after the servent structure, then
         * replace spaces with null bytes as necessary */

        while (p < line_end)
        {
            ++alias_count;

            p = next_space( p, line_end );
            p = next_non_space( p, line_end );
        }
        needed_size = sizeof(*serv) + line_len + 1 + (alias_count + 1) * sizeof(char *);

        if (!(serv = get_servent_buffer( needed_size )))
        {
            SetLastError( WSAENOBUFS );
            return NULL;
        }

        serv->s_port = htons( port );
        serv->s_aliases = (char **)(serv + 1);
        serv->s_name = (char *)(serv->s_aliases + alias_count + 1);

        memcpy( serv->s_name, name, line_len );
        serv->s_name[line_len] = 0;

        line_end = serv->s_name + line_len;

        q = serv->s_name;
        q = next_space( q, line_end );
        *q++ = 0;
        q = next_non_space( q, line_end );
        /* skip over the number */
        q = memchr( q, '/', line_end - q );
        serv->s_proto = ++q;
        q = next_space( q, line_end );
        if (q < line_end) *q++ = 0;
        q = next_non_space( q, line_end );

        alias_count = 0;
        while (q < line_end)
        {
            serv->s_aliases[alias_count++] = q;
            q = next_space( q, line_end );
            if (q < line_end) *q++ = 0;
            q = next_non_space( q, line_end );
        }
        serv->s_aliases[alias_count] = NULL;

        *cursor = next_line;
        return serv;
    }

    SetLastError( WSANO_DATA );
    return NULL;
}


/***********************************************************************
 *      getservbyname   (ws2_32.55)
 */
struct servent * WINAPI getservbyname( const char *name, const char *proto )
{
    struct servent *serv;
    const char *cursor;
    char *file;
    DWORD size;

    TRACE( "name %s, proto %s\n", debugstr_a(name), debugstr_a(proto) );

    if (!(file = read_etc_file( L"services", &size )))
    {
        SetLastError( WSANO_DATA );
        return NULL;
    }

    cursor = file;
    while ((serv = get_next_service( &cursor, file + size )))
    {
        if (!strcasecmp( serv->s_name, name ) && (!proto || !strcasecmp( serv->s_proto, proto )))
            break;
    }

    HeapFree( GetProcessHeap(), 0, file );
    return serv;
}


/***********************************************************************
 *      getservbyport   (ws2_32.56)
 */
struct servent * WINAPI getservbyport( int port, const char *proto )
{
    struct servent *serv;
    const char *cursor;
    char *file;
    DWORD size;

    TRACE( "port %d, proto %s\n", port, debugstr_a(proto) );

    if (!(file = read_etc_file( L"services", &size )))
    {
        SetLastError( WSANO_DATA );
        return NULL;
    }

    cursor = file;
    while ((serv = get_next_service( &cursor, file + size )))
    {
        if (serv->s_port == port && (!proto || !strcasecmp( serv->s_proto, proto )))
            break;
    }

    HeapFree( GetProcessHeap(), 0, file );
    return serv;
}


/***********************************************************************
 *      inet_ntoa   (ws2_32.12)
 */
char * WINAPI inet_ntoa( struct in_addr in )
{
    unsigned int long_ip = ntohl( in.s_addr );
    struct per_thread_data *data = get_per_thread_data();

    sprintf( data->ntoa_buffer, "%u.%u.%u.%u",
             (long_ip >> 24) & 0xff,
             (long_ip >> 16) & 0xff,
             (long_ip >> 8) & 0xff,
             long_ip & 0xff );

    return data->ntoa_buffer;
}


/***********************************************************************
 *      inet_ntop   (ws2_32.@)
 */
const char * WINAPI inet_ntop( int family, void *addr, char *buffer, SIZE_T len )
{
    NTSTATUS status;
    ULONG size = min( len, (ULONG)-1 );

    TRACE( "family %d, addr %p, buffer %p, len %ld\n", family, addr, buffer, len );
    if (!buffer)
    {
        SetLastError( STATUS_INVALID_PARAMETER );
        return NULL;
    }

    switch (family)
    {
    case AF_INET:
    {
        status = RtlIpv4AddressToStringExA( (IN_ADDR *)addr, 0, buffer, &size );
        break;
    }
    case AF_INET6:
    {
        status = RtlIpv6AddressToStringExA( (IN6_ADDR *)addr, 0, 0, buffer, &size );
        break;
    }
    default:
        SetLastError( WSAEAFNOSUPPORT );
        return NULL;
    }

    if (status == STATUS_SUCCESS) return buffer;
    SetLastError( STATUS_INVALID_PARAMETER );
    return NULL;
}

/***********************************************************************
 *      inet_pton   (ws2_32.@)
 */
int WINAPI inet_pton( int family, const char *addr, void *buffer )
{
    NTSTATUS status;
    const char *terminator;

    TRACE( "family %d, addr %s, buffer %p\n", family, debugstr_a(addr), buffer );

    if (!addr || !buffer)
    {
        SetLastError( WSAEFAULT );
        return -1;
    }

    switch (family)
    {
    case AF_INET:
        status = RtlIpv4StringToAddressA(addr, TRUE, &terminator, buffer);
        break;
    case AF_INET6:
        status = RtlIpv6StringToAddressA(addr, &terminator, buffer);
        break;
    default:
        SetLastError( WSAEAFNOSUPPORT );
        return -1;
    }

    return (status == STATUS_SUCCESS && *terminator == 0);
}

/***********************************************************************
 *      InetPtonW   (ws2_32.@)
 */
int WINAPI InetPtonW( int family, const WCHAR *addr, void *buffer )
{
    char *addrA;
    int len;
    INT ret;

    TRACE( "family %d, addr %s, buffer %p\n", family, debugstr_w(addr), buffer );

    if (!addr)
    {
        SetLastError(WSAEFAULT);
        return SOCKET_ERROR;
    }

    len = WideCharToMultiByte( CP_ACP, 0, addr, -1, NULL, 0, NULL, NULL );
    if (!(addrA = HeapAlloc( GetProcessHeap(), 0, len )))
    {
        SetLastError( WSA_NOT_ENOUGH_MEMORY );
        return -1;
    }
    WideCharToMultiByte( CP_ACP, 0, addr, -1, addrA, len, NULL, NULL );

    ret = inet_pton( family, addrA, buffer );
    if (!ret) SetLastError( WSAEINVAL );

    HeapFree( GetProcessHeap(), 0, addrA );
    return ret;
}

/***********************************************************************
 *      InetNtopW   (ws2_32.@)
 */
const WCHAR * WINAPI InetNtopW( int family, void *addr, WCHAR *buffer, SIZE_T len )
{
    char bufferA[INET6_ADDRSTRLEN];
    PWSTR ret = NULL;

    TRACE( "family %d, addr %p, buffer %p, len %ld\n", family, addr, buffer, len );

    if (inet_ntop( family, addr, bufferA, sizeof(bufferA) ))
    {
        if (MultiByteToWideChar( CP_ACP, 0, bufferA, -1, buffer, len ))
            ret = buffer;
        else
            SetLastError( ERROR_INVALID_PARAMETER );
    }
    return ret;
}


/***********************************************************************
 *      WSAStringToAddressA   (ws2_32.@)
 */
int WINAPI WSAStringToAddressA( char *string, int family, WSAPROTOCOL_INFOA *protocol_info,
                                struct sockaddr *addr, int *addr_len )
{
    NTSTATUS status;

    TRACE( "string %s, family %u\n", debugstr_a(string), family );

    if (!addr || !addr_len) return -1;

    if (!string)
    {
        SetLastError( WSAEINVAL );
        return -1;
    }

    if (protocol_info)
        FIXME( "ignoring protocol_info\n" );

    switch (family)
    {
    case AF_INET:
    {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;

        if (*addr_len < sizeof(struct sockaddr_in))
        {
            *addr_len = sizeof(struct sockaddr_in);
            SetLastError( WSAEFAULT );
            return -1;
        }
        memset( addr, 0, sizeof(struct sockaddr_in) );

        status = RtlIpv4StringToAddressExA( string, FALSE, &addr4->sin_addr, &addr4->sin_port );
        if (status != STATUS_SUCCESS)
        {
            SetLastError( WSAEINVAL );
            return -1;
        }
        addr4->sin_family = AF_INET;
        *addr_len = sizeof(struct sockaddr_in);
        return 0;
    }
    case AF_INET6:
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;

        if (*addr_len < sizeof(struct sockaddr_in6))
        {
            *addr_len = sizeof(struct sockaddr_in6);
            SetLastError( WSAEFAULT );
            return -1;
        }
        memset( addr, 0, sizeof(struct sockaddr_in6) );

        status = RtlIpv6StringToAddressExA( string, &addr6->sin6_addr, &addr6->sin6_scope_id, &addr6->sin6_port );
        if (status != STATUS_SUCCESS)
        {
            SetLastError( WSAEINVAL );
            return -1;
        }
        addr6->sin6_family = AF_INET6;
        *addr_len = sizeof(struct sockaddr_in6);
        return 0;
    }
    default:
        /* According to MSDN, only AF_INET and AF_INET6 are supported. */
        TRACE( "Unsupported address family specified: %d.\n", family );
        SetLastError( WSAEINVAL );
        return -1;
    }
}


/***********************************************************************
 *      WSAStringToAddressW   (ws2_32.@)
 */
int WINAPI WSAStringToAddressW( WCHAR *string, int family, WSAPROTOCOL_INFOW *protocol_info,
                                struct sockaddr *addr, int *addr_len )
{
    WSAPROTOCOL_INFOA infoA;
    WSAPROTOCOL_INFOA *protocol_infoA = NULL;
    int sizeA, ret;
    char *stringA;

    TRACE( "string %s, family %u\n", debugstr_w(string), family );

    if (!addr || !addr_len) return -1;

    if (protocol_info)
    {
        protocol_infoA = &infoA;
        memcpy( protocol_infoA, protocol_info, FIELD_OFFSET( WSAPROTOCOL_INFOA, szProtocol ) );

        if (!WideCharToMultiByte( CP_ACP, 0, protocol_info->szProtocol, -1, protocol_infoA->szProtocol,
                                  sizeof(protocol_infoA->szProtocol), NULL, NULL ))
        {
            SetLastError( WSAEINVAL );
            return -1;
        }
    }

    if (!string)
    {
        SetLastError( WSAEINVAL );
        return -1;
    }

    sizeA = WideCharToMultiByte( CP_ACP, 0, string, -1, NULL, 0, NULL, NULL );
    if (!(stringA = HeapAlloc( GetProcessHeap(), 0, sizeA )))
    {
        SetLastError( WSA_NOT_ENOUGH_MEMORY );
        return -1;
    }
    WideCharToMultiByte( CP_ACP, 0, string, -1, stringA, sizeA, NULL, NULL );
    ret = WSAStringToAddressA( stringA, family, protocol_infoA, addr, addr_len );
    HeapFree( GetProcessHeap(), 0, stringA );
    return ret;
}


/***********************************************************************
 *      WSAAddressToStringA   (ws2_32.@)
 */
int WINAPI WSAAddressToStringA( struct sockaddr *addr, DWORD addr_len,
                                WSAPROTOCOL_INFOA *info, char *string, DWORD *string_len )
{
    char buffer[54]; /* 32 digits + 7':' + '[' + '%" + 5 digits + ']:' + 5 digits + '\0' */
    DWORD size;

    TRACE( "addr %s\n", debugstr_sockaddr(addr) );

    if (!addr) return SOCKET_ERROR;
    if (!string || !string_len) return SOCKET_ERROR;

    switch (addr->sa_family)
    {
    case AF_INET:
    {
        const struct sockaddr_in *addr4 = (const struct sockaddr_in *)addr;
        unsigned int long_ip = ntohl( addr4->sin_addr.s_addr );
        char *p;

        if (addr_len < sizeof(struct sockaddr_in)) return -1;
        sprintf( buffer, "%u.%u.%u.%u:%u",
                 (long_ip >> 24) & 0xff,
                 (long_ip >> 16) & 0xff,
                 (long_ip >> 8) & 0xff,
                 long_ip & 0xff,
                 ntohs( addr4->sin_port ) );

        p = strchr( buffer, ':' );
        if (!addr4->sin_port) *p = 0;
        break;
    }
    case AF_INET6:
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        size_t len;

        buffer[0] = 0;
        if (addr_len < sizeof(struct sockaddr_in6)) return -1;
        if (addr6->sin6_port)
            strcpy( buffer, "[" );
        len = strlen( buffer );
        if (!inet_ntop( AF_INET6, &addr6->sin6_addr, &buffer[len], sizeof(buffer) - len ))
        {
            SetLastError( WSAEINVAL );
            return -1;
        }
        if (addr6->sin6_scope_id)
            sprintf( buffer + strlen( buffer ), "%%%u", addr6->sin6_scope_id );
        if (addr6->sin6_port)
            sprintf( buffer + strlen( buffer ), "]:%u", ntohs( addr6->sin6_port ) );
        break;
    }

    default:
        SetLastError( WSAEINVAL );
        return -1;
    }

    size = strlen( buffer ) + 1;

    if (*string_len < size)
    {
        *string_len = size;
        SetLastError( WSAEFAULT );
        return -1;
    }

    TRACE( "=> %s, %u bytes\n", debugstr_a(buffer), size );
    *string_len = size;
    strcpy( string, buffer );
    return 0;
}


/***********************************************************************
 *      WSAAddressToStringW   (ws2_32.@)
 */
int WINAPI WSAAddressToStringW( struct sockaddr *addr, DWORD addr_len,
                                WSAPROTOCOL_INFOW *info, WCHAR *string, DWORD *string_len )
{
    INT ret;
    char buf[54]; /* 32 digits + 7':' + '[' + '%" + 5 digits + ']:' + 5 digits + '\0' */

    TRACE( "(%p, %d, %p, %p, %p)\n", addr, addr_len, info, string, string_len );

    if ((ret = WSAAddressToStringA( addr, addr_len, NULL, buf, string_len ))) return ret;

    MultiByteToWideChar( CP_ACP, 0, buf, *string_len, string, *string_len );
    TRACE( "=> %s, %u chars\n", debugstr_w(string), *string_len );
    return 0;
}


/***********************************************************************
 *      inet_addr   (ws2_32.11)
 */
u_long WINAPI inet_addr( const char *str )
{
    u_long addr;

    if (inet_pton( AF_INET, str, &addr ) == 1)
        return addr;
    return INADDR_NONE;
}


/***********************************************************************
 *      htonl   (ws2_32.8)
 */
u_long WINAPI WS_htonl( u_long hostlong )
{
    return htonl( hostlong );
}


/***********************************************************************
 *      htons   (ws2_32.9)
 */
u_short WINAPI WS_htons( u_short hostshort )
{
    return htons( hostshort );
}


/***********************************************************************
 *      WSAHtonl   (ws2_32.@)
 */
int WINAPI WSAHtonl( SOCKET s, u_long hostlong, u_long *netlong )
{
    if (netlong)
    {
        *netlong = htonl( hostlong );
        return 0;
    }
    SetLastError( WSAEFAULT );
    return -1;
}


/***********************************************************************
 *      WSAHtons   (ws2_32.@)
 */
int WINAPI WSAHtons( SOCKET s, u_short hostshort, u_short *netshort )
{
    if (netshort)
    {
        *netshort = htons( hostshort );
        return 0;
    }
    SetLastError( WSAEFAULT );
    return -1;
}


/***********************************************************************
 *      ntohl   (ws2_32.14)
 */
u_long WINAPI WS_ntohl( u_long netlong )
{
    return ntohl( netlong );
}


/***********************************************************************
 *      ntohs   (ws2_32.15)
 */
u_short WINAPI WS_ntohs( u_short netshort )
{
    return ntohs( netshort );
}


/***********************************************************************
 *      WSANtohl   (ws2_32.@)
 */
int WINAPI WSANtohl( SOCKET s, u_long netlong, u_long *hostlong )
{
    if (!hostlong) return WSAEFAULT;

    *hostlong = ntohl( netlong );
    return 0;
}


/***********************************************************************
 *      WSANtohs   (ws2_32.@)
 */
int WINAPI WSANtohs( SOCKET s, u_short netshort, u_short *hostshort )
{
    if (!hostshort) return WSAEFAULT;

    *hostshort = ntohs( netshort );
    return 0;
}


/***********************************************************************
 *      WSAInstallServiceClassA   (ws2_32.@)
 */
int WINAPI WSAInstallServiceClassA( WSASERVICECLASSINFOA *info )
{
    FIXME( "Request to install service %s\n", debugstr_a(info->lpszServiceClassName) );
    SetLastError( WSAEACCES );
    return -1;
}


/***********************************************************************
 *      WSAInstallServiceClassW   (ws2_32.@)
 */
int WINAPI WSAInstallServiceClassW( WSASERVICECLASSINFOW *info )
{
    FIXME( "Request to install service %s\n", debugstr_w(info->lpszServiceClassName) );
    SetLastError( WSAEACCES );
    return -1;
}


/***********************************************************************
 *      WSARemoveServiceClass   (ws2_32.@)
 */
int WINAPI WSARemoveServiceClass( GUID *info )
{
    FIXME( "Request to remove service %s\n", debugstr_guid(info) );
    SetLastError( WSATYPE_NOT_FOUND );
    return -1;
}


/***********************************************************************
 *      WSAGetServiceClassInfoA   (ws2_32.@)
 */
int WINAPI WSAGetServiceClassInfoA( GUID *provider, GUID *service, DWORD *len,
                                    WSASERVICECLASSINFOA *info )
{
    FIXME( "(%s %s %p %p) Stub!\n", debugstr_guid(provider), debugstr_guid(service), len, info );
    SetLastError( WSA_NOT_ENOUGH_MEMORY );
    return -1;
}


/***********************************************************************
 *      WSAGetServiceClassInfoW   (ws2_32.@)
 */
int WINAPI WSAGetServiceClassInfoW( GUID *provider, GUID *service, DWORD *len,
                                    WSASERVICECLASSINFOW *info )
{
    FIXME( "(%s %s %p %p) Stub!\n", debugstr_guid(provider), debugstr_guid(service), len, info );
    SetLastError( WSA_NOT_ENOUGH_MEMORY );
    return -1;
}


/***********************************************************************
 *      WSAGetServiceClassNameByClassIdA   (ws2_32.@)
 */
int WINAPI WSAGetServiceClassNameByClassIdA( GUID *class, char *service, DWORD *len )
{
    FIXME( "(%s %p %p) Stub!\n", debugstr_guid(class), service, len );
    SetLastError( WSA_NOT_ENOUGH_MEMORY );
    return -1;
}


/***********************************************************************
 *      WSAGetServiceClassNameByClassIdW   (ws2_32.@)
 */
int WINAPI WSAGetServiceClassNameByClassIdW( GUID *class, WCHAR *service, DWORD *len )
{
    FIXME( "(%s %p %p) Stub!\n", debugstr_guid(class), service, len );
    SetLastError( WSA_NOT_ENOUGH_MEMORY );
    return -1;
}


/***********************************************************************
 *      WSALookupServiceBeginA   (ws2_32.@)
 */
int WINAPI WSALookupServiceBeginA( WSAQUERYSETA *query, DWORD flags, HANDLE *lookup )
{
    FIXME( "(%p 0x%08x %p) Stub!\n", query, flags, lookup );
    SetLastError( WSA_NOT_ENOUGH_MEMORY );
    return -1;
}


/***********************************************************************
 *      WSALookupServiceBeginW   (ws2_32.@)
 */
int WINAPI WSALookupServiceBeginW( WSAQUERYSETW *query, DWORD flags, HANDLE *lookup )
{
    FIXME( "(%p 0x%08x %p) Stub!\n", query, flags, lookup );
    SetLastError( WSA_NOT_ENOUGH_MEMORY );
    return -1;
}


/***********************************************************************
 *      WSALookupServiceEnd   (ws2_32.@)
 */
int WINAPI WSALookupServiceEnd( HANDLE lookup )
{
    FIXME("(%p) Stub!\n", lookup );
    return 0;
}


/***********************************************************************
 *      WSALookupServiceNextA   (ws2_32.@)
 */
int WINAPI WSALookupServiceNextA( HANDLE lookup, DWORD flags, DWORD *len, WSAQUERYSETA *results )
{
    FIXME( "(%p 0x%08x %p %p) Stub!\n", lookup, flags, len, results );
    SetLastError( WSA_E_NO_MORE );
    return -1;
}


/***********************************************************************
 *      WSALookupServiceNextW   (ws2_32.@)
 */
int WINAPI WSALookupServiceNextW( HANDLE lookup, DWORD flags, DWORD *len, WSAQUERYSETW *results )
{
    FIXME( "(%p 0x%08x %p %p) Stub!\n", lookup, flags, len, results );
    SetLastError( WSA_E_NO_MORE );
    return -1;
}


/***********************************************************************
 *      WSASetServiceA   (ws2_32.@)
 */
int WINAPI WSASetServiceA( WSAQUERYSETA *query, WSAESETSERVICEOP operation, DWORD flags )
{
    FIXME( "(%p 0x%08x 0x%08x) Stub!\n", query, operation, flags );
    return 0;
}


/***********************************************************************
 *      WSASetServiceW   (ws2_32.@)
 */
int WINAPI WSASetServiceW( WSAQUERYSETW *query, WSAESETSERVICEOP operation, DWORD flags )
{
    FIXME( "(%p 0x%08x 0x%08x) Stub!\n", query, operation, flags );
    return 0;
}


/***********************************************************************
 *      WSAEnumNameSpaceProvidersA   (ws2_32.@)
 */
int WINAPI WSAEnumNameSpaceProvidersA( DWORD *len, WSANAMESPACE_INFOA *buffer )
{
    FIXME( "(%p %p) Stub!\n", len, buffer );
    return 0;
}


/***********************************************************************
 *      WSAEnumNameSpaceProvidersW   (ws2_32.@)
 */
int WINAPI WSAEnumNameSpaceProvidersW( DWORD *len, WSANAMESPACE_INFOW *buffer )
{
    FIXME( "(%p %p) Stub!\n", len, buffer );
    return 0;
}


/***********************************************************************
 *      WSAProviderConfigChange   (ws2_32.@)
 */
int WINAPI WSAProviderConfigChange( HANDLE *handle, OVERLAPPED *overlapped,
                                    LPWSAOVERLAPPED_COMPLETION_ROUTINE completion )
{
    FIXME( "(%p %p %p) Stub!\n", handle, overlapped, completion );
    return -1;
}


/***********************************************************************
 *      WSANSPIoctl   (ws2_32.@)
 */
int WINAPI WSANSPIoctl( HANDLE lookup, DWORD code, void *in_buffer,
                        DWORD in_size, void *out_buffer, DWORD out_size,
                        DWORD *ret_size, WSACOMPLETION *completion )
{
    FIXME( "(%p, 0x%08x, %p, 0x%08x, %p, 0x%08x, %p, %p) Stub!\n", lookup, code,
           in_buffer, in_size, out_buffer, out_size, ret_size, completion );
    SetLastError( WSA_NOT_ENOUGH_MEMORY );
    return -1;
}


/***********************************************************************
 *      WSCEnableNSProvider   (ws2_32.@)
 */
int WINAPI WSCEnableNSProvider( GUID *provider, BOOL enable )
{
    FIXME( "(%s 0x%08x) Stub!\n", debugstr_guid(provider), enable );
    return 0;
}


/***********************************************************************
 *      WSCGetProviderInfo   (ws2_32.@)
 */
int WINAPI WSCGetProviderInfo( GUID *provider, WSC_PROVIDER_INFO_TYPE info_type,
                               BYTE *info, size_t *len, DWORD flags, int *errcode )
{
    FIXME( "(%s 0x%08x %p %p 0x%08x %p) Stub!\n",
           debugstr_guid(provider), info_type, info, len, flags, errcode );

    if (!errcode)
        return -1;

    if (!provider)
    {
        *errcode = WSAEFAULT;
        return -1;
    }

    *errcode = WSANO_RECOVERY;
    return -1;
}


/***********************************************************************
 *      WSCGetProviderPath   (ws2_32.@)
 */
int WINAPI WSCGetProviderPath( GUID *provider, WCHAR *path, int *len, int *errcode )
{
    FIXME( "(%s %p %p %p) Stub!\n", debugstr_guid(provider), path, len, errcode );

    if (!provider || !len)
    {
        if (errcode)
            *errcode = WSAEFAULT;
        return -1;
    }

    if (*len <= 0)
    {
        if (errcode)
            *errcode = WSAEINVAL;
        return -1;
    }

    return 0;
}


/***********************************************************************
 *      WSCInstallNameSpace   (ws2_32.@)
 */
int WINAPI WSCInstallNameSpace( WCHAR *identifier, WCHAR *path, DWORD namespace,
                                DWORD version, GUID *provider )
{
    FIXME( "(%s %s 0x%08x 0x%08x %s) Stub!\n", debugstr_w(identifier), debugstr_w(path),
           namespace, version, debugstr_guid(provider) );
    return 0;
}


/***********************************************************************
 *      WSCUnInstallNameSpace   (ws2_32.@)
 */
int WINAPI WSCUnInstallNameSpace( GUID *provider )
{
    FIXME( "(%s) Stub!\n", debugstr_guid(provider) );
    return NO_ERROR;
}


/***********************************************************************
 *      WSCWriteProviderOrder   (ws2_32.@)
 */
int WINAPI WSCWriteProviderOrder( DWORD *entry, DWORD number )
{
    FIXME( "(%p 0x%08x) Stub!\n", entry, number );
    return 0;
}


/***********************************************************************
 *      WSCInstallProvider   (ws2_32.@)
 */
int WINAPI WSCInstallProvider( GUID *provider, const WCHAR *path,
                               WSAPROTOCOL_INFOW *protocol_info, DWORD count, int *err )
{
    FIXME( "(%s, %s, %p, %d, %p): stub !\n", debugstr_guid(provider),
           debugstr_w(path), protocol_info, count, err );
    *err = 0;
    return 0;
}


/***********************************************************************
 *      WSCDeinstallProvider   (ws2_32.@)
 */
int WINAPI WSCDeinstallProvider( GUID *provider, int *err )
{
    FIXME( "(%s, %p): stub !\n", debugstr_guid(provider), err );
    *err = 0;
    return 0;
}


/***********************************************************************
 *      WSCSetApplicationCategory   (ws2_32.@)
 */
int WINAPI WSCSetApplicationCategory( const WCHAR *path, DWORD len, const WCHAR *extra, DWORD extralen,
                                      DWORD lspcat, DWORD *prev_lspcat, int *err )
{
    FIXME( "(%s %d %s %d %d %p) Stub!\n", debugstr_w(path), len, debugstr_w(extra),
           extralen, lspcat, prev_lspcat );
    return 0;
}


/***********************************************************************
 *      WSCEnumProtocols   (ws2_32.@)
 */
int WINAPI WSCEnumProtocols( int *protocols, WSAPROTOCOL_INFOW *info, DWORD *len, int *err )
{
    int ret = WSAEnumProtocolsW( protocols, info, len );

    if (ret == SOCKET_ERROR) *err = WSAENOBUFS;

    return ret;
}
