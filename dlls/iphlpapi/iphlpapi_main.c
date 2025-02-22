/*
 * iphlpapi dll implementation
 *
 * Copyright (C) 2003,2006 Juan Lang
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

#include "config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#define USE_WS_PREFIX
#include "winsock2.h"
#include "winternl.h"
#include "ws2ipdef.h"
#include "windns.h"
#include "iphlpapi.h"
#include "ifenum.h"
#include "ipstats.h"
#include "ipifcons.h"
#include "fltdefs.h"
#include "ifdef.h"
#include "netioapi.h"
#include "tcpestats.h"
#include "ip2string.h"
#include "netiodef.h"

#include "wine/nsi.h"
#include "wine/debug.h"
#include "wine/unicode.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(iphlpapi);

#ifndef IF_NAMESIZE
#define IF_NAMESIZE 16
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ~0UL
#endif

#define CHARS_IN_GUID 39

DWORD WINAPI AllocateAndGetIfTableFromStack( MIB_IFTABLE **table, BOOL sort, HANDLE heap, DWORD flags );
DWORD WINAPI AllocateAndGetIpAddrTableFromStack( MIB_IPADDRTABLE **table, BOOL sort, HANDLE heap, DWORD flags );

static const NPI_MODULEID *ip_module_id( USHORT family )
{
    if (family == WS_AF_INET) return &NPI_MS_IPV4_MODULEID;
    if (family == WS_AF_INET6) return &NPI_MS_IPV6_MODULEID;
    return NULL;
}

DWORD WINAPI ConvertGuidToStringA( const GUID *guid, char *str, DWORD len )
{
    if (len < CHARS_IN_GUID) return ERROR_INSUFFICIENT_BUFFER;
    sprintf( str, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1], guid->Data4[2],
             guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7] );
    return ERROR_SUCCESS;
}

DWORD WINAPI ConvertGuidToStringW( const GUID *guid, WCHAR *str, DWORD len )
{
    static const WCHAR fmt[] = { '{','%','0','8','X','-','%','0','4','X','-','%','0','4','X','-',
                                 '%','0','2','X','%','0','2','X','-','%','0','2','X','%','0','2','X',
                                 '%','0','2','X','%','0','2','X','%','0','2','X','%','0','2','X','}',0 };

    if (len < CHARS_IN_GUID) return ERROR_INSUFFICIENT_BUFFER;
    sprintfW( str, fmt,
              guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1], guid->Data4[2],
              guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7] );
    return ERROR_SUCCESS;
}

DWORD WINAPI ConvertStringToGuidW( const WCHAR *str, GUID *guid )
{
    UNICODE_STRING ustr;

    RtlInitUnicodeString( &ustr, str );
    return RtlNtStatusToDosError( RtlGUIDFromString( &ustr, guid ) );
}

static void if_counted_string_copy( WCHAR *dst, unsigned int len, IF_COUNTED_STRING *src )
{
    unsigned int copy = src->Length;

    if (copy >= len * sizeof(WCHAR)) copy = 0;
    memcpy( dst, src->String, copy );
    memset( (char *)dst + copy, 0, len * sizeof(WCHAR) - copy );
}

/******************************************************************
 *    AddIPAddress (IPHLPAPI.@)
 *
 * Add an IP address to an adapter.
 *
 * PARAMS
 *  Address     [In]  IP address to add to the adapter
 *  IpMask      [In]  subnet mask for the IP address
 *  IfIndex     [In]  adapter index to add the address
 *  NTEContext  [Out] Net Table Entry (NTE) context for the IP address
 *  NTEInstance [Out] NTE instance for the IP address
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub. Currently returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI AddIPAddress(IPAddr Address, IPMask IpMask, DWORD IfIndex, PULONG NTEContext, PULONG NTEInstance)
{
  FIXME(":stub\n");
  return ERROR_NOT_SUPPORTED;
}

/******************************************************************
 *    CancelIPChangeNotify (IPHLPAPI.@)
 *
 * Cancel a previous notification created by NotifyAddrChange or
 * NotifyRouteChange.
 *
 * PARAMS
 *  overlapped [In]  overlapped structure that notifies the caller
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * FIXME
 *  Stub, returns FALSE.
 */
BOOL WINAPI CancelIPChangeNotify(LPOVERLAPPED overlapped)
{
  FIXME("(overlapped %p): stub\n", overlapped);
  return FALSE;
}


/******************************************************************
 *    CancelMibChangeNotify2 (IPHLPAPI.@)
 */
DWORD WINAPI CancelMibChangeNotify2(HANDLE handle)
{
    FIXME("(handle %p): stub\n", handle);
    return NO_ERROR;
}


/******************************************************************
 *    CreateIpForwardEntry (IPHLPAPI.@)
 *
 * Create a route in the local computer's IP table.
 *
 * PARAMS
 *  pRoute [In] new route information
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, always returns NO_ERROR.
 */
DWORD WINAPI CreateIpForwardEntry(PMIB_IPFORWARDROW pRoute)
{
  FIXME("(pRoute %p): stub\n", pRoute);
  /* could use SIOCADDRT, not sure I want to */
  return 0;
}


/******************************************************************
 *    CreateIpNetEntry (IPHLPAPI.@)
 *
 * Create entry in the ARP table.
 *
 * PARAMS
 *  pArpEntry [In] new ARP entry
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, always returns NO_ERROR.
 */
DWORD WINAPI CreateIpNetEntry(PMIB_IPNETROW pArpEntry)
{
  FIXME("(pArpEntry %p)\n", pArpEntry);
  /* could use SIOCSARP on systems that support it, not sure I want to */
  return 0;
}


/******************************************************************
 *    CreateProxyArpEntry (IPHLPAPI.@)
 *
 * Create a Proxy ARP (PARP) entry for an IP address.
 *
 * PARAMS
 *  dwAddress [In] IP address for which this computer acts as a proxy. 
 *  dwMask    [In] subnet mask for dwAddress
 *  dwIfIndex [In] interface index
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI CreateProxyArpEntry(DWORD dwAddress, DWORD dwMask, DWORD dwIfIndex)
{
  FIXME("(dwAddress 0x%08x, dwMask 0x%08x, dwIfIndex 0x%08x): stub\n",
   dwAddress, dwMask, dwIfIndex);
  return ERROR_NOT_SUPPORTED;
}

static char *debugstr_ipv6(const struct WS_sockaddr_in6 *sin, char *buf)
{
    const IN6_ADDR *addr = &sin->sin6_addr;
    char *p = buf;
    int i;
    BOOL in_zero = FALSE;

    for (i = 0; i < 7; i++)
    {
        if (!addr->u.Word[i])
        {
            if (i == 0)
                *p++ = ':';
            if (!in_zero)
            {
                *p++ = ':';
                in_zero = TRUE;
            }
        }
        else
        {
            p += sprintf(p, "%x:", ntohs(addr->u.Word[i]));
            in_zero = FALSE;
        }
    }
    sprintf(p, "%x", ntohs(addr->u.Word[7]));
    return buf;
}

static BOOL map_address_6to4( const SOCKADDR_IN6 *addr6, SOCKADDR_IN *addr4 )
{
    ULONG i;

    if (addr6->sin6_family != WS_AF_INET6) return FALSE;

    for (i = 0; i < 5; i++)
        if (addr6->sin6_addr.u.Word[i]) return FALSE;

    if (addr6->sin6_addr.u.Word[5] != 0xffff) return FALSE;

    addr4->sin_family = WS_AF_INET;
    addr4->sin_port   = addr6->sin6_port;
    addr4->sin_addr.S_un.S_addr = addr6->sin6_addr.u.Word[6] << 16 | addr6->sin6_addr.u.Word[7];
    memset( &addr4->sin_zero, 0, sizeof(addr4->sin_zero) );

    return TRUE;
}

static BOOL find_src_address( MIB_IPADDRTABLE *table, const SOCKADDR_IN *dst, SOCKADDR_IN6 *src )
{
    MIB_IPFORWARDROW row;
    DWORD i, j;

    if (GetBestRoute( dst->sin_addr.S_un.S_addr, 0, &row )) return FALSE;

    for (i = 0; i < table->dwNumEntries; i++)
    {
        /* take the first address */
        if (table->table[i].dwIndex == row.dwForwardIfIndex)
        {
            src->sin6_family   = WS_AF_INET6;
            src->sin6_port     = 0;
            src->sin6_flowinfo = 0;
            for (j = 0; j < 5; j++) src->sin6_addr.u.Word[j] = 0;
            src->sin6_addr.u.Word[5] = 0xffff;
            src->sin6_addr.u.Word[6] = table->table[i].dwAddr & 0xffff;
            src->sin6_addr.u.Word[7] = table->table[i].dwAddr >> 16;
            return TRUE;
        }
    }

    return FALSE;
}

/******************************************************************
 *    CreateSortedAddressPairs (IPHLPAPI.@)
 */
DWORD WINAPI CreateSortedAddressPairs( const PSOCKADDR_IN6 src_list, DWORD src_count,
                                       const PSOCKADDR_IN6 dst_list, DWORD dst_count,
                                       DWORD options, PSOCKADDR_IN6_PAIR *pair_list,
                                       DWORD *pair_count )
{
    DWORD i, size, ret;
    SOCKADDR_IN6_PAIR *pairs;
    SOCKADDR_IN6 *ptr;
    SOCKADDR_IN addr4;
    MIB_IPADDRTABLE *table;

    FIXME( "(src_list %p src_count %u dst_list %p dst_count %u options %x pair_list %p pair_count %p): stub\n",
           src_list, src_count, dst_list, dst_count, options, pair_list, pair_count );

    if (src_list || src_count || !dst_list || !pair_list || !pair_count || dst_count > 500)
        return ERROR_INVALID_PARAMETER;

    for (i = 0; i < dst_count; i++)
    {
        if (!map_address_6to4( &dst_list[i], &addr4 ))
        {
            FIXME("only mapped IPv4 addresses are supported\n");
            return ERROR_NOT_SUPPORTED;
        }
    }

    size = dst_count * sizeof(*pairs);
    size += dst_count * sizeof(SOCKADDR_IN6) * 2; /* source address + destination address */
    if (!(pairs = HeapAlloc( GetProcessHeap(), 0, size ))) return ERROR_NOT_ENOUGH_MEMORY;
    ptr = (SOCKADDR_IN6 *)&pairs[dst_count];

    if ((ret = AllocateAndGetIpAddrTableFromStack( &table, FALSE, GetProcessHeap(), 0 )))
    {
        HeapFree( GetProcessHeap(), 0, pairs );
        return ret;
    }

    for (i = 0; i < dst_count; i++)
    {
        pairs[i].SourceAddress = ptr++;
        if (!map_address_6to4( &dst_list[i], &addr4 ) ||
            !find_src_address( table, &addr4, pairs[i].SourceAddress ))
        {
            char buf[46];
            FIXME( "source address for %s not found\n", debugstr_ipv6(&dst_list[i], buf) );
            memset( pairs[i].SourceAddress, 0, sizeof(*pairs[i].SourceAddress) );
            pairs[i].SourceAddress->sin6_family = WS_AF_INET6;
        }

        pairs[i].DestinationAddress = ptr++;
        memcpy( pairs[i].DestinationAddress, &dst_list[i], sizeof(*pairs[i].DestinationAddress) );
    }
    *pair_list = pairs;
    *pair_count = dst_count;

    HeapFree( GetProcessHeap(), 0, table );
    return NO_ERROR;
}


/******************************************************************
 *    DeleteIPAddress (IPHLPAPI.@)
 *
 * Delete an IP address added with AddIPAddress().
 *
 * PARAMS
 *  NTEContext [In] NTE context from AddIPAddress();
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI DeleteIPAddress(ULONG NTEContext)
{
  FIXME("(NTEContext %d): stub\n", NTEContext);
  return ERROR_NOT_SUPPORTED;
}


/******************************************************************
 *    DeleteIpForwardEntry (IPHLPAPI.@)
 *
 * Delete a route.
 *
 * PARAMS
 *  pRoute [In] route to delete
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns NO_ERROR.
 */
DWORD WINAPI DeleteIpForwardEntry(PMIB_IPFORWARDROW pRoute)
{
  FIXME("(pRoute %p): stub\n", pRoute);
  /* could use SIOCDELRT, not sure I want to */
  return 0;
}


/******************************************************************
 *    DeleteIpNetEntry (IPHLPAPI.@)
 *
 * Delete an ARP entry.
 *
 * PARAMS
 *  pArpEntry [In] ARP entry to delete
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns NO_ERROR.
 */
DWORD WINAPI DeleteIpNetEntry(PMIB_IPNETROW pArpEntry)
{
  FIXME("(pArpEntry %p): stub\n", pArpEntry);
  /* could use SIOCDARP on systems that support it, not sure I want to */
  return 0;
}


/******************************************************************
 *    DeleteProxyArpEntry (IPHLPAPI.@)
 *
 * Delete a Proxy ARP entry.
 *
 * PARAMS
 *  dwAddress [In] IP address for which this computer acts as a proxy. 
 *  dwMask    [In] subnet mask for dwAddress
 *  dwIfIndex [In] interface index
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI DeleteProxyArpEntry(DWORD dwAddress, DWORD dwMask, DWORD dwIfIndex)
{
  FIXME("(dwAddress 0x%08x, dwMask 0x%08x, dwIfIndex 0x%08x): stub\n",
   dwAddress, dwMask, dwIfIndex);
  return ERROR_NOT_SUPPORTED;
}


/******************************************************************
 *    EnableRouter (IPHLPAPI.@)
 *
 * Turn on ip forwarding.
 *
 * PARAMS
 *  pHandle     [In/Out]
 *  pOverlapped [In/Out] hEvent member should contain a valid handle.
 *
 * RETURNS
 *  Success: ERROR_IO_PENDING
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI EnableRouter(HANDLE * pHandle, OVERLAPPED * pOverlapped)
{
  FIXME("(pHandle %p, pOverlapped %p): stub\n", pHandle, pOverlapped);
  /* could echo "1" > /proc/net/sys/net/ipv4/ip_forward, not sure I want to
     could map EACCESS to ERROR_ACCESS_DENIED, I suppose
   */
  return ERROR_NOT_SUPPORTED;
}


/******************************************************************
 *    FlushIpNetTable (IPHLPAPI.@)
 *
 * Delete all ARP entries of an interface
 *
 * PARAMS
 *  dwIfIndex [In] interface index
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI FlushIpNetTable(DWORD dwIfIndex)
{
  FIXME("(dwIfIndex 0x%08x): stub\n", dwIfIndex);
  /* this flushes the arp cache of the given index */
  return ERROR_NOT_SUPPORTED;
}

/******************************************************************
 *    FreeMibTable (IPHLPAPI.@)
 *
 * Free buffer allocated by network functions
 *
 * PARAMS
 *  ptr     [In] pointer to the buffer to free
 *
 */
void WINAPI FreeMibTable(void *ptr)
{
  TRACE("(%p)\n", ptr);
  HeapFree(GetProcessHeap(), 0, ptr);
}

/******************************************************************
 *    GetAdapterIndex (IPHLPAPI.@)
 *
 * Get interface index from its name.
 *
 * PARAMS
 *  adapter_name [In]  unicode string with the adapter name
 *  index        [Out] returns found interface index
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetAdapterIndex( WCHAR *adapter_name, ULONG *index )
{
    MIB_IFTABLE *if_table;
    DWORD err, i;

    TRACE( "name %s, index %p\n", debugstr_w( adapter_name ), index );

    err = AllocateAndGetIfTableFromStack( &if_table, 0, GetProcessHeap(), 0 );
    if (err) return err;

    err = ERROR_INVALID_PARAMETER;
    for (i = 0; i < if_table->dwNumEntries; i++)
    {
        if (!strcmpW( adapter_name, if_table->table[i].wszName ))
        {
            *index = if_table->table[i].dwIndex;
            err = ERROR_SUCCESS;
            break;
        }
    }
    heap_free( if_table );
    return err;
}

static DWORD get_wins_servers( SOCKADDR_INET **servers )
{
    HKEY key;
    char buf[4 * 4];
    DWORD size, i, count = 0;
    static const char *values[] = { "WinsServer", "BackupWinsServer" };
    IN_ADDR addrs[ARRAY_SIZE(values)];

    *servers = NULL;
    /* @@ Wine registry key: HKCU\Software\Wine\Network */
    if (RegOpenKeyA( HKEY_CURRENT_USER, "Software\\Wine\\Network", &key )) return 0;

    for (i = 0; i < ARRAY_SIZE(values); i++)
    {
        size = sizeof(buf);
        if (!RegQueryValueExA( key, values[i], NULL, NULL, (LPBYTE)buf, &size ))
            if (!RtlIpv4StringToAddressA( buf, TRUE, NULL, addrs + count ) &&
                addrs[count].WS_s_addr != INADDR_NONE && addrs[count].WS_s_addr != INADDR_ANY)
                count++;
    }
    RegCloseKey( key );

    if (count)
    {
        *servers = heap_alloc_zero( count * sizeof(**servers) );
        if (!*servers) return 0;
        for (i = 0; i < count; i++)
        {
            (*servers)[i].Ipv4.sin_family = WS_AF_INET;
            (*servers)[i].Ipv4.sin_addr = addrs[i];
        }
    }
    return count;
}

static void ip_addr_string_init( IP_ADDR_STRING *s, const IN_ADDR *addr, const IN_ADDR *mask, DWORD ctxt )
{
    s->Next = NULL;

    if (addr) RtlIpv4AddressToStringA( addr, s->IpAddress.String );
    else s->IpAddress.String[0] = '\0';
    if (mask) RtlIpv4AddressToStringA( mask, s->IpMask.String );
    else s->IpMask.String[0] = '\0';
    s->Context = ctxt;
}

/******************************************************************
 *    GetAdaptersInfo (IPHLPAPI.@)
 *
 * Get information about adapters.
 *
 * PARAMS
 *  info [Out] buffer for adapter infos
 *  size [In]  length of output buffer
 */
