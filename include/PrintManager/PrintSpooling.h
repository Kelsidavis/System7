/*
 * PrintSpooling.h
 *
 * Print Spooling and Queue Management for System 7.1 Portable
 * Handles print job queuing, spooling, and background printing
 *
 * Based on Apple's Print Manager spooling from Mac OS System 7.1
 */

#ifndef __PRINTSPOOLING__
#define __PRINTSPOOLING__

#include "PrintTypes.h"
#include "Files.h"
#include "Processes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Spooling Constants */
enum {
    kMaxSpoolFiles = 32,                /* Maximum number of spool files */
    kMaxJobName = 64,                   /* Maximum job name length */
    kMaxDocumentName = 256,             /* Maximum document name length */
    kSpoolFileBufferSize = 8192,        /* Spool file buffer size */
    kMaxConcurrentJobs = 8              /* Maximum concurrent print jobs */
};

/* Spool File Constants */
enum {
    kSpoolFileType = 'SPOOL',           /* Spool file type */
    kSpoolFileCreator = 'PRNT',         /* Spool file creator */
    kSpoolFileVersion = 1               /* Current spool file version */
};

/* Print Job Priority */
enum {
    kJobPriorityLow = 0,                /* Low priority */
    kJobPriorityNormal = 1,             /* Normal priority */
    kJobPriorityHigh = 2,               /* High priority */
    kJobPriorityUrgent = 3              /* Urgent priority */
};

/* Spool File Header */
struct TSpoolFileHeader {
    OSType fileType;                    /* File type signature */
    OSType creator;                     /* Creator signature */
    short version;                      /* Spool file version */
    long jobID;                         /* Unique job ID */
    unsigned long createTime;           /* File creation time */
    unsigned long modifyTime;           /* Last modification time */
    StringPtr documentName;             /* Document name */
    StringPtr userName;                 /* User name */
    StringPtr printerName;              /* Target printer name */
    TPrint printRecord;                 /* Print settings */
    long dataOffset;                    /* Offset to print data */
    long dataSize;                      /* Size of print data */
    short pageCount;                    /* Number of pages */
    short copies;                       /* Number of copies */
    short priority;                     /* Job priority */
    short status;                       /* Current status */
    long reserved[8];                   /* Reserved for future use */
};
typedef struct TSpoolFileHeader TSpoolFileHeader;
typedef TSpoolFileHeader *TSpoolFileHeaderPtr;

/* Print Queue Entry */
struct TSpoolQueueEntry {
    long jobID;                         /* Unique job ID */
    FSSpec spoolFile;                   /* Spool file specification */
    StringPtr documentName;             /* Document name */
    StringPtr userName;                 /* User name */
    StringPtr printerName;              /* Target printer name */
    unsigned long submitTime;           /* Job submission time */
    unsigned long startTime;            /* Print start time */
    unsigned long endTime;              /* Print end time */
    short status;                       /* Current job status */
    short priority;                     /* Job priority */
    short pageCount;                    /* Total pages */
    short currentPage;                  /* Current page being printed */
    short copies;                       /* Number of copies */
    short currentCopy;                  /* Current copy being printed */
    short percentComplete;              /* Completion percentage */
    long errorCode;                     /* Error code if failed */
    StringPtr errorMessage;             /* Error message */
    struct TSpoolQueueEntry *next;      /* Next entry in queue */
    struct TSpoolQueueEntry *prev;      /* Previous entry in queue */
};
typedef struct TSpoolQueueEntry TSpoolQueueEntry;
typedef TSpoolQueueEntry *TSpoolQueueEntryPtr;

/* Print Queue */
struct TPrintQueue {
    StringPtr queueName;                /* Queue name */
    StringPtr printerName;              /* Associated printer */
    TSpoolQueueEntryPtr firstEntry;     /* First entry in queue */
    TSpoolQueueEntryPtr lastEntry;      /* Last entry in queue */
    TSpoolQueueEntryPtr currentEntry;   /* Currently printing entry */
    short entryCount;                   /* Number of entries */
    short maxEntries;                   /* Maximum entries allowed */
    Boolean active;                     /* Queue is active */
    Boolean paused;                     /* Queue is paused */
    ProcessSerialNumber spoolerProcess; /* Background spooler process */
    long totalJobsProcessed;            /* Total jobs processed */
    unsigned long queueCreateTime;      /* Queue creation time */
};
typedef struct TPrintQueue TPrintQueue;
typedef TPrintQueue *TPrintQueuePtr;

