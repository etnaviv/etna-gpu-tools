/* Get info about vivante device */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>

#include "etnaviv_drm.h"
#include "hw/common.xml.h"

#ifdef __GNUC__
#define __maybe_unused __attribute__((unused))
#else
#define __maybe_unused
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
struct feature {
	uint32_t mask;
	const char *name;
};

#include "features.h"

static void print_features(uint32_t feature, const struct feature *f, size_t n)
{
	unsigned int i = 0;
	while (n--) {
		bool flag = feature & f->mask;
	        printf("  %c %2u:%-32s\n",
			flag ? '+' : '-', i, f->name);
	        f++;
	        i++;
	}
	printf("\n");
}

struct param {
	uint32_t param;
	const char *format;
	struct feature *tbl;
	size_t tbl_sz;
	void (*decode)(uint32_t value);
};

static struct param params[] = {
	{
		.param = ETNAVIV_PARAM_GPU_MODEL,
		.format = "Chip model: GC%x\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_REVISION,
		.format = "Chip revision: 0x%04x\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_FEATURES_0,
		.format = "Chip features: 0x%08x\n",
		.tbl = vivante_chipFeatures,
		.tbl_sz = ARRAY_SIZE(vivante_chipFeatures),
	}, {
		.param = ETNAVIV_PARAM_GPU_FEATURES_1,
		.format = "Chip minor features 0: 0x%08x\n",
		.tbl = vivante_chipMinorFeatures0,
		.tbl_sz = ARRAY_SIZE(vivante_chipMinorFeatures0),
	}, {
		.param = ETNAVIV_PARAM_GPU_FEATURES_2,
		.format = "Chip minor features 1: 0x%08x\n",
		.tbl = vivante_chipMinorFeatures1,
		.tbl_sz = ARRAY_SIZE(vivante_chipMinorFeatures1),
	}, {
		.param = ETNAVIV_PARAM_GPU_FEATURES_3,
		.format = "Chip minor features 2: 0x%08x\n",
		.tbl = vivante_chipMinorFeatures2,
		.tbl_sz = ARRAY_SIZE(vivante_chipMinorFeatures2),
	}, {
		.param = ETNAVIV_PARAM_GPU_FEATURES_4,
		.format = "Chip minor features 3: 0x%08x\n",
		.tbl = vivante_chipMinorFeatures3,
		.tbl_sz = ARRAY_SIZE(vivante_chipMinorFeatures3),
	}, {
		.param = ETNAVIV_PARAM_GPU_FEATURES_5,
		.format = "Chip minor features 4: 0x%08x\n",
		.tbl = vivante_chipMinorFeatures4,
		.tbl_sz = ARRAY_SIZE(vivante_chipMinorFeatures4),
	}, {
		.param = ETNAVIV_PARAM_GPU_FEATURES_6,
		.format = "Chip minor features 5: 0x%08x\n",
		.tbl = vivante_chipMinorFeatures5,
		.tbl_sz = ARRAY_SIZE(vivante_chipMinorFeatures5),
	}, {
		.param = ETNAVIV_PARAM_GPU_STREAM_COUNT,
		.format = "Stream count: %u\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_REGISTER_MAX,
		.format = "Register max: %u\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_THREAD_COUNT,
		.format = "Thread count: %u\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_SHADER_CORE_COUNT,
		.format = "Shader core count: %u\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_VERTEX_CACHE_SIZE,
		.format = "Vertex cache size: %ukB\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_VERTEX_OUTPUT_BUFFER_SIZE,
		.format = "Vertex output buffer size: %u\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_PIXEL_PIPES,
		.format = "Pixel pipes: %u\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_INSTRUCTION_COUNT,
		.format = "Instruction count: %u\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_NUM_CONSTANTS,
		.format = "Num constants: %u\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_BUFFER_SIZE,
		.format = "Buffer size: %u\n",
	}, {
		.param = ETNAVIV_PARAM_GPU_NUM_VARYINGS,
		.format = "Varyings count: %u\n",
	},
};

static int open_render(void)
{
	drmVersionPtr version;
	char buf[64];
	int minor, fd, rc;

	for (minor = 0; minor < 64; minor++) {
		snprintf(buf, sizeof(buf), "%s/renderD%d",
			 DRM_DIR_NAME, 128 + minor);

		fd = open(buf, O_RDWR);
		if (fd == -1)
			continue;

		version = drmGetVersion(fd);
		if (version) {
			rc = strcmp(version->name, "etnaviv");
			drmFreeVersion(version);

			if (rc == 0)
				return fd;
		}

		close(fd);
	}

	return -1;
}

static void show_one_gpu(int fd, int pipe)
{
	struct drm_etnaviv_param req;
	int i;

	req.pipe = pipe;

	printf("********** core: %i ***********\n", pipe);
	printf("* Chip identity:\n");
	for (i = 0; i < ARRAY_SIZE(params); i++) {
		uint32_t val;

		req.param = params[i].param;
		if (drmCommandWriteRead(fd, DRM_ETNAVIV_GET_PARAM, &req, sizeof(req)))
			continue;

		val = req.value;

		printf(params[i].format, val);
		if (params[i].tbl)
			print_features(val, params[i].tbl, params[i].tbl_sz);
	}
	printf("\n");
}

int main()
{
	struct drm_etnaviv_param req;
	int fd, pipe;

	fd = open_render();
	if (fd == -1) {
		perror("Cannot open device");
		exit(1);
	}

	for (pipe = 0; pipe < 5; pipe++) {
		req.pipe = pipe;
		req.param = ETNAVIV_PARAM_GPU_MODEL;
		if (drmCommandWriteRead(fd, DRM_ETNAVIV_GET_PARAM, &req, sizeof(req)))
			continue;

		show_one_gpu(fd, pipe);
	}

	close(fd);

	return 0;
}

