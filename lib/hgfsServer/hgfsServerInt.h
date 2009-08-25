/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/


#ifndef __HGFS_SERVER_INT_H__
#   define __HGFS_SERVER_INT_H__

#include "vm_basic_types.h"

/*
 * Definitions of wrapped internal primitives.
 *
 * We wrap open file handles and directory entries so that cross-platform HGFS
 * server code can use them without platform-specific pre-processing.
 *
 * On Linux, we use dirent64 from the kernel headers so as to alleviate any
 * confusion between what the kernel will give us from the getdents64()
 * syscall and what userspace will expect. Note that to avoid depending on
 * kernel headers directly, I've copied the dirent struct, replacing certain
 * kernel basic types with our own.
 *
 * On Windows, we define our own DirectoryEntry struct with d_reclen and
 * d_name, as those are the only two fields we're interested in. It's not
 * essential that it match the dirents from another platform, because we
 * control how they get created and populated, and they never pass down a wire.
 *
 * Otherwise, we use the native dirent struct provided by the platform's libc,
 * as nobody else seems to suffer from the 32-bit vs. 64-bit ino_t and off_t
 * insanity that plagues Linux.
 */
#ifndef _WIN32
#  ifdef linux
   typedef struct DirectoryEntry {
      uint64 d_ino;
      uint64 d_off;
      uint16 d_reclen;
      uint8  d_type;
      int8   d_name[256];
   } DirectoryEntry;
#  else
#    include <dirent.h>
     typedef struct dirent DirectoryEntry;
#  endif
   typedef int fileDesc;
#else
#  include <windows.h>
   typedef HANDLE fileDesc;
   typedef struct DirectoryEntry {
      unsigned short d_reclen;      /* The total length of this record. */
      char d_name[PATH_MAX * 4];    /* 4 bytes is the maximum size of a utf8 */
                                    /* representation of utf-16 encoded */
                                    /* unicode character in the BMP */
   } DirectoryEntry;
#endif

#include "dbllnklst.h"
#include "hgfsProto.h"
#include "cpName.h"     // for HgfsNameStatus
#include "hgfsServerPolicy.h"
#include "hgfsUtil.h"   // for HgfsInternalStatus
#include "syncMutex.h"

/*
 * Does this platform have oplock support? We define it here to avoid long
 * ifdefs all over the code. For now, Linux and Windows hosts only.
 *
 * XXX: Just kidding, no oplock support yet.
 */
#if 0
#define HGFS_OPLOCKS
#endif

/* Value of config option to require using host timestamps */
EXTERN Bool alwaysUseHostTime;

/* Identifier for a local file */
typedef struct HgfsLocalId {
   uint64 volumeId;
   uint64 fileId;
} HgfsLocalId;


/* Three possible filenode states */
typedef enum {
   FILENODE_STATE_UNUSED,              /* Linked on the free list */
   FILENODE_STATE_IN_USE_CACHED,       /* Linked on the cached nodes list */
   FILENODE_STATE_IN_USE_NOT_CACHED,   /* Not linked on any list */
} FileNodeState;

/* Three possible search types */
typedef enum {
   DIRECTORY_SEARCH_TYPE_DIR,       /* Objects are files and subdirectories */
   DIRECTORY_SEARCH_TYPE_BASE,      /* Objects are shares */
   DIRECTORY_SEARCH_TYPE_OTHER,     /* Objects are the contents of
                                       "root/drive" or contents of "root" */
} DirectorySearchType;

/* Two possible volume info type */
typedef enum {
   VOLUME_INFO_TYPE_MIN,
   VOLUME_INFO_TYPE_MAX,
} VolumeInfoType;

/*
 * The "default" share access is used in cross-platform code, so it's helpful
 * to have a single macro for accessing it.
 */
#ifdef _WIN32
#  define HGFS_DEFAULT_SHARE_ACCESS (FILE_SHARE_READ | FILE_SHARE_WRITE | \
                                     FILE_SHARE_DELETE)
#else
#  define HGFS_DEFAULT_SHARE_ACCESS 0
#endif // _WIN32

