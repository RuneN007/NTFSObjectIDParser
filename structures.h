typedef struct _INDX_FILE_HEADER
{
    qint32 Signature;   // 0-3 (char)
    qint16 OffsetUpdateSequenceArray; // 4-5 (short)
    qint16 EntriesUpdateSequenceArray; // 6-7
    quint64 LogfileSequenceNumber; //8-15
    quint64 VCN; //16-23
    qint32 OffsetIndexEntryHeader; //24-27, Counting from offset 24
    qint32 OffsetEndLastIndexEntry; // 28-31, Counting from offset 24
    qint32 AllocatedSizeIndexEntries; // 32-35
    qint8 IndexTypeFlag; //36, small or large
    qint8 Padding1[3]; // 37-39
    qint16 UpdateSequenceArray[9]; // 40-57. First entry is the Update Sequence Number
    qint8 Padding[6]; // 58-63
} INDX_FILE_HEADER;  // Total size is 64 or 0x40 bytes

typedef struct _INDX_ROOT_HEADER
{
    qint32 AttributeType; // Should be zeros
    qint32 Sorting; // Seen 0x13
    qint32 indxRecordSizeBytes; // Seen 0x1000 (4096)
    qint8 indexRecordSizeClusters; // Seen 0x01
    qint8 reserved[3];
    qint32 OffsetIndexEntryHeader; //Counting from this offset 0x10
    qint32 OffsetEndLastIndexEntry; // Counting from offset 0x10
    qint32 AllocatedSizeIndexEntries; // Counting from offset 0x10
    qint8 IndexTypeFlag; //small or large
    qint8 Padding1[3]; // 37-39
} INDX_ROOT_HEADER; // Total size of 0x10

typedef struct _INDX_ENTRY
{
    qint16 OffsetData;
    qint16 SizeData;
    qint8 Padding1[4];
    qint16 SizeIndexEntry;
    qint16 SizeIndexKey; // Size of the Objetct ID
    qint16 Flags;
    qint16 Padding;
    qint8 ObjectID[16];
    quint64 MFTRecord;
    qint8 BirthVolumeID[16];
    qint8 BirthObjectID[16];
    qint8 DomainID[16];
} INDX_ENTRY;   // Total of 64 or 0x40 bytes


typedef struct _MFT_RECORD_HEADER
{
    qint8 Signature[4]; // FILE
    qint16 UpdateSequenceArrayOffset; // Offset to Fixup Array
    qint16 UpdateSequenceEntries; // Normally 2 in a MFT Record
    quint64 LSN;
    qint16 SequenceCount; // Number of times the record has been used
    qint16 HardlinkCount; // One for each filename pointing to this record
    qint16 OffsetToStartAttribute;
    qint16 Flags; // 0x00 = Deleted file, 0x01 = Allocated file, 0x02 Deleted Directory, 0x03 Allocated Directory
    qint32   AcctualSize;    // for this MFT Record
    qint32   AllocatedSpace; // for this MFT Record
    quint64 BaseFileReference; // Points to the base NFT Record if it use multiple entries
    qint16 NextAttributeID; // Does not decrease if an attribute is deleted
    qint16 Reserved;
    quint32   MftRecordNumber;
    quint16 UpdateSequenceArray[3];
    qint16 Padding; // Must be a 8 byte boundary
} MFT_RECORD_HEADER;  // 0x38 in size


typedef struct _MFT_ATTRIBUTE_HEADER
{
    qint32  AttributeType;
    qint32  AttributeLength;
    qint8 NonResidentFlag; // 0x01 = Non Resident, 0x00 = Resident
    qint8 LengthOfStreamName;
    qint16 OffetToStreamName;
    qint16 Flags;
    qint16 AttributeID;
    qint32   SizeOfResidentData;
    qint16 OffsetToResidentData;
    qint8 Indexed; // 0x01 = Yes, this file is also in an INDX record
    qint8 Padding; // Reserved
} MFT_ATTRIBUTE_HEADER;


typedef struct _OBJECT_IDENTIFIER_ATTRIBUTE_CONTENT
{
   qint8 ObjectID[16];
}OBJECT_IDENTIFIER_ATTRIBUTE_CONTENT;

typedef struct _FILE_NAME_ATTRIBUTE_CONTENT
{
    quint64 ParentMftRecord; // only the first 6
    quint64 CreationDate;
    quint64 LastModifiedDate;
    quint64 MFTRecordModifiedDate;
    quint64 LastAccessDate;
    quint64 AllocatedSize; // Physical
    quint64 ActualSize; // Used size
    qint32 FileTypeFlags;
    qint32 ReparseValue;
    qint8 LengthOfFilename; // In Unicode elements
    qint8 FilenameType; // 0x00 = Posix, 0x01=Win32, 0x02 DOS Short name, 0x03=Win32/DOS
	wchar_t * Filename; // Memory must be allocated
}FILE_NAME_ATTRIBUTE_CONTENT;


typedef struct _STANDARD_INFORMATION_ATTRIBUTE_CONTENT
{
    quint64 CreationDate;
    quint64 LastModifiedDate;
    quint64 MFTRecordModifiedDate;
    quint64 LastAccessDate;
    qint32 DosFlags;
    qint32 MaxVersions;
    qint32 VersionNumber;
    qint32 ClassID;
    qint32 OwnerID;
    qint32 SecurityID;
    quint64 QuotaCharged;
    quint64 SequenceNumber;
} STANDARD_INFORMATION_ATTRIBUTE_CONTENT;
