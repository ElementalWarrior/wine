/*
 * msvcrt.dll C++ objects
 *
 * Copyright 2000 Jon Griffiths
 */

#include "msvcrt.h"
#include "msvcrt/eh.h"
#include "msvcrt/malloc.h"


DEFAULT_DEBUG_CHANNEL(msvcrt);


void _purecall(void);

typedef void (*v_table_ptr)();

static v_table_ptr exception_vtable[2];
static v_table_ptr bad_typeid_vtable[3];
static v_table_ptr __non_rtti_object_vtable[3];
static v_table_ptr bad_cast_vtable[3];
static v_table_ptr type_info_vtable[1];

typedef struct __exception
{
  v_table_ptr *vtable;
  const char *name;
  int do_free; /* FIXME: take string copy with char* ctor? */
} exception;

typedef struct __bad_typeid
{
  exception base;
} bad_typeid;

typedef struct ____non_rtti_object
{
  bad_typeid base;
} __non_rtti_object;

typedef struct __bad_cast
{
  exception base;
} bad_cast;

typedef struct __type_info
{
  v_table_ptr *vtable;
  void *data;
  char name[1];
} type_info;

/******************************************************************
 *		??0exception@@QAE@ABQBD@Z (MSVCRT.@)
 */
void MSVCRT_exception_ctor(exception * _this, const char ** name)
{
  TRACE("(%p %s)\n",_this,*name);
  _this->vtable = exception_vtable;
  _this->name = *name;
  TRACE("name = %s\n",_this->name);
  _this->do_free = 0; /* FIXME */
}

/******************************************************************
 *		??0exception@@QAE@ABV0@@Z (MSVCRT.@)
 */
void MSVCRT_exception_copy_ctor(exception * _this, const exception * rhs)
{
  TRACE("(%p %p)\n",_this,rhs);
  if (_this != rhs)
    memcpy (_this, rhs, sizeof (*_this));
  TRACE("name = %s\n",_this->name);
}

/******************************************************************
 *		??0exception@@QAE@XZ (MSVCRT.@)
 */
void MSVCRT_exception_default_ctor(exception * _this)
{
  TRACE("(%p)\n",_this);
  _this->vtable = exception_vtable;
  _this->name = "";
  _this->do_free = 0; /* FIXME */
}

/******************************************************************
 *		??1exception@@UAE@XZ (MSVCRT.@)
 */
void MSVCRT_exception_dtor(exception * _this)
{
  TRACE("(%p)\n",_this);
}

/******************************************************************
 *		??4exception@@QAEAAV0@ABV0@@Z (MSVCRT.@)
 */
exception * MSVCRT_exception_opequals(exception * _this, const exception * rhs)
{
  TRACE("(%p %p)\n",_this,rhs);
  memcpy (_this, rhs, sizeof (*_this));
  TRACE("name = %s\n",_this->name);
  return _this;
}

/******************************************************************
 *		??_Eexception@@UAEPAXI@Z (MSVCRT.@)
 */
void * MSVCRT_exception__unknown_E(exception * _this, unsigned int arg1)
{
  TRACE("(%p %d)\n",_this,arg1);
  _purecall();
  return NULL;
}

/******************************************************************
 *		??_Gexception@@UAEPAXI@Z (MSVCRT.@)
 */
void * MSVCRT_exception__unknown_G(exception * _this, unsigned int arg1)
{
  TRACE("(%p %d)\n",_this,arg1);
  _purecall();
  return NULL;
}

/******************************************************************
 *		?what@exception@@UBEPBDXZ (MSVCRT.@)
 */
const char * __stdcall MSVCRT_exception_what(exception * _this)
{
  TRACE("(%p) returning %s\n",_this,_this->name);
  return _this->name;
}


static terminate_function func_terminate=NULL;
static unexpected_function func_unexpected=NULL;

/******************************************************************
 *		?set_terminate@@YAP6AXXZP6AXXZ@Z (MSVCRT.@)
 */
terminate_function MSVCRT_set_terminate(terminate_function func)
{
  terminate_function previous=func_terminate;
  TRACE("(%p) returning %p\n",func,previous);
  func_terminate=func;
  return previous;
}

