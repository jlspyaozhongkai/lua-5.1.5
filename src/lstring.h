/*
** $Id: lstring.h,v 1.43.1.1 2007/12/27 13:02:25 roberto Exp $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#ifndef lstring_h
#define lstring_h


#include "lgc.h"
#include "lobject.h"
#include "lstate.h"

//TString的内存长度
#define sizestring(s)	(sizeof(union TString)+((s)->len+1)*sizeof(char))

//Udata的内存长度，因为不确定长度，所以要算一下
#define sizeudata(u)	(sizeof(union Udata)+(u)->len)

//对不知道长度的字符串创建TString，前提是受得了strlen
#define luaS_new(L, s)	(luaS_newlstr(L, s, strlen(s)))
//对一个静态字符串，创建TString，"" s 是套路
#define luaS_newliteral(L, s)	(luaS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))
//被设置过的对象没法被Gc
#define luaS_fix(s)	l_setbit((s)->tsv.marked, FIXEDBIT)

//重建hash表
LUAI_FUNC void luaS_resize (lua_State *L, int newsize);
//类似于luaS_newlstr，这个是创建Udata
LUAI_FUNC Udata *luaS_newudata (lua_State *L, size_t s, Table *e);
//根据跟定长度的char *，返回一个TString
LUAI_FUNC TString *luaS_newlstr (lua_State *L, const char *str, size_t l);


#endif
