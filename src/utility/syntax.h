#pragma clang diagnostic push
#pragma once

#define tt template<typename t>
#define tt1 template<typename t0>
#define tt2 template<typename t0, typename t1>
#define tt3 template<typename t0, typename t1, typename t2>
#define tt4 template<typename t0, typename t1, typename t2, typename t3>
// 'defer'

template<typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda):lambda(lambda) {}
    ~ExitScope() { lambda(); }
private:
    ExitScope& operator =(const ExitScope&);
};

class ExitScopeHelp {
public:
    template<typename T>
    ExitScope<T> operator+(T t) { return t; }
};

#define stx_concat_internal(x,y) x##y
#define stx_concat(x,y) stx_concat_internal(x,y)
#define defer [[maybe_unused]]let& stx_concat(stx_concat(defer_, __LINE__), __COUNTER__) = ExitScopeHelp() + [&]

// release and reset a var_ at the end of the current scope
#define defer_release(name) defer { release(name); }
#define defer_release2(name0, name1) defer { release(name1); release(name0); }

#define var auto
#define let const auto
#define def constexpr
#define ref auto&
#define ptr_line stx_concat(ptr,__LINE__)
#define if_ref(name, init) let ptr_line = init; ref name = *(ptr_line); if (ptr_line)

#define tmp(name, initializer) var name = (initializer); defer_release(name)

#define dbg_fail_return printf("Failed!\n"); assert(false); return
#define dbg_fail_continue assert(false); continue
#define dbg_fail_break assert(false); break

#define CODE(...) #__VA_ARGS__

#pragma clang diagnostic pop