DWORD WINAPI GetAdaptersInfo( IP_ADAPTER_INFO *info, ULONG *size )
{
    DWORD err, if_count, if_num = 0, uni_count, fwd_count, needed, wins_server_count;
    DWORD len, i, uni, fwd;
    NET_LUID *if_keys = NULL;
    struct nsi_ndis_ifinfo_rw *if_rw = NULL;
    struct nsi_ndis_ifinfo_dynamic *if_dyn = NULL;
    struct nsi_ndis_ifinfo_static *if_stat = NULL;
    struct nsi_ipv4_unicast_key *uni_keys = NULL;
    struct nsi_ip_unicast_rw *uni_rw = NULL;
    struct nsi_ipv4_forward_key *fwd_keys = NULL;
    SOCKADDR_INET *wins_servers = NULL;
    IP_ADDR_STRING *extra_ip_addrs, *cursor;
    IN_ADDR gw, mask;

    TRACE( "info %p, size %p\n", info, size );
    if (!size) return ERROR_INVALID_PARAMETER;

    err = NsiAllocateAndGetTable( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE,
                                  (void **)&if_keys, sizeof(*if_keys), (void **)&if_rw, sizeof(*if_rw),
                                  (void **)&if_dyn, sizeof(*if_dyn), (void **)&if_stat, sizeof(*if_stat), &if_count, 0 );
    if (err) return err;
    for (i = 0; i < if_count; i++)
    {
        if (if_stat[i].type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if_num++;
    }

    if (!if_num)
    {
        err = ERROR_NO_DATA;
        goto err;
    }

    err = NsiAllocateAndGetTable( 1, &NPI_MS_IPV4_MODULEID, NSI_IP_UNICAST_TABLE,
                                  (void **)&uni_keys, sizeof(*uni_keys), (void **)&uni_rw, sizeof(*uni_rw),
                                  NULL, 0, NULL, 0, &uni_count, 0 );
    if (err) goto err;

    /* Slightly overestimate the needed size by assuming that all
       unicast addresses require a separate IP_ADDR_STRING. */

    needed = if_num * sizeof(*info) + uni_count * sizeof(IP_ADDR_STRING);
    if (!info || *size < needed)
    {
        *size = needed;
        err = ERROR_BUFFER_OVERFLOW;
        goto err;
    }

    err = NsiAllocateAndGetTable( 1, &NPI_MS_IPV4_MODULEID, NSI_IP_FORWARD_TABLE,
                                  (void **)&fwd_keys, sizeof(*fwd_keys), NULL, 0,
                                  NULL, 0, NULL, 0, &fwd_count, 0 );
    if (err) goto err;

    wins_server_count = get_wins_servers( &wins_servers );

    extra_ip_addrs = (IP_ADDR_STRING *)(info + if_num);
    for (i = 0; i < if_count; i++)
    {
        if (if_stat[i].type == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        info->Next = info + 1;
        info->ComboIndex = 0;
        ConvertGuidToStringA( &if_stat[i].if_guid, info->AdapterName, sizeof(info->AdapterName) );
        len = WideCharToMultiByte( CP_ACP, 0, if_stat[i].descr.String, if_stat[i].descr.Length / sizeof(WCHAR),
                                   info->Description, sizeof(info->Description) - 1, NULL, NULL );
        info->Description[len] = '\0';
        info->AddressLength = if_rw[i].phys_addr.Length;
        if (info->AddressLength > sizeof(info->Address)) info->AddressLength = 0;
        memcpy( info->Address, if_rw[i].phys_addr.Address, info->AddressLength );
        memset( info->Address + info->AddressLength, 0, sizeof(info->Address) - info->AddressLength );
        info->Index = if_stat[i].if_index;
        info->Type = if_stat[i].type;
        info->DhcpEnabled = TRUE; /* FIXME */
        info->CurrentIpAddress = NULL;

        cursor = NULL;
        for (uni = 0; uni < uni_count; uni++)
        {
            if (uni_keys[uni].luid.Value != if_keys[i].Value) continue;
            if (!cursor) cursor = &info->IpAddressList;
            else
            {
                cursor->Next = extra_ip_addrs++;
                cursor = cursor->Next;
            }
            ConvertLengthToIpv4Mask( uni_rw[uni].on_link_prefix, &mask.WS_s_addr );
            ip_addr_string_init( cursor, &uni_keys[uni].addr, &mask, 0 );
        }
        if (!cursor)
        {
            mask.WS_s_addr = INADDR_ANY;
            ip_addr_string_init( &info->IpAddressList, &mask, &mask, 0 );
        }

        gw.WS_s_addr = INADDR_ANY;
        mask.WS_s_addr = INADDR_NONE;
        for (fwd = 0; fwd < fwd_count; fwd++)
        { /* find the first router on this interface */
            if (fwd_keys[fwd].luid.Value == if_keys[i].Value &&
                fwd_keys[fwd].next_hop.WS_s_addr != INADDR_ANY &&
                !fwd_keys[fwd].prefix_len)
            {
                gw = fwd_keys[fwd].next_hop;
                break;
            }
        }
        ip_addr_string_init( &info->GatewayList, &gw, &mask, 0 );

        ip_addr_string_init( &info->DhcpServer, NULL, NULL, 0 );

        info->HaveWins = !!wins_server_count;
        ip_addr_string_init( &info->PrimaryWinsServer, NULL, NULL, 0 );
        ip_addr_string_init( &info->SecondaryWinsServer, NULL, NULL, 0 );
        if (info->HaveWins)
        {
            mask.WS_s_addr = INADDR_NONE;
            ip_addr_string_init( &info->PrimaryWinsServer, &wins_servers[0].Ipv4.sin_addr, &mask, 0 );
            if (wins_server_count > 1)
                ip_addr_string_init( &info->SecondaryWinsServer, &wins_servers[1].Ipv4.sin_addr, &mask, 0 );
        }

        info->LeaseObtained = 0;
        info->LeaseExpires = 0;

        info++;
    }
    info[-1].Next = NULL;

err:
    heap_free( wins_servers );
    NsiFreeTable( fwd_keys, NULL, NULL, NULL );
    NsiFreeTable( uni_keys, uni_rw, NULL, NULL );
    NsiFreeTable( if_keys, if_rw, if_dyn, if_stat );
    return err;
}

static void address_entry_free( void *ptr, ULONG offset, void *ctxt )
{
    heap_free( ptr );
}

static void address_entry_size( void *ptr, ULONG offset, void *ctxt )
{
    IP_ADAPTER_DNS_SERVER_ADDRESS *src_addr = ptr; /* all list types are super-sets of this type */
    ULONG *total = (ULONG *)ctxt, align = sizeof(ULONGLONG) - 1;

    *total = (*total + src_addr->u.s.Length + src_addr->Address.iSockaddrLength + align) & ~align;
}

struct address_entry_copy_params
{
    IP_ADAPTER_ADDRESSES *src, *dst;
    char *ptr;
    void *next;
    ULONG cur_offset;
};

static void address_entry_copy( void *ptr, ULONG offset, void *ctxt )
{
    struct address_entry_copy_params *params = ctxt;
    IP_ADAPTER_DNS_SERVER_ADDRESS *src_addr = ptr; /* all list types are super-sets of this type */
    IP_ADAPTER_DNS_SERVER_ADDRESS *dst_addr = (IP_ADAPTER_DNS_SERVER_ADDRESS *)params->ptr;
    ULONG align = sizeof(ULONGLONG) - 1;

    memcpy( dst_addr, src_addr, src_addr->u.s.Length );
    params->ptr += src_addr->u.s.Length;
    dst_addr->Address.lpSockaddr = (SOCKADDR *)params->ptr;
    memcpy( dst_addr->Address.lpSockaddr, src_addr->Address.lpSockaddr, src_addr->Address.iSockaddrLength );
    params->ptr += (src_addr->Address.iSockaddrLength + align) & ~align;

    if (params->cur_offset != offset) /* new list */
    {
        params->next = (BYTE *)params->dst + offset;
        params->cur_offset = offset;
    }
    *(IP_ADAPTER_DNS_SERVER_ADDRESS **)params->next = dst_addr;
    params->next = &dst_addr->Next;
}

static void address_lists_iterate( IP_ADAPTER_ADDRESSES *aa, void (*fn)(void *entry, ULONG offset, void *ctxt), void *ctxt )
{
    IP_ADAPTER_UNICAST_ADDRESS *uni;
    IP_ADAPTER_DNS_SERVER_ADDRESS *dns;
    IP_ADAPTER_GATEWAY_ADDRESS *gw;
    IP_ADAPTER_PREFIX *prefix;
    void *next;

    for (uni = aa->FirstUnicastAddress; uni; uni = next)
    {
        next = uni->Next;
        fn( uni, FIELD_OFFSET( IP_ADAPTER_ADDRESSES, FirstUnicastAddress ), ctxt );
    }

    for (dns = aa->FirstDnsServerAddress; dns; dns = next)
    {
        next = dns->Next;
        fn( dns, FIELD_OFFSET( IP_ADAPTER_ADDRESSES, FirstDnsServerAddress ), ctxt );
    }

    for (gw = aa->FirstGatewayAddress; gw; gw = next)
    {
        next = gw->Next;
        fn( gw, FIELD_OFFSET( IP_ADAPTER_ADDRESSES, FirstGatewayAddress ), ctxt );
    }

    for (prefix = aa->FirstPrefix; prefix; prefix = next)
    {
        next = prefix->Next;
        fn( prefix, FIELD_OFFSET( IP_ADAPTER_ADDRESSES, FirstPrefix ), ctxt );
    }
}

void adapters_addresses_free( IP_ADAPTER_ADDRESSES *info )
{
    IP_ADAPTER_ADDRESSES *aa;

    for (aa = info; aa; aa = aa->Next)
    {
        address_lists_iterate( aa, address_entry_free, NULL );

        heap_free( aa->DnsSuffix );
    }
    heap_free( info );
}

ULONG adapters_addresses_size( IP_ADAPTER_ADDRESSES *info )
{
    IP_ADAPTER_ADDRESSES *aa;
    ULONG size = 0, align = sizeof(ULONGLONG) - 1;

    for (aa = info; aa; aa = aa->Next)
    {
        size += sizeof(*aa) + ((strlen( aa->AdapterName ) + 1 + 1) & ~1);
        size += (strlenW( aa->Description ) + 1 + strlenW( aa->DnsSuffix ) + 1) * sizeof(WCHAR);
        if (aa->FriendlyName) size += (strlenW(aa->FriendlyName) + 1) * sizeof(WCHAR);
        size = (size + align) & ~align;
        address_lists_iterate( aa, address_entry_size, &size );
    }
    return size;
}

void adapters_addresses_copy( IP_ADAPTER_ADDRESSES *dst, IP_ADAPTER_ADDRESSES *src )
{
    char *ptr;
    DWORD len, align = sizeof(ULONGLONG) - 1;
    struct address_entry_copy_params params;

    while (src)
    {
        ptr = (char *)(dst + 1);
        *dst = *src;
        dst->AdapterName = ptr;
        len = strlen( src->AdapterName ) + 1;
        memcpy( dst->AdapterName, src->AdapterName, len );
        ptr += (len + 1) & ~1;
        dst->Description = (WCHAR *)ptr;
        len = (strlenW( src->Description ) + 1) * sizeof(WCHAR);
        memcpy( dst->Description, src->Description, len );
        ptr += len;
        dst->DnsSuffix = (WCHAR *)ptr;
        len = (strlenW( src->DnsSuffix ) + 1) * sizeof(WCHAR);
        memcpy( dst->DnsSuffix, src->DnsSuffix, len );
        ptr += len;
        if (src->FriendlyName)
        {
            dst->FriendlyName = (WCHAR *)ptr;
            len = (strlenW( src->FriendlyName ) + 1) * sizeof(WCHAR);
            memcpy( dst->FriendlyName, src->FriendlyName, len );
            ptr += len;
        }
        ptr = (char *)(((UINT_PTR)ptr + align) & ~align);

        params.src = src;
        params.dst = dst;
        params.ptr = ptr;
        params.next = NULL;
        params.cur_offset = ~0u;
        address_lists_iterate( src, address_entry_copy, &params );
        ptr = params.ptr;

        if (src->Next)
        {
            dst->Next = (IP_ADAPTER_ADDRESSES *)ptr;
            dst = dst->Next;
        }
        src = src->Next;
    }
}

static BOOL sockaddr_is_loopback( SOCKADDR *sock )
{
    if (sock->sa_family == WS_AF_INET)
    {
        SOCKADDR_IN *sin = (SOCKADDR_IN *)sock;
        return (sin->sin_addr.WS_s_addr & 0xff) == 127;
    }
    else if (sock->sa_family == WS_AF_INET6)
    {
        SOCKADDR_IN6 *sin6 = (SOCKADDR_IN6 *)sock;
        return WS_IN6_IS_ADDR_LOOPBACK( &sin6->sin6_addr );
    }
    return FALSE;
}

static BOOL sockaddr_is_linklocal( SOCKADDR *sock )
{
    if (sock->sa_family == WS_AF_INET6)
    {
        SOCKADDR_IN6 *sin6 = (SOCKADDR_IN6 *)sock;
        return WS_IN6_IS_ADDR_LINKLOCAL( &sin6->sin6_addr );
    }
    return FALSE;
}

static BOOL unicast_is_dns_eligible( IP_ADAPTER_UNICAST_ADDRESS *uni )
{
    return !sockaddr_is_loopback( uni->Address.lpSockaddr ) &&
        !sockaddr_is_linklocal( uni->Address.lpSockaddr );
}

static DWORD unicast_addresses_alloc( IP_ADAPTER_ADDRESSES *aa, ULONG family, ULONG flags )
{
    struct nsi_ipv4_unicast_key *key4;
    struct nsi_ipv6_unicast_key *key6;
    struct nsi_ip_unicast_rw *rw;
    struct nsi_ip_unicast_dynamic *dyn;
    struct nsi_ip_unicast_static *stat;
    IP_ADAPTER_UNICAST_ADDRESS *addr, **next;
    DWORD err, count, i, key_size = (family == WS_AF_INET) ? sizeof(*key4) : sizeof(*key6);
    DWORD sockaddr_size = (family == WS_AF_INET) ? sizeof(SOCKADDR_IN) : sizeof(SOCKADDR_IN6);
    NET_LUID *luid;
    void *key;

    err = NsiAllocateAndGetTable( 1, ip_module_id( family ), NSI_IP_UNICAST_TABLE, &key, key_size,
                                  (void **)&rw, sizeof(*rw), (void **)&dyn, sizeof(*dyn),
                                  (void **)&stat, sizeof(*stat), &count, 0 );
    if (err) return err;


    while (aa)
    {
        for (next = &aa->FirstUnicastAddress; *next; next = &(*next)->Next)
            ;

        for (i = 0; i < count; i++)
        {
            key4 = (struct nsi_ipv4_unicast_key *)key + i;
            key6 = (struct nsi_ipv6_unicast_key *)key + i;
            luid = (family == WS_AF_INET) ? &key4->luid : &key6->luid;
            if (luid->Value != aa->Luid.Value) continue;
            addr = heap_alloc_zero( sizeof(*addr) + sockaddr_size );
            if (!addr)
            {
                err = ERROR_NOT_ENOUGH_MEMORY;
                goto err;
            }
            addr->u.s.Length = sizeof(*addr);
            addr->Address.lpSockaddr = (SOCKADDR *)(addr + 1);
            addr->Address.iSockaddrLength = sockaddr_size;
            addr->Address.lpSockaddr->sa_family = family;
            if (family == WS_AF_INET)
            {
                SOCKADDR_IN *in = (SOCKADDR_IN *)addr->Address.lpSockaddr;
                in->sin_addr = key4->addr;
            }
            else
            {
                SOCKADDR_IN6 *in6 = (SOCKADDR_IN6 *)addr->Address.lpSockaddr;
                in6->sin6_addr = key6->addr;
                in6->sin6_scope_id = dyn[i].scope_id;
            }
            addr->PrefixOrigin = rw[i].prefix_origin;
            addr->SuffixOrigin = rw[i].suffix_origin;
            addr->DadState = dyn[i].dad_state;
            addr->ValidLifetime = rw[i].valid_lifetime;
            addr->PreferredLifetime = rw[i].preferred_lifetime;
            addr->LeaseLifetime = rw[i].valid_lifetime; /* FIXME */
            addr->OnLinkPrefixLength = rw[i].on_link_prefix;
            if (unicast_is_dns_eligible( addr )) addr->u.s.Flags |= IP_ADAPTER_ADDRESS_DNS_ELIGIBLE;

            *next = addr;
            next = &addr->Next;
        }
        aa = aa->Next;
    }

err:
    NsiFreeTable( key, rw, dyn, stat );
    return err;
}

static DWORD gateway_and_prefix_addresses_alloc( IP_ADAPTER_ADDRESSES *aa, ULONG family, ULONG flags )
{
    struct nsi_ipv4_forward_key *key4;
    struct nsi_ipv6_forward_key *key6;
    IP_ADAPTER_GATEWAY_ADDRESS *gw, **gw_next;
    IP_ADAPTER_PREFIX *prefix, **prefix_next;
    DWORD err, count, i, prefix_len, key_size = (family == WS_AF_INET) ? sizeof(*key4) : sizeof(*key6);
    DWORD sockaddr_size = (family == WS_AF_INET) ? sizeof(SOCKADDR_IN) : sizeof(SOCKADDR_IN6);
    SOCKADDR_INET sockaddr;
    NET_LUID *luid;
    void *key;

    err = NsiAllocateAndGetTable( 1, ip_module_id( family ), NSI_IP_FORWARD_TABLE, &key, key_size,
                                  NULL, 0, NULL, 0, NULL, 0, &count, 0 );
    if (err) return err;

    while (aa)
    {
        for (gw_next = &aa->FirstGatewayAddress; *gw_next; gw_next = &(*gw_next)->Next)
            ;
        for (prefix_next = &aa->FirstPrefix; *prefix_next; prefix_next = &(*prefix_next)->Next)
            ;

        for (i = 0; i < count; i++)
        {
            key4 = (struct nsi_ipv4_forward_key *)key + i;
            key6 = (struct nsi_ipv6_forward_key *)key + i;
            luid = (family == WS_AF_INET) ? &key4->luid : &key6->luid;
            if (luid->Value != aa->Luid.Value) continue;

            if (flags & GAA_FLAG_INCLUDE_ALL_GATEWAYS)
            {
                memset( &sockaddr, 0, sizeof(sockaddr) );
                if (family == WS_AF_INET)
                {
                    if (key4->next_hop.WS_s_addr != 0)
                    {
                        sockaddr.si_family = family;
                        sockaddr.Ipv4.sin_addr = key4->next_hop;
                    }
                }
                else
                {
                    static const IN6_ADDR zero;
                    if (memcmp( &key6->next_hop, &zero, sizeof(zero) ))
                    {
                        sockaddr.si_family = family;
                        sockaddr.Ipv6.sin6_addr = key6->next_hop;
                    }
                }

                if (sockaddr.si_family)
                {
                    gw = heap_alloc_zero( sizeof(*gw) + sockaddr_size );
                    if (!gw)
                    {
                        err = ERROR_NOT_ENOUGH_MEMORY;
                        goto err;
                    }
                    gw->u.s.Length = sizeof(*gw);
                    gw->Address.lpSockaddr = (SOCKADDR *)(gw + 1);
                    gw->Address.iSockaddrLength = sockaddr_size;
                    memcpy( gw->Address.lpSockaddr, &sockaddr, sockaddr_size );
                    *gw_next = gw;
                    gw_next = &gw->Next;
                }
            }

            if (flags & GAA_FLAG_INCLUDE_PREFIX)
            {
                memset( &sockaddr, 0, sizeof(sockaddr) );
                if (family == WS_AF_INET)
                {
                    if (!key4->next_hop.WS_s_addr)
                    {
                        sockaddr.si_family = family;
                        sockaddr.Ipv4.sin_addr = key4->prefix;
                        prefix_len = key4->prefix_len;
                    }
                }
                else
                {
                    static const IN6_ADDR zero;
                    if (!memcmp( &key6->next_hop, &zero, sizeof(zero) ))
                    {
                        sockaddr.si_family = family;
                        sockaddr.Ipv6.sin6_addr = key6->prefix;
                        prefix_len = key6->prefix_len;
                    }
                }

                if (sockaddr.si_family)
                {
                    prefix = heap_alloc_zero( sizeof(*prefix) + sockaddr_size );
                    if (!prefix)
                    {
                        err = ERROR_NOT_ENOUGH_MEMORY;
                        goto err;
                    }
                    prefix->u.s.Length = sizeof(*prefix);
                    prefix->Address.lpSockaddr = (SOCKADDR *)(prefix + 1);
                    prefix->Address.iSockaddrLength = sockaddr_size;
                    memcpy( prefix->Address.lpSockaddr, &sockaddr, sockaddr_size );
                    prefix->PrefixLength = prefix_len;
                    *prefix_next = prefix;
                    prefix_next = &prefix->Next;
                }
            }
        }
        aa = aa->Next;
    }

err:
    NsiFreeTable( key, NULL, NULL, NULL );
    return err;
}

static DWORD call_families( DWORD (*fn)( IP_ADAPTER_ADDRESSES *aa, ULONG family, ULONG flags ),
                            IP_ADAPTER_ADDRESSES *aa, ULONG family, ULONG flags )
{
    DWORD err;

    if (family != WS_AF_INET)
    {
        err = fn( aa, WS_AF_INET6, flags );
        if (err) return err;
    }

    if (family != WS_AF_INET6)
    {
        err = fn( aa, WS_AF_INET, flags );
        if (err) return err;
    }
    return err;
}

static DWORD dns_servers_query_code( ULONG family )
{
    if (family == WS_AF_INET) return DnsConfigDnsServersIpv4;
    if (family == WS_AF_INET6) return DnsConfigDnsServersIpv6;
    return DnsConfigDnsServersUnspec;
}

static DWORD dns_info_alloc( IP_ADAPTER_ADDRESSES *aa, ULONG family, ULONG flags )
{
    char buf[FIELD_OFFSET(DNS_ADDR_ARRAY, AddrArray[3])];
    IP_ADAPTER_DNS_SERVER_ADDRESS *dns, **next;
    DWORD query = dns_servers_query_code( family );
    DWORD err, i, size, attempt, sockaddr_len;
    WCHAR name[MAX_ADAPTER_NAME_LENGTH + 1];
    DNS_ADDR_ARRAY *servers;
    DNS_TXT_DATAW *search;

    while (aa)
    {
        MultiByteToWideChar( CP_ACP, 0, aa->AdapterName, -1, name, ARRAY_SIZE(name) );
        if (!(flags & GAA_FLAG_SKIP_DNS_SERVER))
        {
            servers = (DNS_ADDR_ARRAY *)buf;
            for (attempt = 0; attempt < 5; attempt++)
            {
                err = DnsQueryConfig( query, 0, name, NULL, servers, &size );
                if (err != ERROR_MORE_DATA) break;
                if (servers != (DNS_ADDR_ARRAY *)buf) heap_free( servers );
                servers = heap_alloc( size );
                if (!servers)
                {
                    err = ERROR_NOT_ENOUGH_MEMORY;
                    break;
                }
            }
            if (!err)
            {
                next = &aa->FirstDnsServerAddress;
                for (i = 0; i < servers->AddrCount; i++)
                {
                    sockaddr_len = servers->AddrArray[i].Data.DnsAddrUserDword[0];
                    if (sockaddr_len > sizeof(servers->AddrArray[i].MaxSa))
                        sockaddr_len = sizeof(servers->AddrArray[i].MaxSa);
                    dns = heap_alloc_zero( sizeof(*dns) + sockaddr_len );
                    if (!dns)
                    {
                        err = ERROR_NOT_ENOUGH_MEMORY;
                        break;
                    }
                    dns->u.s.Length = sizeof(*dns);
                    dns->Address.lpSockaddr = (SOCKADDR *)(dns + 1);
                    dns->Address.iSockaddrLength = sockaddr_len;
                    memcpy( dns->Address.lpSockaddr, servers->AddrArray[i].MaxSa, sockaddr_len );
                    *next = dns;
                    next = &dns->Next;
                }
            }
            if (servers != (DNS_ADDR_ARRAY *)buf) heap_free( servers );
            if (err) goto err;
        }

        aa->DnsSuffix = heap_alloc( MAX_DNS_SUFFIX_STRING_LENGTH * sizeof(WCHAR) );
        if (!aa->DnsSuffix)
        {
            err = ERROR_NOT_ENOUGH_MEMORY;
            goto err;
        }
        aa->DnsSuffix[0] = '\0';

        if (!DnsQueryConfig( DnsConfigSearchList, 0, name, NULL, NULL, &size ) &&
            (search = heap_alloc( size )))
        {
            if (!DnsQueryConfig( DnsConfigSearchList, 0, name, NULL, search, &size ) &&
                search->dwStringCount && strlenW( search->pStringArray[0] ) < MAX_DNS_SUFFIX_STRING_LENGTH)
            {
                strcpyW( aa->DnsSuffix, search->pStringArray[0] );
            }
            heap_free( search );
        }

        aa = aa->Next;
    }

err:
    return err;
}

static DWORD adapters_addresses_alloc( ULONG family, ULONG flags, IP_ADAPTER_ADDRESSES **info )
{
    IP_ADAPTER_ADDRESSES *aa;
    NET_LUID *luids;
    struct nsi_ndis_ifinfo_rw *rw;
    struct nsi_ndis_ifinfo_dynamic *dyn;
    struct nsi_ndis_ifinfo_static *stat;
    DWORD err, i, count, needed;
    GUID guid;
    char *str_ptr;

    err = NsiAllocateAndGetTable( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE, (void **)&luids, sizeof(*luids),
                                  (void **)&rw, sizeof(*rw), (void **)&dyn, sizeof(*dyn),
                                  (void **)&stat, sizeof(*stat), &count, 0 );
    if (err) return err;

    needed = count * (sizeof(*aa) + ((CHARS_IN_GUID + 1) & ~1) + sizeof(stat->descr.String));
    if (!(flags & GAA_FLAG_SKIP_FRIENDLY_NAME)) needed += count * sizeof(rw->alias.String);

    aa = heap_alloc_zero( needed );
    if (!aa)
    {
        err = ERROR_NOT_ENOUGH_MEMORY;
        goto err;
    }

    str_ptr = (char *)(aa + count);
    for (i = 0; i < count; i++)
    {
        aa[i].u.s.Length = sizeof(*aa);
        aa[i].u.s.IfIndex = stat[i].if_index;
        if (i < count - 1) aa[i].Next = aa + i + 1;
        ConvertInterfaceLuidToGuid( luids + i, &guid );
        ConvertGuidToStringA( &guid, str_ptr, CHARS_IN_GUID );
        aa[i].AdapterName = str_ptr;
        str_ptr += (CHARS_IN_GUID + 1) & ~1;
        if_counted_string_copy( (WCHAR *)str_ptr, ARRAY_SIZE(stat[i].descr.String), &stat[i].descr );
        aa[i].Description = (WCHAR *)str_ptr;
        str_ptr += sizeof(stat[i].descr.String);
        if (!(flags & GAA_FLAG_SKIP_FRIENDLY_NAME))
        {
            if_counted_string_copy( (WCHAR *)str_ptr, ARRAY_SIZE(rw[i].alias.String), &rw[i].alias );
            aa[i].FriendlyName = (WCHAR *)str_ptr;
            str_ptr += sizeof(rw[i].alias.String);
        }
        aa[i].PhysicalAddressLength = rw->phys_addr.Length;
        if (aa[i].PhysicalAddressLength > sizeof(aa[i].PhysicalAddress)) aa[i].PhysicalAddressLength = 0;
        memcpy( aa[i].PhysicalAddress, rw->phys_addr.Address, aa[i].PhysicalAddressLength );
        aa[i].Mtu = dyn[i].mtu;
        aa[i].IfType = stat[i].type;
        aa[i].OperStatus = dyn[i].oper_status;
        aa[i].TransmitLinkSpeed = dyn[i].xmit_speed;
        aa[i].ReceiveLinkSpeed = dyn[i].rcv_speed;
        aa[i].Luid = luids[i];
        aa[i].NetworkGuid = rw[i].network_guid;
        aa[i].ConnectionType = stat[i].conn_type;
    }

    if (!(flags & GAA_FLAG_SKIP_UNICAST))
    {
        err = call_families( unicast_addresses_alloc, aa, family, flags );
        if (err) goto err;
    }

    if (flags & (GAA_FLAG_INCLUDE_ALL_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX))
    {
        err = call_families( gateway_and_prefix_addresses_alloc, aa, family, flags );
        if (err) goto err;
    }

    err = dns_info_alloc( aa, family, flags );
    if (err) goto err;

err:
    NsiFreeTable( luids, rw, dyn, stat );
    if (!err) *info = aa;
    else adapters_addresses_free( aa );
    return err;
}

ULONG WINAPI DECLSPEC_HOTPATCH GetAdaptersAddresses( ULONG family, ULONG flags, void *reserved,
                                                     IP_ADAPTER_ADDRESSES *aa, ULONG *size )
{
    IP_ADAPTER_ADDRESSES *info;
    DWORD err, needed;

    TRACE( "(%d, %08x, %p, %p, %p)\n", family, flags, reserved, aa, size );

    if (!size) return ERROR_INVALID_PARAMETER;

    err = adapters_addresses_alloc( family, flags, &info );
    if (err) return err;

    needed = adapters_addresses_size( info );
    if (!aa || *size < needed)
    {
        *size = needed;
        err = ERROR_BUFFER_OVERFLOW;
    }
    else
        adapters_addresses_copy( aa, info );

    adapters_addresses_free( info );
    return err;
}

/******************************************************************
 *    GetBestInterface (IPHLPAPI.@)
 *
 * Get the interface, with the best route for the given IP address.
 *
 * PARAMS
 *  dwDestAddr     [In]  IP address to search the interface for
 *  pdwBestIfIndex [Out] found best interface
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetBestInterface(IPAddr dwDestAddr, PDWORD pdwBestIfIndex)
{
    struct WS_sockaddr_in sa_in;
    memset(&sa_in, 0, sizeof(sa_in));
    sa_in.sin_family = WS_AF_INET;
    sa_in.sin_addr.S_un.S_addr = dwDestAddr;
    return GetBestInterfaceEx((struct WS_sockaddr *)&sa_in, pdwBestIfIndex);
}

/******************************************************************
 *    GetBestInterfaceEx (IPHLPAPI.@)
 *
 * Get the interface, with the best route for the given IP address.
 *
 * PARAMS
 *  dwDestAddr     [In]  IP address to search the interface for
 *  pdwBestIfIndex [Out] found best interface
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetBestInterfaceEx(struct WS_sockaddr *pDestAddr, PDWORD pdwBestIfIndex)
{
  DWORD ret;

  TRACE("pDestAddr %p, pdwBestIfIndex %p\n", pDestAddr, pdwBestIfIndex);
  if (!pDestAddr || !pdwBestIfIndex)
    ret = ERROR_INVALID_PARAMETER;
  else {
    MIB_IPFORWARDROW ipRow;

    if (pDestAddr->sa_family == WS_AF_INET) {
      ret = GetBestRoute(((struct WS_sockaddr_in *)pDestAddr)->sin_addr.S_un.S_addr, 0, &ipRow);
      if (ret == ERROR_SUCCESS)
        *pdwBestIfIndex = ipRow.dwForwardIfIndex;
    } else {
      FIXME("address family %d not supported\n", pDestAddr->sa_family);
      ret = ERROR_NOT_SUPPORTED;
    }
  }
  TRACE("returning %d\n", ret);
  return ret;
}


/******************************************************************
 *    GetBestRoute (IPHLPAPI.@)
 *
 * Get the best route for the given IP address.
 *
 * PARAMS
 *  dwDestAddr   [In]  IP address to search the best route for
 *  dwSourceAddr [In]  optional source IP address
 *  pBestRoute   [Out] found best route
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetBestRoute(DWORD dwDestAddr, DWORD dwSourceAddr, PMIB_IPFORWARDROW pBestRoute)
{
  PMIB_IPFORWARDTABLE table;
  DWORD ret;

  TRACE("dwDestAddr 0x%08x, dwSourceAddr 0x%08x, pBestRoute %p\n", dwDestAddr,
   dwSourceAddr, pBestRoute);
  if (!pBestRoute)
    return ERROR_INVALID_PARAMETER;

  ret = AllocateAndGetIpForwardTableFromStack(&table, FALSE, GetProcessHeap(), 0);
  if (!ret) {
    DWORD ndx, matchedBits, matchedNdx = table->dwNumEntries;

    for (ndx = 0, matchedBits = 0; ndx < table->dwNumEntries; ndx++) {
      if (table->table[ndx].u1.ForwardType != MIB_IPROUTE_TYPE_INVALID &&
       (dwDestAddr & table->table[ndx].dwForwardMask) ==
       (table->table[ndx].dwForwardDest & table->table[ndx].dwForwardMask)) {
        DWORD numShifts, mask;

        for (numShifts = 0, mask = table->table[ndx].dwForwardMask;
         mask && mask & 1; mask >>= 1, numShifts++)
          ;
        if (numShifts > matchedBits) {
          matchedBits = numShifts;
          matchedNdx = ndx;
        }
        else if (!matchedBits) {
          matchedNdx = ndx;
        }
      }
    }
    if (matchedNdx < table->dwNumEntries) {
      memcpy(pBestRoute, &table->table[matchedNdx], sizeof(MIB_IPFORWARDROW));
      ret = ERROR_SUCCESS;
    }
    else {
      /* No route matches, which can happen if there's no default route. */
      ret = ERROR_HOST_UNREACHABLE;
    }
    HeapFree(GetProcessHeap(), 0, table);
  }
  TRACE("returning %d\n", ret);
  return ret;
}


