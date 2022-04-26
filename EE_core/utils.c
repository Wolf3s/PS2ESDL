#include <kernel.h>
#include <iopheap.h>
#include <loadfile.h>
#include <sifdma.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

#ifdef DEBUG_TTY_FEEDBACK
#include "../modules/POWEROFF_irx.c"
#include "../modules/PS2DEV9_irx.c"
#include "../modules/UDPTTY_irx.c"
#endif

/*----------------------------------------------------------------------------------------*/
/* Load debugging modules                                                                 */
/*----------------------------------------------------------------------------------------*/
#ifdef DEBUG_TTY_FEEDBACK
void LoadDebugModules(void)
{
	SifExecModuleBuffer(POWEROFF_irx,size_POWEROFF_irx,0,NULL,NULL);
	SifExecModuleBuffer(PS2DEV9_irx,size_PS2DEV9_irx,0,NULL,NULL);
	SifExecModuleBuffer(UDPTTY_irx,size_UDPTTY_irx,0,NULL,NULL);
}
#endif

