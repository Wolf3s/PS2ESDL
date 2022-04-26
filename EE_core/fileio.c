/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (C)2001, Gustavo Scotti (gustavo@scotti.com)
# (c) 2003 Marcus R. Brown (mrbrown@0xd6.org)
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
# EE FILE IO handling
*/

#include <tamtypes.h>
#include <ps2lib_err.h>
#include <kernel.h>
#include <sifrpc.h>
#include <fileio.h>
#include <string.h>

enum _fio_functions {
	FIO_F_OPEN = 0,
	FIO_F_CLOSE,
	FIO_F_READ,
	FIO_F_WRITE,
	FIO_F_LSEEK,
	FIO_F_IOCTL,
	FIO_F_REMOVE,
	FIO_F_MKDIR,
	FIO_F_RMDIR,
	FIO_F_DOPEN,
	FIO_F_DCLOSE,
	FIO_F_DREAD,
	FIO_F_GETSTAT,
	FIO_F_CHSTAT,
	FIO_F_FORMAT,
	FIO_F_ADDDRV,
	FIO_F_DELDRV
};

/* Shared between _fio_read_intr and fio_read.  The updated modules shipped
   with licensed games changed the size of the buffers from 16 to 64.  */
struct _fio_read_data {
	u32	size1;
	u32	size2;
	void	*dest1;
	void	*dest2;
	u8	buf1[16];
	u8	buf2[16];
};

static void _fio_read_intr(void *);

static SifRpcClientData_t _fio_cd;
static int _fio_recv_data[512] __attribute__((aligned(64)));
static int _fio_intr_data[32] __attribute__((aligned(64)));
static int _fio_init = 0;
static int _fio_block_mode;

int fioInit(void)
{
	int res;

	while (((res = SifBindRpc(&_fio_cd, 0x80000001, 0)) >= 0) &&
			(_fio_cd.server == NULL))
		nopdelay();

	if(res < 0)
		return res;

	_fio_init = 1;

	return 0;
}

void fioExit(void)
{
	_fio_init = 0;
	memset(&_fio_cd, 0, sizeof _fio_cd);
}

struct _fio_open_arg {
	int	mode;
	char 	name[FIO_PATH_MAX];
} ALIGNED(16);

int fioOpen(const char *name, int mode)
{
	struct _fio_open_arg arg;
	int res;

	arg.mode = mode;
	strncpy(arg.name, name, FIO_PATH_MAX - 1);
	arg.name[FIO_PATH_MAX - 1] = 0;

	if((res=SifCallRpc(&_fio_cd, FIO_F_OPEN, _fio_block_mode, &arg, sizeof arg, _fio_recv_data, 4, NULL, NULL))<0) return res;

	return _fio_recv_data[0];
}

int fioClose(int fd)
{
	union { int fd; int result; } arg;
	int res;

	arg.fd = fd;
	return(((res=SifCallRpc(&_fio_cd, FIO_F_CLOSE, 0, &arg, 4, &arg, 4, NULL, NULL))>=0)?arg.result:res);
}

static void _fio_read_intr(void *data)
{
	struct _fio_read_data *uncached = UNCACHED_SEG(data);

	if(uncached->size1 && uncached->dest1!=NULL) memcpy(UNCACHED_SEG(uncached->dest1), uncached->buf1, uncached->size1);
	if(uncached->size2 && uncached->dest2!=NULL) memcpy(UNCACHED_SEG(uncached->dest2), uncached->buf2, uncached->size2);
}

struct _fio_read_arg {
	int	fd;
	void	*ptr;
	int	size;
	struct _fio_read_data *read_data;
} ALIGNED(16);

int fioRead(int fd, void *ptr, int size)
{
	struct _fio_read_arg arg;
	int res;

	arg.fd      = fd;
	arg.ptr       = ptr;
	arg.size      = size;
	arg.read_data = (struct _fio_read_data *)_fio_intr_data;

	if(!IS_UNCACHED_SEG(ptr)) SifWriteBackDCache(ptr, size);

	if((res=SifCallRpc(&_fio_cd, FIO_F_READ, _fio_block_mode, &arg, sizeof arg, _fio_recv_data, 4, &_fio_read_intr, _fio_intr_data))<0) return res;

	return _fio_recv_data[0];
}

struct _fio_write_arg {
	int	fd;
	const void	*ptr;
	u32	size;
	u32	mis;
	u8	aligned[16];
} ALIGNED(16);

int fioWrite(int fd, const void *ptr, int size)
{
	struct _fio_write_arg arg;
	int mis, res, result;

	arg.fd = fd;
	arg.ptr  = ptr;
	arg.size = size;

	/* Copy the unaligned (16-byte) portion into the argument */
	mis = 0;
	if ((u32)ptr & 0xf) {
		mis = 16 - ((u32)ptr & 0xf);
		if (mis > size)
			mis = size;
	}
	arg.mis = mis;

	if (mis)
		memcpy(arg.aligned, ptr, mis);

	if (!IS_UNCACHED_SEG(ptr)) SifWriteBackDCache((struct fileXioDirEntry *)ptr, size);

	if ((res = SifCallRpc(&_fio_cd, FIO_F_WRITE, 0, &arg, sizeof arg,
					_fio_recv_data, 4, NULL, NULL)) >= 0)
	{
		result=_fio_recv_data[0];
	}
	else
	{
		result=res;
	}

	return result;
}

struct _fio_lseek_arg {
	union {
		int	fd;
		int	result;
	} p;
	int	offset;
	int	whence;
} ALIGNED(16);

int fioLseek(int fd, int offset, int whence)
{
	struct _fio_lseek_arg arg;
	int res;

	arg.p.fd   = fd;
	arg.offset = offset;
	arg.whence = whence;

	return(((res=SifCallRpc(&_fio_cd, FIO_F_LSEEK, 0, &arg, sizeof arg, &arg, 4, NULL, NULL))>=0)?arg.p.result:res);
}