/******************************************************************
 *    GetFriendlyIfIndex (IPHLPAPI.@)
 *
 * Get a "friendly" version of IfIndex, which is one that doesn't
 * have the top byte set.  Doesn't validate whether IfIndex is a valid
 * adapter index.
 *
 * PARAMS
 *  IfIndex [In] interface index to get the friendly one for
 *
 * RETURNS
 *  A friendly version of IfIndex.
 */
DWORD WINAPI GetFriendlyIfIndex(DWORD IfIndex)
{
  /* windows doesn't validate these, either, just makes sure the top byte is
     cleared.  I assume my ifenum module never gives an index with the top
     byte set. */
  TRACE("returning %d\n", IfIndex);
  return IfIndex;
}

static void icmp_stats_ex_to_icmp_stats( MIBICMPSTATS_EX *stats_ex, MIBICMPSTATS *stats )
{
    stats->dwMsgs = stats_ex->dwMsgs;
    stats->dwErrors = stats_ex->dwErrors;
    stats->dwDestUnreachs = stats_ex->rgdwTypeCount[ICMP4_DST_UNREACH];
    stats->dwTimeExcds = stats_ex->rgdwTypeCount[ICMP4_TIME_EXCEEDED];
    stats->dwParmProbs = stats_ex->rgdwTypeCount[ICMP4_PARAM_PROB];
    stats->dwSrcQuenchs = stats_ex->rgdwTypeCount[ICMP4_SOURCE_QUENCH];
    stats->dwRedirects = stats_ex->rgdwTypeCount[ICMP4_REDIRECT];
    stats->dwEchos = stats_ex->rgdwTypeCount[ICMP4_ECHO_REQUEST];
    stats->dwEchoReps = stats_ex->rgdwTypeCount[ICMP4_ECHO_REPLY];
    stats->dwTimestamps = stats_ex->rgdwTypeCount[ICMP4_TIMESTAMP_REQUEST];
    stats->dwTimestampReps = stats_ex->rgdwTypeCount[ICMP4_TIMESTAMP_REPLY];
    stats->dwAddrMasks = stats_ex->rgdwTypeCount[ICMP4_MASK_REQUEST];
    stats->dwAddrMaskReps = stats_ex->rgdwTypeCount[ICMP4_MASK_REPLY];
}

