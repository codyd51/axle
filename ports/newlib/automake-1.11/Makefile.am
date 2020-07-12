## Process this file with automake to create Makefile.in

## Makefile for Automake.

## Copyright (C) 1995, 1996, 1997, 1998, 1999, 2001, 2002, 2003, 2004,
## 2005, 2006, 2007, 2008, 2009  Free Software Foundation, Inc.

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

## lib goes first, because it builds Config.pm, used by aclocal and
## automake (run in doc, tests, and in the rebuild rules.)
## `.' goes before doc and tests, because the latter two directories
## run aclocal and automake.
SUBDIRS = lib . doc m4 tests

bin_SCRIPTS = automake aclocal

CLEANFILES = $(bin_SCRIPTS)
AUTOMAKESOURCES = automake.in aclocal.in

TAGS_FILES = $(AUTOMAKESOURCES)

EXTRA_DIST = \
  ChangeLog.96 \
  ChangeLog.98 \
  ChangeLog.00 \
  ChangeLog.01 \
  ChangeLog.02 \
  ChangeLog.03 \
  ChangeLog.04 \
  $(AUTOMAKESOURCES)

## Make versioned links.  We only run the transform on the root name;
## then we make a versioned link with the transformed base name.  This
## seemed like the most reasonable approach.
install-exec-hook:
	@$(POST_INSTALL)
	@for p in $(bin_SCRIPTS); do \
	  f="`echo $$p|sed '$(transform)'`"; \
	  fv="$$f-$(APIVERSION)"; \
	  rm -f $(DESTDIR)$(bindir)/$$fv; \
	  echo " $(LN) $(DESTDIR)$(bindir)/$$f $(DESTDIR)$(bindir)/$$fv"; \
	  $(LN) $(DESTDIR)$(bindir)/$$f $(DESTDIR)$(bindir)/$$fv; \
	done

uninstall-hook:
	@for p in $(bin_SCRIPTS); do \
	  f="`echo $$p|sed '$(transform)'`"; \
	  fv="$$f-$(APIVERSION)"; \
	  rm -f $(DESTDIR)$(bindir)/$$fv; \
	done


## We can't use configure to do the substitution here; we must do it
## by hand.  We use a funny notation here to avoid configure
## substitutions in our text.
do_subst = sed \
  -e 's,[@]APIVERSION[@],$(APIVERSION),g' \
  -e 's,[@]PACKAGE[@],$(PACKAGE),g' \
  -e 's,[@]PATH_SEPARATOR[@],$(PATH_SEPARATOR),g' \
  -e 's,[@]PERL[@],$(PERL),g' \
  -e 's,[@]PERL_THREADS[@],$(PERL_THREADS),g' \
  -e 's,[@]SHELL[@],$(SHELL),g' \
  -e 's,[@]VERSION[@],$(VERSION),g' \
  -e 's,[@]configure_input[@],Generated from $@.in; do not edit by hand.,g' \
  -e 's,[@]datadir[@],$(datadir),g'

## These files depend on Makefile so they are rebuilt if $(VERSION),
## $(datadir) or other do_subst'ituted variables change.
## Use chmod a-w to prevent people from editing the wrong file by accident.
automake: automake.in
aclocal: aclocal.in
automake aclocal: Makefile
	rm -f $@ $@.tmp
	$(do_subst) $(srcdir)/$@.in >$@.tmp
	chmod +x $@.tmp
	chmod a-w $@.tmp
	mv -f $@.tmp $@

## The master location for INSTALL is lib/INSTALL.
## This is where `make fetch' will install new versions.
## Make sure we also update this copy.
INSTALL: lib/INSTALL
	cp $(srcdir)/lib/INSTALL $@

################################################################
##
## Everything past here is useful to the maintainer, but probably not
## to anybody else
##

# Ensure tests are world-executable
dist-hook:
	cd $(distdir)/tests && chmod a+rx *.test

