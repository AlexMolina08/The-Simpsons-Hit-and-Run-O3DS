#!/usr/bin/env python3
"""Add a bounded-memory unencrypted CIA build path to pinned makerom sources.

The browser mounts /output/content.ncch on OPFS before calling makerom. All
NCCH metadata, IVFC layout, signatures and CIA metadata retain upstream code.
Only the large RomFS payload and final content hash/copy use bounded file I/O.
"""
from pathlib import Path
import argparse

HEADER = r'''#pragma once
#include "oschar.h"
#ifdef SRR_BROWSER_STREAMING
void SrrStreamConfigure(bool enabled);
bool SrrStreamRequested(void);
bool SrrStreamActive(void);
int SrrStreamBegin(u64 total, u64 prefix, u64 romfsOffset);
int SrrStreamFinishNcch(const u8 *prefix);
int SrrStreamHash(u64 offset, u64 size, u8 hash[32]);
int SrrStreamCopy(FILE *output, u64 offset, u64 size);
void SrrStreamClose(void);
int SrrBuildRomFs(romfs_buildctx *ctx);
void SrrStreamFile(const oschar_t *path, u64 offset, u64 size);
#endif
'''
# romfs_buildctx is not visible to every include site; its entry point is
# declared only by romfs_gen.c after romfs.h, keeping the other ABI untouched.
HEADER = HEADER.replace('int SrrBuildRomFs(romfs_buildctx *ctx);\n', '')
SOURCE = r'''/* Bounded browser packaging adapter for makerom (MIT licensed upstream).
 * Existing ROMFS metadata/order, IVFC layout and NCCH/CIA signing are retained.
 * Browser builds enable this only for unencrypted, single-content CIA output.
 */
#include "lib.h"
#include "ncch_build.h"
#include "romfs.h"
#include "stream_package.h"
#ifdef SRR_BROWSER_STREAMING
#include <mbedtls/sha256.h>

#define SRR_CHUNK (256u * 1024u)
#ifndef SRR_STREAM_FILE
#define SRR_STREAM_FILE "/output/content.ncch"
#endif
static bool requested;
static FILE *content;
static u64 totalBytes, prefixBytes, romfsBase;
static int failed;
static u8 *transfer;

static int fail(const char *message)
{
    if (!failed) fprintf(stderr,"[STREAM ERROR] %s\n", message);
    failed = -1;
    return -1;
}
static int seek(FILE *file, u64 offset)
{
    return fseek_64(file, offset) == 0 ? 0 : fail("File seek failed");
}
static int write_at(u64 offset, const void *data, size_t size)
{
    if (failed || seek(content, offset)) return -1;
    return fwrite(data, 1, size, content) == size ? 0 : fail("Scratch write failed");
}
void SrrStreamClose(void)
{
    if (content) fclose(content);
    content = NULL;
    free(transfer); transfer = NULL;
    requested = false; failed = 0;
}
void SrrStreamConfigure(bool enabled)
{
    SrrStreamClose(); requested = enabled;
}
bool SrrStreamRequested(void) { return requested; }
bool SrrStreamActive(void) { return content != NULL; }
int SrrStreamBegin(u64 total, u64 prefix, u64 romfsOffset)
{
    if (!total || prefix > total || romfsOffset > prefix) return fail("Invalid content sizes");
    totalBytes = total; prefixBytes = prefix; romfsBase = romfsOffset;
    content = fopen(SRR_STREAM_FILE, "w+b");
    if (!content) return fail("Cannot open disk-backed /output/content.ncch");
    transfer = malloc(SRR_CHUNK);
    if (!transfer) return fail("Cannot allocate bounded copy buffer");
    /* Sparse unwritten alignment gaps read as zero, matching upstream calloc. */
    const u8 zero = 0;
    return total ? write_at(total - 1, &zero, 1) : fail("Empty content");
}
int SrrStreamFinishNcch(const u8 *prefix)
{
    if (write_at(0, prefix, prefixBytes)) return -1;
    return fflush(content) ? fail("Scratch flush failed") : 0;
}
void SrrStreamFile(const oschar_t *path, u64 offset, u64 size)
{
    if (failed) return;
    if (offset > totalBytes || size > totalBytes - offset) { fail("Asset exceeds content bounds"); return; }
    FILE *input = os_fopen(path, OS_MODE_READ);
    if (!input) { fail("Cannot open asset"); return; }
    if (seek(content, offset)) { fclose(input); return; }
    while (size && !failed) {
        const size_t amount = size > SRR_CHUNK ? SRR_CHUNK : (size_t)size;
        if (fread(transfer, 1, amount, input) != amount) { fail("Asset read failed"); break; }
        if (fwrite(transfer, 1, amount, content) != amount) { fail("Asset write failed"); break; }
        size -= amount;
    }
    fclose(input);
}
int SrrStreamHash(u64 offset, u64 size, u8 hash[32])
{
    if (offset > totalBytes || size > totalBytes - offset) return fail("Hash exceeds content bounds");
    if (failed || fflush(content) || seek(content, offset)) return fail("Cannot read content for hashing");
    mbedtls_sha256_context state;
    mbedtls_sha256_init(&state);
    mbedtls_sha256_starts(&state, 0);
    while (size && !failed) {
        const size_t amount = size > SRR_CHUNK ? SRR_CHUNK : (size_t)size;
        if (fread(transfer, 1, amount, content) != amount) { fail("Content hash read failed"); break; }
        mbedtls_sha256_update(&state, transfer, amount);
        size -= amount;
    }
    if (!failed) mbedtls_sha256_finish(&state, hash);
    mbedtls_sha256_free(&state);
    return failed;
}
int SrrStreamCopy(FILE *output, u64 offset, u64 size)
{
    if (failed || size != totalBytes || seek(content, 0) || seek(output, offset)) return fail("Invalid content copy");
    while (size && !failed) {
        const size_t amount = size > SRR_CHUNK ? SRR_CHUNK : (size_t)size;
        if (fread(transfer, 1, amount, content) != amount) { fail("Content copy read failed"); break; }
        if (fwrite(transfer, 1, amount, output) != amount) { fail("Content copy write failed"); break; }
        size -= amount;
    }
    return failed;
}

extern void BuildRomfsHeader(romfs_buildctx *ctx);
extern void PopulateRomfs(romfs_buildctx *ctx);
extern void BuildIvfcHeader(romfs_buildctx *ctx);

int SrrBuildRomFs(romfs_buildctx *ctx)
{
    const u32 block = 0x1000;
    u8 *levels[4] = {NULL, NULL, NULL, NULL};
    const u64 metadata = align(sizeof(romfs_infoheader) + ctx->m_dirHashTable * 4 +
        ctx->m_dirTableLen + ctx->m_fileHashTable * 4 + ctx->m_fileTableLen, 0x10);
    ctx->level[0].offset = 0;
    ctx->level[3].offset = align(ctx->level[0].size, block);
    ctx->level[1].offset = ctx->level[3].offset + align(ctx->level[3].size, block);
    ctx->level[2].offset = ctx->level[1].offset + align(ctx->level[1].size, block);
    for (int i = 1; i < 4; ++i)
        ctx->level[i].logicalOffset = i == 1 ? 0 : align(ctx->level[i-1].logicalOffset + ctx->level[i-1].size, block);
    for (int i = 0; i < 4; ++i) {
        const u64 bytes = i == 3 ? metadata : align(ctx->level[i].size, block);
        levels[i] = calloc(1, bytes);
        if (!levels[i]) { fail("Cannot allocate IVFC metadata"); goto done; }
        ctx->level[i].pos = levels[i] + (i == 0 ? align(sizeof(ivfc_hdr), 0x10) : 0);
    }
    ctx->romfsHdr = (romfs_infoheader *)levels[3];
    BuildRomfsHeader(ctx);
    PopulateRomfs(ctx);
    if (failed || write_at(romfsBase + ctx->level[3].offset, levels[3], metadata)) goto done;
    if (fflush(content)) { fail("Cannot flush RomFS assets"); goto done; }
    /* Hash level 3 in bounded sequential batches. Padding remains zero. */
    const u64 dataBytes = align(ctx->level[3].size, block);
    if (seek(content, romfsBase + ctx->level[3].offset)) goto done;
    for (u64 offset = 0; offset < dataBytes && !failed; offset += SRR_CHUNK) {
        const size_t amount = dataBytes - offset > SRR_CHUNK ? SRR_CHUNK : (size_t)(dataBytes - offset);
        if (fread(transfer, 1, amount, content) != amount) { fail("RomFS hash read failed"); break; }
        for (size_t at = 0; at < amount; at += block)
            ShaCalc(transfer + at, block, ctx->level[2].pos + ((offset + at) / block) * 32, CTR_SHA_256);
    }
    for (int level = 1; level >= 0 && !failed; --level) {
        const u64 hashes = align(ctx->level[level+1].size, block) / block;
        for (u64 index = 0; index < hashes; ++index)
            ShaCalc(ctx->level[level+1].pos + index * block, block, ctx->level[level].pos + index * 32, CTR_SHA_256);
    }
    ctx->ivfcHdr = (ivfc_hdr *)levels[0];
    BuildIvfcHeader(ctx);
    for (int level = 0; level < 3 && !failed; ++level)
        write_at(romfsBase + ctx->level[level].offset, levels[level], align(ctx->level[level].size, block));
    /* Keep the superblock in the small NCCH prefix for upstream finalization. */
    if (!failed) memcpy(ctx->output, levels[0], ctx->romfsHeaderSize);
done:
    for (int i = 0; i < 4; ++i) free(levels[i]);
    return failed;
}
#endif
'''