/******************************************************************
 *    GetIcmpStatistics (IPHLPAPI.@)
 *
 * Get the ICMP statistics for the local computer.
 *
 * PARAMS
 *  stats [Out] buffer for ICMP statistics
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetIcmpStatistics( MIB_ICMP *stats )
{
    MIB_ICMP_EX stats_ex;
    DWORD err = GetIcmpStatisticsEx( &stats_ex, WS_AF_INET );

    if (err) return err;

    icmp_stats_ex_to_icmp_stats( &stats_ex.icmpInStats, &stats->stats.icmpInStats );
    icmp_stats_ex_to_icmp_stats( &stats_ex.icmpOutStats, &stats->stats.icmpOutStats );
    return err;
}

/******************************************************************
 *    GetIcmpStatisticsEx (IPHLPAPI.@)
 *
 * Get the IPv4 and IPv6 ICMP statistics for the local computer.
 *
 * PARAMS
 *  stats [Out] buffer for ICMP statistics
 *  family [In] specifies whether IPv4 or IPv6 statistics are returned
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetIcmpStatisticsEx( MIB_ICMP_EX *stats, DWORD family )
{
    const NPI_MODULEID *mod = ip_module_id( family );
    struct nsi_ip_icmpstats_dynamic dyn;
    DWORD err;

    if (!stats || !mod) return ERROR_INVALID_PARAMETER;
    memset( stats, 0, sizeof(*stats) );

    err = NsiGetAllParameters( 1, mod, NSI_IP_ICMPSTATS_TABLE, NULL, 0, NULL, 0,
                               &dyn, sizeof(dyn), NULL, 0 );
    if (err) return err;

    stats->icmpInStats.dwMsgs = dyn.in_msgs;
    stats->icmpInStats.dwErrors = dyn.in_errors;
    memcpy( stats->icmpInStats.rgdwTypeCount, dyn.in_type_counts, sizeof( dyn.in_type_counts ) );
    stats->icmpOutStats.dwMsgs = dyn.out_msgs;
    stats->icmpOutStats.dwErrors = dyn.out_errors;
    memcpy( stats->icmpOutStats.rgdwTypeCount, dyn.out_type_counts, sizeof( dyn.out_type_counts ) );

    return ERROR_SUCCESS;
}

static void if_row_fill( MIB_IFROW *row, struct nsi_ndis_ifinfo_rw *rw, struct nsi_ndis_ifinfo_dynamic *dyn,
                         struct nsi_ndis_ifinfo_static *stat )
{
    static const WCHAR name_prefix[] = {'\\','D','E','V','I','C','E','\\','T','C','P','I','P','_',0};

    memcpy( row->wszName, name_prefix, sizeof(name_prefix) );
    ConvertGuidToStringW( &stat->if_guid, row->wszName + ARRAY_SIZE(name_prefix) - 1, CHARS_IN_GUID );
    row->dwIndex = stat->if_index;
    row->dwType = stat->type;
    row->dwMtu = dyn->mtu;
    row->dwSpeed = dyn->rcv_speed;
    row->dwPhysAddrLen = rw->phys_addr.Length;
    if (row->dwPhysAddrLen > sizeof(row->bPhysAddr)) row->dwPhysAddrLen = 0;
    memcpy( row->bPhysAddr, rw->phys_addr.Address, row->dwPhysAddrLen );
    row->dwAdminStatus = rw->admin_status;
    row->dwOperStatus = (dyn->oper_status == IfOperStatusUp) ? MIB_IF_OPER_STATUS_OPERATIONAL : MIB_IF_OPER_STATUS_NON_OPERATIONAL;
    row->dwLastChange = 0;
    row->dwInOctets = dyn->in_octets;
    row->dwInUcastPkts = dyn->in_ucast_pkts;
    row->dwInNUcastPkts = dyn->in_bcast_pkts + dyn->in_mcast_pkts;
    row->dwInDiscards = dyn->in_discards;
    row->dwInErrors = dyn->in_errors;
    row->dwInUnknownProtos = 0;
    row->dwOutOctets = dyn->out_octets;
    row->dwOutUcastPkts = dyn->out_ucast_pkts;
    row->dwOutNUcastPkts = dyn->out_bcast_pkts + dyn->out_mcast_pkts;
    row->dwOutDiscards = dyn->out_discards;
    row->dwOutErrors = dyn->out_errors;
    row->dwOutQLen = 0;
    row->dwDescrLen = WideCharToMultiByte( CP_ACP, 0, stat->descr.String, stat->descr.Length / sizeof(WCHAR),
                                           (char *)row->bDescr, sizeof(row->bDescr) - 1, NULL, NULL );
    row->bDescr[row->dwDescrLen] = '\0';
}

/******************************************************************
 *    GetIfEntry (IPHLPAPI.@)
 *
 * Get information about an interface.
 *
 * PARAMS
 *  pIfRow [In/Out] In:  dwIndex of MIB_IFROW selects the interface.
 *                  Out: interface information
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetIfEntry( MIB_IFROW *row )
{
    struct nsi_ndis_ifinfo_rw rw;
    struct nsi_ndis_ifinfo_dynamic dyn;
    struct nsi_ndis_ifinfo_static stat;
    NET_LUID luid;
    DWORD err;

    TRACE( "row %p\n", row );
    if (!row) return ERROR_INVALID_PARAMETER;

    err = ConvertInterfaceIndexToLuid( row->dwIndex, &luid );
    if (err) return err;

    err = NsiGetAllParameters( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE,
                               &luid, sizeof(luid), &rw, sizeof(rw),
                               &dyn, sizeof(dyn), &stat, sizeof(stat) );
    if (!err) if_row_fill( row, &rw, &dyn, &stat );
    return err;
}

static int ifrow_cmp( const void *a, const void *b )
{
    return ((const MIB_IFROW*)a)->dwIndex - ((const MIB_IFROW*)b)->dwIndex;
}

/******************************************************************
 *    GetIfTable (IPHLPAPI.@)
 *
 * Get a table of local interfaces.
 *
 * PARAMS
 *  table [Out]    buffer for local interfaces table
 *  size  [In/Out] length of output buffer
 *  sort  [In]     whether to sort the table
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * NOTES
 *  If size is less than required, the function will return
 *  ERROR_INSUFFICIENT_BUFFER, and *pdwSize will be set to the required byte
 *  size.
 *  If sort is true, the returned table will be sorted by interface index.
 */
DWORD WINAPI GetIfTable( MIB_IFTABLE *table, ULONG *size, BOOL sort )
{
    DWORD i, count, needed, err;
    NET_LUID *keys;
    struct nsi_ndis_ifinfo_rw *rw;
    struct nsi_ndis_ifinfo_dynamic *dyn;
    struct nsi_ndis_ifinfo_static *stat;

    if (!size) return ERROR_INVALID_PARAMETER;

    /* While this could be implemented on top of GetIfTable2(), it would require
       an additional copy of the data */
    err = NsiAllocateAndGetTable( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE, (void **)&keys, sizeof(*keys),
                                  (void **)&rw, sizeof(*rw), (void **)&dyn, sizeof(*dyn),
                                  (void **)&stat, sizeof(*stat), &count, 0 );
    if (err) return err;

    needed = FIELD_OFFSET( MIB_IFTABLE, table[count] );

    if (!table || *size < needed)
    {
        *size = needed;
        err = ERROR_INSUFFICIENT_BUFFER;
        goto err;
    }

    table->dwNumEntries = count;
    for (i = 0; i < count; i++)
    {
        MIB_IFROW *row = table->table + i;

        if_row_fill( row, rw + i, dyn + i, stat + i );
    }

    if (sort) qsort( table->table, count, sizeof(MIB_IFROW), ifrow_cmp );

err:
    NsiFreeTable( keys, rw, dyn, stat );
    return err;
}

/******************************************************************
 *    AllocateAndGetIfTableFromStack (IPHLPAPI.@)
 *
 * Get table of local interfaces.
 * Like GetIfTable(), but allocate the returned table from heap.
 *
 * PARAMS
 *  table     [Out] pointer into which the MIB_IFTABLE is
 *                  allocated and returned.
 *  sort      [In]  whether to sort the table
 *  heap      [In]  heap from which the table is allocated
 *  flags     [In]  flags to HeapAlloc
 *
 * RETURNS
 *  ERROR_INVALID_PARAMETER if ppIfTable is NULL, whatever
 *  GetIfTable() returns otherwise.
 */
DWORD WINAPI AllocateAndGetIfTableFromStack( MIB_IFTABLE **table, BOOL sort, HANDLE heap, DWORD flags )
{
    DWORD i, count, size, err;
    NET_LUID *keys;
    struct nsi_ndis_ifinfo_rw *rw;
    struct nsi_ndis_ifinfo_dynamic *dyn;
    struct nsi_ndis_ifinfo_static *stat;

    if (!table) return ERROR_INVALID_PARAMETER;

    /* While this could be implemented on top of GetIfTable(), it would require
       an additional call to retrieve the size */
    err = NsiAllocateAndGetTable( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE, (void **)&keys, sizeof(*keys),
                                  (void **)&rw, sizeof(*rw), (void **)&dyn, sizeof(*dyn),
                                  (void **)&stat, sizeof(*stat), &count, 0 );
    if (err) return err;

    size = FIELD_OFFSET( MIB_IFTABLE, table[count] );
    *table = HeapAlloc( heap, flags, size );
    if (!*table)
    {
        err = ERROR_NOT_ENOUGH_MEMORY;
        goto err;
    }

    (*table)->dwNumEntries = count;
    for (i = 0; i < count; i++)
    {
        MIB_IFROW *row = (*table)->table + i;

        if_row_fill( row, rw + i, dyn + i, stat + i );
    }
    if (sort) qsort( (*table)->table, count, sizeof(MIB_IFROW), ifrow_cmp );

err:
    NsiFreeTable( keys, rw, dyn, stat );
    return err;
}

static void if_row2_fill( MIB_IF_ROW2 *row, struct nsi_ndis_ifinfo_rw *rw, struct nsi_ndis_ifinfo_dynamic *dyn,
                          struct nsi_ndis_ifinfo_static *stat )
{
    row->InterfaceIndex = stat->if_index;
    row->InterfaceGuid = stat->if_guid;
    if_counted_string_copy( row->Alias, ARRAY_SIZE(row->Alias), &rw->alias );
    if_counted_string_copy( row->Description, ARRAY_SIZE(row->Description), &stat->descr );
    row->PhysicalAddressLength = rw->phys_addr.Length;
    if (row->PhysicalAddressLength > sizeof(row->PhysicalAddress)) row->PhysicalAddressLength = 0;
    memcpy( row->PhysicalAddress, rw->phys_addr.Address, row->PhysicalAddressLength );
    memcpy( row->PermanentPhysicalAddress, stat->perm_phys_addr.Address, row->PhysicalAddressLength );
    row->Mtu = dyn->mtu;
    row->Type = stat->type;
    row->TunnelType = TUNNEL_TYPE_NONE; /* fixme */
    row->MediaType = stat->media_type;
    row->PhysicalMediumType = stat->phys_medium_type;
    row->AccessType = stat->access_type;
    row->DirectionType = NET_IF_DIRECTION_SENDRECEIVE; /* fixme */
    row->InterfaceAndOperStatusFlags.HardwareInterface = stat->flags.hw;
    row->InterfaceAndOperStatusFlags.FilterInterface = stat->flags.filter;
    row->InterfaceAndOperStatusFlags.ConnectorPresent = !!stat->conn_present;
    row->InterfaceAndOperStatusFlags.NotAuthenticated = 0; /* fixme */
    row->InterfaceAndOperStatusFlags.NotMediaConnected = dyn->flags.not_media_conn;
    row->InterfaceAndOperStatusFlags.Paused = 0; /* fixme */
    row->InterfaceAndOperStatusFlags.LowPower = 0; /* fixme */
    row->InterfaceAndOperStatusFlags.EndPointInterface = 0; /* fixme */
    row->OperStatus = dyn->oper_status;
    row->AdminStatus = rw->admin_status;
    row->MediaConnectState = dyn->media_conn_state;
    row->NetworkGuid = rw->network_guid;
    row->ConnectionType = stat->conn_type;
    row->TransmitLinkSpeed = dyn->xmit_speed;
    row->ReceiveLinkSpeed = dyn->rcv_speed;
    row->InOctets = dyn->in_octets;
    row->InUcastPkts = dyn->in_ucast_pkts;
    row->InNUcastPkts = dyn->in_bcast_pkts + dyn->in_mcast_pkts;
    row->InDiscards = dyn->in_discards;
    row->InErrors = dyn->in_errors;
    row->InUnknownProtos = 0; /* fixme */
    row->InUcastOctets = dyn->in_ucast_octs;
    row->InMulticastOctets = dyn->in_mcast_octs;
    row->InBroadcastOctets = dyn->in_bcast_octs;
    row->OutOctets = dyn->out_octets;
    row->OutUcastPkts = dyn->out_ucast_pkts;
    row->OutNUcastPkts = dyn->out_bcast_pkts + dyn->out_mcast_pkts;
    row->OutDiscards = dyn->out_discards;
    row->OutErrors = dyn->out_errors;
    row->OutUcastOctets = dyn->out_ucast_octs;
    row->OutMulticastOctets = dyn->out_mcast_octs;
    row->OutBroadcastOctets = dyn->out_bcast_octs;
    row->OutQLen = 0; /* fixme */
}

/******************************************************************
 *    GetIfEntry2Ex (IPHLPAPI.@)
 */
DWORD WINAPI GetIfEntry2Ex( MIB_IF_TABLE_LEVEL level, MIB_IF_ROW2 *row )
{
    DWORD err;
    struct nsi_ndis_ifinfo_rw rw;
    struct nsi_ndis_ifinfo_dynamic dyn;
    struct nsi_ndis_ifinfo_static stat;

    TRACE( "(%d, %p)\n", level, row );

    if (level != MibIfTableNormal) FIXME( "level %u not fully supported\n", level );
    if (!row) return ERROR_INVALID_PARAMETER;

    if (!row->InterfaceLuid.Value)
    {
        if (!row->InterfaceIndex) return ERROR_INVALID_PARAMETER;
        err = ConvertInterfaceIndexToLuid( row->InterfaceIndex, &row->InterfaceLuid );
        if (err) return err;
    }

    err = NsiGetAllParameters( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE,
                               &row->InterfaceLuid, sizeof(row->InterfaceLuid),
                               &rw, sizeof(rw), &dyn, sizeof(dyn), &stat, sizeof(stat) );
    if (!err) if_row2_fill( row, &rw, &dyn, &stat );
    return err;
}

/******************************************************************
 *    GetIfEntry2 (IPHLPAPI.@)
 */
DWORD WINAPI GetIfEntry2( MIB_IF_ROW2 *row )
{
    return GetIfEntry2Ex( MibIfTableNormal, row );
}

/******************************************************************
 *    GetIfTable2Ex (IPHLPAPI.@)
 */
DWORD WINAPI GetIfTable2Ex( MIB_IF_TABLE_LEVEL level, MIB_IF_TABLE2 **table )
{
    DWORD i, count, size, err;
    NET_LUID *keys;
    struct nsi_ndis_ifinfo_rw *rw;
    struct nsi_ndis_ifinfo_dynamic *dyn;
    struct nsi_ndis_ifinfo_static *stat;

    TRACE( "level %u, table %p\n", level, table );

    if (!table || level > MibIfTableNormalWithoutStatistics)
        return ERROR_INVALID_PARAMETER;

    if (level != MibIfTableNormal)
        FIXME("level %u not fully supported\n", level);

    err = NsiAllocateAndGetTable( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE, (void **)&keys, sizeof(*keys),
                                  (void **)&rw, sizeof(*rw), (void **)&dyn, sizeof(*dyn),
                                  (void **)&stat, sizeof(*stat), &count, 0 );
    if (err) return err;

    size = FIELD_OFFSET( MIB_IF_TABLE2, Table[count] );

    if (!(*table = heap_alloc_zero( size )))
    {
        err = ERROR_OUTOFMEMORY;
        goto err;
    }

    (*table)->NumEntries = count;
    for (i = 0; i < count; i++)
    {
        MIB_IF_ROW2 *row = (*table)->Table + i;

        row->InterfaceLuid.Value = keys[i].Value;
        if_row2_fill( row, rw + i, dyn + i, stat + i );
    }
err:
    NsiFreeTable( keys, rw, dyn, stat );
    return err;
}

/******************************************************************
 *    GetIfTable2 (IPHLPAPI.@)
 */
DWORD WINAPI GetIfTable2( MIB_IF_TABLE2 **table )
{
    TRACE( "table %p\n", table );
    return GetIfTable2Ex( MibIfTableNormal, table );
}

/******************************************************************
 *    GetInterfaceInfo (IPHLPAPI.@)
 *
 * Get a list of network interface adapters.
 *
 * PARAMS
 *  pIfTable    [Out] buffer for interface adapters
 *  dwOutBufLen [Out] if buffer is too small, returns required size
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * BUGS
 *  MSDN states this should return non-loopback interfaces only.
 */
DWORD WINAPI GetInterfaceInfo( IP_INTERFACE_INFO *table, ULONG *size )
{
    MIB_IFTABLE *if_table;
    DWORD err, needed, i;

    TRACE("table %p, size %p\n", table, size );
    if (!size) return ERROR_INVALID_PARAMETER;

    err = AllocateAndGetIfTableFromStack( &if_table, 0, GetProcessHeap(), 0 );
    if (err) return err;

    needed = FIELD_OFFSET(IP_INTERFACE_INFO, Adapter[if_table->dwNumEntries]);
    if (!table || *size < needed)
    {
        *size = needed;
        heap_free( if_table );
        return ERROR_INSUFFICIENT_BUFFER;
    }

    table->NumAdapters = if_table->dwNumEntries;
    for (i = 0; i < if_table->dwNumEntries; i++)
    {
        table->Adapter[i].Index = if_table->table[i].dwIndex;
        strcpyW( table->Adapter[i].Name, if_table->table[i].wszName );
    }
    heap_free( if_table );
    return ERROR_SUCCESS;
}

static int ipaddrrow_cmp( const void *a, const void *b )
{
    return ((const MIB_IPADDRROW*)a)->dwAddr - ((const MIB_IPADDRROW*)b)->dwAddr;
}

/******************************************************************
 *    GetIpAddrTable (IPHLPAPI.@)
 *
 * Get interface-to-IP address mapping table. 
 *
 * PARAMS
 *  table        [Out]    buffer for mapping table
 *  size         [In/Out] length of output buffer
 *  sort         [In]     whether to sort the table
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 */
DWORD WINAPI GetIpAddrTable( MIB_IPADDRTABLE *table, ULONG *size, BOOL sort )
{
    DWORD err, count, needed, i, loopback, row_num = 0;
    struct nsi_ipv4_unicast_key *keys;
    struct nsi_ip_unicast_rw *rw;

    TRACE( "table %p, size %p, sort %d\n", table, size, sort );
    if (!size) return ERROR_INVALID_PARAMETER;

    err = NsiAllocateAndGetTable( 1, &NPI_MS_IPV4_MODULEID, NSI_IP_UNICAST_TABLE, (void **)&keys, sizeof(*keys),
                                  (void **)&rw, sizeof(*rw), NULL, 0, NULL, 0, &count, 0 );
    if (err) return err;

    needed = FIELD_OFFSET( MIB_IPADDRTABLE, table[count] );

    if (!table || *size < needed)
    {
        *size = needed;
        err = ERROR_INSUFFICIENT_BUFFER;
        goto err;
    }

    table->dwNumEntries = count;

    for (loopback = 0; loopback < 2; loopback++) /* Move the loopback addresses to the end */
    {
        for (i = 0; i < count; i++)
        {
            MIB_IPADDRROW *row = table->table + row_num;

            if (!!loopback != (keys[i].luid.Info.IfType == MIB_IF_TYPE_LOOPBACK)) continue;

            row->dwAddr = keys[i].addr.WS_s_addr;
            ConvertInterfaceLuidToIndex( &keys[i].luid, &row->dwIndex );
            ConvertLengthToIpv4Mask( rw[i].on_link_prefix, &row->dwMask );
            row->dwBCastAddr = 1;
            row->dwReasmSize = 0xffff;
            row->unused1 = 0;
            row->wType = MIB_IPADDR_PRIMARY;
            row_num++;
        }
    }

    if (sort) qsort( table->table, count, sizeof(MIB_IPADDRROW), ipaddrrow_cmp );
err:
    NsiFreeTable( keys, rw, NULL, NULL );

    return err;
}


/******************************************************************
 *    AllocateAndGetIpAddrTableFromStack (IPHLPAPI.@)
 *
 * Get interface-to-IP address mapping table.
 * Like GetIpAddrTable(), but allocate the returned table from heap.
 *
 * PARAMS
 *  table         [Out] pointer into which the MIB_IPADDRTABLE is
 *                      allocated and returned.
 *  sort          [In]  whether to sort the table
 *  heap          [In]  heap from which the table is allocated
 *  flags         [In]  flags to HeapAlloc
 *
 */
