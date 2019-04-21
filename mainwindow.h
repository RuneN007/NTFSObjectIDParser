#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QtEndian>
#include <QBrush>

#include "structures.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    // Declarations of other functions
    qint32 parseObjID(QString ObjIDfileName, QString MFTFileName);
    void printIndxHeader(const INDX_FILE_HEADER * header);
    void fixUpUSN(char *outIndex, const  INDX_FILE_HEADER * header, quint64 startPos, quint32 sectorSize);
    void fixUpUSN(char *outRecord, const  MFT_RECORD_HEADER * header, quint64 startPos, quint32 sectorSize);
    void printIndxEntry(const INDX_ENTRY * entry, quint64 pos, quint32 page);
    quint64 get48bits(const quint64 *data);
    qint16 getMftRecordSequenceNumber(const unsigned long long * data);
    void printMFTHeader(const MFT_RECORD_HEADER * header);
    void printMftRecord(const INDX_ENTRY * objEntry, QByteArray MFTData, MFT_RECORD_HEADER * mftHeader, MFT_ATTRIBUTE_HEADER * attrHeader);
    quint64 FileTime2Unixepoch(const quint64 ft);
    const char * returnDateAsString(const quint64 aDate, bool localtime);
    quint64 getObjIDDateNumber(const char * buffer);
    QString printObjIDMac(const char * buffer);
    quint16 getObjIDSequence(const char * buffer);
    bool is_bit_set(unsigned value, unsigned bitindex);
    QString printFullPath(quint32 MftRecordNumber, QByteArray * MFTData, QString FullPath);
    qint16 getOrder(const char * buffer);
    bool isNullGUID(const char * buffer, quint32 length=16);
    bool isEqualGUID(const char * buffer1, const char * buffer2, quint32 length=16);

private slots:

    void on_btnObjId_clicked();

    void on_btnMFT_clicked();

    void on_btnRun_clicked();

    void on_btnExit_clicked();

    void on_chkUTC_stateChanged(qint64 arg1);

    void on_btnCsv_clicked();

private:
    Ui::MainWindow *ui;
    QStandardItemModel *model;
    qint32 rowcounter;
    quint32 lastMFTRecord;
    QBrush * bgcolor;
    QString lastPath;

    bool wantlocaltime; // Set UTC or localtime
};

#endif // MAINWINDOW_H
