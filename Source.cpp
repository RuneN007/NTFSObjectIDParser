#include "mainwindow.h"
#include "ui_mainwindow.h"
#ifdef Q_OS_WIN
  #include <time.h>
#endif
#include <QFile>






// main program
qint32 MainWindow::parseObjID(QString ObjIDfileName, QString MFTFileName)
{


	// Open file
    QFile ObjFile(ObjIDfileName);
    QFile MFTFile(MFTFileName);



    // Read both files inta a bytearray
    if(!ObjFile.open(QIODevice::ReadOnly)){
        ui->lblObjID->setText("Unable to open file");
        return -1; // Not opened
    }
    QByteArray ObjIDData = ObjFile.readAll();
    QDataStream ObjIDDataStream(ObjIDData);

    if(!MFTFile.open(QIODevice::ReadOnly)){
        return -1; // Not opened
    }
    QByteArray MFTData = MFTFile.readAll();

    if(ObjIDData.size() < 0x40){
        ui->lblObjID->setText("Not valid");
        return -1;
    }

	INDX_FILE_HEADER objidFileHeader;  // See structures.h file
    INDX_ROOT_HEADER objidRootHeader;
	INDX_ENTRY indxEntry;
	MFT_RECORD_HEADER mftHeader;
	MFT_ATTRIBUTE_HEADER attrHeader;

    // Set progressbar
    qint64 maxSize = ObjIDData.size();
    ui->progressBar->setMaximum(maxSize);
    ui->progressBar->show();


	// For now we only support one Indx size of 4096 bytes
	char outIndx[0x1000];
	// Set posistion in file
    qint32 pos = 0;
    quint32 indxNumber = 0; // We start on Index page 0, each page is 4096 bytes

    while (pos < maxSize)
	{

        // Make sure we start on correct Indx
        pos = indxNumber*0x1000L; // Make sure we start on correct index page

        ui->progressBar->setValue(pos);
        ui->progressBar->hide();
        ui->progressBar->show();
        QApplication::processEvents(); // Make sure the progressbar work/updates

        if ( ( pos + 0x40) > maxSize ){ // Should only be true at eof
            pos +=40; // Which will quit the while loop
            continue;
        }
		
		// copy the info from the memory buffer to the structure ObjectId File Header
        memcpy(&objidFileHeader, ObjIDData.data() + pos, 0x40);
        memcpy(&objidRootHeader, ObjIDData.data() + pos, 0x20);

		// Check if this is a null Index, no records...
		if (objidFileHeader.OffsetUpdateSequenceArray != 0x28) // It should be 0x28 always
		{
            if(objidFileHeader.Signature != 0x00){
                indxNumber++; // We try next index page
                continue;
            }
		}

		// Print the output to file
        // printf("\nAll values in the header are interpreted with the endian order of your computer\n\n");

        if(objidFileHeader.Signature != 0x00){
            printIndxHeader(&objidFileHeader);

            // Change file pointer position to start of index page
            pos = indxNumber*0x1000L; // Make sure we start on correct index page

            if ( (pos + 0x1000) > maxSize ){
                pos +=0x1000; // Not a complete block left, so we make sure we go out of the while loop
                continue;
            }

        }
        // copy the info from the memory buffer to the structure ObjectId File Header

        if(objidFileHeader.Signature == 0x00){
            // We have two use cases; a cluster without any indexes or an Index Root Attribute
            if(objidRootHeader.Sorting == 0x13){
                memcpy(&outIndx, ObjIDData.data(), ObjIDData.size()); // will never be larger than 0x1000
                // QuickFix; We are parsing the objidFileHeader in the while loop below, but this is from the IndexRoot attribute
                objidFileHeader.OffsetEndLastIndexEntry = objidRootHeader.OffsetEndLastIndexEntry;
                pos += objidRootHeader.OffsetIndexEntryHeader + 0x10;
            } else{
                pos = maxSize +1; // Not a valid block, so we make sure we go out of the while loop
                continue;
            }
        }else{
            memcpy(&outIndx, ObjIDData.data() + pos, 0x1000);
            // Fixup the Update Sequence Number found in the last two bytes of each sector
            // with the values that should be there.
            fixUpUSN(outIndx, &objidFileHeader, 0, 512);

            pos = objidFileHeader.OffsetIndexEntryHeader + 0x18;
        }




        // Now we can read all the indx entries

		// Read and print all entries
		
        // printf("\nAll object ID UUID values in the index entries are not interpreted using the endian ordering of your computer. They are shown using Big Endian\n\n");
		while (pos < objidFileHeader.OffsetEndLastIndexEntry)
		{
            // Double check that we have enough space for an entry
            if ( ( pos + 0x58) > maxSize ){ // Should only be true at eof
                pos +=58; // Which will quit the while loop
                continue;
            }
			// copy the info from the memory buffer to the structure indxEntry
			memcpy(&indxEntry, &outIndx[pos], sizeof(INDX_ENTRY));

			// Print this entry to standard out
			printIndxEntry(&indxEntry, pos, indxNumber);
            printMftRecord(&indxEntry, MFTData, &mftHeader, &attrHeader);

			// Update pos to the next Entry, should follow this entry
			pos += indxEntry.SizeIndexEntry; // It could be bigger than sizeof(INDX_ENTRY)
		} // while

		indxNumber++; // To be able to go to next index page

	} // while not eof
	// Best practice is to close an open file handle. We are done reading from the file
    ObjFile.close();
    MFTFile.close();
    ui->progressBar->setValue(maxSize);
    ui->progressBar->show();
    return 0;

} // main

