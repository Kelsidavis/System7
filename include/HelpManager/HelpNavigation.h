/*
 * HelpNavigation.h - Help Navigation and Cross-References
 *
 * This file defines structures and functions for help navigation, hyperlinks,
 * cross-references, and help topic hierarchies.
 */

#ifndef HELPNAVIGATION_H
#define HELPNAVIGATION_H

#include "MacTypes.h"
#include "HelpManager.h"
#include "HelpContent.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Navigation link types */
typedef enum {
    kHMNavLinkNone = 0,              /* No link */
    kHMNavLinkTopic = 1,             /* Link to help topic */
    kHMNavLinkResource = 2,          /* Link to help resource */
    kHMNavLinkURL = 3,               /* Link to external URL */
    kHMNavLinkApplication = 4,       /* Link to application help */
    kHMNavLinkDefinition = 5,        /* Link to definition/glossary */
    kHMNavLinkProcedure = 6,         /* Link to procedure/how-to */
    kHMNavLinkCrossRef = 7,          /* Cross-reference link */
    kHMNavLinkParent = 8,            /* Link to parent topic */
    kHMNavLinkChild = 9,             /* Link to child topic */
    kHMNavLinkSibling = 10,          /* Link to sibling topic */
    kHMNavLinkIndex = 11,            /* Link to help index */
    kHMNavLinkSearch = 12            /* Link to search results */
} HMNavLinkType;

/* Navigation action types */
typedef enum {
    kHMNavActionShow = 1,            /* Show help topic */
    kHMNavActionReplace = 2,         /* Replace current help */
    kHMNavActionPopup = 3,           /* Show in popup window */
    kHMNavActionNewWindow = 4,       /* Show in new window */
    kHMNavActionExternal = 5,        /* Open in external application */
    kHMNavActionCallback = 6         /* Call application callback */
} HMNavAction;

/* Navigation link structure */
typedef struct HMNavLink {
    HMNavLinkType linkType;          /* Type of link */
    HMNavAction action;              /* Action to perform */
    char linkID[64];                 /* Link identifier */
    char linkText[256];              /* Link display text */
    char linkTarget[256];            /* Link target */
    char linkDescription[512];       /* Link description */
    ResType resourceType;            /* Resource type (if applicable) */
    short resourceID;                /* Resource ID (if applicable) */
    short resourceIndex;             /* Resource index (if applicable) */
    Boolean isEnabled;               /* Link is enabled */
    Boolean isVisited;               /* Link has been visited */
    long linkData;                   /* Custom link data */
    struct HMNavLink *next;          /* Next link in list */
} HMNavLink;

/* Help topic hierarchy node */
typedef struct HMNavNode {
    char topicID[64];                /* Topic identifier */
    char topicTitle[256];            /* Topic title */
    char topicDescription[512];      /* Topic description */
    short topicLevel;                /* Hierarchy level (0=root) */
    ResType resourceType;            /* Help resource type */
    short resourceID;                /* Help resource ID */
    HMNavLink *links;                /* Links from this topic */
    struct HMNavNode *parent;        /* Parent topic */
    struct HMNavNode *firstChild;    /* First child topic */
    struct HMNavNode *nextSibling;   /* Next sibling topic */
    struct HMNavNode *prevSibling;   /* Previous sibling topic */
    Boolean isExpanded;              /* Node is expanded in tree */
    Boolean isVisible;               /* Node is visible */
    long visitCount;                 /* Number of visits */
    long lastVisitTime;              /* Last visit time */
} HMNavNode;

/* Navigation history entry */
typedef struct HMNavHistoryEntry {
    char topicID[64];                /* Topic identifier */
    char topicTitle[256];            /* Topic title */
    ResType resourceType;            /* Resource type */
    short resourceID;                /* Resource ID */
    Point scrollPosition;            /* Scroll position */
    long visitTime;                  /* Visit time */
    struct HMNavHistoryEntry *next;  /* Next history entry */
    struct HMNavHistoryEntry *prev;  /* Previous history entry */
} HMNavHistoryEntry;

