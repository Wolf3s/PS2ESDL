/* dummy_IRX - Dummy IRX file that just returns "MODULE_RESIDENT_END" when loaded - used as a substitute for modules which shouldn't be loaded */
/*      Version: 1.13A
*       Last updated: 2010/10/26
*/

#include <loadcore.h>

#define MODNAME "cdvd_dm_driver"
IRX_ID(MODNAME, 1, 1);

int _start(int argc, char *argv[])
{  
  return((argc<0)?MODULE_NO_RESIDENT_END:2); /* A return value of 2 = REMOVABLE_RESIDENT */
}
