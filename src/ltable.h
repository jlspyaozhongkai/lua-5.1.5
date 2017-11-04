/*
** $Id: ltable.h,v 2.10.1.1 2007/12/27 13:02:25 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"

//通过Table取其node指针数组
#define gnode(t,i)	(&(t)->node[i])
//Table node的key
#define gkey(n)		(&(n)->i_key.nk)
//Table node的value
#define gval(n)		(&(n)->i_val)
//Table node的next，是宏，支持赋值
#define gnext(n)	((n)->i_key.nk.next)
//Table 的key是一个值和next的组合，这里只取其值的部分，忽视next
#define key2tval(n)	(&(n)->i_key.tvk)


LUAI_FUNC const TValue *luaH_getnum (Table *t, int key);
LUAI_FUNC TValue *luaH_setnum (lua_State *L, Table *t, int key);
LUAI_FUNC const TValue *luaH_getstr (Table *t, TString *key);
LUAI_FUNC TValue *luaH_setstr (lua_State *L, Table *t, TString *key);
LUAI_FUNC const TValue *luaH_get (Table *t, const TValue *key);
LUAI_FUNC TValue *luaH_set (lua_State *L, Table *t, const TValue *key);
//创建Table 根据数组 和 map的 项数
LUAI_FUNC Table *luaH_new (lua_State *L, int narray, int lnhash);
//给Table的数组resize
LUAI_FUNC void luaH_resizearray (lua_State *L, Table *t, int nasize);
//释放Table
LUAI_FUNC void luaH_free (lua_State *L, Table *t);
LUAI_FUNC int luaH_next (lua_State *L, Table *t, StkId key);
LUAI_FUNC int luaH_getn (Table *t);


#if defined(LUA_DEBUG)
LUAI_FUNC Node *luaH_mainposition (const Table *t, const TValue *key);
LUAI_FUNC int luaH_isdummy (Node *n);
#endif


#endif
