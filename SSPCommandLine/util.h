/*

Copyright 2017 UL TS B.V. The Netherlands

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#ifndef min
    #define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
    #define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef alloca
	#define alloca _alloca
#endif

// Exits when the passed pointer is NULL
void * checkMalloc(void * ptr);

void Crc_init(uint16_t * crc);
void Crc_add(uint16_t * crc, uint8_t byte);

#endif /* not defined UTIL_H */