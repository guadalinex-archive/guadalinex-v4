DOMAIN=debian-tasks
TASKDESC=$(DOMAIN).desc
TASKDIR=/usr/share/tasksel
DESCDIR=tasks
UDOMAIN=ubuntu-tasks
USUITE=edgy
UFLAVOURS=ubuntu kubuntu edubuntu xubuntu guadalinex
UTASKDESC=$(UDOMAIN).desc
UDESCDIR=ubuntu-tasks
DESCPO=$(DESCDIR)/po
VERSION=$(shell expr "`dpkg-parsechangelog 2>/dev/null |grep Version:`" : '.*Version: \(.*\)' | cut -d - -f 1)
LANGS=ar bg bs ca cs cy da de el es et eu fi fr gl he hr hu id it ja ko lt mg mk nb nl nn pa pl pt pt_BR ro ru sk sl sq sv tl tr uk vi zh_CN zh_TW
LANGS_DESC=ar bg bn bs ca cs cy da de el es et eu fi fr gl he hr hu id it ja ko lt mg mk nb nl nn pa pl pt pt_BR ro ru sk sl sq sv tl tr uk vi zh_CN zh_TW
LOCALEDIR=$(DESTDIR)/usr/share/locale

all: $(TASKDESC) $(UTASKDESC) $(DESCPO)/build_stamp po/build_stamp

$(TASKDESC): makedesc.pl $(DESCDIR)/[a-z]??*
	./doincludes.pl $(DESCDIR)
	./makedesc.pl $(DESCDIR) $(TASKDESC)

$(UDESCDIR): ubuntu-seeds.pl
	./ubuntu-seeds.pl $(UDESCDIR) $(USUITE) $(UFLAVOURS)

$(UTASKDESC): makedesc.pl $(UDESCDIR)/[a-z]??*
	./makedesc.pl $(UDESCDIR) $(UTASKDESC)

%.o: %.c
	$(COMPILE) $<

po/build_stamp:
	$(MAKE) -C po LANGS="$(LANGS)"

updatepo:
	$(MAKE) -C po update LANGS="$(LANGS)"

$(DESCPO)/build_stamp:
	$(MAKE) -C $(DESCPO) LANGS="$(LANGS_DESC)"

updatetaskspo:
	$(MAKE) -C $(DESCPO) update LANGS="$(LANGS_DESC)"

install:
	install -d $(DESTDIR)/usr/bin \
		$(DESTDIR)/usr/lib/tasksel/tests \
		$(DESTDIR)/usr/lib/tasksel/packages \
		$(DESTDIR)/usr/share/man/man8
	install -m 755 tasksel.pl $(DESTDIR)/usr/bin/tasksel
	install -m 755 tasksel-debconf $(DESTDIR)/usr/lib/tasksel/
	install -m 755 tests/new-install $(DESTDIR)/usr/lib/tasksel/tests/
	install -m 755 tests/debconf $(DESTDIR)/usr/lib/tasksel/tests/
	install -m 755 tests/lang $(DESTDIR)/usr/lib/tasksel/tests/
	install -m 755 packages/list $(DESTDIR)/usr/lib/tasksel/packages/
	pod2man --section=8 --center "Debian specific manpage" --release $(VERSION) tasksel.pod | gzip -9c > $(DESTDIR)/usr/share/man/man8/tasksel.8.gz
	for lang in $(LANGS); do \
		[ ! -d $(LOCALEDIR)/$$lang/LC_MESSAGES/ ] && mkdir -p $(LOCALEDIR)/$$lang/LC_MESSAGES/; \
		install -m 644 po/$$lang.mo $(LOCALEDIR)/$$lang/LC_MESSAGES/tasksel.mo; \
	done

install-data:
	install -d $(DESTDIR)$(TASKDIR) \
		$(DESTDIR)/usr/lib/tasksel/tests \
		$(DESTDIR)/usr/lib/tasksel/info \
		$(DESTDIR)/usr/lib/tasksel/packages
	install -m 0644 $(UTASKDESC) $(DESTDIR)$(TASKDIR)
	for test in tests/*; do \
		[ "$$test" = "tests/new-install" ] && continue; \
		[ "$$test" = "tests/debconf" ] && continue; \
		[ "$$test" = "tests/lang" ] && continue; \
		install -m 755 $$test $(DESTDIR)/usr/lib/tasksel/tests/; \
	done
	for script in info/*; do \
		install -m 755 $$script $(DESTDIR)/usr/lib/tasksel/info/; \
		for flavour in $(UFLAVOURS); do \
			ln -s desktop.preinst $(DESTDIR)/usr/lib/tasksel/info/$$flavour-desktop.preinst; \
		done; \
	done
	for package in packages/*; do \
		[ "$$package" = "packages/list" ] && continue; \
		install -m 755 $$package $(DESTDIR)/usr/lib/tasksel/packages/; \
	done
	for lang in $(LANGS_DESC); do \
		[ ! -d $(LOCALEDIR)/$$lang/LC_MESSAGES/ ] && mkdir -p $(LOCALEDIR)/$$lang/LC_MESSAGES/; \
		install -m 644 tasks/po/$$lang.mo $(LOCALEDIR)/$$lang/LC_MESSAGES/$(DOMAIN).mo; \
	done

clean:
	rm -f $(TASKDESC) $(UTASKDESC) *~
	$(MAKE) -C po clean
	$(MAKE) -C $(DESCPO) clean

# This taget is run to generate the overrides files.
# It is run from a cron job, so should only generate output if there are
# problems.
override:
	@svn up tasks 2>&1 | grep -v ^U | grep -v "At revision" || true
	@./makeoverride.pl $(DESCDIR) > external-overrides-task
