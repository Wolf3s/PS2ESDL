#define PS2ESDL_PATCH_VERSION 0x100

struct PS2ESDL_ptch_conf_test_hdr{	/* It's the same structure as the one below, but only containing the signature and version fields; This is for validating whether the file is a valud patch file. */
	unsigned char signature[4];	/* "PPI" + a padding field (A 0x00 byte). */
	unsigned short int version;
};

struct PS2ESDL_ptch_conf_hdr{
	unsigned char signature[4];	/* "PPI" + a padding field (A 0x00 byte). */
	unsigned short int version;

	/* File structure layout fields */
	u32 patches;		/* Number of game patches listed. */
	u32 gamesListed;	/* Number of games that require patching listed. */

	/* The section starting from here is the section where patches are listed,
	followed by a section that lists games that need patches and the type of patch they require. */

} __attribute__((packed));

/* A structure that defines the way a game that requires patching should be patched, and the type of patch it requires. */
struct PS2ESDL_gameToBePatched{
	u8	disc_ID[16];	/* SLXX-12389 + padding with 0s. */
	u32	patch_ID;	/* The ID of the patch that should be applied. */
	u32	mode;		/* How the patch should be applied. */
} __attribute__((packed));

/* A structure that defines a patch type. */
struct PS2ESDL_patchType{
	u32 src[4];		/* Offset (Only the 1st element is used, the other 3 are ignored)/sample of the original data that needs to be patched, and is to be search for. */
	u32 mask[4];		/* Source pattern Mask (Optional; If unneeded, just fill it with 0xFF bytes). */
	u32 patchData[4];	/* Data that is to replace the original data in the game's ELF that is loaded into memory (If this patch section belongs to a built-in patch, then the contents of this field depends on the patch itself). */
} __attribute__((packed));


