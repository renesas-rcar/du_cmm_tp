# $(DUCMM_DIR) : please specify the directory path when cloning
# https://github.com/renesas-rcar/du_cmm.
# There is rcar_du_drm.h in top directory of du_cmm/
# $(SDKTARGETSYSROOT) : please specify the directory path of libdrm.

CFLAGS += -I$(DUCMM_DIR)
CFLAGS += -ldrm -I$(SDKTARGETSYSROOT)/usr/include/drm

all: du_cms_tp

du_cms_tp: du_cms_tp.c
	$(CC) -o $@ $< $(CFLAGS)

clean:
	rm -f du_cms_tp
