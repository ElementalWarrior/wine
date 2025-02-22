/*
 * MSVCRT string functions
 *
 * Copyright 1996,1998 Marcus Meissner
 * Copyright 1996 Jukka Iivonen
 * Copyright 1997,2000 Uwe Bonnes
 * Copyright 2000 Jon Griffiths
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <locale.h>
#include <float.h>
#include "msvcrt.h"
#include "bnum.h"
#include "winnls.h"
#include "wine/asm.h"
#include "wine/debug.h"

#include <immintrin.h>

WINE_DEFAULT_DEBUG_CHANNEL(msvcrt);

/*********************************************************************
 *		_mbsdup (MSVCRT.@)
 *		_strdup (MSVCRT.@)
 */
char* CDECL _strdup(const char* str)
{
    if(str)
    {
      char * ret = malloc(strlen(str)+1);
      if (ret) strcpy( ret, str );
      return ret;
    }
    else return 0;
}

/*********************************************************************
 *		_strlwr_s_l (MSVCRT.@)
 */
int CDECL _strlwr_s_l(char *str, size_t len, _locale_t locale)
{
    pthreadlocinfo locinfo;
    char *ptr = str;

    if (!str || !len)
    {
        *_errno() = EINVAL;
        return EINVAL;
    }

    while (len && *ptr)
    {
        len--;
        ptr++;
    }

    if (!len)
    {
        str[0] = '\0';
        *_errno() = EINVAL;
        return EINVAL;
    }

    if(!locale)
        locinfo = get_locinfo();
    else
        locinfo = locale->locinfo;

    if(!locinfo->lc_handle[LC_CTYPE])
    {
        while (*str)
        {
            if (*str >= 'A' && *str <= 'Z')
                *str -= 'A' - 'a';
            str++;
        }
    }
    else
    {
        while (*str)
        {
            *str = _tolower_l((unsigned char)*str, locale);
            str++;
        }
    }

    return 0;
}

/*********************************************************************
 *		_strlwr_s (MSVCRT.@)
 */
int CDECL _strlwr_s(char *str, size_t len)
{
    return _strlwr_s_l(str, len, NULL);
}

/*********************************************************************
 *		_strlwr_l (MSVCRT.@)
 */
char* CDECL _strlwr_l(char *str, _locale_t locale)
{
    _strlwr_s_l(str, -1, locale);
    return str;
}

/*********************************************************************
 *		_strlwr (MSVCRT.@)
 */
char* CDECL _strlwr(char *str)
{
    _strlwr_s_l(str, -1, NULL);
    return str;
}

/*********************************************************************
 *              _strupr_s_l (MSVCRT.@)
 */
int CDECL _strupr_s_l(char *str, size_t len, _locale_t locale)
{
    pthreadlocinfo locinfo;
    char *ptr = str;

    if (!str || !len)
    {
        *_errno() = EINVAL;
        return EINVAL;
    }

    while (len && *ptr)
    {
        len--;
        ptr++;
    }

    if (!len)
    {
        str[0] = '\0';
        *_errno() = EINVAL;
        return EINVAL;
    }

    if(!locale)
        locinfo = get_locinfo();
    else
        locinfo = locale->locinfo;

    if(!locinfo->lc_handle[LC_CTYPE])
    {
        while (*str)
        {
            if (*str >= 'a' && *str <= 'z')
                *str -= 'a' - 'A';
            str++;
        }
    }
    else
    {
        while (*str)
        {
            *str = _toupper_l((unsigned char)*str, locale);
            str++;
        }
    }

    return 0;
}

/*********************************************************************
 *              _strupr_s (MSVCRT.@)
 */
int CDECL _strupr_s(char *str, size_t len)
{
    return _strupr_s_l(str, len, NULL);
}

/*********************************************************************
 *              _strupr_l (MSVCRT.@)
 */
char* CDECL _strupr_l(char *str, _locale_t locale)
{
    _strupr_s_l(str, -1, locale);
    return str;
}

/*********************************************************************
 *              _strupr (MSVCRT.@)
 */
char* CDECL _strupr(char *str)
{
    _strupr_s_l(str, -1, NULL);
    return str;
}

/*********************************************************************
 *              _strnset_s (MSVCRT.@)
 */
int CDECL _strnset_s(char *str, size_t size, int c, size_t count)
{
    size_t i;

    if(!str && !size && !count) return 0;
    if(!MSVCRT_CHECK_PMT(str != NULL)) return EINVAL;
    if(!MSVCRT_CHECK_PMT(size > 0)) return EINVAL;

    for(i=0; i<size-1 && i<count; i++) {
        if(!str[i]) return 0;
        str[i] = c;
    }
    for(; i<size; i++)
        if(!str[i]) return 0;

    str[0] = 0;
    _invalid_parameter(NULL, NULL, NULL, 0, 0);
    *_errno() = EINVAL;
    return EINVAL;
}

/*********************************************************************
 *		_strnset (MSVCRT.@)
 */
char* CDECL _strnset(char* str, int value, size_t len)
{
  if (len > 0 && str)
    while (*str && len--)
      *str++ = value;
  return str;
}

/*********************************************************************
 *		_strrev (MSVCRT.@)
 */
char* CDECL _strrev(char* str)
{
  char * p1;
  char * p2;

  if (str && *str)
    for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
    {
      *p1 ^= *p2;
      *p2 ^= *p1;
      *p1 ^= *p2;
    }

  return str;
}

/*********************************************************************
 *		_strset (MSVCRT.@)
 */
char* CDECL _strset(char* str, int value)
{
  char *ptr = str;
  while (*ptr)
    *ptr++ = value;

  return str;
}

/*********************************************************************
 *		strtok  (MSVCRT.@)
 */
char * CDECL strtok( char *str, const char *delim )
{
    thread_data_t *data = msvcrt_get_thread_data();
    char *ret;

    if (!str)
        if (!(str = data->strtok_next)) return NULL;

    while (*str && strchr( delim, *str )) str++;
    if (!*str)
    {
        data->strtok_next = str;
        return NULL;
    }
    ret = str++;
    while (*str && !strchr( delim, *str )) str++;
    if (*str) *str++ = 0;
    data->strtok_next = str;
    return ret;
}

/*********************************************************************
 *		strtok_s  (MSVCRT.@)
 */
char * CDECL strtok_s(char *str, const char *delim, char **ctx)
{
    if (!MSVCRT_CHECK_PMT(delim != NULL)) return NULL;
    if (!MSVCRT_CHECK_PMT(ctx != NULL)) return NULL;
    if (!MSVCRT_CHECK_PMT(str != NULL || *ctx != NULL)) return NULL;

    if(!str)
        str = *ctx;

    while(*str && strchr(delim, *str))
        str++;
    if(!*str)
    {
        *ctx = str;
        return NULL;
    }

    *ctx = str+1;
    while(**ctx && !strchr(delim, **ctx))
        (*ctx)++;
    if(**ctx)
        *(*ctx)++ = 0;

    return str;
}

/*********************************************************************
 *		_swab (MSVCRT.@)
 */
void CDECL _swab(char* src, char* dst, int len)
{
  if (len > 1)
  {
    len = (unsigned)len >> 1;

    while (len--) {
      char s0 = src[0];
      char s1 = src[1];
      *dst++ = s1;
      *dst++ = s0;
      src = src + 2;
    }
  }
}

static struct fpnum fpnum(int sign, int exp, ULONGLONG m, enum fpmod mod)
{
    struct fpnum ret;

    ret.sign = sign;
    ret.exp = exp;
    ret.m = m;
    ret.mod = mod;
    return ret;
}

int fpnum_double(struct fpnum *fp, double *d)
{
    ULONGLONG bits = 0;

    if (fp->mod == FP_VAL_INFINITY)
    {
        *d = fp->sign * INFINITY;
        return 0;
    }

    if (fp->mod == FP_VAL_NAN)
    {
        bits = ~0;
        if (fp->sign == 1)
            bits &= ~((ULONGLONG)1 << (MANT_BITS + EXP_BITS - 1));
        *d = *(double*)&bits;
        return 0;
    }

    TRACE("%c %s *2^%d (round %d)\n", fp->sign == -1 ? '-' : '+',
            wine_dbgstr_longlong(fp->m), fp->exp, fp->mod);
    if (!fp->m)
    {
        *d = fp->sign * 0.0;
        return 0;
    }

    /* make sure that we don't overflow modifying exponent */
    if (fp->exp > 1<<EXP_BITS)
    {
        *d = fp->sign * INFINITY;
        return ERANGE;
    }
    if (fp->exp < -(1<<EXP_BITS))
    {
        *d = fp->sign * 0.0;
        return ERANGE;
    }
    fp->exp += MANT_BITS - 1;

    /* normalize mantissa */
    while(fp->m < (ULONGLONG)1 << (MANT_BITS-1))
    {
        fp->m <<= 1;
        fp->exp--;
    }
    while(fp->m >= (ULONGLONG)1 << MANT_BITS)
    {
        if (fp->m & 1 || fp->mod != FP_ROUND_ZERO)
        {
            if (!(fp->m & 1)) fp->mod = FP_ROUND_DOWN;
            else if(fp->mod == FP_ROUND_ZERO) fp->mod = FP_ROUND_EVEN;
            else fp->mod = FP_ROUND_UP;
        }
        fp->m >>= 1;
        fp->exp++;
    }
    fp->exp += (1 << (EXP_BITS-1)) - 1;

    /* handle subnormals */
    if (fp->exp <= 0)
    {
        if (fp->m & 1 && fp->mod == FP_ROUND_ZERO) fp->mod = FP_ROUND_EVEN;
        else if (fp->m & 1) fp->mod = FP_ROUND_UP;
        else if (fp->mod != FP_ROUND_ZERO) fp->mod = FP_ROUND_DOWN;
        fp->m >>= 1;
    }
    while(fp->m && fp->exp<0)
    {
        if (fp->m & 1 && fp->mod == FP_ROUND_ZERO) fp->mod = FP_ROUND_EVEN;
        else if (fp->m & 1) fp->mod = FP_ROUND_UP;
        else if (fp->mod != FP_ROUND_ZERO) fp->mod = FP_ROUND_DOWN;
        fp->m >>= 1;
        fp->exp++;
    }

    /* round mantissa */
    if (fp->mod == FP_ROUND_UP || (fp->mod == FP_ROUND_EVEN && fp->m & 1))
    {
        fp->m++;

        /* handle subnormal that falls into regular range due to rounding */
        if (fp->m == (ULONGLONG)1 << (MANT_BITS - 1))
        {
            fp->exp++;
        }
        else if (fp->m >= (ULONGLONG)1 << MANT_BITS)
        {
            fp->exp++;
            fp->m >>= 1;
        }
    }

    if (fp->exp >= (1<<EXP_BITS)-1)
    {
        *d = fp->sign * INFINITY;
        return ERANGE;
    }
    if (!fp->m || fp->exp < 0)
    {
        *d = fp->sign * 0.0;
        return ERANGE;
    }

    if (fp->sign == -1)
        bits |= (ULONGLONG)1 << (MANT_BITS + EXP_BITS - 1);
    bits |= (ULONGLONG)fp->exp << (MANT_BITS - 1);
    bits |= fp->m & (((ULONGLONG)1 << (MANT_BITS - 1)) - 1);

    TRACE("returning %s\n", wine_dbgstr_longlong(bits));
    *d = *(double*)&bits;
    return 0;
}

