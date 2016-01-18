#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "hw/state.xml.h"

enum {
	MAX_STATE = (0xffff + 1) * 4,
};

static uint32_t address_states[] = {
	VIVS_FE_INDEX_STREAM_BASE_ADDR,
	VIVS_FE_VERTEX_STREAM_BASE_ADDR,
	0x1410,
	0x1430,
	0x1460,
	0x1480,
	0x1608,
	0x1610,
};

struct state {
	uint32_t state[MAX_STATE];
	uint32_t draw_op[6];
};

static int read_state(int fd, struct state *state)
{
	uint32_t data[0x400];
	uint32_t word[2];
	unsigned num, addr, i;
	int ret;

	do {
		ret = read(fd, word, sizeof(word));
		if (ret <= 0)
			return ret;
		if (ret != sizeof(word))
			return -1;

		switch (word[0] >> 27) {
		case 0:
			break;
		case 1: /* load state */
			num = (word[0] >> 16) & 0x3ff;
			addr = word[0] & 0xffff;
			if (addr == VIVS_FE_VERTEX_ELEMENT_CONFIG(0) >> 2)
				memset(&state->state[0x600 >> 2], 0, sizeof(uint32_t) * 16);
			state->state[addr++] = word[1];
			num--;
			if (num) {
				i = (num + 1) & ~1;
				ret = read(fd, data, sizeof(uint32_t) * i);
				if (ret <= 0)
					return ret;
				for (i = 0; i < num; i++)
					state->state[addr++] = data[i];
			}
			break;
		case 2:
			memset(state->state, 0xaa, sizeof(state->state));
			break;
		case 3:
		case 9:
			break;

		case 5:
			ret = read(fd, data, sizeof(uint32_t) * 2);
			if (ret <= 0)
				return ret;
			state->draw_op[0] = word[0];
			state->draw_op[1] = word[1];
			memcpy(&state->draw_op[2], data, sizeof(uint32_t) * 2);
			state->state[VIVS_FE_INDEX_STREAM_CONTROL >> 2] = 0;
			return 1;

		case 6:
			ret = read(fd, data, sizeof(uint32_t) * 4);
			if (ret <= 0)
				return ret;
			state->draw_op[0] = word[0];
			state->draw_op[1] = word[1];
			memcpy(&state->draw_op[2], data, sizeof(uint32_t) * 4);
			return 1;

		default:
			fprintf(stderr, "Unknown opcode: %08x\n", word[0]);
			fprintf(stderr, "Position: 0x%llx\n",
				(unsigned long long) lseek(fd, 0, SEEK_CUR));
			return -1;
		}
	} while (1);
}

static int diff_files(const char *file1, const char *file2)
{
	struct state state[2];
	off_t pos[2], new_pos[2];
	int fd[2];

	fd[0] = open(file1, O_RDONLY);
	if (fd[0] == -1)
		error(1, errno, "%s", file1);
	fd[1] = open(file2, O_RDONLY);
	if (fd[1] == -1)
		error(1, errno, "%s", file2);

	memset(state, 0, sizeof(state));

	do {
		int i, ret;

		for (i = 0; i < 2; i++) {
			pos[i] = lseek(fd[i], 0, SEEK_CUR);
			if (pos[i] == (off_t)-1)
				error(2, errno, "lseek");
			ret = read_state(fd[i], &state[i]);
			if (ret < 0)
				error(2, errno, "read");
			if (ret == 0)
				return 0;
			new_pos[i] = lseek(fd[i], 0, SEEK_CUR);
		}

		for (i = 0; i < sizeof(address_states) / sizeof(uint32_t); i++) {
			uint32_t idx = address_states[i] >> 2;

			state[0].state[idx] = state[1].state[idx];
		}

		memset(&state[0].state[0x1600 >> 2], 0, 0x44);
		memset(&state[1].state[0x1600 >> 2], 0, 0x44);

		if (memcmp(state[0].state, state[1].state, sizeof(state[0].state))) {
			printf("State differences:\n"
			       "   %s offset 0x%llx - 0x%llx\n"
			       "   %s offset 0x%llx - 0x%llx\n",
			       file1, (unsigned long long)pos[0], (unsigned long long)new_pos[0],
			       file2, (unsigned long long)pos[1], (unsigned long long)new_pos[1]);
			for (i = 0; i < sizeof(state[0].state) / sizeof(uint32_t); i++) {
				if (state[0].state[i] != state[1].state[i]) {
					printf("%05x: %08x -> %08x\n",
						i << 2, state[0].state[i], state[1].state[i]);
					state[1].state[i] = state[0].state[i];
				}
			}
		}
		if (memcmp(state[0].draw_op, state[1].draw_op, sizeof(state[0].draw_op))) {
			printf("Draw op differs:\n"
			       "   %s offset 0x%llx\n"
			       "   %s offset 0x%llx\n",
			       file1, (unsigned long long)pos[0],
			       file2, (unsigned long long)pos[1]);
		}
	} while (1);
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s FILE1 FILE2\n", argv[0]);
		return 1;
	}

	return diff_files(argv[1], argv[2]);
}