# Some simple checks, and then ordinary check.  These are only really
# guaranteed to work on my machine.
syntax_check_rules = \
sc_diff_automake_in_automake \
sc_perl_syntax \
sc_no_brace_variable_expansions \
sc_rm_minus_f \
sc_no_for_variable_in_macro \
sc_mkinstalldirs \
sc_pre_normal_post_install_uninstall \
sc_perl_no_undef \
sc_perl_no_split_regex_space \
sc_cd_in_backquotes \
sc_cd_relative_dir \
sc_perl_at_uscore_in_scalar_context \
sc_perl_local_no_parens \
sc_perl_local \
sc_AMDEP_TRUE_in_automake_in \
sc_tests_make_without_am_makeflags \
sc_tests_plain_make \
sc_tests_plain_autoconf \
sc_tests_plain_autoupdate \
sc_tests_plain_automake \
sc_tests_here_document_format \
sc_tests_Exit_not_exit \
sc_tests_automake_fails \
sc_tests_plain_aclocal \
sc_tests_plain_perl \
sc_tests_required_after_defs \
sc_tests_overriding_macros_on_cmdline \
sc_tests_plain_sleep \
sc_tests_plain_egrep_fgrep \
sc_mkdir_p \
sc_perl_at_substs \
sc_unquoted_DESTDIR \
sc_tabs_in_texi \
sc_at_in_texi

.PHONY: $(syntax_check_rules)
$(syntax_check_rules): automake aclocal
maintainer-check: $(syntax_check_rules)

## This check avoids accidental configure substitutions in the source.
## There are exactly 6 lines that should be modified.  This works out
## to 22 lines of diffs.
sc_diff_automake_in_automake:
	@if test `diff $(srcdir)/automake.in automake | wc -l` -ne 22; then \
	  echo "found too many diffs between automake.in and automake"; 1>&2; \
	  diff -c $(srcdir)/automake.in automake; \
	  exit 1; \
	fi

## Syntax check with default Perl (on my machine, Perl 5).
sc_perl_syntax:
	perllibdir="./lib$(PATH_SEPARATOR)$(srcdir)/lib" $(PERL) -c -w automake
	perllibdir="./lib$(PATH_SEPARATOR)$(srcdir)/lib" $(PERL) -c -w aclocal

## expect no instances of '${...}'.  However, $${...} is ok, since that
## is a shell construct, not a Makefile construct.
sc_no_brace_variable_expansions:
	@if grep -F '$${' $(srcdir)/lib/am/[a-z]*.am | \
	       grep -F -v '$$$$'; then \
	  echo "Found too many uses of '\$${' in the lines above." 1>&2; \
	  exit 1;				\
	else :; fi

