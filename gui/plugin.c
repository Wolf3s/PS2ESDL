#include <loadfile.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <fileio.h>

#include "include/plugin.h"

#include "../modules/USBD_irx.c"
#include "../modules/USBHDFSD_irx.c"

unsigned int nDeviceDrivers=BUILT_IN_DRIVERS;
struct DeviceDriver Devices[MAX_DEVICES];

extern unsigned char UDNL_irx[];
extern unsigned int size_UDNL_irx;

void InitBuiltInDrivers(void){
	/* Initialize the built in USB drivers. */
	strcpy(Devices[0].DevName, "mass");
	strcpy(Devices[0].DevDispName, "USB Mass Storage Device");
	Devices[0].DriverType=DRIVER_TYPE_BUILT_IN;
	Devices[0].UDNL_module.Size=size_UDNL_irx;
	Devices[0].UDNL_module.Module=UDNL_irx;

	Devices[0].nModules=2;
	Devices[0].Modules=malloc(sizeof(struct ModListEntry)*Devices[0].nModules);
	Devices[0].Modules[0].Size=size_USBD_irx;
	Devices[0].Modules[0].Module=USBD_irx;
	Devices[0].Modules[1].Size=size_USBHDFSD_irx;
	Devices[0].Modules[1].Module=USBHDFSD_irx;
}

int LoadPlugins(char *CWD){
	struct PluginHeader PluginHeader;
	struct PluginHeader SamplePluginHeader={
		"PPLG",
		PLUGIN_VERSION
	};
	struct PluginData PluginData;
	unsigned int i, nPluginsLoaded;
	int PluginFd;
	char *PathToPlugin;

	nPluginsLoaded=0;

	for(i=0; i<3; i++){
		PathToPlugin=malloc(strlen(CWD)+15);	/* Allocate sufficient space for "extension0.plg" */
		sprintf(PathToPlugin, "%sextension%d.plg", CWD, i);

		if((PluginFd=fioOpen(PathToPlugin, O_RDONLY))<0) continue;

		memset(&PluginHeader, 0, sizeof(struct PluginHeader));
		fioRead(PluginFd, &PluginHeader, sizeof(struct PluginHeader));

		if(memcmp(&PluginHeader, &SamplePluginHeader, sizeof(struct PluginHeader))!=0){
			fioClose(PluginFd);
			continue;
		}

		/* Read in the basic information of the plugin. */
		fioRead(PluginFd, &PluginData, sizeof(struct PluginData));

		Devices[nDeviceDrivers].nModules=PluginData.nModules;
		Devices[nDeviceDrivers].Modules=malloc(PluginData.nModules*sizeof(struct ModListEntry));
		memset(Devices[nDeviceDrivers].Modules, 0, sizeof(PluginData.nModules*sizeof(struct ModListEntry)));
		memcpy(Devices[nDeviceDrivers].DevName, PluginData.DevName, sizeof(PluginData.DevName));
		memcpy(Devices[nDeviceDrivers].DevDispName, PluginData.DevDisplayName, sizeof(PluginData.DevDisplayName));
		Devices[nDeviceDrivers].DriverType=DRIVER_TYPE_PLUGIN;

		/* Read in the sizes of all modules (Excluding UDNL). */
		for(i=0; i<PluginData.nModules; i++){
			fioRead(PluginFd, &Devices[nDeviceDrivers].Modules[i].Size, sizeof(Devices[nDeviceDrivers].Modules[i].Size));
		}

		/* Read in UDNL. */
		Devices[nDeviceDrivers].UDNL_module.Size=PluginData.SizeOfUDNL;
		Devices[nDeviceDrivers].UDNL_module.Module=malloc(PluginData.SizeOfUDNL);

		fioRead(PluginFd, Devices[nDeviceDrivers].UDNL_module.Module, PluginData.SizeOfUDNL);

		/* Read in all remaining modules. */
		for(i=0; i<PluginData.nModules; i++){
			Devices[nDeviceDrivers].Modules[i].Module=malloc(Devices[nDeviceDrivers].Modules[i].Size);
			fioRead(PluginFd, Devices[nDeviceDrivers].Modules[i].Module, Devices[nDeviceDrivers].Modules[i].Size);
		}

		fioClose(PluginFd);

		nPluginsLoaded++;
		nDeviceDrivers++;
	}

	free(PathToPlugin);

	return(nPluginsLoaded);
}

void FreeAllDrivers(void){
	unsigned int i;

	for(i=0; i<nDeviceDrivers; i++){
		if(Devices[i].DriverType!=DRIVER_TYPE_BUILT_IN){
			if(Devices[i].UDNL_module.Module!=NULL){
				free(Devices[i].UDNL_module.Module);
				Devices[i].UDNL_module.Module=NULL;
			}
		}
	}
}

int ExecFreeDrivers(struct DeviceDriver *Device){
	unsigned int i;

	/* Load modules. */
	for(i=0; i<Device->nModules; i++){
		SifExecModuleBuffer(Device->Modules[i].Module, Device->Modules[i].Size, 0, NULL, NULL);
		if(Device->DriverType!=DRIVER_TYPE_BUILT_IN){
			free(Device->Modules[i].Module);
			Device->Modules[i].Module=NULL;
		}
	}
	free(Device->Modules);	/* Free the memory allocated for this device driver list. */

	return 1;
};

int LoadUDNL(struct DeviceDriver *Device, void **buffer){
	*buffer=Device->UDNL_module.Module;
	return Device->UDNL_module.Size;
}