DWORD WINAPI AllocateAndGetIpAddrTableFromStack( MIB_IPADDRTABLE **table, BOOL sort, HANDLE heap, DWORD flags )
{
    DWORD err, size = FIELD_OFFSET(MIB_IPADDRTABLE, table[2]), attempt;

    TRACE( "table %p, sort %d, heap %p, flags 0x%08x\n", table, sort, heap, flags );

    for (attempt = 0; attempt < 5; attempt++)
    {
        *table = HeapAlloc( heap, flags, size );
        if (!*table) return ERROR_NOT_ENOUGH_MEMORY;

        err = GetIpAddrTable( *table, &size, sort );
        if (!err) break;
        HeapFree( heap, flags, *table );
        if (err != ERROR_INSUFFICIENT_BUFFER) break;
    }

    return err;
}

static int ipforward_row_cmp( const void *a, const void *b )
{
    const MIB_IPFORWARDROW *rowA = a;
    const MIB_IPFORWARDROW *rowB = b;
    int ret;

    if ((ret = rowA->dwForwardDest - rowB->dwForwardDest) != 0) return ret;
    if ((ret = rowA->u2.dwForwardProto - rowB->u2.dwForwardProto) != 0) return ret;
    if ((ret = rowA->dwForwardPolicy - rowB->dwForwardPolicy) != 0) return ret;
    return rowA->dwForwardNextHop - rowB->dwForwardNextHop;
}

/******************************************************************
 *    GetIpForwardTable (IPHLPAPI.@)
 *
 * Get the route table.
 *
 * PARAMS
 *  table           [Out]    buffer for route table
 *  size            [In/Out] length of output buffer
 *  sort            [In]     whether to sort the table
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetIpForwardTable( MIB_IPFORWARDTABLE *table, ULONG *size, BOOL sort )
{
    DWORD err, count, uni_count, needed, i, addr;
    struct nsi_ipv4_forward_key *keys;
    struct nsi_ip_forward_rw *rw;
    struct nsi_ipv4_forward_dynamic *dyn;
    struct nsi_ip_forward_static *stat;
    struct nsi_ipv4_unicast_key *uni_keys = NULL;

    TRACE( "table %p, size %p, sort %d\n", table, size, sort );
    if (!size) return ERROR_INVALID_PARAMETER;

    err = NsiAllocateAndGetTable( 1, &NPI_MS_IPV4_MODULEID, NSI_IP_FORWARD_TABLE, (void **)&keys, sizeof(*keys),
                                  (void **)&rw, sizeof(*rw), (void **)&dyn, sizeof(*dyn),
                                  (void **)&stat, sizeof(*stat), &count, 0 );
    if (err) return err;

    needed = FIELD_OFFSET( MIB_IPFORWARDTABLE, table[count] );

    if (!table || *size < needed)
    {
        *size = needed;
        err = ERROR_INSUFFICIENT_BUFFER;
        goto err;
    }

    err = NsiAllocateAndGetTable( 1, &NPI_MS_IPV4_MODULEID, NSI_IP_UNICAST_TABLE, (void **)&uni_keys, sizeof(*uni_keys),
                                  NULL, 0, NULL, 0, NULL, 0, &uni_count, 0 );
    if (err) goto err;

    table->dwNumEntries = count;
    for (i = 0; i < count; i++)
    {
        MIB_IPFORWARDROW *row = table->table + i;

        row->dwForwardDest = keys[i].prefix.WS_s_addr;
        ConvertLengthToIpv4Mask( keys[i].prefix_len, &row->dwForwardMask );
        row->dwForwardPolicy = 0;
        row->dwForwardNextHop = keys[i].next_hop.WS_s_addr;
        row->u1.dwForwardType = row->dwForwardNextHop ? MIB_IPROUTE_TYPE_INDIRECT : MIB_IPROUTE_TYPE_DIRECT;
        if (!row->dwForwardNextHop) /* find the interface's addr */
        {
            for (addr = 0; addr < uni_count; addr++)
            {
                if (uni_keys[addr].luid.Value == keys[i].luid.Value)
                {
                    row->dwForwardNextHop = uni_keys[addr].addr.WS_s_addr;
                    break;
                }
            }
        }
        row->dwForwardIfIndex = stat[i].if_index;
        row->u2.dwForwardProto = rw[i].protocol;
        row->dwForwardAge = dyn[i].age;
        row->dwForwardNextHopAS = 0;
        row->dwForwardMetric1 = rw[i].metric; /* FIXME: add interface metric */
        row->dwForwardMetric2 = 0;
        row->dwForwardMetric3 = 0;
        row->dwForwardMetric4 = 0;
        row->dwForwardMetric5 = 0;
    }

    if (sort) qsort( table->table, count, sizeof(MIB_IPFORWARDROW), ipforward_row_cmp );
err:
    NsiFreeTable( uni_keys, NULL, NULL, NULL );
    NsiFreeTable( keys, rw, dyn, stat );

    return err;
}

/******************************************************************
 *    AllocateAndGetIpForwardTableFromStack (IPHLPAPI.@)
 *
 * Get the route table.
 * Like GetIpForwardTable(), but allocate the returned table from heap.
 *
 * PARAMS
 *  table            [Out] pointer into which the MIB_IPFORWARDTABLE is
 *                         allocated and returned.
 *  sort             [In]  whether to sort the table
 *  heap             [In]  heap from which the table is allocated
 *  flags            [In]  flags to HeapAlloc
 *
 * RETURNS
 *  ERROR_INVALID_PARAMETER if ppIfTable is NULL, other error codes
 *  on failure, NO_ERROR on success.
 */
DWORD WINAPI AllocateAndGetIpForwardTableFromStack( MIB_IPFORWARDTABLE **table, BOOL sort, HANDLE heap, DWORD flags )
{
    DWORD err, size = FIELD_OFFSET(MIB_IPFORWARDTABLE, table[2]), attempt;

    TRACE( "table %p, sort %d, heap %p, flags 0x%08x\n", table, sort, heap, flags );

    for (attempt = 0; attempt < 5; attempt++)
    {
        *table = HeapAlloc( heap, flags, size );
        if (!*table) return ERROR_NOT_ENOUGH_MEMORY;

        err = GetIpForwardTable( *table, &size, sort );
        if (!err) break;
        HeapFree( heap, flags, *table );
        if (err != ERROR_INSUFFICIENT_BUFFER) break;
    }

    return err;
}

static void forward_row2_fill( MIB_IPFORWARD_ROW2 *row, USHORT fam, void *key, struct nsi_ip_forward_rw *rw,
                               void *dyn, struct nsi_ip_forward_static *stat )
{
    struct nsi_ipv4_forward_key *key4 = (struct nsi_ipv4_forward_key *)key;
    struct nsi_ipv6_forward_key *key6 = (struct nsi_ipv6_forward_key *)key;
    struct nsi_ipv4_forward_dynamic *dyn4 = (struct nsi_ipv4_forward_dynamic *)dyn;
    struct nsi_ipv6_forward_dynamic *dyn6 = (struct nsi_ipv6_forward_dynamic *)dyn;

    if (fam == WS_AF_INET)
    {
        row->InterfaceLuid = key4->luid;
        row->DestinationPrefix.Prefix.Ipv4.sin_family = fam;
        row->DestinationPrefix.Prefix.Ipv4.sin_port = 0;
        row->DestinationPrefix.Prefix.Ipv4.sin_addr = key4->prefix;
        memset( &row->DestinationPrefix.Prefix.Ipv4.sin_zero, 0, sizeof(row->DestinationPrefix.Prefix.Ipv4.sin_zero) );
        row->DestinationPrefix.PrefixLength = key4->prefix_len;
        row->NextHop.Ipv4.sin_family = fam;
        row->NextHop.Ipv4.sin_port = 0;
        row->NextHop.Ipv4.sin_addr = key4->next_hop;
        memset( &row->NextHop.Ipv4.sin_zero, 0, sizeof(row->NextHop.Ipv4.sin_zero) );

        row->Age = dyn4->age;
    }
    else
    {
        row->InterfaceLuid = key6->luid;

        row->DestinationPrefix.Prefix.Ipv6.sin6_family = fam;
        row->DestinationPrefix.Prefix.Ipv6.sin6_port = 0;
        row->DestinationPrefix.Prefix.Ipv6.sin6_flowinfo = 0;
        row->DestinationPrefix.Prefix.Ipv6.sin6_addr = key6->prefix;
        row->DestinationPrefix.Prefix.Ipv6.sin6_scope_id = 0;
        row->DestinationPrefix.PrefixLength = key6->prefix_len;
        row->NextHop.Ipv6.sin6_family = fam;
        row->NextHop.Ipv6.sin6_port = 0;
        row->NextHop.Ipv6.sin6_flowinfo = 0;
        row->NextHop.Ipv6.sin6_addr = key6->next_hop;
        row->NextHop.Ipv6.sin6_scope_id = 0;

        row->Age = dyn6->age;
    }

    row->InterfaceIndex = stat->if_index;

    row->SitePrefixLength = rw->site_prefix_len;
    row->ValidLifetime = rw->valid_lifetime;
    row->PreferredLifetime = rw->preferred_lifetime;
    row->Metric = rw->metric;
    row->Protocol = rw->protocol;
    row->Loopback = rw->loopback;
    row->AutoconfigureAddress = rw->autoconf;
    row->Publish = rw->publish;
    row->Immortal = rw->immortal;

    row->Origin = stat->origin;
}

/******************************************************************
 *    GetIpForwardTable2 (IPHLPAPI.@)
 */
DWORD WINAPI GetIpForwardTable2( ADDRESS_FAMILY family, MIB_IPFORWARD_TABLE2 **table )
{
    void *key[2] = { NULL, NULL };
    struct nsi_ip_forward_rw *rw[2] = { NULL, NULL };
    void *dyn[2] = { NULL, NULL };
    struct nsi_ip_forward_static *stat[2] = { NULL, NULL };
    static const USHORT fam[2] = { WS_AF_INET, WS_AF_INET6 };
    static const DWORD key_size[2] = { sizeof(struct nsi_ipv4_forward_key), sizeof(struct nsi_ipv6_forward_key) };
    static const DWORD dyn_size[2] = { sizeof(struct nsi_ipv4_forward_dynamic), sizeof(struct nsi_ipv6_forward_dynamic) };
    DWORD err = ERROR_SUCCESS, i, size, count[2] = { 0, 0 };

    TRACE( "%u, %p\n", family, table );

    if (!table || (family != WS_AF_INET && family != WS_AF_INET6 && family != WS_AF_UNSPEC))
        return ERROR_INVALID_PARAMETER;

    for (i = 0; i < 2; i++)
    {
        if (family != WS_AF_UNSPEC && family != fam[i]) continue;

        err = NsiAllocateAndGetTable( 1, ip_module_id( fam[i] ), NSI_IP_FORWARD_TABLE, key + i, key_size[i],
                                      (void **)rw + i, sizeof(**rw), dyn + i, dyn_size[i],
                                      (void **)stat + i, sizeof(**stat), count + i, 0 );
        if (err) count[i] = 0;
    }

    size = FIELD_OFFSET(MIB_IPFORWARD_TABLE2, Table[ count[0] + count[1] ]);
    *table = heap_alloc( size );
    if (!*table)
    {
        err = ERROR_NOT_ENOUGH_MEMORY;
        goto err;
    }

    (*table)->NumEntries = count[0] + count[1];
    for (i = 0; i < count[0]; i++)
    {
        MIB_IPFORWARD_ROW2 *row = (*table)->Table + i;
        struct nsi_ipv4_forward_key *key4 = (struct nsi_ipv4_forward_key *)key[0];
        struct nsi_ipv4_forward_dynamic *dyn4 = (struct nsi_ipv4_forward_dynamic *)dyn[0];

        forward_row2_fill( row, fam[0], key4 + i, rw[0] + i, dyn4 + i, stat[0] + i );
    }

    for (i = 0; i < count[1]; i++)
    {
        MIB_IPFORWARD_ROW2 *row = (*table)->Table + count[0] + i;
        struct nsi_ipv6_forward_key *key6 = (struct nsi_ipv6_forward_key *)key[1];
        struct nsi_ipv6_forward_dynamic *dyn6 = (struct nsi_ipv6_forward_dynamic *)dyn[1];

        forward_row2_fill( row, fam[1], key6 + i, rw[1] + i, dyn6 + i, stat[1] + i );
    }

err:
    for (i = 0; i < 2; i++) NsiFreeTable( key[i], rw[i], dyn[i], stat[i] );
    return err;
}

static int ipnetrow_cmp( const void *a, const void *b )
{
    const MIB_IPNETROW *row_a = a;
    const MIB_IPNETROW *row_b = b;

    return RtlUlongByteSwap( row_a->dwAddr ) - RtlUlongByteSwap( row_b->dwAddr );
}

/******************************************************************
 *    GetIpNetTable (IPHLPAPI.@)
 *
 * Get the IP-to-physical address mapping table.
 *
 * PARAMS
 *  table       [Out]    buffer for mapping table
 *  size        [In/Out] length of output buffer
 *  sort        [In]     whether to sort the table
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 */
DWORD WINAPI GetIpNetTable( MIB_IPNETTABLE *table, ULONG *size, BOOL sort )
{
    DWORD err, count, needed, i;
    struct nsi_ipv4_neighbour_key *keys;
    struct nsi_ip_neighbour_rw *rw;
    struct nsi_ip_neighbour_dynamic *dyn;

    TRACE( "table %p, size %p, sort %d\n", table, size, sort );

    if (!size) return ERROR_INVALID_PARAMETER;

    err = NsiAllocateAndGetTable( 1, &NPI_MS_IPV4_MODULEID, NSI_IP_NEIGHBOUR_TABLE, (void **)&keys, sizeof(*keys),
                                  (void **)&rw, sizeof(*rw), (void **)&dyn, sizeof(*dyn),
                                  NULL, 0, &count, 0 );
    if (err) return err;

    needed = FIELD_OFFSET( MIB_IPNETTABLE, table[count] );

    if (!table || *size < needed)
    {
        *size = needed;
        err = ERROR_INSUFFICIENT_BUFFER;
        goto err;
    }

    table->dwNumEntries = count;
    for (i = 0; i < count; i++)
    {
        MIB_IPNETROW *row = table->table + i;

        ConvertInterfaceLuidToIndex( &keys[i].luid, &row->dwIndex );
        row->dwPhysAddrLen = dyn[i].phys_addr_len;
        if (row->dwPhysAddrLen > sizeof(row->bPhysAddr)) row->dwPhysAddrLen = 0;
        memcpy( row->bPhysAddr, rw[i].phys_addr, row->dwPhysAddrLen );
        memset( row->bPhysAddr + row->dwPhysAddrLen, 0,
                sizeof(row->bPhysAddr) - row->dwPhysAddrLen );
        row->dwAddr = keys[i].addr.WS_s_addr;
        switch (dyn->state)
        {
        case NlnsUnreachable:
        case NlnsIncomplete:
            row->u.Type = MIB_IPNET_TYPE_INVALID;
            break;
        case NlnsProbe:
        case NlnsDelay:
        case NlnsStale:
        case NlnsReachable:
            row->u.Type = MIB_IPNET_TYPE_DYNAMIC;
            break;
        case NlnsPermanent:
            row->u.Type = MIB_IPNET_TYPE_STATIC;
            break;
        default:
            row->u.Type = MIB_IPNET_TYPE_OTHER;
        }
    }

    if (sort) qsort( table->table, table->dwNumEntries, sizeof(*table->table), ipnetrow_cmp );

err:
    NsiFreeTable( keys, rw, dyn, NULL );
    return err;
}

/******************************************************************
 *    AllocateAndGetIpNetTableFromStack (IPHLPAPI.@)
 */
DWORD WINAPI AllocateAndGetIpNetTableFromStack( MIB_IPNETTABLE **table, BOOL sort, HANDLE heap, DWORD flags )
{
    DWORD err, size = FIELD_OFFSET(MIB_IPNETTABLE, table[2]), attempt;

    TRACE( "table %p, sort %d, heap %p, flags 0x%08x\n", table, sort, heap, flags );

    for (attempt = 0; attempt < 5; attempt++)
    {
        *table = HeapAlloc( heap, flags, size );
        if (!*table) return ERROR_NOT_ENOUGH_MEMORY;

        err = GetIpNetTable( *table, &size, sort );
        if (!err) break;
        HeapFree( heap, flags, *table );
        if (err != ERROR_INSUFFICIENT_BUFFER) break;
    }

    return err;
}

static void ipnet_row2_fill( MIB_IPNET_ROW2 *row, USHORT fam, void *key, struct nsi_ip_neighbour_rw *rw,
                             struct nsi_ip_neighbour_dynamic *dyn )
{
    struct nsi_ipv4_neighbour_key *key4 = (struct nsi_ipv4_neighbour_key *)key;
    struct nsi_ipv6_neighbour_key *key6 = (struct nsi_ipv6_neighbour_key *)key;

    if (fam == WS_AF_INET)
    {
        row->Address.Ipv4.sin_family = fam;
        row->Address.Ipv4.sin_port = 0;
        row->Address.Ipv4.sin_addr = key4->addr;
        memset( &row->Address.Ipv4.sin_zero, 0, sizeof(row->Address.Ipv4.sin_zero) );
        row->InterfaceLuid = key4->luid;
    }
    else
    {
        row->Address.Ipv6.sin6_family = fam;
        row->Address.Ipv6.sin6_port = 0;
        row->Address.Ipv6.sin6_flowinfo = 0;
        row->Address.Ipv6.sin6_addr = key6->addr;
        row->Address.Ipv6.sin6_scope_id = 0;
        row->InterfaceLuid = key6->luid;
    }

    ConvertInterfaceLuidToIndex( &row->InterfaceLuid, &row->InterfaceIndex );

    row->PhysicalAddressLength = dyn->phys_addr_len;
    if (row->PhysicalAddressLength > sizeof(row->PhysicalAddress))
        row->PhysicalAddressLength = 0;
    memcpy( row->PhysicalAddress, rw->phys_addr, row->PhysicalAddressLength );
    memset( row->PhysicalAddress + row->PhysicalAddressLength, 0,
            sizeof(row->PhysicalAddress) - row->PhysicalAddressLength );
    row->State = dyn->state;
    row->u.Flags = 0;
    row->u.s.IsRouter = dyn->flags.is_router;
    row->u.s.IsUnreachable = dyn->flags.is_unreachable;
    row->ReachabilityTime.LastReachable = dyn->time;
}

/******************************************************************
 *    GetIpNetTable2 (IPHLPAPI.@)
 */
