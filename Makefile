CFLAGS ?= -O2
CFLAGS += -D_GNU_SOURCE -DNDEBUG -Wall -Wextra

all: fw32

clean:
	$(RM) fw32

install: all
	mkdir -p $(DESTDIR)/usr/bin $(DESTDIR)/etc/fw32 $(DESTDIR)/lib/systemd/system
	mkdir -p $(DESTDIR)/usr/sbin
	mkdir -p $(DESTDIR)/{proc,sys,dev,etc,home,tmp,mnt,media}
	mkdir -p $(DESTDIR)/usr/share/{kde,icons,fonts,themes}
	mkdir -p $(DESTDIR)/var/{tmp,cache/pacman-g2,fst}
	chmod 1777 $(DESTDIR)/tmp
	cp fw32 $(DESTDIR)/usr/sbin
	cp fw32-makepkg $(DESTDIR)/usr/sbin
	cp pacman-g2.conf $(DESTDIR)/etc/fw32
	cp fw32.service $(DESTDIR)/lib/systemd/system
	chown root:root $(DESTDIR)/usr/sbin/fw32
	chmod +s $(DESTDIR)/usr/sbin/fw32
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-create
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-delete
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/bin/fw32-run
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-upgrade
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-install
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-remove
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-clean
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-mount-all
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-umount-all
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-update
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-install-package
	ln -sf /usr/sbin/fw32 $(DESTDIR)/usr/sbin/fw32-merge
