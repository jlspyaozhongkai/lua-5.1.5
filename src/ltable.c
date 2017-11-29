/*
** $Id: ltable.c,v 2.32.1.2 2007/12/28 15:32:23 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/


/*
** Implementation of tables (aka arrays, objects, or hash tables).
** Tables keep its elements in two parts: an array part and a hash part.
** Non-negative integer keys are all candidates to be kept in the array
** part. The actual size of the array is the largest `n' such that at
** least half the slots between 0 and n are in use.
** Hash uses a mix of chained scatter table with Brent's variation.
** A main invariant of these tables is that, if an element is not
** in its main position (i.e. the `original' position that its hash gives
** to it), then the colliding element is in its own main position.
** Hence even when the load factor reaches 100%, performance remains good.
*/

#include <math.h>
#include <string.h>

#define ltable_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"


/*
** max size of array part is 2^MAXBITS
*/
#if LUAI_BITSINT > 26
#define MAXBITS		26
#else
#define MAXBITS		(LUAI_BITSINT-2)
#endif

#define MAXASIZE	(1 << MAXBITS)

#define hashpow2(t,n)      (gnode(t, lmod((n), sizenode(t))))			//通过各种值来的hash，在map表中定位首个Node*，对各种hash值按照node部分尺寸求余数

#define hashstr(t,str)  hashpow2(t, (str)->tsv.hash)					//先从TString中取现成的hash值，再通过hash值定位到Node*

#define hashboolean(t,p)        hashpow2(t, p)							//通过bool值，在hash表中定位Node*


/*
** for some types, it is better to avoid modulus by power of 2, as
** they tend to have many 2 factors.
*/
#define hashmod(t,n)	(gnode(t, ((n) % ((sizenode(t)-1)|1))))			//矫正过的hashpow2，因为有些数据太2幂对齐了

#define hashpointer(t,p)	hashmod(t, IntPoint(p))						//指针做输入，定位Node*


/*
** number of ints inside a lua_Number
*/
#define numints		cast_int(sizeof(lua_Number)/sizeof(int))			//尺寸上，number抵几个int

#define dummynode		(&dummynode_)									//哑表项通常作为指针使用

static const Node dummynode_ = {										//Table map 的哑表项目
  {{NULL}, LUA_TNIL},  /* value */										//value的值
  {{{NULL}, LUA_TNIL, NULL}}  /* key */									//key的值，包括它的next指针
};


/*
** hash for lua_Numbers
*/
static Node *hashnum (const Table *t, lua_Number n) {					//在Table中，通过number 算hash，返回第一个Node
  unsigned int a[numints];
  int i;
  if (luai_numeq(n, 0))  /* avoid problems with -0 */
    return gnode(t, 0);													//0 不用计算直接返回
  memcpy(a, &n, sizeof(a));
  for (i = 1; i < numints; i++) a[0] += a[i];							//将Number散列到一个uint上
  return hashmod(t, a[0]);												//数值散列定位到首个Node
}

/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
static Node *mainposition (const Table *t, const TValue *key) {		//各种通过key的TValue定位Node
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));									//Number 使用自身的值做hash
    case LUA_TSTRING:
      return hashstr(t, rawtsvalue(key));								//TString 的指针，是gc对象的指针
    case LUA_TBOOLEAN:
      return hashboolean(t, bvalue(key));								//Boolean 使用值做hash
    case LUA_TLIGHTUSERDATA:
      return hashpointer(t, pvalue(key));								//指针类型地址
    default:
      return hashpointer(t, gcvalue(key));								//其他类型使用使用gc对象的指针
  }
}


/*
** returns the index for `key' if `key' is an appropriate key to live in
** the array part of the table, -1 otherwise.
*/
static int arrayindex (const TValue *key) {								//输入key 返回数组索引，必须是number int 类型的
  if (ttisnumber(key)) {
    lua_Number n = nvalue(key);
    int k;
    lua_number2int(k, n);
    if (luai_numeq(cast_num(k), n))
      return k;
  }
  return -1;  /* `key' did not match some condition */					//否则返回-1
}