#define LDBL_EXP_BITS 15
#define LDBL_MANT_BITS 64
int fpnum_ldouble(struct fpnum *fp, MSVCRT__LDOUBLE *d)
{
    if (fp->mod == FP_VAL_INFINITY)
    {
        d->x80[0] = 0;
        d->x80[1] = 0x80000000;
        d->x80[2] = (1 << LDBL_EXP_BITS) - 1;
        if (fp->sign == -1)
            d->x80[2] |= 1 << LDBL_EXP_BITS;
        return 0;
    }

    if (fp->mod == FP_VAL_NAN)
    {
        d->x80[0] = ~0;
        d->x80[1] = ~0;
        d->x80[2] = (1 << LDBL_EXP_BITS) - 1;
        if (fp->sign == -1)
            d->x80[2] |= 1 << LDBL_EXP_BITS;
        return 0;
    }

    TRACE("%c %s *2^%d (round %d)\n", fp->sign == -1 ? '-' : '+',
            wine_dbgstr_longlong(fp->m), fp->exp, fp->mod);
    if (!fp->m)
    {
        d->x80[0] = 0;
        d->x80[1] = 0;
        d->x80[2] = 0;
        if (fp->sign == -1)
            d->x80[2] |= 1 << LDBL_EXP_BITS;
        return 0;
    }

    /* make sure that we don't overflow modifying exponent */
    if (fp->exp > 1<<LDBL_EXP_BITS)
    {
        d->x80[0] = 0;
        d->x80[1] = 0x80000000;
        d->x80[2] = (1 << LDBL_EXP_BITS) - 1;
        if (fp->sign == -1)
            d->x80[2] |= 1 << LDBL_EXP_BITS;
        return ERANGE;
    }
    if (fp->exp < -(1<<LDBL_EXP_BITS))
    {
        d->x80[0] = 0;
        d->x80[1] = 0;
        d->x80[2] = 0;
        if (fp->sign == -1)
            d->x80[2] |= 1 << LDBL_EXP_BITS;
        return ERANGE;
    }
    fp->exp += LDBL_MANT_BITS - 1;

    /* normalize mantissa */
    while(fp->m < (ULONGLONG)1 << (LDBL_MANT_BITS-1))
    {
        fp->m <<= 1;
        fp->exp--;
    }
    fp->exp += (1 << (LDBL_EXP_BITS-1)) - 1;

    /* handle subnormals */
    if (fp->exp <= 0)
    {
        if (fp->m & 1 && fp->mod == FP_ROUND_ZERO) fp->mod = FP_ROUND_EVEN;
        else if (fp->m & 1) fp->mod = FP_ROUND_UP;
        else if (fp->mod != FP_ROUND_ZERO) fp->mod = FP_ROUND_DOWN;
        fp->m >>= 1;
    }
    while(fp->m && fp->exp<0)
    {
        if (fp->m & 1 && fp->mod == FP_ROUND_ZERO) fp->mod = FP_ROUND_EVEN;
        else if (fp->m & 1) fp->mod = FP_ROUND_UP;
        else if (fp->mod != FP_ROUND_ZERO) fp->mod = FP_ROUND_DOWN;
        fp->m >>= 1;
        fp->exp++;
    }

    /* round mantissa */
    if (fp->mod == FP_ROUND_UP || (fp->mod == FP_ROUND_EVEN && fp->m & 1))
    {
        if (fp->m == UI64_MAX)
        {
            fp->m = (ULONGLONG)1 << (LDBL_MANT_BITS - 1);
            fp->exp++;
        }
        else
        {
            fp->m++;

            /* handle subnormal that falls into regular range due to rounding */
            if ((fp->m ^ (fp->m - 1)) & ((ULONGLONG)1 << (LDBL_MANT_BITS - 1))) fp->exp++;
        }
    }

    if (fp->exp >= (1<<LDBL_EXP_BITS)-1)
    {
        d->x80[0] = 0;
        d->x80[1] = 0x80000000;
        d->x80[2] = (1 << LDBL_EXP_BITS) - 1;
        if (fp->sign == -1)
            d->x80[2] |= 1 << LDBL_EXP_BITS;
        return ERANGE;
    }
    if (!fp->m || fp->exp < 0)
    {
        d->x80[0] = 0;
        d->x80[1] = 0;
        d->x80[2] = 0;
        if (fp->sign == -1)
            d->x80[2] |= 1 << LDBL_EXP_BITS;
        return ERANGE;
    }

    d->x80[0] = fp->m;
    d->x80[1] = fp->m >> 32;
    d->x80[2] = fp->exp;
    if (fp->sign == -1)
        d->x80[2] |= 1 << LDBL_EXP_BITS;
    return 0;
}

#if _MSVCR_VER >= 140

static inline int hex2int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static struct fpnum fpnum_parse16(wchar_t get(void *ctx), void unget(void *ctx),
        void *ctx, int sign, pthreadlocinfo locinfo)
{
    BOOL found_digit = FALSE, found_dp = FALSE;
    enum fpmod round = FP_ROUND_ZERO;
    wchar_t nch;
    ULONGLONG m = 0;
    int val, exp = 0;

    nch = get(ctx);
    while(m < UI64_MAX/16)
    {
        val = hex2int(nch);
        if (val == -1) break;
        found_digit = TRUE;
        nch = get(ctx);

        m = m*16 + val;
    }
    while(1)
    {
        val = hex2int(nch);
        if (val == -1) break;
        nch = get(ctx);
        exp += 4;

        if (val || round != FP_ROUND_ZERO)
        {
            if (val < 8) round = FP_ROUND_DOWN;
            else if (val == 8 && round == FP_ROUND_ZERO) round = FP_ROUND_EVEN;
            else round = FP_ROUND_UP;
        }
    }

    if(nch == *locinfo->lconv->decimal_point)
    {
        found_dp = TRUE;
        nch = get(ctx);
    }
    else if (!found_digit)
    {
        if(nch!=WEOF) unget(ctx);
        unget(ctx);
        return fpnum(0, 0, 0, 0);
    }

    while(m <= UI64_MAX/16)
    {
        val = hex2int(nch);
        if (val == -1) break;
        found_digit = TRUE;
        nch = get(ctx);

        m = m*16 + val;
        exp -= 4;
    }
    while(1)
    {
        val = hex2int(nch);
        if (val == -1) break;
        nch = get(ctx);

        if (val || round != FP_ROUND_ZERO)
        {
            if (val < 8) round = FP_ROUND_DOWN;
            else if (val == 8 && round == FP_ROUND_ZERO) round = FP_ROUND_EVEN;
            else round = FP_ROUND_UP;
        }
    }

    if (!found_digit)
    {
        if (nch != WEOF) unget(ctx);
        if (found_dp) unget(ctx);
        unget(ctx);
        return fpnum(0, 0, 0, 0);
    }

    if(nch=='p' || nch=='P') {
        BOOL found_sign = FALSE;
        int e=0, s=1;

        nch = get(ctx);
        if(nch == '-') {
            found_sign = TRUE;
            s = -1;
            nch = get(ctx);
        } else if(nch == '+') {
            found_sign = TRUE;
            nch = get(ctx);
        }
        if(nch>='0' && nch<='9') {
            while(nch>='0' && nch<='9') {
                if(e>INT_MAX/10 || e*10>INT_MAX-nch+'0')
                    e = INT_MAX;
                else
                    e = e*10+nch-'0';
                nch = get(ctx);
            }
            if((nch!=WEOF) && (nch < '0' || nch > '9')) unget(ctx);
            e *= s;

            if(e<0 && exp<INT_MIN-e) exp = INT_MIN;
            else if(e>0 && exp>INT_MAX-e) exp = INT_MAX;
            else exp += e;
        } else {
            if(nch != WEOF) unget(ctx);
            if(found_sign) unget(ctx);
            unget(ctx);
        }
    }

    return fpnum(sign, exp, m, round);
}
#endif

/* Converts first 3 limbs to ULONGLONG */
/* Return FALSE on overflow */
static inline BOOL bnum_to_mant(struct bnum *b, ULONGLONG *m)
{
    if(UI64_MAX / LIMB_MAX / LIMB_MAX < b->data[bnum_idx(b, b->e-1)]) return FALSE;
    *m = (ULONGLONG)b->data[bnum_idx(b, b->e-1)] * LIMB_MAX * LIMB_MAX;
    if(b->b == b->e-1) return TRUE;
    if(UI64_MAX - *m < (ULONGLONG)b->data[bnum_idx(b, b->e-2)] * LIMB_MAX) return FALSE;
    *m += (ULONGLONG)b->data[bnum_idx(b, b->e-2)] * LIMB_MAX;
    if(b->b == b->e-2) return TRUE;
    if(UI64_MAX - *m < b->data[bnum_idx(b, b->e-3)]) return FALSE;
    *m += b->data[bnum_idx(b, b->e-3)];
    return TRUE;
}

static struct fpnum fpnum_parse_bnum(wchar_t (*get)(void *ctx), void (*unget)(void *ctx),
        void *ctx, pthreadlocinfo locinfo, BOOL ldouble, struct bnum *b)
{
#if _MSVCR_VER >= 140
    const wchar_t _infinity[] = L"infinity";
    const wchar_t _nan[] = L"nan";
    const wchar_t *str_match = NULL;
    int matched=0;
#endif
    BOOL found_digit = FALSE, found_dp = FALSE, found_sign = FALSE;
    int e2 = 0, dp=0, sign=1, off, limb_digits = 0, i;
    enum fpmod round = FP_ROUND_ZERO;
    wchar_t nch;
    ULONGLONG m;

    nch = get(ctx);
    if(nch == '-') {
        found_sign = TRUE;
        sign = -1;
        nch = get(ctx);
    } else if(nch == '+') {
        found_sign = TRUE;
        nch = get(ctx);
    }

#if _MSVCR_VER >= 140
    if(nch == _infinity[0] || nch == _toupper(_infinity[0]))
        str_match = _infinity;
    if(nch == _nan[0] || nch == _toupper(_nan[0]))
        str_match = _nan;
    while(str_match && nch != WEOF &&
            (nch == str_match[matched] || nch == _toupper(str_match[matched]))) {
        nch = get(ctx);
        matched++;
    }
    if(str_match) {
        int keep = 0;
        if(matched >= 8) keep = 8;
        else if(matched >= 3) keep = 3;
        if(nch != WEOF) unget(ctx);
        for (; matched > keep; matched--) {
            unget(ctx);
        }
        if(keep) {
            if (str_match == _infinity)
                return fpnum(sign, 0, 0, FP_VAL_INFINITY);
            if (str_match == _nan)
                return fpnum(sign, 0, 0, FP_VAL_NAN);
        } else if(found_sign) {
            unget(ctx);
        }

        return fpnum(0, 0, 0, 0);
    }

    if(nch == '0') {
        found_digit = TRUE;
        nch = get(ctx);
        if(nch == 'x' || nch == 'X')
            return fpnum_parse16(get, unget, ctx, sign, locinfo);
    }
#endif

    while(nch == '0') {
        found_digit = TRUE;
        nch = get(ctx);
    }

    b->b = 0;
    b->e = 1;
    b->data[0] = 0;
    while(nch>='0' && nch<='9') {
        found_digit = TRUE;
        if(limb_digits == LIMB_DIGITS) {
            if(bnum_idx(b, b->b-1) == bnum_idx(b, b->e)) break;
            else {
                b->b--;
                b->data[bnum_idx(b, b->b)] = 0;
                limb_digits = 0;
            }
        }

        b->data[bnum_idx(b, b->b)] = b->data[bnum_idx(b, b->b)] * 10 + nch - '0';
        limb_digits++;
        nch = get(ctx);
        dp++;
    }
    while(nch>='0' && nch<='9') {
        if(nch != '0') b->data[bnum_idx(b, b->b)] |= 1;
        nch = get(ctx);
        dp++;
    }

    if(nch == *locinfo->lconv->decimal_point) {
        found_dp = TRUE;
        nch = get(ctx);
    }

    /* skip leading '0' */
    if(nch=='0' && !limb_digits && !b->b) {
        found_digit = TRUE;
        while(nch == '0') {
            nch = get(ctx);
            dp--;
        }
    }

    while(nch>='0' && nch<='9') {
        found_digit = TRUE;
        if(limb_digits == LIMB_DIGITS) {
            if(bnum_idx(b, b->b-1) == bnum_idx(b, b->e)) break;
            else {
                b->b--;
                b->data[bnum_idx(b, b->b)] = 0;
                limb_digits = 0;
            }
        }

        b->data[bnum_idx(b, b->b)] = b->data[bnum_idx(b, b->b)] * 10 + nch - '0';
        limb_digits++;
        nch = get(ctx);
    }
    while(nch>='0' && nch<='9') {
        if(nch != '0') b->data[bnum_idx(b, b->b)] |= 1;
        nch = get(ctx);
    }

    if(!found_digit) {
        if(nch != WEOF) unget(ctx);
        if(found_dp) unget(ctx);
        if(found_sign) unget(ctx);
        return fpnum(0, 0, 0, 0);
    }

    if(nch=='e' || nch=='E' || nch=='d' || nch=='D') {
        int e=0, s=1;

        nch = get(ctx);
        if(nch == '-') {
            found_sign = TRUE;
            s = -1;
            nch = get(ctx);
        } else if(nch == '+') {
            found_sign = TRUE;
            nch = get(ctx);
        } else {
            found_sign = FALSE;
        }

        if(nch>='0' && nch<='9') {
            while(nch>='0' && nch<='9') {
                if(e>INT_MAX/10 || e*10>INT_MAX-nch+'0')
                    e = INT_MAX;
                else
                    e = e*10+nch-'0';
                nch = get(ctx);
            }
            if(nch != WEOF) unget(ctx);
            e *= s;

            if(e<0 && dp<INT_MIN-e) dp = INT_MIN;
            else if(e>0 && dp>INT_MAX-e) dp = INT_MAX;
            else dp += e;
        } else {
            if(nch != WEOF) unget(ctx);
            if(found_sign) unget(ctx);
            unget(ctx);
        }
    } else if(nch != WEOF) {
        unget(ctx);
    }

    if(!b->data[bnum_idx(b, b->e-1)])
        return fpnum(sign, 0, 0, 0);

    /* Fill last limb with 0 if needed */
    if(b->b+1 != b->e) {
        for(; limb_digits != LIMB_DIGITS; limb_digits++)
            b->data[bnum_idx(b, b->b)] *= 10;
    }
    for(; bnum_idx(b, b->b) < bnum_idx(b, b->e); b->b++) {
        if(b->data[bnum_idx(b, b->b)]) break;
    }

    /* move decimal point to limb boundary */
    if(limb_digits==dp && b->b==b->e-1)
        return fpnum(sign, 0, b->data[bnum_idx(b, b->e-1)], FP_ROUND_ZERO);
    off = (dp - limb_digits) % LIMB_DIGITS;
    if(off < 0) off += LIMB_DIGITS;
    if(off) bnum_mult(b, p10s[off]);

    if(dp-1 > (ldouble ? DBL80_MAX_10_EXP : DBL_MAX_10_EXP))
        return fpnum(sign, INT_MAX, 1, FP_ROUND_ZERO);
    /* Count part of exponent stored in denormalized mantissa. */
    /* Increase exponent range to handle subnormals. */
    if(dp-1 < (ldouble ? DBL80_MIN_10_EXP : DBL_MIN_10_EXP-DBL_DIG-18))
        return fpnum(sign, INT_MIN, 1, FP_ROUND_ZERO);

    while(dp > 3*LIMB_DIGITS) {
        if(bnum_rshift(b, 9)) dp -= LIMB_DIGITS;
        e2 += 9;
    }
    while(dp <= 2*LIMB_DIGITS) {
        if(bnum_lshift(b, 29)) dp += LIMB_DIGITS;
        e2 -= 29;
    }
    /* Make sure most significant mantissa bit will be set */
    while(b->data[bnum_idx(b, b->e-1)] <= 9) {
        bnum_lshift(b, 1);
        e2--;
    }
    while(!bnum_to_mant(b, &m)) {
        bnum_rshift(b, 1);
        e2++;
    }

    if(b->e-4 >= b->b && b->data[bnum_idx(b, b->e-4)]) {
        if(b->data[bnum_idx(b, b->e-4)] > LIMB_MAX/2) round = FP_ROUND_UP;
        else if(b->data[bnum_idx(b, b->e-4)] == LIMB_MAX/2) round = FP_ROUND_EVEN;
        else round = FP_ROUND_DOWN;
    }
    if(round == FP_ROUND_ZERO || round == FP_ROUND_EVEN) {
        for(i=b->e-5; i>=b->b; i--) {
            if(!b->data[bnum_idx(b, b->b)]) continue;
            if(round == FP_ROUND_EVEN) round = FP_ROUND_UP;
            else round = FP_ROUND_DOWN;
        }
    }

    return fpnum(sign, e2, m, round);
}

