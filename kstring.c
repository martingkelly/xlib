#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "kstring.h"

int ksprintf(kstring_t *s, const char *fmt, ...)
{
	va_list ap;
	int l;
	va_start(ap, fmt);
	l = vsnprintf(s->s + s->l, s->m - s->l, fmt, ap); // This line does not work with glibc 2.0. See `man snprintf'.
	va_end(ap);
	if (l + 1 > s->m - s->l) {
		s->m = s->l + l + 2;
		kroundup32(s->m);
		s->s = (char*)realloc(s->s, s->m);
		va_start(ap, fmt);
		l = vsnprintf(s->s + s->l, s->m - s->l, fmt, ap);
	}
	va_end(ap);
	s->l += l;
	return l;
}

char *kstrtok(const char *str, const char *sep, ks_tokaux_t *aux)
{
	const char *p, *start;
	if (sep) { // set up the table
		if (str == 0 && (aux->tab[0]&1)) return 0; // no need to set up if we have finished
		aux->finished = 0;
		if (sep[1]) {
			aux->sep = -1;
			aux->tab[0] = aux->tab[1] = aux->tab[2] = aux->tab[3] = 0;
			for (p = sep; *p; ++p) aux->tab[*p>>6] |= 1ull<<(*p&0x3f);
		} else aux->sep = sep[0];
	}
	if (aux->finished) return 0;
	else if (str) aux->p = str - 1, aux->finished = 0;
	if (aux->sep < 0) {
		for (p = start = aux->p + 1; *p; ++p)
			if (aux->tab[*p>>6]>>(*p&0x3f)&1) break;
	} else {
		for (p = start = aux->p + 1; *p; ++p)
			if (*p == aux->sep) break;
	}
	aux->p = p; // end of token
	if (*p == 0) aux->finished = 1; // no more tokens
	return (char*)start;
}

// s MUST BE a null terminated string; l = strlen(s)
int ksplit_core(char *s, int delimiter, int *_max, int **_offsets)
{
	int i, n, max, last_char, last_start, *offsets, l;
	n = 0; max = *_max; offsets = *_offsets;
	l = strlen(s);
	
#define __ksplit_aux do {												\
		if (_offsets) {													\
			s[i] = 0;													\
			if (n == max) {												\
				max = max? max<<1 : 2;									\
				offsets = (int*)realloc(offsets, sizeof(int) * max);	\
			}															\
			offsets[n++] = last_start;									\
		} else ++n;														\
	} while (0)

	for (i = 0, last_char = last_start = 0; i <= l; ++i) {
		if (delimiter == 0) {
			if (isspace(s[i]) || s[i] == 0) {
				if (isgraph(last_char)) __ksplit_aux; // the end of a field
			} else {
				if (isspace(last_char) || last_char == 0) last_start = i;
			}
		} else {
			if (s[i] == delimiter || s[i] == 0) {
				if (last_char != 0 && last_char != delimiter) __ksplit_aux; // the end of a field
			} else {
				if (last_char == delimiter || last_char == 0) last_start = i;
			}
		}
		last_char = s[i];
	}
	*_max = max; *_offsets = offsets;
	return n;
}

/**********************
 * Boyer-Moore search *
 **********************/

typedef unsigned char ubyte_t;

// reference: http://www-igm.univ-mlv.fr/~lecroq/string/node14.html
static int *ksBM_prep(const ubyte_t *pat, int m)
{
	int i, *suff, *prep, *bmGs, *bmBc;
	prep = calloc(m + 256, sizeof(int));
	bmGs = prep; bmBc = prep + m;
	{ // preBmBc()
		for (i = 0; i < 256; ++i) bmBc[i] = m;
		for (i = 0; i < m - 1; ++i) bmBc[pat[i]] = m - i - 1;
	}
	suff = calloc(m, sizeof(int));
	{ // suffixes()
		int f = 0, g;
		suff[m - 1] = m;
		g = m - 1;
		for (i = m - 2; i >= 0; --i) {
			if (i > g && suff[i + m - 1 - f] < i - g)
				suff[i] = suff[i + m - 1 - f];
			else {
				if (i < g) g = i;
				f = i;
				while (g >= 0 && pat[g] == pat[g + m - 1 - f]) --g;
				suff[i] = f - g;
			}
		}
	}
	{ // preBmGs()
		int j = 0;
		for (i = 0; i < m; ++i) bmGs[i] = m;
		for (i = m - 1; i >= 0; --i)
			if (suff[i] == i + 1)
				for (; j < m - 1 - i; ++j)
					if (bmGs[j] == m)
						bmGs[j] = m - 1 - i;
		for (i = 0; i <= m - 2; ++i)
			bmGs[m - 1 - suff[i]] = m - 1 - i;
	}
	free(suff);
	return prep;
}

