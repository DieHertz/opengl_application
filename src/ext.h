#ifndef ext_h
#define ext_h

template<typename T, size_t N> constexpr size_t array_length(T (&)[N]) { return N; }

#endif /* ext_h */