QString MainWindow::printFullPath(quint32 MftRecordNumber, QByteArray * MFTData, QString FullPath){
    FILE_NAME_ATTRIBUTE_CONTENT fnaContent;
    MFT_RECORD_HEADER mftHeader;
    MFT_ATTRIBUTE_HEADER attrHeader;
    quint64 pos = 0;
    pos = MftRecordNumber * 0x400; // Make sure we start on the correct MFT Record
    quint32 mftpos = 0;

    if (MFTData->count() < pos )
        return "";
    // copy the info from the memory buffer to the structure mftHeader
    memcpy(&mftHeader, MFTData->data() + pos, sizeof(MFT_RECORD_HEADER));

    mftpos = mftHeader.OffsetToStartAttribute;

    // Capture the first Attribute Header
    memcpy(&attrHeader, MFTData->data() + pos + mftpos, sizeof(MFT_ATTRIBUTE_HEADER));

    while (attrHeader.AttributeType != 0x30 && attrHeader.AttributeLength > 0  && mftpos < 1023){
        mftpos += attrHeader.AttributeLength; // Skip all attributes before the FNA
        memcpy(&attrHeader, MFTData->data() + pos + mftpos, sizeof(MFT_ATTRIBUTE_HEADER));
    }

    //Capture the first FNA
    memcpy(&fnaContent, MFTData->data() + pos + mftpos + attrHeader.OffsetToResidentData, sizeof(FILE_NAME_ATTRIBUTE_CONTENT));

    while(fnaContent.FilenameType == 0x02 && attrHeader.AttributeType == 0x30){ // Short name, we do not want this
        mftpos += attrHeader.AttributeLength; // Skip all attributes before the FNA
        memcpy(&attrHeader, MFTData->data() + pos + mftpos, sizeof(MFT_ATTRIBUTE_HEADER));
        //Capture the next FNA
        memcpy(&fnaContent, MFTData->data() + pos + mftpos + attrHeader.OffsetToResidentData, sizeof(FILE_NAME_ATTRIBUTE_CONTENT));
    }
    fnaContent.Filename = (wchar_t *)malloc((fnaContent.LengthOfFilename + 1)  * sizeof(wchar_t));

    // After the stream name which is empty, the filename starts
    memcpy(fnaContent.Filename, MFTData->data() + pos + mftpos  + 0x5A, (fnaContent.LengthOfFilename) * sizeof(wchar_t));

    // Make sure the Filename ends with the 0 terminator
    fnaContent.Filename[fnaContent.LengthOfFilename] = (wchar_t)'\0';

    if(mftHeader.MftRecordNumber != 5){
       // Recursive
       FullPath = printFullPath(get48bits(&fnaContent.ParentMftRecord), MFTData, FullPath);
    }


    // Now it should be equal to the root first time its here

    FullPath += "\\" + QString::fromUtf16((quint16*)fnaContent.Filename,fnaContent.LengthOfFilename ) ;

    return FullPath;

}


