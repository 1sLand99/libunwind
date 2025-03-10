/* libunwind - a platform-independent unwind library
   Copyright (C) 2001-2004 Hewlett-Packard Co
	Contributed by David Mosberger-Tang <davidm@hpl.hp.com>
   Copyright 2022 Blackberry Limited.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "compiler.h"

#include <errno.h>
#if HAVE_EXECINFO_H
# include <execinfo.h>
#else
  extern int backtrace (void **, int);
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <ucontext.h>
#include <unistd.h>
#include <libunwind.h>

#include "ident.h"

#define panic(...)				\
	{ fprintf (stderr, __VA_ARGS__); exit (-1); }

#define MIN_FRAMES 3
#define SIG_STACK_SIZE 0x100000

#if UNW_TARGET_ARM
#define unwi_unwind_method   UNW_OBJ(unwind_method)
#define UNW_ARM_METHOD_DWARF 0x01
#define UNW_ARM_METHOD_EXIDX 0x04
#endif

int verbose;
int num_errors;
sigjmp_buf env;

/* These variables are global because they
 * cause the signal stack to overflow */
char buf[512], name[256];
unw_cursor_t cursor;
unw_context_t uc;

static void
do_backtrace (void)
{
  unw_word_t ip, sp, off;
  unw_proc_info_t pi;
  unsigned int num_frames = 0;
  int ret;

  if (verbose)
    printf ("\texplicit backtrace:\n");

  unw_getcontext (&uc);
  if (unw_init_local (&cursor, &uc) < 0)
    panic ("unw_init_local failed!\n");

  do
    {
      unw_get_reg (&cursor, UNW_REG_IP, &ip);
      unw_get_reg (&cursor, UNW_REG_SP, &sp);
      buf[0] = '\0';
      if (unw_get_proc_name (&cursor, name, sizeof (name), &off) == 0)
	{
	  if (off)
	    snprintf (buf, sizeof (buf), "<%s+0x%lx>", name, (long) off);
	  else
	    snprintf (buf, sizeof (buf), "<%s>", name);
	}
      if (verbose)
	{
	  printf ("%016lx %-32s (sp=%016lx)\n", (long) ip, buf, (long) sp);

	  if (unw_get_proc_info (&cursor, &pi) == 0)
	    {
	      printf ("\tproc=0x%lx-0x%lx\n\thandler=0x%lx lsda=0x%lx gp=0x%lx",
		  (long) pi.start_ip, (long) pi.end_ip,
		  (long) pi.handler, (long) pi.lsda, (long) pi.gp);
	    }

#if UNW_TARGET_IA64
	  {
	    unw_word_t bsp;

	    unw_get_reg (&cursor, UNW_IA64_BSP, &bsp);
	    printf (" bsp=%lx", bsp);
	  }
#endif
	  printf ("\n");

          name[0] = '\0';
          if (unw_get_elf_filename (&cursor, name, sizeof (name), &off) == UNW_ESUCCESS)
                printf ("\t[%s+0x%lx]\n", name, (long) off);
	}

      ret = unw_step (&cursor);
      if (ret < 0)
	{
#if UNW_TARGET_ARM
	  /*
	   * On ARM, when using EXIDX, the last frame will return
	   * cantunwind. Stop unwinding and check later on if enough frames
	   * have been unwound.
	   */
	  extern int unwi_unwind_method;
	  if (unwi_unwind_method & UNW_ARM_METHOD_EXIDX &&
	      ret == -UNW_ESTOPUNWIND)
	    break;
#endif
	  unw_get_reg (&cursor, UNW_REG_IP, &ip);
	  printf ("FAILURE: unw_step() returned %d for ip=%lx\n",
		  ret, (long) ip);
	  ++num_errors;
	}

      ++num_frames;
    }
  while (ret > 0);

  if (num_frames < MIN_FRAMES) {
         printf ("FAILURE: only found %u frames in the backtrace\n",
                 num_frames);
         ++num_errors;
  }

  {
    void *buffer[20];
    int i, n;

    if (verbose)
      printf ("\n\tvia backtrace():\n");
    n = backtrace (buffer, 20);
    if (verbose)
      for (i = 0; i < n; ++i)
	printf ("[%d] ip=%p\n", i, buffer[i]);
  }
}

void
foo (long val UNUSED)
{
  do_backtrace ();
}

void
bar (long v)
{
  int arr[v];
  arr[0] = 0;

  /* This is a vain attempt to use up lots of registers to force
     the frame-chain info to be saved on the memory stack on ia64.
     It happens to work with gcc v3.3.4 and gcc v3.4.1 but perhaps
     not with any other compiler.  */
  foo (f (arr[0]) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v)
       + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + (f (v) + f (v))
       ))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
       )))))))))))))))))))))))))))))))))))))))))))))))))))))));
}

