

# CFLAGS += -I$(BUILDDIR)
CFLAGS += -ldrm -I$(SDKTARGETSYSROOT)/usr/include/drm

all: du_cms_tp

du_cms_tp: du_cms_tp.c
	$(CC) -o $@ $< $(CFLAGS)

clean:
	rm -f du_cms_tp
