#ifndef UTIL_H_
#define UTIL_H_

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

unsigned long adler32(const unsigned char *buf, unsigned long len);

#endif //UTIL_H_
