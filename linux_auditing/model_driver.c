// linux_auditing/model_driver.c --- kernel-side lab companion (user-space build)
//
// This file shows the *structure* of a typical vulnerable character device
// ioctl/mmap handler as it appears in vendor drivers. It compiles in user
// space for source-level demonstration under ASan; the auditing reasoning in
// the lab applies unchanged to the real in-kernel versions.
//
// Lab exercises live in lab.tex.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* userspace stand-ins for kernel primitives (clearly labeled in lab) */
#define copy_from_user(dst, src, n) memcpy(dst, src, n)
#define copy_to_user(dst, src, n)   memcpy(dst, src, n)

struct memdesc {
    uint32_t size;         /* claimed size of the mapping          */
    uint32_t offset;       /* claimed offset into the device bank  */
    uint8_t  rw;           /* read/write intent                    */
};

struct bank {
    unsigned char *mem;    /* simulated device memory pool         */
    size_t            len;
};

/* pretend the driver grants read/write based on flags, then registers mmap */
static int bank_mmap(struct bank *b, const struct memdesc *d, void *uaddr)
{
    /* BUG: comparison promotes to uint32 only -- wraparound passes.
       0xFFFFFFF0 + 0x40 == 0x30, which is < len. What check is safe? */
    if ((uint32_t)(d->offset + d->size) > b->len)
        return -1;

    /* What does not distinguish read vs write?  What should?      */
    (void)uaddr;
    unsigned char *dst = b->mem + d->offset;
    if (d->rw) {
        memset(dst, 0xAA, d->size);        /* simulated write path */
    }
    return 0;
}

/* pretend ioctl: user passes a memdesc pointer (i.e. copy_from_user) */
static int bank_ioctl(struct bank *b, const struct memdesc *ud, void *uaddr)
{
    struct memdesc d;          /* never zeroed before fill */
    if (ud != (void *)-1) {
        copy_from_user(&d, ud, sizeof(d) - 1);   /* BUG: one byte short */
    }
    /* rw and possibly top-stuffed bytes remain uninitialized */

    return bank_mmap(b, &d, uaddr);
}

/* ---------------- user-space main: drives the two bug paths ------------ */
int main(void)
{
    struct bank b = { .mem = malloc(0x1000), .len = 0x1000 };
    if (!b.mem) return 1;
    memset(b.mem, 0, b.len);

    struct memdesc d_ok = { .size = 0x10, .offset = 0, .rw = 1 };
    printf("ok call: %d\n", bank_ioctl(&b, &d_ok, NULL));

    /* overflow: claimed offset 0xFFFFFFF0 + size 0x40 wraps in u32 */
    struct memdesc d_ovf = { .size = 0x40, .offset = 0xFFFFFFF0u, .rw = 1 };
    printf("ovf call: %d\n", bank_ioctl(&b, &d_ovf, NULL));

    free(b.mem);
    return 0;
}
