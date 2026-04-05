/*
 * ╔═══════════════════════════════════════╗
 * ║           HEX CORE v1.0               ║
 * ╚═══════════════════════════════════════╝
 *
 * A lightweight binary file viewer
 * Displays file contents in hex and ASCII format
 * 
 * Author  : Seungjun Kim
 * GitHub  : github.com/pyshit3, mokalover
 * License : MIT License (c) 2026
 *
 * Written during one of the harder chapters of my life.
 * If you're reading this, I made it through.
 * Better days are coming — I'm sure of it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned short WORD;
typedef unsigned int DWORD;

typedef struct _IMAGE_DOS_HEADER {
    WORD  e_magic;      // 0x00 - "MZ" signature (0x5A4D). Used to identify PE file.
    WORD  e_cblp;       // 0x02 - Bytes used in the last page. No longer used in modern PE.
    WORD  e_cp;         // 0x04 - Total number of pages in file. No longer used in modern PE.
    WORD  e_crlc;       // 0x06 - Number of relocation entries. No longer used in modern PE.
    WORD  e_cparhdr;    // 0x08 - Size of DOS header in paragraphs (1 paragraph = 16 bytes). No longer used in modern PE.
    WORD  e_minalloc;   // 0x0A - Minimum memory required to run program. No longer used in modern PE.
    WORD  e_maxalloc;   // 0x0C - Maximum memory required to run program. No longer used in modern PE.
    WORD  e_ss;         // 0x0E - Initial stack segment value for DOS program. No longer used in modern PE.
    WORD  e_sp;         // 0x10 - Initial stack pointer value for DOS program. No longer used in modern PE.
    WORD  e_csum;       // 0x12 - File checksum for integrity verification. Usually 0 in modern PE.
    WORD  e_ip;         // 0x14 - Initial instruction pointer value for DOS program. No longer used in modern PE.
    WORD  e_cs;         // 0x16 - Initial code segment value for DOS program. No longer used in modern PE.
    WORD  e_lfarlc;     // 0x18 - File offset of relocation table. No longer used in modern PE.
    WORD  e_ovno;       // 0x1A - Overlay number for loading large programs in segments. No longer used in modern PE.
    WORD  e_res[4];     // 0x1C - Reserved. 4 words = 8 bytes. Always 0.
    WORD  e_oemid;      // 0x24 - OEM identifier. Usually 0 in modern PE.
    WORD  e_oeminfo;    // 0x26 - OEM information. Usually 0 in modern PE.
    WORD  e_res2[10];   // 0x28 - Reserved. 10 words = 20 bytes. Always 0.
    DWORD e_lfanew;     // 0x3C - Most important field. Offset to PE header location in file.
} IMAGE_DOS_HEADER;

static char ASCII_CODE_TABLE[256];  // Byte value(0~255) to printable ASCII character table
static const char HEX_TABLE[] = "0123456789ABCDEF";  // Index(0~15) to hex character table

/* ─────────────────────────────────────────
 * Internal helpers
 * ───────────────────────────────────────── */

/* Copies a fixed-length string into out_buf at *pos, then advances *pos. */
static void write_str(char* out_buf, long* pos, const char* str, int len) {
    memcpy(out_buf + *pos, str, len);
    *pos += len;
}

/* Writes a 2-byte WORD as a 6-char hex string ("0xFFFF") into out_buf. */
static void write_word(char* out_buf, long* pos, WORD v) {
    out_buf[(*pos)++] = '0';
    out_buf[(*pos)++] = 'x';
    out_buf[(*pos)++] = HEX_TABLE[(v >> 12) & 0x0F];
    out_buf[(*pos)++] = HEX_TABLE[(v >>  8) & 0x0F];
    out_buf[(*pos)++] = HEX_TABLE[(v >>  4) & 0x0F];
    out_buf[(*pos)++] = HEX_TABLE[(v >>  0) & 0x0F];
}

