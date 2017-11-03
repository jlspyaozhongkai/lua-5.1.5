/*
** $Id: lmem.h,v 1.31.1.1 2007/12/27 13:02:25 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "lua.h"

#define MEMERRMSG	"not enough memory"

//出去尺寸检查以外，主要是释放on*e的b和分配n*e的新
#define luaM_reallocv(L,b,on,n,e) \
	((cast(size_t, (n)+1) <= MAX_SIZET/(e)) ?  /* +1 to avoid warnings */ \
		luaM_realloc_(L, (b), (on)*(e), (n)*(e)) : \
		luaM_toobig(L))

//释放内存，需要制定字符串
#define luaM_freemem(L, b, s)	luaM_realloc_(L, (b), (s), 0)
//释放内存，b是指针，长度是sizeof，就是在释放指针
#define luaM_free(L, b)		luaM_realloc_(L, (b), sizeof(*(b)), 0)
//(自动检查)释放数组b，b的成员数是n
#define luaM_freearray(L, b, n, t)   luaM_reallocv(L, (b), n, 0, sizeof(t))


//分配内存
#define luaM_malloc(L,t)	luaM_realloc_(L, NULL, 0, (t))
//使用类型分配内存，还强转到制定的类型
#define luaM_new(L,t)		cast(t *, luaM_malloc(L, sizeof(t)))


//(自动检查)分配数组，数组成员数是n
#define luaM_newvector(L,n,t) \
		cast(t *, luaM_reallocv(L, NULL, 0, n, sizeof(t)))
//(自动检查)调整数组，如果vector的成员数nelems如果大于等于size，就开始调整
//使用luaM_growaux_调整
#define luaM_growvector(L,v,nelems,size,t,limit,e) \
          if ((nelems)+1 > (size)) \
            ((v)=cast(t *, luaM_growaux_(L,v,&(size),sizeof(t),limit,e)))
//(自动检查)重新分配向量，需要提供之前的指针和成员数
#define luaM_reallocvector(L, v,oldn,n,t) \
   ((v)=cast(t *, luaM_reallocv(L, v, oldn, n, sizeof(t))))


//内存分配或者释放函数，oldsize 进，newsize 返
LUAI_FUNC void *luaM_realloc_ (lua_State *L, void *block, size_t oldsize,
                                                          size_t size);
LUAI_FUNC void *luaM_toobig (lua_State *L);
LUAI_FUNC void *luaM_growaux_ (lua_State *L, void *block, int *size,
                               size_t size_elem, int limit,
                               const char *errormsg);

#endif

