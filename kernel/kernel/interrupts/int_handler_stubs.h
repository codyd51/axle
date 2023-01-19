#ifndef INT_HANDLER_STUBS_H
#define INT_HANDLER_STUBS_H

extern void idt_activate(idt_pointer_t* table);

// Interrupt vectors reserved by the CPU
extern void internal_interrupt0(void);
extern void internal_interrupt1(void);
extern void internal_interrupt2(void);
extern void internal_interrupt3(void);
extern void internal_interrupt4(void);
extern void internal_interrupt5(void);
extern void internal_interrupt6(void);
extern void internal_interrupt7(void);
extern void internal_interrupt8(void);
extern void internal_interrupt9(void);
extern void internal_interrupt10(void);
extern void internal_interrupt11(void);
extern void internal_interrupt12(void);
extern void internal_interrupt13(void);
extern void internal_interrupt14(void);
extern void internal_interrupt15(void);
extern void internal_interrupt16(void);
extern void internal_interrupt17(void);
extern void internal_interrupt18(void);
extern void internal_interrupt19(void);
extern void internal_interrupt20(void);
extern void internal_interrupt21(void);
extern void internal_interrupt22(void);
extern void internal_interrupt23(void);
extern void internal_interrupt24(void);
extern void internal_interrupt25(void);
extern void internal_interrupt26(void);
extern void internal_interrupt27(void);
extern void internal_interrupt28(void);
extern void internal_interrupt29(void);
extern void internal_interrupt30(void);
extern void internal_interrupt31(void);

// Syscall vector
extern void internal_interrupt128(void);

// Hardware interrupts
extern void external_interrupt0(void);
extern void external_interrupt1(void);
extern void external_interrupt2(void);
extern void external_interrupt3(void);
extern void external_interrupt4(void);
extern void external_interrupt5(void);
extern void external_interrupt6(void);
extern void external_interrupt7(void);
extern void external_interrupt8(void);
extern void external_interrupt9(void);
extern void external_interrupt10(void);
extern void external_interrupt11(void);
extern void external_interrupt12(void);
extern void external_interrupt13(void);
extern void external_interrupt14(void);
extern void external_interrupt15(void);

extern void external_interrupt16(void);
extern void external_interrupt17(void);
extern void external_interrupt18(void);
extern void external_interrupt19(void);
extern void external_interrupt20(void);
extern void external_interrupt21(void);
extern void external_interrupt22(void);
extern void external_interrupt23(void);
extern void external_interrupt24(void);
extern void external_interrupt25(void);
extern void external_interrupt26(void);
extern void external_interrupt27(void);
extern void external_interrupt28(void);
extern void external_interrupt29(void);
extern void external_interrupt30(void);
extern void external_interrupt31(void);
extern void external_interrupt32(void);
extern void external_interrupt33(void);
extern void external_interrupt34(void);
extern void external_interrupt35(void);
extern void external_interrupt36(void);
extern void external_interrupt37(void);
extern void external_interrupt38(void);
extern void external_interrupt39(void);
extern void external_interrupt40(void);
extern void external_interrupt41(void);
extern void external_interrupt42(void);
extern void external_interrupt43(void);
extern void external_interrupt44(void);
extern void external_interrupt45(void);
extern void external_interrupt46(void);
extern void external_interrupt47(void);
extern void external_interrupt48(void);

#endif