/*
** returns the index of a `key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning of a traversal is signalled by -1.
*/
static int findindex (lua_State *L, Table *t, StkId key) {
  int i;
  if (ttisnil(key)) return -1;  /* first iteration */
  i = arrayindex(key);
  if (0 < i && i <= t->sizearray)  /* is `key' inside array part? */
    return i-1;  /* yes; that's the index (corrected to C) */
  else {
    Node *n = mainposition(t, key);
    do {  /* check whether `key' is somewhere in the chain */
      /* key may be dead already, but it is ok to use it in `next' */
      if (luaO_rawequalObj(key2tval(n), key) ||
            (ttype(gkey(n)) == LUA_TDEADKEY && iscollectable(key) &&
             gcvalue(gkey(n)) == gcvalue(key))) {
        i = cast_int(n - gnode(t, 0));  /* key index in hash table */
        /* hash elements are numbered after array ones */
        return i + t->sizearray;
      }
      else n = gnext(n);
    } while (n);
    luaG_runerror(L, "invalid key to " LUA_QL("next"));  /* key not found */
    return 0;  /* to avoid warnings */
  }
}


int luaH_next (lua_State *L, Table *t, StkId key) {
  int i = findindex(L, t, key);  /* find original element */
  for (i++; i < t->sizearray; i++) {  /* try first array part */
    if (!ttisnil(&t->array[i])) {  /* a non-nil value? */
      setnvalue(key, cast_num(i+1));
      setobj2s(L, key+1, &t->array[i]);
      return 1;
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {  /* then hash part */
    if (!ttisnil(gval(gnode(t, i)))) {  /* a non-nil value? */
      setobj2s(L, key, key2tval(gnode(t, i)));
      setobj2s(L, key+1, gval(gnode(t, i)));
      return 1;
    }
  }
  return 0;  /* no more elements */
}


/*
** {=============================================================
** Rehash
** ==============================================================
*/


static int computesizes (int nums[], int *narray) {								//计算新的数组部分信息 （计算数组的尺寸和统计数值key的数量）
  int i;																		//依次扫描，满足条件时就设置，最后一次被设置的就是尺寸
  int twotoi;  /* 2^i */														//2的i次幂，这个值的一般很有意义
  int a = 0;  /* number of elements smaller than 2^i */							//已经被扫描过的部分，数字键的数量
  int na = 0;  /* number of elements to go to array part */						//返回值，用于从总数中减去
  int n = 0;  /* optimal size for array part */									//出参，用于创建array
  for (i = 0, twotoi = 1; twotoi/2 < *narray; i++, twotoi *= 2) {
    if (nums[i] > 0) {															//如果区间内有下标，才进入处理，否则没意义。
      a += nums[i];																//开始有意义的计算，统计过往数值key的数量
      if (a > twotoi/2) {  /* more than half elements present? */				//当前总数超过，幂值的半数，就开始更新
        n = twotoi;  /* optimal size (till now) */								//创建数组的大小
        na = a;  /* all elements smaller than n will go to array part */		//进入数组数值的数量（到时候从总数中减去）
      }
    }
    if (a == *narray) break;  /* all elements already counted */				//后边不会再有数值key了，提前结束
  }
  *narray = n;																	//出参，数组的大小
  lua_assert(*narray/2 <= na && na <= *narray);
  return na;																	//返回数值键的数量
}


static int countint (const TValue *key, int *nums) {							//如果key是数字下标，则返回 1 个，并更新nums 分布数组
  int k = arrayindex(key);
  if (0 < k && k <= MAXASIZE) {  /* is `key' an appropriate array index? */
    nums[ceillog2(k)]++;  /* count as such */
    return 1;
  }
  else
    return 0;
}


static int numusearray (const Table *t, int *nums) {							//计算array部分尺寸，并且将 2^(lg-1) 到 2^lg 区间里的非nil记录到nums
  int lg;																		//log值
  int ttlg;  /* 2^lg */															//返回值，array use
  int ause = 0;  /* summation of `nums' */										//
  int i = 1;  /* count to traverse all array keys */
  for (lg=0, ttlg=1; lg<=MAXBITS; lg++, ttlg*=2) {  /* for each slice */		//lg 每次加一，ttlg 每次乘以2，尝试递增
    int lc = 0;  /* counter */
    int lim = ttlg;																//(2^(lg-1) 到 2^lg 区间非nil统计值
    if (lim > t->sizearray) {													//但是，区间不能超过 array部分的尺寸
      lim = t->sizearray;  /* adjust upper limit */
      if (i > lim)																//i是上一轮检查停留的位置，超过lim就没有必要再执行了。
        break;  /* no more elements to count */
    }
    /* count elements in range (2^(lg-1), 2^lg] */
    for (; i <= lim; i++) {														//检查 2^(lg-1) 到 2^lg 区间内，有多少非nil
      if (!ttisnil(&t->array[i-1]))
        lc++;
    }
    nums[lg] += lc;																//将区间统计值保存到nums
    ause += lc;																	//累加一个总数
  }
  return ause;
}


static int numusehash (const Table *t, int *nums, int *pnasize) {				//计算hash部分尺寸通过结果返回，并且期间统计hash中数值下标的数量通过pnasize，并更新分布数组
  int totaluse = 0;  /* total number of elements */								//返回结果，总数
  int ause = 0;  /* summation of `nums' */										//
  int i = sizenode(t);															//现在的node数量
  while (i--) {																	//遍历所有的node
    Node *n = &t->node[i];
    if (!ttisnil(gval(n))) {													//只看value，没看key，估计有值的value，key也都不是空
      ause += countint(key2tval(n), nums);										//如果是数字下标，和array一样被统计
      totaluse++;																//总数
    }
  }
  *pnasize += ause;																//数组下标的，数量增加。
  return totaluse;
}

static void setarrayvector (lua_State *L, Table *t, int size) {			//给Table array分配内存，有时候是重新分配内存
  int i;
  luaM_reallocvector(L, t->array, t->sizearray, size, TValue);			//重新分配内存
  for (i=t->sizearray; i<size; i++)										//新增部分设置为nil
     setnilvalue(&t->array[i]);
  t->sizearray = size;													//更新长度
}

static void setnodevector (lua_State *L, Table *t, int size) {			//给Table map分配内存
  int lsize;
  if (size == 0) {  /* no elements to hash part? */						//0长度的map表
    t->node = cast(Node *, dummynode);  /* use common `dummynode' */
    lsize = 0;
  }
  else {																//分配map表，不过这里没见着map表扩充
    int i;
    lsize = ceillog2(size);												//计算map表的尺寸 的log值
    if (lsize > MAXBITS)
      luaG_runerror(L, "table overflow");								//太大了
    size = twoto(lsize);												//重新计算size，log 对齐过的
    t->node = luaM_newvector(L, size, Node);							//重新分配map表内存
    for (i=0; i<size; i++) {
      Node *n = gnode(t, i);											//Table map表，表项的指针
      gnext(n) = NULL;													//下一个节点是空
      setnilvalue(gkey(n));												//当前节点的key为nil
      setnilvalue(gval(n));												//当前节点的值设置为nil
    }
  }
  t->lsizenode = cast_byte(lsize);										//map表的尺寸，使用log值
  t->lastfree = gnode(t, size);  /* all positions are free */			//map表越界的位置
}


static void resize (lua_State *L, Table *t, int nasize, int nhsize) {			//给table的数组做resize，指定了: array的size 和 hash 的size
  int i;
  int oldasize = t->sizearray;													//备份旧尺寸
  int oldhsize = t->lsizenode;
  Node *nold = t->node;  /* save old hash ... */								//这里保存了旧hash，因为hash的内存总是重新分配
  if (nasize > oldasize)  /* array part must grow? */							//数组部分，有增大就 growup，增大部分会自动设置nil
    setarrayvector(L, t, nasize);
  
  /* create new hash part with appropriate size */
  setnodevector(L, t, nhsize);  												//这里分配就了新hash

  if (nasize < oldasize) {  /* array part must shrink? */						//如果map size变小了
    t->sizearray = nasize;
    /* re-insert elements from vanishing slice */
    for (i=nasize; i<oldasize; i++) {											//对于多出来的部分，且不是nil的，要重新安置
      if (!ttisnil(&t->array[i]))
        setobjt2t(L, luaH_setnum(L, t, i+1), &t->array[i]);						//luaH_setnum 是返回重新安置的位置
    }
    /* shrink array */
    luaM_reallocvector(L, t->array, oldasize, nasize, TValue);					//缩小table数组
  }
  
  /* re-insert elements from hash part */
  for (i = twoto(oldhsize) - 1; i >= 0; i--) {									//遍历所有旧的table map 项目，定位到旧node，转移到新map数组
    Node *old = nold+i;
    if (!ttisnil(gval(old)))
      setobjt2t(L, luaH_set(L, t, key2tval(old)), gval(old));					//给原来的map节点都重新安装
  }
  if (nold != dummynode)
    luaM_freearray(L, nold, twoto(oldhsize), Node);  /* free old array */		//释放旧的map表
}


void luaH_resizearray (lua_State *L, Table *t, int nasize) {						//给Table的数组 resize
  int nsize = (t->node == dummynode) ? 0 : sizenode(t);
  resize(L, t, nasize, nsize);														//重新设置size，hash部分取自table，array部分长度用参数
}


static void rehash (lua_State *L, Table *t, const TValue *ek) {						//Table 因为ek 扩充的时候调用
  int nasize, na;
  int nums[MAXBITS+1];  /* nums[i] = number of keys between 2^(i-1) and 2^i */
  int i;
  int totaluse;																		//总尺寸
  for (i=0; i<=MAXBITS; i++) nums[i] = 0;  /* reset counts */
  nasize = numusearray(t, nums);  /* count keys in array part */					//统计array部分的非nil值，并且统计其各个2n次区间的值
  totaluse = nasize;  /* all those keys are integer keys */							//合计入中成员数
  totaluse += numusehash(t, nums, &nasize);  /* count keys in hash part */			//计算map部分尺寸，并计入总数，并且对map中的数字下标者更新 nums 和 nasize
  /* count extra key */
  nasize += countint(ek, nums);														//以上是现存信息，再加上这次这个新条件的key
  totaluse++;
  /* compute new size for array part */
  na = computesizes(nums, &nasize);													//现在开始根据 nums 和 nasize 进行计算，期间可能会改变nasize
  /* resize the table to new computed sizes */
  resize(L, t, nasize, totaluse - na);
}



/*
** }=============================================================
*/

Table *luaH_new (lua_State *L, int narray, int nhash) {			//创建Table
  Table *t = luaM_new(L, Table);								//分内存
  luaC_link(L, obj2gco(t), LUA_TTABLE);							//向lua_State登记这个Table
  t->metatable = NULL;
  t->flags = cast_byte(~0);
  /* temporary values (kept only if some malloc fails) */		//先给配置个临时值
  t->array = NULL;
  t->sizearray = 0;
  t->lsizenode = 0;
  t->node = cast(Node *, dummynode);
  setarrayvector(L, t, narray);									//扩充数组表
  setnodevector(L, t, nhash);									//重新分配map表
  return t;
}

void luaH_free (lua_State *L, Table *t) {						//释放Table
  if (t->node != dummynode)										//释放map表，0长度的map表为dummy，不用释放
    luaM_freearray(L, t->node, sizenode(t), Node);
  luaM_freearray(L, t->array, t->sizearray, TValue);			//释放数组表
  luaM_free(L, t);												//释放掉表
}


static Node *getfreepos (Table *t) {							//给插入新key找一个空位置
  while (t->lastfree-- > t->node) {								//从后往前找
    if (ttisnil(gkey(t->lastfree)))								//nil key 就是可以用的
      return t->lastfree;
  }
  return NULL;  /* could not find a free place */
}



/*
** inserts a new key into a hash table; first, check whether key's main 
** position is free. If not, check whether colliding node is in its main 
** position or not: if it is not, move colliding node to an empty place and 
** put new key in its main position; otherwise (colliding node is in its main 
** position), new key goes to an empty position. 
*/
static TValue *newkey (lua_State *L, Table *t, const TValue *key) {					//在要设置但是没有对应key的情况下，根据key新建一个table key
  Node *mp = mainposition(t, key);													//首先根据在这个key的hash值，定位到Node*，mp就是mainposition的意思
  if (!ttisnil(gval(mp)) || mp == dummynode) {										//首个Node，如果是不是nil值，那么就是被使用着（否则就直接赋值了）
    Node *othern;
    Node *n = getfreepos(t);  /* get a free place */								//找个现在为nil的key来使用，从末尾的lastfree来使用
    if (n == NULL) {  /* cannot find a free place? */
      rehash(L, t, key);  /* grow table */											//找不到就扩充map表
      return luaH_set(L, t, key);  /* re-insert key into grown table */				//然后再重新插入一次
    }
    lua_assert(n != dummynode);
    othern = mainposition(t, key2tval(mp));											//检查mp上节点，是hash对号的（因为有可能是getfreepos来的，本该我的节点被占用了）
    if (othern != mp) {  /* is colliding node out of its main position? */			//当前mp节点在这里是不合适的
      /* yes; move colliding node into free position */
      while (gnext(othern) != mp) othern = gnext(othern);  /* find previous */		//从你源头的mp开始找，找到谁指向了你。
      gnext(othern) = n;  /* redo the chain with `n' in place of `mp' */			//把找到的n，代替之前的mp
      *n = *mp;  /* copy colliding node into free pos. (mp->next also goes) */
      gnext(mp) = NULL;  /* now `mp' is free */										//那么mp就闲出来了
      setnilvalue(gval(mp));
    }
    else {  /* colliding node is in its own main position */
      /* new node will go into free position */
      gnext(n) = gnext(mp);  /* chain new position */								//找到的节点放在mp的后面
      gnext(mp) = n;
      mp = n;																		//赋值给mp，后边用
    }
  }
  gkey(mp)->value = key->value; gkey(mp)->tt = key->tt;								//mp 来做key了，key的部分按照参数key 赋值类型和值
  luaC_barriert(L, t, key);
  lua_assert(ttisnil(gval(mp)));													//新建的mp，其value一定是nil的
  return gval(mp);																	//value返回去，去给做修改
}


/*
** search function for integers
*/
const TValue *luaH_getnum (Table *t, int key) {							//通过数字索引找成员
  /* (1 <= key && key <= t->sizearray) */
  if (cast(unsigned int, key-1) < cast(unsigned int, t->sizearray))		//内部下标还是从0开始的，先尝试在数组中查找
    return &t->array[key-1];											//将TValue返回
  else {																//否则在map里找
    lua_Number nk = cast_num(key);
    Node *n = hashnum(t, nk);											//到map找到首个节点
    do {  /* check whether `key' is somewhere in the chain */
      if (ttisnumber(gkey(n)) && luai_numeq(nvalue(gkey(n)), nk))		//遍历链表比较检查Node，比较类型和数值
        return gval(n);  /* that's it */								//找到了就返回
      else n = gnext(n);
    } while (n);														//没有next就结束吧，会返回luaO_nilobject
    return luaO_nilobject;
  }
}

/*
** search function for strings
*/
const TValue *luaH_getstr (Table *t, TString *key) {					//通过String 读取table的值
  Node *n = hashstr(t, key);											//在hash中找到节点，因为没有必要在数组中找
  do {  /* check whether `key' is somewhere in the chain */
    if (ttisstring(gkey(n)) && rawtsvalue(gkey(n)) == key)				//是string类型，也要相等，相等比较的是gc对象指针
      return gval(n);  /* that's it */
    else n = gnext(n);
  } while (n);															//没有next就结束吧，会返回luaO_nilobject
  return luaO_nilobject;
}


/*
** main search function
*/
const TValue *luaH_get (Table *t, const TValue *key) {					//通过key在Table中查找
  switch (ttype(key)) {
    case LUA_TNIL: return luaO_nilobject;								//nil就不给他返回
    case LUA_TSTRING: return luaH_getstr(t, rawtsvalue(key));			//string类型，先取其TString
    case LUA_TNUMBER: {													//Number可能是int 也可能是double，double不能做数组
      int k;
      lua_Number n = nvalue(key);
      lua_number2int(k, n);												//Number转int
      if (luai_numeq(cast_num(k), nvalue(key))) /* index is int? */		//测试其是否为int，不是int的就当做普通对象来处理了
        return luaH_getnum(t, k);  /* use specialized version */
      /* else go through */
    }
    default: {
      Node *n = mainposition(t, key);									//通用的定位Node
      do {  /* check whether `key' is somewhere in the chain */
        if (luaO_rawequalObj(key2tval(n), key))							//遍历node链，比较当前key与输入key
          return gval(n);  /* that's it */
        else n = gnext(n);
      } while (n);														//没有next就结束吧，会返回luaO_nilobject
      return luaO_nilobject;
    }
  }
}

TValue *luaH_set (lua_State *L, Table *t, const TValue *key) {			//通用的table写
  const TValue *p = luaH_get(t, key);									//先找一下
  t->flags = 0;															//有设置动作就清一清
  if (p != luaO_nilobject)												//如果找到了，就直接返回去，要改是外边的事
    return cast(TValue *, p);
  else {
    if (ttisnil(key)) luaG_runerror(L, "table index is nil");			//nil 不能做key
    else if (ttisnumber(key) && luai_numisnan(nvalue(key)))				//Double里的无效值也不能做key
      luaG_runerror(L, "table index is NaN");
    return newkey(L, t, key);											//去新建一个key，因为前边找不到
  }
}

TValue *luaH_setnum (lua_State *L, Table *t, int key) {					//Table中通过key设置值
  const TValue *p = luaH_getnum(t, key);								//先找看有没有，如果有就返回
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
    TValue k;
    setnvalue(&k, cast_num(key));										//先准备好key
    return newkey(L, t, &k);											//然后使用key新建一个TValue返回
  }
}

TValue *luaH_setstr (lua_State *L, Table *t, TString *key) {			//通过String往Table里设置值
  const TValue *p = luaH_getstr(t, key);								//先找找看有没有，如果有就直接返回
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
    TValue k;
    setsvalue(L, &k, key);												//先准备好key
    return newkey(L, t, &k);											//然后使用key新建一个TValue返回
  }
}