void MainWindow::printMftRecord(const INDX_ENTRY * objEntry, QByteArray MFTData, MFT_RECORD_HEADER * mftHeader, MFT_ATTRIBUTE_HEADER * attrHeader)
{
	//clearerr(mftfp); // In case of a previous error
    static FILE_NAME_ATTRIBUTE_CONTENT fnaContent;
	static STANDARD_INFORMATION_ATTRIBUTE_CONTENT siaContent;
    static OBJECT_IDENTIFIER_ATTRIBUTE_CONTENT oiaContent;

    quint64 pos = 0;
    quint16 sequenceNumber = 0;

    if( ui->chkUTC->isChecked() ){
        wantlocaltime = false;
    }else
    {
        wantlocaltime = true;
    }

	//rewind(mftfp);
    pos = get48bits(&objEntry->MFTRecord) * 0x400; // Make sure we start on the correct MFT Record

    sequenceNumber = (objEntry->MFTRecord & 0xFFFF000000000000) >> 48;


	char recBuffer[0x400]; // Normal size of a MFT Record
    for (qint32 i = 0; i < 0x400; i++)
	{
        recBuffer[i] = MFTData.at(pos + i);
	}



	// copy the info from the memory buffer to the structure mftHeader
	memcpy(mftHeader, &recBuffer[0], sizeof(MFT_RECORD_HEADER));

    if(mftHeader->SequenceCount != sequenceNumber){
      return; // No point in parsing MFT record, the mapping is not reliable
    }

	// Yes, also the MFT records have this Update Sequence Numbers
	fixUpUSN(&recBuffer[0], mftHeader, 0, 512);



	printMFTHeader(mftHeader);

	// We will not use SIA, which is the first attribute always
	// when references to a MFT Record from the $ObjID:$O file. We just need to find the next
	memcpy(attrHeader, &recBuffer[mftHeader->OffsetToStartAttribute], sizeof(MFT_ATTRIBUTE_HEADER));

    qint32 mftpos = mftHeader->OffsetToStartAttribute;

	memcpy(&siaContent, &recBuffer[mftHeader->OffsetToStartAttribute + sizeof(MFT_ATTRIBUTE_HEADER)], sizeof(STANDARD_INFORMATION_ATTRIBUTE_CONTENT));
	if (wantlocaltime){
        // printf(" -> MFT Record Standard Information Attribute (local time): \n");
	}
	else{
        // printf(" -> MFT Record Standard Information Attribute (UTC): \n");
	}

    QList<QStandardItem *> fieldSIAList;


    QStandardItem *mftRecord = new QStandardItem(QString::number(mftHeader->MftRecordNumber) );
    fieldSIAList.append(mftRecord);

    QStandardItem *mftPosition = new QStandardItem("MFT offset = " + QString::number(pos) + "+"  + QString::number(mftpos));
    fieldSIAList.append(mftPosition);


    QStandardItem *siaAttrib = new QStandardItem("Standard Information Attribute");
    fieldSIAList.append(siaAttrib);

    QString allocationStatus;
    // 0 = Deleted file, 1= Allocated file, 2 = Deleted Dir, 3 = Allocated Dir
    if(is_bit_set(mftHeader->Flags, 0) ){
        allocationStatus = "Allocated ";
    }else{
        allocationStatus = "Unallocated ";
    }

    if(is_bit_set(mftHeader->Flags, 1) ){
        allocationStatus += "Directory";
    }else{
        allocationStatus += "File";
    }



    QStandardItem *mftFlags = new QStandardItem(allocationStatus);
    fieldSIAList.append(mftFlags);

    if(siaContent.CreationDate == siaContent.LastModifiedDate == siaContent.LastAccessDate){
        QStandardItem *siaEqualDates = new QStandardItem("Created, and content not changed");
        fieldSIAList.append(siaEqualDates);
    }else{
        if(siaContent.CreationDate < siaContent.LastModifiedDate ){
            QStandardItem *siaContentChanged = new QStandardItem("Content changed after creation");
            fieldSIAList.append(siaContentChanged);
        }else{
            fieldSIAList.append(NULL);
        }
    }

    QStandardItem *aSiaName = new QStandardItem("" );
    fieldSIAList.append(aSiaName);

    QStandardItem *aSIAFileCreated = new QStandardItem(returnDateAsString(siaContent.CreationDate, wantlocaltime));
    fieldSIAList.append(aSIAFileCreated);

    QStandardItem *aSIALastModifiedDate = new QStandardItem(returnDateAsString(siaContent.LastModifiedDate, wantlocaltime));
    fieldSIAList.append(aSIALastModifiedDate);

    QStandardItem *aSIAMFTRecordModifiedDate = new QStandardItem(returnDateAsString(siaContent.MFTRecordModifiedDate, wantlocaltime));
    fieldSIAList.append(aSIAMFTRecordModifiedDate);

    QStandardItem *aSIALastAccessDate = new QStandardItem(returnDateAsString(siaContent.LastAccessDate, wantlocaltime));
    fieldSIAList.append(aSIALastAccessDate);



    // printf("  ----> SIA File Created: %s\n", returnDateAsString(siaContent.CreationDate, wantlocaltime));
    // printf("  ----> SIA File Modified: %s\n", returnDateAsString(siaContent.LastModifiedDate, wantlocaltime));
    // printf("  ----> SIA MFT Record Modified: %s\n", returnDateAsString(siaContent.MFTRecordModifiedDate, wantlocaltime));
    // printf("  ----> SIA File Accessed: %s\n", returnDateAsString(siaContent.LastAccessDate, wantlocaltime));


    model->insertRow(rowcounter++, fieldSIAList);
    for(int i=0; i< model->columnCount(); i++){
            model->setData(model->index(rowcounter-1,i), *bgcolor, Qt::BackgroundRole);
    }




	if (wantlocaltime){
        // printf(" -> MFT Record File Name Attribute (local time): \n"); // We will find filenames
	}
	else{
        // printf(" -> MFT Record File Name Attribute (UTC): \n"); // We will find filenames
	}
	

	while ( mftpos < 1023)
	{
		mftpos += attrHeader->AttributeLength; // Go to next attribute
		memcpy(attrHeader, &recBuffer[mftpos], sizeof(MFT_ATTRIBUTE_HEADER));


		if (attrHeader->AttributeType == 0x30 && attrHeader->AttributeLength > 0)
		{
			
            memcpy(&fnaContent, &recBuffer[mftpos + attrHeader->OffsetToResidentData], sizeof(FILE_NAME_ATTRIBUTE_CONTENT));

			// We are still missing the filename, and we need to allocate space for it
			fnaContent.Filename = (wchar_t *)malloc((fnaContent.LengthOfFilename + 1)  * sizeof(wchar_t));

			// After the stream name which is empty, the filename starts
            memcpy(fnaContent.Filename, &recBuffer[mftpos + 0x5A], (fnaContent.LengthOfFilename) * sizeof(wchar_t));

			// Set a nullterminator, which is not there by standard
            fnaContent.Filename[fnaContent.LengthOfFilename] = (wchar_t)'\0'; // This is why we allocated one more byte above
			
			// We need to use wide print function because the filename is in wide characters (Unicode)

            QList<QStandardItem *> fieldList;

            QStandardItem *mftRecord = new QStandardItem(QString::number(mftHeader->MftRecordNumber) );
            fieldList.append(mftRecord);

            QStandardItem *mftFNAPosition = new QStandardItem("MFT offset = " + QString::number(pos) + "+"  + QString::number(mftpos));
            fieldList.append(mftFNAPosition);

            QStandardItem *fnaAttrib = new QStandardItem("File Name Attribute");
            fieldList.append(fnaAttrib);

            QStandardItem *mftFNAFlags = new QStandardItem(allocationStatus);
            fieldList.append(mftFNAFlags);

            fieldList.append(NULL);


            QString fname = QString::fromUtf16((quint16*)fnaContent.Filename,fnaContent.LengthOfFilename );
            quint32 parentRecord = get48bits(&fnaContent.ParentMftRecord);
            QStandardItem *aFileName = new QStandardItem(printFullPath(parentRecord, &MFTData, "") + "\\" + fname );
            fieldList.append(aFileName);




            QStandardItem *aFileCreated = new QStandardItem(returnDateAsString(fnaContent.CreationDate, wantlocaltime));
            fieldList.append(aFileCreated);

            QStandardItem *aLastModifiedDate = new QStandardItem(returnDateAsString(fnaContent.LastModifiedDate, wantlocaltime));
            fieldList.append(aLastModifiedDate);

            QStandardItem *aMFTRecordModifiedDate = new QStandardItem(returnDateAsString(fnaContent.MFTRecordModifiedDate, wantlocaltime));
            fieldList.append(aMFTRecordModifiedDate);

            QStandardItem *aLastAccessDate = new QStandardItem(returnDateAsString(fnaContent.LastAccessDate, wantlocaltime));
            fieldList.append(aLastAccessDate);

            // wprintf(L"  --> %s\n", fnaContent.Filename);

            // printf("  ----> FNA File Created: %s\n", returnDateAsString(fnaContent.CreationDate, wantlocaltime));
            // printf("  ----> FNA File Modified: %s\n", returnDateAsString(fnaContent.LastModifiedDate, wantlocaltime));
            // printf("  ----> FNA MFT Record Modified: %s\n", returnDateAsString(fnaContent.MFTRecordModifiedDate, wantlocaltime));
            // printf("  ----> FNA File Accessed: %s\n", returnDateAsString(fnaContent.LastAccessDate, wantlocaltime));

            model->insertRow(rowcounter++, fieldList);
            for(int i=0; i< model->columnCount(); i++){
                model->setData(model->index(rowcounter-1,i), *bgcolor, Qt::BackgroundRole);
            }
			// delete memory allocation
			free(fnaContent.Filename);
			fnaContent.Filename = NULL;

		}

        if (attrHeader->AttributeType == 0x40 && attrHeader->AttributeLength > 0){
            memcpy(&oiaContent, &recBuffer[mftpos + attrHeader->OffsetToResidentData], sizeof(_OBJECT_IDENTIFIER_ATTRIBUTE_CONTENT));
            QList<QStandardItem *> fieldOIAList;

            QStandardItem *mftOIARecord = new QStandardItem(QString::number(mftHeader->MftRecordNumber) );
            fieldOIAList.append(mftOIARecord);

            QStandardItem *mftOIAPosition = new QStandardItem("MFT offset = " + QString::number(pos) + "+"  + QString::number(mftpos));
            fieldOIAList.append(mftOIAPosition);

            QStandardItem *oiaAttrib = new QStandardItem("Object Index Attribute");
            fieldOIAList.append(oiaAttrib);


            QStandardItem *mftOIAFlags = new QStandardItem(allocationStatus);
            fieldOIAList.append(mftOIAFlags);

            fieldOIAList.append(NULL);

            QByteArray objOIAN((char*)oiaContent.ObjectID, 16);
            QStandardItem *aObjOIAName = new QStandardItem( QString(objOIAN.toHex()));
            fieldOIAList.append(aObjOIAName);

            if(QString::number(mftHeader->MftRecordNumber) != "3"){
                QStandardItem *aOIACreated = new QStandardItem(returnDateAsString(getObjIDDateNumber((char*)oiaContent.ObjectID), wantlocaltime));
                fieldOIAList.append(aOIACreated);
            }else{
                fieldOIAList.append(NULL);
            }

            fieldOIAList.append(NULL);
            fieldOIAList.append(NULL);
            fieldOIAList.append(NULL);


            if(QString::number(mftHeader->MftRecordNumber) != "3"){
                QStandardItem *aOIAMacAddress = new QStandardItem(printObjIDMac((char*)oiaContent.ObjectID));
                fieldOIAList.append(aOIAMacAddress);
            }else{
                fieldOIAList.append(NULL);
            }
            QStandardItem *aOIAOrder = new QStandardItem( QString::number((quint16)(getOrder((char*)oiaContent.ObjectID))));
            fieldOIAList.append(aOIAOrder);


            QStandardItem *aOIASequence = new QStandardItem( QString::number( getObjIDSequence((char*)oiaContent.ObjectID) & 0xffff ) );
            fieldOIAList.append(aOIASequence);

            model->insertRow(rowcounter++, fieldOIAList);
            for(int i=0; i< model->columnCount(); i++){
                model->setData(model->index(rowcounter-1,i), *bgcolor, Qt::BackgroundRole);
            }

        }

		if (attrHeader->AttributeLength == 0 || attrHeader->AttributeLength <0 || attrHeader->AttributeLength> 1024)
		{
			mftpos = 1024; // Means we end this loop
		}
	}

	

	



}

