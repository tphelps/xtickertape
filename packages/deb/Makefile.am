BUILT_SOURCES = changelog control rules
EXTRA_DIST = changelog.in control.in rules.in $(BUILT_SOURCES)

.PHONY: clean deb

# Building the deb packages requires the devscripts and debhelper packages.
# There are fairly standard packages for a development debian box to have
# so i dont see this as a problem.
deb: $(srcdir)/rules $(srcdir)/control $(srcdir)/changelog
	ln -sf @top_srcdir@/COPYING copyright
	ln -sf ./packages/deb @top_srcdir@/debian
	(cd @top_srcdir@; fakeroot $(MAKE) -f debian/rules binary)

edit = sed \
	-e 's,@PACKAGE\@,$(PACKAGE),g' \
	-e 's,@PACKAGE_CONFIG_OPTS\@,$(PACKAGE_CONFIG_OPTS),g' \
	-e 's,@VERSION\@,$(VERSION),g'

$(srcdir)/changelog: changelog.in
	rm -f $@ $@.tmp
	$(edit) $< > changelog.tmp
	mv changelog.tmp $@

$(srcdir)/control: control.in
	rm -f $@ control.tmp
	$(edit) $< > control.tmp
	mv control.tmp $@

$(srcdir)/rules: rules.in
	rm -f $@ rules.tmp
	$(edit) $< > rules.tmp
	mv rules.tmp $@
