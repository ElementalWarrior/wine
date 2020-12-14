/*
 * NT threads support
 *
 * Copyright 1996, 2003 Alexandre Julliard
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
#include "wine/port.h"

#include <assert.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>

#define NONAMELESSUNION
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "ntdll_misc.h"
#include "ddk/wdm.h"
#include "wine/exception.h"

WINE_DEFAULT_DEBUG_CHANNEL(thread);
WINE_DECLARE_DEBUG_CHANNEL(relay);

struct _KUSER_SHARED_DATA *user_shared_data = (void *)0x7ffe0000;

static PEB *peb;
static PEB_LDR_DATA ldr;
static RTL_BITMAP tls_bitmap;
static RTL_BITMAP tls_expansion_bitmap;
static RTL_BITMAP fls_bitmap;
static API_SET_NAMESPACE_ARRAY apiset_map;
static int nb_threads = 1;

struct ldt_copy *__wine_ldt_copy = NULL;

static RTL_CRITICAL_SECTION peb_lock;
static RTL_CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &peb_lock,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": peb_lock") }
};
static RTL_CRITICAL_SECTION peb_lock = { &critsect_debug, -1, 0, 0, 0, 0 };


/***********************************************************************
 *		__wine_dbg_get_channel_flags  (NTDLL.@)
 *
 * Get the flags to use for a given channel, possibly setting them too in case of lazy init
 */
unsigned char __cdecl __wine_dbg_get_channel_flags( struct __wine_debug_channel *channel )
{
    return unix_funcs->dbg_get_channel_flags( channel );
}

/***********************************************************************
 *		__wine_dbg_strdup  (NTDLL.@)
 */
const char * __cdecl __wine_dbg_strdup( const char *str )
{
    return unix_funcs->dbg_strdup( str );
}

/***********************************************************************
 *		__wine_dbg_header  (NTDLL.@)
 */
int __cdecl __wine_dbg_header( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                               const char *function )
{
    return unix_funcs->dbg_header( cls, channel, function );
}

/***********************************************************************
 *		__wine_dbg_output  (NTDLL.@)
 */
int __cdecl __wine_dbg_output( const char *str )
{
    return unix_funcs->dbg_output( str );
}

#if defined(__i386__) || defined(__x86_64__)
extern void DECLSPEC_NORETURN __wine_syscall_dispatcher( void );
#else
void __wine_syscall_dispatcher( void )
{
    FIXME("Syscall dispatcher is not available for this architecture.\n");
}
#endif

#if defined(__x86_64__)
extern unsigned int __wine_nb_syscalls;
#else
unsigned int __wine_nb_syscalls;
#endif

void *WINAPI __wine_fakedll_dispatcher( const char *module, ULONG ord )
{
    UNICODE_STRING name;
    NTSTATUS status;
    HMODULE base;
    WCHAR *moduleW;
    void *proc = NULL;
    DWORD len = strlen(module);

    TRACE( "(%s, %u)\n", debugstr_a(module), ord );

    if (!(moduleW = RtlAllocateHeap( GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR) )))
        return NULL;

    ascii_to_unicode( moduleW, module, len );
    moduleW[ len ] = 0;
    RtlInitUnicodeString( &name, moduleW );

    status = LdrGetDllHandle( NULL, 0, &name, &base );
    if (status == STATUS_DLL_NOT_FOUND)
        status = LdrLoadDll( NULL, 0, &name, &base );
    if (status == STATUS_SUCCESS)
        status = LdrAddRefDll( LDR_ADDREF_DLL_PIN, base );
    if (status == STATUS_SUCCESS)
        status = LdrGetProcedureAddress( base, NULL, ord, &proc );

    if (status)
        FIXME( "No procedure address found for %s.#%u, status %x\n", debugstr_a(module), ord, status );

    RtlFreeHeap( GetProcessHeap(), 0, moduleW );
    return proc;
}

/***********************************************************************
 *           thread_init
 *
 * Setup the initial thread.
 *
 * NOTES: The first allocated TEB on NT is at 0x7ffde000.
 */
