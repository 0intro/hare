#                           -*- mode: makefile -*-

# directory type (p=project, c=category, d=directory, and s=stub
TYPE=p

# FIXME: turn on debugging by default
override GTD_DEBUG=1

# include GTD's generic Makefile
ifneq ("$(shell ls | grep Makefile.tmpl | wc -l)","0")
include ./Makefile.tmpl
else
include /usr/share/gtd-2/templates/Makefile.tmpl
endif

# beginning of user defined/modified settings

PROJ = hpd # mpipes thesis proposal class_mereotopology glos_ex

mpipes.pdf: *.tex

all:: proj

CLEANFILES = *.lot *.lof *.cglg *.elg *.mlg *.cglo *.elo *.mlo \
	*.cgls *.els *.mls *.xdy

commit:
	hg $@
push clone:
	hg $@ ssh://arlhg@riga.dreamhost.com/fox
pull:
	hg $@ -u ssh://arlhg@riga.dreamhost.com/fox

