/* -*-c++-*-  vi: set ts=4 sw=4 :

  (C) Copyright 2006-2007, vitki.net. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

  $Date$
  $Revision$
  $Source$

  Memory allocation, copying and zeroing wrappers.

*/

#ifndef OPTIKUS_CONF_MEM_H
#define OPTIKUS_CONF_MEM_H

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <optikus/types.h>
#include <string.h>		/* for memcpy,memset,strdup */
#include <stdlib.h>		/* for calloc,free */

#if defined(MALLOC_ASSERTION)
#include <assert.h>
#endif

OPTIKUS_BEGIN_C_DECLS


#define OX_INLINE	__attribute__((always_inline)) static inline


/*				memory copy & fill				*/

OX_INLINE void *
oxbcopy(const void *src, void *dest, int n)
{
	if (n > 0)
		memcpy(dest, src, n);
	return dest;
}

OX_INLINE void *
oxbzero(void *buf, int n)
{
	if (n > 0)
		memset(buf, 0, n);
	return buf;
}

#define oxvcopy(s,d)	oxbcopy((s),(d),sizeof(*(s)))
#define oxvzero(p)		oxbzero((p),sizeof(*(p)))
#define oxacopy(s,d)	oxbcopy((s),(d),sizeof(d))
#define oxazero(a)		oxbzero((a),sizeof(a))


/*				memory allocation				*/

OX_INLINE void *
oxcalloc(size_t nitem, size_t size)
{
	void *allocated;
	if (0 == nitem || 0 == size)
		return NULL;
	allocated = calloc(nitem, size);
#if defined(MALLOC_ASSERTION)
	assert(allocated);
#endif
	return allocated;
}

OX_INLINE void *
oxrecalloc(void *optr, size_t nsize, size_t osize)
{
	void *allocated;
	if (0 == nsize) {
		allocated = NULL;
	} else if (NULL == (allocated = calloc(1, nsize))) {
#if defined(MALLOC_ASSERTION)
		assert(allocated);
#else
		return NULL;
#endif /* MALLOC_ASSERTION */
	} else if (NULL != optr && 0 != osize) {
		memcpy(allocated, optr, osize < nsize ? osize : nsize);
	}
	if (NULL != optr)
		free(optr);
	return allocated;
}

OX_INLINE void
oxfree(void *ptr)
{
	free(ptr);
}

OX_INLINE char *
oxstrdup(const char *str)
{
	return (NULL == str ? NULL : strdup(str));
}

#define oxnew(T,n)		((T *)oxcalloc((n),sizeof(T)))

#define oxrenew(T,newnum,oldnum,oldptr)	\
		((T *)oxrecalloc((oldptr),(newnum)*sizeof(T),(oldnum)*sizeof(T)))


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_CONF_MEM_H */