TEB *thread_init( SIZE_T *info_size, BOOL *suspend )
{
    TEB *teb = unix_funcs->init_threading( &nb_threads, &__wine_ldt_copy, info_size, suspend, &server_cpus,
                                           &is_wow64, &server_start_time, __wine_syscall_dispatcher, __wine_nb_syscalls );
    teb->Spare2 = (ULONG_PTR)__wine_fakedll_dispatcher;

    peb = teb->Peb;
    peb->FastPebLock        = &peb_lock;
    peb->ApiSetMap          = &apiset_map;
    peb->TlsBitmap          = &tls_bitmap;
    peb->TlsExpansionBitmap = &tls_expansion_bitmap;
    peb->FlsBitmap          = &fls_bitmap;
    peb->LdrData            = &ldr;
    peb->OSMajorVersion     = 5;
    peb->OSMinorVersion     = 1;
    peb->OSBuildNumber      = 0xA28;
    peb->OSPlatformId       = VER_PLATFORM_WIN32_NT;
    ldr.Length = sizeof(ldr);
    ldr.Initialized = TRUE;
    RtlInitializeBitMap( &tls_bitmap, peb->TlsBitmapBits, sizeof(peb->TlsBitmapBits) * 8 );
    RtlInitializeBitMap( &tls_expansion_bitmap, peb->TlsExpansionBitmapBits,
                         sizeof(peb->TlsExpansionBitmapBits) * 8 );
    RtlInitializeBitMap( &fls_bitmap, peb->FlsBitmapBits, sizeof(peb->FlsBitmapBits) * 8 );
    RtlSetBits( peb->TlsBitmap, 0, 1 ); /* TLS index 0 is reserved and should be initialized to NULL. */
    RtlSetBits( peb->FlsBitmap, 0, 1 );
    InitializeListHead( &peb->FlsListHead );
    InitializeListHead( &ldr.InLoadOrderModuleList );
    InitializeListHead( &ldr.InMemoryOrderModuleList );
    InitializeListHead( &ldr.InInitializationOrderModuleList );

    /*
     * Starting with Vista, the first user to log on has session id 1.
     * Session id 0 is for processes that don't interact with the user (like services).
     */
    peb->SessionId = 1;

    unix_funcs->get_paths( &build_dir, &data_dir, &config_dir );
    fill_cpu_info();
    return teb;
}


/***********************************************************************
 *           RtlExitUserThread  (NTDLL.@)
 */
void WINAPI RtlExitUserThread( ULONG status )
{
    if (status)  /* send the exit code to the server (0 is already the default) */
    {
        SERVER_START_REQ( terminate_thread )
        {
            req->handle    = wine_server_obj_handle( GetCurrentThread() );
            req->exit_code = status;
            wine_server_call( req );
        }
        SERVER_END_REQ;
    }

    if (InterlockedCompareExchange( &nb_threads, 0, 0 ) <= 0)
    {
        LdrShutdownProcess();
        unix_funcs->exit_process( status );
    }

    LdrShutdownThread();
    RtlFreeThreadActivationContextStack();
    for (;;) unix_funcs->exit_thread( status );
}


/***********************************************************************
 *           RtlUserThreadStart (NTDLL.@)
 */
#ifdef __i386__
__ASM_STDCALL_FUNC( RtlUserThreadStart, 8,
                   "pushl %ebp\n\t"
                   __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                   __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
                   "movl %esp,%ebp\n\t"
                   __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                   "pushl %ebx\n\t"  /* arg */
                   "pushl %eax\n\t"  /* entry */
                   "call " __ASM_NAME("call_thread_func") )

/* wrapper for apps that don't declare the thread function correctly */
extern DWORD call_thread_func_wrapper( PRTL_THREAD_START_ROUTINE entry, void *arg );
__ASM_GLOBAL_FUNC(call_thread_func_wrapper,
                  "pushl %ebp\n\t"
                  __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                  __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
                  "movl %esp,%ebp\n\t"
                  __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                  "subl $4,%esp\n\t"
                  "pushl 12(%ebp)\n\t"
                  "call *8(%ebp)\n\t"
                  "leave\n\t"
                  __ASM_CFI(".cfi_def_cfa %esp,4\n\t")
                  __ASM_CFI(".cfi_same_value %ebp\n\t")
                  "ret" )

