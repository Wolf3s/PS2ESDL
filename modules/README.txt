The structure of how CDVDMAN, CDVDFSV and EESYNC are loaded has been changed.

Previously (up to around v0.823), simply changing the source code and recompiling PS2ESDL was sufficient.
Now, CDVDMAN, CDVDFSV and EESYNC have to be recompiled if necessary, and rebuilt into an IOPRP image that gets placed as ioprp.img in udnl.

1. Compile CDVDMAN, CDVDFSV and/or EESYNC.
2. Create an IOPRP image (I used romimg for this) with these 3 modules, with cdvdman.irx as CDVDMAN, cdvdfsv.irx as CDVDFSV and eesync.irx as EESYNC.
3. Place the created IOPRP image in udnl's source code folder as ioprp.img and rebuild udnl.
4. Rebuild UDNL and generate UDNL_irx.c with bin2c: bin2c udnl.irx UDNL_irx.c UDNL_irx.
5. Place UDNL_irx.c under the gui folder, overwriting the old UDNL_irx.c file.