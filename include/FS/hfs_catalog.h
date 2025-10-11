/* HFS Catalog Operations */
#pragma once
#include "hfs_types.h"
#include "hfs_btree.h"
#include <stdbool.h>

/* Catalog operations context */
typedef struct {
    HFS_BTree   bt;          /* B-tree for catalog */
    HFS_Volume* vol;         /* Volume reference */
} HFS_Catalog;

/* Initialize catalog */
bool HFS_CatalogInit(HFS_Catalog* cat, HFS_Volume* vol);

/* Close catalog */
void HFS_CatalogClose(HFS_Catalog* cat);

/* Enumerate entries in a directory */
bool HFS_CatalogEnumerate(HFS_Catalog* cat, DirID parentID,
                         CatEntry* entries, int maxEntries, int* count);

/* Lookup a specific entry by name */
bool HFS_CatalogLookup(HFS_Catalog* cat, DirID parentID, const char* name,
                       CatEntry* entry);

/* Get entry by CNID */
bool HFS_CatalogGetByID(HFS_Catalog* cat, FileID cnid, CatEntry* entry);

/* Create a new folder in the catalog */
bool HFS_CatalogCreateFolder(HFS_Catalog* cat, DirID parentID, const char* name,
                             FileID* newFolderID);

/* Convert MacRoman to ASCII */
void HFS_MacRomanToASCII(char* dst, const uint8_t* src, uint8_t len, size_t maxDst);

/* Parse catalog record into CatEntry */
bool HFS_ParseCatalogRecord(const HFS_CatKey* key, const void* data, uint16_t dataLen,
                           CatEntry* entry);