static int unbound_search (Table *t, unsigned int j) {
  unsigned int i = j;  /* i is zero or a present index */
  j++;
  /* find `i' and `j' such that i is present and j is not */
  while (!ttisnil(luaH_getnum(t, j))) {
    i = j;
    j *= 2;
    if (j > cast(unsigned int, MAX_INT)) {  /* overflow? */
      /* table was built with bad purposes: resort to linear search */
      i = 1;
      while (!ttisnil(luaH_getnum(t, i))) i++;
      return i - 1;
    }
  }
  /* now do a binary search between them */
  while (j - i > 1) {
    unsigned int m = (i+j)/2;
    if (ttisnil(luaH_getnum(t, m))) j = m;
    else i = m;
  }
  return i;
}


/*
** Try to find a boundary in table `t'. A `boundary' is an integer index
** such that t[i] is non-nil and t[i+1] is nil (and 0 if t[1] is nil).
*/
int luaH_getn (Table *t) {													//
  unsigned int j = t->sizearray;
  if (j > 0 && ttisnil(&t->array[j - 1])) {
    /* there is a boundary in the array part: (binary) search for it */
    unsigned int i = 0;
    while (j - i > 1) {
      unsigned int m = (i+j)/2;
      if (ttisnil(&t->array[m - 1])) j = m;
      else i = m;
    }
    return i;
  }
  /* else must find a boundary in hash part */
  else if (t->node == dummynode)  /* hash part is empty? */
    return j;  /* that is easy... */
  else return unbound_search(t, j);
}



#if defined(LUA_DEBUG)

Node *luaH_mainposition (const Table *t, const TValue *key) {
  return mainposition(t, key);
}

int luaH_isdummy (Node *n) { return n == dummynode; }

#endif