/* Navigation bookmark */
typedef struct HMNavBookmark {
    char bookmarkID[64];             /* Bookmark identifier */
    char bookmarkName[256];          /* Bookmark display name */
    char topicID[64];                /* Topic identifier */
    char topicTitle[256];            /* Topic title */
    ResType resourceType;            /* Resource type */
    short resourceID;                /* Resource ID */
    Point scrollPosition;            /* Scroll position */
    long creationTime;               /* Bookmark creation time */
    char notes[512];                 /* User notes */
    struct HMNavBookmark *next;      /* Next bookmark */
} HMNavBookmark;

/* Navigation state */
typedef struct HMNavState {
    HMNavNode *rootNode;             /* Root of topic hierarchy */
    HMNavNode *currentNode;          /* Current topic node */
    HMNavHistoryEntry *historyHead;  /* History list head */
    HMNavHistoryEntry *historyCurrent; /* Current history position */
    HMNavBookmark *bookmarks;        /* Bookmark list */
    short maxHistoryEntries;         /* Maximum history entries */
    short historyCount;              /* Current history count */
    short bookmarkCount;             /* Current bookmark count */
    Boolean navigationEnabled;       /* Navigation is enabled */
    Boolean trackHistory;            /* Track navigation history */
    Boolean allowBookmarks;          /* Allow user bookmarks */
} HMNavState;

/* Link callback function */
typedef OSErr (*HMNavLinkCallback)(const HMNavLink *link, void *userData);

/* Topic provider callback */
typedef OSErr (*HMNavTopicProvider)(const char *topicID, HMNavNode **node);

/* Navigation initialization */
OSErr HMNavInit(short maxHistoryEntries);
void HMNavShutdown(void);

/* Navigation state management */
OSErr HMNavGetState(HMNavState **state);
OSErr HMNavSaveState(void);
OSErr HMNavRestoreState(void);
OSErr HMNavResetState(void);

/* Topic hierarchy management */
OSErr HMNavCreateNode(const char *topicID, const char *title,
                     ResType resourceType, short resourceID,
                     HMNavNode **node);

OSErr HMNavAddChild(HMNavNode *parent, HMNavNode *child);
OSErr HMNavRemoveChild(HMNavNode *parent, HMNavNode *child);
OSErr HMNavMoveNode(HMNavNode *node, HMNavNode *newParent);

OSErr HMNavFindNode(const char *topicID, HMNavNode **node);
OSErr HMNavFindNodeByResource(ResType resourceType, short resourceID,
                            HMNavNode **node);

OSErr HMNavGetRoot(HMNavNode **rootNode);
OSErr HMNavSetRoot(HMNavNode *rootNode);

/* Navigation traversal */
OSErr HMNavGoToTopic(const char *topicID);
OSErr HMNavGoToResource(ResType resourceType, short resourceID);
OSErr HMNavGoToParent(void);
OSErr HMNavGoToChild(short childIndex);
OSErr HMNavGoToSibling(short siblingIndex);

OSErr HMNavGetCurrent(HMNavNode **currentNode);
OSErr HMNavGetParent(HMNavNode **parentNode);
OSErr HMNavGetChildren(HMNavNode ***children, short *childCount);
OSErr HMNavGetSiblings(HMNavNode ***siblings, short *siblingCount);

/* Link management */
OSErr HMNavAddLink(HMNavNode *fromNode, const HMNavLink *link);
OSErr HMNavRemoveLink(HMNavNode *fromNode, const char *linkID);
OSErr HMNavGetLinks(HMNavNode *node, HMNavLink ***links, short *linkCount);
OSErr HMNavFindLink(HMNavNode *node, const char *linkID, HMNavLink **link);

OSErr HMNavFollowLink(const HMNavLink *link);
OSErr HMNavRegisterLinkCallback(HMNavLinkType linkType, HMNavLinkCallback callback);

/* History management */
OSErr HMNavHistoryAdd(const char *topicID, ResType resourceType, short resourceID);
OSErr HMNavHistoryBack(void);
OSErr HMNavHistoryForward(void);
OSErr HMNavHistoryGoTo(short historyIndex);