void MainWindow::printMFTHeader(const MFT_RECORD_HEADER * header)
{
    // printf("Checking the MFT Record header:\n");
    // printf(" -> Flags: %d (0 = Deleted file, 1= Allocated file, 2 = Deleted Dir, 3 = Allocated Dir)\n", header->Flags);
    // printf(" -> Sequence count: %d\n", header->SequenceCount);
    // printf(" -> Hard link count: %d (number of FNA entries)\n", header->HardlinkCount);
    // printf(" -> Record number: %d\n", header->MftRecordNumber);
    // printf(" -> Offset to start attribute %d\n", header->OffsetToStartAttribute);

}

void MainWindow::printIndxHeader(const INDX_FILE_HEADER * header)
{
	
    for (qint32 i = 0; i < 4; i++)
	{
        // printf("%x", header->Signature);
	}
    // printf("\n");
    // printf("Offset to Update Sequence Array: %d\n", header->OffsetUpdateSequenceArray);
    // printf("Entries in Update Sequence Array: %d\n", header->EntriesUpdateSequenceArray);
    // printf("Logfile Sequence Number: %llu\n", header->LogfileSequenceNumber);
    // printf("Virtual Cluster Number: %llu\n", header->VCN);
    // printf("Offset to Index Header: %d, add 24 to get the offet in file\n", header->OffsetIndexEntryHeader);
    // printf("Offset to End of Last Index Entry: %d, add 24 to get the offet in file\n", header->OffsetEndLastIndexEntry);
    // printf("Index type flag: %d\n", header->IndexTypeFlag);
    // printf("Update Sequence Number: %04x\n", header->UpdateSequenceArray[0]);
	for (int i = 1; i<header->EntriesUpdateSequenceArray; i++)
	{
        // printf("Sector %d Sequence Array[%d]: %04x\n", i - 1, i, header->UpdateSequenceArray[i]);
	}

}

