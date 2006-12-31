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

  Simple implementation of tree based hash for strings or numbers.

*/

#include <optikus/tree.h>
#include <optikus/log.h>
#include <optikus/conf-mem.h>	/* for oxrenew,oxnew,oxfree,oxvzero */
#include <string.h>		/* for strncmp,strcpy,... */


#define NLINKS   4
#define NCHARS   256

typedef struct
{
	int     val;			/* numeric value of the tree element */
	int     key;			/* offset of the string in buffer (or key) */
	int     len;			/* length of the meaningful string or 0 */
	ushort_t link[NLINKS];	/* index pointers to child branches */
} TreeNode_t;


typedef struct TreeBody_st
{
	char    fence;			/* fence symbol or 0 */
	char   *s_buf;			/* buffer containing all strings */
	int     s_lim;			/* total size of string buffer */
	int     s_end;			/* end of filled part in string buffer */
	TreeNode_t *e_arr;		/* array containing all tree nodes */
	int     e_lim;			/* total size of the array */
	int     e_end;			/* current number of elements in the array */
	ushort_t root[NCHARS];	/* indexes of root elements */
	bool_t  bad;			/* 1=memory allocation error */
	int     max_lev;		/* maximum length of a tree branch */
} TreeBody_t;

#define ARRAY_GROWTH	100
#define BUFFER_GROWTH	1024


/*
 * Create the hash tree.
 */
tree_t
treeAlloc(int fence)
{
	int     i;
	tree_t  tp;

	if ((fence < 0 || fence >= NCHARS) && fence != NUMERIC_TREE)
		return NULL;
	tp = oxnew(TreeBody_t, 1);
	tp->fence = fence;
	tp->s_buf = NULL;
	tp->s_lim = tp->s_end = 0;
	tp->e_arr = NULL;
	tp->e_lim = 0;
	tp->e_end = 1;
	for (i = 0; i < NCHARS; i++)
		tp->root[i] = 0;
	tp->bad = FALSE;
	tp->max_lev = 0;
	return tp;
}


/*
 * Destroy the hash tree.
 */
oret_t
treeFree(tree_t tp)
{
	if (NULL == tp)
		return ERROR;
	oxfree(tp->e_arr);
	oxfree(tp->s_buf);
	oxvzero(tp);
	oxfree(tp);
	return OK;
}

int
treeGetDepth(tree_t tp)
{
	return tp ? tp->max_lev : 0;
}


/*
 * Search for a string item in the hash tree.
 */
bool_t
treeFind(tree_t tp, const char *str, int *val)
{
	TreeNode_t *e;
	uint_t  k;
	ushort_t no;
	char   *b;
	int     lev;

	if (val)
		*val = 0;
	if (!tp || tp->bad || tp->fence == (char) NUMERIC_TREE)
		return FALSE;
	e = tp->e_arr;
	b = tp->s_buf;
	no = tp->root[(int) (uchar_t) * str & (NCHARS - 1)];
	lev = 0;
	while (no) {
		if (e[no].len)
			k = strncmp(str, b + e[no].key, e[no].len);
		else
			k = strcmp(str, b + e[no].key);
		if (!k) {
			if (val)
				*val = e[no].val;
			if (lev > tp->max_lev)
				tp->max_lev = lev;
			return TRUE;
		}
		k &= NLINKS - 1;
		no = e[no].link[k];
		lev++;
	}
	if (lev > tp->max_lev)
		tp->max_lev = lev;
	return FALSE;
}


/*
 * Search for a numeric item in the hash tree.
 */
static bool_t
treeNumFindInternal(tree_t tp, int key, int *val, int *new_val)
{
	TreeNode_t *e;
	uint_t  k;
	ushort_t no;
	int     lev;

	if (val)
		*val = 0;
	if (!tp || tp->bad || tp->fence != (char) NUMERIC_TREE)
		return FALSE;
	e = tp->e_arr;
	no = tp->root[key & (NCHARS - 1)];
	lev = 0;
	while (no) {
		k = key - e[no].key;
		if (!k) {
			if (val)
				*val = e[no].val;
			if (new_val)
				e[no].val = *new_val;
			if (lev > tp->max_lev)
				tp->max_lev = lev;
			return TRUE;
		}
		no = e[no].link[k & (NLINKS - 1)];
		lev++;
	}
	if (lev > tp->max_lev)
		tp->max_lev = lev;
	return FALSE;
}


bool_t
treeNumFind(tree_t tp, int key, int *val)
{
	return treeNumFindInternal(tp, key, val, NULL);
}


/*
 * Insert a new branch in the hash tree.
 */
