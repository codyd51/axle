## Process this file with automake to create Makefile.in

## Copyright (C) 2002, 2003, 2008, 2009  Free Software Foundation, Inc.

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.

## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

PL_LOG_COMPILER = $(PERL)
AM_PL_LOG_FLAGS = -Mstrict -I ../.. -I $(top_srcdir)/lib -w
TEST_EXTENSIONS = .pl

TESTS = \
Condition.pl \
Condition-t.pl \
DisjConditions.pl \
DisjConditions-t.pl \
Version.pl \
Wrap.pl

EXTRA_DIST = $(TESTS)
