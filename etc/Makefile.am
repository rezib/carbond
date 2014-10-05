EXTRA_DIST = carbon.conf.in storage-schemas.conf

sysconf_DATA = carbon.conf storage-schemas.conf storage-aggregation.conf

edit = sed -e 's|@sysconfdir[@]|$(sysconfdir)|g' -e 's|@localstatedir[@]|$(localstatedir)|g'

carbon.conf: carbon.conf.in Makefile
	$(edit) $(srcdir)/$@.in > $@

CLEANFILES = carbon.conf
