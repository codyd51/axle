typedef struct __attribute__((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

extern void int32(unsigned char intnum, regs16_t* regs);

void int32_test();
