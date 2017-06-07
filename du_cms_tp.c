/* 
 * Copyright (c) 2016 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php
 */

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <rcar_du_drm.h>

/* Macro */
#define CLU_NUM (17 * 17 * 17)

/** Structure **/
struct du_cmm_tp_mem_t {
	struct rcar_du_cmm_buf	buf;
	void			*user_virt_addr;
};

struct du_cmm_tp_display_t {
	int num;
	uint32_t *crtc_id;
	bool *active;
};


/** Function **/
int get_displays(int fd, struct du_cmm_tp_display_t *displays);
void put_displays(struct du_cmm_tp_display_t *displays);
int du_cmm_tp_alloc(int fd, struct du_cmm_tp_mem_t *mem_info, unsigned long size);
void du_cmm_tp_free(int fd, struct du_cmm_tp_mem_t *mem_info);
int du_cmm_tp_set_clu(int fd, int crtc_id, struct du_cmm_tp_mem_t *clu);

int main(int argc, char** argv)
{
	int ret = -1;
	int drm_fd;
	struct du_cmm_tp_display_t displays;
	int i;
	struct du_cmm_tp_mem_t table;
	uint32_t *ptr;

	/* Open CMM(DRM) FD */
	drm_fd = drmOpen("rcar-du", NULL);
	if (drm_fd < 0) {
		puts("error: open drm rcar-du device");
		goto error_open;
	}

	/* Get Displays */
	ret = get_displays(drm_fd, &displays);
	if (ret) {
		puts("error: open rcar-du device");
		goto error_get_displays;
	}

	/* Create CLU color table  */
	ret = du_cmm_tp_alloc(drm_fd, &table, CLU_NUM * 4);
	if (ret) {
		printf("error alloc clu table : %s\n", strerror(-ret));
		goto error_alloc_table;
	}

	/* Set blue color table */
	ptr = (uint32_t *)table.user_virt_addr;
	for (i = 0; i < CLU_NUM; i++) {
		ptr[i] = 0x000000FF;
	}


	for (i = 0; i < displays.num; i++) {
		if (!displays.active[i])
			continue;

		/* set CLU */
		ret = du_cmm_tp_set_clu(drm_fd, displays.crtc_id[i], &table);
		if (ret) {
			printf("error set clu : %s\n", strerror(-ret));
		}
	}

	printf("Please check active display becomes blue. after, hit any key.");
	{
		char c = getchar();
	}

	/* Set default color table */
	ptr = (uint32_t *)table.user_virt_addr;
	for (i = 0; i < CLU_NUM; i++) {
		int r, g, b;
		int index = i;

		r = index % 17;
		r = (r << 20);
		if (r > (255 << 16))
		        r = (255 << 16);
		index /= 17;

		g = index % 17;
		g = (g << 12);
		if (g > (255 << 8))
		        g = (255 << 8);
		index /= 17;

		b = index % 17;
		b = (b << 4);
		if (b > (255 << 0))
		        b = (255 << 0);

		ptr[i] = r | g | b;
	}


	for (i = 0; i < displays.num; i++) {
		if (!displays.active[i])
			continue;
		du_cmm_tp_set_clu(drm_fd, displays.crtc_id[i], &table);
	}


	du_cmm_tp_free(drm_fd, &table);

error_alloc_table:
	put_displays(&displays);
error_get_displays:
	drmClose(drm_fd);
error_open:
	return ret;
}

int get_displays(int fd, struct du_cmm_tp_display_t *displays)
{
	int ret = -1;
	drmModeRes *drm_res = NULL;
	int i;

	/* Get DRM resource */
	drm_res = drmModeGetResources(fd);
	if (drm_res == NULL) {
		printf("error : drmModeGetResources\n", __func__);
		goto error_get_resources;
	}

	/* Get DU(CRTC) count */
	displays->num = drm_res->count_crtcs;

	displays->crtc_id = calloc(displays->num, sizeof (*displays->crtc_id));
	if (displays->crtc_id == NULL) {
		printf("error : alloc id array\n", __func__);
		goto error_alloc_id_array;
	}

	displays->active = calloc(displays->num, sizeof (*displays->active));
	if (displays->active == NULL) {
		printf("error : alloc active array\n", __func__);
		goto error_alloc_active_array;
	}

	/* Get DU(CRTC) ID and active */
	for (i = 0; i < displays->num; i++) {
		drmModeCrtc *crtc;

		displays->crtc_id[i] = drm_res->crtcs[i];

		crtc = drmModeGetCrtc(fd, displays->crtc_id[i]);

		displays->active[i] = crtc->buffer_id == 0 ? false : true;

		drmModeFreeCrtc(crtc);
	}

	drmModeFreeResources(drm_res);

	return 0;

error_alloc_active_array:
	free(displays->crtc_id);
error_alloc_id_array:
	drmModeFreeResources(drm_res);
error_get_resources:
	return -1;
}

void put_displays(struct du_cmm_tp_display_t *displays)
{
	free(displays->crtc_id);
	free(displays->active);
}


int du_cmm_tp_set_clu(int fd, int crtc_id, struct du_cmm_tp_mem_t *clu)
{
	int ret;
	int i;
	struct rcar_du_cmm_table table = {
		.user_data	= clu->buf.handle,
		.crtc_id	= crtc_id,
		.buff_len	= clu->buf.size,
		.buff		= clu->buf.handle,
	};
	struct rcar_du_cmm_event event = {
		.crtc_id = crtc_id,
		.event 		= CMM_EVENT_CLU_DONE,
	};
	unsigned long handle;

	/* Que CLU table */
	ret = drmCommandWrite(fd, DRM_RCAR_DU_CMM_SET_CLU, &table, sizeof table);
	if (ret) {
		puts("error: set clu");
		return -1;
	}

	/* Wait CLU table done */
	drmCommandWriteRead(fd, DRM_RCAR_DU_CMM_WAIT_EVENT, &event, sizeof event);

	handle = event.callback_data;
	if ((handle != clu->buf.handle) || (event.event != CMM_EVENT_CLU_DONE)) {
		printf("error: CLU event. event %u(get %u), addr 0x%lx(get 0x%lx)\n",
			CMM_EVENT_CLU_DONE, event.event,
			clu->buf.handle, handle);
		return -1;
	}

end:
	return 0;
}

int du_cmm_tp_alloc(int fd, struct du_cmm_tp_mem_t *mem_info, unsigned long size)
{
	struct rcar_du_cmm_buf cmm_buf = {
		.size = size,
	};
	void *map = NULL;

	/* Create CMM color table */
	if (drmCommandWriteRead(fd, DRM_RCAR_DU_CMM_ALLOC,
		&cmm_buf, sizeof cmm_buf)) {
		return -1;
	}

	/* Map memory for user space */
	map = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, cmm_buf.mmap_offset);
	if (map == MAP_FAILED) {
		drmCommandWrite(fd, DRM_RCAR_DU_CMM_FREE, &cmm_buf, sizeof cmm_buf);
		return -1;
	}

	mem_info->buf = cmm_buf;
	mem_info->user_virt_addr = map;

	return 0;
}

void du_cmm_tp_free(int fd, struct du_cmm_tp_mem_t *mem_info)
{
	/* Relase memory mapping */
	munmap(mem_info->user_virt_addr, mem_info->buf.size);

	/* Relase CMM color table */
	drmCommandWrite(fd, DRM_RCAR_DU_CMM_FREE, &mem_info->buf, sizeof mem_info->buf);
}

