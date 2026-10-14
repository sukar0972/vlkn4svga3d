/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Bare-metal Multiboot guest: exercises the native device without host APIs. */
typedef unsigned int u32;
static void out(u32 port, u32 value) { __asm__ volatile("outl %0,%w1"::"a"(value),"d"(port)); }
static u32 in(u32 port) { u32 v; __asm__ volatile("inl %w1,%0":"=a"(v):"d"(port)); return v; }
static void message(const char *s) { while (*s) { __asm__ volatile("outb %0,%w1"::"a"(*s++),"d"((unsigned short)0xe9)); } }
static void fail(void) { message("GUEST_FAIL\n"); out(0xf4, 0x11); for (;;) __asm__ volatile("hlt"); }
static void cfg(u32 reg, u32 value) { out(0xcf8, 0x80001000 + reg); out(0xcfc, value); }
static void reg(u32 index, u32 value) { out(0xc000, index); out(0xc001, value); }
static u32 get(u32 index) { out(0xc000, index); return in(0xc001); }
static volatile u32 *fifo = (volatile u32 *)0xf0000000;
static u32 cursor;
static void word(u32 value) { fifo[cursor++] = value; }
static void header(u32 command, u32 words) { word(command); word(words * 4); }
void guest_main(void) {
    out(0xcf8, 0x80001000);
    if (in(0xcfc) != 0x040515ad) fail();
    cfg(0x10, 0xc001); cfg(0x14, 0xe0000000); cfg(0x18, 0xf0000000); cfg(4, 7);
    reg(0, 0x90000002); reg(2, 16); reg(3, 16); reg(7, 32); reg(1, 1);
    if (!(get(17) & (1 << 14))) fail();
    fifo[0] = 4096; fifo[1] = get(19); fifo[2] = fifo[3] = 4096;
    reg(20, 1); cursor = 1024;
    header(1040, 12); word(1); word(1 << 6); word(2);
    word(1); for (u32 i=0;i<5;++i) word(0);
    word(16); word(16); word(1);
    header(1045, 1); word(1);
    header(1050, 5); word(1); word(2); word(1); word(0); word(0);
    header(1057, 9); word(1); word(1); word(0xff2468ac); word(0x3f800000); word(0);
    word(0); word(0); word(16); word(16);
    header(1058, 7); word(1); word(0); word(0); word(16); word(16); word(0); word(0);
    word(30); word(77);
    __asm__ volatile("":::"memory");
    fifo[2] = cursor * 4;
    reg(21, 1);
    for (u32 attempt=0; fifo[6]!=77 && attempt<1000000; ++attempt) reg(21,1);
    if (fifo[6]!=77 || fifo[3]!=cursor*4) fail();
    volatile u32 *pixels = (volatile u32 *)0xe0000000;
    for (u32 i=0;i<256;++i) if (pixels[i]!=0xff2468ac) fail();
    message("GUEST_BOOT_PASS\n");
    /* Host resets the machine and requires a second successful boot. */
    for (;;) __asm__ volatile("cli; hlt");
}
