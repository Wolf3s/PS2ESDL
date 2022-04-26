#ifndef _MASS_DEBUG_H
#define _MASS_DEBUG_H

#ifdef DEBUG
#define XPRINTF(args...) printf(args)
#else
#define XPRINTF(args...)
#endif

#endif  /* _MASS_DEBUG_H */