void *kmemmem(const void *_str, int n, const void *_pat, int m, int **_prep)
{
	int i, j, *prep = 0, *bmGs, *bmBc;
	const ubyte_t *str, *pat;
	str = (const ubyte_t*)_str; pat = (const ubyte_t*)_pat;
	prep = (_prep == 0 || *_prep == 0)? ksBM_prep(pat, m) : *_prep;
	if (_prep && *_prep == 0) *_prep = prep;
	bmGs = prep; bmBc = prep + m;
	j = 0;
	while (j <= n - m) {
		for (i = m - 1; i >= 0 && pat[i] == str[i+j]; --i);
		if (i >= 0) {
			int max = bmBc[str[i+j]] - m + 1 + i;
			if (max < bmGs[i]) max = bmGs[i];
			j += max;
		} else return (void*)(str + j);
	}
	if (_prep == 0) free(prep);
	return 0;
}

char *kstrstr(const char *str, const char *pat, int **_prep)
{
	return (char*)kmemmem(str, strlen(str), pat, strlen(pat), _prep);
}

char *kstrnstr(const char *str, const char *pat, int n, int **_prep)
{
	return (char*)kmemmem(str, n, pat, strlen(pat), _prep);
}

/****************
 * fast sprintf *
 ****************/

typedef struct {
	int base, w, f, left_aln, n_ell, n_Ell;
} printf_conv_t;

static inline void enlarge(kstring_t *s, int l)
{
	if (s->l + l + 1 >= s->m) {
		s->m = s->l + l + 2;
		kroundup32(s->m);
		s->s = (char*)realloc(s->s, s->m);
	}
}

double frexp10(double x, int *e)
{
	const double M_LOG_10_2 = M_LN2 / M_LN10;
	double z;
	int tmp;
	if (x == 0.) {
		*e = 0;
		return 0.;
	}
	frexp(x, e);
	if (*e >= 0) {
		*e = (int)(*e * M_LOG_10_2);
		for (z = .1, tmp = *e; tmp; z *= z, tmp >>= 1)
			if (tmp & 1) x *= z;
	} else {
		*e = (int)((*e - 1) * M_LOG_10_2);
		for (z = 10., tmp = -*e; tmp; z *= z, tmp >>= 1)
			if (tmp & 1) x *= z;
	}
	if (x >= 10. || x <= -10.) ++(*e), x *= .1;
	else if (x > -1. && x < 1.) --(*e), x *= 10.;
	return x;
}

int ksprintf_fast(kstring_t *s, const char *fmt, ...)
{

#define write_integer0(_c, _s, _type, _z, _conv) do { \
		char buf[32]; \
		int l = 0, k, w, f; \
		_type x; \
		if (_c != 0) { \
			for (x = _c < 0? -_c : _c; x > 0; x /= _z.base) buf[l++] = _conv[x%_z.base]; \
			if (_c < 0) buf[l++] = '-'; \
		} else buf[l++] = '0'; \
		f = l > _z.f? l : _z.f; \
		w = f > _z.w? f : _z.w; \
		enlarge(_s, w); \
		if (w > f && !_z.left_aln) \
			for (k = f; k < w; ++k) _s->s[_s->l++] = ' '; \
		if (f > l) \
			for (k = l; k < f; ++k) _s->s[_s->l++] = '0'; \
		for (k = l - 1; k >= 0; --k) _s->s[_s->l++] = buf[k]; \
		if (w > f && _z.left_aln) \
			for (k = f; k < w; ++k) _s->s[_s->l++] = ' '; \
	} while (0)