OSErr HMNavHistoryGetCurrent(HMNavHistoryEntry **entry);
OSErr HMNavHistoryGetList(HMNavHistoryEntry ***entries, short *entryCount);
OSErr HMNavHistoryClear(void);

Boolean HMNavHistoryCanGoBack(void);
Boolean HMNavHistoryCanGoForward(void);

/* Bookmark management */
OSErr HMNavBookmarkAdd(const char *bookmarkName, const char *topicID,
                      ResType resourceType, short resourceID);
OSErr HMNavBookmarkRemove(const char *bookmarkID);
OSErr HMNavBookmarkGoTo(const char *bookmarkID);

OSErr HMNavBookmarkGetList(HMNavBookmark ***bookmarks, short *bookmarkCount);
OSErr HMNavBookmarkFind(const char *bookmarkID, HMNavBookmark **bookmark);
OSErr HMNavBookmarkRename(const char *bookmarkID, const char *newName);

/* Topic search and indexing */
OSErr HMNavSearchTopics(const char *searchText, HMNavNode ***results, short *resultCount);
OSErr HMNavSearchContent(const char *searchText, char ***results, short *resultCount);
OSErr HMNavBuildIndex(void);
OSErr HMNavGetIndex(char ***indexEntries, short *entryCount);

/* Cross-reference management */
OSErr HMNavAddCrossRef(const char *fromTopic, const char *toTopic,
                      const char *linkText);
OSErr HMNavRemoveCrossRef(const char *fromTopic, const char *toTopic);
OSErr HMNavGetCrossRefs(const char *topicID, HMNavLink ***crossRefs, short *refCount);

/* Topic validation */
Boolean HMNavValidateNode(const HMNavNode *node);
Boolean HMNavValidateHierarchy(const HMNavNode *rootNode);
OSErr HMNavRepairHierarchy(HMNavNode *rootNode);

/* Navigation preferences */
OSErr HMNavSetPreference(const char *prefName, const char *prefValue);
OSErr HMNavGetPreference(const char *prefName, char *prefValue, short maxLength);

OSErr HMNavSetHistorySize(short maxEntries);
short HMNavGetHistorySize(void);

OSErr HMNavSetTrackingEnabled(Boolean enabled);
Boolean HMNavGetTrackingEnabled(void);

/* Topic loading and caching */
OSErr HMNavPreloadTopic(const char *topicID);
OSErr HMNavPreloadChildren(const char *topicID);
OSErr HMNavFlushCache(void);

OSErr HMNavRegisterTopicProvider(HMNavTopicProvider provider);
OSErr HMNavLoadTopicOnDemand(const char *topicID, HMNavNode **node);

/* Navigation UI support */
OSErr HMNavGetBreadcrumb(char *breadcrumb, short maxLength);
OSErr HMNavGetNavigationMenu(MenuHandle *navMenu);
OSErr HMNavUpdateNavigationUI(void);

/* Export and import */
OSErr HMNavExportHierarchy(const char *filePath);
OSErr HMNavImportHierarchy(const char *filePath);
OSErr HMNavExportBookmarks(const char *filePath);
OSErr HMNavImportBookmarks(const char *filePath);

/* Modern navigation support */
OSErr HMNavSupportHyperlinks(Boolean enable);
OSErr HMNavSupportURLs(Boolean enable);
OSErr HMNavSetDefaultBrowser(const char *browserPath);

/* Accessibility navigation */
OSErr HMNavSetAccessibilityMode(Boolean enabled);
OSErr HMNavGetAccessibleDescription(const HMNavNode *node, char *description,
                                  short maxLength);
OSErr HMNavAnnounceNavigation(const HMNavNode *node);

/* Navigation debugging */
OSErr HMNavDumpHierarchy(void);
OSErr HMNavValidateNavigation(void);
OSErr HMNavGetStatistics(long *nodeCount, long *linkCount, long *historyCount,
                        long *bookmarkCount);

/* Node disposal */
void HMNavDisposeNode(HMNavNode *node);
void HMNavDisposeHierarchy(HMNavNode *rootNode);
void HMNavDisposeLink(HMNavLink *link);

#ifdef __cplusplus
}
#endif

#endif /* HELPNAVIGATION_H */