struct fpnum fpnum_parse(wchar_t (*get)(void *ctx), void (*unget)(void *ctx),
       void *ctx, pthreadlocinfo locinfo, BOOL ldouble)
{
    if(!ldouble) {
        BYTE bnum_data[FIELD_OFFSET(struct bnum, data[BNUM_PREC64])];
        struct bnum *b = (struct bnum*)bnum_data;

        b->size = BNUM_PREC64;
        return fpnum_parse_bnum(get, unget, ctx, locinfo, ldouble, b);
    } else {
        BYTE bnum_data[FIELD_OFFSET(struct bnum, data[BNUM_PREC80])];
        struct bnum *b = (struct bnum*)bnum_data;

        b->size = BNUM_PREC80;
        return fpnum_parse_bnum(get, unget, ctx, locinfo, ldouble, b);
    }
}

static wchar_t strtod_str_get(void *ctx)
{
    const char **p = ctx;
    if (!**p) return WEOF;
    return *(*p)++;
}

static void strtod_str_unget(void *ctx)
{
    const char **p = ctx;
    (*p)--;
}

static inline double strtod_helper(const char *str, char **end, _locale_t locale, int *perr)
{
    pthreadlocinfo locinfo;
    const char *beg, *p;
    struct fpnum fp;
    double ret;
    int err;

    if (perr) *perr = 0;
#if _MSVCR_VER == 0
    else *_errno() = 0;
#endif

    if (!MSVCRT_CHECK_PMT(str != NULL)) {
        if (end) *end = NULL;
        return 0;
    }

    if (!locale)
        locinfo = get_locinfo();
    else
        locinfo = locale->locinfo;

    p = str;
    while(_isspace_l((unsigned char)*p, locale))
        p++;
    beg = p;

    fp = fpnum_parse(strtod_str_get, strtod_str_unget, &p, locinfo, FALSE);
    if (end) *end = (p == beg ? (char*)str : (char*)p);

    err = fpnum_double(&fp, &ret);
    if (perr) *perr = err;
    else if(err) *_errno() = err;
    return ret;
}

/*********************************************************************
 *		_strtod_l  (MSVCRT.@)
 */
double CDECL _strtod_l(const char *str, char **end, _locale_t locale)
{
    return strtod_helper(str, end, locale, NULL);
}

/*********************************************************************
 *		strtod  (MSVCRT.@)
 */
double CDECL strtod( const char *str, char **end )
{
    return _strtod_l( str, end, NULL );
}

#if _MSVCR_VER>=120

/*********************************************************************
 *		strtof_l  (MSVCR120.@)
 */
float CDECL _strtof_l( const char *str, char **end, _locale_t locale )
{
    double ret = _strtod_l(str, end, locale);
    if (ret && isfinite(ret)) {
        float f = ret;
        if (!f || !isfinite(f))
            *_errno() = ERANGE;
    }
    return ret;
}

/*********************************************************************
 *		strtof  (MSVCR120.@)
 */
float CDECL strtof( const char *str, char **end )
{
    return _strtof_l(str, end, NULL);
}

#endif /* _MSVCR_VER>=120 */

/*********************************************************************
 *		atof  (MSVCRT.@)
 */
double CDECL atof( const char *str )
{
    return _strtod_l(str, NULL, NULL);
}

/*********************************************************************
 *		_atof_l  (MSVCRT.@)
 */
double CDECL _atof_l( const char *str, _locale_t locale)
{
    return _strtod_l(str, NULL, locale);
}

/*********************************************************************
 *		_atoflt_l  (MSVCRT.@)
 */
int CDECL _atoflt_l(_CRT_FLOAT *value, char *str, _locale_t locale)
{
    double d;
    int err;

    d = strtod_helper(str, NULL, locale, &err);
    value->f = d;
    if(isinf(value->f))
        return _OVERFLOW;
    if((d!=0 || err) && value->f>-FLT_MIN && value->f<FLT_MIN)
        return _UNDERFLOW;
    return 0;
}

/*********************************************************************
 * _atoflt  (MSVCR100.@)
 */
int CDECL _atoflt(_CRT_FLOAT *value, char *str)
{
    return _atoflt_l(value, str, NULL);
}

/*********************************************************************
 *              _atodbl_l  (MSVCRT.@)
 */
int CDECL _atodbl_l(_CRT_DOUBLE *value, char *str, _locale_t locale)
{
    int err;

    value->x = strtod_helper(str, NULL, locale, &err);
    if(isinf(value->x))
        return _OVERFLOW;
    if((value->x!=0 || err) && value->x>-DBL_MIN && value->x<DBL_MIN)
        return _UNDERFLOW;
    return 0;
}

/*********************************************************************
 *              _atodbl  (MSVCRT.@)
 */
int CDECL _atodbl(_CRT_DOUBLE *value, char *str)
{
    return _atodbl_l(value, str, NULL);
}

/*********************************************************************
 *		_strcoll_l (MSVCRT.@)
 */
int CDECL _strcoll_l( const char* str1, const char* str2, _locale_t locale )
{
    pthreadlocinfo locinfo;

    if(!locale)
        locinfo = get_locinfo();
    else
        locinfo = locale->locinfo;

    if(!locinfo->lc_handle[LC_COLLATE])
        return strcmp(str1, str2);
    return CompareStringA(locinfo->lc_handle[LC_COLLATE], 0, str1, -1, str2, -1)-CSTR_EQUAL;
}

/*********************************************************************
 *		strcoll (MSVCRT.@)
 */
int CDECL strcoll( const char* str1, const char* str2 )
{
    return _strcoll_l(str1, str2, NULL);
}

/*********************************************************************
 *		_stricoll_l (MSVCRT.@)
 */
int CDECL _stricoll_l( const char* str1, const char* str2, _locale_t locale )
{
    pthreadlocinfo locinfo;

    if(!locale)
        locinfo = get_locinfo();
    else
        locinfo = locale->locinfo;

    if(!locinfo->lc_handle[LC_COLLATE])
        return _stricmp(str1, str2);
    return CompareStringA(locinfo->lc_handle[LC_COLLATE], NORM_IGNORECASE,
            str1, -1, str2, -1)-CSTR_EQUAL;
}

/*********************************************************************
 *		_stricoll (MSVCRT.@)
 */
int CDECL _stricoll( const char* str1, const char* str2 )
{
    return _stricoll_l(str1, str2, NULL);
}

/*********************************************************************
 *              _strncoll_l (MSVCRT.@)
 */
int CDECL _strncoll_l( const char* str1, const char* str2, size_t count, _locale_t locale )
{
    pthreadlocinfo locinfo;

    if(!locale)
        locinfo = get_locinfo();
    else
        locinfo = locale->locinfo;

    if(!locinfo->lc_handle[LC_COLLATE])
        return strncmp(str1, str2, count);
    return CompareStringA(locinfo->lc_handle[LC_COLLATE], 0,
              str1, strnlen(str1, count),
              str2, strnlen(str2, count))-CSTR_EQUAL;
}

/*********************************************************************
 *              _strncoll (MSVCRT.@)
 */
int CDECL _strncoll( const char* str1, const char* str2, size_t count )
{
    return _strncoll_l(str1, str2, count, NULL);
}

/*********************************************************************
 *              _strnicoll_l (MSVCRT.@)
 */
int CDECL _strnicoll_l( const char* str1, const char* str2, size_t count, _locale_t locale )
{
    pthreadlocinfo locinfo;

    if(!locale)
        locinfo = get_locinfo();
    else
        locinfo = locale->locinfo;

    if(!locinfo->lc_handle[LC_COLLATE])
        return _strnicmp(str1, str2, count);
    return CompareStringA(locinfo->lc_handle[LC_COLLATE], NORM_IGNORECASE,
            str1, strnlen(str1, count),
            str2, strnlen(str2, count))-CSTR_EQUAL;
}

/*********************************************************************
 *              _strnicoll (MSVCRT.@)
 */
int CDECL _strnicoll( const char* str1, const char* str2, size_t count )
{
    return _strnicoll_l(str1, str2, count, NULL);
}

/*********************************************************************
 *                  strncpy (MSVCRT.@)
 */
char* __cdecl strncpy(char *dst, const char *src, size_t len)
{
    size_t i;

    for(i=0; i<len; i++)
        if((dst[i] = src[i]) == '\0') break;

    while (i < len) dst[i++] = 0;

    return dst;
}

