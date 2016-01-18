#include <stdint.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

#include "etnaviv_dump.h"
#include "hw/state.xml.h"

static const char *buf_name[] = {
	"reg",
	"mmu",
	"ring",
	"cmd",
	"bomap",
	"bo",
};

struct etnaviv_dump_hdr {
	uint32_t magic;
	uint32_t axi;
	uint32_t idle;
	uint32_t dma;
	uint32_t state;
	uint32_t last[2];
	struct {
		uint32_t offset;
		uint32_t size;
		uint32_t iova;
		uint16_t type;
		uint16_t map_offset;
	} obj[0];
};

static const char idle_units[12][4] = {
	"FE", "DE", "PE", "SH", "PA", "SE", "RA", "TX", "VG", "IM", "FP", "TS",
};

static const char *cmdstate[32] = {
	"idle", "dec", "adr0", "load0", "adr1", "load1", "3dadr", "3dcmd",
	"3dcntl", "3didxcntl", "initreqdma", "drawidx", "draw", "2drect0",
	"2drect1", "2ddata0", "2ddata1", "waitfifo", "wait", "link", "end",
	"stall",
};

static const char *cmddmastate[4] = {
	"idle", "start", "req", "end"
};

static const char *cmdfetchstate[4] = {
	"idle", "ramvalid", "valid", "",
};

static const char *reqdmastate[4] = {
	"idle", "waitidx", "cal", "",
};

static const char *calstate[4] = {
	"idle", "ldadr", "idxcalc", "",
};

static char *reg_decode(char *buf, size_t size, uint32_t reg, uint32_t val)
{
	unsigned int i;
	char *p;

	switch (reg) {
	case 0x004: /* idle */
		p = buf;
		p += sprintf(p, "Idle:");
		for (i = 0; i < 12; i++)
			p += sprintf(p, " %s%c", idle_units[i],
					val & (1 << i) ? '+' : '-');
		return buf;
	case 0x660: /* dma debug */
		p = buf;
		p += sprintf(p, "Cmd: [%s DMA: %s Fetch: %s] Req %s Cal %s",
			cmdstate[val & 31],
			cmddmastate[(val >> 8) & 3],
			cmdfetchstate[(val >> 10) & 3],
			reqdmastate[(val >> 12) & 3],
			calstate[(val >> 14) & 3]);
		return buf;
	case 0x664:
		return "Command DMA address";
	case 0x668:
		return "FE fetched word 0";
	case 0x66c:
		return "FE fetched word 1";
	default:
		return NULL;
	}
}