/******************************************************************
 *		?set_unexpected@@YAP6AXXZP6AXXZ@Z (MSVCRT.@)
 */
unexpected_function MSVCRT_set_unexpected(unexpected_function func)
{
  unexpected_function previous=func_unexpected;
  TRACE("(%p) returning %p\n",func,previous);
  func_unexpected=func;
  return previous;
}

/******************************************************************
 *		?terminate@@YAXXZ (MSVCRT.@)
 */
void MSVCRT_terminate()
{
  (*func_terminate)();
}

/******************************************************************
 *		?unexpected@@YAXXZ (MSVCRT.@)
 */
void MSVCRT_unexpected()
{
  (*func_unexpected)();
}


/******************************************************************
 *		??0bad_typeid@@QAE@ABV0@@Z (MSVCRT.@)
 */
void MSVCRT_bad_typeid_copy_ctor(bad_typeid * _this, const bad_typeid * rhs)
{
  TRACE("(%p %p)\n",_this,rhs);
  MSVCRT_exception_copy_ctor(&_this->base,&rhs->base);
}

/******************************************************************
 *		??0bad_typeid@@QAE@PBD@Z (MSVCRT.@)
 */
void MSVCRT_bad_typeid_ctor(bad_typeid * _this, const char * name)
{
  TRACE("(%p %s)\n",_this,name);
  MSVCRT_exception_ctor(&_this->base, &name);
  _this->base.vtable = bad_typeid_vtable;
}

/******************************************************************
 *		??1bad_typeid@@UAE@XZ (MSVCRT.@)
 */
void MSVCRT_bad_typeid_dtor(bad_typeid * _this)
{
  TRACE("(%p)\n",_this);
  MSVCRT_exception_dtor(&_this->base);
}

/******************************************************************
 *		??4bad_typeid@@QAEAAV0@ABV0@@Z (MSVCRT.@)
 */
bad_typeid * MSVCRT_bad_typeid_opequals(bad_typeid * _this, const bad_typeid * rhs)
{
  TRACE("(%p %p)\n",_this,rhs);
  MSVCRT_exception_copy_ctor(&_this->base,&rhs->base);
  return _this;
}

/******************************************************************
 *		??0__non_rtti_object@@QAE@ABV0@@Z (MSVCRT.@)
 */
void MSVCRT___non_rtti_object_copy_ctor(__non_rtti_object * _this,
                                                const __non_rtti_object * rhs)
{
  TRACE("(%p %p)\n",_this,rhs);
  MSVCRT_bad_typeid_copy_ctor(&_this->base,&rhs->base);
}

/******************************************************************
 *		??0__non_rtti_object@@QAE@PBD@Z (MSVCRT.@)
 */
void MSVCRT___non_rtti_object_ctor(__non_rtti_object * _this,
                                           const char * name)
{
  TRACE("(%p %s)\n",_this,name);
  MSVCRT_bad_typeid_ctor(&_this->base,name);
  _this->base.base.vtable = __non_rtti_object_vtable;
}

/******************************************************************
 *		??1__non_rtti_object@@UAE@XZ (MSVCRT.@)
 */
void MSVCRT___non_rtti_object_dtor(__non_rtti_object * _this)
{
  TRACE("(%p)\n",_this);
  MSVCRT_bad_typeid_dtor(&_this->base);
}

/******************************************************************
 *		??4__non_rtti_object@@QAEAAV0@ABV0@@Z (MSVCRT.@)
 */
__non_rtti_object * MSVCRT___non_rtti_object_opequals(__non_rtti_object * _this,
                                                              const __non_rtti_object *rhs)
{
  TRACE("(%p %p)\n",_this,rhs);
  memcpy (_this, rhs, sizeof (*_this));
  TRACE("name = %s\n",_this->base.base.name);
  return _this;
}

/******************************************************************
 *		??_E__non_rtti_object@@UAEPAXI@Z (MSVCRT.@)
 */
void * MSVCRT___non_rtti_object__unknown_E(__non_rtti_object * _this, unsigned int arg1)
{
  TRACE("(%p %d)\n",_this,arg1);
  _purecall();
  return NULL;
}

/******************************************************************
 *		??_G__non_rtti_object@@UAEPAXI@Z (MSVCRT.@)
 */