/*********************************************************************
 *      strcpy (MSVCRT.@)
 */
char* CDECL strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

/*********************************************************************
 *      strcpy_s (MSVCRT.@)
 */
int CDECL strcpy_s( char* dst, size_t elem, const char* src )
{
    size_t i;
    if(!elem) return EINVAL;
    if(!dst) return EINVAL;
    if(!src)
    {
        dst[0] = '\0';
        return EINVAL;
    }

    for(i = 0; i < elem; i++)
    {
        if((dst[i] = src[i]) == '\0') return 0;
    }
    dst[0] = '\0';
    return ERANGE;
}

/*********************************************************************
 *      strcat_s (MSVCRT.@)
 */
int CDECL strcat_s( char* dst, size_t elem, const char* src )
{
    size_t i, j;
    if(!dst) return EINVAL;
    if(elem == 0) return EINVAL;
    if(!src)
    {
        dst[0] = '\0';
        return EINVAL;
    }

    for(i = 0; i < elem; i++)
    {
        if(dst[i] == '\0')
        {
            for(j = 0; (j + i) < elem; j++)
            {
                if((dst[j + i] = src[j]) == '\0') return 0;
            }
        }
    }
    /* Set the first element to 0, not the first element after the skipped part */
    dst[0] = '\0';
    return ERANGE;
}

/*********************************************************************
 *      strcat (MSVCRT.@)
 */
char* __cdecl strcat( char *dst, const char *src )
{
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++));
    return dst;
}

/*********************************************************************
 *      strncat_s (MSVCRT.@)
 */
int CDECL strncat_s( char* dst, size_t elem, const char* src, size_t count )
{
    size_t i, j;

    if (!MSVCRT_CHECK_PMT(dst != 0)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(elem != 0)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(src != 0))
    {
        dst[0] = '\0';
        return EINVAL;
    }

    for(i = 0; i < elem; i++)
    {
        if(dst[i] == '\0')
        {
            for(j = 0; (j + i) < elem; j++)
            {
                if(count == _TRUNCATE && j + i == elem - 1)
                {
                    dst[j + i] = '\0';
                    return STRUNCATE;
                }
                if(j == count || (dst[j + i] = src[j]) == '\0')
                {
                    dst[j + i] = '\0';
                    return 0;
                }
            }
        }
    }
    /* Set the first element to 0, not the first element after the skipped part */
    dst[0] = '\0';
    return ERANGE;
}

/*********************************************************************
 *      strncat (MSVCRT.@)
 */
char* __cdecl strncat(char *dst, const char *src, size_t len)
{
    char *d = dst;
    while (*d) d++;
    for ( ; len && *src; d++, src++, len--) *d = *src;
    *d = 0;
    return dst;
}

/*********************************************************************
 *		_strxfrm_l (MSVCRT.@)
 */
size_t CDECL _strxfrm_l( char *dest, const char *src,
        size_t len, _locale_t locale )
{
    pthreadlocinfo locinfo;
    int ret;

    if(!MSVCRT_CHECK_PMT(src)) return INT_MAX;
    if(!MSVCRT_CHECK_PMT(dest || !len)) return INT_MAX;

    if(len > INT_MAX) {
        FIXME("len > INT_MAX not supported\n");
        len = INT_MAX;
    }

    if(!locale)
        locinfo = get_locinfo();
    else
        locinfo = locale->locinfo;

    if(!locinfo->lc_handle[LC_COLLATE]) {
        strncpy(dest, src, len);
        return strlen(src);
    }

    ret = LCMapStringA(locinfo->lc_handle[LC_COLLATE],
            LCMAP_SORTKEY, src, -1, NULL, 0);
    if(!ret) {
        if(len) dest[0] = 0;
        *_errno() = EILSEQ;
        return INT_MAX;
    }
    if(!len) return ret-1;

    if(ret > len) {
        dest[0] = 0;
        *_errno() = ERANGE;
        return ret-1;
    }

    return LCMapStringA(locinfo->lc_handle[LC_COLLATE],
            LCMAP_SORTKEY, src, -1, dest, len) - 1;
}

/*********************************************************************
 *		strxfrm (MSVCRT.@)
 */
size_t CDECL strxfrm( char *dest, const char *src, size_t len )
{
    return _strxfrm_l(dest, src, len, NULL);
}

/********************************************************************
 *		__STRINGTOLD_L (MSVCR80.@)
 */
int CDECL __STRINGTOLD_L( MSVCRT__LDOUBLE *value, char **endptr,
        const char *str, int flags, _locale_t locale )
{
    pthreadlocinfo locinfo;
    const char *beg, *p;
    int err, ret = 0;
    struct fpnum fp;

    if (flags) FIXME("flags not supported: %x\n", flags);

    if (!locale)
        locinfo = get_locinfo();
    else
        locinfo = locale->locinfo;

    p = str;
    while (_isspace_l((unsigned char)*p, locale))
        p++;
    beg = p;

    fp = fpnum_parse(strtod_str_get, strtod_str_unget, &p, locinfo, TRUE);
    if (endptr) *endptr = (p == beg ? (char*)str : (char*)p);
    if (p == beg) ret = 4;

    err = fpnum_ldouble(&fp, value);
    if (err) ret = (value->x80[2] & 0x7fff ? 2 : 1);
    return ret;
}

/********************************************************************
 *              __STRINGTOLD (MSVCRT.@)
 */
int CDECL __STRINGTOLD( MSVCRT__LDOUBLE *value, char **endptr, const char *str, int flags )
{
    return __STRINGTOLD_L( value, endptr, str, flags, NULL );
}

/********************************************************************
 *              _atoldbl_l (MSVCRT.@)
 */
int CDECL _atoldbl_l( MSVCRT__LDOUBLE *value, char *str, _locale_t locale )
{
    char *endptr;
    switch(__STRINGTOLD_L( value, &endptr, str, 0, locale ))
    {
    case 1: return _UNDERFLOW;
    case 2: return _OVERFLOW;
    default: return 0;
    }
}

/********************************************************************
 *		_atoldbl (MSVCRT.@)
 */
int CDECL _atoldbl(_LDOUBLE *value, char *str)
{
    return _atoldbl_l( (MSVCRT__LDOUBLE*)value, str, NULL );
}

/*********************************************************************
 *              strlen (MSVCRT.@)
 */
size_t __cdecl strlen(const char *str)
{
    const char *s = str;
    while (*s) s++;
    return s - str;
}

/******************************************************************
 *              strnlen (MSVCRT.@)
 */
size_t CDECL strnlen(const char *s, size_t maxlen)
{
    size_t i;

    for(i=0; i<maxlen; i++)
        if(!s[i]) break;

    return i;
}

/*********************************************************************
 *  _strtoi64_l (MSVCRT.@)
 *
 * FIXME: locale parameter is ignored
 */
__int64 CDECL _strtoi64_l(const char *nptr, char **endptr, int base, _locale_t locale)
{
    const char *p = nptr;
    BOOL negative = FALSE;
    BOOL got_digit = FALSE;
    __int64 ret = 0;

    TRACE("(%s %p %d %p)\n", debugstr_a(nptr), endptr, base, locale);

    if (!MSVCRT_CHECK_PMT(nptr != NULL)) return 0;
    if (!MSVCRT_CHECK_PMT(base == 0 || base >= 2)) return 0;
    if (!MSVCRT_CHECK_PMT(base <= 36)) return 0;

    while(_isspace_l((unsigned char)*nptr, locale)) nptr++;

    if(*nptr == '-') {
        negative = TRUE;
        nptr++;
    } else if(*nptr == '+')
        nptr++;

    if((base==0 || base==16) && *nptr=='0' && _tolower_l(*(nptr+1), locale)=='x') {
        base = 16;
        nptr += 2;
    }

    if(base == 0) {
        if(*nptr=='0')
            base = 8;
        else
            base = 10;
    }

    while(*nptr) {
        char cur = _tolower_l(*nptr, locale);
        int v;

        if(cur>='0' && cur<='9') {
            if(cur >= '0'+base)
                break;
            v = cur-'0';
        } else {
            if(cur<'a' || cur>='a'+base-10)
                break;
            v = cur-'a'+10;
        }
        got_digit = TRUE;

        if(negative)
            v = -v;

        nptr++;

        if(!negative && (ret>I64_MAX/base || ret*base>I64_MAX-v)) {
            ret = I64_MAX;
            *_errno() = ERANGE;
        } else if(negative && (ret<I64_MIN/base || ret*base<I64_MIN-v)) {
            ret = I64_MIN;
            *_errno() = ERANGE;
        } else
            ret = ret*base + v;
    }

    if(endptr)
        *endptr = (char*)(got_digit ? nptr : p);

    return ret;
}

/*********************************************************************
 *  _strtoi64 (MSVCRT.@)
 */
__int64 CDECL _strtoi64(const char *nptr, char **endptr, int base)
{
    return _strtoi64_l(nptr, endptr, base, NULL);
}

/*********************************************************************
 *  _atoi_l (MSVCRT.@)
 */
int __cdecl _atoi_l(const char *str, _locale_t locale)
{
    __int64 ret = _strtoi64_l(str, NULL, 10, locale);

    if(ret > INT_MAX) {
        ret = INT_MAX;
        *_errno() = ERANGE;
    } else if(ret < INT_MIN) {
        ret = INT_MIN;
        *_errno() = ERANGE;
    }
    return ret;
}

/*********************************************************************
 *  atoi (MSVCRT.@)
 */
#if _MSVCR_VER == 0
int __cdecl atoi(const char *str)
{
    BOOL minus = FALSE;
    int ret = 0;

    if(!str)
        return 0;

    while(_isspace_l((unsigned char)*str, NULL)) str++;

    if(*str == '+') {
        str++;
    }else if(*str == '-') {
        minus = TRUE;
        str++;
    }

    while(*str>='0' && *str<='9') {
        ret = ret*10+*str-'0';
        str++;
    }

    return minus ? -ret : ret;
}
#else
int CDECL atoi(const char *str)
{
    return _atoi_l(str, NULL);
}
#endif

/******************************************************************
 *      _atoi64_l (MSVCRT.@)
 */
__int64 CDECL _atoi64_l(const char *str, _locale_t locale)
{
    return _strtoi64_l(str, NULL, 10, locale);
}

/******************************************************************
 *      _atoi64 (MSVCRT.@)
 */
__int64 CDECL _atoi64(const char *str)
{
    return _strtoi64_l(str, NULL, 10, NULL);
}

/******************************************************************
 *      _atol_l (MSVCRT.@)
 */
__msvcrt_long CDECL _atol_l(const char *str, _locale_t locale)
{
    __int64 ret = _strtoi64_l(str, NULL, 10, locale);

    if(ret > LONG_MAX) {
        ret = LONG_MAX;
        *_errno() = ERANGE;
    } else if(ret < LONG_MIN) {
        ret = LONG_MIN;
        *_errno() = ERANGE;
    }
    return ret;
}

/******************************************************************
 *      atol (MSVCRT.@)
 */
__msvcrt_long CDECL atol(const char *str)
{
#if _MSVCR_VER == 0
    return atoi(str);
#else
    return _atol_l(str, NULL);
#endif
}

#if _MSVCR_VER>=120

/******************************************************************
 *      _atoll_l (MSVCR120.@)
 */
__int64 CDECL _atoll_l(const char* str, _locale_t locale)
{
    return _strtoi64_l(str, NULL, 10, locale);
}

/******************************************************************
 *      atoll (MSVCR120.@)
 */
__int64 CDECL atoll(const char* str)
{
    return _atoll_l(str, NULL);
}

#endif /* _MSVCR_VER>=120 */

/******************************************************************
 *		_strtol_l (MSVCRT.@)
 */
__msvcrt_long CDECL _strtol_l(const char* nptr,
        char** end, int base, _locale_t locale)
{
    __int64 ret = _strtoi64_l(nptr, end, base, locale);

    if(ret > LONG_MAX) {
        ret = LONG_MAX;
        *_errno() = ERANGE;
    } else if(ret < LONG_MIN) {
        ret = LONG_MIN;
        *_errno() = ERANGE;
    }

    return ret;
}

/******************************************************************
 *		strtol (MSVCRT.@)
 */