DWORD WINAPI GetIpNetTable2( ADDRESS_FAMILY family, MIB_IPNET_TABLE2 **table )
{
    void *key[2] = { NULL, NULL };
    struct nsi_ip_neighbour_rw *rw[2] = { NULL, NULL };
    struct nsi_ip_neighbour_dynamic *dyn[2] = { NULL, NULL };
    static const USHORT fam[2] = { WS_AF_INET, WS_AF_INET6 };
    static const DWORD key_size[2] = { sizeof(struct nsi_ipv4_neighbour_key), sizeof(struct nsi_ipv6_neighbour_key) };
    DWORD err = ERROR_SUCCESS, i, size, count[2] = { 0, 0 };

    TRACE( "%u, %p\n", family, table );

    if (!table || (family != WS_AF_INET && family != WS_AF_INET6 && family != WS_AF_UNSPEC))
        return ERROR_INVALID_PARAMETER;

    for (i = 0; i < 2; i++)
    {
        if (family != WS_AF_UNSPEC && family != fam[i]) continue;

        err = NsiAllocateAndGetTable( 1, ip_module_id( fam[i] ), NSI_IP_NEIGHBOUR_TABLE, key + i, key_size[i],
                                      (void **)rw + i, sizeof(**rw), (void **)dyn + i, sizeof(**dyn),
                                      NULL, 0, count + i, 0 );
        if (err) count[i] = 0;
    }

    size = FIELD_OFFSET(MIB_IPNET_TABLE2, Table[ count[0] + count[1] ]);
    *table = heap_alloc( size );
    if (!*table)
    {
        err = ERROR_NOT_ENOUGH_MEMORY;
        goto err;
    }

    (*table)->NumEntries = count[0] + count[1];
    for (i = 0; i < count[0]; i++)
    {
        MIB_IPNET_ROW2 *row = (*table)->Table + i;
        struct nsi_ipv4_neighbour_key *key4 = (struct nsi_ipv4_neighbour_key *)key[0];

        ipnet_row2_fill( row, fam[0], key4 + i, rw[0] + i, dyn[0] + i );
    }

    for (i = 0; i < count[1]; i++)
    {
        MIB_IPNET_ROW2 *row = (*table)->Table + count[0] + i;
        struct nsi_ipv6_neighbour_key *key6 = (struct nsi_ipv6_neighbour_key *)key[1];

        ipnet_row2_fill( row, fam[1], key6 + i, rw[1] + i, dyn[1] + i );
    }

err:
    for (i = 0; i < 2; i++) NsiFreeTable( key[i], rw[i], dyn[i], NULL );
    return err;
}

/******************************************************************
 *    GetIpStatistics (IPHLPAPI.@)
 *
 * Get the IP statistics for the local computer.
 *
 * PARAMS
 *  stats [Out] buffer for IP statistics
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetIpStatistics( MIB_IPSTATS *stats )
{
    return GetIpStatisticsEx( stats, WS_AF_INET );
}

/******************************************************************
 *    GetIpStatisticsEx (IPHLPAPI.@)
 *
 * Get the IPv4 and IPv6 statistics for the local computer.
 *
 * PARAMS
 *  stats [Out] buffer for IP statistics
 *  family [In] specifies whether IPv4 or IPv6 statistics are returned
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetIpStatisticsEx( MIB_IPSTATS *stats, DWORD family )
{
    struct nsi_ip_ipstats_dynamic dyn;
    struct nsi_ip_ipstats_static stat;
    struct nsi_ip_cmpt_rw cmpt_rw;
    struct nsi_ip_cmpt_dynamic cmpt_dyn;
    const NPI_MODULEID *mod;
    DWORD err, cmpt = 1;

    TRACE( "%p %d\n", stats, family );

    if (!stats) return ERROR_INVALID_PARAMETER;
    mod = ip_module_id( family );
    if (!mod) return ERROR_INVALID_PARAMETER;

    memset( stats, 0, sizeof(*stats) );

    err = NsiGetAllParameters( 1, mod, NSI_IP_IPSTATS_TABLE, NULL, 0, NULL, 0,
                               &dyn, sizeof(dyn), &stat, sizeof(stat) );
    if (err) return err;

    err = NsiGetAllParameters( 1, mod, NSI_IP_COMPARTMENT_TABLE, &cmpt, sizeof(cmpt), &cmpt_rw, sizeof(cmpt_rw),
                               &cmpt_dyn, sizeof(cmpt_dyn), NULL, 0 );
    if (err) return err;

    stats->u.Forwarding = cmpt_rw.not_forwarding + 1;
    stats->dwDefaultTTL = cmpt_rw.default_ttl;
    stats->dwInReceives = dyn.in_recv;
    stats->dwInHdrErrors = dyn.in_hdr_errs;
    stats->dwInAddrErrors = dyn.in_addr_errs;
    stats->dwForwDatagrams = dyn.fwd_dgrams;
    stats->dwInUnknownProtos = dyn.in_unk_protos;
    stats->dwInDiscards = dyn.in_discards;
    stats->dwInDelivers = dyn.in_delivers;
    stats->dwOutRequests = dyn.out_reqs;
    stats->dwRoutingDiscards = dyn.routing_discards;
    stats->dwOutDiscards = dyn.out_discards;
    stats->dwOutNoRoutes = dyn.out_no_routes;
    stats->dwReasmTimeout = stat.reasm_timeout;
    stats->dwReasmReqds = dyn.reasm_reqds;
    stats->dwReasmOks = dyn.reasm_oks;
    stats->dwReasmFails = dyn.reasm_fails;
    stats->dwFragOks = dyn.frag_oks;
    stats->dwFragFails = dyn.frag_fails;
    stats->dwFragCreates = dyn.frag_creates;
    stats->dwNumIf = cmpt_dyn.num_ifs;
    stats->dwNumAddr = cmpt_dyn.num_addrs;
    stats->dwNumRoutes = cmpt_dyn.num_routes;

    return err;
}

/* Gets the DNS server list into the list beginning at list.  Assumes that
 * a single server address may be placed at list if *len is at least
 * sizeof(IP_ADDR_STRING) long.  Otherwise, list->Next is set to firstDynamic,
 * and assumes that all remaining DNS servers are contiguously located
 * beginning at second.  On input, *len is assumed to be the total number
 * of bytes available for all DNS servers, and is ignored if list is NULL.
 * On return, *len is set to the total number of bytes required for all DNS
 * servers.
 * Returns ERROR_BUFFER_OVERFLOW if *len is insufficient,
 * ERROR_SUCCESS otherwise.
 */
static DWORD get_dns_server_list( const NET_LUID *luid, IP_ADDR_STRING *list, IP_ADDR_STRING *second, DWORD *len )
{
    char buf[FIELD_OFFSET(IP4_ARRAY, AddrArray[3])];
    IP4_ARRAY *servers = (IP4_ARRAY *)buf;
    DWORD needed, num, err, i, array_len = sizeof(buf);
    IP_ADDR_STRING *ptr;

    if (luid && luid->Info.IfType == MIB_IF_TYPE_LOOPBACK) return ERROR_NO_DATA;

    for (;;)
    {
        err = DnsQueryConfig( DnsConfigDnsServerList, 0, NULL, NULL, servers, &array_len );
        num = (array_len - FIELD_OFFSET(IP4_ARRAY, AddrArray[0])) / sizeof(IP4_ADDRESS);
        needed = num * sizeof(IP_ADDR_STRING);
        if (!list || *len < needed)
        {
            *len = needed;
            err = ERROR_BUFFER_OVERFLOW;
            goto err;
        }
        if (!err) break;

        if ((char *)servers != buf) heap_free( servers );
        servers = heap_alloc( array_len );
        if (!servers)
        {
            err = ERROR_NOT_ENOUGH_MEMORY;
            goto err;
        }
    }

    *len = needed;

    for (i = 0, ptr = list; i < num; i++, ptr = ptr->Next)
    {
        RtlIpv4AddressToStringA( (IN_ADDR *)&servers->AddrArray[i], ptr->IpAddress.String );
        if (i == num - 1) ptr->Next = NULL;
        else if (i == 0) ptr->Next = second;
        else ptr->Next = ptr + 1;
    }

err:
    if ((char *)servers != buf) heap_free( servers );
    return err;
}

/******************************************************************
 *    GetNetworkParams (IPHLPAPI.@)
 *
 * Get the network parameters for the local computer.
 *
 * PARAMS
 *  info [Out]    buffer for network parameters
 *  size [In/Out] length of output buffer
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * NOTES
 *  If size is less than required, the function will return
 *  ERROR_INSUFFICIENT_BUFFER, and size will be set to the required byte
 *  size.
 */
DWORD WINAPI GetNetworkParams( FIXED_INFO *info, ULONG *size )
{
    DWORD needed = sizeof(*info), dns_size, err;
    MIB_IPSTATS ip_stats;
    HKEY key;

    TRACE( "info %p, size %p\n", info, size );
    if (!size) return ERROR_INVALID_PARAMETER;

    if (get_dns_server_list( NULL, NULL, NULL, &dns_size ) == ERROR_BUFFER_OVERFLOW)
        needed += dns_size - sizeof(IP_ADDR_STRING);
    if (!info || *size < needed)
    {
        *size = needed;
        return ERROR_BUFFER_OVERFLOW;
    }

    *size = needed;
    memset( info, 0, needed );
    needed = sizeof(info->HostName);
    GetComputerNameExA( ComputerNameDnsHostname, info->HostName, &needed );
    needed = sizeof(info->DomainName);
    GetComputerNameExA( ComputerNameDnsDomain, info->DomainName, &needed );
    get_dns_server_list( NULL, &info->DnsServerList, (IP_ADDR_STRING *)(info + 1), &dns_size );
    info->CurrentDnsServer = &info->DnsServerList;
    info->NodeType = HYBRID_NODETYPE;
    err = RegOpenKeyExA( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\VxD\\MSTCP",
                         0, KEY_READ, &key );
    if (err)
        err = RegOpenKeyExA( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\NetBT\\Parameters",
                             0, KEY_READ, &key );
    if (!err)
    {
        needed = sizeof(info->ScopeId);
        RegQueryValueExA( key, "ScopeID", NULL, NULL, (BYTE *)info->ScopeId, &needed );
        RegCloseKey( key );
    }

    if (!GetIpStatistics( &ip_stats ))
        info->EnableRouting = (ip_stats.u.Forwarding == MIB_IP_FORWARDING);

    return ERROR_SUCCESS;
}


/******************************************************************
 *    GetNumberOfInterfaces (IPHLPAPI.@)
 *
 * Get the number of interfaces.
 *
 * PARAMS
 *  pdwNumIf [Out] number of interfaces
 *
 * RETURNS
 *  NO_ERROR on success, ERROR_INVALID_PARAMETER if pdwNumIf is NULL.
 */
DWORD WINAPI GetNumberOfInterfaces( DWORD *count )
{
    DWORD err, num;

    TRACE( "count %p\n", count );
    if (!count) return ERROR_INVALID_PARAMETER;

    err = NsiEnumerateObjectsAllParameters( 1, 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE, NULL, 0,
                                            NULL, 0, NULL, 0, NULL, 0, &num );
    *count = err ? 0 : num;
    return err;
}

/******************************************************************
 *    GetPerAdapterInfo (IPHLPAPI.@)
 *
 * Get information about an adapter corresponding to an interface.
 *
 * PARAMS
 *  IfIndex         [In]     interface info
 *  pPerAdapterInfo [Out]    buffer for per adapter info
 *  pOutBufLen      [In/Out] length of output buffer
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetPerAdapterInfo( ULONG index, IP_PER_ADAPTER_INFO *info, ULONG *size )
{
    DWORD needed = sizeof(*info), dns_size;
    NET_LUID luid;

    TRACE( "(index %d, info %p, size %p)\n", index, info, size );

    if (!size) return ERROR_INVALID_PARAMETER;
    if (ConvertInterfaceIndexToLuid( index, &luid )) return ERROR_NO_DATA;

    if (get_dns_server_list( &luid, NULL, NULL, &dns_size ) == ERROR_BUFFER_OVERFLOW)
        needed += dns_size - sizeof(IP_ADDR_STRING);

    if (!info || *size < needed)
    {
        *size = needed;
        return ERROR_BUFFER_OVERFLOW;
    }

    memset( info, 0, needed );
    get_dns_server_list( &luid, &info->DnsServerList, (IP_ADDR_STRING *)(info + 1), &dns_size );
    info->CurrentDnsServer = &info->DnsServerList;

    /* FIXME Autoconfig: get unicast addresses and compare to 169.254.x.x */
    return ERROR_SUCCESS;
}


/******************************************************************
 *    GetRTTAndHopCount (IPHLPAPI.@)
 *
 * Get round-trip time (RTT) and hop count.
 *
 * PARAMS
 *
 *  DestIpAddress [In]  destination address to get the info for
 *  HopCount      [Out] retrieved hop count
 *  MaxHops       [In]  maximum hops to search for the destination
 *  RTT           [Out] RTT in milliseconds
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * FIXME
 *  Stub, returns FALSE.
 */
BOOL WINAPI GetRTTAndHopCount(IPAddr DestIpAddress, PULONG HopCount, ULONG MaxHops, PULONG RTT)
{
  FIXME("(DestIpAddress 0x%08x, HopCount %p, MaxHops %d, RTT %p): stub\n",
   DestIpAddress, HopCount, MaxHops, RTT);
  return FALSE;
}

/******************************************************************
 *    GetTcpStatistics (IPHLPAPI.@)
 *
 * Get the TCP statistics for the local computer.
 *
 * PARAMS
 *  stats [Out] buffer for TCP statistics
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetTcpStatistics( MIB_TCPSTATS *stats )
{
    return GetTcpStatisticsEx( stats, WS_AF_INET );
}

/******************************************************************
 *    GetTcpStatisticsEx (IPHLPAPI.@)
 *
 * Get the IPv4 and IPv6 TCP statistics for the local computer.
 *
 * PARAMS
 *  stats [Out] buffer for TCP statistics
 *  family [In] specifies whether IPv4 or IPv6 statistics are returned
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 */
DWORD WINAPI GetTcpStatisticsEx( MIB_TCPSTATS *stats, DWORD family )
{
    struct nsi_tcp_stats_dynamic dyn;
    struct nsi_tcp_stats_static stat;
    USHORT key = (USHORT)family;
    DWORD err;

    if (!stats || !ip_module_id( family )) return ERROR_INVALID_PARAMETER;
    memset( stats, 0, sizeof(*stats) );

    err = NsiGetAllParameters( 1, &NPI_MS_TCP_MODULEID, NSI_TCP_STATS_TABLE, &key, sizeof(key), NULL, 0,
                               &dyn, sizeof(dyn), &stat, sizeof(stat) );
    if (err) return err;

    stats->u.RtoAlgorithm = stat.rto_algo;
    stats->dwRtoMin = stat.rto_min;
    stats->dwRtoMax = stat.rto_max;
    stats->dwMaxConn = stat.max_conns;
    stats->dwActiveOpens = dyn.active_opens;
    stats->dwPassiveOpens = dyn.passive_opens;
    stats->dwAttemptFails = dyn.attempt_fails;
    stats->dwEstabResets = dyn.est_rsts;
    stats->dwCurrEstab = dyn.cur_est;
    stats->dwInSegs = (DWORD)dyn.in_segs;
    stats->dwOutSegs = (DWORD)dyn.out_segs;
    stats->dwRetransSegs = dyn.retrans_segs;
    stats->dwInErrs = dyn.in_errs;
    stats->dwOutRsts = dyn.out_rsts;
    stats->dwNumConns = dyn.num_conns;

    return err;
}

/******************************************************************
 *    GetTcpTable (IPHLPAPI.@)
 *
 * Get the table of active TCP connections.
 *
 * PARAMS
 *  pTcpTable [Out]    buffer for TCP connections table
 *  pdwSize   [In/Out] length of output buffer
 *  bOrder    [In]     whether to order the table
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * NOTES
 *  If pdwSize is less than required, the function will return 
 *  ERROR_INSUFFICIENT_BUFFER, and *pdwSize will be set to 
 *  the required byte size.
 *  If bOrder is true, the returned table will be sorted, first by
 *  local address and port number, then by remote address and port
 *  number.
 */
DWORD WINAPI GetTcpTable(PMIB_TCPTABLE pTcpTable, PDWORD pdwSize, BOOL bOrder)
{
    TRACE("pTcpTable %p, pdwSize %p, bOrder %d\n", pTcpTable, pdwSize, bOrder);
    return GetExtendedTcpTable(pTcpTable, pdwSize, bOrder, WS_AF_INET, TCP_TABLE_BASIC_ALL, 0);
}

/******************************************************************
 *    GetExtendedTcpTable (IPHLPAPI.@)
 */
DWORD WINAPI GetExtendedTcpTable(PVOID pTcpTable, PDWORD pdwSize, BOOL bOrder,
                                 ULONG ulAf, TCP_TABLE_CLASS TableClass, ULONG Reserved)
{
    DWORD ret, size;
    void *table;

    TRACE("pTcpTable %p, pdwSize %p, bOrder %d, ulAf %u, TableClass %u, Reserved %u\n",
           pTcpTable, pdwSize, bOrder, ulAf, TableClass, Reserved);

    if (!pdwSize) return ERROR_INVALID_PARAMETER;

    if (TableClass >= TCP_TABLE_OWNER_MODULE_LISTENER)
        FIXME("module classes not fully supported\n");

    switch (ulAf)
    {
        case WS_AF_INET:
            ret = build_tcp_table(TableClass, &table, bOrder, GetProcessHeap(), 0, &size);
            break;

        case WS_AF_INET6:
            ret = build_tcp6_table(TableClass, &table, bOrder, GetProcessHeap(), 0, &size);
            break;

        default:
            FIXME("ulAf = %u not supported\n", ulAf);
            ret = ERROR_NOT_SUPPORTED;
    }

    if (ret)
        return ret;

    if (!pTcpTable || *pdwSize < size)
    {
        *pdwSize = size;
        ret = ERROR_INSUFFICIENT_BUFFER;
    }
    else
    {
        *pdwSize = size;
        memcpy(pTcpTable, table, size);
    }
    HeapFree(GetProcessHeap(), 0, table);
    return ret;
}

/******************************************************************
 *    GetUdpTable (IPHLPAPI.@)
 *
 * Get a table of active UDP connections.
 *
 * PARAMS
 *  pUdpTable [Out]    buffer for UDP connections table
 *  pdwSize   [In/Out] length of output buffer
 *  bOrder    [In]     whether to order the table
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * NOTES
 *  If pdwSize is less than required, the function will return 
 *  ERROR_INSUFFICIENT_BUFFER, and *pdwSize will be set to the
 *  required byte size.
 *  If bOrder is true, the returned table will be sorted, first by
 *  local address, then by local port number.
 */
DWORD WINAPI GetUdpTable(PMIB_UDPTABLE pUdpTable, PDWORD pdwSize, BOOL bOrder)
{
    return GetExtendedUdpTable(pUdpTable, pdwSize, bOrder, WS_AF_INET, UDP_TABLE_BASIC, 0);
}

/******************************************************************
 *    GetUdp6Table (IPHLPAPI.@)
 */
DWORD WINAPI GetUdp6Table(PMIB_UDP6TABLE pUdpTable, PDWORD pdwSize, BOOL bOrder)
{
    return GetExtendedUdpTable(pUdpTable, pdwSize, bOrder, WS_AF_INET6, UDP_TABLE_BASIC, 0);
}

