#ifndef SYSTEM7_STATIC_ASSERT_H
#define SYSTEM7_STATIC_ASSERT_H

/* Named layout checks for C++, C11+, and older C translation units. */
#if defined(__cplusplus)
#define SYSTEM7_STATIC_ASSERT(condition, name) static_assert((condition), #name)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define SYSTEM7_STATIC_ASSERT(condition, name) _Static_assert((condition), #name)
#else
#define SYSTEM7_STATIC_ASSERT(condition, name) \
    typedef char system7_static_assert_##name[(condition) ? 1 : -1]
#endif

#endif /* SYSTEM7_STATIC_ASSERT_H */