__msvcrt_long CDECL strtol(const char* nptr, char** end, int base)
{
    return _strtol_l(nptr, end, base, NULL);
}

/******************************************************************
 *		_strtoul_l (MSVCRT.@)
 */
__msvcrt_ulong CDECL _strtoul_l(const char* nptr, char** end, int base, _locale_t locale)
{
    __int64 ret = _strtoi64_l(nptr, end, base, locale);

    if(ret > ULONG_MAX) {
        ret = ULONG_MAX;
        *_errno() = ERANGE;
    }else if(ret < -(__int64)ULONG_MAX) {
        ret = 1;
        *_errno() = ERANGE;
    }

    return ret;
}

/******************************************************************
 *		strtoul (MSVCRT.@)
 */
__msvcrt_ulong CDECL strtoul(const char* nptr, char** end, int base)
{
    return _strtoul_l(nptr, end, base, NULL);
}

/*********************************************************************
 *  _strtoui64_l (MSVCRT.@)
 *
 * FIXME: locale parameter is ignored
 */
unsigned __int64 CDECL _strtoui64_l(const char *nptr, char **endptr, int base, _locale_t locale)
{
    const char *p = nptr;
    BOOL negative = FALSE;
    BOOL got_digit = FALSE;
    unsigned __int64 ret = 0;

    TRACE("(%s %p %d %p)\n", debugstr_a(nptr), endptr, base, locale);

    if (!MSVCRT_CHECK_PMT(nptr != NULL)) return 0;
    if (!MSVCRT_CHECK_PMT(base == 0 || base >= 2)) return 0;
    if (!MSVCRT_CHECK_PMT(base <= 36)) return 0;

    while(_isspace_l((unsigned char)*nptr, locale)) nptr++;

    if(*nptr == '-') {
        negative = TRUE;
        nptr++;
    } else if(*nptr == '+')
        nptr++;

    if((base==0 || base==16) && *nptr=='0' && _tolower_l(*(nptr+1), locale)=='x') {
        base = 16;
        nptr += 2;
    }

    if(base == 0) {
        if(*nptr=='0')
            base = 8;
        else
            base = 10;
    }

    while(*nptr) {
        char cur = _tolower_l(*nptr, locale);
        int v;

        if(cur>='0' && cur<='9') {
            if(cur >= '0'+base)
                break;
            v = *nptr-'0';
        } else {
            if(cur<'a' || cur>='a'+base-10)
                break;
            v = cur-'a'+10;
        }
        got_digit = TRUE;

        nptr++;

        if(ret>UI64_MAX/base || ret*base>UI64_MAX-v) {
            ret = UI64_MAX;
            *_errno() = ERANGE;
        } else
            ret = ret*base + v;
    }

    if(endptr)
        *endptr = (char*)(got_digit ? nptr : p);

    return negative ? -ret : ret;
}

/*********************************************************************
 *  _strtoui64 (MSVCRT.@)
 */
unsigned __int64 CDECL _strtoui64(const char *nptr, char **endptr, int base)
{
    return _strtoui64_l(nptr, endptr, base, NULL);
}

static int ltoa_helper(__msvcrt_long value, char *str, size_t size, int radix)
{
    __msvcrt_ulong val;
    unsigned int digit;
    BOOL is_negative;
    char buffer[33], *pos;
    size_t len;

    if (value < 0 && radix == 10)
    {
        is_negative = TRUE;
        val = -value;
    }
    else
    {
        is_negative = FALSE;
        val = value;
    }

    pos = buffer + 32;
    *pos = '\0';

    do
    {
        digit = val % radix;
        val /= radix;

        if (digit < 10)
            *--pos = '0' + digit;
        else
            *--pos = 'a' + digit - 10;
    }
    while (val != 0);

    if (is_negative)
        *--pos = '-';

    len = buffer + 33 - pos;
    if (len > size)
    {
        size_t i;
        char *p = str;

        /* Copy the temporary buffer backwards up to the available number of
         * characters. Don't copy the negative sign if present. */

        if (is_negative)
        {
            p++;
            size--;
        }

        for (pos = buffer + 31, i = 0; i < size; i++)
            *p++ = *pos--;

        str[0] = '\0';
        MSVCRT_INVALID_PMT("str[size] is too small", ERANGE);
        return ERANGE;
    }

    memcpy(str, pos, len);
    return 0;
}

/*********************************************************************
 *  _ltoa_s (MSVCRT.@)
 */
int CDECL _ltoa_s(__msvcrt_long value, char *str, size_t size, int radix)
{
    if (!MSVCRT_CHECK_PMT(str != NULL)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(size > 0)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(radix >= 2 && radix <= 36))
    {
        str[0] = '\0';
        return EINVAL;
    }

    return ltoa_helper(value, str, size, radix);
}

/*********************************************************************
 *  _ltow_s (MSVCRT.@)
 */
int CDECL _ltow_s(__msvcrt_long value, wchar_t *str, size_t size, int radix)
{
    __msvcrt_ulong val;
    unsigned int digit;
    BOOL is_negative;
    wchar_t buffer[33], *pos;
    size_t len;

    if (!MSVCRT_CHECK_PMT(str != NULL)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(size > 0)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(radix >= 2 && radix <= 36))
    {
        str[0] = '\0';
        return EINVAL;
    }

    if (value < 0 && radix == 10)
    {
        is_negative = TRUE;
        val = -value;
    }
    else
    {
        is_negative = FALSE;
        val = value;
    }

    pos = buffer + 32;
    *pos = '\0';

    do
    {
        digit = val % radix;
        val /= radix;

        if (digit < 10)
            *--pos = '0' + digit;
        else
            *--pos = 'a' + digit - 10;
    }
    while (val != 0);

    if (is_negative)
        *--pos = '-';

    len = buffer + 33 - pos;
    if (len > size)
    {
        size_t i;
        wchar_t *p = str;

        /* Copy the temporary buffer backwards up to the available number of
         * characters. Don't copy the negative sign if present. */

        if (is_negative)
        {
            p++;
            size--;
        }

        for (pos = buffer + 31, i = 0; i < size; i++)
            *p++ = *pos--;

        str[0] = '\0';
        MSVCRT_INVALID_PMT("str[size] is too small", ERANGE);
        return ERANGE;
    }

    memcpy(str, pos, len * sizeof(wchar_t));
    return 0;
}

/*********************************************************************
 *  _itoa_s (MSVCRT.@)
 */
int CDECL _itoa_s(int value, char *str, size_t size, int radix)
{
    return _ltoa_s(value, str, size, radix);
}

/*********************************************************************
 *  _itoa (MSVCRT.@)
 */
char* CDECL _itoa(int value, char *str, int radix)
{
    return ltoa_helper(value, str, SIZE_MAX, radix) ? NULL : str;
}

/*********************************************************************
 *  _itow_s (MSVCRT.@)
 */
int CDECL _itow_s(int value, wchar_t *str, size_t size, int radix)
{
    return _ltow_s(value, str, size, radix);
}

/*********************************************************************
 *  _ui64toa_s (MSVCRT.@)
 */
int CDECL _ui64toa_s(unsigned __int64 value, char *str,
        size_t size, int radix)
{
    char buffer[65], *pos;
    int digit;

    if (!MSVCRT_CHECK_PMT(str != NULL)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(size > 0)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(radix >= 2 && radix <= 36))
    {
        str[0] = '\0';
        return EINVAL;
    }

    pos = buffer+64;
    *pos = '\0';

    do {
        digit = value%radix;
        value /= radix;

        if(digit < 10)
            *--pos = '0'+digit;
        else
            *--pos = 'a'+digit-10;
    }while(value != 0);

    if(buffer-pos+65 > size) {
        MSVCRT_INVALID_PMT("str[size] is too small", EINVAL);
        return EINVAL;
    }

    memcpy(str, pos, buffer-pos+65);
    return 0;
}

/*********************************************************************
 *      _ui64tow_s  (MSVCRT.@)
 */
int CDECL _ui64tow_s( unsigned __int64 value, wchar_t *str,
                             size_t size, int radix )
{
    wchar_t buffer[65], *pos;
    int digit;

    if (!MSVCRT_CHECK_PMT(str != NULL)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(size > 0)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(radix >= 2 && radix <= 36))
    {
        str[0] = '\0';
        return EINVAL;
    }

    pos = &buffer[64];
    *pos = '\0';

    do {
	digit = value % radix;
	value = value / radix;
	if (digit < 10)
	    *--pos = '0' + digit;
	else
	    *--pos = 'a' + digit - 10;
    } while (value != 0);

    if(buffer-pos+65 > size) {
        MSVCRT_INVALID_PMT("str[size] is too small", EINVAL);
        return EINVAL;
    }

    memcpy(str, pos, (buffer-pos+65)*sizeof(wchar_t));
    return 0;
}

/*********************************************************************
 *  _ultoa_s (MSVCRT.@)
 */
int CDECL _ultoa_s(__msvcrt_ulong value, char *str, size_t size, int radix)
{
    __msvcrt_ulong digit;
    char buffer[33], *pos;
    size_t len;

    if (!str || !size || radix < 2 || radix > 36)
    {
        if (str && size)
            str[0] = '\0';

        *_errno() = EINVAL;
        return EINVAL;
    }

    pos = buffer + 32;
    *pos = '\0';

    do
    {
        digit = value % radix;
        value /= radix;

        if (digit < 10)
            *--pos = '0' + digit;
        else
            *--pos = 'a' + digit - 10;
    }
    while (value != 0);

    len = buffer + 33 - pos;
    if (len > size)
    {
        size_t i;
        char *p = str;

        /* Copy the temporary buffer backwards up to the available number of
         * characters. */

        for (pos = buffer + 31, i = 0; i < size; i++)
            *p++ = *pos--;

        str[0] = '\0';
        *_errno() = ERANGE;
        return ERANGE;
    }

    memcpy(str, pos, len);
    return 0;
}

/*********************************************************************
 *  _ultow_s (MSVCRT.@)
 */
int CDECL _ultow_s(__msvcrt_ulong value, wchar_t *str, size_t size, int radix)
{
    __msvcrt_ulong digit;
    WCHAR buffer[33], *pos;
    size_t len;

    if (!str || !size || radix < 2 || radix > 36)
    {
        if (str && size)
            str[0] = '\0';

        *_errno() = EINVAL;
        return EINVAL;
    }

    pos = buffer + 32;
    *pos = '\0';

    do
    {
        digit = value % radix;
        value /= radix;

        if (digit < 10)
            *--pos = '0' + digit;
        else
            *--pos = 'a' + digit - 10;
    }
    while (value != 0);

    len = buffer + 33 - pos;
    if (len > size)
    {
        size_t i;
        WCHAR *p = str;

        /* Copy the temporary buffer backwards up to the available number of
         * characters. */

        for (pos = buffer + 31, i = 0; i < size; i++)
            *p++ = *pos--;

        str[0] = '\0';
        *_errno() = ERANGE;
        return ERANGE;
    }

    memcpy(str, pos, len * sizeof(wchar_t));
    return 0;
}

/*********************************************************************
 *  _i64toa_s (MSVCRT.@)
 */
int CDECL _i64toa_s(__int64 value, char *str, size_t size, int radix)
{
    unsigned __int64 val;
    unsigned int digit;
    BOOL is_negative;
    char buffer[65], *pos;
    size_t len;

    if (!MSVCRT_CHECK_PMT(str != NULL)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(size > 0)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(radix >= 2 && radix <= 36))
    {
        str[0] = '\0';
        return EINVAL;
    }

    if (value < 0 && radix == 10)
    {
        is_negative = TRUE;
        val = -value;
    }
    else
    {
        is_negative = FALSE;
        val = value;
    }

    pos = buffer + 64;
    *pos = '\0';

    do
    {
        digit = val % radix;
        val /= radix;

        if (digit < 10)
            *--pos = '0' + digit;
        else
            *--pos = 'a' + digit - 10;
    }
    while (val != 0);

    if (is_negative)
        *--pos = '-';

    len = buffer + 65 - pos;
    if (len > size)
    {
        size_t i;
        char *p = str;

        /* Copy the temporary buffer backwards up to the available number of
         * characters. Don't copy the negative sign if present. */

        if (is_negative)
        {
            p++;
            size--;
        }

        for (pos = buffer + 63, i = 0; i < size; i++)
            *p++ = *pos--;

        str[0] = '\0';
        MSVCRT_INVALID_PMT("str[size] is too small", ERANGE);
        return ERANGE;
    }

    memcpy(str, pos, len);
    return 0;
}