void DECLSPEC_HIDDEN call_thread_func( PRTL_THREAD_START_ROUTINE entry, void *arg )
{
    __TRY
    {
        TRACE_(relay)( "\1Starting thread proc %p (arg=%p)\n", entry, arg );
        RtlExitUserThread( call_thread_func_wrapper( entry, arg ));
    }
    __EXCEPT(call_unhandled_exception_filter)
    {
        NtTerminateThread( GetCurrentThread(), GetExceptionCode() );
    }
    __ENDTRY
    abort();  /* should not be reached */
}

#else  /* __i386__ */

void WINAPI RtlUserThreadStart( PRTL_THREAD_START_ROUTINE entry, void *arg )
{
    __TRY
    {
        TRACE_(relay)( "\1Starting thread proc %p (arg=%p)\n", entry, arg );
        RtlExitUserThread( ((LPTHREAD_START_ROUTINE)entry)( arg ));
    }
    __EXCEPT(call_unhandled_exception_filter)
    {
        NtTerminateThread( GetCurrentThread(), GetExceptionCode() );
    }
    __ENDTRY
    abort();  /* should not be reached */
}

#endif  /* __i386__ */


/***********************************************************************
 *              NtCreateThreadEx   (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateThreadEx( HANDLE *handle_ptr, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr,
                                  HANDLE process, PRTL_THREAD_START_ROUTINE start, void *param,
                                  ULONG flags, SIZE_T zero_bits, SIZE_T stack_commit,
                                  SIZE_T stack_reserve, PS_ATTRIBUTE_LIST *attr_list )
{
    return unix_funcs->NtCreateThreadEx( handle_ptr, access, attr, process, start, param,
                                         flags, zero_bits, stack_commit, stack_reserve, attr_list );
}


/***********************************************************************
 *              RtlCreateUserThread   (NTDLL.@)
 */
NTSTATUS WINAPI RtlCreateUserThread( HANDLE process, SECURITY_DESCRIPTOR *descr,
                                     BOOLEAN suspended, PVOID stack_addr,
                                     SIZE_T stack_reserve, SIZE_T stack_commit,
                                     PRTL_THREAD_START_ROUTINE start, void *param,
                                     HANDLE *handle_ptr, CLIENT_ID *id )
{
    ULONG flags = suspended ? THREAD_CREATE_FLAGS_CREATE_SUSPENDED : 0;
    HANDLE handle;
    NTSTATUS status;
    CLIENT_ID client_id;
    OBJECT_ATTRIBUTES attr;
    PS_ATTRIBUTE_LIST attr_list = { sizeof(attr_list) };

    attr_list.Attributes[0].Attribute = PS_ATTRIBUTE_CLIENT_ID;
    attr_list.Attributes[0].Size      = sizeof(client_id);
    attr_list.Attributes[0].ValuePtr  = &client_id;

    InitializeObjectAttributes( &attr, NULL, 0, NULL, descr );

    status = NtCreateThreadEx( &handle, THREAD_ALL_ACCESS, &attr, process, start, param,
                               flags, 0, stack_commit, stack_reserve, &attr_list );
    if (!status)
    {
        if (id) *id = client_id;
        if (handle_ptr) *handle_ptr = handle;
        else NtClose( handle );
    }
    return status;
}


/******************************************************************************
 *              RtlGetNtGlobalFlags   (NTDLL.@)
 */
ULONG WINAPI RtlGetNtGlobalFlags(void)
{
    if (!peb) return 0;  /* init not done yet */
    return peb->NtGlobalFlag;
}


