#ifndef RAND_H
#define RAND_H

//state
typedef struct mtwist_s mtwist;
//constructor
mtwist* mtwist_new(void);
//destructor
void mtwist_free(mtwist* mt);

void mtwist_init(mtwist* mt, unsigned long seed);
unsigned long mtwist_rand(mtwist* mt);
double mtwist_drand(mtwist* mt);

unsigned long mtwist_seed_system(mtwist* mt);

#endif