void MainWindow::printIndxEntry(const INDX_ENTRY * entry, quint64 pos, quint32 page)
{

    if( ui->chkUTC->isChecked() ){
        wantlocaltime = false;
    }else
    {
        wantlocaltime = true;
    }

    QList<QStandardItem *> fieldObjList;

    QString mftRec = QString::number(get48bits(&entry->MFTRecord));

    QStandardItem *mftObjRecord = new QStandardItem(QString::number(get48bits(&entry->MFTRecord)) );
    fieldObjList.append(mftObjRecord);

    QStandardItem *objPosition = new QStandardItem("ObjectID$O offset = " + QString::number(pos + (page * 0x1000) ) + " + 16");
    fieldObjList.append(objPosition);

    QStandardItem *Attrib = new QStandardItem("Object ID");
    fieldObjList.append(Attrib);

    QStandardItem *objFlags = new QStandardItem(getObjFlags(entry->Flags));
    fieldObjList.append(objFlags);


    // Only $Volume should have this
    if(!isNullGUID((char*)entry->ObjectID ) && ( get48bits(&entry->MFTRecord) == 3)){
        QStandardItem *uniqeVolID= new QStandardItem( "GUID for volume" );
        fieldObjList.append(uniqeVolID);
    }else{
        fieldObjList.append(NULL);
    }


    QByteArray objN((char*)entry->ObjectID, 16);
    QStandardItem *aObjName = new QStandardItem( QString(objN.toHex()));
    fieldObjList.append(aObjName);

    if(mftRec != "3"){
        QStandardItem *aObjCreated = new QStandardItem(returnDateAsString(getObjIDDateNumber((char*)entry->ObjectID), wantlocaltime));
        fieldObjList.append(aObjCreated);
    }else{
        fieldObjList.append(NULL);
    }

    fieldObjList.append(NULL);
    fieldObjList.append(NULL);
    fieldObjList.append(NULL);


    if(mftRec != "3"){
        QStandardItem *aObjMacAddress = new QStandardItem(printObjIDMac((char*)entry->ObjectID));
        fieldObjList.append(aObjMacAddress);
    }else{
        fieldObjList.append(NULL);
    }
    QStandardItem *aObjOrder = new QStandardItem( QString::number(( quint16)(getOrder((char*)entry->ObjectID)) ) );
    fieldObjList.append(aObjOrder);


    QStandardItem *aObjSequence = new QStandardItem( QString::number( getObjIDSequence((char*)entry->ObjectID) & 0xffff ) );
    fieldObjList.append(aObjSequence);
    model->insertRow(rowcounter++, fieldObjList);
    for(int i=0; i< model->columnCount(); i++){


            if(lastMFTRecord != get48bits(&entry->MFTRecord) ){
                lastMFTRecord = get48bits(&entry->MFTRecord); // Update the last mft record before next iteration
                if(*bgcolor == QBrush(Qt::white)){
                    bgcolor->setColor(Qt::lightGray);
                }else{
                    bgcolor->setColor(Qt::white);
                }

            }
            model->setData(model->index(rowcounter-1,i), *bgcolor, Qt::BackgroundRole);
    }




    QList<QStandardItem *> fieldObjVolList;

    QStandardItem *mftObjVolRecord = new QStandardItem(QString::number(get48bits(&entry->MFTRecord)) );
    fieldObjVolList.append(mftObjVolRecord);

    QStandardItem *objVolPosition = new QStandardItem("ObjectID$O offset = " + QString::number(pos+ (page * 0x1000) ) + " + 40");
    fieldObjVolList.append(objVolPosition);

    QStandardItem *VolAttrib = new QStandardItem("Birth Volume Object ID");
    fieldObjVolList.append(VolAttrib);

    QStandardItem *objVolFlags = new QStandardItem(getObjFlags(entry->Flags));
    fieldObjVolList.append(objVolFlags);



    QByteArray objVolN((char*)entry->BirthVolumeID, 16);
    if(objVolN[0] & (1 << 0) ){
        QStandardItem *aMovedToVol= new QStandardItem( "Moved to volume" );
        fieldObjVolList.append(aMovedToVol);
    }else{
        if( !isEqualGUID( (char*)entry->BirthObjectID, (char*)entry->ObjectID) ){
            if(!isNullGUID((char*)entry->BirthObjectID) || !isNullGUID((char*)entry->ObjectID )){
                if(get48bits(&entry->MFTRecord) != 3){
                    if(!isNullGUID((char*)entry->BirthVolumeID )){
                        QStandardItem *aMovedToVol= new QStandardItem( "Copied to volume" );
                        fieldObjVolList.append(aMovedToVol);
                    }else{ // BirthVolumeID is zero
                        if(isNullGUID((char*)entry->BirthObjectID)){
                            QStandardItem *aMovedToVol= new QStandardItem( "Created before Object ID was assigned to $Volume" );
                            fieldObjVolList.append(aMovedToVol);

                        }else{ // BirthObjectID is not zero
                            QStandardItem *aMovedToVol= new QStandardItem( "Created directly after format (not rebooted while attached), or USB thumb drive!" );
                            fieldObjVolList.append(aMovedToVol);
                        }
                    }

                }else{
                        fieldObjVolList.append(NULL);
                }
            } else{
                    fieldObjVolList.append(NULL);

            }
        }else{
            if(isNullGUID((char*) entry->BirthVolumeID)){
                QStandardItem *aMovedToVol= new QStandardItem( "BirthVolumeID is not set!" );
                fieldObjVolList.append(aMovedToVol);
            }else{
                fieldObjVolList.append(NULL);
            }
        }
    }
    QStandardItem *aObjVolName = new QStandardItem( QString(objVolN.toHex()));
    fieldObjVolList.append(aObjVolName);

    fieldObjVolList.append(NULL); // UUID does not have a timestamp


    fieldObjVolList.append(NULL);

    fieldObjVolList.append(NULL);
    fieldObjVolList.append(NULL);
    fieldObjVolList.append(NULL);

    model->insertRow(rowcounter++, fieldObjVolList);
    for(int i=0; i< model->columnCount(); i++){
            model->setData(model->index(rowcounter-1,i), *bgcolor, Qt::BackgroundRole);
    }




    QList<QStandardItem *> fieldObjBirthList;


    QStandardItem *mftObjBirthRecord = new QStandardItem( mftRec );
    fieldObjBirthList.append(mftObjBirthRecord);

    QStandardItem *objBirthPosition = new QStandardItem("ObjectID$O offset = " + QString::number(pos + (page * 0x1000) )  + " + 56");
    fieldObjBirthList.append(objBirthPosition);

    QStandardItem *BirthAttrib = new QStandardItem("Birth Object ID");
    fieldObjBirthList.append(BirthAttrib);

    QStandardItem *objBirthFlags = new QStandardItem(getObjFlags(entry->Flags));
    fieldObjBirthList.append(objBirthFlags);

    fieldObjBirthList.append(NULL);


    QByteArray objBirthN((char*)entry->BirthObjectID, 16);
    QStandardItem *aObjBirthName = new QStandardItem( QString(objBirthN.toHex()));
    fieldObjBirthList.append(aObjBirthName);

    if(mftRec != "3"){
    QStandardItem *aObjBirthCreated = new QStandardItem(returnDateAsString(getObjIDDateNumber((char*)entry->BirthObjectID), wantlocaltime));
    fieldObjBirthList.append(aObjBirthCreated);
    } else{
        fieldObjBirthList.append(NULL);
    }

    fieldObjBirthList.append(NULL);
    fieldObjBirthList.append(NULL);
    fieldObjBirthList.append(NULL);

    if(mftRec != "3"){
        QStandardItem *aObjBirthMacAddress = new QStandardItem(printObjIDMac((char*)entry->BirthObjectID));
        fieldObjBirthList.append(aObjBirthMacAddress);
    }else{
       fieldObjBirthList.append(NULL);
    }

    QStandardItem *aObjBirthOrder = new QStandardItem( QString::number(( quint16)(getOrder((char*)entry->BirthObjectID)) ) );
    fieldObjBirthList.append(aObjBirthOrder);


    QStandardItem *aObjBirthSequence = new QStandardItem( QString::number( getObjIDSequence((char*)entry->BirthObjectID) & 0xffff ) );
    fieldObjBirthList.append(aObjBirthSequence);

    model->insertRow(rowcounter++, fieldObjBirthList);
    for(int i=0; i< model->columnCount(); i++){
        model->setData(model->index(rowcounter-1,i), *bgcolor, Qt::BackgroundRole);
    }




    QList<QStandardItem *> fieldObjDomainList;


    QStandardItem *mftObjDomainRecord = new QStandardItem( mftRec );
    fieldObjDomainList.append(mftObjDomainRecord);

    QStandardItem *objDomainPosition = new QStandardItem("ObjectID$O offset = " + QString::number(pos + (page * 0x1000) ) + " + 72");
    fieldObjDomainList.append(objDomainPosition);

    QStandardItem *DomainAttrib = new QStandardItem("Domain Object ID");
    fieldObjDomainList.append(DomainAttrib);

    QStandardItem *objDomainFlags = new QStandardItem(getObjFlags(entry->Flags));
    fieldObjDomainList.append(objDomainFlags);

    fieldObjDomainList.append(NULL);


    QByteArray objDomainN((char*)entry->DomainID, 16);

    QStandardItem *aObjDomainName = new QStandardItem( QString(objDomainN.toHex()));
    fieldObjDomainList.append(aObjDomainName);

    fieldObjDomainList.append(NULL); // UUID not a timestamp


    fieldObjDomainList.append(NULL);
    fieldObjDomainList.append(NULL);
    fieldObjDomainList.append(NULL);

    fieldObjDomainList.append(NULL);

    model->insertRow(rowcounter++, fieldObjDomainList);
    for(int i=0; i< model->columnCount(); i++){
        model->setData(model->index(rowcounter-1,i), *bgcolor, Qt::BackgroundRole);
    }




    // printf("\nAt file offset %llu the following Index Entry was found:", (page*0x1000 )+ pos);

    // printf("\nObject ID (Big Endian): ");
    for (qint32 i = 0; i < 16; i++)
	{
        // printf("%02x", entry->ObjectID[i] & 0xff);
	}

	if (wantlocaltime){
        // printf("\nObject ID Date (localtime): %s", returnDateAsString(getObjIDDateNumber((char*)entry->ObjectID), wantlocaltime));
	}
	else{
        // printf("\nObject ID Date (UTC): %s", returnDateAsString(getObjIDDateNumber((char*)entry->ObjectID), wantlocaltime));
	}

	
    // printf("\nObject ID Clock Sequence: %04x", getObjIDSequence((char*)entry->ObjectID) & 0xffff );
    printObjIDMac((char*)entry->ObjectID);
	
    // printf("\nMFT Record: %llu (LE)", get48bits(&entry->MFTRecord));
    // printf("\nMFT Record sequence count: %d (LE)", getMftRecordSequenceNumber(&entry->MFTRecord));
	
	/* Must be implemented later
    // printf("\nMFT Record Sequence : ");
	for (int i = 6; i < 8; i++) // The MFT Record is the first 6 bytes
	{
        // printf("%02x", entry->MFTRecord[i] & 0xff);
	}
	*/

    // printf("\nBirth Volume ID (Big Endian): ");
    for (qint32 i = 0; i < 16; i++)
	{
        // printf("%02x", entry->BirthVolumeID[i] & 0xff);
	}
    // printf("\nBirth Object ID (Big Endian): ");
    for (qint32 i = 0; i < 16; i++)
	{
        // printf("%02x", entry->BirthObjectID[i] & 0xff);
	}
	if (wantlocaltime){
        // printf("\nBirth Object ID Date (local time): %s", returnDateAsString(getObjIDDateNumber((char*)entry->BirthObjectID),wantlocaltime));
	}
	else{
        // printf("\nBirth Object ID Date (UTC): %s", returnDateAsString(getObjIDDateNumber((char*)entry->BirthObjectID), wantlocaltime));
	}

		
    // printf("\nBirth Object ID Clock Sequence: %04x (LE)", getObjIDSequence((char*)entry->BirthObjectID) & 0xffff );
	
    printObjIDMac((char*)entry->BirthObjectID);

    // printf("\nDomain ID (Big Endian): ");
    for (qint32 i = 0; i < 16; i++)
	{
        // printf("%02x", entry->DomainID[i] & 0xff);
	}
    // printf("\n");

}