typedef struct HgfsShareInfo {
   /* Filename of the root directory for the shared folder */
   const char *rootDir;

   /* Length of the root directory filename (does not include nul terminator) */
   size_t rootDirLen;

   /* Read permissions for the shared folder, needed for handle => name conversions. */
   Bool readPermissions;

   /* Write permissions for the shared folder, needed for handle => name conversions. */
   Bool writePermissions;
} HgfsShareInfo;

/*
 * This struct represents a file on the local filesystem that has been
 * opened by a remote client. We store the name of the local file and
 * enough state to keep track of whether the file has changed locally
 * between remote accesses. None of the fields contain cross-platform
 * types; everything has been converted for the local filesystem.
 *
 * A file node object can only be in 1 of these 3 states:
 * 1) FILENODE_STATE_UNUSED: linked on the free list
 * 2) FILENODE_STATE_IN_USE_CACHED: Linked on the cached nodes list
 * 3) FILENODE_STATE_IN_USE_NOT_CACHED: Linked on neither of the above two lists.
 */
typedef struct HgfsFileNode {
   /* Links to place the object on various lists */
   DblLnkLst_Links links;

   /* HGFS handle uniquely identifying this node. */
   HgfsHandle handle;

   /* Local filename (in UTF8) */
   char *utf8Name;

   /* Length of filename (does not include nul terminator) */
   size_t utf8NameLen;

   /* share name */
   char *shareName;

   /* Length of share name (does not include nul terminator) */
   size_t shareNameLen;

   /* ID of file in local filesystem */
   HgfsLocalId localId;

   /* File descriptor */
   fileDesc fileDesc;

   /* On POSIX, access mode. On Windows, desired access */
   uint32 mode;

   /* Share access to open with (Windows only) */
   uint32 shareAccess;

   /* The server lock that the node currently has. */
   HgfsServerLock serverLock;

   /* File node state on lists */
   FileNodeState state;

   /* File flags - see below. */
   uint32 flags;

   /*
    * Context as required by some file operations. Eg: BackupWrite on
    * Windows: BackupWrite requires the caller to hold on to a pointer
    * to a Windows internal data structure between subsequent calls to
    * BackupWrite while restoring a file.
    */
   void *fileCtx;

   /* Parameters associated with the share. */
   HgfsShareInfo shareInfo;
} HgfsFileNode;


/* HgfsFileNode flags. */

/* TRUE if opened in append mode */
#define HGFS_FILE_NODE_APPEND_FL               (1 << 0)
/* Whether this file was opened in sequential mode. */
#define HGFS_FILE_NODE_SEQUENTIAL_FL           (1 << 1)
/* Whether this a shared folder open. */
#define HGFS_FILE_NODE_SHARED_FOLDER_OPEN_FL   (1 << 2)

/*
 * This struct represents a file search that a client initiated.
 *
 * A search object can only be in 1 of these 2 states:
 * 1) Unused: linked on the free list
 * 2) In use: unlinked
 */
typedef struct HgfsSearch {
   /* Links to place the object on various lists */
   DblLnkLst_Links links;

   /* HGFS handle uniquely identifying this search. */
   HgfsHandle handle;

   /* Local directory name (in UTF8) */
   char *utf8Dir;

   /* Length of directory name (does not include nul terminator) */
   size_t utf8DirLen;

   /* Share name. */
   char *utf8ShareName;

   /* Share name length. */
   size_t utf8ShareNameLen;

   /* Directory entries for this search */
   DirectoryEntry **dents;

   /* Number of dents */
   uint32 numDents;

   /*
    * What type of search is this (what objects does it track)? This is
    * important to know so we can do the right kind of stat operation later
    * when we want to retrieve the attributes for each dent.
    */
   DirectorySearchType type;

   /* Parameters associated with the share. */
   HgfsShareInfo shareInfo;
} HgfsSearch;

/* HgfsSessionInfo flags. */
typedef enum {
   HGFS_SESSION_TYPE_REGULAR,      /* Dynamic session, created by the HgfsTransport. */
   HGFS_SESSION_TYPE_INTERNAL,     /* This is a static session. */
} HgfsSessionInfoType;

/* HgfsTransportSessionState, used for session status. */
typedef enum {
   HGFS_SESSION_STATE_OPEN,
   HGFS_SESSION_STATE_CLOSED,
} HgfsSessionInfoState;


