/*
** $Id: lstate.h,v 2.24.1.2 2008/01/03 15:20:39 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"



struct lua_longjmp;  /* defined in ldo.c */


/* table of globals */
#define gt(L)	(&L->l_gt)					//状态机的全局变量表,_G,使用LUA_GLOBALSINDEX定位

/* registry */
#define registry(L)	(&G(L)->l_registry)		//状态机的注册表, 是所有状态机的注册表，使用LUA_REGISTRYINDEX定位


/* extra stack space to handle TM calls and some other extras */
#define EXTRA_STACK   5


#define BASIC_CI_SIZE           8

#define BASIC_STACK_SIZE        (2*LUA_MINSTACK)


//全局的字符串标
typedef struct stringtable {
  GCObject **hash;							//GCObject *数据的数组
  lu_int32 nuse;  /* number of elements */	//表中字符串的数量，计数，用于调整hashkey尺寸
  int size;									//hash表的尺寸
} stringtable;


/*
** informations about a call
*/
//函数调用信息
typedef struct CallInfo {
  StkId base;  /* base for this function */								//调用栈的栈底
  StkId func;  /* function index in the stack */						//调用的函数
  StkId	top;  /* top for this function */								//调用栈的栈顶
  const Instruction *savedpc;											//调用时保存的指令地址，应该是下一条指令
  int nresults;  /* expected number of results from this function */	//
  int tailcalls;  /* number of tail calls lost under this entry */		//
} CallInfo;



#define curr_func(L)	(clvalue(L->ci->func))					//取当前调用函数的 Closure函数对象
#define ci_func(ci)	(clvalue((ci)->func))						//从callinfo中取 Closure函数对象
#define f_isLua(ci)	(!ci_func(ci)->c.isC)						//从callinfo中取 Closure函数对象 是否为C函数
#define isLua(ci)	(ttisfunction((ci)->func) && f_isLua(ci))	//从callinfo中取 判断函数对象，再判断是否是lua


/*
** `global state', shared by all threads of this state
*/
//所有的lua_State 都共用的结构
typedef struct global_State {
  //全局的字符串表
  stringtable strt;  /* hash table for strings */
  lua_Alloc frealloc;  /* function to reallocate memory */					//全局的内存分配函数，也管释放
  void *ud;         /* auxiliary data to `frealloc' */
  lu_byte currentwhite;
  lu_byte gcstate;  /* state of garbage collector */
  int sweepstrgc;  /* position of sweep in `strt' */
  GCObject *rootgc;  /* list of all collectable objects */					//Gc对象登记的对象
  GCObject **sweepgc;  /* position of sweep in `rootgc' */
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of weak tables (to be cleared) */
  GCObject *tmudata;  /* last element of list of userdata to be GC */
  Mbuffer buff;  /* temporary buffer for string concatentation */
  lu_mem GCthreshold;
  lu_mem totalbytes;  /* number of bytes currently allocated */				//内存分配计数
  lu_mem estimate;  /* an estimate of number of bytes actually in use */
  lu_mem gcdept;  /* how much GC is `behind schedule' */
  int gcpause;  /* size of pause between successive GCs */
  int gcstepmul;  /* GC `granularity' */
  lua_CFunction panic;  /* to be called in unprotected errors */
  TValue l_registry;														//全局的注册表使用伪索引LUA_REGISTRYINDEX
  struct lua_State *mainthread;
  UpVal uvhead;  /* head of double-linked list of all open upvalues */
  struct Table *mt[NUM_TAGS];  /* metatables for basic types */
  TString *tmname[TM_N];  /* array with tag-method names */
} global_State;


/*
** `per thread' state
*/
//这是一个虚拟机一类的结构
struct lua_State {
  CommonHeader;
  lu_byte status;
  StkId top;  /* first free slot in the stack */							//调用栈的栈顶，是个free的
  StkId base;  /* base of current function */								//当前函数，调用栈的栈底，StkId 就是TValue的指针。
  global_State *l_G;														//每个调用栈都共用一个全局资源对象
  CallInfo *ci;  /* call info for current function */						//当前的调用函数info
  const Instruction *savedpc;  /* `savedpc' of current function */
  StkId stack_last;  /* last free slot in the stack */						//调用栈的最大处
  StkId stack;  /* stack base */											//调用栈的开始处
  CallInfo *end_ci;  /* points after end of ci array*/
  CallInfo *base_ci;  /* array of CallInfo's */
  int stacksize;
  int size_ci;  /* size of array `base_ci' */
  unsigned short nCcalls;  /* number of nested C calls */
  unsigned short baseCcalls;  /* nested C calls when resuming coroutine */
  lu_byte hookmask;
  lu_byte allowhook;
  int basehookcount;
  int hookcount;
  lua_Hook hook;
  TValue l_gt;  /* table of globals */										//状态机的全局表，全局_G
  TValue env;  /* temporary place for environments */						//临时环境
  GCObject *openupval;  /* list of open upvalues in this stack */			
  GCObject *gclist;
  struct lua_longjmp *errorJmp;  /* current error recover point */
  ptrdiff_t errfunc;  /* current error handling function (stack index) */
};

//lua_State 里都有一个global_State *l_G; 这个恐怕是所有lua_State共用的
#define G(L)	(L->l_G)


/*
** Union of all collectable objects
*/
//所有的要gc类型，都包在这个共用里，各个类型都包含CommonHeader中
//GCheader特殊，仅仅是为了方便使用CommonHeader，就是读取TString等其他类型的CommonHeader时，通过它。
//这里的union估计是做个样子，没人会直接分配GCObject，而是各分配各的，然互当GCObject用
union GCObject {
  GCheader gch;								//其他各个值的公共头CommonHeader的代表
  union TString ts;							//需要gc的string数据部，ts表示TString
  union Udata u;							//需要gc的userdata，u表示Udata
  union Closure cl;							//需要gc的Closure，cl表示Closure，和函数差不多
  struct Table h;							//需要gc的Table，h表示Table是几个意思
  struct Proto p;							//需要gc的Prototypes
  struct UpVal uv;							//
  struct lua_State th;  /* thread */		//线程
};


/* macros to convert a GCObject into a specific value */
//通过GCObject有检查的返回string对象
#define rawgco2ts(o)	check_exp((o)->gch.tt == LUA_TSTRING, &((o)->ts))
#define gco2ts(o)	(&rawgco2ts(o)->tsv)
#define rawgco2u(o)	check_exp((o)->gch.tt == LUA_TUSERDATA, &((o)->u))
#define gco2u(o)	(&rawgco2u(o)->uv)
#define gco2cl(o)	check_exp((o)->gch.tt == LUA_TFUNCTION, &((o)->cl))
#define gco2h(o)	check_exp((o)->gch.tt == LUA_TTABLE, &((o)->h))
#define gco2p(o)	check_exp((o)->gch.tt == LUA_TPROTO, &((o)->p))
#define gco2uv(o)	check_exp((o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define ngcotouv(o) \
	check_exp((o) == NULL || (o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define gco2th(o)	check_exp((o)->gch.tt == LUA_TTHREAD, &((o)->th))

/* macro to convert any Lua object into a GCObject */
//输入Gc类型数据，返回GCObject，直接强转就行
#define obj2gco(v)	(cast(GCObject *, (v)))


LUAI_FUNC lua_State *luaE_newthread (lua_State *L);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);

#endif

