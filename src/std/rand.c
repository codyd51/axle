#include <std/rand.h>
#include <std/std.h>
#include <std/math.h>
#include <stdint.h>
#include <std/rand_internal.h>
#include <kernel/drivers/rtc/clock.h>

unsigned long mtwist_seed_system(mtwist* mt) {
	return time_unique();
}

#define MTWIST_UPPER_MASK    UINT32_C(0x80000000)
#define MTWIST_LOWER_MASK    UINT32_C(0x7FFFFFFF)
#define MTWIST_FULL_MASK     UINT32_C(0xFFFFFFFF)

#define MTWIST_MATRIX_A      UINT32_C(0x9908B0DF)

#define MTWIST_MIXBITS(u, v) ( ( (u) & MTWIST_UPPER_MASK) | ( (v) & MTWIST_LOWER_MASK) )
#define MTWIST_TWIST(u, v)  ( (MTWIST_MIXBITS(u, v) >> 1) ^ ( (v) & UINT32_C(1) ? MTWIST_MATRIX_A : UINT32_C(0)) )

mtwist* mtwist_new(void) {
	mtwist* mt;
	mt = (mtwist*)calloc(1, sizeof(*mt));
	if (!mt) return NULL;

	mt->left = 0;
	mt->next = NULL;
	mt->seeded = 0;

	return mt;
}

void mtwist_free(mtwist* mt) {
	if (mt) kfree(mt);
}

void mtwist_init(mtwist* mt, unsigned long seed) {
	int i;
	if (!mt) return;

	mt->state[0] = (uint32_t)(seed & MTWIST_FULL_MASK);
	for (int i = 1; i < MTWIST_N; i++) {
		mt->state[i] = (UINT32_C(1812433253) * (mt->state[i - 1] ^ (mt->state[i - 1] >> 30)) + i);
		mt->state[i] &= MTWIST_FULL_MASK;
	}

	mt->left = 0;
	mt->next = NULL;

	mt->seeded = 1;
}

static void mtwist_update_state(mtwist* mt) {
	int count;
	uint32_t* p = mt->state;

	for (count = (MTWIST_N - MTWIST_M + 1); --count; p++) {
		*p = p[MTWIST_M] ^ MTWIST_TWIST(p[0], p[1]);
	}

	for (count = MTWIST_M; --count; p++) {
		*p = p[MTWIST_M - MTWIST_N] ^ MTWIST_TWIST(p[0], p[1]);
	}

	*p = p[MTWIST_M - MTWIST_N] ^ MTWIST_TWIST(p[0], mt->state[0]);

	mt->left = MTWIST_N;
	mt->next = mt->state;
}

unsigned long mtwist_rand(mtwist* mt) {
	uint32_t r;
	if (!mt) return 0UL;

	if (!mt->seeded) {
		mtwist_init(mt, mtwist_seed_system(mt));
	}

	if (!mt->left) {
		mtwist_update_state(mt);
	}

	r = *mt->next++;
	mt->left--;

	r ^= (r >> 11);
	r ^= (r << 7) & UINT32_C(0x9D2C5680);
	r ^= (r << 15) & UINT32_C(0xEFC60000);
	r ^= (r >> 18);

	r &= MTWIST_FULL_MASK;

	return (unsigned long)abs(r);
}

double mtwist_drange(mtwist* mt) {
	unsigned long r;
	double d;

	if (!mt) return 0.0;

	r = mtwist_rand(mt);

	d = r / 4294967296.0; ///2^32

	return d;
}
