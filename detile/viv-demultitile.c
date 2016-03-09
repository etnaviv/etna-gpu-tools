#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

static int safe_write(int fd, void *buf, size_t size)
{
	size_t written = 0;
	ssize_t ret = 0;

	while (size) {
		ret = write(fd, buf, size);
		if (ret == -1 && errno == EINTR) {
			continue;
		} else if (ret > 0) {
			written += ret;
			buf += ret;
			size -= ret;
		} else if (written) {
			ret = written;
			break;
		} else {
			break;
		}
	}

	return ret;
}

static int safe_read(int fd, void *buf, size_t size)
{
	size_t rd = 0;
	ssize_t ret = 0;

	while (size) {
		ret = read(fd, buf, size);
		if (ret == -1 && errno == EINTR) {
			continue;
		} else if (ret > 0) {
			rd += ret;
			buf += ret;
			size -= ret;
		} else if (rd) {
			ret = rd;
			break;
		} else {
			break;
		}
	}

	return ret;
}


static void detile_gen(void *dst, void *src, unsigned int unit_size,
	unsigned int tile_width, unsigned int tile_height,
	unsigned int blocks_x, unsigned int blocks_y)
{
	unsigned int major_x, minor_x, major_y, minor_y;
	unsigned int tile_size = tile_width * tile_height;
	unsigned int tile_stride = tile_size * blocks_x;
	unsigned int src_p, dst_p;

	for (major_y = 0; major_y < blocks_y; major_y++) {
		for (minor_y = 0; minor_y < tile_height; minor_y++) {
			unsigned int dst_y = major_y * tile_height + minor_y;

			for (major_x = 0; major_x < blocks_x; major_x++) {
				for (minor_x = 0; minor_x < tile_width; minor_x++) {
					unsigned int dst_x = major_x * tile_width + minor_x;
					unsigned int src_maj = major_y * tile_stride + major_x * tile_size;
					unsigned int src_min = minor_y * tile_width + minor_x;

					dst_p = (dst_y * tile_width * blocks_x + dst_x) * unit_size;
					src_p = (src_maj + src_min) * unit_size;
					memcpy(dst + dst_p, src + src_p, unit_size);
				}
			}
		}
	}
}

static void demultitile(void *dst, void *src, unsigned ps, unsigned w, unsigned h)
{
	void *tmp = malloc(ps * w * h);
	void *src_u, *src_l;
	unsigned int tile_bytes;
	unsigned int tile_stride;
	unsigned int tile_w = w / 4;
	unsigned int tile_h = h / 4;
	int x, y;

	/*
	 * 4  8  12 16 20 24 28 32 36  40  44  48  52  56  60  64
	 * u1 u2 l1 l2 u5 u6 l5 l6 u9  u10 l9  l10 u13 u14 l13 l14
	 * l3 l4 u3 u4 l7 l8 u7 u8 l11 l12 u11 u12 l15 l16 u15 u16
	 */

	tile_bytes = ps * 4 * 4; /* each 4x4 tile */
	tile_stride = tile_bytes * tile_w; /* one full row of tiles */

	src_u = src;
	src_l = src + tile_stride * tile_h / 2;

	if (0) {
		fprintf(stderr, "tile bytes = 0x%x\n", tile_bytes);
		fprintf(stderr, "tile stride = 0x%x\n", tile_stride);
		fprintf(stderr, "u -> l = 0x%x\n", src_l - src_u);
	}

	for (y = 0; y < tile_h / 2; y++) {
		void *dpyu = tmp + y * 2 * tile_stride;
		void *dpyl = dpyu + tile_stride;
		void *spyu = src_u + y * tile_stride; /* upper half */
		void *spyl = src_l + y * tile_stride; /* lower half */
		for (x = 0; x < tile_w / 4; x++) {
			void *dpu = dpyu + x * 4 * tile_bytes;
			void *dpl = dpyl + x * 4 * tile_bytes;
			void *spu = spyu + x * 4 * tile_bytes;
			void *spl = spyl + x * 4 * tile_bytes;

			memcpy(dpu, spu, 2 * tile_bytes);
			memcpy(dpu + 2 * tile_bytes, spl, 2 * tile_bytes);
			memcpy(dpl, spl + 2 * tile_bytes, 2 * tile_bytes);
			memcpy(dpl + 2 * tile_bytes, spu + 2 * tile_bytes, 2 * tile_bytes);
		}
	}

	detile_gen(dst, tmp, ps, 4, 4, tile_w, tile_h);
	free(tmp);
}

static void detile(void *dst, void *src, unsigned ps, unsigned w, unsigned h)
{
	detile_gen(dst, src, ps, 4, 4, w / 4, h / 4);
}

int main(int argc, char *argv[])
{
	struct stat st;
	void *ptr, *out;
	int fd = 0;
	int opt, ret, cpp = 4, width = 0, height = -1, multitile = 0;
	size_t size;

	while ((opt = getopt(argc, argv, "w:h:m")) != -1) {
		switch (opt) {
		case 'w':
			width = strtoul(optarg, NULL, 10);
			break;
		case 'h':
			height = strtoul(optarg, NULL, 10);
			break;
		case 'm':
			multitile = 1;
			break;
		default:
			fprintf(stderr, "Usage: %s [-w WIDTH] [-h HEIGHT] [FILE]\n",
				argv[0]);
			return 1;
		}
	}

	if (optind < argc) {
		fd = open(argv[optind], O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "%s: %s: %m\n", argv[0], argv[optind]);
			return 1;
		}
	} else {
		fd = dup(0);
	}

	if (fstat(fd, &st) == -1) {
		fprintf(stderr, "%s: failed to stat: %m\n", argv[0]);
		close(fd);
		return 1;
	}

	if (height == -1)
		height = st.st_size / (width * cpp);

	size = cpp * width * height;
	if (st.st_size) {
		if (size > st.st_size) {
			fprintf(stderr, "%s: width/height exceeds file size\n",
				argv[0]);
			close(fd);
			return 1;
		}

		ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if (ptr == (void *)-1) {
			fprintf(stderr, "%s: failed to mmap: %m", argv[0]);
			close(fd);
			return 1;
		}
	} else {
		ptr = malloc(size);
		if (!ptr) {
			fprintf(stderr, "%s: out of memory\n", argv[0]);
			close(fd);
			return 1;
		}
		ret = safe_read(fd, ptr, size);
		if (ret < 0) {
			fprintf(stderr, "%s: %m\n", argv[0]);
			free(ptr);
			close(fd);
			return 1;
		} else if (ret != size) {
			fprintf(stderr, "%s: short read\n", argv[0]);
			free(ptr);
			close(fd);
			return 1;
		}
	}

	out = malloc(size);

	if (multitile)
		demultitile(out, ptr, cpp, width, height);
	else
		detile(out, ptr, cpp, width, height);

	ret = safe_write(1, out, size);

	free(out);

	if (ret < 0) {
		fprintf(stderr, "%s: write: %m\n", argv[0]);
		return 1;
	} else if (ret != size) {
		fprintf(stderr, "%s: short write\n", argv[0]);
		return 1;
	}

	return 0;
}
