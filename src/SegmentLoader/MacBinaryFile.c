/*
 * MacBinaryFile.c - unpack a MacBinary file on disk into the file it carries
 *
 * A MacBinary archive copied onto a volume as-is (hcopy -r, a download) is
 * one data fork holding both forks of the real file. Nothing can launch it
 * like that: LaunchApplication opens the application's resource fork, and
 * this one has none. A Mac user ran StuffIt Expander or BinHex over it to get
 * the real file back; this is that step, done in place beside the archive.
 */

#include "SegmentLoader/MacBinary.h"
#include "FileManager.h"
#include "FileManagerTypes.h"   /* ioErr */
#include "MacTypes.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>

static Boolean mb_exists(short vRefNum, long dirID, ConstStr255Param name)
{
    FInfo info;
    return HGetFInfo(vRefNum, dirID, name, &info) == noErr;
}

/* The archive's own name, or "name.1" .. "name.9" if that is taken - the
 * archive itself often has it. HFS names stop at 31 characters. */
static OSErr mb_free_name(short vRefNum, long dirID, const UInt8* wanted, Str255 out)
{
    UInt8 len = wanted[0] > 31 ? 31 : wanted[0];
    out[0] = len;
    memcpy(out + 1, wanted + 1, len);
    if (!mb_exists(vRefNum, dirID, out)) {
        return noErr;
    }
    UInt8 base = len > 29 ? 29 : len;
    for (char n = '1'; n <= '9'; n++) {
        out[0] = base + 2;
        memcpy(out + 1, wanted + 1, base);
        out[base + 1] = '.';
        out[base + 2] = (UInt8)n;
        if (!mb_exists(vRefNum, dirID, out)) {
            return noErr;
        }
    }
    return dupFNErr;
}

static OSErr mb_write_fork(short refNum, const UInt8* bytes, UInt32 len)
{
    UInt32 count = len;
    OSErr err = len ? FSWrite(refNum, &count, bytes) : noErr;
    if (err == noErr && count != len) {
        err = ioErr;
    }
    OSErr closeErr = FSClose(refNum);
    return err != noErr ? err : closeErr;
}

Boolean MacBinary_IsMacBinaryFile(short vRefNum, long dirID, ConstStr255Param name)
{
    short refNum;
    UInt8 header[kMacBinHeaderSize];
    UInt32 eof = 0, count = sizeof(header);

    if (HOpenDF(vRefNum, dirID, name, fsRdPerm, &refNum) != noErr) {
        return false;
    }
    Boolean ok = FSGetEOF(refNum, &eof) == noErr &&
                 FSRead(refNum, &count, header) == noErr &&
                 count == sizeof(header) &&
                 MacBinary_IsMacBinary(header, eof);
    FSClose(refNum);
    return ok;
}

OSErr MacBinary_UnpackFile(short vRefNum, long dirID, ConstStr255Param archiveName,
                           FSSpec* result)
{
    short refNum;
    UInt32 eof = 0;
    OSErr err;

    if (!archiveName || !result) {
        return paramErr;
    }

    err = HOpenDF(vRefNum, dirID, archiveName, fsRdPerm, &refNum);
    if (err != noErr) {
        return err;
    }
    err = FSGetEOF(refNum, &eof);
    UInt8* archive = NULL;
    if (err == noErr && eof < kMacBinHeaderSize) {
        err = paramErr;
    }
    if (err == noErr) {
        archive = (UInt8*)NewPtr(eof);
        if (!archive) err = memFullErr;
    }
    if (err == noErr) {
        UInt32 count = eof;
        err = FSRead(refNum, &count, archive);
        if (err == noErr && count != eof) err = ioErr;
    }
    FSClose(refNum);

    MacBinaryArchive mb;
    if (err == noErr) {
        err = MacBinary_Parse(archive, eof, &mb);
    }

    Str255 name;
    if (err == noErr) {
        err = mb_free_name(vRefNum, dirID, mb.header.fileName, name);
    }

    OSType type = 0, creator = 0;
    if (err == noErr) {
        type    = ((OSType)mb.header.fileTypeId[0] << 24) | ((OSType)mb.header.fileTypeId[1] << 16) |
                  ((OSType)mb.header.fileTypeId[2] << 8)  |  (OSType)mb.header.fileTypeId[3];
        creator = ((OSType)mb.header.fileCreator[0] << 24) | ((OSType)mb.header.fileCreator[1] << 16) |
                  ((OSType)mb.header.fileCreator[2] << 8)  |  (OSType)mb.header.fileCreator[3];
        err = HCreate(vRefNum, dirID, name, creator, type);
    }
    if (err == noErr) {
        err = HOpenDF(vRefNum, dirID, name, fsRdWrPerm, &refNum);
        if (err == noErr) err = mb_write_fork(refNum, mb.dataFork, mb.dataForkSize);
        if (err == noErr) err = HOpenRF(vRefNum, dirID, name, fsRdWrPerm, &refNum);
        if (err == noErr) err = mb_write_fork(refNum, mb.resourceFork, mb.resourceForkSize);
        if (err == noErr) {
            /* The flags travel, but not where the file sat on the sender's
             * desktop, nor that their Finder had already read its bundle -
             * this Finder has not, and must place it and read it itself. */
            FInfo info;
            memset(&info, 0, sizeof(info));
            info.fdType    = type;
            info.fdCreator = creator;
            info.fdFlags   = mb.header.finderFlags & (UInt16)~kMacBinSenderFinderFlags;
            err = HSetFInfo(vRefNum, dirID, name, &info);
        }
        if (err != noErr) {
            HDelete(vRefNum, dirID, name);   /* no half-written file left behind */
        }
    }

    if (archive) {
        DisposePtr((Ptr)archive);
    }
    if (err == noErr) {
        memset(result, 0, sizeof(*result));
        result->vRefNum = vRefNum;
        result->parID   = dirID;
        memcpy(result->name, name, (size_t)name[0] + 1);
    }
    return err;
}