#define write_integer(_ap, _s, _type, _z, _conv) do { \
		_type c = va_arg(_ap, _type); \
		write_integer0(c, _s, _type, _z, _conv); \
	} while (0)

#define write_fraction(_s, _f, _z, _trim0) do { \
		int j, last; \
		for (j = 0; j < _z.f - 1; ++j) { \
			_f = (_f - (int)_f) * 10.; \
			_s->s[_s->l++] = (int)_f + '0'; \
		} \
		_f = (_f - (int)_f) * 10.; \
		last = (int)(_f + .499999999999999999); \
		if (last >= 10) { \
			_s->s[_s->l++] = '0'; \
			for (j = _s->l - 2; _s->s[j] == '9'; --j) _s->s[j] = '0'; \
			if (_s->s[j] != '.') ++_s->s[j]; \
		} else _s->s[_s->l++] = last + '0'; \
		if (_trim0) { \
			while (_s->s[_s->l - 1] == '0') --_s->l; \
			if (_s->s[_s->l - 1] == '.') --_s->l; \
		} \
	} while (0)

	va_list ap;
	const char *p = fmt, *q;
	int state = 0;
	printf_conv_t z, ztmp;
	memset(&z, 0, sizeof(printf_conv_t)); z.f = -1;
	ztmp = z; ztmp.base = 10;
	va_start(ap, fmt);
	while (*p) {
		if (state == 1) {
			int finished = 0;
			if (*p == '%') {
				enlarge(s, 1);
				s->s[s->l++] = '%';
				finished = 1;
			} else if (*p >= '0' && *p <= '9') { // w
				char *r;
				z.w = strtol(p, &r, 10);
				p = r - 1;
			} else if (*p == '.') { // f
				char *r;
				z.f = strtol(p + 1, &r, 10);
				p = r - 1;
			} else if (*p == 'l') { // %l
				++z.n_ell;
			} else if (*p == 'L') { // %L
				++z.n_Ell;
			} else if (*p == '-') { // %-
				z.left_aln = 1;
			} else if (*p == 's') { // %s
				char *r = va_arg(ap, char*);
				int l = strlen(r);
				enlarge(s, l);
				memcpy(s->s + s->l, r, l);
				s->l += l;
				finished = 1;
			} else if (*p == 'c') { // %c
				enlarge(s, 1);
				s->s[s->l++] = va_arg(ap, int);
				finished = 1;
			} else if (*p == 'd' || *p == 'i') { // %d or %i
				z.base = 10;
				if (z.n_ell == 0) write_integer(ap, s, int, z, "0123456789");
				else if (z.n_ell == 1) write_integer(ap, s, long, z, "0123456789");
				else write_integer(ap, s, long long, z, "0123456789");
				finished = 1;
			} else if (*p == 'u' || *p == 'o' || *p == 'x' || *p == 'X') { // %u, %o or %x
				const char *conv = (*p == 'X')? "0123456789ABCDEF" : "0123456789abcdef";
				if (*p == 'u') z.base = 10;
				else if (*p == 'o') z.base = 8;
				else if (*p == 'x' || *p == 'X') z.base = 16;
				if (z.n_ell == 0) write_integer(ap, s, unsigned, z, conv);
				else if (z.n_ell == 1) write_integer(ap, s, unsigned long, z, conv);
				else write_integer(ap, s, unsigned long long, z, conv);
				finished = 1;
			} else if (*p == 'f' || *p == 'e' || *p == 'g') {
				const char *t, *r = 0;
				double x = va_arg(ap, double);
				uint64_t *y = (uint64_t*)&x;
				int l;
				if ((*y >> 52 & 0x7ff) == 0x7ff) { // nan, inf or -inf
					if (*y & 0x000fffffffffffffull) r = "nan";
					else if (*y>>63) r = "-inf";
					else r = "inf";
					l = strlen(r);
					enlarge(s, l);
					for (t = r; *t; ++t) s->s[s->l++] = *t;
				} else {
					double f;
					int type = *p, e, w, trim0 = (*p == 'g')? 1 : 0;
					if (z.f < 0) z.f = 6;
					f = frexp10(x, &e);
					if (*p == 'g') type = (e < -4 || e >= z.f)? 'e' : 'f'; // see the printf() manual page
					if (type == 'e') {
						int j, w = 8 + z.f > z.w? 8 + z.f : z.w;
						enlarge(s, w);
						if (f < 0) s->s[s->l++] = '-', f = -f;
						s->s[s->l++] = (int)f + '0';
						if (z.f > 0) {
							s->s[s->l++] = '.';
							write_fraction(s, f, z, trim0);
						}
						s->s[s->l++] = 'e';
						if (e >= 0) s->s[s->l++] = '+';
						ztmp.f = 2;
						write_integer0(e, s, int, ztmp, "0123456789");
					} else {
						int j, w = abs(e) + 2 + z.f > z.w? abs(e) + 2 + z.f : z.w;
						enlarge(s, w);
						if (f < 0) s->s[s->l++] = '-', f = -f;
						if (e >= 0) {
							for (j = 0; j < e; ++j) {
								s->s[s->l++] = (int)f + '0';
								f = (f - (int)f) * 10.;
							}
							if (z.f != 0) {
								s->s[s->l++] = (int)f + '0';
								s->s[s->l++] = '.';
								write_fraction(s, f, z, trim0);
							} else s->s[s->l++] = (int)(f + .49999999999999999) + '0';
						} else {
							s->s[s->l++] = '0';
							if (z.f != 0) {
								s->s[s->l++] = '.';
								for (j = 1; j < -e && j <= z.f; ++j) s->s[s->l++] = '0';
								if (j - 1 < z.f) {
									f *= .1;
									z.f -= j - 1;
									write_fraction(s, f, z, trim0);
								}
							}
						}
					}
				}
				finished = 1;
			}
			++p;
			if (finished) state = 0;
		} else {
			q = p;
			while (*p && *p != '%') ++p;
			if (p > q) {
				enlarge(s, p - q);
				memcpy(s->s + s->l, q, p - q);
				s->l += p - q;
			}
			if (*p == '%') {
				state = 1;
				++p;
				memset(&z, 0, sizeof(printf_conv_t)); z.f = -1;
			}
		}
	}
	va_end(ap);
	s->s[s->l] = 0;
	return 0;
}