/******************************************************************
 *    GetExtendedUdpTable (IPHLPAPI.@)
 */
DWORD WINAPI GetExtendedUdpTable(PVOID pUdpTable, PDWORD pdwSize, BOOL bOrder,
                                 ULONG ulAf, UDP_TABLE_CLASS TableClass, ULONG Reserved)
{
    DWORD ret, size;
    void *table;

    TRACE("pUdpTable %p, pdwSize %p, bOrder %d, ulAf %u, TableClass %u, Reserved %u\n",
           pUdpTable, pdwSize, bOrder, ulAf, TableClass, Reserved);

    if (!pdwSize) return ERROR_INVALID_PARAMETER;

    if (TableClass == UDP_TABLE_OWNER_MODULE)
        FIXME("UDP_TABLE_OWNER_MODULE not fully supported\n");

    switch (ulAf)
    {
        case WS_AF_INET:
            ret = build_udp_table(TableClass, &table, bOrder, GetProcessHeap(), 0, &size);
            break;

        case WS_AF_INET6:
            ret = build_udp6_table(TableClass, &table, bOrder, GetProcessHeap(), 0, &size);
            break;

        default:
            FIXME("ulAf = %u not supported\n", ulAf);
            ret = ERROR_NOT_SUPPORTED;
    }

    if (ret)
        return ret;

    if (!pUdpTable || *pdwSize < size)
    {
        *pdwSize = size;
        ret = ERROR_INSUFFICIENT_BUFFER;
    }
    else
    {
        *pdwSize = size;
        memcpy(pUdpTable, table, size);
    }
    HeapFree(GetProcessHeap(), 0, table);
    return ret;
}

static void unicast_row_fill( MIB_UNICASTIPADDRESS_ROW *row, USHORT fam, void *key, struct nsi_ip_unicast_rw *rw,
                              struct nsi_ip_unicast_dynamic *dyn, struct nsi_ip_unicast_static *stat )
{
    struct nsi_ipv4_unicast_key *key4 = (struct nsi_ipv4_unicast_key *)key;
    struct nsi_ipv6_unicast_key *key6 = (struct nsi_ipv6_unicast_key *)key;

    if (fam == WS_AF_INET)
    {
        row->Address.Ipv4.sin_family = fam;
        row->Address.Ipv4.sin_port = 0;
        row->Address.Ipv4.sin_addr = key4->addr;
        memset( row->Address.Ipv4.sin_zero, 0, sizeof(row->Address.Ipv4.sin_zero) );
        row->InterfaceLuid.Value = key4->luid.Value;
    }
    else
    {
        row->Address.Ipv6.sin6_family = fam;
        row->Address.Ipv6.sin6_port = 0;
        row->Address.Ipv6.sin6_flowinfo = 0;
        row->Address.Ipv6.sin6_addr = key6->addr;
        row->Address.Ipv6.sin6_scope_id = dyn->scope_id;
        row->InterfaceLuid.Value = key6->luid.Value;
    }

    ConvertInterfaceLuidToIndex( &row->InterfaceLuid, &row->InterfaceIndex );
    row->PrefixOrigin = rw->prefix_origin;
    row->SuffixOrigin = rw->suffix_origin;
    row->ValidLifetime = rw->valid_lifetime;
    row->PreferredLifetime = rw->preferred_lifetime;
    row->OnLinkPrefixLength = rw->on_link_prefix;
    row->SkipAsSource = 0;
    row->DadState = dyn->dad_state;
    row->ScopeId.u.Value = dyn->scope_id;
    row->CreationTimeStamp.QuadPart = stat->creation_time;
}

DWORD WINAPI GetUnicastIpAddressEntry(MIB_UNICASTIPADDRESS_ROW *row)
{
    struct nsi_ipv4_unicast_key key4;
    struct nsi_ipv6_unicast_key key6;
    struct nsi_ip_unicast_rw rw;
    struct nsi_ip_unicast_dynamic dyn;
    struct nsi_ip_unicast_static stat;
    const NPI_MODULEID *mod;
    DWORD err, key_size;
    void *key;

    TRACE( "%p\n", row );

    if (!row) return ERROR_INVALID_PARAMETER;
    mod = ip_module_id( row->Address.si_family );
    if (!mod) return ERROR_INVALID_PARAMETER;

    if (!row->InterfaceLuid.Value)
    {
        err = ConvertInterfaceIndexToLuid( row->InterfaceIndex, &row->InterfaceLuid );
        if (err) return err;
    }

    if (row->Address.si_family == WS_AF_INET)
    {
        key4.luid = row->InterfaceLuid;
        key4.addr = row->Address.Ipv4.sin_addr;
        key4.pad = 0;
        key = &key4;
        key_size = sizeof(key4);
    }
    else if (row->Address.si_family == WS_AF_INET6)
    {
        key6.luid = row->InterfaceLuid;
        key6.addr = row->Address.Ipv6.sin6_addr;
        key = &key6;
        key_size = sizeof(key6);
    }
    else return ERROR_INVALID_PARAMETER;

    err = NsiGetAllParameters( 1, mod, NSI_IP_UNICAST_TABLE, key, key_size, &rw, sizeof(rw),
                               &dyn, sizeof(dyn), &stat, sizeof(stat) );
    if (!err) unicast_row_fill( row, row->Address.si_family, key, &rw, &dyn, &stat );
    return err;
}

DWORD WINAPI GetUnicastIpAddressTable(ADDRESS_FAMILY family, MIB_UNICASTIPADDRESS_TABLE **table)
{
    void *key[2] = { NULL, NULL };
    struct nsi_ip_unicast_rw *rw[2] = { NULL, NULL };
    struct nsi_ip_unicast_dynamic *dyn[2] = { NULL, NULL };
    struct nsi_ip_unicast_static *stat[2] = { NULL, NULL };
    static const USHORT fam[2] = { WS_AF_INET, WS_AF_INET6 };
    static const DWORD key_size[2] = { sizeof(struct nsi_ipv4_unicast_key), sizeof(struct nsi_ipv6_unicast_key) };
    DWORD err, i, size, count[2] = { 0, 0 };

    TRACE( "%u, %p\n", family, table );

    if (!table || (family != WS_AF_INET && family != WS_AF_INET6 && family != WS_AF_UNSPEC))
        return ERROR_INVALID_PARAMETER;

    for (i = 0; i < 2; i++)
    {
        if (family != WS_AF_UNSPEC && family != fam[i]) continue;

        err = NsiAllocateAndGetTable( 1, ip_module_id( fam[i] ), NSI_IP_UNICAST_TABLE, key + i, key_size[i],
                                      (void **)rw + i, sizeof(**rw), (void **)dyn + i, sizeof(**dyn),
                                      (void **)stat + i, sizeof(**stat), count + i, 0 );
        if (err) goto err;
    }

    size = FIELD_OFFSET(MIB_UNICASTIPADDRESS_TABLE, Table[ count[0] + count[1] ]);
    *table = heap_alloc( size );
    if (!*table)
    {
        err = ERROR_NOT_ENOUGH_MEMORY;
        goto err;
    }

    (*table)->NumEntries = count[0] + count[1];
    for (i = 0; i < count[0]; i++)
    {
        MIB_UNICASTIPADDRESS_ROW *row = (*table)->Table + i;
        struct nsi_ipv4_unicast_key *key4 = (struct nsi_ipv4_unicast_key *)key[0];

        unicast_row_fill( row, fam[0], (void *)(key4 + i), rw[0] + i, dyn[0] + i, stat[0] + i );
    }

    for (i = 0; i < count[1]; i++)
    {
        MIB_UNICASTIPADDRESS_ROW *row = (*table)->Table + count[0] + i;
        struct nsi_ipv6_unicast_key *key6 = (struct nsi_ipv6_unicast_key *)key[1];

        unicast_row_fill( row, fam[1], (void *)(key6 + i), rw[1] + i, dyn[1] + i, stat[1] + i );
    }

err:
    for (i = 0; i < 2; i++) NsiFreeTable( key[i], rw[i], dyn[i], stat[i] );
    return err;
}

/******************************************************************
 *    GetUniDirectionalAdapterInfo (IPHLPAPI.@)
 *
 * This is a Win98-only function to get information on "unidirectional"
 * adapters.  Since this is pretty nonsensical in other contexts, it
 * never returns anything.
 *
 * PARAMS
 *  pIPIfInfo   [Out] buffer for adapter infos
 *  dwOutBufLen [Out] length of the output buffer
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI GetUniDirectionalAdapterInfo(PIP_UNIDIRECTIONAL_ADAPTER_ADDRESS pIPIfInfo, PULONG dwOutBufLen)
{
  TRACE("pIPIfInfo %p, dwOutBufLen %p\n", pIPIfInfo, dwOutBufLen);
  /* a unidirectional adapter?? not bloody likely! */
  return ERROR_NOT_SUPPORTED;
}


/******************************************************************
 *    IpReleaseAddress (IPHLPAPI.@)
 *
 * Release an IP obtained through DHCP,
 *
 * PARAMS
 *  AdapterInfo [In] adapter to release IP address
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * NOTES
 *  Since GetAdaptersInfo never returns adapters that have DHCP enabled,
 *  this function does nothing.
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI IpReleaseAddress(PIP_ADAPTER_INDEX_MAP AdapterInfo)
{
  FIXME("Stub AdapterInfo %p\n", AdapterInfo);
  return ERROR_NOT_SUPPORTED;
}


/******************************************************************
 *    IpRenewAddress (IPHLPAPI.@)
 *
 * Renew an IP obtained through DHCP.
 *
 * PARAMS
 *  AdapterInfo [In] adapter to renew IP address
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * NOTES
 *  Since GetAdaptersInfo never returns adapters that have DHCP enabled,
 *  this function does nothing.
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI IpRenewAddress(PIP_ADAPTER_INDEX_MAP AdapterInfo)
{
  FIXME("Stub AdapterInfo %p\n", AdapterInfo);
  return ERROR_NOT_SUPPORTED;
}


/******************************************************************
 *    NotifyAddrChange (IPHLPAPI.@)
 *
 * Notify caller whenever the ip-interface map is changed.
 *
 * PARAMS
 *  Handle     [Out] handle usable in asynchronous notification
 *  overlapped [In]  overlapped structure that notifies the caller
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI NotifyAddrChange(PHANDLE Handle, LPOVERLAPPED overlapped)
{
  FIXME("(Handle %p, overlapped %p): stub\n", Handle, overlapped);
  if (Handle) *Handle = INVALID_HANDLE_VALUE;
  if (overlapped) ((IO_STATUS_BLOCK *) overlapped)->u.Status = STATUS_PENDING;
  return ERROR_IO_PENDING;
}


/******************************************************************
 *    NotifyIpInterfaceChange (IPHLPAPI.@)
 */
DWORD WINAPI NotifyIpInterfaceChange(ADDRESS_FAMILY family, PIPINTERFACE_CHANGE_CALLBACK callback,
                                     PVOID context, BOOLEAN init_notify, PHANDLE handle)
{
    FIXME("(family %d, callback %p, context %p, init_notify %d, handle %p): stub\n",
          family, callback, context, init_notify, handle);
    if (handle) *handle = NULL;
    return NO_ERROR;
}

/******************************************************************
 *    NotifyRouteChange2 (IPHLPAPI.@)
 */
DWORD WINAPI NotifyRouteChange2(ADDRESS_FAMILY family, PIPFORWARD_CHANGE_CALLBACK callback, VOID* context,
                                BOOLEAN init_notify, HANDLE* handle)
{
    FIXME("(family %d, callback %p, context %p, init_notify %d, handle %p): stub\n",
        family, callback, context, init_notify, handle);
    if (handle) *handle = NULL;
    return NO_ERROR;
}


/******************************************************************
 *    NotifyRouteChange (IPHLPAPI.@)
 *
 * Notify caller whenever the ip routing table is changed.
 *
 * PARAMS
 *  Handle     [Out] handle usable in asynchronous notification
 *  overlapped [In]  overlapped structure that notifies the caller
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI NotifyRouteChange(PHANDLE Handle, LPOVERLAPPED overlapped)
{
  FIXME("(Handle %p, overlapped %p): stub\n", Handle, overlapped);
  return ERROR_NOT_SUPPORTED;
}


/******************************************************************
 *    NotifyUnicastIpAddressChange (IPHLPAPI.@)
 */
DWORD WINAPI NotifyUnicastIpAddressChange(ADDRESS_FAMILY family, PUNICAST_IPADDRESS_CHANGE_CALLBACK callback,
                                          PVOID context, BOOLEAN init_notify, PHANDLE handle)
{
    FIXME("(family %d, callback %p, context %p, init_notify %d, handle %p): semi-stub\n",
          family, callback, context, init_notify, handle);
    if (handle) *handle = NULL;

    if (init_notify)
        callback(context, NULL, MibInitialNotification);

    return NO_ERROR;
}

/******************************************************************
 *    SendARP (IPHLPAPI.@)
 *
 * Send an ARP request.
 *
 * PARAMS
 *  DestIP     [In]     attempt to obtain this IP
 *  SrcIP      [In]     optional sender IP address
 *  pMacAddr   [Out]    buffer for the mac address
 *  PhyAddrLen [In/Out] length of the output buffer
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI SendARP(IPAddr DestIP, IPAddr SrcIP, PULONG pMacAddr, PULONG PhyAddrLen)
{
  FIXME("(DestIP 0x%08x, SrcIP 0x%08x, pMacAddr %p, PhyAddrLen %p): stub\n",
   DestIP, SrcIP, pMacAddr, PhyAddrLen);
  return ERROR_NOT_SUPPORTED;
}


/******************************************************************
 *    SetIfEntry (IPHLPAPI.@)
 *
 * Set the administrative status of an interface.
 *
 * PARAMS
 *  pIfRow [In] dwAdminStatus member specifies the new status.
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI SetIfEntry(PMIB_IFROW pIfRow)
{
  FIXME("(pIfRow %p): stub\n", pIfRow);
  /* this is supposed to set an interface administratively up or down.
     Could do SIOCSIFFLAGS and set/clear IFF_UP, but, not sure I want to, and
     this sort of down is indistinguishable from other sorts of down (e.g. no
     link). */
  return ERROR_NOT_SUPPORTED;
}


/******************************************************************
 *    SetIpForwardEntry (IPHLPAPI.@)
 *
 * Modify an existing route.
 *
 * PARAMS
 *  pRoute [In] route with the new information
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns NO_ERROR.
 */
DWORD WINAPI SetIpForwardEntry(PMIB_IPFORWARDROW pRoute)
{
  FIXME("(pRoute %p): stub\n", pRoute);
  /* this is to add a route entry, how's it distinguishable from
     CreateIpForwardEntry?
     could use SIOCADDRT, not sure I want to */
  return 0;
}


/******************************************************************
 *    SetIpNetEntry (IPHLPAPI.@)
 *
 * Modify an existing ARP entry.
 *
 * PARAMS
 *  pArpEntry [In] ARP entry with the new information
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns NO_ERROR.
 */
DWORD WINAPI SetIpNetEntry(PMIB_IPNETROW pArpEntry)
{
  FIXME("(pArpEntry %p): stub\n", pArpEntry);
  /* same as CreateIpNetEntry here, could use SIOCSARP, not sure I want to */
  return 0;
}


/******************************************************************
 *    SetIpStatistics (IPHLPAPI.@)
 *
 * Toggle IP forwarding and det the default TTL value.
 *
 * PARAMS
 *  pIpStats [In] IP statistics with the new information
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns NO_ERROR.
 */
DWORD WINAPI SetIpStatistics(PMIB_IPSTATS pIpStats)
{
  FIXME("(pIpStats %p): stub\n", pIpStats);
  return 0;
}


/******************************************************************
 *    SetIpTTL (IPHLPAPI.@)
 *
 * Set the default TTL value.
 *
 * PARAMS
 *  nTTL [In] new TTL value
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns NO_ERROR.
 */
DWORD WINAPI SetIpTTL(UINT nTTL)
{
  FIXME("(nTTL %d): stub\n", nTTL);
  /* could echo nTTL > /proc/net/sys/net/ipv4/ip_default_ttl, not sure I
     want to.  Could map EACCESS to ERROR_ACCESS_DENIED, I suppose */
  return 0;
}


/******************************************************************
 *    SetTcpEntry (IPHLPAPI.@)
 *
 * Set the state of a TCP connection.
 *
 * PARAMS
 *  pTcpRow [In] specifies connection with new state
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns NO_ERROR.
 */
DWORD WINAPI SetTcpEntry(PMIB_TCPROW pTcpRow)
{
  FIXME("(pTcpRow %p): stub\n", pTcpRow);
  return 0;
}

/******************************************************************
 *    SetPerTcpConnectionEStats (IPHLPAPI.@)
 */
DWORD WINAPI SetPerTcpConnectionEStats(PMIB_TCPROW row, TCP_ESTATS_TYPE state, PBYTE rw,
                                       ULONG version, ULONG size, ULONG offset)
{
  FIXME("(row %p, state %d, rw %p, version %u, size %u, offset %u): stub\n",
        row, state, rw, version, size, offset);
  return ERROR_NOT_SUPPORTED;
}


/******************************************************************
 *    UnenableRouter (IPHLPAPI.@)
 *
 * Decrement the IP-forwarding reference count. Turn off IP-forwarding
 * if it reaches zero.
 *
 * PARAMS
 *  pOverlapped     [In/Out] should be the same as in EnableRouter()
 *  lpdwEnableCount [Out]    optional, receives reference count
 *
 * RETURNS
 *  Success: NO_ERROR
 *  Failure: error code from winerror.h
 *
 * FIXME
 *  Stub, returns ERROR_NOT_SUPPORTED.
 */
DWORD WINAPI UnenableRouter(OVERLAPPED * pOverlapped, LPDWORD lpdwEnableCount)
{
  FIXME("(pOverlapped %p, lpdwEnableCount %p): stub\n", pOverlapped,
   lpdwEnableCount);
  /* could echo "0" > /proc/net/sys/net/ipv4/ip_forward, not sure I want to
     could map EACCESS to ERROR_ACCESS_DENIED, I suppose
   */
  return ERROR_NOT_SUPPORTED;
}

/******************************************************************
 *    PfCreateInterface (IPHLPAPI.@)
 */
