#include "lgc.h"
#include "lmem.h"
#include "lstring.h"

#define GCMAXSWEEPGCO 25

#define gettotalbytes(g) (g->totalbytes + g->GCdebt)
#define white2gray(o) resetbits((o)->marked, WHITEBITS)
#define gray2black(o) l_setbit((o)->marked, BLACKBIT)
#define black2gray(o) resetbit((o)->marked, BLACKBIT)

#define sweepwholelist(L, list) sweeplist(L, list, MAX_LUMEM)

struct GCObject* luaC_newobj(struct lua_State* L, int tt_, size_t size)
{
    struct global_State* g = G(L);
    struct GCObject* obj = (struct GCObject*)luaM_realloc(L, NULL, 0, size);
    obj->marked = luaC_white(g);
    obj->next = g->allgc;
    obj->tt_ = tt_;
    g->allgc = obj;

    return obj;
}

void reallymarkobject(struct lua_State* L, struct GCObject* gco)
{
    struct global_State* g = G(L);
    white2gray(gco);

    switch (gco->tt_)
    {
    case LUA_TTHREAD: 
        linkgclist(gco2th(gco), g->gray);
        break;
    case LUA_SHRSTR:{
            gray2black(gco);
            struct TString* ts = gco2ts(gco);
            g->GCmemtrav += sizelstring(ts->shrlen);
        } break;
    case LUA_LNGSTR: {
            gray2black(gco);
            struct TString* ts = gco2ts(gco);
            g->GCmemtrav += sizelstring(ts->u.lnglen);
        } break;
    default:
        break;
    }
}

static void restart_collection(struct lua_State* L)
{
    struct global_State* g = G(L);
    g->gray = g->grayagain = NULL;
    markobject(L, g->mainthread);
}

static lu_mem traversethread(struct lua_State* L, struct lua_State* th)
{
    TValue* o = th->stack;
    for (; o < th->top; o ++) {
        markvalue(L, o);
    }

    return sizeof(struct lua_State) + sizeof(TValue) * th->stack_size + sizeof(struct CallInfo) * th->nci;
}

static void propagatemark(struct lua_State* L)
{
    struct global_State* g = G(L);
    if (!g->gray) {
        return;
    }
    struct GCObject* gco = g->gray;
    gray2black(gco);
    lu_mem size = 0;

    switch(gco->tt_) {
        case LUA_TTHREAD:{
            black2gray(gco);
            struct lua_State* th = gco2th(gco);
            g->gray = th->gclist;
            linkgclist(th, g->grayagain);
            size = traversethread(L, th);
        } break;
        default:break;
    }

    g->GCmemtrav += size;
}

static void propagateall(struct lua_State* L)
{
    struct global_State* g = G(L);
    while(g->gray) {
        propagateall(L);
    }
}

static void atomic(struct lua_State* L)
{
    struct global_State* g = G(L);
    g->gray = g->grayagain;
    g->grayagain = NULL;

    g->gcstate = GCSinsideatomic;
    propagateall(L);
    g->currentwhite = cast(lu_byte, otherwhite(g));
    
    luaS_clearcache(L);
}

static lu_mem freeobj(struct lua_State* L, struct GCObject* gco)
{
    switch(gco->tt_) {
        case LUA_SHRSTR: {
            struct TString* ts = gco2ts(gco);
            luaS_remove(L, ts);
            lu_mem sz = sizelstring(ts->shrlen);
            luaM_free(L, ts, sz);
            return sz;
        } break;
        case LUA_LNGSTR: {
            struct TString* ts = gco2ts(gco);
            lu_mem sz = sizelstring(ts->u.lnglen);
            luaM_free(L, ts, sz);
        } break;
        default:{
            lua_assert(0);
        } break;
    }
    return 0;
}

static struct GCObject** sweeplist(struct lua_State* L, struct GCObject** p, size_t count)
{
    struct global_State* g = G(L);
    lu_byte ow = otherwhite(g);
    while (*p != NULL && count > 0)
    {
        lu_byte marked = (*p)->marked;
        if (isdeadm(ow, marked))
        {
            struct GCObject* gco = *p;
            *p = (*p)->next;
            g->GCmemtrav += freeobj(L, gco);
        }
        else
        {
            (*p)->marked &= cast(lu_byte, ~(bitmask(BLACKBIT) | WHITEBITS));
            (*p)->marked |= luaC_white(g);
            p = &((*p)->next);
        }
        count--;
    }
    return (*p) == NULL ? NULL : p;
}

