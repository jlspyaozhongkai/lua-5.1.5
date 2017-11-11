/*
** $Id: ltm.h,v 2.6.1.1 2007/12/27 13:02:25 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM"
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_EQ,  /* last tag method with `fast' access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_DIV,
  TM_MOD,
  TM_POW,
  TM_UNM,
  TM_LEN,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_N		/* number of elements in the enum */
} TMS;


//在元表中，先判空，在判定标志位(加速判断有没有)，最后通过 转换过来的名字 在 元表中查找
#define gfasttm(g,et,e) ((et) == NULL ? NULL : \
  ((et)->flags & (1u<<(e))) ? NULL : luaT_gettm(et, e, (g)->tmname[e]))

//在元表里通过TM_INDEX等宏找元素
#define fasttm(l,et,e)	gfasttm(G(l), et, e)

//数据类型宏对应的数据类型名字
LUAI_DATA const char *const luaT_typenames[];


LUAI_FUNC const TValue *luaT_gettm (Table *events, TMS event, TString *ename);

//获得对象的 metatable，然后按照event对应的字符型key找数据，比如__index 等，都和元表的作用有关。
LUAI_FUNC const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o,
                                                       TMS event);
LUAI_FUNC void luaT_init (lua_State *L);

#endif
