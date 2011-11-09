CFLAGS ?= -O2
CFLAGS += -D_GNU_SOURCE -DNDEBUG

all: fw32

install: all
	mkdir -p $(DESTDIR)/usr/bin $(DESTDIR)/etc/fw32 $(DESTDIR)/lib/systemd/system
	mkdir -p $(DESTDIR)/{proc,sys,dev,etc,home,tmp,mnt,media}
	mkdir -p $(DESTDIR)/usr/share/{kde,icons,fonts,themes}
	mkdir -p $(DESTDIR)/var/{tmp,cache/pacman-g2/pkg}
	cp fw32 $(DESTDIR)/usr/bin
	cp pacman-g2.conf $(DESTDIR)/etc/fw32
	cp fw32.service $(DESTDIR)/lib/systemd/system
	chown root:root $(DESTDIR)/usr/bin/fw32
	chmod +s $(DESTDIR)/usr/bin/fw32
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-create
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-delete
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-run
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-upgrade
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-install
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-remove
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-clean
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-mount-all
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-umount-all
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-update
	ln -sf fw32 $(DESTDIR)/usr/bin/fw32-install-package