int main(int argc, char *argv[])
{
	struct etnaviv_dump_object_header *hdr;
	struct etnaviv_dump_object_header *h_regs, *h_bomap, *h_mmu;
	struct stat st;
	unsigned int nr_bufs, i, err;
	uint32_t dma_addr;
	int dump_fd, dma_buf;
	void *file;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s DUMPFILE DIR\n", argv[0]);
		return 1;
	}

	dump_fd = open(argv[1], O_RDONLY);
	if (dump_fd == -1) {
		perror("open dump file");
		return 1;
	}

	if (fstat(dump_fd, &st) == -1) {
		perror("fstat");
		close(dump_fd);
		return 1;
	}

	file = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, dump_fd, 0);
	if (file == (void *)-1) {
		perror("mmap");
		close(dump_fd);
		return 1;
	}

	close(dump_fd);

	hdr = file;
	if (hdr[0].magic != ETDUMP_MAGIC) {
		munmap(hdr, st.st_size);
		fprintf(stderr, "%s: invalid dump file\n",
			argv[1]);
		return 2;
	}

	h_mmu = NULL;
	h_regs = NULL;
	h_bomap = NULL;

	for (nr_bufs = i = 0;
	     hdr[i].magic == ETDUMP_MAGIC && nr_bufs == 0; i++) {
		switch (hdr[i].type) {
		case ETDUMP_BUF_MMU:
			h_mmu = &hdr[i];
			break;

		case ETDUMP_BUF_REG:
			h_regs = &hdr[i];
			break;

		case ETDUMP_BUF_BOMAP:
			h_bomap = &hdr[i];
			break;

		case ETDUMP_BUF_END:
			nr_bufs = i;
			break;
		}
	}

	if (nr_bufs == 0) {
		fprintf(stderr, "%s: no buffers\n", argv[1]);
		return 3;
	}

	/* Parse the register dump to find the DMA address */
	dma_addr = 0;
	dma_buf = -1;
	if (h_regs) {
		struct etnaviv_dump_registers *regs = file + h_regs->file_offset;
		unsigned int num = h_regs->file_size / sizeof(*regs);

		printf("=== Register dump\n");
		for (i = 0; i < num; i++) {
			char buf[128], *p;
			if (regs[i].reg == VIVS_FE_DMA_ADDRESS)
				dma_addr = regs[i].value;
			p = reg_decode(buf, sizeof(buf), regs[i].reg, regs[i].value);
			printf("%08x = %08x%s%s\n",
				regs[i].reg, regs[i].value,
				p ? " " : "", p ? p : "");
		}

		/* Find the DMA buffer */
		for (i = 0; i < nr_bufs; i++) {
			if (hdr[i].type != ETDUMP_BUF_RING &&
			    hdr[i].type != ETDUMP_BUF_CMD)
				continue;
			if (dma_addr >= hdr[i].iova &&
			    dma_addr < hdr[i].iova + hdr[i].file_size)
				dma_buf = i;
		}
	}

	printf("=== Buffers\n");
	printf(" %-3s %-5s %-8s %-8s\n", "Num", "Name", "IOVA", "Size");
	for (i = 0; i < nr_bufs; i++) {
		printf("%c%3u %-5s %08llx %08x %8u\n",
			i == dma_buf ? '*' : ' ',
			i, buf_name[hdr[i].type],
			hdr[i].iova, hdr[i].file_size, hdr[i].file_size);
	}

	for (i = 0; i < nr_bufs; i++) {
		char name[80];
		int fd;

		if (hdr[i].type == ETDUMP_BUF_MMU) {
			sprintf(name, "%s/mmu.bin", argv[2]);
		} else if (hdr[i].type == ETDUMP_BUF_BOMAP) {
			sprintf(name, "%s/bomap.bin", argv[2]);
		} else if (hdr[i].type == ETDUMP_BUF_RING) {
			sprintf(name, "%s/ring.bin", argv[2]);
		} else if (hdr[i].type == ETDUMP_BUF_CMD) {
			if (hdr[i].iova == 0)
				continue;

			sprintf(name, "%s/cmd-%08llx.bin", argv[2],
				hdr[i].iova);
		} else {
			if (hdr[i].iova == 0)
				continue;

			sprintf(name, "%s/bo-%08llx.bin", argv[2],
				hdr[i].iova);
		}

		fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd >= 0) {
			write(fd, file + hdr[i].file_offset,
			      hdr[i].file_size);
			close(fd);
		}
	}

	if (h_mmu && h_bomap) {
		uint32_t *mmu = file + h_mmu->file_offset;
		uint64_t *bomap = file + h_bomap->file_offset;

		printf("Checking MMU entries...");
		err = 0;
		for (i = 0; i < nr_bufs; i++) {
			unsigned int mmu_ofs, bm_ofs, num_pages, j;

			if (hdr[i].type != ETDUMP_BUF_BO ||
			    hdr[i].iova < 0x80000000)
				continue;

			num_pages = hdr[i].file_size >> 12;
			mmu_ofs = (hdr[i].iova - 0x80000000) >> 12;
			bm_ofs = hdr[i].data[0];

			for (j = 0; j < num_pages; j++)
				if (mmu[mmu_ofs + j] != bomap[bm_ofs + j]) {
					if (!err)
						printf(" failed\n");
					printf("Buf %u Offset %08x: %08x %08llx\n", i,
						j << 12, mmu[mmu_ofs + j], bomap[bm_ofs + j]);
					err = 1;
				}
		}
		if (!err)
			printf(" ok\n");
	}

	return 0;
}
