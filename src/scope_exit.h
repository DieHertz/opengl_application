#ifndef scope_exit_h
#define scope_exit_h

template<class Function> class __scope_exit {
    Function f;

    __scope_exit(const __scope_exit&) = delete;
    __scope_exit& operator=(const __scope_exit&) = delete;

public:
    __scope_exit(Function f) : f{f} {}
    ~__scope_exit() { f(); }
};

template<class Function>
__scope_exit<Function> make_scope_exit(Function&& f) {
    return forward<Function>(f);
}

#define scope_exit_concat(fn, n) \
auto at_scope_exit##n = make_scope_exit([&] { fn })
#define scope_exit_fwd(fn, n) __scope_exit(fn, n)
#define scope_exit(fn) scope_exit_fwd(fn, __LINE__)

#endif /* scope_exit_h */