/* Writes a 4-byte DWORD as a 10-char hex string ("0xFFFFFFFF") into out_buf. */
static void write_dword(char* out_buf, long* pos, DWORD v) {
    out_buf[(*pos)++] = '0';
    out_buf[(*pos)++] = 'x';
    out_buf[(*pos)++] = HEX_TABLE[(v >> 28) & 0x0F];
    out_buf[(*pos)++] = HEX_TABLE[(v >> 24) & 0x0F];
    out_buf[(*pos)++] = HEX_TABLE[(v >> 20) & 0x0F];
    out_buf[(*pos)++] = HEX_TABLE[(v >> 16) & 0x0F];
    out_buf[(*pos)++] = HEX_TABLE[(v >> 12) & 0x0F];
    out_buf[(*pos)++] = HEX_TABLE[(v >>  8) & 0x0F];
    out_buf[(*pos)++] = HEX_TABLE[(v >>  4) & 0x0F];
    out_buf[(*pos)++] = HEX_TABLE[(v >>  0) & 0x0F];
}

/*
 * Writes one formatted row into out_buf with the following columns:
 *   label | hex (WORD or DWORD) | decimal (10 chars) | ASCII (2 chars)
 * Columns are padded to fixed widths so all rows align perfectly.
 */
static void write_row(char* out_buf, long* pos, const char* label, int label_len,
                      unsigned long val, int is_dword) {
    char dec_buf[12];

    write_str(out_buf, pos, label, label_len);

    if (is_dword) {
        write_dword(out_buf, pos, (DWORD)val);
        write_str(out_buf, pos, "  ", 2);           // Pad to match WORD column width
    } else {
        write_word(out_buf, pos, (WORD)val);
        write_str(out_buf, pos, "      ", 6);       // Pad to match DWORD column width
    }

    sprintf(dec_buf, "%-10u", (unsigned int)val);   // Left-aligned decimal, 10 chars wide
    write_str(out_buf, pos, dec_buf, 10);

    // Low byte ASCII
    out_buf[(*pos)++] = (val & 0xFF) >= 32 && (val & 0xFF) <= 126
                        ? (char)(val & 0xFF) : '.';
    // High byte ASCII
    out_buf[(*pos)++] = ((val >> 8) & 0xFF) >= 32 && ((val >> 8) & 0xFF) <= 126
                        ? (char)((val >> 8) & 0xFF) : '.';
    out_buf[(*pos)++] = '\n';
}

/* ─────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────── */

/*
 * Initializes the ASCII lookup table.
 * Printable characters (0x20~0x7E) map to themselves.
 * All other byte values map to '.' for safe display.
 * Must be called once before any view functions.
 */
void initEnvironment() {
    memset(ASCII_CODE_TABLE, '.', 256);
    for (int i = 32; i <= 126; i++) {
        ASCII_CODE_TABLE[i] = (char)i;
    }
}

/*
 * Opens a binary file, reads its entire contents into a heap-allocated buffer,
 * and returns a pointer to that buffer. The caller is responsible for freeing
 * it via freeBuffer(). Returns NULL on any error.
 *
 *   fileName  - path to the file to open
 *   outSize   - receives the file size in bytes
 */
unsigned char* loadFile(const char* fileName, long* outSize) {
    FILE* fp = fopen(fileName, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) { fclose(fp); return NULL; }

    unsigned char* buf = (unsigned char*)malloc(size);
    if (!buf) { fclose(fp); return NULL; }

    if (fread(buf, 1, size, fp) != (size_t)size) {
        free(buf); fclose(fp); return NULL;
    }

    fclose(fp);
    *outSize = size;
    return buf;
}

/*
 * Converts the entire buffer into a hex string ("FF 0A 3C ...\n...").
 * Each byte becomes 3 characters ("XX "). A newline is inserted every 16 bytes.
 * out_buf must be at least size * 4 bytes.
 */
void getHexView(const unsigned char* buffer, long size, char* out_buf) {
    long pos = 0;
    for (long i = 0; i < size; i++) {
        unsigned char byte = buffer[i];
        out_buf[pos++] = HEX_TABLE[byte >> 4];     // Upper 4 bits
        out_buf[pos++] = HEX_TABLE[byte & 0x0F];   // Lower 4 bits
        out_buf[pos++] = ' ';
        if (i % 16 == 15) out_buf[pos++] = '\n';   // New line every 16 bytes
    }
    out_buf[pos] = '\0';
}

