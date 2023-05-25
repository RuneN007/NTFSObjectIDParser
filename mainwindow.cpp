#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    model = new QStandardItemModel(0,2, this); // 1 row and 1 column
    model->setHorizontalHeaderItem(0, new QStandardItem(QString("MFT Record")) );
    model->setHorizontalHeaderItem(1, new QStandardItem(QString("Byte Offset")) );
    model->setHorizontalHeaderItem(2, new QStandardItem(QString("Attribute or Type")) );
    model->setHorizontalHeaderItem(3, new QStandardItem(QString("Flags")) );
    ui->tableViewResults->setProperty("toolTip", "The Flags are from the MFT header for MFT records, or from the Flags field in the Object ID entries");
    model->setHorizontalHeaderItem(4, new QStandardItem(QString("Volume Action")) );
    model->setHorizontalHeaderItem(5, new QStandardItem(QString("Name")) );
    model->setHorizontalHeaderItem(6, new QStandardItem(QString("Created")) );
    model->setHorizontalHeaderItem(7, new QStandardItem(QString("Modified")) );
    model->setHorizontalHeaderItem(8, new QStandardItem(QString("Record Modified")) );
    model->setHorizontalHeaderItem(9, new QStandardItem(QString("Accessed")) );
    model->setHorizontalHeaderItem(10, new QStandardItem(QString("MAC Address")) );
    model->setHorizontalHeaderItem(11, new QStandardItem(QString("ObjectID Order")) );
    model->setHorizontalHeaderItem(12, new QStandardItem(QString("Clock Sequence")) );
    ui->tableViewResults->setModel(model);
    // ui->tableViewResults->hide();
    rowcounter=0;
    lastMFTRecord=0;
    bgcolor = new QBrush(Qt::white);
    lastPath = QDir::homePath();
    ui->chkUTC->setChecked(true);
    wantlocaltime = false;
    ui->progressBar->hide();
    ui->btnClearAll->setDisabled(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}



void MainWindow::on_btnObjId_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select ObjID $O file"), lastPath, tr("ObjID $O (*.* *)"));
    ui->lblObjID->setText( fileName );
    lastPath = QFileInfo(fileName).path();
    ui->btnRun->setEnabled(true);
}

void MainWindow::on_btnMFT_clicked()
{
     QString fileName = QFileDialog::getOpenFileName(this, tr("Select MFT file"), lastPath, tr("MFT (*.* *)"));
    ui->lblMFT->setText( fileName );
    lastPath = QFileInfo(fileName).path();
    ui->btnRun->setEnabled(true);
}

void MainWindow::on_btnRun_clicked()
{
    if(ui->lblObjID->text() != "" && ui->lblMFT->text() != ""){
        ui->btnCsv->setDisabled(true);
        ui->btnClearAll->setDisabled(true);
        ui->chkUTC->setDisabled(true);
        ui->btnMFT->setDisabled(true);
        ui->btnObjId->setDisabled(true);
        ui->btnRun->setDisabled(true);
        parseObjID(ui->lblObjID->text(), ui->lblMFT->text());
        ui->tableViewResults->show();
        ui->btnCsv->setEnabled(true);
        ui->btnClearAll->setEnabled(true);
    }


}

bool MainWindow::is_bit_set(unsigned value, unsigned bitindex)
{
    return (value & (1 << bitindex)) != 0;
}

void MainWindow::on_btnExit_clicked()
{
    QApplication::quit();
}


bool MainWindow::isNullGUID(const char * buffer, quint32 length)
{

    for(quint32 i=0; i< length; i++){
        if(buffer[i] != 0){
            return false;
        } // if
    } // for
    return true; // All characters was 0
}

bool MainWindow::isEqualGUID(const char * buffer1, const char * buffer2, quint32 length)
{
    for(quint32 i=0; i<length; i++){
        if(buffer1[i] != buffer2[i]){
            return false; // Not equal
        } // if
    } // for
    return true; // Is characters are equal
}

void MainWindow::on_btnCsv_clicked()
{


    QString textData;
    qint32 rows = model->rowCount();
    qint32 columns = model->columnCount();

    // Set headers first
    for(qint32 x= 0; x< columns; x++){
            textData += model->headerData(x,Qt::Horizontal).toString();
            textData += "\t ";      // for .csv file format
    }
    textData += "\n";             // (optional: for new line segmentation)

    for (qint32 i = 0; i < rows; i++) {
        for (qint32 j = 0; j < columns; j++) {


            textData += model->data(model->index(i,j)).toString();
            textData += "\t ";      // for .csv file format
        }
        textData += "\n";             // (optional: for new line segmentation)
    }

    // [Save to file] (header file <QFile> needed)
    // .csv
    QDir theDir(ui->lblObjID->text());
    theDir.cdUp();
    QString csvpath(theDir.absolutePath() );
    csvpath = csvpath + QDir::separator() + "ObjectID-" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss") + ".csv";

    csvpath = QFileDialog::getSaveFileName(this,
        tr("Save CSV file to"), csvpath, tr("Comma Separated file (*.csv)"));

    QFile csvFile(csvpath);
    if(csvFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {

        QTextStream out(&csvFile);
        out << textData;

        csvFile.close();
    }else{
        QMessageBox msg;
        msg.setText("CSV file: " + csvpath + "not saved!");
        msg.exec();
        ui->btnCsv->setDisabled(false);
        return;
    }

    QMessageBox msg;
    msg.setText("CSV file saved to: " + csvpath);
    msg.exec();
    ui->btnCsv->setDisabled(true);

}

void MainWindow::on_chkUTC_stateChanged(int arg1)
{
    wantlocaltime = !arg1; // If the UTC check box is checked. Then Not wantlocaltime
}

void MainWindow::on_btnClearAll_clicked()
{
    model->removeRows(0, model->rowCount() );
    // ui->tableViewResults->hide();
    ui->progressBar->hide();
    ui->chkUTC->setEnabled(true);
    ui->btnMFT->setEnabled(true);
    ui->btnObjId->setEnabled(true);
    ui->btnRun->setDisabled(true);
    ui->lblObjID->setText("");
    ui->lblMFT->setText("");
    ui->btnCsv->setDisabled(true);
    ui->btnClearAll->setDisabled(true);
    // Private members that need to be initialised
    rowcounter=0;
    lastMFTRecord=0;
    bgcolor = new QBrush(Qt::white);
}

QString MainWindow::getObjFlags(const quint16 flag)
{
    switch(flag)
    {
        case 0:
            return "Leaf node";
            break;
        case 1:
            return "Child node exists";
            break;
        case 2:
            return "Last entry in node list";
            break;
        default:
            return "Unknown value";
            break;
    }
}