/***********************************************************************
 *              NtOpenThread   (NTDLL.@)
 *              ZwOpenThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenThread( HANDLE *handle, ACCESS_MASK access,
                              const OBJECT_ATTRIBUTES *attr, const CLIENT_ID *id )
{
    return unix_funcs->NtOpenThread( handle, access, attr, id );
}


/******************************************************************************
 *              NtSuspendThread   (NTDLL.@)
 *              ZwSuspendThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtSuspendThread( HANDLE handle, PULONG count )
{
    return unix_funcs->NtSuspendThread( handle, count );
}


/******************************************************************************
 *              NtResumeThread   (NTDLL.@)
 *              ZwResumeThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtResumeThread( HANDLE handle, PULONG count )
{
    return unix_funcs->NtResumeThread( handle, count );
}


/******************************************************************************
 *              NtAlertResumeThread   (NTDLL.@)
 *              ZwAlertResumeThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtAlertResumeThread( HANDLE handle, PULONG count )
{
    return unix_funcs->NtAlertResumeThread( handle, count );
}


/******************************************************************************
 *              NtAlertThread   (NTDLL.@)
 *              ZwAlertThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtAlertThread( HANDLE handle )
{
    return unix_funcs->NtAlertThread( handle );
}


/******************************************************************************
 *              NtTerminateThread  (NTDLL.@)
 *              ZwTerminateThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtTerminateThread( HANDLE handle, LONG exit_code )
{
    return unix_funcs->NtTerminateThread( handle, exit_code );
}


/******************************************************************************
 *              NtQueueApcThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtQueueApcThread( HANDLE handle, PNTAPCFUNC func, ULONG_PTR arg1,
                                  ULONG_PTR arg2, ULONG_PTR arg3 )
{
    return unix_funcs->NtQueueApcThread( handle, func, arg1, arg2, arg3 );
}


/******************************************************************************
 *              RtlPushFrame  (NTDLL.@)
 */
void WINAPI RtlPushFrame( TEB_ACTIVE_FRAME *frame )
{
    frame->Previous = NtCurrentTeb()->ActiveFrame;
    NtCurrentTeb()->ActiveFrame = frame;
}


/******************************************************************************
 *              RtlPopFrame  (NTDLL.@)
 */
void WINAPI RtlPopFrame( TEB_ACTIVE_FRAME *frame )
{
    NtCurrentTeb()->ActiveFrame = frame->Previous;
}


/******************************************************************************
 *              RtlGetFrame  (NTDLL.@)
 */
TEB_ACTIVE_FRAME * WINAPI RtlGetFrame(void)
{
    return NtCurrentTeb()->ActiveFrame;
}


/***********************************************************************
 *              NtContinue  (NTDLL.@)
 */
NTSTATUS WINAPI NtContinue( CONTEXT *context, BOOLEAN alertable )
{
    return unix_funcs->NtContinue( context, alertable );
}


/***********************************************************************
 *              NtSetContextThread  (NTDLL.@)
 *              ZwSetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetContextThread( HANDLE handle, const CONTEXT *context )
{
    return unix_funcs->NtSetContextThread( handle, context );
}


/***********************************************************************
 *              NtGetContextThread  (NTDLL.@)
 *              ZwGetContextThread  (NTDLL.@)
 */
#ifndef __i386__
NTSTATUS WINAPI NtGetContextThread( HANDLE handle, CONTEXT *context )
{
    return unix_funcs->NtGetContextThread( handle, context );
}
#endif


/******************************************************************************
 *           NtSetLdtEntries   (NTDLL.@)
 *           ZwSetLdtEntries   (NTDLL.@)
 */
NTSTATUS WINAPI NtSetLdtEntries( ULONG sel1, LDT_ENTRY entry1, ULONG sel2, LDT_ENTRY entry2 )
{
    return unix_funcs->NtSetLdtEntries( sel1, entry1, sel2, entry2 );
}


/******************************************************************************
 *              NtQueryInformationThread  (NTDLL.@)
 *              ZwQueryInformationThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryInformationThread( HANDLE handle, THREADINFOCLASS class,
                                          void *data, ULONG length, ULONG *ret_len )
{
    return unix_funcs->NtQueryInformationThread( handle, class, data, length, ret_len );
}


/******************************************************************************
 *              NtSetInformationThread  (NTDLL.@)
 *              ZwSetInformationThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetInformationThread( HANDLE handle, THREADINFOCLASS class,
                                        LPCVOID data, ULONG length )
{
    return unix_funcs->NtSetInformationThread( handle, class, data, length );
}

/******************************************************************************
 * NtGetCurrentProcessorNumber (NTDLL.@)
 *
 * Return the processor, on which the thread is running
 *
 */
ULONG WINAPI NtGetCurrentProcessorNumber(void)
{
    return unix_funcs->NtGetCurrentProcessorNumber();
}
