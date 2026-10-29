/* RAM staging shared by production and fault tests. No Flash writes here.
 * A BEGIN resets the bitmap; an invalid/conflicting segment poisons the block.
 * Only COMMIT checks CRC and permits the transport to program staged words. */
static unsigned long g_block_words[CAN_BOOT_BLOCK_BYTES / 4U];
static unsigned long g_block_offset;
static unsigned long g_block_crc;
static unsigned char g_block_length;
static unsigned char g_block_mask;
static unsigned char g_block_received;
static unsigned char g_block_bad;

static void BlockBegin(unsigned long offset, unsigned long crc,
                       unsigned long expected, unsigned long size)
{
    unsigned long remaining;
    g_block_bad = 1U;
    g_block_received = 0U;
    if ((offset & 63UL) || offset >= size || offset > expected ||
        expected - offset > 64UL) return;
    remaining = size - offset;
    g_block_length = (unsigned char)((remaining < 64UL) ? remaining : 64UL);
    g_block_mask = (unsigned char)(0xFFU >> (8U - ((g_block_length + 7U) / 8U)));
    g_block_offset = offset;
    g_block_crc = crc;
    g_block_bad = 0U;
}

static void BlockSegment(unsigned char index, unsigned char length,
                         const unsigned char *data)
{
    unsigned char pos;
    unsigned char count;
    unsigned char bit;
    unsigned char i;
    unsigned char *bytes;
    if (g_block_bad) return;
    pos = (unsigned char)(index * 8U);
    count = (unsigned char)(g_block_length - pos);
    if (count > 8U) count = 8U;
    if (index >= 8U || pos >= g_block_length || length != count)
    {
        g_block_bad = 1U;
        return;
    }
    bytes = (unsigned char *)g_block_words;
    bit = (unsigned char)(1U << index);
    for (i = 0U; i < count; i++)
    {
        if ((g_block_received & bit) && bytes[pos+i] != data[i]) g_block_bad = 1U;
        bytes[pos+i] = data[i];
    }
    g_block_received |= bit;
}

static unsigned char BlockCheck(unsigned long offset, unsigned long crc)
{
    if (g_block_bad || offset != g_block_offset || crc != g_block_crc ||
        g_block_received != g_block_mask) return CAN_BOOT_STATUS_BAD_SEQUENCE;
    if (F13_Crc32Bytes((const unsigned char __far *)g_block_words,
                       g_block_length) != crc) return CAN_BOOT_STATUS_CRC_ERROR;
    return CAN_BOOT_STATUS_OK;
}
