/*
** $Id: lua.h,v 1.218.1.7 2012/01/13 20:36:20 roberto Exp $
** Lua - An Extensible Extension Language
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** See Copyright Notice at the end of this file
*/


#ifndef lua_h
#define lua_h

#include <stdarg.h>
#include <stddef.h>


#include "luaconf.h"


#define LUA_VERSION	"Lua 5.1"
#define LUA_RELEASE	"Lua 5.1.5"
#define LUA_VERSION_NUM	501
#define LUA_COPYRIGHT	"Copyright (C) 1994-2012 Lua.org, PUC-Rio"
#define LUA_AUTHORS 	"R. Ierusalimschy, L. H. de Figueiredo & W. Celes"


/* mark for precompiled code (`<esc>Lua') */
#define	LUA_SIGNATURE	"\033Lua"

/* option for multiple returns in `lua_pcall' and `lua_call' */
#define LUA_MULTRET	(-1)

//
//http://www.lua.org/manual/5.1/
//

/*
** pseudo-indices
*/
//为了避免这个问题，Lua提供了一个独立的被称为registry的表，C代码可以自由使用，但Lua代码不能访问他。
//http://www.lua.org/manual/5.1/manual.html#pdf-LUA_REGISTRYINDEX
//指定调用栈上索引时，会使用这些特殊宏定义
                                                        //-1 到 LUA_REGISTRYINDEX 之间是从栈的top往下找索引
#define LUA_REGISTRYINDEX	(-10000)					//注册表的调用栈假索引
#define LUA_ENVIRONINDEX	(-10001)					//C 函数所对应的 环境
#define LUA_GLOBALSINDEX	(-10002)					//Lua 中的全局环境
#define lua_upvalueindex(i)	(LUA_GLOBALSINDEX-(i))		//比LUA_GLOBALSINDEX数更小的是upvalue的索引了


/* thread status; 0 is OK */
//线程的状态
#define LUA_YIELD	1
#define LUA_ERRRUN	2
#define LUA_ERRSYNTAX	3
#define LUA_ERRMEM	4
#define LUA_ERRERR	5


typedef struct lua_State lua_State;

typedef int (*lua_CFunction) (lua_State *L);


/*
** functions that read/write blocks when loading/dumping Lua chunks
*/
typedef const char * (*lua_Reader) (lua_State *L, void *ud, size_t *sz);

typedef int (*lua_Writer) (lua_State *L, const void* p, size_t sz, void* ud);