void * MSVCRT___non_rtti_object__unknown_G(__non_rtti_object * _this, unsigned int arg1)
{
  TRACE("(%p %d)\n",_this,arg1);
  _purecall();
  return NULL;
}

/******************************************************************
 *		??0bad_cast@@QAE@ABQBD@Z (MSVCRT.@)
 */
void MSVCRT_bad_cast_ctor(bad_cast * _this, const char ** name)
{
  TRACE("(%p %s)\n",_this,*name);
  MSVCRT_exception_ctor(&_this->base, name);
  _this->base.vtable = bad_cast_vtable;
}

/******************************************************************
 *		??0bad_cast@@QAE@ABV0@@Z (MSVCRT.@)
 */
void MSVCRT_bad_cast_copy_ctor(bad_cast * _this, const bad_cast * rhs)
{
  TRACE("(%p %p)\n",_this,rhs);
  MSVCRT_exception_copy_ctor(&_this->base,&rhs->base);
}

/******************************************************************
 *		??1bad_cast@@UAE@XZ (MSVCRT.@)
 */
void MSVCRT_bad_cast_dtor(bad_cast * _this)
{
  TRACE("(%p)\n",_this);
  MSVCRT_exception_dtor(&_this->base);
}

/******************************************************************
 *		??4bad_cast@@QAEAAV0@ABV0@@Z (MSVCRT.@)
 */
bad_cast * MSVCRT_bad_cast_opequals(bad_cast * _this, const bad_cast * rhs)
{
  TRACE("(%p %p)\n",_this,rhs);
  MSVCRT_exception_copy_ctor(&_this->base,&rhs->base);
  return _this;
}

/******************************************************************
 *		??8type_info@@QBEHABV0@@Z (MSVCRT.@)
 */
int __stdcall MSVCRT_type_info_opequals_equals(type_info * _this, const type_info * rhs)
{
  TRACE("(%p %p) returning %d\n",_this,rhs,_this->name == rhs->name);
  return _this->name == rhs->name;
}

/******************************************************************
 *		??9type_info@@QBEHABV0@@Z (MSVCRT.@)
 */
int __stdcall MSVCRT_type_info_opnot_equals(type_info * _this, const type_info * rhs)
{
  TRACE("(%p %p) returning %d\n",_this,rhs,_this->name == rhs->name);
  return _this->name != rhs->name;
}

/******************************************************************
 *		??1type_info@@UAE@XZ (MSVCRT.@)
 */
void MSVCRT_type_info_dtor(type_info * _this)
{
  TRACE("(%p)\n",_this);
  if (_this->data)
    MSVCRT_free(_this->data);
}

/******************************************************************
 *		?name@type_info@@QBEPBDXZ (MSVCRT.@)
 */
const char * __stdcall MSVCRT_type_info_name(type_info * _this)
{
  TRACE("(%p) returning %s\n",_this,_this->name);
  return _this->name;
}

/******************************************************************
 *		?raw_name@type_info@@QBEPBDXZ (MSVCRT.@)
 */
const char * __stdcall MSVCRT_type_info_raw_name(type_info * _this)
{
  TRACE("(%p) returning %s\n",_this,_this->name);
  return _this->name;
}


/* INTERNAL: Set up vtables
 * FIXME:should be static, cope with versions?
 */
void msvcrt_init_vtables(void)
{
  exception_vtable[0] = MSVCRT_exception_dtor;
  exception_vtable[1] = (void*)MSVCRT_exception_what;

  bad_typeid_vtable[0] = MSVCRT_bad_typeid_dtor;
  bad_typeid_vtable[1] = exception_vtable[1];
  bad_typeid_vtable[2] = _purecall; /* FIXME */

  __non_rtti_object_vtable[0] = MSVCRT___non_rtti_object_dtor;
  __non_rtti_object_vtable[1] = bad_typeid_vtable[1];
  __non_rtti_object_vtable[2] = bad_typeid_vtable[2];

  bad_cast_vtable[0] = MSVCRT_bad_cast_dtor;
  bad_cast_vtable[1] = exception_vtable[1];
  bad_cast_vtable[2] = _purecall; /* FIXME */

  type_info_vtable[0] = MSVCRT_type_info_dtor;

}

