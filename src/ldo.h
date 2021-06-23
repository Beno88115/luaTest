#ifndef _IDO_H_
#define _IDO_H_

#include "lstate.h"

typedef int (*Pfunc)(struct lua_State* L, void* ud);

void seterrobj(struct lua_State* L, int error);

// 检测堆栈是否超出，超出则进行扩容
void luaD_checkstack(struct lua_State* L, int need);
// 扩容堆栈
void luaD_growstack(struct lua_State* L, int size);

void luaD_throw(struct lua_State* L, int error);

int luaD_rawrunprotected(struct lua_State* L, Pfunc f, void* ud);
int luaD_precall(struct lua_State* L, StkId func, int nresult);
int luaD_poscall(struct lua_State* L, StkId first_result, int nresult);
int luaD_call(struct lua_State* L, StkId func, int nresult);
int luaD_pcall(struct lua_State* L, Pfunc f, void* ud, ptrdiff_t oldtop, ptrdiff_t ef);

#endif // _IDO_H_