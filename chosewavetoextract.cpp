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
    options.append(QString::number(tmpWave));
    while( tmpWave < daCalib.maxWavelength )
    {
        tmpWave += daCalib.minSpecRes;
        options.append("," + QString::number(tmpWave));
    }

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
    QString waveSelected;
    id = tableOrig->currentRow();
    waveSelected = tableOrig->item(id,0)->text();
    tableOrig->removeRow(id);
    insertRow(waveSelected,tableDest);
    fromTablesToFiles();
    refreshOptChoi();
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
        addByStep();
    }
}

void choseWaveToExtract::addByStep()
{
    double tmpWave;
    tmpWave = static_cast<double>( daCalib.minWavelength );
    //QString options;
    int row;
    qDebug() << "daCalib.maxWavelength : " << daCalib.maxWavelength ;
    double actualWave;
    while( tmpWave < static_cast<double>( daCalib.maxWavelength ) )
    {
        row = 0;

        //qDebug() << "tmpWave: " << tmpWave;
        ui->tableOptions->selectRow(row);
        //qDebug() << "ui->tableOptions->item(row,0)->text().toDouble(0) 0: " << ui->tableOptions->item(row,0)->text().toDouble(0);
        while( (actualWave = ui->tableOptions->item(row,0)->text().toDouble()) <= tmpWave )
        {
            //qDebug() << "ui->tableOptions->item(row,0)->text().toDouble(0): " << ui->tableOptions->item(row,0)->text().toDouble(0);
            row++;
        }
        row = (row-1>=0)?(row-1):0;
        ui->tableOptions->setCurrentCell(row,0);
        ui->pbAdd->click();
        //tmpWave += (daCalib.minSpecRes * ui->spinBoxStep->value());
        tmpWave = actualWave + (daCalib.minSpecRes * ui->spinBoxStep->value());
        //qDebug() << "tmpWave: " << tmpWave;
    }
    refreshOptChoi();
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
    index.isValid();
    ui->pbAdd->click();    
}

void choseWaveToExtract::on_tableChoises_doubleClicked(const QModelIndex &index)
{
    index.isValid();
    ui->pbRemove->click();
}