## Make sure `rm' is called with `-f'.
sc_rm_minus_f:
	@if grep -v '^#' $(srcdir)/lib/am/[a-z]*.am $(srcdir)/tests/*.test | \
	    grep -E '\<rm ([^-]|\-[^f ]*\>)'; then \
	  echo "Suspicious 'rm' invocation." 1>&2; \
	  exit 1;				\
	else :; fi

## Never use something like `for file in $(FILES)', this doesn't work
## if FILES is empty or if it contains shell meta characters (e.g. $ is
## commonly used in Java filenames).
sc_no_for_variable_in_macro:
	@if grep 'for .* in \$$(' $(srcdir)/lib/am/[a-z]*.am; then \
	  echo 'Use "list=$$(mumble); for var in $$$$list".' 1>&2 ; \
	  exit 1; \
	else :; fi

## Make sure all invocations of mkinstalldirs are correct.
sc_mkinstalldirs:
	@if grep -n 'mkinstalldirs' $(srcdir)/lib/am/[a-z]*.am | \
	      grep -F -v '$$(mkinstalldirs)'; then \
	  echo "Found incorrect use of mkinstalldirs in the lines above" 1>&2; \
	  exit 1; \
	else :; fi

## Make sure all calls to PRE/NORMAL/POST_INSTALL/UNINSTALL
sc_pre_normal_post_install_uninstall:
	@if grep -E -n '\((PRE|NORMAL|POST)_(|UN)INSTALL\)' \
                 $(srcdir)/lib/am/[a-z]*.am | \
	      grep -v ':##' | grep -v ':	@\$$('; then \
	  echo "Found incorrect use of PRE/NORMAL/POST_INSTALL/UNINSTALL in the lines above" 1>&2; \
	  exit 1; \
	else :; fi

## We never want to use "undef", only "delete", but for $/.
sc_perl_no_undef:
	@if grep -n -w 'undef ' $(srcdir)/automake.in | \
	      grep -F -v 'undef $$/'; then \
	  echo "Found undef in automake.in; use delete instead" 1>&2; \
	  exit 1; \
	fi

## We never want split (/ /,...), only split (' ', ...).
sc_perl_no_split_regex_space:
	@if grep -n 'split (/ /' $(srcdir)/automake.in; then \
	  echo "Found bad split in the lines above." 1>&2; \
	  exit 1; \
	fi

## Look for cd within backquotes
sc_cd_in_backquotes:
	@if grep -n '^[^#]*` *cd ' $(srcdir)/automake.in \
	      $(srcdir)/lib/am/*.am; then \
	  echo "Consider using \$$(am__cd) in the lines above." 1>&2; \
	  exit 1; \
	fi

## Look for cd to a relative directory (may be influenced by CDPATH).
## Skip some known directories that are OK.
sc_cd_relative_dir:
	@if grep -n '^[^#]*cd ' $(srcdir)/automake.in \
	      $(srcdir)/lib/am/*.am | \
	      grep -v 'echo.*cd ' | \
	      grep -v 'am__cd =' | \
	      grep -v '^[^#]*cd [./]' | \
	      grep -v '^[^#]*cd \$$(top_builddir)' | \
	      grep -v '^[^#]*cd "\$$\$$am__cwd' | \
	      grep -v '^[^#]*cd \$$(abs' | \
	      grep -v '^[^#]*cd "\$$(DESTDIR)'; then \
	  echo "Consider using \$$(am__cd) in the lines above." 1>&2; \
	  exit 1; \
	fi

## Using @_ in a scalar context is most probably a programming error.
sc_perl_at_uscore_in_scalar_context:
	@if grep -Hn '[^@_A-Za-z0-9][_A-Za-z0-9]*[^) ] *= *@_' $(srcdir)/automake.in; then \
	  echo "Using @_ in a scalar context in the lines above." 1>&2; \
	  exit 1; \
	fi

## Forbid using parens with `local' to ease counting.
sc_perl_local_no_parens:
	@if grep '^[ \t]*local *(' $(srcdir)/automake.in; then \
	  echo "Don't use \`local' with parens: use several \`local' above." >&2; \
	  exit 1; \
	fi

## Allow only `local $_' in Automake.
sc_perl_local:
	@if grep -v '^[ \t]*local \$$_;' $(srcdir)/automake.in | \
	        grep '^[ \t]*local [^*]'; then \
	  echo "Please avoid \`local'." 1>&2; \
	  exit 1; \
	fi

## Don't let AMDEP_TRUE substitution appear in automake.in.
sc_AMDEP_TRUE_in_automake_in:
	@if grep '@AMDEP''_TRUE@' $(srcdir)/automake.in; then \
	  echo "Don't put AMDEP_TRUE substitution in automake.in" 1>&2; \
	  exit 1; \
	fi

## Tests should never call make directly.
sc_tests_make_without_am_makeflags:
	@if grep '^[^#].*(MAKE) ' $(srcdir)/lib/am/*.am $(srcdir)/automake.in |\
		grep -v 'AM_MAKEFLAGS'; then \
	  echo 'Use $$(MAKE) $$(AM_MAKEFLAGS).' 1>&2; \
	  exit 1; \
	fi

## Tests should never call make directly.
sc_tests_plain_make:
	@if grep -v '^#' $(srcdir)/tests/*.test | grep ':[	]*make'; then \
	  echo 'Do not run "make" in the above tests.  Use "$$MAKE" instead.' 1>&2; \
	  exit 1; \
	fi

## Tests should never call autoconf directly.
sc_tests_plain_autoconf:
	@if grep -v '^#' $(srcdir)/tests/*.test | grep ':[	]*autoconf'; then \
	  echo 'Do not run "autoconf" in the above tests.  Use "$$AUTOCONF" instead.' 1>&2; \
	  exit 1; \
	fi

## Tests should never call autoupdate directly.
sc_tests_plain_autoupdate:
	@if grep -v '^#' $(srcdir)/tests/*.test | grep ':[	]*autoupdate'; then \
	  echo 'Do not run "autoupdate" in the above tests.  Use "$$AUTOUPDATE" instead.' 1>&2; \
	  exit 1; \
	fi

## Tests should never call automake directly.
sc_tests_plain_automake:
	@if grep -v '^#' $(srcdir)/tests/*.test | grep -E ':[	]*automake([^:]|$$)'; then \
	  echo 'Do not run "automake" in the above tests.  Use "$$AUTOMAKE" instead.' 1>&2;  \
	  exit 1; \
	fi

## Tests should only use END and EOF for here documents
## (so that the next test is effective).
sc_tests_here_document_format:
	@if grep '<<' $(srcdir)/tests/*.test | grep -v 'END' | grep -v 'EOF'; then \
	  echo 'Use here documents with "END" and "EOF" only, for greppability.' 1>&2; \
	  exit 1; \
	fi

## Tests should never call exit directly, but use Exit.
## This is so that the exit status is transported correctly across the 0 trap.
## Ignore comments, and ignore one perl line in ext2.test.
sc_tests_Exit_not_exit:
	@found=false; for file in $(srcdir)/tests/*.test; do \
	  res=`sed -n '/^#/d; /^\$$PERL/d; /<<.*END/,/^END/{b;}; /<<.*EOF/,/^EOF/{b;}; /exit [$$0-9]/p' $$file`; \
	  if test -n "$$res"; then \
	    echo "$$file:$$res"; \
	    found=true; \
	  fi; \
	done; \
	if $$found; then \
	  echo 'Do not call plain "exit", use "Exit" instead, in above tests.' 1>&2; \
	  exit 1; \
	fi

## Use AUTOMAKE_fails when appropriate
sc_tests_automake_fails:
	@if grep -v '^#' $(srcdir)/tests/*.test | grep '\$$AUTOMAKE.*&&.*[eE]xit'; then \
	  echo 'Use AUTOMAKE_fails + grep to catch automake failures in the above tests.' 1>&2;  \
	  exit 1; \
	fi

## Tests should never call aclocal directly.
sc_tests_plain_aclocal:
	@if grep -v '^#' $(srcdir)/tests/*.test | grep ':[	]*aclocal'; then \
	  echo 'Do not run "aclocal" in the above tests.  Use "$$ACLOCAL" instead.' 1>&2;  \
	  exit 1; \
	fi

## Tests should never call perl directly.
sc_tests_plain_perl:
	@if grep -v '^#' $(srcdir)/tests/*.test | grep ':[	]*perl'; then \
	  echo 'Do not run "perl" in the above tests.  Use "$$PERL" instead.' 1>&2; \
	  exit 1; \
	fi

## Setting `required' after sourcing `./defs' is a bug.
sc_tests_required_after_defs:
	@for file in $(srcdir)/tests/*.test; do \
	  if out=`sed -n '/defs/,$${/required=/p;}' $$file`; test -n "$$out"; then \
	    echo 'Do not set "required" after sourcing "defs" in '"$$file: $$out" 1>&2; \
	    exit 1; \
	  fi; \
	done

## Overriding a Makefile macro on the command line is not portable when
## recursive targets are used.  Better use an envvar.  SHELL is an exception,
## POSIX says it can't come from the environment.  V and DESTDIRS are exceptions,
## too, as package authors are urged not to initialize them anywhere.
sc_tests_overriding_macros_on_cmdline:
	@if grep -E '\$$MAKE .*(SHELL=.*=|=.*SHELL=)' $(srcdir)/tests/*.test; then \
	  echo 'Rewrite "$$MAKE foo=bar SHELL=$$SHELL" as "foo=bar $$MAKE -e SHELL=$$SHELL"' 1>&2; \
	  echo ' in the above lines, it is more portable.' 1>&2; \
	  exit 1; \
	fi
	@if sed 's/DESTDIR=[^ ]*//; s/SHELL=[^ ]*//; s/V=[^ ]*//' $(srcdir)/tests/*.test | \
	    grep '\$$MAKE .*=' ; then \
	  echo 'Rewrite "$$MAKE foo=bar" as "foo=bar $$MAKE -e" in the above lines,' 1>&2; \
	  echo 'it is more portable.' 1>&2; \
	  exit 1; \
	fi
	@if grep 'SHELL=.*\$$MAKE' $(srcdir)/tests/*.test; then \
	  echo '$$MAKE ignores the SHELL envvar, use "$$MAKE SHELL=$$SHELL" in' 1>&2; \
	  echo 'the above lines.' 1>&2; \
	  exit 1; \
	fi

## Never use `sleep 1' to create files with different timestamps.
## Use `$sleep' instead.  Some filesystems (e.g., Windows') have only
## a 2sec resolution.
sc_tests_plain_sleep:
	@if grep -E '\bsleep +[12345]\b' $(srcdir)/tests/*.test; then \
	  echo 'Do not use "sleep x" in the above tests.  Use "$$sleep" instead.' 1>&2; \
	  exit 1; \
	fi

## fgrep and egrep are not required by POSIX.
sc_tests_plain_egrep_fgrep:
	@if grep -E '\b[ef]grep\b' $(srcdir)/tests/*.test ; then \
	  echo 'Do not use egrep or fgrep in test cases.  Use $$FGREP or $$EGREP.' 1>&2; \
	  exit 1; \
	fi
	@if grep -E '\b[ef]grep\b' $(srcdir)/lib/am/*.am $(srcdir)/m4/*.m4; then \
	  echo 'Do not use egrep or fgrep in the above files, they are not portable.' 1>&2; \
	  exit 1; \
	fi

sc_mkdir_p:
	@if grep 'mkdir_p' $(srcdir)/automake.in \
	      $(srcdir)/lib/am/*.am $(srcdir)/tests/*.test; then \
	  echo 'Do not use mkdir_p in the above files, use MKDIR_P.' 1>&2; \
	  exit 1; \
	fi

## Try to make sure all @...@ substitutions are covered by our
## substitution rule.
sc_perl_at_substs:
	@if test `grep -E '^[^#]*@[A-Za-z_0-9]+@' aclocal | wc -l` -ne 0; then \
	  echo "Unresolved @...@ substitution in aclocal" 1>&2; \
	  exit 1; \
	fi
	@if test `grep -E '^[^#]*@[A-Za-z_0-9]+@' automake | wc -l` -ne 0; then \
	  echo "Unresolved @...@ substitution in automake" 1>&2; \
	  exit 1; \
	fi

sc_unquoted_DESTDIR:
	@if grep -E "[^\'\"]\\\$$\(DESTDIR" $(srcdir)/lib/am/*.am; then \
	  echo 'Suspicious unquoted DESTDIR uses.' 1>&2 ; \
	  exit 1; \
	fi

sc_tabs_in_texi:
	@if grep '	' $(srcdir)/doc/automake.texi; then \
	  echo 'Do not use tabs in the manual.' 1>&2; \
	  exit 1; \
	fi

sc_at_in_texi:
	@if grep -E '([^@]|^)@([	 ][^@]|$$)' $(srcdir)/doc/automake.texi; \
	then \
	  echo 'Unescaped @.' 1>&2; \
	  exit 1; \
	fi


git-dist: maintainer-check
## Make sure the NEWS file is up-to-date.
	@if sed 1q $(srcdir)/NEWS | grep -e "$(VERSION)" > /dev/null; then :; else \
	  echo "NEWS not updated; not releasing" 1>&2; \
	  exit 1;				\
	fi
## Build the distribution
	$(MAKE) $(AM_MAKEFLAGS) distcheck
## Finally, if anything was successful, commit the last changes and tag
## the release in the repository.  We don't use RCS keywords so it's OK
## to distribute the files before they were committed.
	cd $(srcdir) && git commit -a -s && \
	   git tag -s "v$(VERSION)" -m "Release $(VERSION)"

git-release: git-dist
	case $(VERSION) in \
	  *[a-z]) dest=alpha;; \
	  *)      dest=ftp;; \
	esac; \
	$(srcdir)/lib/gnupload $(GNUPLOADFLAGS) \
	  --to $$dest.gnu.org:automake $(DIST_ARCHIVES)

git-diff:
	thisver="v$(VERSION)"; \
	if test -z "$$OLDVERSION"; then \
	  prevno=`echo "$(VERSION)" - 0.01 | bc | sed 's/^\./0./'`; \
	else prevno="$$OLDVERSION"; fi; \
	prevver=v$$prevno; \
	git diff $$prevver..$$thisver $(PACKAGE) \
	    > $(PACKAGE)-$$prevno-$(VERSION).diff

## Check our path lengths.
path-check: distdir
	(cd $(distdir) && \
## FIXME there's got to be a better way!  pathchk should take the list
## of files on stdin, at least.
	  find . -print | xargs pathchk -p); \
	  status=$$?; \
	  chmod -R a+w $(distdir) > /dev/null 2>&1; rm -rf $(distdir); \
	  exit $$status

## Program to use to fetch files.
WGET = wget
WGET_SV_CVS = $(WGET) http://savannah.gnu.org/cgi-bin/viewcvs/~checkout~/
WGET_SV_GIT_CF = $(WGET) 'http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;hb=HEAD;f='
WGET_SV_GIT_AC = $(WGET) 'http://git.savannah.gnu.org/gitweb/?p=autoconf.git;a=blob_plain;hb=HEAD;f='
WGET_SV_GIT_GL = $(WGET) 'http://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=blob_plain;hb=HEAD;f='
WGET_GCC = $(WGET) 'http://gcc.gnu.org/viewcvs/*checkout*/trunk/'

