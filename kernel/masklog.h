/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * Copyright (C) 2005 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#ifndef O2CLUSTER_MASKLOG_H
#define O2CLUSTER_MASKLOG_H

/*
 * For now this is a trivial wrapper around printk() that gives the critical
 * ability to enable sets of debugging output at run-time.  In the future this
 * will almost certainly be redirected to relayfs so that it can pay a
 * substantially lower heisenberg tax.
 *
 * Callers associate the message with a bitmask and a global bitmask is
 * maintained with help from /proc.  If any of the bits match the message is
 * output.
 *
 * We must have efficient bit tests on i386 and it seems gcc still emits crazy
 * code for the 64bit compare.  It emits very good code for the dual unsigned
 * long tests, though, completely avoiding tests that can never pass if the
 * caller gives a constant bitmask that fills one of the longs with all 0s.  So
 * the desire is to have almost all of the calls decided on by comparing just
 * one of the longs.  This leads to having infrequently given bits that are
 * frequently matched in the high bits.
 *
 * _ERROR and _NOTICE are used for messages that always go to the console and
 * have appropriate KERN_ prefixes.  We wrap these in our function instead of
 * just calling printk() so that this can eventually make its way through
 * relayfs along with the debugging messages.  Everything else gets KERN_DEBUG.
 * The inline tests and macro dance give GCC the opportunity to quite cleverly
 * only emit the appropriage printk() when the caller passes in a constant
 * mask, as is almost always the case.
 *
 * All this bitmask nonsense is hidden from the /proc interface so that Joel
 * doesn't have an aneurism.  Reading the file gives a straight forward
 * indication of which bits are on or off:
 * 	ENTRY off
 * 	EXIT off
 * 	ERROR off
 * 	NOTICE on
 *
 * Writing changes the state of a given bit and requires a strictly formatted
 * single write() call:
 *
 * 	write(fd, "ENTRY on", 8);
 *
 * would turn the entry bit on.  "1" is also accepted in the place of "on", and
 * "off" and "0" behave as expected.
 *
 * Some trivial shell can flip all the bits on or off:
 *
 * log_mask="/proc/fs/oracleasm/log_mask"
 * cat $log_mask | (
 * 	while read bit status; do
 * 		# $1 is "on" or "off", say
 * 		echo "$bit $1" > $log_mask
 * 	done
 * )
 */

/* for task_struct */
#include <linux/sched.h>

/* bits that are frequently given and infrequently matched in the low word */
/* NOTE: If you add a flag, you need to also update mlog.c! */
#define ML_ENTRY	0x0000000000000001ULL /* func call entry */
#define ML_EXIT		0x0000000000000002ULL /* func call exit */
#define ML_DISK		0x0000000000000004ULL /* Disk information */
#define ML_REQUEST	0x0000000000000010ULL /* I/O requests */
#define ML_BIO		0x0000000000000020ULL /* bios backing I/O */
#define ML_IOC		0x0000000000000040ULL /* asm_iocs */
#define ML_ABI		0x0000000000000100ULL /* ABI entry points */
/* bits that are infrequently given and frequently matched in the high word */
#define ML_ERROR	0x0000000100000000ULL /* sent to KERN_ERR */
#define ML_NOTICE	0x0000000200000000ULL /* setn to KERN_NOTICE */

#define MLOG_INITIAL_AND_MASK (ML_ERROR|ML_NOTICE)
#define MLOG_INITIAL_NOT_MASK (ML_ENTRY|ML_EXIT)
#ifndef MLOG_MASK_PREFIX
#define MLOG_MASK_PREFIX 0
#endif

#define MLOG_MAX_BITS 64

struct mlog_bits {
	unsigned long words[MLOG_MAX_BITS / BITS_PER_LONG];
};

extern struct mlog_bits mlog_and_bits, mlog_not_bits;

#if BITS_PER_LONG == 32

#define __mlog_test_u64(mask, bits)			\
	( (u32)(mask & 0xffffffff) & bits.words[0] || 	\
	  ((u64)(mask) >> 32) & bits.words[1] )
#define __mlog_set_u64(mask, bits) do {			\
	bits.words[0] |= (u32)(mask & 0xffffffff);	\
       	bits.words[1] |= (u64)(mask) >> 32;		\
} while (0)
#define __mlog_clear_u64(mask, bits) do {		\
	bits.words[0] &= ~((u32)(mask & 0xffffffff));	\
       	bits.words[1] &= ~((u64)(mask) >> 32);		\
} while (0)
#define MLOG_BITS_RHS(mask) {				\
	{						\
		[0] = (u32)(mask & 0xffffffff),		\
		[1] = (u64)(mask) >> 32,		\
	}						\
}

#else /* 32bit long above, 64bit long below */