/*
** prototype for memory-allocation functions
*/
//内存分配函数的回调
typedef void * (*lua_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** basic types
*/
#define LUA_TNONE		(-1)			//数据类型，什么都不类型

#define LUA_TNIL		0				//数据类型nil
#define LUA_TBOOLEAN		1			//数据类型bool型
#define LUA_TLIGHTUSERDATA	2			//数据类型指针
#define LUA_TNUMBER		3				//数据类型number
//从这个位置往上的是不需要gc的，从这个位置往下的是需要gc的
#define LUA_TSTRING		4				//需要回收的string
#define LUA_TTABLE		5				//需要回收的table
#define LUA_TFUNCTION		6			//需要回收的函数，函数第一类值，是可以执行的数据
#define LUA_TUSERDATA		7			//需要回收的 通用数据
#define LUA_TTHREAD		8				//需要回收的 伪线程



/* minimum Lua stack available to a C function */
#define LUA_MINSTACK	20


/*
** generic extra include file
*/
#if defined(LUA_USER_H)
#include LUA_USER_H
#endif


/* type of numbers in Lua */
typedef LUA_NUMBER lua_Number;					//Lua 的number类型，一般是double类型


/* type for integer functions */
typedef LUA_INTEGER lua_Integer;				//好像是做指针用的



/*
** state manipulation
*/
LUA_API lua_State *(lua_newstate) (lua_Alloc f, void *ud);
LUA_API void       (lua_close) (lua_State *L);
LUA_API lua_State *(lua_newthread) (lua_State *L);

LUA_API lua_CFunction (lua_atpanic) (lua_State *L, lua_CFunction panicf);


/*
** basic stack manipulation
*/
//调用栈操作 取top
LUA_API int   (lua_gettop) (lua_State *L);
//调用栈操作      填充调用栈 使用nil
LUA_API void  (lua_settop) (lua_State *L, int idx);
//调用栈操作 取调用栈上指定位置的值 压入调用栈
LUA_API void  (lua_pushvalue) (lua_State *L, int idx);
//调用栈操作 抽掉指定的变量
LUA_API void  (lua_remove) (lua_State *L, int idx);
//调用栈操作 在指定的位置插入top
LUA_API void  (lua_insert) (lua_State *L, int idx);
//调用栈操作 在调用栈上替换 TODO 没分析完
LUA_API void  (lua_replace) (lua_State *L, int idx);
//调用栈操作 TODO 没分析完
LUA_API int   (lua_checkstack) (lua_State *L, int sz);
//调用栈操作， 状态机之间转义 变量
LUA_API void  (lua_xmove) (lua_State *from, lua_State *to, int n);


/*
** access functions (stack -> C)
*/
//调用栈上值是否为number
LUA_API int             (lua_isnumber) (lua_State *L, int idx);
//调用栈上值是否为string
LUA_API int             (lua_isstring) (lua_State *L, int idx);
//调用栈上值是否为c 函数
LUA_API int             (lua_iscfunction) (lua_State *L, int idx);
//调用栈上值是否为userdata或者指针
LUA_API int             (lua_isuserdata) (lua_State *L, int idx);
//调用栈上值得type，nil类型无type，其他类型有type
LUA_API int             (lua_type) (lua_State *L, int idx);
//调用栈上值得type, 但是返回的是字符串（描述信息）
LUA_API const char     *(lua_typename) (lua_State *L, int tp);
//调用栈上 两值是否相等
LUA_API int            (lua_equal) (lua_State *L, int idx1, int idx2);
//调用栈上 两值 底层比较
LUA_API int            (lua_rawequal) (lua_State *L, int idx1, int idx2);
//调用栈上 两值 小于比较
LUA_API int            (lua_lessthan) (lua_State *L, int idx1, int idx2);
//--------下面的操作经常会改变对象本身，就是将对象做转换了。
//调用栈上值 转 number
LUA_API lua_Number      (lua_tonumber) (lua_State *L, int idx);
//调用栈上值 转 int
LUA_API lua_Integer     (lua_tointeger) (lua_State *L, int idx);
//调用栈上值 转 boolean，主要看是否为nil 和  false
LUA_API int             (lua_toboolean) (lua_State *L, int idx);
//调用栈上值 转 string
LUA_API const char     *(lua_tolstring) (lua_State *L, int idx, size_t *len);
//调用栈上值 的长度
LUA_API size_t          (lua_objlen) (lua_State *L, int idx);
//调用栈上值 转 c函数
LUA_API lua_CFunction   (lua_tocfunction) (lua_State *L, int idx);
//调用栈上值 转指针
LUA_API void	       *(lua_touserdata) (lua_State *L, int idx);
//调用栈上值 转线程
LUA_API lua_State      *(lua_tothread) (lua_State *L, int idx);
//调用栈上值 转线程, 不过这个类型多
LUA_API const void     *(lua_topointer) (lua_State *L, int idx);


/*
** push functions (C -> stack)
*/
//往调用栈里压入 nil
LUA_API void  (lua_pushnil) (lua_State *L);
//往调用栈里压入 number
LUA_API void  (lua_pushnumber) (lua_State *L, lua_Number n);
//往调用栈里压入 int
LUA_API void  (lua_pushinteger) (lua_State *L, lua_Integer n);
//往调用栈里压入 带长度的string
LUA_API void  (lua_pushlstring) (lua_State *L, const char *s, size_t l);
//往调用栈里压入 不带长度的string
LUA_API void  (lua_pushstring) (lua_State *L, const char *s);
//往调用栈里压入 print va
LUA_API const char *(lua_pushvfstring) (lua_State *L, const char *fmt,
                                                      va_list argp);
//往调用栈里压入 print 格式串
LUA_API const char *(lua_pushfstring) (lua_State *L, const char *fmt, ...);
//往调用栈里压入 函数
LUA_API void  (lua_pushcclosure) (lua_State *L, lua_CFunction fn, int n);
//往调用栈里压入 boolean
LUA_API void  (lua_pushboolean) (lua_State *L, int b);
//往调用栈里压入 指针
LUA_API void  (lua_pushlightuserdata) (lua_State *L, void *p);
//往调用栈里压入 线程
LUA_API int   (lua_pushthread) (lua_State *L);


/*
** get functions (Lua -> stack)
*/
//lua运行时，读取table
LUA_API void  (lua_gettable) (lua_State *L, int idx);
LUA_API void  (lua_getfield) (lua_State *L, int idx, const char *k);
LUA_API void  (lua_rawget) (lua_State *L, int idx);
LUA_API void  (lua_rawgeti) (lua_State *L, int idx, int n);
LUA_API void  (lua_createtable) (lua_State *L, int narr, int nrec);
LUA_API void *(lua_newuserdata) (lua_State *L, size_t sz);
LUA_API int   (lua_getmetatable) (lua_State *L, int objindex);
LUA_API void  (lua_getfenv) (lua_State *L, int idx);


/*
** set functions (stack -> Lua)
*/
//lua运行时，写入table
LUA_API void  (lua_settable) (lua_State *L, int idx);
LUA_API void  (lua_setfield) (lua_State *L, int idx, const char *k);
LUA_API void  (lua_rawset) (lua_State *L, int idx);
LUA_API void  (lua_rawseti) (lua_State *L, int idx, int n);
LUA_API int   (lua_setmetatable) (lua_State *L, int objindex);
LUA_API int   (lua_setfenv) (lua_State *L, int idx);


/*
** `load' and `call' functions (load and run Lua code)
*/
LUA_API void  (lua_call) (lua_State *L, int nargs, int nresults);
LUA_API int   (lua_pcall) (lua_State *L, int nargs, int nresults, int errfunc);
LUA_API int   (lua_cpcall) (lua_State *L, lua_CFunction func, void *ud);
LUA_API int   (lua_load) (lua_State *L, lua_Reader reader, void *dt,
                                        const char *chunkname);

LUA_API int (lua_dump) (lua_State *L, lua_Writer writer, void *data);


/*
** coroutine functions
*/
LUA_API int  (lua_yield) (lua_State *L, int nresults);
LUA_API int  (lua_resume) (lua_State *L, int narg);
LUA_API int  (lua_status) (lua_State *L);

/*
** garbage-collection function and options
*/

#define LUA_GCSTOP		0
#define LUA_GCRESTART		1
#define LUA_GCCOLLECT		2
#define LUA_GCCOUNT		3
#define LUA_GCCOUNTB		4
#define LUA_GCSTEP		5
#define LUA_GCSETPAUSE		6
#define LUA_GCSETSTEPMUL	7

LUA_API int (lua_gc) (lua_State *L, int what, int data);


/*
** miscellaneous functions
*/

LUA_API int   (lua_error) (lua_State *L);

LUA_API int   (lua_next) (lua_State *L, int idx);

LUA_API void  (lua_concat) (lua_State *L, int n);

LUA_API lua_Alloc (lua_getallocf) (lua_State *L, void **ud);
LUA_API void lua_setallocf (lua_State *L, lua_Alloc f, void *ud);



/* 
** ===============================================================
** some useful macros
** ===============================================================
*/

#define lua_pop(L,n)		lua_settop(L, -(n)-1)

#define lua_newtable(L)		lua_createtable(L, 0, 0)

#define lua_register(L,n,f) (lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))

#define lua_pushcfunction(L,f)	lua_pushcclosure(L, (f), 0)

#define lua_strlen(L,i)		lua_objlen(L, (i))

//判断栈上值是否为function
#define lua_isfunction(L,n)	(lua_type(L, (n)) == LUA_TFUNCTION)
//判断栈上值是否为table
#define lua_istable(L,n)	(lua_type(L, (n)) == LUA_TTABLE)
//判断栈上值是否为指针
#define lua_islightuserdata(L,n)	(lua_type(L, (n)) == LUA_TLIGHTUSERDATA)
//判断栈上值是否为nil
#define lua_isnil(L,n)		(lua_type(L, (n)) == LUA_TNIL)
//判断栈上值是否为boolen
#define lua_isboolean(L,n)	(lua_type(L, (n)) == LUA_TBOOLEAN)
//判断栈上值是否为 线程
#define lua_isthread(L,n)	(lua_type(L, (n)) == LUA_TTHREAD)
//判断栈上值是否为 None,none是什么数据？
#define lua_isnone(L,n)		(lua_type(L, (n)) == LUA_TNONE)
//判断栈上值是否为 不怎样
#define lua_isnoneornil(L, n)	(lua_type(L, (n)) <= 0)
//lua_pushlstring 静态字符版
#define lua_pushliteral(L, s)	\
	lua_pushlstring(L, "" s, (sizeof(s)/sizeof(char))-1)

#define lua_setglobal(L,s)	lua_setfield(L, LUA_GLOBALSINDEX, (s))
#define lua_getglobal(L,s)	lua_getfield(L, LUA_GLOBALSINDEX, (s))

#define lua_tostring(L,i)	lua_tolstring(L, (i), NULL)



/*
** compatibility macros and functions
*/

#define lua_open()	luaL_newstate()

#define lua_getregistry(L)	lua_pushvalue(L, LUA_REGISTRYINDEX)

#define lua_getgccount(L)	lua_gc(L, LUA_GCCOUNT, 0)

#define lua_Chunkreader		lua_Reader
#define lua_Chunkwriter		lua_Writer


/* hack */
LUA_API void lua_setlevel	(lua_State *from, lua_State *to);


/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define LUA_HOOKCALL	0
#define LUA_HOOKRET	1
#define LUA_HOOKLINE	2
#define LUA_HOOKCOUNT	3
#define LUA_HOOKTAILRET 4


/*
** Event masks
*/
#define LUA_MASKCALL	(1 << LUA_HOOKCALL)
#define LUA_MASKRET	(1 << LUA_HOOKRET)
#define LUA_MASKLINE	(1 << LUA_HOOKLINE)
#define LUA_MASKCOUNT	(1 << LUA_HOOKCOUNT)

typedef struct lua_Debug lua_Debug;  /* activation record */


/* Functions to be called by the debuger in specific events */
typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);


LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar);
LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar);
LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *lua_getupvalue (lua_State *L, int funcindex, int n);
LUA_API const char *lua_setupvalue (lua_State *L, int funcindex, int n);

LUA_API int lua_sethook (lua_State *L, lua_Hook func, int mask, int count);
LUA_API lua_Hook lua_gethook (lua_State *L);
LUA_API int lua_gethookmask (lua_State *L);
LUA_API int lua_gethookcount (lua_State *L);


struct lua_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) `global', `local', `field', `method' */
  const char *what;	/* (S) `Lua', `C', `main', `tail' */
  const char *source;	/* (S) */
  int currentline;	/* (l) */
  int nups;		/* (u) number of upvalues */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  char short_src[LUA_IDSIZE]; /* (S) */
  /* private part */
  int i_ci;  /* active function */
};

/* }====================================================================== */


/******************************************************************************
* Copyright (C) 1994-2012 Lua.org, PUC-Rio.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/


#endif
