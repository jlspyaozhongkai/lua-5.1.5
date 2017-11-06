/*
** $Id: lobject.h,v 2.20.1.2 2008/08/06 13:29:48 roberto Exp $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/


#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "lua.h"


/* tags for values visible from Lua */
#define LAST_TAG	LUA_TTHREAD

#define NUM_TAGS	(LAST_TAG+1)


/*
** Extra tags for non-values
*/
#define LUA_TPROTO	(LAST_TAG+1)
#define LUA_TUPVAL	(LAST_TAG+2)
#define LUA_TDEADKEY	(LAST_TAG+3)


/*
** Union of all collectable objects
*/
//其实所有的回收类型都包在这个共用体里，见lstate.h
typedef union GCObject GCObject;


/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
//所有需要GC的对象，都以这个宏开头，包括gc链的next，内存尺寸，还有gc标记，总之为为回收用的
#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked


/*
** Common header in struct form
*/
//各种需要回收的类型使用CommonHeader里的三个头做数据头，因为是三个数据，不太好摆弄，
//就封装在GCheader中，记住，CommonHeader 啥都不是，就是个格式。
typedef struct GCheader {
  CommonHeader;
} GCheader;




/*
** Union of all Lua values
*/
//这是一个正儿八经的变量值，参数值等。如果值是不需要gc的，那么他就直接存放在共用中
//如果是一个需要gc的值，那么它真正的不存放在本地，而是通过gc指向
typedef union {
  GCObject *gc;
  void *p;
  lua_Number n;
  int b;
} Value;


/*
** Tagged Values
*/
//光有Value是不行的，因为还不知道里边到底是怎么存数据的
#define TValuefields	Value value; int tt

//这才是全功能的值，包含了数据类型和数据的值，要是把他传给别人，别人也知道怎么用
typedef struct lua_TValue {
  TValuefields;
} TValue;


/* Macros to test type */
//检查是否是各种数据类型
#define ttisnil(o)	(ttype(o) == LUA_TNIL)
#define ttisnumber(o)	(ttype(o) == LUA_TNUMBER)
#define ttisstring(o)	(ttype(o) == LUA_TSTRING)
#define ttistable(o)	(ttype(o) == LUA_TTABLE)
#define ttisfunction(o)	(ttype(o) == LUA_TFUNCTION)
#define ttisboolean(o)	(ttype(o) == LUA_TBOOLEAN)
#define ttisuserdata(o)	(ttype(o) == LUA_TUSERDATA)
#define ttisthread(o)	(ttype(o) == LUA_TTHREAD)
#define ttislightuserdata(o)	(ttype(o) == LUA_TLIGHTUSERDATA)

/* Macros to access values */
//输入TValue返回其类型
#define ttype(o)	((o)->tt)
//输入TValue返回其值，首先输入要是gc回收类型
#define gcvalue(o)	check_exp(iscollectable(o), (o)->value.gc)
//取TValue中指针值
#define pvalue(o)	check_exp(ttislightuserdata(o), (o)->value.p)
//取TValue中的number
#define nvalue(o)	check_exp(ttisnumber(o), (o)->value.n)
//取TValue中的string对象的指针，value里边不是有个gc对象指针么，gc对象作为
#define rawtsvalue(o)	check_exp(ttisstring(o), &(o)->value.gc->ts)
//TString是由tsv结构和一个对齐数据dummy组成，这里返回tsv结构的指针，估计用的比rawtsvalue多
#define tsvalue(o)	(&rawtsvalue(o)->tsv)
//取TValue中的userdata对象的指针 u
#define rawuvalue(o)	check_exp(ttisuserdata(o), &(o)->value.gc->u)
//Udata是由uv结构和dummy组成，这里返回uv结构的指针，估计主要是用这个
#define uvalue(o)	(&rawuvalue(o)->uv)
//取TValue中的Closure
#define clvalue(o)	check_exp(ttisfunction(o), &(o)->value.gc->cl)
//取TValue中的Table
#define hvalue(o)	check_exp(ttistable(o), &(o)->value.gc->h)
//取TValue中的boolean
#define bvalue(o)	check_exp(ttisboolean(o), (o)->value.b)
//取TValue中的Thead，类型lua_State
#define thvalue(o)	check_exp(ttisthread(o), &(o)->value.gc->th)
//这个有意思，就是平时的not判断，nil 和 false 都为假
#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))

