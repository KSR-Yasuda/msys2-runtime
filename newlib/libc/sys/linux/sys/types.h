/* libc/sys/linux/sys/types.h - The usual type zoo */

/* Written 2000 by Werner Almesberger */


#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

/* Newlib has it's own time_t and clock_t definitions in 
 * libc/include/sys/types.h.  Repeat those here and
 * skip the kernel's definitions. */

#include <machine/types.h>

#ifndef __time_t_defined
#define _TIME_T
#define __time_t_defined
typedef _TIME_T_ time_t;
#endif

#ifndef __clock_t_defined
#define _CLOCK_T
#define __clock_t_defined
typedef _CLOCK_T_ clock_t;
#endif

typedef unsigned int __socklen_t;
typedef unsigned int __useconds_t;

typedef __pid_t pid_t;
typedef __off_t off_t;
typedef __loff_t loff_t;

/* Time Value Specification Structures, P1003.1b-1993, p. 261 */

#ifndef _STRUCT_TIMESPEC
#define _STRUCT_TIMESPEC
struct timespec {
  time_t  tv_sec;   /* Seconds */
  long    tv_nsec;  /* Nanoseconds */
};
#endif /* !_STRUCT_TIMESPEC */

#include <linux/types.h>

#endif