/***********************
 * The main() function *
 ***********************/

#ifdef KSTRING_MAIN
#include <stdio.h>
int main()
{
	kstring_t *s;
	int *fields, n, i;
	ks_tokaux_t aux;
	char *p;
	s = (kstring_t*)calloc(1, sizeof(kstring_t));
	{ // test ksprintf_fast()
		long xx = -10;
		int e;
		ksprintf_fast(s, " pooiu %% %s %ld %c%-5.4X %g", "+++", xx, '*', 110, 0.246); printf("'%s'\n", s->s); s->l = 0;
		frexp10(-1.2e10, &e);
		printf("%g, %.1f\n", 1e100, 1e30);
	}
	// test ksprintf()
	ksprintf(s, " abcdefg:    %d ", 100);
	printf("'%s'\n", s->s);
	// test ksplit()
	fields = ksplit(s, 0, &n);
	for (i = 0; i < n; ++i)
		printf("field[%d] = '%s'\n", i, s->s + fields[i]);
	// test kstrtok()
	s->l = 0;
	for (p = kstrtok("ab:cde:fg/hij::k", ":/", &aux); p; p = kstrtok(0, 0, &aux)) {
		kputsn(p, aux.p - p, s);
		kputc('\n', s);
	}
	printf("%s", s->s);
	// free
	free(s->s); free(s); free(fields);

	{
		static char *str = "abcdefgcdgcagtcakcdcd";
		static char *pat = "cd";
		char *ret, *s = str;
		int *prep = 0;
		while ((ret = kstrstr(s, pat, &prep)) != 0) {
			printf("match: %s\n", ret);
			s = ret + prep[0];
		}
		free(prep);
	}
	return 0;
}
#endif
