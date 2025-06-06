/* libunwind - a platform-independent unwind library
   Copyright (c) 2003 Hewlett-Packard Development Company, L.P.
        Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

   Modified for x86_64 by Max Asbock <masbock@us.ibm.com>

This file is part of libunwind.

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

#include "init.h"
#include "libunwind_i.h"
#include "unwind_i.h"

int
unw_init_remote (unw_cursor_t *cursor UNUSED, unw_addr_space_t as UNUSED, void *as_arg UNUSED)
{
#ifdef UNW_LOCAL_ONLY
  // to suppress warning (unused)
  (void)cursor;
  (void)as;
  (void)as_arg;
  return -UNW_EINVAL;
#else /* !UNW_LOCAL_ONLY */
  struct cursor *c = (struct cursor *) cursor;

  if (!atomic_load(&tdep_init_done))
    tdep_init ();

  Debug (1, "(cursor=%p)\n", c);

  c->dwarf.as = as;
  if (as == unw_local_addr_space)
    {
      c->dwarf.as_arg = dwarf_build_as_arg(as_arg, /*validate*/ 0);
    }
  else
    {
      c->dwarf.as_arg = as_arg;
    }
  return common_init (c, 0);
#endif /* !UNW_LOCAL_ONLY */
}