typedef struct HgfsSessionInfo {
   /* Transport session context. */
   void *transportData;

   /* Current state of the session. */
   HgfsSessionInfoState state;

   /* Session is dynamic or internal. */
   HgfsSessionInfoType type;

   /* Function to send reply for a packet after processing. */
   HgfsSessionSendFunc *send;

   /* Lock to ensure some fileIO requests are atomic for a handle. */
   SyncMutex fileIOLock;

   Atomic_uint32 refCount;    /* Reference count for session. */

   /*
    ** START NODE ARRAY **************************************************
    *
    * Lock for the following 6 fields: the node array,
    * counters and lists for this session.
    */
   SyncMutex nodeArrayLock;

   /* Open file nodes of this session. */
   HgfsFileNode *nodeArray;

   /* Number of nodes in the nodeArray. */
   uint32 numNodes;

   /* Free list of file nodes. LIFO to be cache-friendly. */
   DblLnkLst_Links nodeFreeList;

   /* List of cached open nodes. */
   DblLnkLst_Links nodeCachedList;

   /* Current number of open nodes. */
   unsigned int numCachedOpenNodes;

   /* Number of open nodes having server locks. */
   unsigned int numCachedLockedNodes;
   /** END NODE ARRAY ****************************************************/

   /*
    ** START SEARCH ARRAY ************************************************
    *
    * Lock for the following three fields: for the search array
    * and it's counter and list, for this session.
    */
   SyncMutex searchArrayLock;

   /* Directory entry cache for this session. */
   HgfsSearch *searchArray;

   /* Number of entries in searchArray. */
   uint32 numSearches;

   /* Free list of searches. LIFO. */
   DblLnkLst_Links searchFreeList;
   /** END SEARCH ARRAY ****************************************************/

} HgfsSessionInfo;


/*
 * These structs represent information about file open requests, file
 * attributes, and directory creation requests.
 *
 * The main reason for these structs is data abstraction -- we pass
 * a struct around instead of the individual parameters. This way
 * as more parameters are implemented, we don't have to add more
 * parameters to the functions, instead just extend the structs.
 */

typedef struct HgfsFileOpenInfo {
   HgfsOp requestType;
   HgfsHandle file;                  /* Opaque file ID used by the server */
   HgfsOpenValid mask;               /* Bitmask that specified which fields are valid. */
   HgfsOpenMode mode;                /* Which type of access requested. See desiredAccess */
   HgfsOpenFlags flags;              /* Which flags to open the file with */
   HgfsPermissions specialPerms;     /* Desired 'special' permissions for file creation */
   HgfsPermissions ownerPerms;       /* Desired 'owner' permissions for file creation */
   HgfsPermissions groupPerms;       /* Desired 'group' permissions for file creation */
   HgfsPermissions otherPerms;       /* Desired 'other' permissions for file creation */
   HgfsAttrFlags attr;               /* Attributes, if any, for file creation */
   uint64 allocationSize;            /* How much space to pre-allocate during creation */
   uint32 desiredAccess;             /* Extended support for windows access modes */
   uint32 shareAccess;               /* Windows only, share access modes */
   HgfsServerLock desiredLock;       /* The type of lock desired by the client */
   HgfsServerLock acquiredLock;      /* The type of lock acquired by the server */
   uint32 cpNameSize;
   char *cpName;
   char *utf8Name;
   uint32 caseFlags;                 /* Case-sensitivity flags. */
   HgfsShareInfo shareInfo;          /* Parameters associated with the share. */
} HgfsFileOpenInfo;

typedef struct HgfsFileAttrInfo {
   HgfsOp requestType;
   HgfsAttrValid mask;
   HgfsFileType type;            /* File type */
   uint64 size;                  /* File size (in bytes) */
   uint64 creationTime;          /* Creation time. Ignored by POSIX */
   uint64 accessTime;            /* Time of last access */
   uint64 writeTime;             /* Time of last write */
   uint64 attrChangeTime;        /* Time file attributes were last
                                  * changed. Ignored by Windows */
   HgfsPermissions specialPerms; /* Special permissions bits. Ignored by Windows */
   HgfsPermissions ownerPerms;   /* Owner permissions bits */
   HgfsPermissions groupPerms;   /* Group permissions bits. Ignored by Windows */
   HgfsPermissions otherPerms;   /* Other permissions bits. Ignored by Windows */
   HgfsAttrFlags flags;          /* Various flags and Windows 'attributes' */
   uint64 allocationSize;        /* Actual size of file on disk */
   uint32 userId;                /* User identifier, ignored by Windows */
   uint32 groupId;               /* group identifier, ignored by Windows */
   uint64 hostFileId;            /* File Id of the file on host: inode_t on Linux */
   uint32 volumeId;              /* Volume Id of the volune on which the file resides */
   uint32 effectivePerms;        /* Permissions in effect for the current user */
} HgfsFileAttrInfo;