/*
 * Converts the entire buffer into a printable ASCII string.
 * Non-printable bytes are replaced with '.'.
 * A newline is inserted every 16 bytes.
 * out_buf must be at least size * 2 bytes.
 */
void getAsciiView(const unsigned char* buffer, long size, char* out_buf) {
    long pos = 0;
    for (long i = 0; i < size; i++) {
        out_buf[pos++] = ASCII_CODE_TABLE[buffer[i]];
        if (i % 16 == 15) out_buf[pos++] = '\n';   // New line every 16 bytes
    }
    out_buf[pos] = '\0';
}

/*
 * Generates a column of 8-digit hex offsets ("00000000\n00000010\n...").
 * One offset is emitted per 16-byte row, matching getHexView line count.
 * out_buf must be at least ((size + 15) / 16) * 9 bytes.
 */
void getOffsetView(long size, char* out_buf) {
    long pos = 0;
    for (long i = 0; i < size; i += 16) {
        unsigned long addr = i;
        out_buf[pos++] = HEX_TABLE[(addr >> 28) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >> 24) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >> 20) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >> 16) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >> 12) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >>  8) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >>  4) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >>  0) & 0x0F];
        out_buf[pos++] = '\n';
    }
    out_buf[pos] = '\0';
}

/*
 * Lazy-load variant of getHexView.
 * Converts only chunk_size bytes starting at offset into a hex string.
 * offset is automatically aligned down to the nearest 16-byte boundary.
 * out_buf must be at least chunk_size * 4 bytes.
 *
 *   buffer     - full file buffer
 *   size       - total file size
 *   offset     - byte offset to start from (aligned to 16)
 *   chunk_size - number of bytes to convert
 */
void getHexViewChunk(const unsigned char* buffer, long size,
                     long offset, long chunk_size, char* out_buf) {
    long pos   = 0;
    long start = (offset / 16) * 16;    // Align to 16-byte boundary
    long end   = start + chunk_size;
    if (end > size) end = size;

    for (long i = start; i < end; i++) {
        unsigned char byte = buffer[i];
        out_buf[pos++] = HEX_TABLE[byte >> 4];
        out_buf[pos++] = HEX_TABLE[byte & 0x0F];
        out_buf[pos++] = ' ';
        if (i % 16 == 15) out_buf[pos++] = '\n';
    }
    out_buf[pos] = '\0';
}

/*
 * Lazy-load variant of getAsciiView.
 * Converts only chunk_size bytes starting at offset into a printable ASCII string.
 * offset is automatically aligned down to the nearest 16-byte boundary.
 * out_buf must be at least chunk_size * 2 bytes.
 *
 *   buffer     - full file buffer
 *   size       - total file size
 *   offset     - byte offset to start from (aligned to 16)
 *   chunk_size - number of bytes to convert
 */
void getAsciiViewChunk(const unsigned char* buffer, long size,
                       long offset, long chunk_size, char* out_buf) {
    long pos   = 0;
    long start = (offset / 16) * 16;    // Align to 16-byte boundary
    long end   = start + chunk_size;
    if (end > size) end = size;

    for (long i = start; i < end; i++) {
        out_buf[pos++] = ASCII_CODE_TABLE[buffer[i]];
        if (i % 16 == 15) out_buf[pos++] = '\n';
    }
    out_buf[pos] = '\0';
}

/*
 * Lazy-load variant of getOffsetView.
 * Generates offset labels only for the rows within the given chunk range.
 * offset is automatically aligned down to the nearest 16-byte boundary.
 * out_buf must be at least (chunk_size / 16 + 1) * 9 bytes.
 *
 *   size       - total file size
 *   offset     - byte offset to start from (aligned to 16)
 *   chunk_size - number of bytes to cover
 */
void getOffsetViewChunk(long size, long offset, long chunk_size, char* out_buf) {
    long pos   = 0;
    long start = (offset / 16) * 16;    // Align to 16-byte boundary
    long end   = start + chunk_size;
    if (end > size) end = size;

    for (long i = start; i < end; i += 16) {
        unsigned long addr = i;
        out_buf[pos++] = HEX_TABLE[(addr >> 28) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >> 24) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >> 20) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >> 16) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >> 12) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >>  8) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >>  4) & 0x0F];
        out_buf[pos++] = HEX_TABLE[(addr >>  0) & 0x0F];
        out_buf[pos++] = '\n';
    }
    out_buf[pos] = '\0';
}

