#include "chosewavetoextract.h"
#include "ui_chosewavetoextract.h"


choseWaveToExtract::choseWaveToExtract(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::choseWaveToExtract)
{
    ui->setupUi(this);


    funcGetCalibration(&daCalib);
    fillOptions();

}

choseWaveToExtract::~choseWaveToExtract()
{
    delete ui;
}

void choseWaveToExtract::fillOptions(){
    //Get options and choises
    //..
    if( !QFile::exists(_PATH_WAVE_OPTIONS) )
    {
        iniOptsAndChois(true);
    }
    else
    {
        refreshOptChoi();
    }
}


void choseWaveToExtract::refreshOptChoi()
{
    //----------------------------------------------
    //Clear tables
    //----------------------------------------------
    while(ui->tableOptions->rowCount() > 0)
    {
        ui->tableOptions->removeRow(0);
    }
    while (ui->tableChoises->rowCount() > 0)
    {
        ui->tableChoises->removeRow(0);
    }

    //----------------------------------------------
    //Fill tables
    //----------------------------------------------
    QString options, choises;
    options = readAllFile(_PATH_WAVE_OPTIONS).trimmed();
    choises = readAllFile(_PATH_WAVE_CHOISES).trimmed();

    QList<QString> lstOptions;
    QList<QString> lstChoises;
    if(!options.isEmpty()) lstOptions = options.split(",");
    if(!choises.isEmpty()) lstChoises = choises.split(",");


    QList<float> lstOptNumber;
    QList<float> lstChoisesNumber;
    int i;
    //qDebug() << "lstOptions.at(size-1): " << lstOptions.at(lstOptions.size()-1);
    if(!options.isEmpty())
    {
        for(i=0;i<lstOptions.size();i++)
            lstOptNumber.append(lstOptions.at(i).toFloat());

        std::sort(lstOptNumber.begin(), lstOptNumber.end());

        Q_FOREACH(const float option, lstOptNumber)
        {
            insertRow(QString::number(option),ui->tableOptions);
        }
    }


    if(!choises.isEmpty())
    {
        for(i=0;i<lstChoises.size();i++)
            lstChoisesNumber.append(lstChoises.at(i).toFloat());

        std::sort(lstChoisesNumber.begin(), lstChoisesNumber.end());

        Q_FOREACH(const float choise, lstChoisesNumber)
        {
            //qDebug() << "lstChoises.size(): " << lstChoises.size();
            insertRow(QString::number(choise),ui->tableChoises);
        }
    }

}

void choseWaveToExtract::iniOptsAndChois(bool allOptions)
{
    float tmpWave;
    tmpWave = daCalib.minWavelength;
    QString options;
    options.clear();
    options.append(QString::number(tmpWave));
    while( tmpWave < daCalib.maxWavelength )
    {
        tmpWave += daCalib.minSpecRes;
        options.append("," + QString::number(tmpWave));
    }
    //qDebug() << "lst-> tmpWave: " << tmpWave << " options: " << options;

    if(allOptions)
    {
        saveFile(_PATH_WAVE_OPTIONS,options);
        saveFile(_PATH_WAVE_CHOISES,"");
    }
    else
    {
        saveFile(_PATH_WAVE_CHOISES,options);
        saveFile(_PATH_WAVE_OPTIONS,"");
    }

    refreshOptChoi();
}

void choseWaveToExtract::insertRow(QString wave, QTableWidget *table )
{
    if( !wave.isEmpty() && wave != "\n" )
    {
        int row;
        row = table->rowCount();
        table->insertRow(row);
        table->setItem(row,0,new QTableWidgetItem(wave));
    }
}

void choseWaveToExtract::switchSelected( QTableWidget *tableOrig, QTableWidget *tableDest)
{
    int id;
    id = tableOrig->currentRow();

    if( id >= 0 )
    {
        QString waveSelected;
        waveSelected = tableOrig->item(id,0)->text();
        tableOrig->removeRow(id);
        insertRow(waveSelected,tableDest);
        fromTablesToFiles();
        refreshOptChoi();
        if( tableOrig->rowCount() >= 1 )
        {
            tableOrig->setCurrentCell(0,0);
        }
    }
    else
    {
        qDebug() << "No row selected";
    }
}

void choseWaveToExtract::fromTablesToFiles()
{
    QString options, choises;
    int auxCounter = 0;

    while (ui->tableOptions->rowCount() > 0)
    {
        if(auxCounter==0)auxCounter++;
        else options.append(",");
        options.append(ui->tableOptions->item(0,0)->text());
        ui->tableOptions->removeRow(0);
    }
    auxCounter = 0;
    while (ui->tableChoises->rowCount() > 0)
    {
        if(auxCounter==0)auxCounter++;
        else choises.append(",");
        //qDebug() << "Val2: " << ui->tableChoises->item(0,0)->text();
        choises.append(ui->tableChoises->item(0,0)->text());
        ui->tableChoises->removeRow(0);

    }
    saveFile(_PATH_WAVE_OPTIONS,options);
    saveFile(_PATH_WAVE_CHOISES,choises);
}

void choseWaveToExtract::on_pbRemoveAll_clicked()
{
    iniOptsAndChois(true);
}

void choseWaveToExtract::on_pbAddAll_clicked()
{

    if( ui->spinBoxStep->value() == 1 )
    {
        iniOptsAndChois(false);
    }
    else
    {
        addByStep(ui->spinBoxStep->value());
    }
}

void choseWaveToExtract::addByStep(int stepLength)
{
    int nextRow;


    nextRow     = ui->tableOptions->rowCount() - 1;
    while(nextRow > 0)
    {
        ui->tableOptions->setCurrentCell(nextRow,0);
        ui->pbAdd->click();
        nextRow -= stepLength;
        //qDebug() << "Row0: " << ui->tableChoises->item(0,0)->text();
    }
    // First Row
    ui->tableOptions->setCurrentCell(0,0);
    ui->pbAdd->click();

    fromTablesToFiles();
    refreshOptChoi();

    //qDebug() << "Row0: " << ui->tableChoises->item(0,0)->text();

    //refreshOptChoi();

    /*
    double tmpWave;
    tmpWave = static_cast<double>( daCalib.minWavelength );
    int row;
    double actualWave;
    while( tmpWave < static_cast<double>( daCalib.maxWavelength ) )
    {
        row = 0;
        ui->tableOptions->selectRow(row);
        while( (actualWave = ui->tableOptions->item(row,0)->text().toDouble()) <= tmpWave )
        {
            row++;
        }
        row = (row-1>=0)?(row-1):0;
        ui->tableOptions->setCurrentCell(row,0);
        ui->pbAdd->click();
        tmpWave = actualWave + (daCalib.minSpecRes * ui->spinBoxStep->value());
    }
    refreshOptChoi();
    */
}

void choseWaveToExtract::on_pbAdd_clicked()
{
    switchSelected( ui->tableOptions, ui->tableChoises );
}

void choseWaveToExtract::on_pbRemove_clicked()
{
    switchSelected( ui->tableChoises, ui->tableOptions );
}

void choseWaveToExtract::on_tableOptions_doubleClicked(const QModelIndex &index)
{

    if( index.isValid() )
    {
        ui->pbAdd->click();
    }
}

void choseWaveToExtract::on_tableChoises_doubleClicked(const QModelIndex &index)
{

    if( index.isValid() )
    {
        ui->pbRemove->click();
    }
}

void choseWaveToExtract::on_spinBoxStep_valueChanged(const QString &arg1)
{

}