static void entersweep(struct lua_State* L) 
{
    struct global_State* g = G(L);
    g->gcstate = GCSsweepallgc;
    g->sweepgc = sweeplist(L, &g->allgc, 1);
}

static void sweepstep(struct lua_State* L) 
{
    struct global_State* g = G(L);
    if (g->sweepgc) {
        g->sweepgc = sweeplist(L, g->sweepgc, GCMAXSWEEPGCO);
        g->GCestimate = gettotalbytes(g);

        if (g->sweepgc) {
            return;
        }
    }
    g->gcstate = GCSsweepend;
    g->sweepgc = NULL;
}

static void setdebt(struct lua_State* L, l_mem debt)
{
    struct global_State* g = G(L);
    lu_mem totalbytes = gettotalbytes(g);

    g->totalbytes = totalbytes - debt;
    g->GCdebt = debt;
}

static void setpause(struct lua_State* L)
{
    struct global_State* g = G(L);
    l_mem estimate = g->GCestimate / GCPAUSE;
    estimate = (estimate * g->GCstepmul) >= MAX_LMEM ? MAX_LMEM : estimate * g->GCstepmul;

    l_mem debt = g->GCestimate - estimate;
    setdebt(L, debt);
}

static l_mem get_debt(struct lua_State* L) 
{
    struct global_State* g = G(L);
    l_mem debt = g->GCdebt;
    if (debt <= 0) {
        return 0;
    }

    debt = debt / STEPMULADJ + 1;
    debt = debt >= (MAX_LMEM / STEPMULADJ) ? MAX_LMEM : debt * g->GCstepmul;

    return debt; 
}

static lu_mem singlestep(struct lua_State* L)
{
    struct global_State* g = G(L);
    switch(g->gcstate) {
        case GCSpause: {
            g->GCmemtrav = 0;
            restart_collection(L);
            g->gcstate = GCSpropagate;
            return g->GCmemtrav;
        } break;
        case GCSpropagate:{
            g->GCmemtrav = 0;
            propagatemark(L);
            if (g->gray == NULL) {
                g->gcstate = GCSatomic;
            }
            return g->GCmemtrav;
        } break;
        case GCSatomic:{
            g->GCmemtrav = 0;
            if (g->gray) {
                propagateall(L);
            }
            atomic(L);
            entersweep(L);
            g->GCestimate = gettotalbytes(g);
            return g->GCmemtrav;
        } break;
        case GCSsweepallgc: {
            g->GCmemtrav = 0;
            sweepstep(L);
            return g->GCmemtrav;
        } break;
        case GCSsweepend: {
            g->GCmemtrav = 0;
            g->gcstate = GCSpause;
            return 0;
        } break;
        default:break;
    }

    return g->GCmemtrav;
}

void luaC_step(struct lua_State* L)
{
    struct global_State* g = G(L);
    l_mem debt = get_debt(L);
    do {
        l_mem work = singlestep(L);
        debt -= work;

        // 一次处理一个GCSTEPSIZE单位的数据
    } while(debt > -GCSTEPSIZE && g->gcstate != GCSpause);

    if (g->gcstate == GCSpause)
    {
        // GC清理完比
        setpause(L);
    }
    else
    {
        // GC只清理一部分
        debt = g->GCdebt / STEPMULADJ * g->GCstepmul;
        setdebt(L, debt);
    }
}

void luaC_freeallobjects(struct lua_State* L)
{
    struct global_State* g = G(L);
    g->currentwhite = WHITEBITS;
    sweepwholelist(L, &g->allgc);
}

void luaC_fix(struct lua_State* L, struct GCObject* o)
{
    struct global_State* g = G(L);
    lua_assert(g->allgc == o);

    g->allgc = g->allgc->next;
    o->next = g->fixgc;
    g->fixgc = o;
    white2gray(o);
}