/*
 * Parses the IMAGE_DOS_HEADER from buf and writes a formatted table into out_buf.
 * Each row shows: field name | hex value | decimal value | ASCII representation.
 * Returns an error message if the file is too small or not a valid PE (no MZ signature).
 * out_buf must be at least 1024 bytes.
 */
void parseDosHeader(const unsigned char* buf, long size, char* out_buf) {
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)buf;
    long pos = 0;

    if (size < sizeof(IMAGE_DOS_HEADER)) {
        write_str(out_buf, &pos, "File too small\n", 15);
        out_buf[pos] = '\0';
        return;
    }

    if (dosHeader->e_magic != 0x5A4D) {
        write_str(out_buf, &pos, "Not a valid PE file\n", 20);
        out_buf[pos] = '\0';
        return;
    }

    write_str(out_buf, &pos, "Field          Hex        Dec      ASCII\n", 41);
    write_str(out_buf, &pos, "-----------------------------------------\n", 42);

    write_row(out_buf, &pos, "e_magic   : ", 12, dosHeader->e_magic,    0);
    write_row(out_buf, &pos, "e_cblp    : ", 12, dosHeader->e_cblp,     0);
    write_row(out_buf, &pos, "e_cp      : ", 12, dosHeader->e_cp,       0);
    write_row(out_buf, &pos, "e_crlc    : ", 12, dosHeader->e_crlc,     0);
    write_row(out_buf, &pos, "e_cparhdr : ", 12, dosHeader->e_cparhdr,  0);
    write_row(out_buf, &pos, "e_minalloc: ", 12, dosHeader->e_minalloc, 0);
    write_row(out_buf, &pos, "e_maxalloc: ", 12, dosHeader->e_maxalloc, 0);
    write_row(out_buf, &pos, "e_ss      : ", 12, dosHeader->e_ss,       0);
    write_row(out_buf, &pos, "e_sp      : ", 12, dosHeader->e_sp,       0);
    write_row(out_buf, &pos, "e_csum    : ", 12, dosHeader->e_csum,     0);
    write_row(out_buf, &pos, "e_ip      : ", 12, dosHeader->e_ip,       0);
    write_row(out_buf, &pos, "e_cs      : ", 12, dosHeader->e_cs,       0);
    write_row(out_buf, &pos, "e_lfarlc  : ", 12, dosHeader->e_lfarlc,   0);
    write_row(out_buf, &pos, "e_ovno    : ", 12, dosHeader->e_ovno,     0);
    write_row(out_buf, &pos, "e_oemid   : ", 12, dosHeader->e_oemid,    0);
    write_row(out_buf, &pos, "e_oeminfo : ", 12, dosHeader->e_oeminfo,  0);
    write_row(out_buf, &pos, "e_lfanew  : ", 12, dosHeader->e_lfanew,   1);

    out_buf[pos] = '\0';
}


/*
 * Parses and dumps the DOS Stub Program from a PE file.
 *
 * The DOS Stub is a small x86 real-mode program located between the
 * DOS header and the PE header. When the PE file is executed in a DOS
 * environment, this stub runs and prints:
 *   "This program cannot be run in DOS mode."
 *
 * Memory layout:
 *   [0x00 ~ 0x3F]              IMAGE_DOS_HEADER  (64 bytes)
 *   [0x40 ~ e_lfanew - 1]      DOS Stub          (this function parses here)
 *   [e_lfanew ~ ...]           PE Header
 *
 * Output format:
 *   DOS Stub
 *   -----------------------------------------
 *   Start  : 0x00000040
 *   End    : 0x000000E8
 *   Size   : 0x000000A8
 *   -----------------------------------------
 *   00000040  0E 1F BA 0E 00 B4 09 CD  ........
 *   ...
 *
 * Parameters:
 *   buf     - full file buffer loaded by loadFile()
 *   size    - total file size in bytes
 *   out_buf - output buffer to write the formatted result into
 *             must be at least (stub_size * 5 + 256) bytes
 */