static oret_t
treeBranchInsert(tree_t tp, ushort_t new_no)
{
	TreeNode_t *e;
	char   *b;
	uint_t  k;
	char   *str;
	int     l;
	ushort_t no, pno;
	int     lev;

	if (!new_no || !tp || tp->bad)
		return ERROR;
	lev = 0;
	if (tp->fence == (char) NUMERIC_TREE) {
		/* numeric hash */
		e = tp->e_arr;
		l = e[new_no].key & (NCHARS - 1);
		no = tp->root[l];
		if (!no) {
			tp->root[l] = new_no;
		} else {
			do {
				k = e[new_no].key - e[no].key;
				if (!k) {
					if (lev > tp->max_lev)
						tp->max_lev = lev;
					return ERROR;
				}
				k &= NLINKS - 1;
				no = e[pno = no].link[k];
				lev++;
			} while (no);
			e[pno].link[k] = new_no;
		}
		/* ----------- */
	} else {
		/* string hash */
		e = tp->e_arr;
		b = tp->s_buf;
		str = b + e[new_no].key;
		l = (uchar_t) * str & (NCHARS - 1);
		no = tp->root[l];
		if (!no) {
			tp->root[l] = new_no;
		} else {
			do {
				l = e[no].len;
				if (l)
					k = strncmp(str, b + e[no].key, l);
				else
					k = strcmp(str, b + e[no].key);
				if (!k) {
					if (lev > tp->max_lev)
						tp->max_lev = lev;
					return ERROR;
				}
				k &= NLINKS - 1;
				no = e[pno = no].link[k];
				lev++;
			} while (no);
			e[pno].link[k] = new_no;
		}
		/* ----------- */
	}
	if (lev > tp->max_lev)
		tp->max_lev = lev;
	return OK;
}


/*
 * Add new string item to the hash tree.
 */
oret_t
treeAdd(tree_t tp, const char *str, int val)
{
	char   *signif, *nbuf;
	int     i, off, len, no, nlim;
	bool_t  found;
	TreeNode_t *e, *narr;

	/* Probably, the item already exists. */
	if (NULL == tp || tp->bad || tp->fence == (char) NUMERIC_TREE)
		return ERROR;
	found = treeFind(tp, str, &i);
	if (found)
		return (i == val ? OK : ERROR);
	/* Append the string in the tree buffer. */
	len = strlen(str);
	signif = tp->fence ? strchr(str, tp->fence) : NULL;
	off = tp->s_end;
	if (off + len + 2 >= tp->s_lim) {
		nlim = tp->s_lim + BUFFER_GROWTH;
		if (NULL == (nbuf = oxrenew(char, nlim, tp->s_lim, tp->s_buf)))
			goto FAIL;
		tp->s_lim = nlim;
		tp->s_buf = nbuf;
	}
	strcpy(tp->s_buf + off, str);
	tp->s_end += len + 1;
	/* Add the node to array. */
	no = tp->e_end;
	if (no >= tp->e_lim) {
		nlim = tp->e_lim + ARRAY_GROWTH;
		if (NULL == (narr = oxrenew(TreeNode_t, nlim, tp->e_lim, tp->e_arr)))
			goto FAIL;
		tp->e_lim = nlim;
		tp->e_arr = narr;
	}
	tp->e_end = no + 1;
	/* Fill fields of the new node. */
	e = tp->e_arr;
	e[no].val = val;
	e[no].key = off;
	found = (signif && (int) (signif - str) < len);
	e[no].len = found ? (int) (signif - str) : 0;
	for (i = 0; i < NLINKS; i++)
		e[no].link[i] = 0;
	/* Insert the new branch in the tree. */
	return treeBranchInsert(tp, (ushort_t) no);
  FAIL:
	olog("treeAdd: out of memory");
	tp->bad = TRUE;
	return ERROR;
}


/*
 * Add new numeric item to the hash tree.
 */
oret_t
treeNumAddInternal(tree_t tp, int key, int val, bool_t force)
{
	int     i, no, nlim;
	bool_t  found;
	TreeNode_t *e, *narr;

	/* Probably, the item already exists. */
	if (!tp || tp->bad || tp->fence != (char) NUMERIC_TREE)
		return ERROR;
	if (force) {
		found = treeNumFindInternal(tp, key, &i, &val);
		if (found)
			return OK;
	} else {
		found = treeNumFind(tp, key, &i);
		if (found)
			return (i == val ? OK : ERROR);
	}
	/* Add the node to array. */
	no = tp->e_end;
	if (no >= tp->e_lim) {
		nlim = tp->e_lim + ARRAY_GROWTH;
		if (NULL == (narr = oxrenew(TreeNode_t, nlim, tp->e_lim, tp->e_arr)))
			goto FAIL;
		tp->e_lim = nlim;
		tp->e_arr = narr;
	}
	tp->e_end = no + 1;
	/* Fill fields of the new tree node. */
	e = tp->e_arr;
	e[no].val = val;
	e[no].key = key;
	e[no].len = 0;
	for (i = 0; i < NLINKS; i++)
		e[no].link[i] = 0;
	/* Insert the new branch in the tree. */
	return treeBranchInsert(tp, (ushort_t) no);
  FAIL:
	olog("treeNumAdd: out of memory");
	tp->bad = TRUE;
	return ERROR;
}


oret_t
treeNumAdd(tree_t tp, int key, int val)
{
	return treeNumAddInternal(tp, key, val, FALSE);
}


oret_t
treeNumAddOrSet(tree_t tp, int key, int val)
{
	return treeNumAddInternal(tp, key, val, TRUE);
}
