//
// shoes/internal.h
// Debugging and allocation functions within Shoes.
//

#ifndef SHOES_INTERNAL_H
#define SHOES_INTERNAL_H

#define SHOE_REALLOC_N(V, T, N)     (V)=(T *)realloc((char*)(V), sizeof(T)*(N))
#define SHOE_ALLOC_N(T, N)          (T *)malloc(sizeof(T) * N)
#define SHOE_ALLOC(T)               (T *)malloc(sizeof(T))
#define SHOE_FREE(T)                free((void*)T)

#define SHOE_MEMZERO(p,type,n)      memset((p), 0, sizeof(type)*(n))
#define SHOE_MEMCPY(p1,p2,type,n)   memcpy((p1), (p2), sizeof(type)*(n))
#define SHOE_MEMMOVE(p1,p2,type,n)  memmove((p1), (p2), sizeof(type)*(n))
#define SHOE_MEMCMP(p1,p2,type,n)   memcmp((p1), (p2), sizeof(type)*(n))

#ifndef min
#define min(a, b) ((a) <= (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) >= (b) ? (a) : (b))
#endif

#ifdef _MSC_VER

void odprintf(const char *format, ...);
static inline void odignore(const char *format, ...) {}
int shoes_snprintf(char* str, size_t size, const char* format, ...);
#define PUTS odprintf 

#ifdef DEBUG
#define INFO PUTS
#else
#define INFO odignore
#endif
#define WARN PUTS
#define QUIT(msg) \
  if (code == SHOES_OK) code = SHOES_FAIL; \
  PUTS(msg); \
  goto quit

#else

#define shoes_snprintf snprintf

#define PUTS printf
#ifdef DEBUG

#define INFO(f, s...) PUTS(f, ## s)
#else
#define INFO(f, s...)
#endif
#define WARN(f, s...) PUTS(f, ## s)
#define QUIT(f, s...) \
  if (code == SHOES_OK) code = SHOES_FAIL; \
  PUTS(f, ## s); \
  goto quit
#endif

#endif
