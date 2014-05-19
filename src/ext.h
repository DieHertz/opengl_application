#ifndef ext_h
#define ext_h

template<typename T, size_t N> inline size_t array_length(T (&)[N]) { return N; }

#endif /* ext_h */
