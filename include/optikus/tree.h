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

  A simple tree-based hash.

*/

#ifndef OPTIKUS_TREE_H
#define OPTIKUS_TREE_H

#include <optikus/types.h>

OPTIKUS_BEGIN_C_DECLS


typedef struct TreeBody_st *tree_t;

#define NUMERIC_TREE  -1

tree_t  treeAlloc(int fence);
oret_t  treeFree(tree_t tp);
oret_t  treeAdd(tree_t tp, const char *str, int val);
bool_t  treeFind(tree_t tp, const char *str, int *val);
oret_t  treeNumAdd(tree_t tp, int key, int val);
bool_t  treeNumFind(tree_t tp, int key, int *val);
oret_t  treeNumAddOrSet(tree_t tp, int key, int val);
int     treeGetDepth(tree_t tp);


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_TREE_H */