void MainWindow::fixUpUSN(char *outIndex, const  INDX_FILE_HEADER * header, quint64 startPos, quint32 sectorSize)
{
    quint16 dummy;
    for (qint32 i = 1; i < header->EntriesUpdateSequenceArray; i++)
	{
		// Copy the last two bytes in the sector to dummy
        memcpy(&dummy, &outIndex[ ( (i*sectorSize)-2 ) + startPos], sizeof(quint16));
		//Check if we found the Update Sequence number
		if (dummy == header->UpdateSequenceArray[0])
		{
			// We found the update sequence nuber, now we change it to the original value
			dummy = header->UpdateSequenceArray[i]; // Set dummy equal original value
			// Now overwrite the Update Sequence Number
            memcpy(&outIndex[((i*sectorSize) - 2) + startPos], &dummy, sizeof(quint16));
		} // if
	} // for
}

void MainWindow::fixUpUSN(char *outRecord, const  MFT_RECORD_HEADER * header, quint64 startPos, quint32 sectorSize)
{
    quint16 dummy;
    for (qint32 i = 1; i < header->UpdateSequenceEntries; i++)
	{
		// Copy the last two bytes in the sector to dummy
        memcpy(&dummy, &outRecord[((i*sectorSize) - 2) + startPos], sizeof(quint16));
		//Check if we found the Update Sequence number
		if (dummy == header->UpdateSequenceArray[0])
		{
			// We found the update sequence nuber, now we change it to the original value
			dummy = header->UpdateSequenceArray[i]; // Set dummy equal original value
			// Now overwrite the Update Sequence Number
            memcpy(&outRecord[((i*sectorSize) - 2) + startPos], &dummy, sizeof(quint16));
		} // if
	} // for
}