/*********************************************************************
 *  _i64tow_s (MSVCRT.@)
 */
int CDECL _i64tow_s(__int64 value, wchar_t *str, size_t size, int radix)
{
    unsigned __int64 val;
    unsigned int digit;
    BOOL is_negative;
    wchar_t buffer[65], *pos;
    size_t len;

    if (!MSVCRT_CHECK_PMT(str != NULL)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(size > 0)) return EINVAL;
    if (!MSVCRT_CHECK_PMT(radix >= 2 && radix <= 36))
    {
        str[0] = '\0';
        return EINVAL;
    }

    if (value < 0 && radix == 10)
    {
        is_negative = TRUE;
        val = -value;
    }
    else
    {
        is_negative = FALSE;
        val = value;
    }

    pos = buffer + 64;
    *pos = '\0';

    do
    {
        digit = val % radix;
        val /= radix;

        if (digit < 10)
            *--pos = '0' + digit;
        else
            *--pos = 'a' + digit - 10;
    }
    while (val != 0);

    if (is_negative)
        *--pos = '-';

    len = buffer + 65 - pos;
    if (len > size)
    {
        size_t i;
        wchar_t *p = str;

        /* Copy the temporary buffer backwards up to the available number of
         * characters. Don't copy the negative sign if present. */

        if (is_negative)
        {
            p++;
            size--;
        }

        for (pos = buffer + 63, i = 0; i < size; i++)
            *p++ = *pos--;

        str[0] = '\0';
        MSVCRT_INVALID_PMT("str[size] is too small", ERANGE);
        return ERANGE;
    }

    memcpy(str, pos, len * sizeof(wchar_t));
    return 0;
}

#define I10_OUTPUT_MAX_PREC 21
/* Internal structure used by $I10_OUTPUT */
struct _I10_OUTPUT_DATA {
    short pos;
    char sign;
    BYTE len;
    char str[I10_OUTPUT_MAX_PREC+1]; /* add space for '\0' */
};

/*********************************************************************
 *              $I10_OUTPUT (MSVCRT.@)
 * ld80 - long double (Intel 80 bit FP in 12 bytes) to be printed to data
 * prec - precision of part, we're interested in
 * flag - 0 for first prec digits, 1 for fractional part
 * data - data to be populated
 *
 * return value
 *      0 if given double is NaN or INF
 *      1 otherwise
 *
 * FIXME
 *      Native sets last byte of data->str to '0' or '9', I don't know what
 *      it means. Current implementation sets it always to '0'.
 */
int CDECL I10_OUTPUT(MSVCRT__LDOUBLE ld80, int prec, int flag, struct _I10_OUTPUT_DATA *data)
{
    struct fpnum num;
    double d;
    char format[8];
    char buf[I10_OUTPUT_MAX_PREC+9]; /* 9 = strlen("0.e+0000") + '\0' */
    char *p;

    if ((ld80.x80[2] & 0x7fff) == 0x7fff)
    {
        if (ld80.x80[0] == 0 && ld80.x80[1] == 0x80000000)
            strcpy( data->str, "1#INF" );
        else
            strcpy( data->str, (ld80.x80[1] & 0x40000000) ? "1#QNAN" : "1#SNAN" );
        data->pos = 1;
        data->sign = (ld80.x80[2] & 0x8000) ? '-' : ' ';
        data->len = strlen(data->str);
        return 0;
    }

    num.sign = (ld80.x80[2] & 0x8000) ? -1 : 1;
    num.exp  = (ld80.x80[2] & 0x7fff) - 0x3fff - 63;
    num.m    = ld80.x80[0] | ((ULONGLONG)ld80.x80[1] << 32);
    num.mod  = FP_ROUND_EVEN;
    fpnum_double( &num, &d );
    TRACE("(%lf %d %x %p)\n", d, prec, flag, data);

    if(d<0) {
        data->sign = '-';
        d = -d;
    } else
        data->sign = ' ';

    if(flag&1) {
        int exp = 1 + floor(log10(d));

        prec += exp;
        if(exp < 0)
            prec--;
    }
    prec--;

    if(prec+1 > I10_OUTPUT_MAX_PREC)
        prec = I10_OUTPUT_MAX_PREC-1;
    else if(prec < 0) {
        d = 0.0;
        prec = 0;
    }

    sprintf(format, "%%.%dle", prec);
    sprintf(buf, format, d);

    buf[1] = buf[0];
    data->pos = atoi(buf+prec+3);
    if(buf[1] != '0')
        data->pos++;

    for(p = buf+prec+1; p>buf+1 && *p=='0'; p--);
    data->len = p-buf;

    memcpy(data->str, buf+1, data->len);
    data->str[data->len] = '\0';

    if(buf[1]!='0' && prec-data->len+1>0)
        memcpy(data->str+data->len+1, buf+data->len+1, prec-data->len+1);

    return 1;
}
#undef I10_OUTPUT_MAX_PREC

/*********************************************************************
 *                  memcmp (MSVCRT.@)
 */
int __cdecl memcmp(const void *ptr1, const void *ptr2, size_t n)
{
    const unsigned char *p1, *p2;

    for (p1 = ptr1, p2 = ptr2; n; n--, p1++, p2++)
    {
        if (*p1 < *p2) return -1;
        if (*p1 > *p2) return 1;
    }
    return 0;
}

extern BOOL sse2_supported;
extern BOOL avx_supported;

#define likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)

#define MEMMOVEV_UNALIGNED_DECLARE(name, type, size, loadu, storeu) \
static FORCEINLINE void memmove_ ## name ## _unaligned(char *d, const char *s, size_t n) \
{ \
    type tmp0, tmp1, tmp2, tmp3, tmp4, tmp5; \
    if (unlikely(n > 4 * size)) \
    { \
        tmp0 = loadu((type *)(s + 0 * size)); \
        tmp1 = loadu((type *)(s + 1 * size)); \
        tmp2 = loadu((type *)(s + 2 * size)); \
        tmp3 = loadu((type *)(s + n - 3 * size)); \
        tmp4 = loadu((type *)(s + n - 2 * size)); \
        tmp5 = loadu((type *)(s + n - 1 * size)); \
        storeu((type *)(d + 0 * size), tmp0); \
        storeu((type *)(d + 1 * size), tmp1); \
        storeu((type *)(d + 2 * size), tmp2); \
        storeu((type *)(d + n - 3 * size), tmp3); \
        storeu((type *)(d + n - 2 * size), tmp4); \
        storeu((type *)(d + n - 1 * size), tmp5); \
    } \
    else if (unlikely(n > size * 2)) \
    { \
        tmp0 = loadu((type *)(s + 0 * size)); \
        tmp1 = loadu((type *)(s + 1 * size)); \
        tmp2 = loadu((type *)(s + n - 2 * size)); \
        tmp3 = loadu((type *)(s + n - 1 * size)); \
        storeu((type *)(d + 0 * size), tmp0); \
        storeu((type *)(d + 1 * size), tmp1); \
        storeu((type *)(d + n - 2 * size), tmp2); \
        storeu((type *)(d + n - 1 * size), tmp3); \
    } \
    else if (unlikely(n > size)) \
    { \
        tmp0 = loadu((type *)(s + 0 * size)); \
        tmp1 = loadu((type *)(s + n - 1 * size)); \
        storeu((type *)(d + 0 * size), tmp0); \
        storeu((type *)(d + n - 1 * size), tmp1); \
    } \
    else memmove_c_unaligned_32(d, s, n); \
}

#define MEMMOVEV_DECLARE(name, type, size, loadu, storeu, store) \
static void *__cdecl memmove_ ## name(char *d, const char *s, size_t n) \
{ \
    if (likely(n <= 6 * size)) memmove_ ## name ## _unaligned(d, s, n); \
    else if (d <= s) \
    { \
        type tmp0, tmp1, tmp2, tmp3, tmp4, tmp5; \
        size_t k = (size - ((uintptr_t)d & (size - 1))); \
        tmp0 = loadu((type *)s); \
        tmp1 = loadu((type *)(s + k + 0 * size)); \
        tmp2 = loadu((type *)(s + k + 1 * size)); \
        tmp3 = loadu((type *)(s + k + 2 * size)); \
        tmp4 = loadu((type *)(s + k + 3 * size)); \
        tmp5 = loadu((type *)(s + k + 4 * size)); \
        storeu((type *)d, tmp0); \
        store((type *)(d + k + 0 * size), tmp1); \
        store((type *)(d + k + 1 * size), tmp2); \
        store((type *)(d + k + 2 * size), tmp3); \
        store((type *)(d + k + 3 * size), tmp4); \
        store((type *)(d + k + 4 * size), tmp5); \
        k += 5 * size; d += k; s += k; n -= k; \
        while (unlikely(n >= 12 * size)) \
        { \
            tmp0 = loadu((type *)(s + 0 * size)); \
            tmp1 = loadu((type *)(s + 1 * size)); \
            tmp2 = loadu((type *)(s + 2 * size)); \
            tmp3 = loadu((type *)(s + 3 * size)); \
            tmp4 = loadu((type *)(s + 4 * size)); \
            tmp5 = loadu((type *)(s + 5 * size)); \
            store((type *)(d + 0 * size), tmp0); \
            store((type *)(d + 1 * size), tmp1); \
            store((type *)(d + 2 * size), tmp2); \
            store((type *)(d + 3 * size), tmp3); \
            store((type *)(d + 4 * size), tmp4); \
            store((type *)(d + 5 * size), tmp5); \
            tmp0 = loadu((type *)(s +  6 * size)); \
            tmp1 = loadu((type *)(s +  7 * size)); \
            tmp2 = loadu((type *)(s +  8 * size)); \
            tmp3 = loadu((type *)(s +  9 * size)); \
            tmp4 = loadu((type *)(s + 10 * size)); \
            tmp5 = loadu((type *)(s + 11 * size)); \
            store((type *)(d +  6 * size), tmp0); \
            store((type *)(d +  7 * size), tmp1); \
            store((type *)(d +  8 * size), tmp2); \
            store((type *)(d +  9 * size), tmp3); \
            store((type *)(d + 10 * size), tmp4); \
            store((type *)(d + 11 * size), tmp5); \
            d += 12 * size; s += 12 * size; n -= 12 * size; k += 12 * size; \
        } \
        while (unlikely(n >= 6 * size)) \
        { \
            tmp0 = loadu((type *)(s + 0 * size)); \
            tmp1 = loadu((type *)(s + 1 * size)); \
            tmp2 = loadu((type *)(s + 2 * size)); \
            tmp3 = loadu((type *)(s + 3 * size)); \
            tmp4 = loadu((type *)(s + 4 * size)); \
            tmp5 = loadu((type *)(s + 5 * size)); \
            store((type *)(d + 0 * size), tmp0); \
            store((type *)(d + 1 * size), tmp1); \
            store((type *)(d + 2 * size), tmp2); \
            store((type *)(d + 3 * size), tmp3); \
            store((type *)(d + 4 * size), tmp4); \
            store((type *)(d + 5 * size), tmp5); \
            d += 6 * size; s += 6 * size; n -= 6 * size; k += 6 * size; \
        } \
        memmove_ ## name ## _unaligned(d, s, n); \
        return d - k; \
    } \
    else \
    { \
        type tmp0, tmp1, tmp2, tmp3, tmp4, tmp5; \
        size_t k = n - ((uintptr_t)(d + n) & (size - 1)); \
        tmp0 = loadu((type *)(s + n - 1 * size)); \
        tmp1 = loadu((type *)(s + k - 1 * size)); \
        tmp2 = loadu((type *)(s + k - 2 * size)); \
        tmp3 = loadu((type *)(s + k - 3 * size)); \
        tmp4 = loadu((type *)(s + k - 4 * size)); \
        tmp5 = loadu((type *)(s + k - 5 * size)); \
        storeu((type *)(d + n - 1 * size), tmp0); \
        store((type *)(d + k - 1 * size), tmp1); \
        store((type *)(d + k - 2 * size), tmp2); \
        store((type *)(d + k - 3 * size), tmp3); \
        store((type *)(d + k - 4 * size), tmp4); \
        store((type *)(d + k - 5 * size), tmp5); \
        k -= 5 * size; \
        while (unlikely(k >= 12 * size)) \
        { \
            tmp0 = loadu((type *)(s + k - 1 * size)); \
            tmp1 = loadu((type *)(s + k - 2 * size)); \
            tmp2 = loadu((type *)(s + k - 3 * size)); \
            tmp3 = loadu((type *)(s + k - 4 * size)); \
            tmp4 = loadu((type *)(s + k - 5 * size)); \
            tmp5 = loadu((type *)(s + k - 6 * size)); \
            store((type *)(d + k - 1 * size), tmp0); \
            store((type *)(d + k - 2 * size), tmp1); \
            store((type *)(d + k - 3 * size), tmp2); \
            store((type *)(d + k - 4 * size), tmp3); \
            store((type *)(d + k - 5 * size), tmp4); \
            store((type *)(d + k - 6 * size), tmp5); \
            tmp0 = loadu((type *)(s + k -  7 * size)); \
            tmp1 = loadu((type *)(s + k -  8 * size)); \
            tmp2 = loadu((type *)(s + k -  9 * size)); \
            tmp3 = loadu((type *)(s + k - 10 * size)); \
            tmp4 = loadu((type *)(s + k - 11 * size)); \
            tmp5 = loadu((type *)(s + k - 12 * size)); \
            store((type *)(d + k -  7 * size), tmp0); \
            store((type *)(d + k -  8 * size), tmp1); \
            store((type *)(d + k -  9 * size), tmp2); \
            store((type *)(d + k - 10 * size), tmp3); \
            store((type *)(d + k - 11 * size), tmp4); \
            store((type *)(d + k - 12 * size), tmp5); \
            k -= 12 * size; \
        } \
        while (unlikely(k >= 6 * size)) \
        { \
            tmp0 = loadu((type *)(s + k - 1 * size)); \
            tmp1 = loadu((type *)(s + k - 2 * size)); \
            tmp2 = loadu((type *)(s + k - 3 * size)); \
            tmp3 = loadu((type *)(s + k - 4 * size)); \
            tmp4 = loadu((type *)(s + k - 5 * size)); \
            tmp5 = loadu((type *)(s + k - 6 * size)); \
            store((type *)(d + k - 1 * size), tmp0); \
            store((type *)(d + k - 2 * size), tmp1); \
            store((type *)(d + k - 3 * size), tmp2); \
            store((type *)(d + k - 4 * size), tmp3); \
            store((type *)(d + k - 5 * size), tmp4); \
            store((type *)(d + k - 6 * size), tmp5); \
            k -= 6 * size; \
        } \
        memmove_ ## name ## _unaligned(d, s, k); \
    } \
    return d; \
}