typedef struct HgfsCreateDirInfo {
   HgfsOp requestType;
   HgfsCreateDirValid mask;
   HgfsPermissions specialPerms; /* Special permissions bits. Ignored by Windows */
   HgfsPermissions ownerPerms;   /* Owner permissions bits */
   HgfsPermissions groupPerms;   /* Group permissions bits. Ignored by Windows */
   HgfsPermissions otherPerms;   /* Other permissions bits. Ignored by Windows */
   uint32 cpNameSize;
   char *cpName;
   uint32 caseFlags;             /* Case-sensitivity flags. */
   HgfsAttrFlags fileAttr;       /* Various flags and Windows 'attributes' */
} HgfsCreateDirInfo;


/* Server lock related structure */
typedef struct {
   fileDesc fileDesc;
   int32 event;
   HgfsServerLock serverLock;
} ServerLockData;


Bool
HgfsCreateAndCacheFileNode(HgfsFileOpenInfo *openInfo, // IN: Open info struct
                           HgfsLocalId const *localId, // IN: Local unique file ID
                           fileDesc fileDesc,          // IN: OS file handle
                           Bool append,                // IN: Open with append flag
                           HgfsSessionInfo *session);  // IN: Session info

Bool
HgfsSearchHandle2FileName(HgfsHandle handle,       // IN: Hgfs search handle
                          char **fileName,         // OUT: cp file name
                          uint32 *fileNameSize);   // OUT: cp file name size

void
HgfsUpdateNodeNames(const char *oldLocalName,  // IN: Name of file to look for
                    const char *newLocalName,  // IN: Name to replace with
                    HgfsSessionInfo *session); // IN: Session info

Bool
HgfsRemoveSearch(HgfsHandle searchHandle,
                 HgfsSessionInfo *session);

void
HgfsServerDumpDents(HgfsHandle searchHandle,   // IN: Handle to dump dents from
                    HgfsSessionInfo *session); // IN: Session info

DirectoryEntry *
HgfsGetSearchResult(HgfsHandle handle,        // IN: Handle to search
                    HgfsSessionInfo *session, // IN: Session info
                    uint32 offset,            // IN: Offset to retrieve at
                    Bool remove);             // IN: If true, removes the result

Bool
HgfsServerStatFs(const char *pathName, // IN: Path we're interested in
                 size_t pathLength,    // IN: Length of path
                 uint64 *freeBytes,    // OUT: Free bytes on volume
                 uint64 *totalBytes);  // OUT: Total bytes on volume

HgfsNameStatus
HgfsServerGetAccess(char *in,                    // IN:  CP filename to check
                    size_t inSize,               // IN:  Size of name in
                    HgfsOpenMode mode,           // IN:  Requested access mode
                    uint32 caseFlags,            // IN:  Case-sensitivity flags
                    char **bufOut,               // OUT: File name in local fs
                    size_t *outLen);             // OUT: Length of name out

HgfsNameStatus
HgfsServerGetShareInfo(char *cpName,            // IN:  Cross-platform filename to check
                       size_t cpNameSize,       // IN:  Size of name cpName
                       uint32 caseFlags,        // IN:  Case-sensitivity flags
                       HgfsShareInfo* shareInfo,// OUT: Shared folder properties
                       char **bufOut,           // OUT: File name in local fs
                       size_t *outLen);         // OUT: Length of name out

Bool
HgfsServerIsSharedFolderOnly(char const *in,  // IN:  CP filename to check
                             size_t inSize);  // IN:  Size of name in