quint64 MainWindow::get48bits(const quint64 * data)
{

    quint64 dummy = *data;
	for (int i = 48; i < 64; i++)
	{
		// First we left shift the the first bit in 1L to i th posistion, thus only 
		// i th position is set. Then we negate it. Now all except i th position is set
		// Finally we bitwise AND the dummy with the right side. The only bit which is
		// zero is the bith on the i th position, so if dummy at that bit is set, it will
		// be cleared. If it is not set, it will stay cleared.
		dummy &= ~(1LL << i);
	}
	return dummy;
}


qint16 MainWindow::getMftRecordSequenceNumber(const quint64 * data)
{
	char buffer[8] = {0};
	memcpy(buffer, data, 8);
	short sequenceNumber = 0;
    memcpy(&sequenceNumber, &buffer[6], sizeof(qint16));

	return sequenceNumber;
}


quint64 MainWindow::getObjIDDateNumber(const char * buffer)
{
    quint64 wintime;



	// first we just copy the first 8 bytes from the buffer. Object ID buffers are 16 bytes
	memcpy(&wintime, buffer, 8);

	// Then we must clear the first nibble in the last byte of the firstpart variable

	for (int i = 60; i < 64; i++)
	{
		// First we left shift the the first bit in 1L to i th posistion, thus only 
		// i th position is set. Then we negate it. Now all except i th position is set
		// Finally we bitwise AND the dummy with the right side. The only bit which is
		// zero is the bith on the i th position, so if dummy at that bit is set, it will
		// be cleared. If it is not set, it will stay cleared.
        wintime &= ~(1ULL << i);
	}
	// then we subtract the number of nano seconds intervals between the ObjID epoch and the FILETIME

    if(wintime >= 0x146BF33E42C000)
        wintime -= 0x146BF33E42C000;



    return wintime; // Time is in UTC


}