/*
** for internal debug only
*/
//看看TValue对劲不？
#define checkconsistency(obj) \
  lua_assert(!iscollectable(obj) || (ttype(obj) == (obj)->value.gc->gch.tt))

//在给需要gc的类型TValue配对象以后，会调用这个，首先 非gc类型直接放行
//TValue中指定的类型必须和指向对象的gc对象的类型必须一致
//指向的gc对象也不能是dead的
//就是说给TValue配对象的时候不能乱给
#define checkliveness(g,obj) \
  lua_assert(!iscollectable(obj) || \
  ((ttype(obj) == (obj)->value.gc->gch.tt) && !isdead(g, (obj)->value.gc)))


/* Macros to set values */
//给TValue设置为nil
#define setnilvalue(obj) ((obj)->tt=LUA_TNIL)

//给TValue设置为number和值
#define setnvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.n=(x); i_o->tt=LUA_TNUMBER; }

//给TValue设置为指针和值
#define setpvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.p=(x); i_o->tt=LUA_TLIGHTUSERDATA; }

//给TValue设置为boolean和值
#define setbvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.b=(x); i_o->tt=LUA_TBOOLEAN; }

//给TValue配string对象
#define setsvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TSTRING; \
    checkliveness(G(L),i_o); }

//给TValue配userdata对象
#define setuvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TUSERDATA; \
    checkliveness(G(L),i_o); }

//给TValue配线程对象 thread
#define setthvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTHREAD; \
    checkliveness(G(L),i_o); }

//给TValue配function对象，closure
#define setclvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TFUNCTION; \
    checkliveness(G(L),i_o); }

//给TValue配table对象
#define sethvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTABLE; \
    checkliveness(G(L),i_o); }

//给TValue配 ？？？
#define setptvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TPROTO; \
    checkliveness(G(L),i_o); }



//TValue给TValue赋值，类型和值都赋过去
#define setobj(L,obj1,obj2) \
  { const TValue *o2=(obj2); TValue *o1=(obj1); \
    o1->value = o2->value; o1->tt=o2->tt; \
    checkliveness(G(L),o1); }


/*
** different types of sets, according to destination
*/

/* from stack to (same) stack */
#define setobjs2s	setobj
/* to stack (not from same stack) */
#define setobj2s	setobj
#define setsvalue2s	setsvalue
#define sethvalue2s	sethvalue
#define setptvalue2s	setptvalue
/* from table to same table */
#define setobjt2t	setobj
/* to table */
#define setobj2t	setobj
/* to new object */
#define setobj2n	setobj
#define setsvalue2n	setsvalue

//就是个赋值语句，将obj->tt = tt
#define setttype(obj, tt) (ttype(obj) = (tt))


//返回TValue，是不是需要gc回收的类型，回收和不回收的数据保存是不一样的
#define iscollectable(o)	(ttype(o) >= LUA_TSTRING)


//StkId 就是stack idx 之value
typedef TValue *StkId;  /* index to stack elements */


/*
** String headers for string table
*/
//string类型的gc对象，直接看里边的tsv TString value
typedef union TString {
  L_Umaxalign dummy;  /* ensures maximum alignment for strings */
  struct {
    CommonHeader;			//公共头
    lu_byte reserved;		//是否是保留的string，比如各种关键字，类型名什么的
    unsigned int hash;		//string 内容的hash值
    size_t len;				//字符串的长度
    						//真正的字符内容就在TString结构的后边
  } tsv;
} TString;

//通过TString对象定位后边的字符串内容
#define getstr(ts)	cast(const char *, (ts) + 1)
//通过TValue获得TString对象的字符串值
#define svalue(o)       getstr(rawtsvalue(o))



typedef union Udata {
  L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    CommonHeader;
    struct Table *metatable;
    struct Table *env;
    size_t len;                 //UserData数据的长度
								//Udata的数据就存在于Udata结构的后边
  } uv;
} Udata;




