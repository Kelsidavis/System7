/*
 * Date & Time Control Panel Resources
 *
 * RE-AGENT-BANNER
 * Source: Date & Time.rsrc (SHA256: 9c3392e59c8b6bed0774aa9a654ceaf569ca7f10f5718499fdd5e4cd4b411fea)
 * Evidence: /home/k/Desktop/system7/evidence.curated.datetime.json
 * Resource definitions for Date & Time control panel
 */

#ifndef DATETIME_RESOURCES_H
#define DATETIME_RESOURCES_H

/* Resource IDs */
#define kDateTimeDLOG           128     /* Main dialog resource */
#define kDateTimeDITL           129     /* Dialog item list */
#define kDateTimeSTR            128     /* Help strings */
#define kDateTimeICON           128     /* Control panel icon */

/* String Resource IDs for Help Text */
#define kDateFormatHelpStr      1
#define kTimeFormatHelpStr      2
#define kCurrentDateHelpStr     3
#define kCurrentTimeHelpStr     4
#define kControlPanelDescStr    5

/* Dialog Item Numbers */
#define kDateDisplayItem        1
#define kTimeDisplayItem        2
#define kDateFormatButton       3
#define kTimeFormatButton       4
#define kStatusDisplayItem      5
#define kHelpTextItem           6

/* Menu Resource IDs */
#define kDateFormatMenu         128
#define kTimeFormatMenu         129

/* DITL Item Types */
#define kUserItem               0
#define kButtonItem             4
#define kCheckBoxItem           5
#define kRadioButtonItem        6
#define kStaticTextItem         8
#define kEditTextItem           16

/* Control Panel Signature */
#define kCDevSignature          'time'
#define kCDevType               'cdev'

/* Version Information */
#define kCDevVersion            1

#endif /* DATETIME_RESOURCES_H */