/* Background Print Monitor */
struct TBackgroundPrintMonitor {
    Boolean enabled;                    /* Background printing enabled */
    Boolean active;                     /* Currently printing in background */
    ProcessSerialNumber monitorProcess; /* Monitor process */
    TPrintQueuePtr queues[16];          /* Array of print queues */
    short queueCount;                   /* Number of active queues */
    long nextJobID;                     /* Next available job ID */
    unsigned long lastActivityTime;     /* Last activity time */
    Handle statusWindow;                /* Print status window */
    Boolean showStatus;                 /* Show status window */
};
typedef struct TBackgroundPrintMonitor TBackgroundPrintMonitor;
typedef TBackgroundPrintMonitor *TBackgroundPrintMonitorPtr;

/* Spooling Functions */

/* Spool File Management */
OSErr CreateSpoolFile(StringPtr documentName, StringPtr printerName,
                      THPrint hPrint, FSSpec *spoolFile, long *jobID);
OSErr OpenSpoolFile(FSSpec *spoolFile, short *fileRefNum);
OSErr CloseSpoolFile(short fileRefNum);
OSErr DeleteSpoolFile(FSSpec *spoolFile);
OSErr WriteSpoolHeader(short fileRefNum, TSpoolFileHeaderPtr header);
OSErr ReadSpoolHeader(short fileRefNum, TSpoolFileHeaderPtr header);
OSErr WriteSpoolData(short fileRefNum, Ptr data, long size);
OSErr ReadSpoolData(short fileRefNum, Ptr data, long *size);

/* Print Queue Management */
OSErr CreatePrintQueue(StringPtr queueName, StringPtr printerName, TPrintQueuePtr *queue);
OSErr DeletePrintQueue(TPrintQueuePtr queue);
OSErr AddJobToQueue(TPrintQueuePtr queue, FSSpec *spoolFile, TSpoolQueueEntryPtr *entry);
OSErr RemoveJobFromQueue(TPrintQueuePtr queue, long jobID);
OSErr GetNextJobFromQueue(TPrintQueuePtr queue, TSpoolQueueEntryPtr *entry);
OSErr SetJobPriority(TPrintQueuePtr queue, long jobID, short priority);
OSErr GetJobStatus(TPrintQueuePtr queue, long jobID, TSpoolQueueEntryPtr *entry);

/* Queue Operations */
OSErr StartQueue(TPrintQueuePtr queue);
OSErr StopQueue(TPrintQueuePtr queue);
OSErr PauseQueue(TPrintQueuePtr queue);
OSErr ResumeQueue(TPrintQueuePtr queue);
OSErr PurgeQueue(TPrintQueuePtr queue);
OSErr GetQueueInfo(TPrintQueuePtr queue, short *entryCount, Boolean *active);

/* Background Printing */
OSErr InitBackgroundPrinting(void);
OSErr CleanupBackgroundPrinting(void);
OSErr EnableBackgroundPrinting(Boolean enable);
Boolean IsBackgroundPrintingEnabled(void);
OSErr StartBackgroundSpooler(void);
OSErr StopBackgroundSpooler(void);
OSErr GetBackgroundPrintStatus(TBackgroundPrintMonitorPtr *monitor);

/* Job Submission */
OSErr SubmitPrintJob(StringPtr documentName, StringPtr printerName,
                     THPrint hPrint, PicHandle picture, long *jobID);
OSErr SubmitSpoolJob(FSSpec *spoolFile, long *jobID);
OSErr CancelPrintJob(long jobID);
OSErr HoldPrintJob(long jobID);
OSErr ReleasePrintJob(long jobID);
OSErr RestartPrintJob(long jobID);