## Files that we fetch and which we compare against.
## FIXME should be a lot more here
FETCHFILES = \
INSTALL \
config-ml.in \
config.guess \
config.sub \
symlink-tree \
texinfo.tex

## Fetch the latest versions of files we care about.
fetch:
	rm -rf Fetchdir > /dev/null 2>&1
	mkdir Fetchdir
## If a get fails then that is a problem.
	(cd Fetchdir && \
	$(WGET_SV_GIT_CF)config.guess -O config.guess && \
	$(WGET_SV_GIT_CF)config.sub -O config.sub && \
	$(WGET_SV_CVS)texinfo/texinfo/doc/texinfo.tex -O texinfo.tex && \
	$(WGET_SV_GIT_GL)doc/INSTALL -O INSTALL && \
	$(WGET_GCC)config-ml.in -O config-ml.in && \
	$(WGET_GCC)symlink-tree -O symlink-tree)
## Don't exit after test because we want to give as many errors as
## possible.
	@stat=0; for file in $(FETCHFILES); do \
	  if diff -u $(srcdir)/lib/$$file Fetchdir/$$file \
	          >>Fetchdir/update.patch 2>/dev/null; then :; \
	  else \
	    stat=1; \
	    echo "Updating $(srcdir)/lib/$$file..."; \
	    cp Fetchdir/$$file $(srcdir)/lib/$$file; \
	  fi; \
	done; \
	test $$stat = 0 || \
	  echo "See Fetchdir/update.patch for a log of the changes."; \
	exit $$stat