void
segv_handler (int signal, siginfo_t *siginfo UNUSED, void *context UNUSED)
{
  if (verbose)
    fprintf (stderr, "segv_handler: got signal %d\n", signal);

  do_backtrace ();
  siglongjmp (env, 42);
}

void
sighandler (int signal, siginfo_t *siginfo UNUSED, void *context)
{
  ucontext_t *uc UNUSED;
  int sp;

  uc = context;

  if (verbose)
    {
      printf ("sighandler: got signal %d, sp=%p", signal, (void *)&sp);
#if UNW_TARGET_IA64
# if defined(__linux__) || defined __sun
      printf (" @ %lx", uc->uc_mcontext.sc_ip);
# else
      {
	uint16_t reason;
	uint64_t ip;

	__uc_get_reason (uc, &reason);
	__uc_get_ip (uc, &ip);
	printf (" @ %lx (reason=%d)", ip, reason);
      }
# endif
#elif UNW_TARGET_X86
# if defined __linux__ || defined __sun
      printf (" @ %lx", (unsigned long) uc->uc_mcontext.gregs[REG_EIP]);
# elif defined __FreeBSD__
      printf (" @ %lx", (unsigned long) uc->uc_mcontext.mc_eip);
# endif
#elif UNW_TARGET_X86_64
# if defined __linux__ || defined __sun
      printf (" @ %lx", (unsigned long) uc->uc_mcontext.gregs[REG_RIP]);
# elif defined __FreeBSD__
      printf (" @ %lx", (unsigned long) uc->uc_mcontext.mc_rip);
# endif
#elif UNW_TARGET_AARCH64
# if defined(__QNX__)
      fprintf (stderr, " @ %#010lx", (unsigned long) uc->uc_mcontext.cpu.elr);
# endif /* defined(__QNX__) */
#endif
      printf ("\n");
    }
  do_backtrace();
}

int
main (int argc, char **argv UNUSED)
{
  struct sigaction act;
#ifdef HAVE_SIGALTSTACK
  stack_t stk;
#endif /* HAVE_SIGALTSTACK */

  verbose = (argc > 1);

  if (verbose)
    printf ("Normal backtrace:\n");

  bar (1);

  memset (&act, 0, sizeof (act));
  act.sa_flags = SA_SIGINFO;
  act.sa_sigaction = sighandler;
  if (sigaction (SIGTERM, &act, NULL) < 0)
    panic ("sigaction: %s\n", strerror (errno));

  if (verbose)
    printf ("\nBacktrace across signal handler:\n");
  kill (getpid (), SIGTERM);

  if (verbose)
    printf ("\nBacktrace across SIGSEGV handler:\n");

  act.sa_sigaction = segv_handler;
  if (sigaction (SIGSEGV, &act, NULL) < 0)
    panic ("sigaction: %s\n", strerror (errno));

  if (sigsetjmp (env, 1) == 0)
  {
    /* Make a NULL pointer dereference.  */
    int *bad_ptr = NULL;
    *bad_ptr = 0;
  }

#ifdef HAVE_SIGALTSTACK
  if (verbose)
    printf ("\nBacktrace across signal handler on alternate stack:\n");
  stk.ss_sp = malloc (SIG_STACK_SIZE);
  if (!stk.ss_sp)
    panic ("failed to allocate %u bytes\n", SIG_STACK_SIZE);
  stk.ss_size = SIG_STACK_SIZE;
  stk.ss_flags = 0;
  if (sigaltstack (&stk, NULL) < 0)
    panic ("sigaltstack: %s\n", strerror (errno));

  memset (&act, 0, sizeof (act));
  act.sa_flags = SA_ONSTACK | SA_SIGINFO;
  act.sa_sigaction = sighandler;
  if (sigaction (SIGTERM, &act, NULL) < 0)
    panic ("sigaction: %s\n", strerror (errno));
  kill (getpid (), SIGTERM);
#endif /* HAVE_SIGALTSTACK */

  if (num_errors > 0)
    {
      fprintf (stderr, "FAILURE: detected %d errors\n", num_errors);
      exit (-1);
    }
  if (verbose)
    printf ("SUCCESS.\n");

  signal (SIGTERM, SIG_DFL);
#ifdef HAVE_SIGALTSTACK
  stk.ss_flags = SS_DISABLE;
  sigaltstack (&stk, NULL);
  free (stk.ss_sp);
#endif /* HAVE_SIGALTSTACK */

  return 0;
}