def change(path, old, new):
    source = path.read_text()
    if (new and new in source) or (not new and old not in source):
        return
    if old not in source:
        raise ValueError(f'Pinned makerom source differs at {path.name}: {old[:80]!r}')
    path.write_text(source.replace(old, new, 1))


def patch(source):
    source = Path(source)
    for name in ('ncch.c', 'cia.c', 'makerom.c', 'romfs_gen.c'):
        change(source/name, '#include "lib.h"', '#include "lib.h"\n#include "stream_package.h"')
    (source/'stream_package.h').write_text(HEADER)
    (source/'stream_package.c').write_text(SOURCE)
    change(source/'ncch.c', '\t// Import Data\n', '''#ifdef SRR_BROWSER_STREAMING
    SrrStreamConfigure(usrset->common.outFormat == CIA && !ncchset->options.Encrypt &&
        !usrset->cia.titleKey && !ncchset->componentFilePtrs.romfs);
#endif
\t// Import Data
''')
    change(source/'ncch.c', '\tu8 *ncch = calloc(1,ncchSize);', '''#ifdef SRR_BROWSER_STREAMING
    const bool streamed = SrrStreamRequested() && romfsSize;
    const u64 allocationSize = streamed ? romfsOffset + romfsHashSize : ncchSize;
    if (streamed && SrrStreamBegin(ncchSize, allocationSize, romfsOffset)) return -1;
#else
    const u64 allocationSize = ncchSize;
#endif
\tu8 *ncch = calloc(1,allocationSize);''')
    change(source/'ncch.c', '''\tresult = FinaliseNcch(ncchset);
\tif(result)''', '''\tresult = FinaliseNcch(ncchset);
#ifdef SRR_BROWSER_STREAMING
    if (!result && SrrStreamActive()) result = SrrStreamFinishNcch(ncchset->out->buffer);
#endif
\tif(result)''')
    change(source/'romfs_gen.c', 'int BuildRomFsBinary(romfs_buildctx *ctx)\n{', '''#ifdef SRR_BROWSER_STREAMING
int SrrBuildRomFs(romfs_buildctx *ctx);
#endif
int BuildRomFsBinary(romfs_buildctx *ctx)
{
#ifdef SRR_BROWSER_STREAMING
    if (SrrStreamActive()) return SrrBuildRomFs(ctx);
#endif''')
    change(source/'romfs_gen.c', '\t\tu8 *data_pos = (ctx->data + ctx->u_dataLen);', '')
    change(source/'romfs_gen.c', '''\t\tFILE *fp = os_fopen(file->path, OS_MODE_READ);
\t\tfread(data_pos, file->size, 1, fp);
\t\tfclose(fp);''', '''#ifdef SRR_BROWSER_STREAMING
        if (SrrStreamActive()) {
            const u64 dataStart = u8_to_u32(ctx->romfsHdr->dataoffset, LE);
            /* The file offset is relative to the start of the complete NCCH. */
            extern u64 SrrStreamRomfsBase(void);
            SrrStreamFile(file->path, SrrStreamRomfsBase() + ctx->level[3].offset + dataStart + ctx->u_dataLen, file->size);
        } else
#endif
        {
            u8 *data_pos = ctx->data + ctx->u_dataLen;
            FILE *fp = os_fopen(file->path, OS_MODE_READ);
            fread(data_pos, file->size, 1, fp);
            fclose(fp);
        }''')
    change(source/'cia.c', 'void GetContentHashes(cia_settings *ciaset);', 'int GetContentHashes(cia_settings *ciaset);')
    change(source/'cia.c', '\tGetContentHashes(ciaset);', '\tif (GetContentHashes(ciaset)) return -1;')
    change(source/'cia.c', '''int ImportNcchContent(cia_settings *ciaset)
{''', '''int ImportNcchContent(cia_settings *ciaset)
{
#ifdef SRR_BROWSER_STREAMING
    if (SrrStreamActive()) {
        if (ciaset->content.count != 1 || ciaset->content.encryptCia) {
            fprintf(stderr,"[STREAM ERROR] Only unencrypted single-content CIA output is supported\\n");
            return -1;
        }
        ciaset->ciaSections.content.size = ciaset->content.totalSize;
        return 0;
    }
#endif''')
    change(source/'cia.c', '''void GetContentHashes(cia_settings *ciaset)
{''', '''int GetContentHashes(cia_settings *ciaset)
{
#ifdef SRR_BROWSER_STREAMING
    if (SrrStreamActive()) return SrrStreamHash(0, ciaset->content.size[0], ciaset->content.hash[0]);
#endif''')
    change(source/'cia.c', '''\t\t\tprintf("Done!\\n");
\t}
}

void EncryptContent''', '''\t\t\tprintf("Done!\\n");
\t}
    return 0;
}

void EncryptContent''')
    change(source/'cia.c', '\tWriteBuffer(ciaset->ciaSections.content.buffer,ciaset->ciaSections.content.size,ciaset->ciaSections.contentOffset,ciaset->out);', '''#ifdef SRR_BROWSER_STREAMING
    if (SrrStreamActive()) {
        if (SrrStreamCopy(ciaset->out, ciaset->ciaSections.contentOffset, ciaset->ciaSections.content.size)) return -1;
    } else
#endif
\tWriteBuffer(ciaset->ciaSections.content.buffer,ciaset->ciaSections.content.size,ciaset->ciaSections.contentOffset,ciaset->out);''')
    change(source/'cia.c', '\tWriteBuffer(ciaset->ciaSections.meta.buffer,ciaset->ciaSections.meta.size,ciaset->ciaSections.metaOffset,ciaset->out);', '''\tWriteBuffer(ciaset->ciaSections.meta.buffer,ciaset->ciaSections.meta.size,ciaset->ciaSections.metaOffset,ciaset->out);
#ifdef SRR_BROWSER_STREAMING
    if (SrrStreamActive() && (fflush(ciaset->out) || ferror(ciaset->out))) {
        fprintf(stderr,"[STREAM ERROR] CIA output flush failed\\n");
        return -1;
    }
#endif''')
    change(source/'makerom.c', '''finish:
\tfree_UserSettings(set);''', '''finish:
#ifdef SRR_BROWSER_STREAMING
    SrrStreamClose();
#endif
\tfree_UserSettings(set);''')
    # A private accessor avoids changing the upstream ROMFS context layout.
    path = source/'stream_package.c'
    path.write_text(path.read_text().replace('bool SrrStreamActive(void)', 'u64 SrrStreamRomfsBase(void) { return romfsBase; }\nbool SrrStreamActive(void)', 1))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Pinned Project_CTR/makerom/src directory')
    patch(parser.parse_args().source)
