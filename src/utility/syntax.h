#pragma clang diagnostic push
#pragma once

#define tt template<typename t>
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

// release and reset a variable at the end of the current scope
#define defer_release(name) defer { release(name); }

#define var auto
#define let const auto
#define def constexpr

#define tmp(name, initializer) var name = (initializer); defer_release(name)

#define CODE(...) #__VA_ARGS__

#pragma clang diagnostic pop