/* Job Monitoring */
OSErr GetPrintJobList(long jobIDs[], short *count);
OSErr GetJobProgress(long jobID, short *percentComplete, short *currentPage);
OSErr GetJobEstimatedTime(long jobID, unsigned long *estimatedSeconds);
OSErr SetJobNotification(long jobID, ProcPtr notificationProc);

/* Spooler Status */
struct TSpoolerStatus {
    Boolean active;                     /* Spooler is active */
    short queueCount;                   /* Number of queues */
    short totalJobs;                    /* Total jobs in all queues */
    short printingJobs;                 /* Jobs currently printing */
    short pendingJobs;                  /* Jobs pending */
    short errorJobs;                    /* Jobs with errors */
    long memoryUsed;                    /* Memory used by spooler */
    unsigned long upTime;               /* Spooler uptime */
};
typedef struct TSpoolerStatus TSpoolerStatus;
typedef TSpoolerStatus *TSpoolerStatusPtr;

OSErr GetSpoolerStatus(TSpoolerStatusPtr status);
OSErr RestartSpooler(void);
OSErr ResetSpooler(void);

/* Print Status Window */
OSErr ShowPrintStatusWindow(Boolean show);
OSErr UpdatePrintStatusWindow(void);
Boolean IsPrintStatusWindowVisible(void);
OSErr SetStatusWindowPosition(Point position);

/* Spool File Utilities */
OSErr ValidateSpoolFile(FSSpec *spoolFile);
OSErr RepairSpoolFile(FSSpec *spoolFile);
OSErr ConvertSpoolFile(FSSpec *oldFile, FSSpec *newFile, short newVersion);
OSErr CompressSpoolFile(FSSpec *spoolFile);
OSErr DecompressSpoolFile(FSSpec *spoolFile);

/* Print Preview from Spool */
OSErr PreviewSpoolFile(FSSpec *spoolFile, WindowPtr previewWindow);
OSErr GetSpoolFilePageCount(FSSpec *spoolFile, short *pageCount);
OSErr DrawSpoolFilePage(FSSpec *spoolFile, short pageNumber, Rect *drawRect);

/* Network Printing Support */
OSErr SubmitNetworkPrintJob(StringPtr serverName, StringPtr queueName,
                           StringPtr documentName, THPrint hPrint,
                           PicHandle picture, long *jobID);
OSErr GetNetworkQueueStatus(StringPtr serverName, StringPtr queueName,
                           TSpoolerStatusPtr status);
OSErr CancelNetworkPrintJob(StringPtr serverName, long jobID);

/* Print Accounting */
struct TPrintAccountingInfo {
    StringPtr userName;                 /* User name */
    StringPtr documentName;             /* Document name */
    StringPtr printerName;              /* Printer name */
    unsigned long printTime;            /* Print time */
    short pageCount;                    /* Number of pages */
    short copies;                       /* Number of copies */
    long printCost;                     /* Print cost (if applicable) */
    Boolean colorUsed;                  /* Color printing used */
    long paperUsed;                     /* Paper sheets used */
    long inkUsed;                       /* Ink/toner used */
};
typedef struct TPrintAccountingInfo TPrintAccountingInfo;
typedef TPrintAccountingInfo *TPrintAccountingInfoPtr;

OSErr RecordPrintJob(TPrintAccountingInfoPtr info);
OSErr GetPrintAccountingReport(unsigned long startTime, unsigned long endTime,
                              TPrintAccountingInfoPtr report[], short *count);

/* Error Recovery */
OSErr RecoverFailedJobs(void);
OSErr SaveSpoolerState(void);
OSErr RestoreSpoolerState(void);
OSErr CleanupOrphanedFiles(void);

/* Configuration */
OSErr SetSpoolFolder(FSSpec *folder);
OSErr GetSpoolFolder(FSSpec *folder);
OSErr SetMaxSpoolFileSize(long maxSize);
long GetMaxSpoolFileSize(void);
OSErr SetSpoolFileLifetime(unsigned long lifetime);
unsigned long GetSpoolFileLifetime(void);

#ifdef __cplusplus
}
#endif

#endif /* __PRINTSPOOLING__ */