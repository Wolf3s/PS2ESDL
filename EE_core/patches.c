#include <kernel.h>
#include <loadfile.h>
#include <smem.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

static void *scan_pattern(u32 *pattern, u32 *mask, void *start, void *end);

/* Data used for patching Kingdom Hearts 2 */ 
static int (*pCdRead)(u32 lsn, u32 nSectors, void *buffer, u32 *mode);
static int delayed_cdRead(u32 lsn, u32 nSectors, void *buffer, u32 *mode);

/* Data used for patching Street Fighter Alpha Anthology. */
static int (*LoadPackedModule)(void *moduleBuffer, int mod_index, int mod_argc, char *mod_argv[]);
static int PatchPackedModule(void *moduleBuffer, int mod_index, int mod_argc, char *mod_argv[]);

/*----------------------------------------------------------------------------------------*/
/* Patch an ELF that has been loaded into memory.                                         */
/*----------------------------------------------------------------------------------------*/
inline void patch_ELF(void *buffer, struct EE_ELF_patch_data *patch){
	u32 *ptr, i;

	DEBUG_PRINTF("PS2ESDL_EE_CORE: Patching ELF. Start offset: %p. Mode: 0x%02x.\n", buffer, patch->mode);

	if(patch->mode&BUILT_IN_PATCH){
		if((patch->mode&(~BUILT_IN_PATCH))==DELAYED_READ_PATCH){
		    	/* patchData[0] -> Offset relative to the current offset to patch. */

			DEBUG_PRINTF("PS2ESDL_EE_CORE: Applying delayed read patch.\n");
			ptr=scan_pattern(patch->src, patch->mask, (void *)0x000D0000, (void *)0x02000000);
			if(ptr!=NULL){
				DEBUG_PRINTF("PS2ESDL_EE_CORE: Patching index 0x%04x relative to %p.\n", patch->patchData[0], ptr);

				pCdRead=(void *)GETJADDR(ptr[patch->patchData[0]]);
				ptr[patch->patchData[0]]=JAL((u32)&delayed_cdRead);
			}
			else DEBUG_PRINTF("PS2ESDL_EE_CORE: Warning! Error looking for the section to patch.\n");

		}
		else if((patch->mode&(~BUILT_IN_PATCH))==BLOCKING_MODULE_PATCH){
			/* patchData[0] -> Offset relative to the current offset to patch.
			 * patchData[1] -> The nth module in the sequence of modules to patch.
			 */

			DEBUG_PRINTF("PS2ESDL_EE_CORE: Applying a patch that disables a blocking module.\n");

			ptr=(void *)0x000D0000;
			for(i=0; i<patch->patchData[1]; i++){
				ptr=scan_pattern(patch->src, patch->mask, ptr, (void *)0x02000000);
				if(ptr==NULL) break;
				ptr++; /* Increment ptr to begin scanning at the next offset. */
			}
			if(ptr!=NULL){
				ptr--; /* ptr was pre-incremented, so decrement it. */

				DEBUG_PRINTF("PS2ESDL_EE_CORE: Patching index 0x%04x relative to %p.\n", patch->patchData[0], ptr);

				LoadPackedModule=(void *)GETJADDR(ptr[patch->patchData[0]]);
				ptr[patch->patchData[0]]=JAL((u32)&PatchPackedModule);
			}
			else DEBUG_PRINTF("PS2ESDL_EE_CORE: Warning! Error looking for the section to patch.\n");

		}
	}
	else if(patch->mode&SEARCH_SCAN){

		DEBUG_PRINTF("PS2ESDL_EE_CORE: Scanning for the specified pattern...\n");

		ptr=buffer;
		do{
			ptr=scan_pattern(patch->src, patch->mask, ptr, (void *)0x02000000);
			if(ptr!=NULL){
				memcpy(ptr, patch->patchData, sizeof(patch->patchData));
				if(patch->mode&TERMINATE_ON_FIND) break;
				else ptr++; /* Increment ptr to begin scanning at the next offset. */
			}
			else break;
		}while(ptr<(u32 *)0x02000000);
	}
	else if(patch->mode==FIXED_PATCH){
		DEBUG_PRINTF("PS2ESDL_EE_CORE: Patching %p.\n", (void *)(buffer+patch->src[0]));
		*((u32*)(buffer+patch->src[0]))=patch->patchData[0]; /* FIXED_PATCH */
	}
	else{
		DEBUG_PRINTF("PS2ESDL_EE_CORE: Unsupported patch type: 0x%02x.\n", patch->mode);

	}

	DEBUG_PRINTF("PS2ESDL_EE_CORE: Completed patching operation.\n");
}

static void *scan_pattern(u32 *pattern, u32 *mask, void *start, void *end){
		u32 *ptr;

		for(ptr=start; ptr<(u32 *)end; ptr++){
			if(
				((ptr[0]&mask[0])==pattern[0])&&
				((ptr[1]&mask[1])==pattern[1])&&
				((ptr[2]&mask[2])==pattern[2])&&
				((ptr[3]&mask[3])==pattern[3])
			){
				DEBUG_PRINTF("PS2ESDL_EE_CORE: Pattern found at %p.\n", ptr);

				return((void *)ptr);
			}
		}

	return NULL;
}

static int delayed_cdRead(u32 lsn, u32 nSectors, void *buffer, u32 *mode)
{
	int result;
	u32 i;

	result=pCdRead(lsn, nSectors, buffer, mode);
	for(i=0; i<0x0100000; i++) __asm("\tnop\n\tnop\n\tnop\n\tnop\n");

	return(result);
}

/* Patches a single module before calling the game's function that loads the module (NOTE: The game must call the function that loads the packed module one at a time, and not using a loop). */
static int PatchPackedModule(void *moduleBuffer, int mod_index, int mod_argc, char *mod_argv[])
{
	void *iop_addr = *((void **)(moduleBuffer + (mod_index << 3) + 8));
	u32 opcode = 0x10000025;

	SyncDCache((void *)&opcode, (void *)(&opcode+sizeof(opcode)));
	smem_write((void *)(iop_addr+0x270), (void *)&opcode, sizeof(opcode));

	return(LoadPackedModule(moduleBuffer, mod_index, mod_argc, mod_argv));
}