#define MEMSETV_UNALIGNED_DECLARE(name, type, size, cast64, storeu) \
static FORCEINLINE void memset_ ## name ## _unaligned(char *d, type v, size_t n) \
{ \
    if (unlikely(n > 4 * size)) \
    { \
        storeu((type *)(d + 0 * size), v); \
        storeu((type *)(d + 1 * size), v); \
        storeu((type *)(d + 2 * size), v); \
        storeu((type *)(d + n - 3 * size), v); \
        storeu((type *)(d + n - 2 * size), v); \
        storeu((type *)(d + n - 1 * size), v); \
    } \
    else if (unlikely(n > 2 * size)) \
    { \
        storeu((type *)(d + 0 * size), v); \
        storeu((type *)(d + 1 * size), v); \
        storeu((type *)(d + n - 2 * size), v); \
        storeu((type *)(d + n - 1 * size), v); \
    } \
    else if (unlikely(n > size)) \
    { \
        storeu((type *)(d + 0 * size), v); \
        storeu((type *)(d + n - 1 * size), v); \
    } \
    else memset_c_unaligned_32(d, cast64(v), n); \
}

#define MEMSETV_DECLARE(name, type, size, bcast, storeu, store) \
static void *__cdecl memset_ ## name(char *d, int c, size_t n) \
{ \
    type v = bcast(c); \
    if (likely(n <= 6 * size)) memset_ ## name ## _unaligned(d, v, n); \
    else \
    { \
        size_t k = n - ((uintptr_t)(d + n) & (size - 1)); \
        storeu((type *)(d + n - 1 * size), v); \
        store((type *)(d + k - 1 * size), v); \
        store((type *)(d + k - 2 * size), v); \
        store((type *)(d + k - 3 * size), v); \
        store((type *)(d + k - 4 * size), v); \
        store((type *)(d + k - 5 * size), v); \
        k -= 5 * size; \
        while (unlikely(k >= 12 * size)) \
        { \
            store((type *)(d + k -  1 * size), v); \
            store((type *)(d + k -  2 * size), v); \
            store((type *)(d + k -  3 * size), v); \
            store((type *)(d + k -  4 * size), v); \
            store((type *)(d + k -  5 * size), v); \
            store((type *)(d + k -  6 * size), v); \
            store((type *)(d + k -  7 * size), v); \
            store((type *)(d + k -  8 * size), v); \
            store((type *)(d + k -  9 * size), v); \
            store((type *)(d + k - 10 * size), v); \
            store((type *)(d + k - 11 * size), v); \
            store((type *)(d + k - 12 * size), v); \
            k -= 12 * size; \
        } \
        while (unlikely(k >= 6 * size)) \
        { \
            store((type *)(d + k - 1 * size), v); \
            store((type *)(d + k - 2 * size), v); \
            store((type *)(d + k - 3 * size), v); \
            store((type *)(d + k - 4 * size), v); \
            store((type *)(d + k - 5 * size), v); \
            store((type *)(d + k - 6 * size), v); \
            k -= 6 * size; \
        } \
        memset_ ## name ## _unaligned(d, v, k); \
    } \
    return d; \
}

static FORCEINLINE void __cdecl memmove_c_unaligned_32(char *d, const char *s, size_t n)
{
    uint64_t tmp0, tmp1, tmp2, tmpn;

    if (unlikely(n >= 24))
    {
        tmp0 = *(uint64_t *)s;
        tmp1 = *(uint64_t *)(s + 8);
        tmp2 = *(uint64_t *)(s + 16);
        tmpn = *(uint64_t *)(s + n - 8);
        *(uint64_t *)d = tmp0;
        *(uint64_t *)(d + 8) = tmp1;
        *(uint64_t *)(d + 16) = tmp2;
        *(uint64_t *)(d + n - 8) = tmpn;
    }
    else if (unlikely(n >= 16))
    {
        tmp0 = *(uint64_t *)s;
        tmp1 = *(uint64_t *)(s + 8);
        tmpn = *(uint64_t *)(s + n - 8);
        *(uint64_t *)d = tmp0;
        *(uint64_t *)(d + 8) = tmp1;
        *(uint64_t *)(d + n - 8) = tmpn;
    }
    else if (unlikely(n >= 8))
    {
        tmp0 = *(uint64_t *)s;
        tmpn = *(uint64_t *)(s + n - 8);
        *(uint64_t *)d = tmp0;
        *(uint64_t *)(d + n - 8) = tmpn;
    }
    else if (unlikely(n >= 4))
    {
        tmp0 = *(uint32_t *)s;
        tmpn = *(uint32_t *)(s + n - 4);
        *(uint32_t *)d = tmp0;
        *(uint32_t *)(d + n - 4) = tmpn;
    }
    else if (unlikely(n >= 2))
    {
        tmp0 = *(uint16_t *)s;
        tmpn = *(uint16_t *)(s + n - 2);
        *(uint16_t *)d = tmp0;
        *(uint16_t *)(d + n - 2) = tmpn;
    }
    else if (likely(n >= 1))
    {
        *(uint8_t *)d = *(uint8_t *)s;
    }
}

static void *__cdecl memmove_c(char *d, const char *s, size_t n)
{
    if (likely(n <= 32)) memmove_c_unaligned_32(d, s, n);
    else if (d <= s)
    {
        uint64_t tmp0, tmp1, tmp2;
        size_t k = 0;
        while (unlikely(n >= 48))
        {
            tmp0 = *(uint64_t *)(s +  0);
            tmp1 = *(uint64_t *)(s +  8);
            tmp2 = *(uint64_t *)(s + 16);
            *(uint64_t*)(d +  0) = tmp0;
            *(uint64_t*)(d +  8) = tmp1;
            *(uint64_t*)(d + 16) = tmp2;
            tmp0 = *(uint64_t *)(s + 24);
            tmp1 = *(uint64_t *)(s + 32);
            tmp2 = *(uint64_t *)(s + 40);
            *(uint64_t*)(d + 24) = tmp0;
            *(uint64_t*)(d + 32) = tmp1;
            *(uint64_t*)(d + 40) = tmp2;
            d += 48; s += 48; n -= 48; k += 48;
        }
        while (unlikely(n >= 24))
        {
            tmp0 = *(uint64_t *)(s +  0);
            tmp1 = *(uint64_t *)(s +  8);
            tmp2 = *(uint64_t *)(s + 16);
            *(uint64_t*)(d +  0) = tmp0;
            *(uint64_t*)(d +  8) = tmp1;
            *(uint64_t*)(d + 16) = tmp2;
            d += 24; s += 24; n -= 24; k += 24;
        }
        memmove_c_unaligned_32(d, s, n);
        return d - k;
    }
    else
    {
        uint64_t tmp0, tmp1, tmp2;
        size_t k = n;
        while (unlikely(k >= 48))
        {
            tmp0 = *(uint64_t *)(s + k -  8);
            tmp1 = *(uint64_t *)(s + k - 16);
            tmp2 = *(uint64_t *)(s + k - 24);
            *(uint64_t*)(d + k -  8) = tmp0;
            *(uint64_t*)(d + k - 16) = tmp1;
            *(uint64_t*)(d + k - 24) = tmp2;
            tmp0 = *(uint64_t *)(s + k - 32);
            tmp1 = *(uint64_t *)(s + k - 40);
            tmp2 = *(uint64_t *)(s + k - 48);
            *(uint64_t*)(d + k - 32) = tmp0;
            *(uint64_t*)(d + k - 40) = tmp1;
            *(uint64_t*)(d + k - 48) = tmp2;
            k -= 48;
        }
        while (unlikely(k >= 24))
        {
            tmp0 = *(uint64_t *)(s + k -  8);
            tmp1 = *(uint64_t *)(s + k - 16);
            tmp2 = *(uint64_t *)(s + k - 24);
            *(uint64_t*)(d + k -  8) = tmp0;
            *(uint64_t*)(d + k - 16) = tmp1;
            *(uint64_t*)(d + k - 24) = tmp2;
            k -= 24;
        }
        memmove_c_unaligned_32(d, s, k);
    }
    return d;
}

static FORCEINLINE void __cdecl memset_c_unaligned_32(char *d, uint64_t v, size_t n)
{
    if (unlikely(n >= 24))
    {
        *(uint64_t *)d = v;
        *(uint64_t *)(d + 8) = v;
        *(uint64_t *)(d + 16) = v;
        *(uint64_t *)(d + n - 8) = v;
    }
    else if (unlikely(n >= 16))
    {
        *(uint64_t *)d = v;
        *(uint64_t *)(d + 8) = v;
        *(uint64_t *)(d + n - 8) = v;
    }
    else if (unlikely(n >= 8))
    {
        *(uint64_t *)d = v;
        *(uint64_t *)(d + n - 8) = v;
    }
    else if (unlikely(n >= 4))
    {
        *(uint32_t *)d = v;
        *(uint32_t *)(d + n - 4) = v;
    }
    else if (unlikely(n >= 2))
    {
        *(uint16_t *)d = v;
        *(uint16_t *)(d + n - 2) = v;
    }
    else if (likely(n >= 1))
    {
        *(uint8_t *)d = v;
    }
}

