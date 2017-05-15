#include <stddef.h>

#define BASE 65521U
#define NMAX 5552

#define DO1(buf, i)	{a += buf[i]; b += a;}
#define DO2(buf, i)	DO1(buf,i); DO1(buf,i+1);
#define DO4(buf, i)	DO2(buf,i); DO2(buf,i+2);
#define DO8(buf, i)	DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)	DO8(buf,0); DO8(buf,8);

unsigned long adler32(const unsigned char *buf, unsigned long len)
{
	unsigned long a = 1;
	unsigned long b = 0;
	int k;

	while(len > 0){
		k = len > NMAX ? NMAX : len;
		len -= k;
		while(k >= 16){
			// unroll 16 steps
			// NOTE: not sure if this increases performance as
			// the compiler usually optimize loops
			DO16(buf);
			buf += 16;
			k -= 16;
		}
		while(k-- != 0){
			a += *buf++;
			b += a;
		}
		a %= BASE;
		b %= BASE;
	}
	return a | (b << 16);
}
