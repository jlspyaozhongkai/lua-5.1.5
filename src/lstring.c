/*
** $Id: lstring.c,v 2.8.1.1 2007/12/27 13:02:25 roberto Exp $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#define lstring_c
#define LUA_CORE

#include "lua.h"

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"


//将全局字符串hash table进行resize（尝试）
void luaS_resize (lua_State *L, int newsize) {
  GCObject **newhash;
  stringtable *tb;
  int i;
  //如果正在gc则放弃本次尝试
  if (G(L)->gcstate == GCSsweepstring)
    return;  /* cannot resize during GC traverse */
  //分配hash表
  newhash = luaM_newvector(L, newsize, GCObject *);
  //全局字符串管理表
  tb = &G(L)->strt;
  //初始化hash表
  for (i=0; i<newsize; i++) newhash[i] = NULL;
  /* rehash */
  for (i=0; i<tb->size; i++) {
    GCObject *p = tb->hash[i];
    while (p) {  /* for each node in the list */
      GCObject *next = p->gch.next;  /* save next */
	  //遍历TString链表，取得hash，重新找位置
      unsigned int h = gco2ts(p)->hash;
      int h1 = lmod(h, newsize);  /* new position */
      lua_assert(cast_int(h%newsize) == lmod(h, newsize));
      p->gch.next = newhash[h1];  /* chain it */
      newhash[h1] = p;
      p = next;
    }
  }
  //释放和更新
  luaM_freearray(L, tb->hash, tb->size, TString *);
  tb->size = newsize;
  tb->hash = newhash;
}

//全新造一个TString
static TString *newlstr (lua_State *L, const char *str, size_t l,
                                       unsigned int h) {
  TString *ts;
  stringtable *tb;
  if (l+1 > (MAX_SIZET - sizeof(TString))/sizeof(char))
    luaM_toobig(L);		//超大，异常的处理值得研究
  //分配内存，包括TString和后边的字符串，另加一个\0
  ts = cast(TString *, luaM_malloc(L, (l+1)*sizeof(char)+sizeof(TString)));
  ts->tsv.len = l;						//字符串的长度
  ts->tsv.hash = h;						//hash是外边传过来的
  ts->tsv.marked = luaC_white(G(L));	//设定三个gc头
  ts->tsv.tt = LUA_TSTRING;
  ts->tsv.reserved = 0;
  memcpy(ts+1, str, l*sizeof(char));			//拷贝内存
  ((char *)(ts+1))[l] = '\0';  /* ending 0 */
  tb = &G(L)->strt;								//保存到全局表
  h = lmod(h, tb->size);
  ts->tsv.next = tb->hash[h];  /* chain new entry */
  tb->hash[h] = obj2gco(ts);
  tb->nuse++;									//维护计数
  //当字符串数量大于hash表尺寸，切不是往死大的时候，就扩充一下
  if (tb->nuse > cast(lu_int32, tb->size) && tb->size <= MAX_INT/2)	
    luaS_resize(L, tb->size*2);  /* too crowded */
  return ts;
}

//根据char * 创建TString
TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  GCObject *o;
  //首先对输入的字符串取hash，且是有规律的有选择的计算hash,跳着做
  unsigned int h = cast(unsigned int, l);  /* seed */
  size_t step = (l>>5)+1;  /* if string is too long, don't hash all its chars */
  size_t l1;
  for (l1=l; l1>=step; l1-=step)  /* compute hash */
    h = h ^ ((h<<5)+(h>>2)+cast(unsigned char, str[l1-1]));
  //从hashkey开始找到或者找不到的第一个节点一路向后
  for (o = G(L)->strt.hash[lmod(h, G(L)->strt.size)];
       o != NULL;
       o = o->gch.next) {
    //取得TString对象
    TString *ts = rawgco2ts(o);
	//长度相同的情况下，比对字符串的内容
    if (ts->tsv.len == l && (memcmp(str, getstr(ts), l) == 0)) {
      /* string may be dead */
      if (isdead(G(L), o)) changewhite(o);		//如果在死，则需要救活
      return ts;
    }
  }
  return newlstr(L, str, l, h);  /* not found */	//全新造一个字符串
}

//虽不知Table *e为何物，但是这个函数的功能是根据给定的尺寸造UData，就如同TString
//要不为什么放这个文件里
Udata *luaS_newudata (lua_State *L, size_t s, Table *e) {
  Udata *u;
  if (s > MAX_SIZET - sizeof(Udata))
    luaM_toobig(L);				//尺寸太大了不行
  //分配内存，这个不需要多一个\0
  u = cast(Udata *, luaM_malloc(L, s + sizeof(Udata)));

  //设置Gc头
  u->uv.marked = luaC_white(G(L));  /* is not finalized */
  u->uv.tt = LUA_TUSERDATA;
  //设置Udata数据
  u->uv.len = s;
  u->uv.metatable = NULL;
  u->uv.env = e;
  /* chain it on udata list (after main thread) */
  //挂在一个lua_State中的链表了，估计和释放有关
  u->uv.next = G(L)->mainthread->next;
  G(L)->mainthread->next = obj2gco(u);
  return u;
}

