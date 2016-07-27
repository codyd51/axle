#ifndef RAND_INT_H
#define RAND_INT_H

#define MTWIST_N 624
#define MTWIST_M 397

struct mtwist_s {
	//MT buffer holding N uint32's
	uint32_t state[MTWIST_N];

	//pointer to above
	//next long to use
	uint32_t* next;

	//number of integers left in state before update is needed
	unsigned int left;

	//1 if seed was given
	unsigned int seeded : 1;

	//1 to always return static system seed
	//MT_STATIC_SEED
	unsigned int static_system_seed : 1;
};

#endif