static void *__cdecl memset_c(char *d, unsigned char c, size_t n)
{
    uint16_t tmp16 = ((uint16_t)c << 8) | c;
    uint32_t tmp32 = ((uint32_t)tmp16 << 16) | tmp16;
    uint64_t v = ((uint64_t)tmp32 << 32) | tmp32;

    while (unlikely(n >= 48))
    {
        *(uint64_t*)(d + n -  8) = v;
        *(uint64_t*)(d + n - 16) = v;
        *(uint64_t*)(d + n - 24) = v;
        *(uint64_t*)(d + n - 32) = v;
        *(uint64_t*)(d + n - 40) = v;
        *(uint64_t*)(d + n - 48) = v;
        n -= 48;
    }
    while (unlikely(n >= 24))
    {
        *(uint64_t*)(d + n -  8) = v;
        *(uint64_t*)(d + n - 16) = v;
        *(uint64_t*)(d + n - 24) = v;
        n -= 24;
    }

    memset_c_unaligned_32(d, v, n);
    return d;
}

#ifndef __SSE2__
#ifdef __clang__
#pragma clang attribute push (__attribute__((target("sse2"))), apply_to=function)
#else
#pragma GCC push_options
#pragma GCC target("sse2")
#endif
#define __DISABLE_SSE2__
#endif /* __SSE2__ */

MEMMOVEV_UNALIGNED_DECLARE(sse2, __m128i, 16, _mm_loadu_si128, _mm_storeu_si128)
MEMMOVEV_DECLARE(sse2, __m128i, 16, _mm_loadu_si128, _mm_storeu_si128, _mm_store_si128)

#define __m128i_to_u64(x) ((x)[0])
MEMSETV_UNALIGNED_DECLARE(sse2, __m128i, 16, __m128i_to_u64, _mm_storeu_si128)
MEMSETV_DECLARE(sse2, __m128i, 16, _mm_set1_epi8, _mm_storeu_si128, _mm_store_si128)
#undef __m128i_to_u64

#ifdef __DISABLE_SSE2__
#ifdef __clang__
#pragma clang attribute pop
#else
#pragma GCC pop_options
#endif
#undef __DISABLE_SSE2__
#endif /* __DISABLE_SSE2__ */

#ifndef __AVX__
#ifdef __clang__
#pragma clang attribute push (__attribute__((target("avx"))), apply_to=function)
#else
#pragma GCC push_options
#pragma GCC target("avx")
#endif
#define __DISABLE_AVX__
#endif /* __AVX__ */

MEMMOVEV_UNALIGNED_DECLARE(avx, __m256i, 32, _mm256_loadu_si256, _mm256_storeu_si256)
MEMMOVEV_DECLARE(avx, __m256i, 32, _mm256_loadu_si256, _mm256_storeu_si256, _mm256_store_si256)

#define __m256i_to_u64(x) (_mm256_castsi256_si128(x)[0])
MEMSETV_UNALIGNED_DECLARE(avx, __m256i, 32, __m256i_to_u64, _mm256_storeu_si256)
MEMSETV_DECLARE(avx, __m256i, 32, _mm256_set1_epi8, _mm256_storeu_si256, _mm256_store_si256)
#undef __m256i_to_u64

#ifdef __DISABLE_AVX__
#undef __DISABLE_AVX__
#ifdef __clang__
#pragma clang attribute pop
#else
#pragma GCC pop_options
#endif
#endif /* __DISABLE_AVX__ */

/*********************************************************************
 *                  memmove (MSVCRT.@)
 */
void *__cdecl memmove(void *dst, const void *src, size_t n)
{
    if (unlikely(n < 32)) { memmove_c_unaligned_32(dst, src, n); return dst; }
    if (likely(avx_supported)) return memmove_avx(dst, src, n);
    if (likely(sse2_supported)) return memmove_sse2(dst, src, n);
    return memmove_c(dst, src, n);
}

/*********************************************************************
 *                  memcpy   (MSVCRT.@)
 */
void *__cdecl memcpy(void *dst, const void *src, size_t n)
{
    if (unlikely(n < 32)) { memmove_c_unaligned_32(dst, src, n); return dst; }
    if (likely(avx_supported)) return memmove_avx(dst, src, n);
    if (likely(sse2_supported)) return memmove_sse2(dst, src, n);
    return memmove_c(dst, src, n);
}

/*********************************************************************
 *		    memset (MSVCRT.@)
 */
void* __cdecl memset(void *dst, int c, size_t n)
{
    if (unlikely(n <= 32))
    {
        uint16_t tmp16 = ((uint16_t)c << 8) | c;
        uint32_t tmp32 = ((uint32_t)tmp16 << 16) | tmp16;
        uint64_t v = ((uint64_t)tmp32 << 32) | tmp32;
        memset_c_unaligned_32(dst, v, n);
        return dst;
    }
    if (likely(avx_supported)) return memset_avx(dst, c, n);
    if (likely(sse2_supported)) return memset_sse2(dst, c, n);
    return memset_c(dst, c, n);
}

#undef MEMMOVEV_DECLARE
#undef MEMMOVEV_UNALIGNED_DECLARE
#undef MEMSETV_DECLARE
#undef MEMSETV_UNALIGNED_DECLARE
#undef likely
#undef unlikely

/*********************************************************************
 *		    strchr (MSVCRT.@)
 */
char* __cdecl strchr(const char *str, int c)
{
    do
    {
        if (*str == (char)c) return (char*)str;
    } while (*str++);
    return NULL;
}

/*********************************************************************
 *                  strrchr (MSVCRT.@)
 */
char* __cdecl strrchr(const char *str, int c)
{
    char *ret = NULL;
    do { if (*str == (char)c) ret = (char*)str; } while (*str++);
    return ret;
}

/*********************************************************************
 *                  memchr   (MSVCRT.@)
 */
void* __cdecl memchr(const void *ptr, int c, size_t n)
{
    const unsigned char *p = ptr;

    for (p = ptr; n; n--, p++) if (*p == (unsigned char)c) return (void *)(ULONG_PTR)p;
    return NULL;
}

/*********************************************************************
 *                  strcmp (MSVCRT.@)
 */
int __cdecl strcmp(const char *str1, const char *str2)
{
    while (*str1 && *str1 == *str2) { str1++; str2++; }
    if ((unsigned char)*str1 > (unsigned char)*str2) return 1;
    if ((unsigned char)*str1 < (unsigned char)*str2) return -1;
    return 0;
}

/*********************************************************************
 *                  strncmp   (MSVCRT.@)
 */
int __cdecl strncmp(const char *str1, const char *str2, size_t len)
{
    if (!len) return 0;
    while (--len && *str1 && *str1 == *str2) { str1++; str2++; }
    return (unsigned char)*str1 - (unsigned char)*str2;
}

/*********************************************************************
 *                  _strnicmp_l   (MSVCRT.@)
 */
int __cdecl _strnicmp_l(const char *s1, const char *s2,
        size_t count, _locale_t locale)
{
    pthreadlocinfo locinfo;
    int c1, c2;

    if(s1==NULL || s2==NULL)
        return _NLSCMPERROR;

    if(!count)
        return 0;

    if(!locale)
        locinfo = get_locinfo();
    else
        locinfo = locale->locinfo;

    if(!locinfo->lc_handle[LC_CTYPE])
    {
        do {
            if ((c1 = *s1++) >= 'A' && c1 <= 'Z')
                c1 -= 'A' - 'a';
            if ((c2 = *s2++) >= 'A' && c2 <= 'Z')
                c2 -= 'A' - 'a';
        }while(--count && c1 && c1==c2);

        return c1-c2;
    }

    do {
        c1 = _tolower_l((unsigned char)*s1++, locale);
        c2 = _tolower_l((unsigned char)*s2++, locale);
    }while(--count && c1 && c1==c2);

    return c1-c2;
}

/*********************************************************************
 *                  _stricmp_l   (MSVCRT.@)
 */
int __cdecl _stricmp_l(const char *s1, const char *s2, _locale_t locale)
{
    return _strnicmp_l(s1, s2, -1, locale);
}

/*********************************************************************
 *                  _strnicmp   (MSVCRT.@)
 */
int __cdecl _strnicmp(const char *s1, const char *s2, size_t count)
{
    return _strnicmp_l(s1, s2, count, NULL);
}

/*********************************************************************
 *                  _stricmp   (MSVCRT.@)
 */
int __cdecl _stricmp(const char *s1, const char *s2)
{
    return _strnicmp_l(s1, s2, -1, NULL);
}

/*********************************************************************
 *                  strstr   (MSVCRT.@)
 */
char* __cdecl strstr(const char *haystack, const char *needle)
{
    size_t i, j, len, needle_len, lps_len;
    BYTE lps[256];

    needle_len = strlen(needle);
    if (!needle_len) return (char*)haystack;
    lps_len = needle_len > ARRAY_SIZE(lps) ? ARRAY_SIZE(lps) : needle_len;

    lps[0] = 0;
    len = 0;
    i = 1;
    while (i < lps_len)
    {
        if (needle[i] == needle[len]) lps[i++] = ++len;
        else if (len) len = lps[len-1];
        else lps[i++] = 0;
    }

    i = j = 0;
    while (haystack[i])
    {
        while (j < lps_len && haystack[i] && haystack[i] == needle[j])
        {
            i++;
            j++;
        }

        if (j == needle_len) return (char*)haystack + i - j;
        else if (j)
        {
            if (j == ARRAY_SIZE(lps) && !strncmp(haystack + i, needle + j, needle_len - j))
                return (char*)haystack + i - j;
            j = lps[j-1];
        }
        else if (haystack[i]) i++;
    }
    return NULL;
}

/*********************************************************************
 *                  _memicmp_l   (MSVCRT.@)
 */
int __cdecl _memicmp_l(const void *v1, const void *v2, size_t len, _locale_t locale)
{
    const char *s1 = v1, *s2 = v2;
    int ret = 0;

#if _MSVCR_VER == 0 || _MSVCR_VER >= 80
    if (!s1 || !s2)
    {
        if (len)
            MSVCRT_INVALID_PMT(NULL, EINVAL);
        return len ? _NLSCMPERROR : 0;
    }
#endif

    while (len--)
    {
        if ((ret = _tolower_l(*s1, locale) - _tolower_l(*s2, locale)))
            break;
        s1++;
        s2++;
    }
    return ret;
}

/*********************************************************************
 *                  _memicmp   (MSVCRT.@)
 */
int __cdecl _memicmp(const void *s1, const void *s2, size_t len)
{
    return _memicmp_l(s1, s2, len, NULL);
}

/*********************************************************************
 *                  strcspn   (MSVCRT.@)
 */
size_t __cdecl strcspn(const char *str, const char *reject)
{
    BOOL rejects[256];
    const char *p;

    memset(rejects, 0, sizeof(rejects));

    p = reject;
    while(*p)
    {
        rejects[(unsigned char)*p] = TRUE;
        p++;
    }

    p = str;
    while(*p && !rejects[(unsigned char)*p]) p++;
    return p - str;
}

/*********************************************************************
 *                  strpbrk   (MSVCRT.@)
 */
char* __cdecl strpbrk(const char *str, const char *accept)
{
    for (; *str; str++) if (strchr( accept, *str )) return (char*)str;
    return NULL;
}

/*********************************************************************
 *                  __strncnt   (MSVCRT.@)
 */
size_t __cdecl __strncnt(const char *str, size_t size)
{
    size_t ret = 0;

#if _MSVCR_VER >= 140
    while (*str++ && size--)
#else
    while (size-- && *str++)
#endif
    {
        ret++;
    }

    return ret;
}


#ifdef _CRTDLL
/*********************************************************************
 *		_strdec (CRTDLL.@)
 */
char * CDECL _strdec(const char *str1, const char *str2)
{
    return (char *)(str2 - 1);
}

/*********************************************************************
 *		_strinc (CRTDLL.@)
 */
char * CDECL _strinc(const char *str)
{
    return (char *)(str + 1);
}

/*********************************************************************
 *		_strnextc (CRTDLL.@)
 */
unsigned int CDECL _strnextc(const char *str)
{
    return (unsigned char)str[0];
}

/*********************************************************************
 *		_strninc (CRTDLL.@)
 */
char * CDECL _strninc(const char *str, size_t len)
{
    return (char *)(str + len);
}

/*********************************************************************
 *		_strspnp (CRTDLL.@)
 */
char * CDECL _strspnp( const char *str1, const char *str2)
{
    str1 += strspn( str1, str2 );
    return *str1 ? (char*)str1 : NULL;
}
#endif
