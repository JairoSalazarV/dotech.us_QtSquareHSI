#ifndef FORMMERGEROIANDAPERTURE_H
#define FORMMERGEROIANDAPERTURE_H

#include <__common.h>
#include <QDialog>

namespace Ui {
class formMergeROIAndAperture;
}

class formMergeROIAndAperture : public QDialog
{
    Q_OBJECT

public:
    explicit formMergeROIAndAperture(QWidget *parent = nullptr);
    ~formMergeROIAndAperture();

private slots:
    void on_pbROI_clicked();

    void on_formMergeROIAndAperture_accepted();

    void on_pbSave_clicked();

    void on_pbAperture_clicked();

private:
    Ui::formMergeROIAndAperture *ui;
};

#endif // FORMMERGEROIANDAPERTURE_H
