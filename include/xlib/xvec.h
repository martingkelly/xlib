/*
Copyright 2018 Xevo Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

<http://www.apache.org/licenses/LICENSE-2.0>

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

---
xlib is originally derived from klib, which used the MIT license. The full text
of the klib license is:
---
Copyright (c) 2018 by Attractive Chaos <attractor@live.co.uk>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
---
*/

/*
  An example:

#include "xvec.h"
int main() {
	xvec_t(int) array;
	xv_init(array);
	xv_push(int, array, 10); // append
	xv_a(int, array, 20) = 5; // dynamic
	xv_A(array, 20) = 4; // static
	xv_destroy(array);
	return 0;
}
*/

/*
  2008-09-22 (0.1.0):

	* The initial version.

*/

#ifndef XLIB_XVEC_H_
#define XLIB_XVEC_H_

#include <stdlib.h>
#include <xlib/alloc.h>

#define xv_roundup32(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, ++(x))

#define xvec_t(type) struct { size_t n, m; type *a; }
#define xv_init(v) ((v).n = (v).m = 0, (v).a = 0)
#define xv_destroy(v) free((v).a)
#define xv_A(v, i) ((v).a[(i)])
#define xv_pop(v) ((v).a[--(v).n])
#define xv_size(v) ((v).n)
#define xv_max(v) ((v).m)
#define xv_data(v) ((v).a)

#define xv_resize(type, v, s)  ((v).m = (s), (v).a = (type*)xrealloc((v).a, sizeof(type) * (v).m))
#define xv_trim(type, v) (xv_resize(type, v, xv_size(v)))

#define xv_copy(type, v1, v0) do {							\
		if ((v1).m < (v0).n) xv_resize(type, v1, (v0).n);	\
		(v1).n = (v0).n;									\
		memcpy((v1).a, (v0).a, sizeof(type) * (v0).n);		\
	} while (0)												\

#define xv_push(type, v, x) do {									\
		if ((v).n == (v).m) {										\
			(v).m = (v).m? (v).m<<1 : 2;							\
			(v).a = (type*)xrealloc((v).a, sizeof(type) * (v).m);	\
		}															\
		(v).a[(v).n++] = (x);										\
	} while (0)

#define xv_pushp(type, v) ((((v).n == (v).m)?							\
						   ((v).m = ((v).m? (v).m<<1 : 2),				\
							(v).a = (type*)xrealloc((v).a, sizeof(type) * (v).m), 0)	\
						   : 0), ((v).a + ((v).n++)))

#define xv_a(type, v, i) (((v).m <= (size_t)(i)? \
						  ((v).m = (v).n = (i) + 1, xv_roundup32((v).m), \
						   (v).a = (type*)xrealloc((v).a, sizeof(type) * (v).m), 0) \
						  : (v).n <= (size_t)(i)? (v).n = (i) + 1 \
						  : 0), (v).a[(i)])

/*
 * Quickly delete an item at the given index, by copying the item at the end of
 * the array to the index and then popping. Thus, this does *not* preserve the
 * order of the array and thus should be used if and only that is okay for your
 * use case.
 */
#define xv_quickdel(v, i) \
    do { \
        xv_A(v, i) = xv_A(v, xv_size(v)-1); \
        (void) xv_pop(v); \
    } while (0);

#endif /* XLIB_XVEC_H_ */