void parseDosStub(const unsigned char* buf, long size, char* out_buf) {
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)buf;
    long pos = 0;

    if (size < sizeof(IMAGE_DOS_HEADER)) {
        write_str(out_buf, &pos, "File is too small\n", 18);
        out_buf[pos] = '\0';
        return;
    }

    if (dosHeader->e_magic != 0x5A4D) {
        write_str(out_buf, &pos, "Not a valid PE file\n", 20);
        out_buf[pos] = '\0';
        return;
    }

    // DOS Stub starts right after DOS header (0x40)
    // and ends just before PE header (e_lfanew)
    long stub_start = sizeof(IMAGE_DOS_HEADER); // 0x40
    long stub_end = (long)dosHeader->e_lfanew; // e_lfanew
    long stub_size = stub_end - stub_start;

    if (stub_size <= 0) {
        write_str(out_buf, &pos, "No DOS Stub found\n", 18);
        out_buf[pos] = '\0';
        return;
    }

    // Print stub info
    write_str(out_buf, &pos, "DOS Stub\n", 9);
    write_str(out_buf, &pos, "-----------------------------------------\n", 42);
    write_str(out_buf, &pos, "Start  : ", 9); write_dword(out_buf, &pos, stub_start); out_buf[pos++] = '\n';
    write_str(out_buf, &pos, "End    : ", 9); write_dword(out_buf, &pos, stub_end);   out_buf[pos++] = '\n';
    write_str(out_buf, &pos, "Size   : ", 9); write_dword(out_buf, &pos, stub_size);  out_buf[pos++] = '\n';
    write_str(out_buf, &pos, "-----------------------------------------\n", 42);

    const unsigned char* stub = buf + stub_start;
    for (long i = 0; i < stub_size; i++){
        // Offset
        if (i % 16 ==0){
            unsigned long addr = stub_start + i;
            out_buf[pos++] = HEX_TABLE[(addr >> 28) & 0x0F];
            out_buf[pos++] = HEX_TABLE[(addr >> 24) & 0x0F];
            out_buf[pos++] = HEX_TABLE[(addr >> 20) & 0x0F];
            out_buf[pos++] = HEX_TABLE[(addr >> 16) & 0x0F];
            out_buf[pos++] = HEX_TABLE[(addr >> 12) & 0x0F];
            out_buf[pos++] = HEX_TABLE[(addr >>  8) & 0x0F];
            out_buf[pos++] = HEX_TABLE[(addr >>  4) & 0x0F];
            out_buf[pos++] = HEX_TABLE[(addr >>  0) & 0x0F];
            out_buf[pos++] = ' ';
            out_buf[pos++] = ' ';
        }

        // Hex
        out_buf[pos++] = HEX_TABLE[stub[i] >> 4];
        out_buf[pos++] = HEX_TABLE[stub[i] & 0x0F];
        out_buf[pos++] = ' ';

        // ASCII at end of each row
        if (i % 16 == 15) {
            out_buf[pos++] = ' ';
            out_buf[pos++] = ' ';
            for (long j = i - 15; j <= i; j++) {
                out_buf[pos++] = ASCII_CODE_TABLE[stub[j]];
            }
            out_buf[pos++] = '\n';
        }
    }

    // Handle last incomplete row
    long remaining = stub_size % 16;
    if (remaining != 0) {
        // Pad hex columns
        for (long i = 0; i < (16 - remaining); i++) {
            out_buf[pos++] = ' ';
            out_buf[pos++] = ' ';
            out_buf[pos++] = ' ';
        }
        // ASCII
        out_buf[pos++] = ' ';
        out_buf[pos++] = ' ';
        for (long i = stub_size - remaining; i < stub_size; i++) {
            out_buf[pos++] = ASCII_CODE_TABLE[stub[i]];
        }
        out_buf[pos++] = '\n';
    }

    out_buf[pos] = '\0';
}

/*
 * Frees the buffer allocated by loadFile.
 * Must be called when the buffer is no longer needed to prevent memory leaks.
 */
void freeBuffer(unsigned char* buf) {
    free(buf);
}