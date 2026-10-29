/* Read-only installed-image check. No erase/write callback or update state. */
static unsigned char VerifyImage(const unsigned long __far *image,
                                 unsigned long *size, unsigned long *crc)
{
    const unsigned long __far *metadata;
    metadata=image+IMAGE_MANIFEST_OFFSET/4UL;
    *size=0UL;
    *crc=0UL;
    if (!MetadataValid(metadata)) return g_metadata_error;
    *size=metadata[6];
    *crc=F13_Crc32Bytes((const unsigned char __far *)image,*size-4UL);
    return (*crc == image[*size/4UL-1UL]) ?
        CAN_BOOT_STATUS_OK : CAN_BOOT_STATUS_CRC_ERROR;
}