## Generate release statistics, for the table in automake.texi.
## This has to be run in an up to date build tree, but there must
## be no temp files nor unused other files lying around!
release-stats: ps
	@am=`wc -l < automake` && \
	acl=`wc -l < aclocal` && \
	pmfiles="lib/Automake/*.pm" && \
	if test . != '$(srcdir)'; then pmfiles="$$pmfiles $(srcdir)/lib/Automake/*.pm"; \
	else :; fi && \
	pm=`cat $$pmfiles | wc -l` && \
	dot_am_files=`ls -1 $(srcdir)/lib/am/*.am | grep -v Makefile.am` && \
	amf=`echo "$$dot_am_files" | wc -l` && \
	aml=`cat $$dot_am_files | wc -l` && \
	m4f=`ls -1 $(srcdir)/m4/*.m4 | wc -l` && \
	m4l=`cat $(srcdir)/m4/*.m4 | wc -l` && \
	doc_text=`cd doc && LC_ALL=C pstops 0 automake.ps unused.ps 2>&1` && \
	echo "$$doc_text" && \
	rm -f doc/unused.ps && \
	doc=`echo "$$doc_text" | sed -n 's/.*Wrote \([1-9][0-9]*\) pages.*/\1/p'` && \
	tests="tests/*.test"; \
	if test . != '$(srcdir)'; then tests="$$tests $(srcdir)/tests/*.test"; \
	else :; fi && \
	t=`ls -1 $$tests | wc -l` && \
	tgen=`ls -1 $$tests | grep '.-p\.test' | wc -l` && \
	today=`date +%Y-%m-%d` && \
	echo "add this to the table in doc/automake.texi after verification:" && \
	printf '@item %s @tab %-6s @tab %4d @tab %4d @tab %4d @tab %4d %-4s @tab %4d %-4s @tab %3d @tab %d %-4s\n' \
	              $$today $(VERSION) $$am    $$acl    $$pm   $$aml "($$amf)" $$m4l "($$m4f)" $$doc $$t "($$tgen)"
.PHONY: release-stats