QString MainWindow::returnDateAsString(const quint64 aDate, bool localtime)
{

    QDateTime timestamp;


    if(aDate == 0)
        return "N/A";

    if(!localtime)
        timestamp.setTimeSpec(Qt::UTC);
    timestamp.setSecsSinceEpoch(FileTime2Unixepoch(aDate)); // Will be in local time (the timezone of the system)

    return timestamp.toString("yyyy.MM.dd hh:mm:ss t");

}



quint64 MainWindow::FileTime2Unixepoch(const quint64 ft)

{
    quint64 adjust = 0;
    quint64 result = ft;
	// 100-nanoseconds = milliseconds * 10000
	adjust = 116444736000000000L;

	// removes the diff between 1970 and 1601
	result -= adjust;

	// converts back from 100-nanoseconds to seconds
	return result / 10000000;

}


QString MainWindow::printObjIDMac(const char * buffer)
{
    QString macaddr, tempHex;
	// Buffer is 16 bytes
    // printf("\nMAC address: ");
    if(buffer[10] & (1 << 0)){ // Is bit 0 set in octet 10?
        return "Not valid MAC!";
    }
    for(qint32 i = 10; i < 16; i++)
	{

        tempHex += QString::number( buffer[i] & 0xff, 16);
        if(tempHex.length() < 2){
            tempHex = "0" + tempHex;
        }

        macaddr += tempHex;

        tempHex = ""; // Set it back

        // printf("%02x", buffer[i] & 0xff);
        if (i< 15){
            macaddr += '-';
            // printf("-");
        } // if
	} // for
    return macaddr;
}

quint16 MainWindow::getObjIDSequence(const char * buffer)
{
    quint16 sequence;
    quint16 number = 1;

	// first we just copy byte 8 and 9 from the buffer. Object ID buffers are 16 bytes
    memcpy(&sequence, &buffer[8], sizeof(quint16));

    // Then we must clear the the two most significant bits in the first byte (the Version). However the system will interpret this multibyte
    // as little endian.


    for (qint32 i = 6; i < 8; i++)
	{
		// First we left shift the the first bit in 1L to i th posistion, thus only 
		// i th position is set. Then we negate it. Now all except i th position is set
		// Finally we bitwise AND the dummy with the right side. The only bit which is
		// zero is the bith on the i th position, so if dummy at that bit is set, it will
		// be cleared. If it is not set, it will stay cleared.
		sequence &= ~(number << i);
	}
    // return sequence;
    return((sequence << 8) | (sequence >> 8)); // Swap bytes. Must be Big Endian!

}

qint16 MainWindow::getOrder(const char * buffer)
{
    qint16 order;

    // first we just copy byte 0 and 1 from the buffer. Object ID buffers are 16 bytes
    memcpy(&order, &buffer[0], sizeof(qint16)); // short should always be two bytes...


    return order;

}