#define __mlog_test_u64(mask, bits)	((mask) & bits.words[0])
#define __mlog_set_u64(mask, bits) do {		\
	bits.words[0] |= (mask);		\
} while (0)
#define __mlog_clear_u64(mask, bits) do {	\
	bits.words[0] &= ~(mask);		\
} while (0)
#define MLOG_BITS_RHS(mask) { { (mask) } }

#endif

/*
 * smp_processor_id() "helpfully" screams when called outside preemptible
 * regions in current kernels.  sles doesn't have the variants that don't
 * scream.  just do this instead of trying to guess which we're building
 * against.. *sigh*.
 */
#define __mlog_cpu_guess ({		\
	unsigned long _cpu = get_cpu();	\
	put_cpu();			\
	_cpu;				\
})

/* In the following two macros, the whitespace after the ',' just
 * before ##args is intentional. Otherwise, gcc 2.95 will eat the
 * previous token if args expands to nothing.
 */
#define __mlog_printk(level, fmt, args...)				\
	printk(level "(%u,%lu):%s:%d " fmt, current->pid,		\
	       __mlog_cpu_guess, __PRETTY_FUNCTION__, __LINE__ ,	\
	       ##args)

#define mlog(mask, fmt, args...) do {					\
	u64 __m = MLOG_MASK_PREFIX | (mask);				\
	if (__mlog_test_u64(__m, mlog_and_bits) &&			\
	    !__mlog_test_u64(__m, mlog_not_bits)) {			\
		if (__m & ML_ERROR)					\
			__mlog_printk(KERN_ERR, "ERROR: "fmt , ##args);	\
		else if (__m & ML_NOTICE)				\
			__mlog_printk(KERN_NOTICE, fmt , ##args);	\
		else __mlog_printk(KERN_INFO, fmt , ##args);		\
	}								\
} while (0)

#define mlog_errno(st) do {						\
	if ((st) != -ERESTARTSYS && (st) != -EINTR)			\
		mlog(ML_ERROR, "status = %lld\n", (long long)(st));	\
} while (0)

#define mlog_entry(fmt, args...) do {					\
	mlog(ML_ENTRY, "ENTRY:" fmt , ##args);				\
} while (0)

#define mlog_entry_void() do {						\
	mlog(ML_ENTRY, "ENTRY:\n");					\
} while (0)

/* We disable this for old compilers since they don't have support for
 * __builtin_types_compatible_p.
 */
#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)) && \
    !defined(__CHECKER__)
#define mlog_exit(st) do {						     \
	if (__builtin_types_compatible_p(typeof(st), unsigned long))	     \
		mlog(ML_EXIT, "EXIT: %lu\n", (unsigned long) (st));	     \
	else if (__builtin_types_compatible_p(typeof(st), signed long))      \
		mlog(ML_EXIT, "EXIT: %ld\n", (signed long) (st));	     \
	else if (__builtin_types_compatible_p(typeof(st), unsigned int)	     \
		 || __builtin_types_compatible_p(typeof(st), unsigned short) \
		 || __builtin_types_compatible_p(typeof(st), unsigned char)) \
		mlog(ML_EXIT, "EXIT: %u\n", (unsigned int) (st));	     \
	else if (__builtin_types_compatible_p(typeof(st), signed int)	     \
		 || __builtin_types_compatible_p(typeof(st), signed short)   \
		 || __builtin_types_compatible_p(typeof(st), signed char))   \
		mlog(ML_EXIT, "EXIT: %d\n", (signed int) (st));		     \
	else if (__builtin_types_compatible_p(typeof(st), long long))	     \
		mlog(ML_EXIT, "EXIT: %lld\n", (long long) (st));	     \
	else								     \
		mlog(ML_EXIT, "EXIT: %llu\n", (unsigned long long) (st));    \
} while (0)
#else
#define mlog_exit(st) do {						     \
	mlog(ML_EXIT, "EXIT: %lld\n", (long long) (st));		     \
} while (0)
#endif

#define mlog_exit_ptr(ptr) do {						\
	mlog(ML_EXIT, "EXIT: %p\n", ptr);				\
} while (0)

#define mlog_exit_void() do {						\
	mlog(ML_EXIT, "EXIT\n");					\
} while (0)

#define mlog_bug_on_msg(cond, fmt, args...) do {			\
	if (cond) {							\
		mlog(ML_ERROR, "bug expression: " #cond "\n");		\
		mlog(ML_ERROR, fmt, ##args);				\
		BUG();							\
	}								\
} while (0)

#if (BITS_PER_LONG == 32) || defined(CONFIG_X86_64)
#define MLFi64 "lld"
#define MLFu64 "llu"
#define MLFx64 "llx"
#else
#define MLFi64 "ld"
#define MLFu64 "lu"
#define MLFx64 "lx"
#endif

#include <linux/proc_fs.h>
int mlog_init_proc(struct proc_dir_entry *parent);
void mlog_remove_proc(struct proc_dir_entry *parent);

#endif /* O2CLUSTER_MASKLOG_H */