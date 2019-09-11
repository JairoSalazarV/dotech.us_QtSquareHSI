#include "formmergeroiandaperture.h"
#include "ui_formmergeroiandaperture.h"
#include <mainwindow.h>

formMergeROIAndAperture::formMergeROIAndAperture(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::formMergeROIAndAperture)
{
    ui->setupUi(this);
}

formMergeROIAndAperture::~formMergeROIAndAperture()
{
    delete ui;
}

void formMergeROIAndAperture::on_pbROI_clicked()
{
    QString tmpPath;
    if( funcLetUserSelectFile( &tmpPath, "Select ROI image", this ) != _OK )
    {
        funcShowMsgERROR_Timeout("Select ROI Source");
        return (void)false;
    }
    ui->txtROI->setText(tmpPath);
}

void formMergeROIAndAperture::on_formMergeROIAndAperture_accepted()
{
    QString tmpPath;
    if( funcLetUserSelectFile( &tmpPath, "Select APERTURE image", this ) != _OK )
    {
        funcShowMsgERROR_Timeout("Select APERTURE Source");
        return (void)false;
    }
    ui->txtAperture->setText(tmpPath);
}

void formMergeROIAndAperture::on_pbSave_clicked()
{
    if( ui->txtROI->text().trimmed().isEmpty() || ui->txtAperture->text().trimmed().isEmpty() )
    {
        funcShowMsgERROR_Timeout("Form incomplete",this);
        return (void)false;
    }


    QImage diffImage(ui->txtROI->text());
    QImage apertureImage(ui->txtAperture->text());

    //
    //Merge Areas
    //
    MainWindow* tmpInternMainW = new MainWindow(this);
    tmpInternMainW->funcMergeSquareAperture(&diffImage,&apertureImage);//Result is saved in diffImage

    //
    //Save cropped image
    //
    tmpInternMainW->updateGlobalImgEdith(&diffImage);
    tmpInternMainW->updateDisplayImage(&diffImage);
}

void formMergeROIAndAperture::on_pbAperture_clicked()
{
    QString tmpPath;
    if( funcLetUserSelectFile( &tmpPath, "Select APERTURE image", this ) != _OK )
    {
        funcShowMsgERROR_Timeout("Select APERTURE Source");
        return (void)false;
    }
    ui->txtAperture->setText(tmpPath);
}