/*
** Function Prototypes
*/
typedef struct Proto {
  CommonHeader;
  TValue *k;  /* constants used by the function */
  Instruction *code;
  struct Proto **p;  /* functions defined inside the function */
  int *lineinfo;  /* map from opcodes to source lines */
  struct LocVar *locvars;  /* information about local variables */
  TString **upvalues;  /* upvalue names */
  TString  *source;
  int sizeupvalues;
  int sizek;  /* size of `k' */
  int sizecode;
  int sizelineinfo;
  int sizep;  /* size of `p' */
  int sizelocvars;
  int linedefined;
  int lastlinedefined;
  GCObject *gclist;
  lu_byte nups;  /* number of upvalues */
  lu_byte numparams;
  lu_byte is_vararg;
  lu_byte maxstacksize;
} Proto;


/* masks for new-style vararg */
#define VARARG_HASARG		1
#define VARARG_ISVARARG		2
#define VARARG_NEEDSARG		4


typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;



/*
** Upvalues
*/

typedef struct UpVal {
  CommonHeader;
  TValue *v;  /* points to stack or to its own value */
  union {
    TValue value;  /* the value (when closed) */
    struct {  /* double linked list (when open) */
      struct UpVal *prev;
      struct UpVal *next;
    } l;
  } u;
} UpVal;


/*
** Closures
*/

#define ClosureHeader \
	CommonHeader; lu_byte isC; lu_byte nupvalues; GCObject *gclist; \
	struct Table *env

typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f;
  TValue upvalue[1];
} CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;
  UpVal *upvals[1];
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;


#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION && clvalue(o)->c.isC)
#define isLfunction(o)	(ttype(o) == LUA_TFUNCTION && !clvalue(o)->c.isC)


/*
** Tables
*/
//map 表项的key，不只是关联一个key数据，还要组织hash链表
typedef union TKey {
  struct {
    TValuefields;							//和TValue相同，只是拿出来用了
    struct Node *next;  /* for chaining */	//指向下一个Node
  } nk;
  TValue tvk;								//当把TKey当TValue来用，忽视后边的next，就是使用这个成员
} TKey;

//Table map 的key value 组合
typedef struct Node {
  TValue i_val;								//map表项的值
  TKey i_key;								//map表项的key
} Node;

//Table结构
typedef struct Table {
  CommonHeader;														//公共头
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */ 		//元方法bit位，首次使用时更新，有bit代表未设置
  lu_byte lsizenode;  /* log2 of size of `node' array */			//map表大小，2的lsizenode次幂个，其实很大
  struct Table *metatable;											//表的元表
  TValue *array;  /* array part */									//表数组部分的TValue们
  Node *node;														//map表，node数组
  Node *lastfree;  /* any free position is before this position */	//lastfree所指向是过界的，不安全的
  GCObject *gclist;													//
  int sizearray;  /* size of `array' array */						//表数组部分的指针数组长度，内部下标还是从0开始的
} Table;



/*
** `module' operation for hashing (size is always a power of 2)
*/
//hash查找的时候取模值，先断言检查 (size&(size-1))==0，size必须是2的n次，只保留被size掩住的部分
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))

//2的x幂
#define twoto(x)	(1<<(x))
//求Table的hash表尺寸
#define sizenode(t)	(twoto((t)->lsizenode))

//这样nil和nil就相等了，同时也能统一处理
#define luaO_nilobject		(&luaO_nilobject_)

//声明之
LUAI_DATA const TValue luaO_nilobject_;

//给Table的map 按照项数量，计算以2为底的log值（往大分，先减后加）
#define ceillog2(x)	(luaO_log2((x)-1) + 1)

LUAI_FUNC int luaO_log2 (unsigned int x);
LUAI_FUNC int luaO_int2fb (unsigned int x);
LUAI_FUNC int luaO_fb2int (int x);
LUAI_FUNC int luaO_rawequalObj (const TValue *t1, const TValue *t2);
LUAI_FUNC int luaO_str2d (const char *s, lua_Number *result);
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);


#endif