HgfsInternalStatus
HgfsServerScandir(char const *baseDir,      // IN: Directory to search in
                  size_t baseDirLen,        // IN: Length of directory
                  Bool followSymlinks,      // IN: followSymlinks config option
                  DirectoryEntry ***dents,  // OUT: Array of DirectoryEntrys
                  int *numDents);           // OUT: Number of DirectoryEntrys

HgfsInternalStatus
HgfsServerSearchRealDir(char const *baseDir,      // IN: Directory to search
                        size_t baseDirLen,        // IN: Length of directory
                        char const *shareName,    // IN: Share name
                        char const *rootDir,      // IN: Root directory for the share
                        HgfsSessionInfo *session, // IN: Session info
                        HgfsHandle *handle);      // OUT: Search handle

HgfsInternalStatus
HgfsServerSearchVirtualDir(HgfsGetNameFunc *getName,     // IN: Name enumerator
                           HgfsInitFunc *initName,       // IN: Init function
                           HgfsCleanupFunc *cleanupName, // IN: Cleanup function
                           DirectorySearchType type,     // IN: Kind of search
                           HgfsSessionInfo *session,     // IN: Session info
                           HgfsHandle *handle);          // OUT: Search handle

/*
 * Opcode handlers
 */

HgfsInternalStatus
HgfsServerOpen(char const *packetIn,       // IN: incoming packet
               size_t packetSize,          // IN: size of packet
               HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerRead(char const *packetIn,       // IN: incoming packet
               size_t packetSize,          // IN: size of packet
               HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerWrite(char const *packetIn,       // IN: incoming packet
                size_t packetSize,          // IN: size of packet
                HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerSearchOpen(char const *packetIn,       // IN: incoming packet
                     size_t packetSize,          // IN: size of packet
                     HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerSearchRead(char const *packetIn,       // IN: incoming packet
                     size_t packetSize,          // IN: size of packet
                     HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerGetattr(char const *packetIn,       // IN: incoming packet
                  size_t packetSize,          // IN: size of packet
                  HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerSetattr(char const *packetIn,       // IN: incoming packet
                  size_t packetSize,          // IN: size of packet
                  HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerCreateDir(char const *packetIn,       // IN: incoming packet
                    size_t packetSize,          // IN: size of packet
                    HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerDeleteFile(char const *packetIn,       // IN: incoming packet
                     size_t packetSize,          // IN: size of packet
                     HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerDeleteDir(char const *packetIn,       // IN: incoming packet
                    size_t packetSize,          // IN: size of packet
                    HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerRename(char const *packetIn,	     // IN: incoming packet
                 size_t packetSize,          // IN: size of packet
                 HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerQueryVolume(char const *packetIn,       // IN: incoming packet
                      size_t packetSize,          // IN: size of packet
                      HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerSymlinkCreate(char const *packetIn,       // IN: incoming packet
                        size_t packetSize,          // IN: size of packet
                        HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerServerLockChange(char const *packetIn,       // IN: incoming packet
                           size_t packetSize,          // IN: size of packet
                           HgfsSessionInfo *session);  // IN: opaque transport data

HgfsInternalStatus
HgfsServerWriteWin32Stream(char const *packetIn,       // IN: incoming packet
                           size_t packetSize,          // IN: size of packet
                           HgfsSessionInfo *session);  // IN: opaque transport data

/* Unpack/pack requests/reply helper functions. */

Bool
HgfsUnpackOpenRequest(char const *packetIn,        // IN: incoming packet
                     size_t packetSize,            // IN: size of packet
                     HgfsFileOpenInfo *openInfo);  // IN/OUT: open info struct

Bool
HgfsPackOpenReply(char const *packetIn,         // IN: incoming packet
                  HgfsInternalStatus status,    // IN: reply status
                  HgfsFileOpenInfo *openInfo,   // IN: open info struct
                  char **packetOut,             // OUT: outgoing packet
                  size_t *packetSize);          // OUT: outgoing packet size

Bool
HgfsUnpackGetattrRequest(char const *packetIn,       // IN: request packet
                         size_t packetSize,          // IN: request packet size
                         HgfsFileAttrInfo *attrInfo, // IN/OUT: unpacked attr struct
                         HgfsAttrHint *hints,        // OUT: getattr hints
                         char **cpName,              // OUT: cpName
                         size_t *cpNameSize,         // OUT: cpName size
                         HgfsHandle *file,           // OUT: file handle
			 uint32 *caseFlags);         // OUT: case-sensitivity flags

Bool
HgfsUnpackDeleteRequest(char const *packetIn,       // IN: request packet
                        size_t packetSize,          // IN: request packet size
                        char **cpName,              // OUT: cpName
                        size_t *cpNameSize,         // OUT: cpName size
                        HgfsDeleteHint *hints,      // OUT: delete hints
                        HgfsHandle *file,           // OUT: file handle
			uint32 *caseFlags);         // OUT: case-sensitivity flags

Bool
HgfsPackDeleteReply(char const *packetIn,          // IN: incoming packet
                    HgfsInternalStatus status,     // IN: reply status
                    char **packetOut,              // OUT: outgoing packet
                    size_t *packetSize);           // OUT: size of packet

Bool
HgfsUnpackRenameRequest(char const *packetIn,       // IN: request packet
                        size_t packetSize,          // IN: request packet size
                        char **cpOldName,           // OUT: rename src
                        uint32 *cpOldNameLen,       // OUT: rename src size
                        char **cpNewName,           // OUT: rename dst
                        uint32 *cpNewNameLen,       // OUT: rename dst size
                        HgfsRenameHint *hints,      // OUT: rename hints
                        HgfsHandle *srcFile,        // OUT: src file handle
                        HgfsHandle *targetFile,     // OUT: target file handle
			uint32 *oldCaseFlags,       // OUT: old case-sensitivity flags
			uint32 *newCaseFlags);      // OUT: new case-sensitivity flags

Bool
HgfsPackRenameReply(char const *packetIn,       // IN: incoming packet
                    HgfsInternalStatus status,  // IN: reply status
                    char **packetOut,           // OUT: outgoing packet
                    size_t *packetSize);        // OUT: size of packet

Bool
HgfsPackGetattrReply(char const *packetIn,       // IN: incoming packet
                     HgfsInternalStatus status,  // IN: reply status
                     HgfsFileAttrInfo *attr,     // IN: attr stucture
                     const char *utf8TargetName, // IN: optional target name
                     uint32 utf8TargetNameLen,   // IN: file name length
                     char **packetOut,           // OUT: outgoing packet
                     size_t *packetSize);        // OUT: size of packet

Bool
HgfsUnpackSearchReadRequest(const char *packetIn,         // IN: request packet
                            size_t packetSize,            // IN: packet size
                            HgfsFileAttrInfo *attr,       // OUT: unpacked attr struct
                            HgfsHandle *hgfsSearchHandle, // OUT: hgfs search handle
                            uint32 *offset);              // OUT: entry offset

Bool
HgfsPackSearchReadReply(char const *packetIn,      // IN: incoming packet
                        HgfsInternalStatus status, // IN: reply status
                        const char *utf8Name,      // IN: file name
                        size_t utf8NameLen,        // IN: file name length
                        HgfsFileAttrInfo *attr,    // IN: file attr struct
                        char **packetOut,          // OUT: outgoing packet
                        size_t *packetSize);       // OUT: size of packet

Bool
HgfsUnpackSetattrRequest(char const *packetIn,            // IN: request packet
                         size_t packetSize,               // IN: request packet size
                         HgfsFileAttrInfo *attr,          // IN/OUT: getattr info
                         HgfsAttrHint *hints,             // OUT: setattr hints
                         char **cpName,                   // OUT: cpName
                         size_t *cpNameSize,              // OUT: cpName size
                         HgfsHandle *file,                // OUT: server file ID
			 uint32 *caseFlags);              // OUT: case-sensitivity flags

Bool
HgfsPackSetattrReply(char const *packetIn,       // IN: setattrOp operation version
                     HgfsInternalStatus status,  // IN: reply status
                     char **packetOut,           // OUT: outgoing packet
                     size_t *packetSize);        // OUT: size of packet


Bool
HgfsUnpackCreateDirRequest(char const *packetIn,     // IN: incoming packet
                           size_t packetSize,        // IN: size of packet
                           HgfsCreateDirInfo *info); // IN/OUT: info struct

Bool
HgfsPackCreateDirReply(char const *packetIn,      // IN: incoming packet
                       HgfsInternalStatus status, // IN: reply status
                       char **packetOut,          // OUT: outgoing packet
                       size_t *packetSize);       // OUT: size of packet

Bool
HgfsUnpackWriteWin32StreamRequest(char const *packetIn, // IN: incoming packet
				  size_t packetSize,    // IN: size of packet
				  HgfsHandle *file,     // OUT: file to write to
				  char **payload,       // OUT: data to write
				  size_t *requiredSize, // OUT: size of data
				  Bool *doSecurity);    // OUT: restore sec.str.

Bool
HgfsPackWriteWin32StreamReply(char const *packetIn,      // IN: incoming packet
			      HgfsInternalStatus status, // IN: reply status
			      uint32 actualSize,         // IN: amount written
			      char **packetOut,          // OUT: outgoing packet
			      size_t *packetSize);       // OUT: size of packet

/* Node cache functions. */

Bool
HgfsRemoveFromCache(HgfsHandle handle,         // IN: Hgfs handle of the node
                    HgfsSessionInfo *session); // IN: Session info

Bool
HgfsAddToCache(HgfsHandle handle,         // IN: Hgfs handle of the node
               HgfsSessionInfo *session); // IN: Session info

Bool
HgfsIsCached(HgfsHandle handle,         // IN: Hgfs handle of the node
             HgfsSessionInfo *session); // IN: Session info

Bool
HgfsIsServerLockAllowed(HgfsSessionInfo *session);  // IN: session info

Bool
HgfsHandle2FileDesc(HgfsHandle handle,        // IN: Hgfs file handle
                    HgfsSessionInfo *session, // IN: session info
                    fileDesc *fd,             // OUT: OS handle (file descriptor)
                    void **fileCtx);          // OUT: OS file context

Bool
HgfsFileDesc2Handle(fileDesc fd,              // IN: OS handle (file descriptor)
                    HgfsSessionInfo *session, // IN: session info
                    HgfsHandle *handle);      // OUT: Hgfs file handle

Bool
HgfsHandle2ShareMode(HgfsHandle handle,         // IN: Hgfs file handle
                     HgfsSessionInfo *session,  // IN: session info
                     HgfsOpenMode *shareMode);  // OUT: UTF8 file name size

Bool
HgfsHandle2FileName(HgfsHandle handle,        // IN: Hgfs file handle
                    HgfsSessionInfo *session, // IN: session info
                    char **fileName,          // OUT: CP file name
                    size_t *fileNameSize);    // OUT: CP file name size
Bool
HgfsHandle2FileNameMode(HgfsHandle handle,       // IN: Hgfs file handle
                        HgfsSessionInfo *session,// IN: Session info
                        Bool *readPermissions,   // OUT: shared folder permissions
                        Bool *writePermissions,  // OUT: shared folder permissions
                        char **fileName,         // OUT: UTF8 file name
                        size_t *fileNameSize);   // OUT: UTF8 file name size
Bool
HgfsHandle2AppendFlag(HgfsHandle handle,        // IN: Hgfs file handle
                      HgfsSessionInfo *session, // IN: session info
                      Bool *appendFlag);        // OUT: Append flag

Bool
HgfsHandle2LocalId(HgfsHandle handle,        // IN: Hgfs file handle
                   HgfsSessionInfo *session, // IN: session info
                   HgfsLocalId *localId);    // OUT: Local id info

Bool
HgfsHandle2ServerLock(HgfsHandle handle,        // IN: Hgfs file handle
                      HgfsSessionInfo *session, // IN: session info
                      HgfsServerLock *lock);    // OUT: Server lock

Bool
HgfsUpdateNodeFileDesc(HgfsHandle handle,        // IN: Hgfs file handle
                       HgfsSessionInfo *session, // IN: session info
                       fileDesc fd,              // IN: OS handle (file desc
                       void *fileCtx);           // IN: OS file context

Bool
HgfsUpdateNodeServerLock(fileDesc fd,                // IN: OS handle
                         HgfsSessionInfo *session,   // IN: session info
                         HgfsServerLock serverLock); // IN: new oplock

Bool
HgfsUpdateNodeAppendFlag(HgfsHandle handle,        // IN: Hgfs file handle
                         HgfsSessionInfo *session, // IN: session info
                         Bool appendFlag);         // OUT: Append flag

Bool
HgfsFileHasServerLock(const char *utf8Name,             // IN: Name in UTF8
                      HgfsSessionInfo *session,         // IN: Session info
                      HgfsServerLock *serverLock,       // OUT: Existing oplock
                      fileDesc   *fileDesc);            // OUT: Existing fd
Bool
HgfsGetNodeCopy(HgfsHandle handle,        // IN: Hgfs file handle
                HgfsSessionInfo *session, // IN: session info
                Bool copyName,            // IN: Should we copy the name?
                HgfsFileNode *copy);      // IN/OUT: Copy of the node

Bool
HgfsHandleIsSequentialOpen(HgfsHandle handle,        // IN:  Hgfs file handle
                           HgfsSessionInfo *session, // IN: session info
                           Bool *sequentialOpen);    // OUT: If open was sequential

Bool
HgfsHandleIsSharedFolderOpen(HgfsHandle handle,        // IN:  Hgfs file handle
                             HgfsSessionInfo *session, // IN: session info
                             Bool *sharedFolderOpen);  // OUT: If shared folder

Bool
HgfsGetSearchCopy(HgfsHandle handle,        // IN: Hgfs search handle
                  HgfsSessionInfo *session, // IN: Session info
                  HgfsSearch *copy);        // IN/OUT: Copy of the search

HgfsInternalStatus
HgfsCloseFile(fileDesc fileDesc,            // IN: OS handle of the file
              void *fileCtx);               // IN: file context

Bool
HgfsServerGetOpenMode(HgfsFileOpenInfo *openInfo, // IN:  Open info to examine
                      uint32 *modeOut);           // OUT: Local mode

Bool
HgfsAcquireServerLock(fileDesc fileDesc,            // IN: OS handle
                      HgfsSessionInfo *session,     // IN: Session info
                      HgfsServerLock *serverLock);  // IN/OUT: Oplock asked for/granted

Bool
HgfsServerPlatformInit(void);

void
HgfsServerPlatformDestroy(void);

HgfsNameStatus
HgfsServerHasSymlink(const char *fileName,      // IN: fileName to be checked
                     size_t fileNameLength,     // IN
                     const char *sharePath,     // IN: share path in question
                     size_t sharePathLen);      // IN
HgfsNameStatus
HgfsServerConvertCase(const char *sharePath,             // IN: share path in question
                      size_t sharePathLength,            // IN
                      char *fileName,                    // IN: filename to be looked up
                      size_t fileNameLength,             // IN
                      uint32 caseFlags,                  // IN: case-sensitivity flags
                      char **convertedFileName,          // OUT: case-converted filename
                      size_t *convertedFileNameLength);  // OUT

Bool
HgfsServerCaseConversionRequired(void);

char*
HgfsBuildRelativePath(const char* source,    // IN: source file name
                      const char* target);   // IN: target file name

/* All oplock-specific functionality is defined here. */
#ifdef HGFS_OPLOCKS
void
HgfsServerOplockBreak(ServerLockData *data); // IN: server lock info

void
HgfsAckOplockBreak(ServerLockData *lockData,  // IN: server lock info
                   HgfsServerLock replyLock); // IN: client has this lock

#endif

/* Transport related functions. */
Bool
HgfsPackAndSendPacket(char *packet,               // IN: packet to send
                      size_t packetSize,          // IN: packet size
                      HgfsInternalStatus status,  // IN: status
                      HgfsHandle id,              // IN: id of the request packet
                      HgfsSessionInfo *session,   // IN: session info
                      HgfsSendFlags flags);       // IN: flags how to send

Bool
HgfsPacketSend(char *packet,                // IN: packet buffer
               size_t packetSize,           // IN: packet size
               HgfsSessionInfo *session,    // IN: session info
               HgfsSendFlags flags);        // IN: flags how to send

Bool
HgfsServerCheckOpenFlagsForShare(HgfsFileOpenInfo *openInfo, // IN: Hgfs file handle
                                 HgfsOpenFlags *flags);      // IN/OUT: open mode

#endif /* __HGFS_SERVER_INT_H__ */