DWORD WINAPI PfCreateInterface(DWORD dwName, PFFORWARD_ACTION inAction, PFFORWARD_ACTION outAction,
        BOOL bUseLog, BOOL bMustBeUnique, INTERFACE_HANDLE *ppInterface)
{
    FIXME("(%d %d %d %x %x %p) stub\n", dwName, inAction, outAction, bUseLog, bMustBeUnique, ppInterface);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/******************************************************************
 *    PfUnBindInterface (IPHLPAPI.@)
 */
DWORD WINAPI PfUnBindInterface(INTERFACE_HANDLE interface)
{
    FIXME("(%p) stub\n", interface);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/******************************************************************
 *   PfDeleteInterface(IPHLPAPI.@)
 */
DWORD WINAPI PfDeleteInterface(INTERFACE_HANDLE interface)
{
    FIXME("(%p) stub\n", interface);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/******************************************************************
 *   PfBindInterfaceToIPAddress(IPHLPAPI.@)
 */
DWORD WINAPI PfBindInterfaceToIPAddress(INTERFACE_HANDLE interface, PFADDRESSTYPE type, PBYTE ip)
{
    FIXME("(%p %d %p) stub\n", interface, type, ip);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/******************************************************************
 *    GetTcpTable2 (IPHLPAPI.@)
 */
ULONG WINAPI GetTcpTable2(PMIB_TCPTABLE2 table, PULONG size, BOOL order)
{
    FIXME("pTcpTable2 %p, pdwSize %p, bOrder %d: stub\n", table, size, order);
    return ERROR_NOT_SUPPORTED;
}

/******************************************************************
 *    GetTcp6Table (IPHLPAPI.@)
 */
ULONG WINAPI GetTcp6Table(PMIB_TCP6TABLE table, PULONG size, BOOL order)
{
    TRACE("(table %p, size %p, order %d)\n", table, size, order);
    return GetExtendedTcpTable(table, size, order, WS_AF_INET6, TCP_TABLE_BASIC_ALL, 0);
}

/******************************************************************
 *    GetTcp6Table2 (IPHLPAPI.@)
 */
ULONG WINAPI GetTcp6Table2(PMIB_TCP6TABLE2 table, PULONG size, BOOL order)
{
    FIXME("pTcp6Table2 %p, size %p, order %d: stub\n", table, size, order);
    return ERROR_NOT_SUPPORTED;
}

/******************************************************************
 *    ConvertInterfaceAliasToLuid (IPHLPAPI.@)
 */
DWORD WINAPI ConvertInterfaceAliasToLuid( const WCHAR *alias, NET_LUID *luid )
{
    struct nsi_ndis_ifinfo_rw *data;
    DWORD err, count, i, len;
    NET_LUID *keys;

    TRACE( "(%s %p)\n", debugstr_w(alias), luid );

    if (!alias || !*alias || !luid) return ERROR_INVALID_PARAMETER;
    luid->Value = 0;
    len = strlenW( alias );

    err = NsiAllocateAndGetTable( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE, (void **)&keys, sizeof(*keys),
                                  (void **)&data, sizeof(*data), NULL, 0, NULL, 0, &count, 0 );
    if (err) return err;

    err = ERROR_INVALID_PARAMETER;
    for (i = 0; i < count; i++)
    {
        if (data[i].alias.Length == len * 2 && !memcmp( data[i].alias.String, alias, len * 2 ))
        {
            luid->Value = keys[i].Value;
            err = ERROR_SUCCESS;
            break;
        }
    }
    NsiFreeTable( keys, data, NULL, NULL );
    return err;
}

/******************************************************************
 *    ConvertInterfaceGuidToLuid (IPHLPAPI.@)
 */
DWORD WINAPI ConvertInterfaceGuidToLuid(const GUID *guid, NET_LUID *luid)
{
    struct nsi_ndis_ifinfo_static *data;
    DWORD err, count, i;
    NET_LUID *keys;

    TRACE( "(%s %p)\n", debugstr_guid(guid), luid );

    if (!guid || !luid) return ERROR_INVALID_PARAMETER;
    luid->Value = 0;

    err = NsiAllocateAndGetTable( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE, (void **)&keys, sizeof(*keys),
                                  NULL, 0, NULL, 0, (void **)&data, sizeof(*data), &count, 0 );
    if (err) return err;

    err = ERROR_INVALID_PARAMETER;
    for (i = 0; i < count; i++)
    {
        if (IsEqualGUID( &data[i].if_guid, guid ))
        {
            luid->Value = keys[i].Value;
            err = ERROR_SUCCESS;
            break;
        }
    }
    NsiFreeTable( keys, NULL, NULL, data );
    return err;
}

/******************************************************************
 *    ConvertInterfaceIndexToLuid (IPHLPAPI.@)
 */
DWORD WINAPI ConvertInterfaceIndexToLuid(NET_IFINDEX index, NET_LUID *luid)
{
    DWORD err;

    TRACE( "(%u %p)\n", index, luid );

    if (!luid) return ERROR_INVALID_PARAMETER;

    err = NsiGetParameter( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_INDEX_LUID_TABLE, &index, sizeof(index),
                           NSI_PARAM_TYPE_STATIC, luid, sizeof(*luid), 0 );
    if (err) luid->Value = 0;
    return err;
}

/******************************************************************
 *    ConvertInterfaceLuidToAlias (IPHLPAPI.@)
 */
DWORD WINAPI ConvertInterfaceLuidToAlias( const NET_LUID *luid, WCHAR *alias, SIZE_T len )
{
    DWORD err;
    IF_COUNTED_STRING name;

    TRACE( "(%p %p %u)\n", luid, alias, (DWORD)len );

    if (!luid || !alias) return ERROR_INVALID_PARAMETER;

    err = NsiGetParameter( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE, luid, sizeof(*luid),
                           NSI_PARAM_TYPE_RW, &name, sizeof(name),
                           FIELD_OFFSET(struct nsi_ndis_ifinfo_rw, alias) );
    if (err) return err;

    if (len <= name.Length / sizeof(WCHAR)) return ERROR_NOT_ENOUGH_MEMORY;
    memcpy( alias, name.String, name.Length );
    alias[name.Length / sizeof(WCHAR)] = '\0';

    return err;
}

/******************************************************************
 *    ConvertInterfaceLuidToGuid (IPHLPAPI.@)
 */
DWORD WINAPI ConvertInterfaceLuidToGuid(const NET_LUID *luid, GUID *guid)
{
    DWORD err;

    TRACE( "(%p %p)\n", luid, guid );

    if (!luid || !guid) return ERROR_INVALID_PARAMETER;

    err = NsiGetParameter( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE, luid, sizeof(*luid),
                           NSI_PARAM_TYPE_STATIC, guid, sizeof(*guid),
                           FIELD_OFFSET(struct nsi_ndis_ifinfo_static, if_guid) );
    if (err) memset( guid, 0, sizeof(*guid) );
    return err;
}

/******************************************************************
 *    ConvertInterfaceLuidToIndex (IPHLPAPI.@)
 */
DWORD WINAPI ConvertInterfaceLuidToIndex(const NET_LUID *luid, NET_IFINDEX *index)
{
    DWORD err;

    TRACE( "(%p %p)\n", luid, index );

    if (!luid || !index) return ERROR_INVALID_PARAMETER;

    err = NsiGetParameter( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE, luid, sizeof(*luid),
                           NSI_PARAM_TYPE_STATIC, index, sizeof(*index),
                           FIELD_OFFSET(struct nsi_ndis_ifinfo_static, if_index) );
    if (err) *index = 0;
    return err;
}

/******************************************************************
 *    ConvertInterfaceLuidToNameA (IPHLPAPI.@)
 */
DWORD WINAPI ConvertInterfaceLuidToNameA(const NET_LUID *luid, char *name, SIZE_T len)
{
    DWORD err;
    WCHAR nameW[IF_MAX_STRING_SIZE + 1];

    TRACE( "(%p %p %u)\n", luid, name, (DWORD)len );

    if (!luid) return ERROR_INVALID_PARAMETER;
    if (!name || !len) return ERROR_NOT_ENOUGH_MEMORY;

    err = ConvertInterfaceLuidToNameW( luid, nameW, ARRAY_SIZE(nameW) );
    if (err) return err;

    if (!WideCharToMultiByte( CP_UNIXCP, 0, nameW, -1, name, len, NULL, NULL ))
        err = GetLastError();
    return err;
}

static const WCHAR otherW[] = {'o','t','h','e','r',0};
static const WCHAR ethernetW[] = {'e','t','h','e','r','n','e','t',0};
static const WCHAR tokenringW[] = {'t','o','k','e','n','r','i','n','g',0};
static const WCHAR pppW[] = {'p','p','p',0};
static const WCHAR loopbackW[] = {'l','o','o','p','b','a','c','k',0};
static const WCHAR atmW[] = {'a','t','m',0};
static const WCHAR wirelessW[] = {'w','i','r','e','l','e','s','s',0};
static const WCHAR tunnelW[] = {'t','u','n','n','e','l',0};
static const WCHAR ieee1394W[] = {'i','e','e','e','1','3','9','4',0};

struct name_prefix
{
    const WCHAR *prefix;
    DWORD type;
};
static const struct name_prefix name_prefixes[] =
{
    { otherW, IF_TYPE_OTHER },
    { ethernetW, IF_TYPE_ETHERNET_CSMACD },
    { tokenringW, IF_TYPE_ISO88025_TOKENRING },
    { pppW, IF_TYPE_PPP },
    { loopbackW, IF_TYPE_SOFTWARE_LOOPBACK },
    { atmW, IF_TYPE_ATM },
    { wirelessW, IF_TYPE_IEEE80211 },
    { tunnelW, IF_TYPE_TUNNEL },
    { ieee1394W, IF_TYPE_IEEE1394 }
};

/******************************************************************
 *    ConvertInterfaceLuidToNameW (IPHLPAPI.@)
 */
DWORD WINAPI ConvertInterfaceLuidToNameW(const NET_LUID *luid, WCHAR *name, SIZE_T len)
{
    DWORD i, needed;
    const WCHAR *prefix = NULL;
    WCHAR buf[IF_MAX_STRING_SIZE + 1];
    static const WCHAR prefix_fmt[] = {'%','s','_','%','d',0};
    static const WCHAR unk_fmt[] = {'i','f','t','y','p','e','%','d','_','%','d',0};

    TRACE( "(%p %p %u)\n", luid, name, (DWORD)len );

    if (!luid || !name) return ERROR_INVALID_PARAMETER;

    for (i = 0; i < ARRAY_SIZE(name_prefixes); i++)
    {
        if (luid->Info.IfType == name_prefixes[i].type)
        {
            prefix = name_prefixes[i].prefix;
            break;
        }
    }

    if (prefix) needed = snprintfW( buf, len, prefix_fmt, prefix, luid->Info.NetLuidIndex );
    else needed = snprintfW( buf, len, unk_fmt, luid->Info.IfType, luid->Info.NetLuidIndex );

    if (needed >= len) return ERROR_NOT_ENOUGH_MEMORY;
    memcpy( name, buf, (needed + 1) * sizeof(WCHAR) );
    return ERROR_SUCCESS;
}

/******************************************************************
 *    ConvertInterfaceNameToLuidA (IPHLPAPI.@)
 */
DWORD WINAPI ConvertInterfaceNameToLuidA(const char *name, NET_LUID *luid)
{
    WCHAR nameW[IF_MAX_STRING_SIZE];

    TRACE( "(%s %p)\n", debugstr_a(name), luid );

    if (!name) return ERROR_INVALID_NAME;
    if (!MultiByteToWideChar( CP_UNIXCP, 0, name, -1, nameW, ARRAY_SIZE(nameW) ))
        return GetLastError();

    return ConvertInterfaceNameToLuidW( nameW, luid );
}

/******************************************************************
 *    ConvertInterfaceNameToLuidW (IPHLPAPI.@)
 */
DWORD WINAPI ConvertInterfaceNameToLuidW(const WCHAR *name, NET_LUID *luid)
{
    const WCHAR *sep;
    static const WCHAR iftype[] = {'i','f','t','y','p','e',0};
    DWORD type = ~0u, i;
    WCHAR buf[IF_MAX_STRING_SIZE + 1];

    TRACE( "(%s %p)\n", debugstr_w(name), luid );

    if (!luid) return ERROR_INVALID_PARAMETER;
    memset( luid, 0, sizeof(*luid) );

    if (!name || !(sep = strchrW( name, '_' )) || sep >= name + ARRAY_SIZE(buf)) return ERROR_INVALID_NAME;
    memcpy( buf, name, (sep - name) * sizeof(WCHAR) );
    buf[sep - name] = '\0';

    if (sep - name > ARRAY_SIZE(iftype) - 1 && !memcmp( buf, iftype, (ARRAY_SIZE(iftype) - 1) * sizeof(WCHAR) ))
    {
        type = atoiW( buf + ARRAY_SIZE(iftype) - 1 );
    }
    else
    {
        for (i = 0; i < ARRAY_SIZE(name_prefixes); i++)
        {
            if (!strcmpW( buf, name_prefixes[i].prefix ))
            {
                type = name_prefixes[i].type;
                break;
            }
        }
    }
    if (type == ~0u) return ERROR_INVALID_NAME;

    luid->Info.NetLuidIndex = atoiW( sep + 1 );
    luid->Info.IfType = type;
    return ERROR_SUCCESS;
}

/******************************************************************
 *    ConvertLengthToIpv4Mask (IPHLPAPI.@)
 */
DWORD WINAPI ConvertLengthToIpv4Mask(ULONG mask_len, ULONG *mask)
{
    if (mask_len > 32)
    {
        *mask = INADDR_NONE;
        return ERROR_INVALID_PARAMETER;
    }

    if (mask_len == 0)
        *mask = 0;
    else
        *mask = htonl(~0u << (32 - mask_len));

    return NO_ERROR;
}

/******************************************************************
 *    if_nametoindex (IPHLPAPI.@)
 */
IF_INDEX WINAPI IPHLP_if_nametoindex(const char *name)
{
    IF_INDEX index;
    NET_LUID luid;
    DWORD err;

    TRACE( "(%s)\n", name );

    err = ConvertInterfaceNameToLuidA( name, &luid );
    if (err) return 0;

    err = ConvertInterfaceLuidToIndex( &luid, &index );
    if (err) index = 0;
    return index;
}

/******************************************************************
 *    if_indextoname (IPHLPAPI.@)
 */
char *WINAPI IPHLP_if_indextoname( NET_IFINDEX index, char *name )
{
    NET_LUID luid;
    DWORD err;

    TRACE( "(%u, %p)\n", index, name );

    err = ConvertInterfaceIndexToLuid( index, &luid );
    if (err) return NULL;

    err = ConvertInterfaceLuidToNameA( &luid, name, IF_MAX_STRING_SIZE );
    if (err) return NULL;
    return name;
}

/******************************************************************
 *    GetIpInterfaceTable (IPHLPAPI.@)
 */
DWORD WINAPI GetIpInterfaceTable(ADDRESS_FAMILY family, PMIB_IPINTERFACE_TABLE *table)
{
    FIXME("(%u %p): stub\n", family, table);
    return ERROR_NOT_SUPPORTED;
}

/******************************************************************
 *    GetBestRoute2 (IPHLPAPI.@)
 */
DWORD WINAPI GetBestRoute2(NET_LUID *luid, NET_IFINDEX index,
                           const SOCKADDR_INET *source, const SOCKADDR_INET *destination,
                           ULONG options, PMIB_IPFORWARD_ROW2 bestroute,
                           SOCKADDR_INET *bestaddress)
{
    static int once;

    if (!once++)
        FIXME("(%p, %d, %p, %p, 0x%08x, %p, %p): stub\n", luid, index, source,
                destination, options, bestroute, bestaddress);

    if (!destination || !bestroute || !bestaddress)
        return ERROR_INVALID_PARAMETER;

    return ERROR_NOT_SUPPORTED;
}

/******************************************************************
 *    ParseNetworkString (IPHLPAPI.@)
 */
DWORD WINAPI ParseNetworkString(const WCHAR *str, DWORD type,
                                NET_ADDRESS_INFO *info, USHORT *port, BYTE *prefix_len)
{
    IN_ADDR temp_addr4;
    IN6_ADDR temp_addr6;
    ULONG temp_scope;
    USHORT temp_port = 0;
    NTSTATUS status;

    TRACE("(%s, %d, %p, %p, %p)\n", debugstr_w(str), type, info, port, prefix_len);

    if (!str)
        return ERROR_INVALID_PARAMETER;

    if (type & NET_STRING_IPV4_ADDRESS)
    {
        status = RtlIpv4StringToAddressExW(str, TRUE, &temp_addr4, &temp_port);
        if (SUCCEEDED(status) && !temp_port)
        {
            if (info)
            {
                info->Format = NET_ADDRESS_IPV4;
                info->u.Ipv4Address.sin_addr = temp_addr4;
                info->u.Ipv4Address.sin_port = 0;
            }
            if (port) *port = 0;
            if (prefix_len) *prefix_len = 255;
            return ERROR_SUCCESS;
        }
    }
    if (type & NET_STRING_IPV4_SERVICE)
    {
        status = RtlIpv4StringToAddressExW(str, TRUE, &temp_addr4, &temp_port);
        if (SUCCEEDED(status) && temp_port)
        {
            if (info)
            {
                info->Format = NET_ADDRESS_IPV4;
                info->u.Ipv4Address.sin_addr = temp_addr4;
                info->u.Ipv4Address.sin_port = temp_port;
            }
            if (port) *port = ntohs(temp_port);
            if (prefix_len) *prefix_len = 255;
            return ERROR_SUCCESS;
        }
    }
    if (type & NET_STRING_IPV6_ADDRESS)
    {
        status = RtlIpv6StringToAddressExW(str, &temp_addr6, &temp_scope, &temp_port);
        if (SUCCEEDED(status) && !temp_port)
        {
            if (info)
            {
                info->Format = NET_ADDRESS_IPV6;
                info->u.Ipv6Address.sin6_addr = temp_addr6;
                info->u.Ipv6Address.sin6_scope_id = temp_scope;
                info->u.Ipv6Address.sin6_port = 0;
            }
            if (port) *port = 0;
            if (prefix_len) *prefix_len = 255;
            return ERROR_SUCCESS;
        }
    }
    if (type & NET_STRING_IPV6_SERVICE)
    {
        status = RtlIpv6StringToAddressExW(str, &temp_addr6, &temp_scope, &temp_port);
        if (SUCCEEDED(status) && temp_port)
        {
            if (info)
            {
                info->Format = NET_ADDRESS_IPV6;
                info->u.Ipv6Address.sin6_addr = temp_addr6;
                info->u.Ipv6Address.sin6_scope_id = temp_scope;
                info->u.Ipv6Address.sin6_port = temp_port;
            }
            if (port) *port = ntohs(temp_port);
            if (prefix_len) *prefix_len = 255;
            return ERROR_SUCCESS;
        }
    }

    if (info) info->Format = NET_ADDRESS_FORMAT_UNSPECIFIED;

    if (type & ~(NET_STRING_IPV4_ADDRESS|NET_STRING_IPV4_SERVICE|NET_STRING_IPV6_ADDRESS|NET_STRING_IPV6_SERVICE))
    {
        FIXME("Unimplemented type 0x%x\n", type);
        return ERROR_NOT_SUPPORTED;
    }

    return ERROR_INVALID_PARAMETER;
}
