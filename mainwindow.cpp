// /home/jairo/Documentos/APPIMAGE/linuxdeployqt-continuous-x86_64.AppImage /home/jairo/Documentos/DESARROLLOS/dotech.us_RELEASES/QtSquareHSI/HypCam -appimage



#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "__common.h"
#include "hypCamAPI.h"

#include <unistd.h>
#include <netdb.h>
#include <QFile>
#include <fstream>
#include <math.h>

#include <QDebug>
#include <QFileInfo>
#include <QFileDialog>
#include <QGraphicsPixmapItem>

#include "graphicsview.h"

#include <QRect>
#include <QRgb>
#include <QProgressBar>

#include <QFormLayout>

#include <selwathtocheck.h>

#include <QImageReader>

#include <formsquareaperturesettings.h>

#include <lstcustoms.h>


#include <hypCamAPI.h>

#include <rasphypcam.h>


//Custom
#include <customline.h>
#include <customrect.h>
#include <selcolor.h>
#include <gencalibxml.h>
#include <rotationfrm.h>
#include <recparamfrm.h>
#include <selwathtocheck.h>

#include <chosewavetoextract.h>
#include <QRgb>

#include <slidehypcam.h>

#include <QtSerialPort/QSerialPort>
#include <slidehypcam.h>

#include <QThread>
#include <arduinomotor.h>

#include <formslidesettings.h>

#include <lstfilenames.h>

#include <QMediaPlayer>
#include <QVideoProbe>
#include <QVideoWidget>
#include <QAbstractVideoSurface>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

#include <formndvisettings.h>

#include <formobtainfolder.h>

#include <formbuildslidehypecubepreview.h>

#include <formtimertxt.h>
#include <QMessageBox>


#include <formgenlinearregression.h>

#include <formslidelinearregression.h>

#include <formmergeslidecalibrations.h>

#include <formmerge3grayintoargb.h>

#include <QTransform>
#include <formhypcubebuildsettings.h>

#include <formhypercubeanalysis.h>

#include <QDesktopServices>

#include <formmergeroiandaperture.h>

structSettings *lstSettings = (structSettings*)malloc(sizeof(structSettings));

structCamSelected *camSelected = (structCamSelected*)malloc(sizeof(structCamSelected));

structRaspcamSettings *raspcamSettings = (structRaspcamSettings*)malloc(sizeof(structRaspcamSettings));

GraphicsView *myCanvas;
GraphicsView *canvasSpec;
GraphicsView *canvasCalib;
GraphicsView *canvasAux;

customLine *globalCanvHLine;
customLine *globalCanvVLine;

QList<QPair<int,int>> *lstBorder;
QList<QPair<int,int>> *lstSelPix;
QList<QPair<int,int>> *lstPixSelAux;

calcAndCropSnap calStruct;
bool globaIsRotated;

qint64 numFrames;

QThread* progBarThread;


int*** tmpHypercube;
static QList<QImage> lstHypercubeImgs;
//int tmpMaxHypcubeVal;
static QList<QFileInfo> lstImages;
GraphicsView* graphViewSmall;
const int frameX    = 40;       //left and right
const int frameY    = 30;       //up and down
const int rangeInit = 300;      //Wavelength(nm) at the origin
const int rangeEnd  = 1100;     //Wavelength(nm) max limit
const int rangeStep = 50;       //Wavelength(nm) between slides

QImage* globalEditImg;
QImage* globalBackEditImg;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    ui->setupUi(this);
    this->setWindowState(Qt::WindowMaximized);


    //=================================================================
    // Before Start, validate minimal successfull status
    //=================================================================
    funcValidateMinimalStatus();
    //=================================================================
    //=================================================================

    funcObtSettings( lstSettings );
    progBarThread = new QThread(this);

    //*****************************************************************
    // Initialize global settings
    //*****************************************************************
    camSelected->isConnected    = false;
    camSelected->On             = false;
    camSelected->stream         = false;

    //Initialize camera parameters
    ui->txtCamParamXMLName->setText("raspcamSettings");
    funcGetRaspParamFromXML( raspcamSettings, _PATH_RASPICAM_SETTINGS );
    funcIniCamParam( raspcamSettings );

    //*****************************************************************
    // Create Graphic View Widget
    //*****************************************************************
    myCanvas = new GraphicsView;
    canvasSpec = new GraphicsView;
    canvasCalib = new GraphicsView;
    canvasAux = new GraphicsView;

    //Initialize points container for free-hand pen tool
    lstBorder = new QList<QPair<int,int>>;
    lstSelPix = new QList<QPair<int,int>>;
    lstPixSelAux = new QList<QPair<int,int>>;

    //*****************************************************************
    // Set layout into spectometer
    //*****************************************************************
    QFormLayout *layout = new QFormLayout;
    //ui->tab_5->setLayout(layout);

    //if(_USE_CAM)ui->sbSpecUsb->setValue(1);
    //else ui->sbSpecUsb->setValue(0);

    //*****************************************************************
    //Enable-Disable buttoms
    //*****************************************************************
    ui->progBar->setValue(0);
    ui->progBar->setVisible(false);

    ui->pbShutdown->setDisabled(true);

    disableAllToolBars();

    //*****************************************************************
    // Try to connect to the last IP
    //*****************************************************************
    QString lastIP = readAllFile( _PATH_LAST_IP );
    lastIP.replace("\n","");
    ui->txtIp->setText(lastIP);

    if( funcShowMsgYesNo("Welcome!","Auto connect?") ){
        ui->pbAddIp->click();
        ui->tableLstCams->selectRow(0);
        ui->pbConnect->click();
    }

    //*****************************************************************
    // Show Last Preview
    //*****************************************************************
    QString lastFileOpen    = readFileParam(_PATH_LAST_USED_IMG_FILENAME);
    globalEditImg           = new QImage(lastFileOpen);
    globalBackEditImg       = new QImage();
    *globalBackEditImg      = *globalEditImg;    

    //*****************************************************************
    // Resize Graphics Tools
    //*****************************************************************
    QResizeEvent* emptyEvent = nullptr;
    this->resizeEvent(emptyEvent);

    //updateDisplayImage(globalBackEditImg);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::disableAllToolBars(){
    //ui->toolBarDraw->setVisible(false);
}

int MainWindow::funcValidateMinimalStatus()
{
    //-------------------------------------------------------
    //FOLDERS
    //-------------------------------------------------------
    QList<QString> lstFolders;
    lstFolders << "./SYNC" << "./tmpImages" << "./tmpImages/frames"
               << "./settings" << "./settings/Calib" << "./settings/Calib/images/" << "./settings/lastPaths/"
               << "./settings/Wavelengths/" << "./settings/Calib/responses"
               << "./XML" << "./XML/camPerfils"
               << "./tmpImages/frames/tmp"
               << _PATH_TMP_HYPCUBES;

    if( func_DirExistOrCreateIt( lstFolders, this ) == _ERROR )
    {
        exit(_ERROR);
    }

    //-------------------------------------------------------
    // Halogen Function
    //-------------------------------------------------------
    if( !fileExists(_PATH_HALOGEN_FUNCTION) )
    {
        //funcShowMsg_Timeout("Alert!","Creating 3800K Halogen Function",this);
        QString halogenFunction = "1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,6,7,7,8,9,10,10,11,12,13,13,14,15,16,17,17,18,19,20,20,21,22,23,23,24,25,26,26,27,28,29,29,30,31,32,32,33,34,35,35,36,37,38,38,39,40,41,41,42,43,44,45,45,46,47,48,48,49,50,51,51,52,53,54,54,55,56,57,57,58,59,60,60,61,62,63,63,64,65,66,66,67,68,69,69,70,71,72,73,73,74,75,76,76,77,78,79,79,80,81,82,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,165,166,167,169,170,172,173,175,176,178,179,181,182,184,185,187,188,190,191,192,194,195,197,198,200,201,203,204,206,207,209,210,212,213,214,216,217,219,220,222,223,225,226,228,229,231,232,234,235,237,238,239,241,242,244,245,247,248,250,251,253,254,256,257,259,261,263,264,266,268,270,272,273,275,277,279,281,282,284,286,288,290,291,293,295,297,299,300,302,304,306,308,309,311,313,315,317,319,320,322,324,326,328,329,331,333,335,337,338,340,342,344,346,347,349,351,353,355,356,358,360,362,364,365,367,369,371,373,374,376,378,380,382,384,385,387,389,391,393,394,396,398,400,402,403,405,407,409,411,412,414,416,418,420,421,423,425,427,429,430,432,434,436,438,440,441,443,445,447,449,451,453,456,458,460,463,465,468,470,472,475,477,479,482,484,487,489,491,494,496,499,501,503,506,508,511,513,515,518,520,523,525,527,530,532,535,537,539,542,544,546,549,551,554,556,558,561,563,566,568,570,573,575,578,580,582,585,587,590,592,594,597,599,601,604,606,609,611,613,616,618,621,623,625,628,630,633,635,637,640,642,645,647,649,652,654,657,659,662,665,667,670,672,675,677,680,683,685,688,690,693,695,698,701,703,706,708,711,713,715,717,719,721,724,726,728,730,732,734,736,738,740,742,744,746,748,750,752,755,757,759,761,763,765,767,769,771,773,775,777,779,781,783,786,788,790,792,794,796,798,800,802,804,807,809,811,813,815,817,819,821,823,825,827,830,832,834,836,838,840,842,843,844,846,847,849,850,851,853,854,855,857,858,859,861,862,863,865,866,868,869,870,872,873,874,876,877,878,880,881,882,884,885,886,888,889,891,892,893,895,896,897,899,900,902,903,904,906,907,908,910,911,912,914,915,917,918,919,921,922,923,925,926,927,928,929,930,931,932,933,934,935,936,937,938,939,940,940,941,942,943,944,945,946,947,948,949,950,951,952,953,953,954,954,955,956,956,957,957,958,958,959,960,960,961,961,962,962,963,964,964,965,965,966,966,967,968,968,969,969,970,970,971,972,972,973,973,974,974,975,976,976,977,977,977,978,978,978,979,979,979,980,980,980,980,981,981,981,982,982,982,982,983,983,983,984,984,984,984,985,985,985,986,986,986,986,987,987,987,988,988,988,989,989,989,989,990,990,990,991,991,991,991,991,992,992,992,992,993,993,993,993,993,994,994,994,994,994,995,995,995,995,995,996,996,996,996,996,997,997,997,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,996,995,995,995,995,994,994,994,994,994,993,993,993,993,993,992,992,992,992,991,991,991,991,990,990,990,990,990,989,989,989,989,989,988,988,988,988,987,987,987,987,986,986,986,986,986,985,985,984,984,984,983,983,982,982,981,981,981,980,980,979,979,978,978,977,977,977,976,976,975,975,974,974,974,973,973,972,972,971,971,971,970,970,969,969,968,968,967,967,967,966,965,965,964,963,962,962,961,960,959,959,958,957,956,956,955,954,953,952,952,951,950,950,949,948,947,946,946,945,944,943,943,942,941,940,940,939,938,937,936,936,935,935,934,933,933,932,931,931,930,929,929,928,927,927,926,925,925,924,923,923,922,921,921,920,919,919,918,917,917,916,915,915,914,913,913,912,911,911,910,909,909,908,907,907,906,905,905,904,903,903,902,901,901,900,899,899,898,897,897,896,895,895,894,893,893,892,891,891,890,889,889,888,887,887,886,885,885,884,884,883,882,882,881,880,880,879,878,878,877,876,876,875,874,874,873,872,872,871,870,870,869,868,868,867,866,866,865,864,864,863,862,862,861,860,860,859,858,858,857,856,856,855,854,854,853,852,852,851,850,850,849,848,848,847,846,846,845,844,844,843,842,842,841,840,840,839,838,838,837,836,836,835,834,834,833,832,832,831,831,830,829,829,828,827,827,826,825,825,824,823,823,822,821,821,820,819,819,818,817,817,816,815,815,814,813,813,812,811,811,810,809,809,808,807,807,806,805,805,804,803,803,802,801,801,800,799,799,798,797,797,796,795,795,794,793,793,792,791,791,790,789,789,788,787,787,786,785,785,784,783,783,782,781,781,780,779,779,778,777,777,776,775,775,774,773,773,772,771,771,770,770,769,768,768,767,766,766,765,764,764,763,762,762,761,760,760,759,758,758,757,756,756,755,754,754,753,752,752,751,750,750,749,748,748,747,746,746,745,744,744,743,742,742,741,740,740,739,738,738,737,736,736,735,734,734,733,732,732,731,730,730,729,728,728,727,726,726,725,724,724,723,722,722,721,720,720,719,718,718,717,716,716,715,714,714,713,712,712,711,711,710,709,709,708,707,707,706,706,705,704,704,703,702,702,701,700,700,699,699,698,697,697,696,695,695,694,693,693,692,692,691,690,690,689,688,688,687,686,686,685,685,684,683,683,682,681,681,680,679,679,678,678,677,676,676,675,674,674,673,672,672,671,671,670,669,669,668,668,667,666,666,665,664,664,663,663,662,661,661,660,660,659,658,658,657,656,656,655,655,654,653,653,652,651,651,650,650,649,648,648,647,647,646,645,645,644,643,643,642,642,641,640,640,639,639,638,637,637,636,635,635,634,634,633,632,632,631,631,630,629,629,628,627,627,626,626,625,624,624,623,622,622,621,621,620,619,619,618,618,617,616,616,615,614,614,613,613,612,611,611,610,610,609,608,608,607,606,606,605,604,604,603,602,602,601,601,600,599,599,598,597,597,596,595,595,594,593,593,592,591,591,590,590,589,588,588,587,586,586,585,584,584,583,582,582,581,581,580,579,579,578,577,577,576,575,575,574,573,573,572,571,571,570,570,569,568,568,567,566,566,565,564,564,563,562,562,561,560,560,559,559,558,557,557,556,555,555,554,553,553,552,551,551,550,550,549,548,548,547,546,546,545,544,544,543,542,542,541,540,540,539,538,538,537,537,536,535,535,534,533,533,532,531,531,530,529,529,528,527,527,526,525,525,524,523,523,522,521,521,520,519,519,518,517,517,516,515,515,514,513,513,512,511,511,510,509,509,508,507,507,506,505,505,504,503,503,502,501,501,500,499,499,498,497,497,496,495,495,494,493,493,492,491,491,490,489,489,488,487,487,486,485,485,484,483,483,482,481,481,480,479,479,478,477,477,476,475,475,474,473,473,472,472,471,470,470,469,468,468,467,466,466,465,464,464,463,462,462,461,460,460,459,458,458,457,456,456,455,455,454,453,453,452,451,451,450,450,449,448,448,447,447,446,445,445,444,443,443,442,442,441,440,440,439,438,438,437,437,436,435,435,434,433,433,432,432,431,430,430,429,428,428,427,427,426,425,425,424,423,423,422,422,421,420,420,419,418,418,417,417,416,415,415,414,413,413,412,412,411,410,410,409,409,408,407,407,406,405,405,404,404,403,402,402,401,400,400,399,399,398,397,397,396,395,395,394,394,393,392,392,391,390,390,389,389,388,387,387,386,385,385,384,384,383,382,382,381,380,380,379,379,378,377,377,376,375,375,374,374,373,372,372,371,371,370,369,369,368,367,367,366,366,365,364,364,363,362,362,361,361,360,359,359,358,357,357,356,356,355,354,354,353,352,352,351,351,350,349,349,348,347,347,346,346,345,344,344,343,342,342,341,341,340,339,339,338,338,337,336,336,335,334,334,333,333,332,331,331,330,329,329,328,328,327,326,326,325,324,324,323,323,322,321,321,320,319,319,318,318,317,316,316,315,314,314,313,313,312,311,311,310,309,309,308,308,307,306,306,305,304,304,303,303,302,301,301,300,299,299,298,297,297,296,296,295,294,294,293,292,292,291,290,290,289,289,288,287,287,286,285,285,284,284,283,282,282,281,280,280,279,278,278,277,277,276,275,275,274,274,273,272,272,271,271,270,270,269,269,268,268,267,266,266,265,265,264,264,263,263,262,262,261,260,260,259,259,258,258,257,257,256,256,255,254,254,253,253,252,252,251,251,250,249,249,248,248,247,247,246,246,245,245,244,243,243,242,242,241,241,240,240,239,239,238,237,237,236,236,235,235,234,234,233,233,232,231,231,230,230,229,229,228,228,227,226,226,225,225,224,224,223,223,222,222,221,220,220,219,219,218,218,217,217,216,216,215,214,214,213,213,212,212,211,211,211,210,210,210,209,209,209,208,208,208,207,207,207,206,206,206,205,205,205,204,204,204,203,203,203,202,202,202,201,201,201,201,200,200,200,199,199,199,198,198,198,197,197,197,196,196,196,195,195,195,194,194,194,193,193,193,192,192,192,191,191,191,190,190,190,189,189,189,188,188,188,187,187,187,187,186,186,186,185,185,185,184,184,184,183,183,183,182,182,182,181,181,181,180,180,180,179,179,179,178,178,178,177,177,177,176,176,176,175,175,175,174,174,174,174,173,173,173,172,172,172,171,171,171,170,170,170,169,169,169,169,168,168,168,167,167,167,167,166,166,166,165,165,165,164,164,164,164,163,163,163,162,162,162,161,161,161,161,160,160,160,159,159,159,159,158,158,158,157,157,157,157,156,156,156,155,155,155,155,154,154,154,153,153,153,152,152,152,152,151,151,151,150,150,150,150,149,149,149,148,148,148,147,147,147,147,146,146,146,145,145,145,145,144,144,144,143,143,143,143,142,142,142,141,141,141,140,140,140,140,139,139,139,138,138,138,138,137,137,137,136,136,136,135,135,135,135,134,134,134,133,133,133,133,132,132,132,131,131,131,130,130,130,130,130,129,129,129,129,129,129,128,128,128,128,128,127,127,127,127,127,126,126,126,126,125,125,125,125,125,124,124,124,124,124,123,123,123,123,123,122,122,122,122,122,121,121,121,121,121,120,120,120,120,120,119,119,119,119,119,118,118,118,118,118,117,117,117,117,117,116,116,116,116,116,115,115,115,115,115,114,114,114,114,114,113,113,113,113,113,112,112,112,112,112,111,111,111,111,111,110,110,110,110,110,109,109,109,109,109,108,108,108,108,108,107,107,107,107,107,106,106,106,106,105,105,105,105,105,104,104,104,104,104,104,104,103,103,103,103,103,103,103,102,102,102,102,102,102,101,101,101,101,101,101,101,100,100,100,100,100,100,99,99,99,99,99,99,98,98,98,98,98,98,98,97,97,97,97,97,97,96,96,96,96,96,96,96,95,95,95,95,95,95,94,94,94,94,94,94,94,93,93,93,93,93,93,92,92,92,92,92,92,91,91,91,91,91,91,91,90,90,90,90,90,90,89,89,89,89,89,89,89,88,88,88,88,88,88,87,87,87,87,87,87,87,86,86,86,86,86,86,85,85,85,85,85,85,84,84,84,84,84,84,83,83,83,83,83,83,83,82,82,82,82,82,82,82,81,81,81,81,81,81,81,80,80,80,80,80,80,80,79,79,79,79,79,79,79,78,78,78,78,78,78,78,77,77,77,77,77,77,77,76,76,76,76,76,76,75,75,75,75,75,75,75,74,74,74,74,74,74,74,73,73,73,73,73,73,73,72,72,72,72,72,72,72,71,71,71,71,71,71,70,70,70,70,70,70,70,69,69,69,69,69,69,69,68,68,68,68,68,68,68,67,67,67,67,67,67,67,66,66,66,66,66,66,66,65,65,65,65,65,65,64,64,64,64,64,64,64,63,63,63,63,63,63,63,62,62,62,62,62,62,62,61,61,61,61,61,61,61,60,60,60,60,60,60,59,59,59,59,59,59,59,58,58,58,58,58,58,58,57,57,57,57,57,57,57,57,56,56,56,56,56,56,56,56,56,55,55,55,55,55,55,55,55,54,54,54,54,54,54,54,54,53,53,53,53,53,53,53,53,52,52,52,52,52,52,52,52,51,51,51,51,51,51,51,51,50,50,50,50,50,50,50,50,49,49,49,49,49,49,49,49,48,48,48,48,48,48,48,48,47,47,47,47,47,47,47,47,46,46,46,46,46,46,46,46,45,45,45,45,45,45,45,45,44,44,44,44,44,44,44,44,43,43,43,43,43,43,43,43,42,42,42,42,42,42,42,42,41,41,41,41,41,41,41,41,41,41,41,41,41,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000";
        saveFile(_PATH_HALOGEN_FUNCTION,halogenFunction);
    }


    //-------------------------------------------------------
    // Raspicam Settings
    //-------------------------------------------------------
    if( !fileExists(_PATH_STARTING_SETTINGS) )
    {
        QString settingsContain;
        settingsContain.append("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
        settingsContain.append("<settings>\n");
        settingsContain.append("\t<version>0.1</version>\n");
        settingsContain.append("\t<tcpPort>51717</tcpPort>\n");
        settingsContain.append("</settings>\n");
        saveFile(_PATH_STARTING_SETTINGS,settingsContain.trimmed());
    }


    //-------------------------------------------------------
    // Last Image Selected
    //-------------------------------------------------------
    if( !fileExists(_PATH_LAST_USED_IMG_FILENAME) )
    {
        QImage newImg(100,100,QImage::Format_ARGB32);
        for(int x=0; x<100; x++)
        {
            for(int y=0; y<100; y++)
            {
                newImg.setPixel(x,y,qRgb(0,0,0));
            }
        }
        newImg.save(_PATH_DISPLAY_IMAGE);
        saveFile(_PATH_LAST_USED_IMG_FILENAME,_PATH_DISPLAY_IMAGE);
    }

    //-------------------------------------------------------
    // Raspicam Settings
    //-------------------------------------------------------
    if( !fileExists(_PATH_RASPICAM_SETTINGS) )
    {
        //funcShowMsg_Timeout("Alert!","Creating Default _PATH_RASPICAM_SETTINGS",this);
        QString fileContain;
        fileContain.append("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
        fileContain.append("<settings>\n");
        fileContain.append("\t<AWB>auto</AWB>\n");
        fileContain.append("\t<Exposure>auto</Exposure>\n");
        fileContain.append("\t<Denoise>1</Denoise>\n");
        fileContain.append("\t<ColorBalance>1</ColorBalance>\n");
        fileContain.append("\t<TriggeringTimeSecs>3</TriggeringTimeSecs>\n");
        fileContain.append("\t<ShutterSpeedMs>0</ShutterSpeedMs>\n");
        fileContain.append("\t<SquareShutterSpeedMs>0</SquareShutterSpeedMs>\n");
        fileContain.append("\t<TimelapseDurationSecs>5</TimelapseDurationSecs>\n");
        fileContain.append("\t<TimelapseInterval_ms>900</TimelapseInterval_ms>\n");
        fileContain.append("\t<VideoDurationSecs>10</VideoDurationSecs>\n");
        fileContain.append("\t<ISO>0</ISO>\n");
        fileContain.append("\t<CameraMp>5</CameraMp>\n");
        fileContain.append("\t<Flipped>1</Flipped>\n");
        fileContain.append("\t<horizontalFlipped>1</horizontalFlipped>\n");
        fileContain.append("</settings>");
        saveFile(_PATH_RASPICAM_SETTINGS,fileContain);
    }

    //-------------------------------------------------------
    // IP
    //-------------------------------------------------------
    if( !fileExists(_PATH_LAST_IP) )
    {
        //funcShowMsg("Alert!","Creating Default _PATH_LAST_IP",this);
        QString lastIPContain("172.24.1.1");
        saveFile(_PATH_LAST_IP,lastIPContain);
    }

    //-------------------------------------------------------
    // PORT
    //-------------------------------------------------------
    if( !fileExists(_PATH_PORT_TO_CONNECT) )
    {
        //funcShowMsg_Timeout("Alert!","Creating Default _PATH_PORT_TO_CONNECT",this);
        saveFile(_PATH_PORT_TO_CONNECT,"51717");
    }

    //-------------------------------------------------------
    // BASIC CALIBRATION WAVELENGTHS
    //-------------------------------------------------------
    if( !fileExists(_PATH_SETTINGS_RED_WAVELEN) )
        saveFile(_PATH_SETTINGS_RED_WAVELEN,"438");
    if( !fileExists(_PATH_SETTINGS_GREEN_WAVELEN) )
        saveFile(_PATH_SETTINGS_GREEN_WAVELEN,"548");
    if( !fileExists(_PATH_SETTINGS_BLUE_WAVELEN) )
        saveFile(_PATH_SETTINGS_BLUE_WAVELEN,"615");
    if( !fileExists(_PATH_SETTINGS_EM_ITERATIONS) )
        saveFile(_PATH_SETTINGS_EM_ITERATIONS,"20");
    if( !fileExists(_PATH_SETTINGS_LOWER_LIMIT) )
        saveFile(_PATH_SETTINGS_LOWER_LIMIT,"1");
    if( !fileExists(_PATH_SETTINGS_UPPER_LIMIT) )
        saveFile(_PATH_SETTINGS_UPPER_LIMIT,"1");


    return _OK;
}

void MainWindow::funcSpectMouseRelease( QMouseEvent *e){
    funcEndRect(e, canvasSpec );
    funcDrawLines(0,0,0,0);
}

void MainWindow::funcCalibMouseRelease( QMouseEvent *e){
    funcEndRect( e, canvasCalib );
    //ui->pbClearCalScene->setText("Clear line");
}

void MainWindow::funcAddPoint( QMouseEvent *e ){

    //Extract pixel's coordinates
    QPair<int,int> tmpPixSel;
    tmpPixSel.first = e->x();
    tmpPixSel.second = e->y();
    lstPixSelAux->append(tmpPixSel);

    //Update politope
    funcUpdatePolitope();

}

bool MainWindow::funcUpdatePolitope(){

    int i, x1, y1, x2, y2, x0, y0;

    //If is the first point
    if( lstPixSelAux->count() < 2 ){
        x1  = lstPixSelAux->at(0).first - 3;//Point +/- error
        y1  = lstPixSelAux->at(0).second - 4;//Point +/- error
        QGraphicsEllipseItem* item = new QGraphicsEllipseItem(x1, y1, 5, 5 );
        item->setPen( QPen(Qt::white) );
        item->setBrush( QBrush(Qt::red) );
        canvasAux->scene()->addItem(item);
        return true;
    }

    //--------------------------------------------
    // Fill the border points
    //--------------------------------------------

    //Last two points
    i   = lstPixSelAux->count()-1;
    x0  = lstPixSelAux->at(0).first;
    y0  = lstPixSelAux->at(0).second;
    x1  = lstPixSelAux->at(i-1).first;
    y1  = lstPixSelAux->at(i-1).second;
    x2  = lstPixSelAux->at(i).first;
    y2  = lstPixSelAux->at(i).second;

    //Draw the line
    if( abs(x0-x2)<=6 && abs(y0-y2)<=6 ){//Politope closed
        funcCreateLine(false,x1,y1,x0,y0);
    }else{//Add line
        funcCreateLine(true,x1,y1,x2,y2);
    }

    //--------------------------------------------
    // Fill pixels inside politope
    //--------------------------------------------
    if( abs(x0-x2)<=6 && abs(y0-y2)<=6 ){
        funcFillFigure();
    }










    return true;
}

void MainWindow::funcFillFigure(){
    // Clear points
    //...
    //qDebug() << "Polytope closed: " << QString::number(lstBorder->count()) ;
    //while(ui->tableLstPoints->rowCount() > 0){
    //    ui->tableLstPoints->removeRow(0);
    //}
    //funcPutImageIntoGV( canvasAux, imgPath );
    canvasAux->scene()->clear();

    // Get max and min Y
    //...
    int i, minX, maxX, minY, maxY;
    minX = minY = INT_MAX;
    maxX = maxY = -1;
    for(i=0;i<lstBorder->count();i++){
        minY = ( lstBorder->at(i).second < minY )?lstBorder->at(i).second:minY;
        maxY = ( lstBorder->at(i).second > maxY )?lstBorder->at(i).second:maxY;
    }

    //Run range in Y
    int tmpY;
    //bool entro;
    QPair<int,int> tmpPair;
    for(tmpY=minY; tmpY<=maxY; tmpY++){
        //qDebug() << "tmpY: "<< QString::number(tmpY);
        //Get range in X
        //entro = false;
        minX = INT_MAX;
        maxX = -1;
        for(i=0;i<lstBorder->count();i++){
            if( lstBorder->at(i).second == tmpY ){
                //entro = true;
                minX = ( lstBorder->at(i).first < minX )?lstBorder->at(i).first:minX;
                maxX = ( lstBorder->at(i).first > maxX )?lstBorder->at(i).first:maxX;
                lstBorder->removeAt(i);
                //qDebug() << "minX: "<< QString::number(minX)<< "maxX: "<< QString::number(maxX);
            }
        }
        //qDebug() << "Linw: minX= "<< QString::number(minX)<< "maxX= "<< QString::number(maxX);

        //Draw line in X
        for(i=minX;i<=maxX;i++){
            tmpPair.first  = i;
            tmpPair.second = tmpY;
            lstSelPix->append( tmpPair );
            funcDrawPointIntoCanvas( tmpPair.first, tmpPair.second, 1, 1, "#FF0000", "#FFFFFF" );
        }
        //funcShowMsg("","jeha");
    }

    //Clear border
    lstBorder->clear();

    //Redrawn pixels
    for(i=0;i<lstSelPix->count();i++){
        funcDrawPointIntoCanvas(
                                    lstSelPix->at(i).first,
                                    lstSelPix->at(i).second,
                                    1,
                                    1,
                                    "#FF0000",
                                    "#FFFFFF"
                                );
    }

    lstPixSelAux->clear();

     disconnect(
                canvasCalib,
                SIGNAL( signalMousePressed(QMouseEvent*) ),
                this,
                SLOT( funcAddPoint(QMouseEvent*) )
           );

     mouseCursorReset();


}





void MainWindow::funcCreateLine(bool drawVertex,
    int x1,
    int y1,
    int x2,
    int y2
){

    QPair<int,int> tmpPair;

    //Drawing the vertex
    if( drawVertex ){
        tmpPair.first = x2-3;
        tmpPair.second = y2-4;
        lstBorder->append(tmpPair);
        funcDrawPointIntoCanvas(tmpPair.first, tmpPair.second, 5, 5, "#FF0000", "#FFFFFF");
        //QGraphicsEllipseItem* item = new QGraphicsEllipseItem(tmpPair.first, tmpPair.second, 5, 5 );
        //item->setPen( QPen(QColor("#FFFFFF")) );
        //item->setBrush( QBrush(QColor("#FF0000")) );
        //myCanvas->scene()->addItem(item);
    }

    //Variables
    int i, yIni, yEnd;
    float m,b;

    //Compute slope
    m = (float)(y2-y1) / (float)(x2-x1);
    b = (float)y1-(m*(float)x1);

    //Obtain points in the border
    int xIni = (x1 <= x2)?x1:x2;
    int xEnd = (xIni == x1)?x2:x1;
    int lastY = -1, j = -1;
    if(xIni==xEnd){
        yIni = (y1<=y2)?y1:y2;
        yEnd = (y1==yIni)?y2:y1;
        for( j=yIni; j<=yEnd; j++ ){
            funcDrawPointIntoCanvas(xIni, j, 1, 1, "#FF0000", "#FFFFFF");
            //QGraphicsEllipseItem* item = new QGraphicsEllipseItem( xIni, j, 1, 1 );
            //item->setPen( QPen(QColor("#FF0000")) );
            //myCanvas->scene()->addItem(item);

            tmpPair.first = xIni;
            tmpPair.second = j;
            lstBorder->append(tmpPair);
        }
    }else{
        for( i=xIni; i<=xEnd; i++ ){
            //Add discrete value
            tmpPair.first = i;
            tmpPair.second = floor( (m*i) + b );
            lstBorder->append(tmpPair);

            funcDrawPointIntoCanvas(tmpPair.first, tmpPair.second, 1, 1, "#FF0000", "#FFFFFF");
            //QGraphicsEllipseItem* item = new QGraphicsEllipseItem( tmpPair.first, tmpPair.second, 1, 1 );
            //item->setPen( QPen(QColor("#FF0000")) );
            //myCanvas->scene()->addItem(item);

            //Complete the line
            if( (i == xIni) ){
                lastY = tmpPair.second;
            }else{
                yIni = (tmpPair.second <= lastY)?tmpPair.second:lastY;
                yEnd = (yIni == tmpPair.second)?lastY:tmpPair.second;
                if( abs(yIni-yEnd) > 1 ){
                    lastY = tmpPair.second;
                    for( j=yIni; j<=yEnd; j++ ){
                        tmpPair.first = i;
                        tmpPair.second = j;
                        lstBorder->append(tmpPair);

                        //QGraphicsEllipseItem* item = new QGraphicsEllipseItem( i, j, 1, 1 );
                        //item->setPen( QPen(QColor("#FF0000")) );
                        //myCanvas->scene()->addItem(item);
                        funcDrawPointIntoCanvas(i,j,1,1, "#FF0000", "#FFFFFF");

                        //funcShowMsg("","Ja");
                    }
                }
            }
        }
    }


}

void MainWindow::funcDrawPointIntoCanvas(
                                            int x,
                                            int y,
                                            int w,
                                            int h,
                                            QString color = "#FF0000",
                                            QString lineColor = "#FFFFFF"
){
    QGraphicsEllipseItem* item = new QGraphicsEllipseItem( x, y, w, h );
    item->setPen( QPen(QColor(color)) );
    canvasAux->scene()->addItem(item);
    item->setBrush( QBrush(QColor(lineColor)) );
}

void MainWindow::funcAddPoit2Graph(
                                        GraphicsView *tmpCanvas,
                                        int x,
                                        int y,
                                        int w,
                                        int h,
                                        QColor color,
                                        QColor lineColor
){
    QGraphicsEllipseItem* item = new QGraphicsEllipseItem( x, y, w, h );
    item->setPen( QPen(color) );
    tmpCanvas->scene()->addItem(item);
    item->setBrush( QBrush(lineColor) );
}


void MainWindow::funcIniCamParam( structRaspcamSettings *raspcamSettings )
{    
    QList<QString> tmpList;

    //Set AWB: off,auto,sun,cloud,shade,tungsten,fluorescent,incandescent,flash,horizon
    tmpList<<"none"<<"off"<<"auto"<<"sun"<<"cloud"<<"shade"<<"tungsten"<<"fluorescent"<<"incandescent"<<"flash"<<"horizon";
    ui->cbAWB->clear();
    ui->cbAWB->addItems( tmpList );
    ui->cbAWB->setCurrentText((char*)raspcamSettings->AWB);
    tmpList.clear();

    //Set Exposure: off,auto,night,nightpreview,backlight,spotlight,sports,snow,beach,verylong,fixedfps,antishake,fireworks
    tmpList<<"none"<<"off"<<"auto"<<"night"<<"nightpreview"<<"backlight"<<"spotlight"<<"sports"<<"snow"<<"beach"<<"verylong"<<"fixedfps"<<"antishake"<<"fireworks";
    ui->cbExposure->clear();
    ui->cbExposure->addItems( tmpList );
    ui->cbExposure->setCurrentText((char*)raspcamSettings->Exposure);
    tmpList.clear();

    //Gray YUV420
    //if( raspcamSettings->Format == 1 )ui->rbFormat1->setChecked(true);
    //if( raspcamSettings->Format == 2 )ui->rbFormat2->setChecked(true);

    //Brightness
    //ui->slideBrightness->setValue( raspcamSettings->Brightness );
    //ui->labelBrightness->setText( "Brightness: " + QString::number(raspcamSettings->Brightness) );

    //Sharpness
    //ui->slideSharpness->setValue( raspcamSettings->Sharpness );
    //ui->labelSharpness->setText( "Sharpness: " + QString::number(raspcamSettings->Sharpness) );

    //Contrast
    //ui->slideContrast->setValue( raspcamSettings->Contrast );
    //ui->labelContrast->setText( "Contrast: " + QString::number(raspcamSettings->Contrast) );

    //Saturation
    //ui->slideSaturation->setValue( raspcamSettings->Saturation );
    //ui->labelSaturation->setText( "Saturation: " + QString::number(raspcamSettings->Saturation) );

    //DiffractionShuterSpeed
    ui->spinBoxShuterSpeed->setValue( raspcamSettings->ShutterSpeedMs );

    //SquareShuterSpeed
    ui->spinBoxSquareShuterSpeed->setValue(raspcamSettings->SquareShutterSpeedMs);

    //Timelapse Duration
    ui->spinBoxTimelapseDuration->setValue( raspcamSettings->TimelapseDurationSecs );

    //Timelapse Interval
    ui->spinBoxTimelapse->setValue( raspcamSettings->TimelapseInterval_ms );

    //Delay Time
    ui->spinBoxDelayTime->setValue( raspcamSettings->DelayTime );

    //Video Duration
    //ui->spinBoxVideoDuration->setValue( raspcamSettings->VideoDurationSecs );

    //ISO
    ui->spinBoxISO->setValue( raspcamSettings->ISO );

    //ExposureCompensation
    //ui->slideExpComp->setValue( raspcamSettings->ExposureCompensation );
    //ui->labelExposureComp->setText( "Exp. Comp.: " + QString::number(raspcamSettings->ExposureCompensation) );

    //RED
    //qDebug() << "Red: " << raspcamSettings->Red;
    //ui->slideRed->setValue( raspcamSettings->Red );
    //ui->labelRed->setText( "Red: " + QString::number(raspcamSettings->Red) );

    //GREEN
    //ui->slideGreen->setValue( raspcamSettings->Green );
    //ui->labelGreen->setText( "Green: " + QString::number(raspcamSettings->Green) );

    //PREVIEW
    //if( raspcamSettings->Preview )ui->cbPreview->setChecked(true);
    //else ui->cbPreview->setChecked(false);

    //TRIGGER TIME
    ui->spinBoxTrigeringTime->setValue(raspcamSettings->TriggeringTimeSecs);
    //ui->labelTriggerTime->setText("Trigger time: " + QString::number(raspcamSettings->TriggeringTimeSecs)+"secs");

    //DENOISE EFX
    if( raspcamSettings->Denoise )ui->cbDenoise->setChecked(true);
    else ui->cbDenoise->setChecked(false);

    //FULL PHOTO
    //if( raspcamSettings->FullPhoto )ui->cbFullPhoto->setChecked(true);
    //else ui->cbFullPhoto->setChecked(false);

    //COLORBALANCE EFX
    if( raspcamSettings->ColorBalance )ui->cbColorBalance->setChecked(true);
    else ui->cbColorBalance->setChecked(false);

    //CAMERA RESOLUTION
    if( raspcamSettings->CameraMp == 5 )ui->radioRes5Mp->setChecked(true);
    else ui->radioRes8Mp->setChecked(true);

    //FLIPPED
    if( raspcamSettings->Flipped )ui->cbFlipped->setChecked(true);
    else ui->cbFlipped->setChecked(false);

    //HORIZONTAL FLIPPED
    if( raspcamSettings->horizontalFlipped )ui->cbFlippedHorizontal->setChecked(true);
    else ui->cbFlippedHorizontal->setChecked(false);


}

/*
void MainWindow::on_pbGetVideo_clicked()
{

    //Prepare socket variables
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    frameStruct *frame2Send     = (frameStruct*)malloc(sizeof(frameStruct));
    portno = lstSettings->tcpPort;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        //error("ERROR opening socket");
    }
    server = gethostbyname( "192.168.1.69" );
    //server = gethostbyname( "10.0.5.126" );
    if (server == NULL) {
        //fprintf(stderr,"ERROR, not such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
    serv_addr.sin_port = htons(portno);
    if (::connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
        //error("ERROR connecting");
    }

    //Request file
    frame2Send->header.idMsg          = (char)4;  // Header: Id instruction
    frame2Send->header.consecutive    = 1;        // Header: Consecutive
    frame2Send->header.numTotMsg      = 1;        // Header: Total number of message to send
    frame2Send->header.bodyLen        = 0;        // Header: Usable message lenght
    int tmpFrameLen = sizeof(frameHeader)+frame2Send->header.bodyLen;
    n = ::write(sockfd,frame2Send,tmpFrameLen);
    if (n < 0){
        qDebug() << "writing to socket";
    }

    //Receibing ack with file len
    unsigned int fileLen;
    unsigned char bufferRead[frameBodyLen];
    n = read(sockfd,bufferRead,frameBodyLen);
    memcpy(&fileLen,&bufferRead,sizeof(unsigned int));
    //funcShowMsg("FileLen n("+QString::number(n)+")",QString::number(fileLen));

    if(fileLen>0){
        //Receive File
        unsigned char tmpFile[fileLen];
        if( funcReceiveFile( sockfd, fileLen, bufferRead, tmpFile ) ){

            //Save file
            std::ofstream myFile ("yoRec2.jpg", std::ios::out | std::ios::binary);
            myFile.write((char*)&tmpFile, fileLen);

            //It finishes succesfully
            funcShowMsg("OK","Successfull reception");

        }else{
            funcShowMsg("ERROR","File does not received");
        }

        ::close(sockfd);

    }else{
        funcShowMsg("Alert","File is empty");
    }


}
*/


bool MainWindow::funcReceiveFile(
                                    int sockfd,
                                    unsigned int fileLen,
                                    unsigned char *bufferRead,
                                    unsigned char *tmpFile
){

    qDebug() << "Inside funcReceiveFile sockfd: " << sockfd;



    //Requesting file
    int i, n;

    n = ::write(sockfd,"sendfile",8);
    if (n < 0){
        qDebug() << "ERROR: writing to socket";
        return false;
    }



    //Receive file parts
    unsigned int numMsgs = (fileLen>0)?floor( (float)fileLen / (float)frameBodyLen ):0;
    numMsgs = ((numMsgs*frameBodyLen)<fileLen)?numMsgs+1:numMsgs;
    unsigned int tmpPos = 0;
    memset(tmpFile,'\0',fileLen);
    qDebug() << "Receibing... " <<  QString::number(numMsgs) << " messages";
    qDebug() << "fileLen: " << fileLen;

    //Receive the last
    if(numMsgs==0){
        //Receives the unik message
        bzero(bufferRead,frameBodyLen);
        //qDebug() << "R1";
        //n = read(sockfd,tmpFile,fileLen);
    }else{

        //ui->progBar->setVisible(true);
        ui->progBar->setRange(0,numMsgs);
        ui->progBar->setValue(0);

        funcActivateProgBar();

        for(i=1;i<=(int)numMsgs;i++){
            //printf("Msg: %d\n",i);

            ui->progBar->setValue(i);
            bzero(bufferRead,frameBodyLen);
            n = read(sockfd,bufferRead,frameBodyLen);
            //qDebug() << "n: " << n;
            if(n!=(int)frameBodyLen&&i<(int)numMsgs){
                qDebug() << "ERROR, message " << i << "WRONG";
                return false;
            }
            //Append message to file
            memcpy( &tmpFile[tmpPos], bufferRead, frameBodyLen );
            tmpPos += n;
            //Request other part
            if( i<(int)numMsgs ){
                //qDebug() << "W2";
                QtDelay(2);
                n = ::write(sockfd,"sendpart",8);                
                if(n<0){
                    qDebug() << "ERROR: Sending part-file request";
                    return false;
                }
            }
        }
        ui->progBar->setValue(0);
        ui->progBar->setVisible(false);
        ui->progBar->update();
        QtDelay(30);

    }

    //qDebug() << "tmpPos: " << tmpPos;


    return true;
}

void MainWindow::funcActivateProgBar(){
    //mouseCursorReset();
    ui->progBar->setVisible(true);
    ui->progBar->setValue(0);
    ui->progBar->update();
    QtDelay(50);
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_pbAddIp_clicked()
{
    //Validating IP
    if( !funcIsIP( ui->txtIp->text().toStdString() ) ){
        funcShowMsg("ERROR","Invalid IP address");
        ui->txtIp->setFocus();
    }else{
        //Checks if IP is in the list and remove it if exists
        int i;
        for(i=0;i<ui->tableLstCams->rowCount();i++){
            if( ui->tableLstCams->item(i,1)->text().trimmed() == ui->txtIp->text().trimmed() ){
                ui->tableLstCams->removeRow(i);
            }
        }

        //Add IP
        camSettings * tmpCamSett = (camSettings*)malloc(sizeof(camSettings));
        tmpCamSett->idMsg = (char)0;
        funcValCam(
                        ui->txtIp->text().trimmed().toStdString(),
                        lstSettings->tcpPort,
                        tmpCamSett
                  );
        if( tmpCamSett->idMsg < 1 ){
            funcShowMsg("ERROR","Camera does not respond at "+ui->txtIp->text());
            ui->txtIp->setFocus();
        }else{
            ui->tableLstCams->insertRow( ui->tableLstCams->rowCount() );
            ui->tableLstCams->setItem(
                                            ui->tableLstCams->rowCount()-1,
                                            0,
                                            new QTableWidgetItem(QString(tmpCamSett->Alias))
                                      );
            ui->tableLstCams->setItem(
                                            ui->tableLstCams->rowCount()-1,
                                            1,
                                            new QTableWidgetItem(ui->txtIp->text().trimmed())
                                      );
            ui->tableLstCams->setColumnWidth(0,150);
            ui->tableLstCams->setColumnWidth(1,150);
            //funcShowMsg("Success","Camera detected at "+ui->txtIp->text().trimmed());
        }
    }
}

void MainWindow::on_pbSearchAll_clicked()
{
    //Clear table
    while( ui->tableLstCams->rowCount() > 0 ){
        ui->tableLstCams->removeRow(0);
    }

    //Obtain IP list
    QString result = "";//idMsg to send
    FILE* pipe = popen("arp", "r");
    char bufferComm[frameBodyLen];
    try {
      while (!feof(pipe)) {
        if (fgets(bufferComm, frameBodyLen, pipe) != NULL){
          result.append( bufferComm );
        }
      }
    } catch (...) {
      pclose(pipe);
      throw;
    }
    pclose(pipe);

    //Check IPs candidates
    camSettings *tmpCamSett = (camSettings*)malloc(sizeof(camSettings));
    QStringList ipsCandidates = result.split("\n");
    QStringList candIP;
    int i;


    int sockfd, n, tmpFrameLen;
    tmpFrameLen = sizeof(camSettings);
    struct sockaddr_in serv_addr;
    struct hostent *server;
    unsigned char bufferRead[tmpFrameLen];

    for( i=0;i<ipsCandidates.count();i++ ){
        if( !ipsCandidates.at(i).contains("unreachable") &&
            !ipsCandidates.at(i).contains("incomplete") ){
            candIP = ipsCandidates.at(i).split("ether");

            if( funcIsIP( candIP.at(0).toStdString() ) &&
                candIP.at(0) != ui->txtIp->text().trimmed() + "254"
            ){

                qDebug() << "IP: " << candIP.at(0).trimmed();

                tmpCamSett->idMsg = (char)0;
                memset(tmpCamSett,'\0',sizeof(camSettings));

                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfd < 0){
                    qDebug() << "opening socket";
                }else{
                    server = gethostbyname( candIP.at(0).trimmed().toStdString().c_str() );
                    if (server == NULL) {
                        qDebug() << "Not such host";
                    }else{


                        bzero((char *) &serv_addr, sizeof(serv_addr));
                        serv_addr.sin_family = AF_INET;
                        bcopy((char *)server->h_addr,
                            (char *)&serv_addr.sin_addr.s_addr,
                            server->h_length);
                        serv_addr.sin_port = htons(lstSettings->tcpPort);




                        if (::connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
                            qDebug() << "connecting";
                        }else{
                            //Request camera settings
                            char tmpIdMsg = (char)1;


                            n = ::write(sockfd,&tmpIdMsg,1);
                            if (n < 0){
                                qDebug() << "writing to socket";
                            }else{
                                //Receibing ack with file len
                                n = read(sockfd,bufferRead,tmpFrameLen);
                                if (n < 0){
                                    qDebug() << "reading socket";
                                }else{
                                    memcpy(tmpCamSett,&bufferRead,tmpFrameLen);

                                    if( tmpCamSett->idMsg > 0 ){
                                        ui->tableLstCams->insertRow( ui->tableLstCams->rowCount() );
                                        ui->tableLstCams->setItem(
                                                                        ui->tableLstCams->rowCount()-1,
                                                                        0,
                                                                        new QTableWidgetItem(QString(tmpCamSett->Alias))
                                                                  );
                                        ui->tableLstCams->setItem(
                                                                        ui->tableLstCams->rowCount()-1,
                                                                        1,
                                                                        new QTableWidgetItem(candIP.at(0).trimmed())
                                                                  );
                                        ui->tableLstCams->setColumnWidth(0,150);
                                        ui->tableLstCams->setColumnWidth(1,150);
                                    }







                                }
                            }
                        }
                    }

                }



                ::close(sockfd);





















            }




        }
    }
}

void MainWindow::on_pbSendComm_clicked()
{
    if( !camSelected->isConnected){
        funcShowMsg("Alert","Camera not connected");
        return (void)NULL;
    }
    if( ui->txtCommand->text().isEmpty() ){
        funcShowMsg("Alert","Empty command");
        return (void)NULL;
    }
    if( ui->tableLstCams->rowCount() == 0 ){
        funcShowMsg("Alert","Not camera detected");
        return (void)NULL;
    }
    ui->tableLstCams->setFocus();

    //Prepare message to send
    frameStruct *frame2send = (frameStruct*)malloc(sizeof(frameStruct));
    memset(frame2send,'\0',sizeof(frameStruct));
    if( !ui->checkBlind->isChecked() ){
        frame2send->header.idMsg = (unsigned char)2;
    }else{
        frame2send->header.idMsg = (unsigned char)3;
    }
    frame2send->header.numTotMsg = 1;
    frame2send->header.consecutive = 1;
    frame2send->header.trigeredTime = 0;
    frame2send->header.bodyLen   = ui->txtCommand->text().length();
    bzero(frame2send->msg,ui->txtCommand->text().length()+1);
    memcpy(
                frame2send->msg,
                ui->txtCommand->text().toStdString().c_str(),
                ui->txtCommand->text().length()
          );

    //Prepare command message
    int sockfd, n, tmpFrameLen;
    //unsigned char bufferRead[sizeof(frameStruct)];
    struct sockaddr_in serv_addr;
    struct hostent *server;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    qDebug() << "Comm IP: " << QString((char*)camSelected->IP);
    if (sockfd < 0){
        qDebug() << "opening socket";
        return (void)NULL;
    }
    //server = gethostbyname( ui->tableLstCams->item(tmpRow,1)->text().toStdString().c_str() );
    server = gethostbyname( (char*)camSelected->IP );
    if (server == NULL) {
        qDebug() << "Not such host";
        return (void)NULL;
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
    serv_addr.sin_port = htons(camSelected->tcpPort);
    if (::connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
        qDebug() << "ERROR: connecting to socket";
        return (void)NULL;
    }


    //Request command result
    qDebug() << "idMsg: " << (int)frame2send->header.idMsg;
    qDebug() << "command: " << frame2send->msg;
    qDebug() << "tmpFrameLen: " << sizeof(frameHeader) + ui->txtCommand->text().length();
    tmpFrameLen = sizeof(frameHeader) + ui->txtCommand->text().length();
    n = ::write(sockfd,frame2send,tmpFrameLen+1);
    if(n<0){
        qDebug() << "ERROR: Sending command";
        return (void)NULL;
    }

    //Receibing ack with file len
    unsigned int fileLen;
    unsigned char bufferRead[frameBodyLen];
    n = read(sockfd,bufferRead,frameBodyLen);
    memcpy(&fileLen,&bufferRead,sizeof(unsigned int));
    fileLen = (fileLen<frameBodyLen)?frameBodyLen:fileLen;
    qDebug() << "fileLen: " << fileLen;
    //funcShowMsg("FileLen n("+QString::number(n)+")",QString::number(fileLen));

    //Receive File
    unsigned char tmpFile[fileLen];
    funcReceiveFile( sockfd, fileLen, bufferRead, tmpFile );
    qDebug() <<tmpFile;
    ::close(sockfd);

    //Show command result
    std::string tmpTxt((char*)tmpFile);
    qDebug() << "Get: " << (char*)tmpFile;
    ui->txtCommRes->setText( QString(tmpTxt.c_str()) ) ;

}

void MainWindow::on_pbConnect_clicked()
{
    ui->tableLstCams->setFocus();
    int numRow = ui->tableLstCams->rowCount();    
    if( numRow > 0 ){
        int rowSelected = ui->tableLstCams->currentRow();
        qDebug() << "CurrentRow: " << QString::number(rowSelected);
        if(rowSelected >= 0){
            if( camSelected->isConnected == true ){
                qDebug() << "Disconnecting";
                camSelected->isConnected = false;
                camSelected->tcpPort = 0;
                memset(camSelected->IP,'\0',sizeof(camSelected->IP));
                ui->pbConnect->setIcon( QIcon(":/new/icons/imagenInte/right.gif") );
                ui->pbSnapshot->setEnabled(false);
                //ui->pbGetSlideCube->setEnabled(false);
                ui->pbShutdown->setDisabled(true);
            }else{
                qDebug() << "Connecting: ";                
                camSelected->tcpPort = lstSettings->tcpPort;
                memcpy(
                            camSelected->IP,
                            ui->tableLstCams->item(rowSelected,1)->text().trimmed().toStdString().c_str(),
                            sizeof(camSelected->IP)
                      );
                camSelected->isConnected = true;
                ui->pbConnect->setIcon( QIcon(":/new/icons/imagenInte/close.png") );
                ui->pbSnapshot->setEnabled(true);
                //ui->pbGetSlideCube->setEnabled(true);
                ui->pbShutdown->setDisabled(false);
                qDebug() << "IP->: " << QString((char*)camSelected->IP);
                //Save last IP
                saveFile( _PATH_LAST_IP, QString((char*)camSelected->IP) );
            }
        }else{
            ui->pbSnapshot->setEnabled(false);
            funcShowMsg("ERROR", "Camara not selected");            
        }
    }else{
        funcShowMsg("ERROR", "Empty cam list");
        ui->pbSnapshot->setEnabled(false);
    }

}

/*
void MainWindow::on_pbCamTurnOn_clicked()
{
    unsigned char v[20];
    v[0] = 'H';
    v[1] = 'o';
    v[2] = 'l';
    v[3] = 'a';
    std::string v2;
    v2.assign((char*)v);
    qDebug() << "Size: " << v2.size();

}
*/




/*
void MainWindow::on_pbStartVideo_clicked()
{    
    if( !camSelected->isConnected ){
        funcShowMsg("Lack","Select and connect a camera");
    }else{
        //Save camera settings
        //..
        saveRaspCamSettings( "tmp" );

        //Play
        //..
        if( camSelected->stream ){//Playing video
            camSelected->stream = false;
            //ui->pbStartVideo->setIcon( QIcon(":/new/icons/imagenInte/player_play.svg") );
            //ui->pbStartVideo->setToolTip("Play");
            qDebug() << "Paused";
        }else{
            if( ui->cbOneShot->isChecked() ){
                funcUpdateVideo( 100, 2.3 );
            }else{
                camSelected->stream = true;
                ui->pbStartVideo->setIcon( QIcon(":/new/icons/imagenInte/pause.png") );
                ui->pbStartVideo->setToolTip("Pause");
                qDebug() << "Play";
                QtDelay(100);
                while( camSelected->stream ){
                    funcUpdateVideo( 100, 2.3 );
                    QtDelay(700);
                }
            }
        }
    }

}
*/

void MainWindow::progBarTimer( int ms )
{

    //this->moveToThread(progBarThread);
    ui->progBar->setVisible(true);
    ui->progBar->setMinimum(0);
    ui->progBar->setMaximum(100);
    ui->progBar->setValue(0);
    ui->progBar->update();

    int step = floor( (float)ms/100.0);
    int i;
    for( i=0; i<=100; i++ )
    {
        ui->progBar->setValue(i);
        ui->progBar->update();
        QThread::msleep(step);
    }
    i=0;

    ui->progBar->setValue(0);
    ui->progBar->update();
    ui->progBar->setVisible(false);
    ui->progBar->update();
}

//unsigned char *MainWindow::funcGetRemoteImg( strReqImg *reqImg, bool saveImg ){
bool MainWindow::funcGetRemoteImg( strReqImg *reqImg, bool saveImg ){

    //Open socket
    int n;
    int sockfd = connectSocket( camSelected );
    unsigned char bufferRead[frameBodyLen];
    qDebug() << "Socket opened";
    //Require photo size
    //QtDelay(5);
    n = ::write(sockfd,reqImg,sizeof(strReqImg));
    qDebug() << "Img request sent";        

    //Receive ACK with the camera status
    memset(bufferRead,'\0',3);
    n = read(sockfd,bufferRead,2);
    if( bufferRead[1] == 1 ){//Begin the image adquisition routine
        qDebug() << "Camera OK";
        ::close(sockfd);

        progBarUpdateLabel("Stabilizing camera",0);
        progBarTimer( (reqImg->raspSett.TriggeringTimeSecs + 1) * 1000 );

        //Get File
        if( saveImg )
        {
            progBarUpdateLabel("Stabilizing remote camera",0);
            obtainImageFile( _PATH_REMOTE_SNAPSHOT, _PATH_IMAGE_RECEIVED );
        }

    }else{//Do nothing becouse camera is not accessible
        qDebug() << "ERROR turning on the camera";
        ::close(sockfd);
        return false;
    }
    n=n;

    /*
    //Receive photo's size
    QtDelay(1000);
    int fileLen;
    memset(bufferRead,'\0',sizeof(int)+1);
    n = read(sockfd,bufferRead,sizeof(int));

    memcpy(&fileLen,&bufferRead,sizeof(int));
    qDebug() << "Receiving fileLen: " << QString::number(fileLen);
    */

    /*
    //Receive File photogram
    int buffLen = ceil((float)fileLen/(float)frameBodyLen)*frameBodyLen;
    unsigned char *tmpFile = (unsigned char*)malloc(buffLen);
    QtDelay(60);
    if( funcReceiveFile( sockfd, fileLen, bufferRead, tmpFile ) ){
        qDebug() << "Frame received";
    }else{
        qDebug() << "ERROR: Frame does not received";
    }

    //Save a backup of the image received
    //..
    if( saveImg )
    {
        if(!saveBinFile( (unsigned long)fileLen, tmpFile,_PATH_IMAGE_RECEIVED))
        {
            qDebug()<< "ERROR: saving image-file received";
        }
    }
    */


    //Close socket and return

    //return tmpFile;

    funcLabelProgBarHide();

    return true;
}

void MainWindow::progBarUpdateLabel( QString txt, int color )
{
    ui->labelProgBar->setVisible(true);
    ui->labelProgBar->setText(txt);
    if( color == 0 )
        ui->labelProgBar->setStyleSheet("QLabel { color : black; }");
    if( color == 1 )
        ui->labelProgBar->setStyleSheet("QLabel { color : white; }");

    ui->labelProgBar->update();
    QtDelay(10);
}

void MainWindow::funcLabelProgBarHide()
{
    ui->labelProgBar->setVisible(false);
    ui->labelProgBar->update();
}

structRaspcamSettings MainWindow::funcFillSnapshotSettings( structRaspcamSettings raspSett ){
    //Take settings from gui ;D
    //raspSett.width                 = ui->slideWidth->value();
    //raspSett.height                = ui->slideHeight->value();
    memcpy(
                raspSett.AWB,
                ui->cbAWB->currentText().toStdString().c_str(),
                sizeof(raspSett.AWB)
          );
    //raspSett.Brightness            = ui->slideBrightness->value();
    //raspSett.Contrast              = ui->slideContrast->value();
    memcpy(
                raspSett.Exposure,
                ui->cbExposure->currentText().toStdString().c_str(),
                sizeof(raspSett.Exposure)
          );
    //raspSett.ExposureCompensation     = ui->slideExpComp->value();
    //raspSett.Format                   = ( ui->rbFormat1->isChecked() )?1:2;
    //raspSett.Green                    = ui->slideGreen->value();
    raspSett.ISO                        = ui->spinBoxISO->value();
    //raspSett.Red                      = ui->slideRed->value();
    //raspSett.Saturation               = ui->slideSaturation->value();
    //raspSett.Sharpness                = ui->slideSharpness->value();
    raspSett.ShutterSpeedMs             = ui->spinBoxShuterSpeed->value();
    raspSett.SquareShutterSpeedMs       = ui->spinBoxSquareShuterSpeed->value();
    raspSett.Denoise                    = (ui->cbDenoise->isChecked())?1:0;
    raspSett.ColorBalance               = (ui->cbColorBalance->isChecked())?1:0;
    raspSett.TriggeringTimeSecs         = ui->spinBoxTrigeringTime->value();

    return raspSett;
}

unsigned char * MainWindow::funcObtVideo( unsigned char saveLocally ){

    int n;

    qDebug() << "Dentro 1";
    //Open socket
    int sockfd = connectSocket( camSelected );
    unsigned int fileLen;
    unsigned char bufferRead[frameBodyLen];

    //Require photo size
    qDebug() << "Dentro 2";
    unsigned char tmpInstRes[2];
    tmpInstRes[0] = (unsigned char)6;//Request photo size
    tmpInstRes[1] = saveLocally;
    n = ::write(sockfd,&tmpInstRes,2);

    unsigned char *tmpFile;

    if( saveLocally==1 ){

        //Receive photo's size
        qDebug() << "Dentro 3";
        n = read(sockfd,bufferRead,frameBodyLen);
        memcpy(&fileLen,&bufferRead,sizeof(unsigned int));
        qDebug() << "fileLen: " << QString::number(fileLen);

        //Receive File photogram
        qDebug() << "Dentro 4";
        tmpFile = (unsigned char*)malloc(fileLen);

        if( funcReceiveFile( sockfd, fileLen, bufferRead, tmpFile ) ){
            qDebug() << "Big image received";
        }else{
            qDebug() << "ERROR: Big image does not received";
        }
    }else{
        tmpFile = (unsigned char*)malloc(2);
    }

    //Close socket
    ::close(sockfd);
    n=n;

    return tmpFile;
}

bool MainWindow::funcUpdateVideo( unsigned int msSleep, int sec2Stab ){

    msSleep = msSleep;
    mouseCursorWait();
    this->update();

    //Set required image's settings
    //..
    strReqImg *reqImg   = (strReqImg*)malloc(sizeof(strReqImg));
    reqImg->idMsg       = (unsigned char)7;    
    reqImg->stabSec     = sec2Stab;    
    reqImg->raspSett    = funcFillSnapshotSettings( reqImg->raspSett );

    qDebug() << "reqImg->raspSett.TriggerTime: " << reqImg->raspSett.TriggeringTimeSecs;

    //Define photo's region
    //..
    //QString tmpSquare2Load = (ui->cbPreview->isChecked())?_PATH_REGION_OF_INTERES:_PATH_SQUARE_APERTURE;
    if( !funGetSquareXML( _PATH_SQUARE_APERTURE, &reqImg->sqApSett ) ){
        funcShowMsg("ERROR","Loading squareAperture.xml");
        return false;
    }
    reqImg->needCut     = true;
    reqImg->squApert    = true;
    //Calculate real number of columns of the required photo
    reqImg->sqApSett.rectX  = round( ((float)reqImg->sqApSett.rectX/(float)reqImg->sqApSett.canvasW) * (float)camRes->width);
    reqImg->sqApSett.rectY  = round( ((float)reqImg->sqApSett.rectY/(float)reqImg->sqApSett.canvasH) * (float)camRes->height);
    reqImg->sqApSett.rectW  = round( ((float)reqImg->sqApSett.rectW/(float)reqImg->sqApSett.canvasW) * (float)camRes->width);
    reqImg->sqApSett.rectH  = round( ((float)reqImg->sqApSett.rectH/(float)reqImg->sqApSett.canvasH) * (float)camRes->height);

    //It save the received image
    funcGetRemoteImg( reqImg, true );
    QImage tmpImg(_PATH_IMAGE_RECEIVED);

    //tmpImg.save("./Results/tmpCropFromSave.ppm");
    ui->labelVideo->setPixmap( QPixmap::fromImage(tmpImg) );
    ui->labelVideo->setFixedWidth( tmpImg.width() );
    ui->labelVideo->setFixedHeight( tmpImg.height() );
    ui->labelVideo->update();



    //Delay in order to refresh actions applied    
    this->update();
    return true;
}







/*
void MainWindow::on_pbSaveImage_clicked()
{    
    int n;
    bool tmpSaveLocaly = funcShowMsgYesNo("Question","Bring into this PC?");
    //Stop streaming
    ui->pbStartVideo->click();
    ui->pbStartVideo->setEnabled(false);
    ui->pbStartVideo->update();

    //Open socket
    int tmpFrameLen = 2+sizeof(structRaspcamSettings);;
    int sockfd = connectSocket( camSelected );

    //Turn off camera
    unsigned char tmpInstRes[tmpFrameLen];
    tmpInstRes[0] = (unsigned char)5;//Camera instruction
    tmpInstRes[1] = (unsigned char)2;
    n = ::write(sockfd,&tmpInstRes,2);
    bzero(tmpInstRes,tmpFrameLen);
    n = read(sockfd,tmpInstRes,2);
    ::close(sockfd);

    //Turn on camera with new parameters
    sockfd = connectSocket( camSelected );
    //raspcamSettings->width  = _BIG_WIDTH;//2592, 640
    //raspcamSettings->height = _BIG_HEIGHT;//1944, 480
    tmpInstRes[0] = 5;
    tmpInstRes[1] = 1;
    memcpy( &tmpInstRes[2], raspcamSettings, sizeof(structRaspcamSettings) );
    n = ::write(sockfd,&tmpInstRes,tmpFrameLen);
    bzero(tmpInstRes,tmpFrameLen);
    n = read(sockfd,tmpInstRes,2);
    //qDebug() << "n: " << QString::number(n);
    qDebug() << "instRes: " << tmpInstRes[1];
    if( tmpInstRes[1] == (unsigned char)0 ){
        funcShowMsg("ERROR","Camera did not completed the instruction");
    }
    ::close(sockfd);


    if( tmpSaveLocaly ){

        //Get big file
        unsigned char *tmpFile = funcObtVideo( (unsigned char)1 );

        //Take file name
        char tmpFileName[16+14];
        snprintf(tmpFileName, 16+14, "tmpImages/%lu.ppm", time(NULL));

        std::ofstream outFile ( tmpFileName, std::ios::binary );
        //outFile<<"P6\n"<<raspcamSettings->width<<" "<<raspcamSettings->height<<" 255\n";
        //outFile.write( (char*)tmpFile, 3*raspcamSettings->width*raspcamSettings->height );
        outFile.close();
        delete tmpFile;
        qDebug() << "Big image saved locally";

    }else{

        //Send instruction: Save image in HypCam
        sockfd = connectSocket( camSelected );
        tmpInstRes[0] = (unsigned char)6;//Camera instruction
        tmpInstRes[1] = (unsigned char)2;
        n = ::write(sockfd,&tmpInstRes,2);
        bzero(tmpInstRes,tmpFrameLen);
        n = read(sockfd,tmpInstRes,2);
        qDebug() << "Image saved into HypCam";
        ::close(sockfd);

    }

    qDebug() << "Before turn off";

    //Turn off camera
    sockfd = connectSocket( camSelected );
    tmpInstRes[0] = (unsigned char)5;//Camera instruction
    tmpInstRes[1] = (unsigned char)2;
    n = ::write(sockfd,&tmpInstRes,2);
    bzero(tmpInstRes,tmpFrameLen);
    n = read(sockfd,tmpInstRes,2);
    ::close(sockfd);
    qDebug() << "Camera Turned off";

    //Turn on camera with new parameters
    sockfd = connectSocket( camSelected );
    funcSetCam(raspcamSettings);
    tmpInstRes[0] = 5;
    tmpInstRes[1] = 1;
    memcpy( &tmpInstRes[2], raspcamSettings, sizeof(structRaspcamSettings) );
    n = ::write(sockfd,&tmpInstRes,tmpFrameLen);
    bzero(tmpInstRes,tmpFrameLen);
    n = read(sockfd,tmpInstRes,2);
    qDebug() << "instRes: " << tmpInstRes[1];
    if( tmpInstRes[1] == (unsigned char)0 ){
        funcShowMsg("ERROR","Camera did not completed the instruction");
    }
    ::close(sockfd);

    //Start streaming
    ui->pbStartVideo->setEnabled(true);
    ui->pbStartVideo->click();

    n=n;
}
*/

/*
void MainWindow::on_slideShuterSpeed_valueChanged(int value)
{
    QString qstrVal = QString::number(value + ui->slideShuterSpeedSmall->value());
    ui->labelShuterSpeed->setText( "Diffraction Shuter Speed: " + qstrVal );
}
*/
void MainWindow::on_slideISO_valueChanged(int value)
{
    ui->labelISO->setText( "ISO: " + QString::number(value) );
}


void MainWindow::on_pbSaveRaspParam_clicked()
{
    if( ui->txtCamParamXMLName->text().isEmpty() )
    {
        funcShowMsg("Lack","Type the scenario's name");
    }else
    {

        QString tmpName = ui->txtCamParamXMLName->text();
        tmpName.replace(".xml","");
        tmpName.replace(".XML","");
        qDebug() << tmpName;

        bool saveFile = false;
        if( QFileInfo::exists( "./XML/camPerfils/" + tmpName + ".xml" ) )
        {
            if( funcShowMsgYesNo("Alert","Replace existent file?") )
            {
                QFile file( "./XML/camPerfils/" + tmpName + ".xml" );
                file.remove();
                saveFile = true;
            }
        }else
        {
            saveFile = true;
        }

        if( saveFile )
        {
            if( saveRaspCamSettings( tmpName ) )
            {
                funcShowMsg("Success","File stored successfully");
            }else
            {
                funcShowMsg("ERROR","Saving raspcamsettings");
            }
        }
    }
}

bool MainWindow::saveRaspCamSettings( QString tmpName ){

    //----------------------------------------------
    //Conditional variables
    //----------------------------------------------
    if( tmpName.isEmpty() )return false;

    tmpName = tmpName.replace(".xml","");
    tmpName = tmpName.replace(".XML","");

    //Resolution
    int tmpResInMp = -1;
    if( ui->radioRes5Mp->isChecked() )
    {
        tmpResInMp = 5;
    }
    else
    {
        tmpResInMp = 8;
    }

    //----------------------------------------------
    //Prepare file contain
    //----------------------------------------------
    QString newFileCon = "";
    QString flipped             = (ui->cbFlipped->isChecked())?"1":"0";
    QString horizontalFlipped   = (ui->cbFlippedHorizontal->isChecked())?"1":"0";
    QString denoiseFlag = (ui->cbDenoise->isChecked())?"1":"0";
    QString colbalFlag = (ui->cbColorBalance->isChecked())?"1":"0";
    newFileCon.append("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    newFileCon.append("<settings>\n");
    newFileCon.append("    <AWB>"+ ui->cbAWB->currentText() +"</AWB>\n");
    newFileCon.append("    <Exposure>"+ ui->cbExposure->currentText() +"</Exposure>\n");
    newFileCon.append("    <Denoise>"+ denoiseFlag +"</Denoise>\n");
    newFileCon.append("    <ColorBalance>"+ colbalFlag +"</ColorBalance>\n");
    newFileCon.append("    <TriggeringTimeSecs>"+ QString::number( ui->spinBoxTrigeringTime->value() ) +"</TriggeringTimeSecs>\n");
    newFileCon.append("    <ShutterSpeedMs>"+ QString::number( ui->spinBoxShuterSpeed->value() ) +"</ShutterSpeedMs>\n");
    newFileCon.append("    <SquareShutterSpeedMs>"+ QString::number( ui->spinBoxSquareShuterSpeed->value() ) +"</SquareShutterSpeedMs>\n");
    newFileCon.append("    <TimelapseDurationSecs>"+ QString::number( ui->spinBoxTimelapseDuration->value() ) +"</TimelapseDurationSecs>\n");
    newFileCon.append("    <TimelapseInterval_ms>"+ QString::number( ui->spinBoxTimelapse->value() ) +"</TimelapseInterval_ms>\n");
    //newFileCon.append("    <VideoDurationSecs>"+ QString::number( ui->spinBoxVideoDuration->value() ) +"</VideoDurationSecs>\n");
    newFileCon.append("    <DelayTime>"+ QString::number( ui->spinBoxDelayTime->value() ) +"</DelayTime>\n");
    newFileCon.append("    <ISO>"+ QString::number( ui->spinBoxISO->value() ) +"</ISO>\n");
    newFileCon.append("    <CameraMp>"+ QString::number( tmpResInMp ) +"</CameraMp>\n");
    newFileCon.append("    <Flipped>"+ flipped +"</Flipped>\n");
    newFileCon.append("    <horizontalFlipped>"+ horizontalFlipped +"</horizontalFlipped>\n");
    newFileCon.append("</settings>");

    //----------------------------------------------
    //Save
    //----------------------------------------------
    QFile newFile( "./XML/camPerfils/" + tmpName + ".xml" );
    if(newFile.exists())newFile.remove();
    if ( newFile.open(QIODevice::ReadWrite) ){
        QTextStream stream( &newFile );
        stream << newFileCon << endl;
        newFile.close();
    }else{
        return false;
    }
    return true;
}

void MainWindow::on_pbObtPar_clicked()
{

    QString filePath = QFileDialog::getOpenFileName(
                                                        this,
                                                        tr("Select XML..."),
                                                        "./XML/camPerfils/",
                                                        "(*.xml);;"
                                                     );
    if( !filePath.isEmpty() ){
        QStringList tmpList = filePath.split("/");
        tmpList = tmpList.at( tmpList.count()-1 ).split(".");
        ui->txtCamParamXMLName->setText( tmpList.at( 0 ) );

        funcGetRaspParamFromXML( raspcamSettings, filePath );
        funcIniCamParam( raspcamSettings );
    }
}



bool MainWindow::funcSetCam( structRaspcamSettings *raspcamSettings ){


        memcpy(
                    raspcamSettings->AWB,
                    ui->cbAWB->currentText().toStdString().c_str(),
                    sizeof(raspcamSettings->AWB)
              );

        memcpy(
                    raspcamSettings->Exposure,
                    ui->cbExposure->currentText().toStdString().c_str(),
                    sizeof(raspcamSettings->Exposure)
              );


        //raspcamSettings->width = ui->slideWidth->value();

        //raspcamSettings->height = ui->slideHeight->value();

        //raspcamSettings->Brightness = ui->slideBrightness->value();

        //raspcamSettings->Sharpness = ui->slideSharpness->value();

        //raspcamSettings->Contrast = ui->slideContrast->value();

        //raspcamSettings->Saturation = ui->slideSaturation->value();

        raspcamSettings->ShutterSpeedMs = ui->spinBoxShuterSpeed->value();

        raspcamSettings->ISO = ui->spinBoxISO->value();

        //raspcamSettings->ExposureCompensation = ui->slideExpComp->value();

        //raspcamSettings->Red = ui->slideRed->value();

        //raspcamSettings->Green = ui->slideGreen->value();



    /*
    memcpy(raspcamSettings->AWB,"AUTO\0",5);            // OFF,AUTO,SUNLIGHT,CLOUDY,TUNGSTEN,FLUORESCENT,INCANDESCENT,FLASH,HORIZON
    memcpy(raspcamSettings->Exposure,"FIREWORKS\0",6);      // OFF,AUTO,NIGHT,NIGHTPREVIEW,BACKLIGHT,SPOTLIGHT,SPORTS,SNOW,BEACH,VERYLONG,FIXEDFPS,ANTISHAKE,FIREWORKS

    raspcamSettings->width                  = 384;      // 1280 to 2592
    raspcamSettings->height                 = 288;      // 960 to 1944
    raspcamSettings->Brightness             = 50;       // 0 to 100
    raspcamSettings->Sharpness              = 0;        // -100 to 100
    raspcamSettings->Contrast               = 0;        // -100 to 100
    raspcamSettings->Saturation             = 0;        // -100 to 100
    raspcamSettings->ShutterSpeed           = 1000*75;  // microsecs (max 330000)
    raspcamSettings->ISO                    = 400;      // 100 to 800
    raspcamSettings->ExposureCompensation   = 0;        // -10 to 10
    raspcamSettings->Format                 = 2;        // 1->raspicam::RASPICAM_FORMAT_GRAY | 2->raspicam::RASPICAM_FORMAT_YUV420
    raspcamSettings->Red                    = 0;        // 0 to 8 set the value for the RED component of white balance
    raspcamSettings->Green                  = 0;        // 0 to 8 set the value for the GREEN component of white balance
    */
    return true;
}


void MainWindow::funcGetSnapshot()
{

    //Getting calibration
    //..
    lstDoubleAxisCalibration daCalib;
    funcGetCalibration(&daCalib);

    //Save path
    //..
    //saveFile(_PATH_LAST_SNAPPATH,ui->txtSnapPath->text());

    //Save lastest settings
    if( saveRaspCamSettings( _PATH_LAST_SNAPPATH ) == false ){
        funcShowMsg("ERROR","Saving last snap-settings");
    }

    //Set required image's settings
    //..
    strReqImg *reqImg       = (strReqImg*)malloc(sizeof(strReqImg));
    memset(reqImg,'\0',sizeof(strReqImg));
    reqImg->idMsg           = (unsigned char)7;
    reqImg->raspSett        = funcFillSnapshotSettings( reqImg->raspSett );

    //Define photo's region
    //..
    reqImg->needCut     = false;
    reqImg->fullFrame   = true;
    reqImg->imgCols     = camRes->width;//2592 | 640
    reqImg->imgRows     = camRes->height;//1944 | 480
    //It saves image into HDD: _PATH_IMAGE_RECEIVED

    if( funcGetRemoteImg( reqImg, true ) )
    {
        //Display image
        QImage imgAperture( _PATH_IMAGE_RECEIVED );

        //Invert phot if required
        //if( _INVERTED_CAMERA )
        //    imgAperture = funcRotateImage( _PATH_DISPLAY_IMAGE , 180 );
        imgAperture.save( _PATH_DISPLAY_IMAGE );

        //Display image into qlabel
        updateDisplayImage(&imgAperture);
    }
    else funcShowMsg("ERROR","Camera respond with error");
    funcLabelProgBarHide();

}

void MainWindow::mergeSnapshot(QImage *diff, QImage *aper, lstDoubleAxisCalibration *daCalib )
{
    int x1, y1;
    int row, col;

    x1 = daCalib->squarePixX;
    y1 = daCalib->squarePixY;

    for(row=0;row<aper->height();row++)
    {
        for(col=0;col<aper->width();col++)
        {
            diff->setPixel(x1+col,y1+row,aper->pixel(col,row));
        }
    }

    diff->save( _PATH_DISPLAY_IMAGE );
}


/*
void MainWindow::getRemoteImgByPartsAndSave( strReqImg *reqImg )
{
    //It saves image into HDD: _PATH_IMAGE_RECEIVED
    funcGetRemoteImg( reqImg, true );
}
*/

/*
void MainWindow::calcRectangles(
                                    QList<QRect> *lstRect,
                                    strDiffProj  *p11Min,
                                    strDiffProj  *p12Min,
                                    strDiffProj  *p21Min,
                                    strDiffProj  *p22Min,
                                    strDiffProj  *p11Max,
                                    strDiffProj  *p12Max,
                                    strDiffProj  *p21Max,
                                    strDiffProj  *p22Max
                               ){

}
*/

void MainWindow::calcDiffPoints(
                                    float wave,
                                    strDiffProj *p11,
                                    strDiffProj *p12,
                                    strDiffProj *p21,
                                    strDiffProj *p22,
                                    lstDoubleAxisCalibration *daCalib
                                ){

    p11->wavelength     = wave;
    p12->wavelength     = wave;
    p21->wavelength     = wave;
    p22->wavelength     = wave;

    p11->x              = 1;                    //1-index
    p11->y              = 1;                    //1-index
    p12->x              = daCalib->squarePixW;  //1-index
    p12->y              = 1;                    //1-index
    p21->x              = 1;                    //1-index
    p21->y              = daCalib->squarePixH;  //1-index
    p22->x              = daCalib->squarePixW;  //1-index
    p22->y              = daCalib->squarePixH;  //1-index

    calcDiffProj( p11, daCalib );
    calcDiffProj( p12, daCalib );
    calcDiffProj( p21, daCalib );
    calcDiffProj( p22, daCalib );

}


void MainWindow::getMaxCalibRect( QRect *rect, lstDoubleAxisCalibration *calib )
{
    //It defines points in the rectangle
    QPoint p11, p12, p21, p22;
    p11.setX(rect->x());                    p11.setY(rect->y());
    p12.setX(rect->x()+rect->width());      p12.setY(rect->y());
    p21.setX(rect->x());                    p21.setY(rect->y()+rect->height());
    p22.setX(rect->x()+rect->width());      p22.setY(rect->y()+rect->height());

    //It calibrates rectangle
    calibPoint( &p11, calib );      calibPoint( &p12, calib );
    calibPoint( &p21, calib );      calibPoint( &p22, calib );

    //Defines rectangle calibrated
    int x1, y1, x2, y2, w, h;
    x1 = (p11.x()<p21.x())?p11.x():p21.x();
    y1 = (p11.y()<p12.y())?p11.y():p12.y();
    x2 = (p12.x()>p22.x())?p12.x():p22.x();
    y2 = (p21.y()>p22.y())?p21.y():p22.y();
    w  = abs(x2 - x1);
    h  = abs(y2 - y1);

    //Set calibrated rectangle
    rect->setX(x1);
    rect->setY(y1);
    rect->setWidth(w);
    rect->setHeight(h);

}

//QString MainWindow::getFilenameForRecImg()
//{
    //QString tmpFileName = ui->txtSnapPath->text().trimmed();
    //tmpFileName.append(QString::number(time(NULL)));
   // tmpFileName.append(".RGB888");
    //QFile::copy(_PATH_IMAGE_RECEIVED, tmpFileName);
    //return tmpFileName;
//}



void MainWindow::updateDisplayImage(QImage* tmpImg)
{
    mouseCursorWait();

    //Update Image Preview
    updatePreviewImage(tmpImg);

    //Update Edit View
    updateImageCanvasEdit(tmpImg);

    //Save Image
    tmpImg->save(_PATH_DISPLAY_IMAGE);

    mouseCursorReset();

}

void MainWindow::updateDisplayImage(QString fileName)
{
    mouseCursorWait();

    //Load Image
    auxQstring = fileName;
    QImage tmpImg(auxQstring);

    //Update Image Preview
    updatePreviewImage(&tmpImg);

    //Update Edit View
    updateImageCanvasEdit(&tmpImg);

    mouseCursorReset();
}

void MainWindow::updatePreviewImage(QString* fileName)
{
    //
    //Display Snapshot
    //
    QImage tmpImg( *fileName );
    int maxW, maxH;
    maxW = (_FRAME_THUMB_W<tmpImg.width())?_FRAME_THUMB_W:tmpImg.width();
    maxH = (_FRAME_THUMB_H<tmpImg.height())?_FRAME_THUMB_H:tmpImg.height();
    QImage tmpThumb = tmpImg.scaledToHeight(maxH);
    tmpThumb = tmpThumb.scaledToWidth(maxW);
    ui->labelVideo->setPixmap( QPixmap::fromImage(tmpThumb) );
    ui->labelVideo->setFixedSize( tmpThumb.width(), tmpThumb.height() );
}

void MainWindow::updatePreviewImage(QImage* tmpImg)
{
    //---------------------------------------
    //Calc thumb size
    //---------------------------------------
    int thumbW, thumbH;
    thumbW = this->geometry().width() - 455;
    thumbH = this->geometry().height() - 125;
    //qDebug() << "thumbW: " << thumbW << "thumbH: " << thumbH;

    //---------------------------------------
    //Display Snapshot
    //---------------------------------------
    int maxW, maxH;    
    maxH = (thumbH<tmpImg->height())?thumbH:tmpImg->height();
    QImage tmpThumb = tmpImg->scaledToHeight(maxH);
    maxW = (thumbW<tmpThumb.width())?thumbW:tmpThumb.width();
    tmpThumb = tmpThumb.scaledToWidth(maxW);
    ui->labelVideo->setPixmap( QPixmap::fromImage(tmpThumb) );
    ui->labelVideo->setFixedSize( tmpThumb.width(), tmpThumb.height() );
}

void MainWindow::on_pbSnapshot_clicked()
{

    /*
    mouseCursorWait();

    camRes = getCamRes();



    funcGetSnapshot();


    mouseCursorReset();
    */

    if( camSelected->isConnected == false )
    {
        funcShowMsgERROR("Camera Offline",this);
        return (void)false;
    }

    mouseCursorWait();

    if( !takeRemoteSnapshot(_PATH_REMOTE_SNAPSHOT,false) )
    {
        qDebug() << "ERROR: Taking Full Area";
        return (void)NULL;
    }

    //
    //Timer
    //
    /*
    int triggeringTime;
    triggeringTime = ui->slideTriggerTime->value();
    if( triggeringTime > 0 )
    {
        formTimerTxt* timerTxt = new formTimerTxt(this,"Remainning Time to Shoot...",triggeringTime);
        timerTxt->setModal(true);
        timerTxt->show();
        QtDelay(200);
        timerTxt->startMyTimer(triggeringTime);
    }*/

    //QImage snapShot = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
    *globalEditImg = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
    saveFile(_PATH_LAST_USED_IMG_FILENAME,_PATH_IMAGE_RECEIVED);
    updateDisplayImage(globalEditImg);
    //snapShot.save(_PATH_DISPLAY_IMAGE);

    funcResetStatusBar();
    mouseCursorReset();


    //
    // Show Last Preview
    //
    //updateDisplayImageReceived();


}





void MainWindow::funcPutImageIntoGV(QGraphicsView *gvContainer, QString imgPath){    
    QPixmap pim(imgPath);
    QGraphicsScene *scene = new QGraphicsScene(0,0,pim.width(),pim.height());
    scene->setBackgroundBrush(pim);
    gvContainer->setScene(scene);
    gvContainer->resize(pim.width(),pim.height());
    gvContainer->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    gvContainer->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

    /*
    QHBoxLayout *layout = new QHBoxLayout;
    ui->tab_3->setLayout(layout);
    ui->tab_3->layout()->addWidget(gvContainer);
    layout->setEnabled(false);
    */
}


/*
void MainWindow::on_pbPtsClearAll_clicked()
{
    lstPixSelAux->clear();

    canvasAux->scene()->clear();

    lstBorder->clear();
    lstSelPix->clear();


}*/


/*
void MainWindow::on_pbSavePixs_clicked()
{
    if( lstSelPix->isEmpty() ){
        funcShowMsg("LACK","Not pixels to export");
    }else{
        int i;
        QString filePath = "./Results/lstPixels.txt";
        QFile fileLstPixels(filePath);
        if (!fileLstPixels.open(QIODevice::WriteOnly | QIODevice::Text)){
            funcShowMsg("ERROR","Creating file fileLstPixels");
        }else{
            QTextStream streamLstPixels(&fileLstPixels);
            for( i=0; i<lstSelPix->count(); i++ ){
                streamLstPixels << QString::number(lstSelPix->at(i).first) << " "<< QString::number(lstSelPix->at(i).second) << "\n";
            }
        }
        fileLstPixels.close();
        funcShowMsg("Success","List of pixels exported into: "+filePath);
    }
}
*/


/*
bool MainWindow::on_pb2XY_clicked()
{
    //Validate that exist pixel selected
    //..
    if( lstSelPix->count()<1){
        funcShowMsg("Lack","Not pixels selected");
        return false;
    }

    //Create xycolor space
    //..
    GraphicsView *xySpace = new GraphicsView(this);
    funcPutImageIntoGV(xySpace, "./imgResources/CIEManual.png");
    xySpace->setWindowTitle( "XY color space" );
    xySpace->show();

    //Transform each pixel from RGB to xy and plot it
    //..
    QImage tmpImg(imgPath);
    QRgb tmpPix;
    colSpaceXYZ *tmpXYZ = (colSpaceXYZ*)malloc(sizeof(colSpaceXYZ));
    int tmpOffsetX = -13;
    int tmpOffsetY = -55;
    int tmpX, tmpY;
    int i;
    for( i=0; i<lstSelPix->count(); i++ ){
        tmpPix = tmpImg.pixel( lstSelPix->at(i).first, lstSelPix->at(i).second );
        funcRGB2XYZ( tmpXYZ, (float)qRed(tmpPix), (float)qGreen(tmpPix), (float)qBlue(tmpPix) );
        //funcRGB2XYZ( tmpXYZ, 255.0, 0, 0  );
        tmpX = floor( (tmpXYZ->x * 441.0) / 0.75 ) + tmpOffsetX;
        tmpY = 522 - floor( (tmpXYZ->y * 481.0) / 0.85 ) + tmpOffsetY;
        funcAddPoit2Graph( xySpace, tmpX, tmpY, 1, 1,
                           QColor(qRed(tmpPix),qGreen(tmpPix),qBlue(tmpPix)),
                           QColor(qRed(tmpPix),qGreen(tmpPix),qBlue(tmpPix))
                         );
    }

    //Save image plotted
    //..
    QPixmap pixMap = QPixmap::grabWidget(xySpace);
    pixMap.save("./Results/Miscelaneas/RGBPloted.png");

    return true;
}
*/


/*
void MainWindow::on_pbLoadImg_clicked()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("Open File"),"./imgResources/",tr("Image files (*.*)"));
    imgPath = fileNames.at(0);
    funcPutImageIntoGV( myCanvas, imgPath );
    //funcShowMsg("",fileNames.at(0));
}
*/

/*
void MainWindow::on_pbUpdCut_clicked()
{

    //Set Canvas dimensions
    //..
    int maxW = 640;
    int maxH = 480;

    QString imgPath = "./imgResources/tmpBigToCut.ppm";

    if( funcShowMsgYesNo("Alert","Get remote image?") ){
        //Set required image's settings
        //..
        ui->progBar->setVisible(true);
        strReqImg *reqImg       = (strReqImg*)malloc(sizeof(strReqImg));
        reqImg->idMsg           = (unsigned char)7;
        reqImg->needCut         = false;
        reqImg->stabSec         = 3;
        reqImg->imgCols         = _BIG_WIDTH;//2592 | 640
        reqImg->imgRows         = _BIG_HEIGHT;//1944 | 480
        reqImg->raspSett        = funcFillSnapshotSettings( reqImg->raspSett );
        unsigned char *tmpFrame = funcGetRemoteImg( reqImg );
        std::ofstream outFile ( imgPath.toStdString(), std::ios::binary );
        outFile<<"P6\n"<<reqImg->imgCols<<" "<<reqImg->imgRows<<" 255\n";
        outFile.write( (char*)tmpFrame, 3*reqImg->imgCols*reqImg->imgRows );
        outFile.close();
        //delete tmpFrame;
        qDebug() << "Big image saved locally";
        ui->progBar->setVisible(false);
    }

    //Load image
    //..
    QImage imgOrig(imgPath);
    QImage tmpImg = imgOrig.scaled(QSize(640,480),Qt::KeepAspectRatio);
    QGraphicsScene *scene = new QGraphicsScene(0,0,tmpImg.width(),tmpImg.height());
    scene->setBackgroundBrush( QPixmap::fromImage(tmpImg) );
    ui->gvCut->setScene( scene );

    //Set UI
    //..
    ui->gvCut->setFixedSize( maxW, maxH );
    ui->gvCut->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    ui->gvCut->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    ui->slideCutPosX->setMaximum( maxW );
    ui->slideCutPosY->setMaximum(maxH);
    ui->slideCutWX->setMaximum( maxW );
    ui->slideCutWY->setMaximum( maxH );
    //ui->slideCutWX->setMinimum(5);
    //ui->slideCutWY->setMinimum(5);
    ui->slideCutWX->setEnabled(true);
    ui->slideCutWY->setEnabled(true);
    ui->slideCutPosX->setEnabled(true);
    ui->slideCutPosY->setEnabled(true);
    ui->pbSaveSquare->setEnabled(true);
    ui->pbSaveBigSquare->setEnabled(true);


}



void MainWindow::funcSetLines(){
    int xPos    = ui->slideCutPosX->value();
    int xW      = ui->slideCutWX->value();
    int yPos    = ui->slideCutPosY->value();
    int yW      = ui->slideCutWY->value();
    ui->gvCut->scene()->clear();
    ui->gvCut->scene()->addLine(xPos,1,xPos,ui->gvCut->height(),QPen(Qt::red));
    ui->gvCut->scene()->addLine(xPos+xW,1,xPos+xW,ui->gvCut->height(),QPen(Qt::red));
    ui->gvCut->scene()->addLine(1,ui->gvCut->height()-yPos,ui->gvCut->width(),ui->gvCut->height()-yPos,QPen(Qt::red));
    ui->gvCut->scene()->addLine(1,ui->gvCut->height()-yPos-yW,ui->gvCut->width(),ui->gvCut->height()-yPos-yW,QPen(Qt::red));
}
*/

/*
void MainWindow::on_slideCutPosX_valueChanged(int xpos)
{
    xpos = xpos;
    funcSetLines();
}

void MainWindow::on_slideCutWX_valueChanged(int value)
{
    value = value;
    funcSetLines();
}

void MainWindow::on_slideCutWY_valueChanged(int value)
{
    value = value;
    funcSetLines();
}

void MainWindow::on_slideCutPosY_valueChanged(int value)
{
    value = value;
    funcSetLines();
}
*/
/*
void MainWindow::on_pbSaveSquare_clicked()
{
    if( funcShowMsgYesNo("Alert","Do you want to replace the setting?") == 1 ){
        if( funcSaveRect ( "./XML/squareAperture.xml" ) ){
            funcShowMsg("Success","Settings saved");
        }else{
            funcShowMsg("ERROR","It can NOT save settings");
        }
    }
}
*/

/*
bool MainWindow::funcSaveRect( QString fileName ){
    QString tmpContain;
    tmpContain.append( "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n" );
    tmpContain.append("<Variables>\n");
    tmpContain.append("\t<width>"+ QString::number(ui->gvCut->width()) +"</width>\n");
    tmpContain.append("\t<height>"+ QString::number(ui->gvCut->height()) +"</height>\n");
    tmpContain.append("\t<x1>"+ QString::number( ui->slideCutPosX->value() ) +"</x1>\n");
    tmpContain.append("\t<y1>"+ QString::number( ui->gvCut->height() - ui->slideCutPosY->value() - ui->slideCutWY->value() ) +"</y1>\n");
    tmpContain.append("\t<x2>"+ QString::number( ui->slideCutPosX->value() + ui->slideCutWX->value() ) +"</x2>\n");
    tmpContain.append("\t<y2>"+ QString::number( ui->gvCut->height() - ui->slideCutPosY->value() ) +"</y2>\n");
    tmpContain.append("</Variables>");
    if( !saveFile( fileName, tmpContain ) ){
        return false;
    }
    return true;
}
*/

/*
void MainWindow::on_pbSaveBigSquare_clicked()
{
    if( funcShowMsgYesNo("Alert","Do you want to replace the setting?") == 1 ){
        if( funcSaveRect ( "./XML/bigSquare.xml" ) ){
            funcShowMsg("Success","Settings saved");
        }else{
            funcShowMsg("ERROR","It can NOT save settings");
        }
    }
}
*/

void MainWindow::on_pbSpecSnapshot_clicked()
{
    /*
    //Turn on camera
    //..
    CvCapture* usbCam  = cvCreateCameraCapture( ui->sbSpecUsb->value() );
    cvSetCaptureProperty( usbCam,  CV_CAP_PROP_FRAME_WIDTH,  320*_FACT_MULT );
    cvSetCaptureProperty( usbCam,  CV_CAP_PROP_FRAME_HEIGHT, 240*_FACT_MULT );
    assert( usbCam );

    //Create image
    //..    
    IplImage *tmpCam = cvQueryFrame( usbCam );
    if( ( tmpCam = cvQueryFrame( usbCam ) ) ){
        //Get the image
        QString tmpName = "./snapshots/tmpUSB.png";
        if( _USE_CAM ){
            IplImage *imgRot = cvCreateImage(
                                                cvSize(tmpCam->height,tmpCam->width),
                                                tmpCam->depth,
                                                tmpCam->nChannels
                                            );
            cvTranspose(tmpCam,imgRot);
            cvTranspose(tmpCam,imgRot);
            cvTranspose(tmpCam,imgRot);

            cv::imwrite( tmpName.toStdString(), cv::cvarrToMat(imgRot) );
            //cv::imwrite( tmpName.toStdString(), cv::Mat(imgRot, true) );
        }
        //Create canvas
        //Display accum
        //..        

        //Show image
        QPixmap pix(tmpName);        
        pix = pix.scaledToHeight( _GRAPH_HEIGHT );
        QGraphicsScene *scene = new QGraphicsScene(0,0,pix.width(),pix.height());
        canvasSpec->setBackgroundBrush(pix);
        canvasSpec->setScene( scene );
        canvasSpec->resize(pix.width(),pix.height());
        canvasSpec->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        canvasSpec->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        ui->tab_5->layout()->addWidget(canvasSpec);
        ui->tab_5->layout()->setEnabled(false);
        ui->tab_5->layout()->setAlignment(Qt::AlignLeft);
        ui->gridColors->setAlignment(Qt::AlignLeft);
        ui->gridColors->setAlignment(Qt::AlignLeft);


        ui->slideRedLen->setMaximumWidth(pix.width());
        ui->slideGreenLen->setMaximumWidth(pix.width());
        ui->slideBlueLen->setMaximumWidth(pix.width());

        pix.save("./snapshots/tmpThumb.png");




    }else{
        funcShowMsg("ERROR", "cvQueryFrame empty");
    }
    cvReleaseCapture(&usbCam);

    //Reset slides
    ui->slideRedLen->setValue(0);
    ui->slideGreenLen->setValue(0);
    ui->slideBlueLen->setValue(0);

    //Reset lines
    funcUpdateColorSensibilities();
    funcDrawLines(0,0,0,0);
    */
}

void MainWindow::funcBeginRect(QMouseEvent* e){
    calStruct.x1 = e->x();
    calStruct.y1 = e->y();
    qDebug() << "funcBeginRect";
}

void MainWindow::funcEndRect(QMouseEvent* e, GraphicsView *tmpCanvas){
    calStruct.x2 = e->x();
    calStruct.y2 = e->y();
    //ui->pbSpecCut->setEnabled(true);
    qDeleteAll(tmpCanvas->scene()->items());
    int x1, y1, x2, y2, w, h;
    x1 = (calStruct.x1<=e->x())?calStruct.x1:e->x();
    x2 = (calStruct.x1>=e->x())?calStruct.x1:e->x();
    y1 = (calStruct.y1<=e->y())?calStruct.y1:e->y();
    y2 = (calStruct.y1>=e->y())?calStruct.y1:e->y();
    w  = abs(x2-x1);
    h  = abs(y2-y1);
    QPoint p1, p2;
    p1.setX(x1);   p1.setY(y1);
    p2.setX(w);    p2.setY(h);
    customRect* tmpRect = new customRect(p1,p2,globalEditImg);
    tmpRect->mapToScene(p1.x(),p1.y(),p2.x(),p2.y());
    //customRect* tmpRect = new customRect(this);
    tmpRect->parameters.W = canvasCalib->scene()->width();
    tmpRect->parameters.H = canvasCalib->scene()->height();
    tmpRect->setPen( QPen(Qt::red) );
    tmpCanvas->scene()->addItem(tmpRect);    
    tmpRect->setFocus();
    tmpRect->parameters.movible = true;
    tmpRect->parameters.canvas = tmpCanvas;
    tmpRect->parameters.backgroundPath = auxQstring;
}

void MainWindow::funcAnalizeAreaSelected(QPoint p1, QPoint p2){
    p1 = p1;
    p2 = p2;
}

void MainWindow::on_chbRed_clicked()
{
    funcUpdateColorSensibilities();
    funcDrawLines(0,0,0,0);
}

void MainWindow::on_chbGreen_clicked()
{
    funcUpdateColorSensibilities();
    funcDrawLines(0,0,0,0);
}

void MainWindow::on_chbBlue_clicked()
{
    funcUpdateColorSensibilities();
    funcDrawLines(0,0,0,0);
}

void MainWindow::funcUpdateColorSensibilities(){
    //Accumulate values in each color
    //..
    canvasSpec->scene()->clear();
    QImage tmpImg( "./snapshots/tmpThumb.png" );
    int Red[tmpImg.width()];memset(Red,'\0',tmpImg.width());
    int Green[tmpImg.width()];memset(Green,'\0',tmpImg.width());
    int Blue[tmpImg.width()];memset(Blue,'\0',tmpImg.width());
    int r, c, maxR, maxG, maxB, xR = 0, xG = 0, xB = 0;
    maxR = 0;
    maxG = 0;
    maxB = 0;
    QRgb pixel;
    for(c=0;c<tmpImg.width();c++){
        Red[c]   = 0;
        Green[c] = 0;
        Blue[c]  = 0;
        for(r=0;r<tmpImg.height();r++){
            if(!tmpImg.valid(QPoint(c,r))){
                qDebug() << "Invalid r: " << r << "c: "<<c;
                qDebug() << "tmpImg.width(): " << tmpImg.width();
                qDebug() << "tmpImg.height(): " << tmpImg.height();
                return (void)NULL;
                close();
            }
            pixel     = tmpImg.pixel(QPoint(c,r));
            Red[c]   += qRed(pixel);
            Green[c] += qGreen(pixel);
            Blue[c]  += qBlue(pixel);
        }
        Red[c]   = round((float)Red[c]/tmpImg.height());
        Green[c] = round((float)Green[c]/tmpImg.height());
        Blue[c]  = round((float)Blue[c]/tmpImg.height());
        if( Red[c] > maxR ){
            maxR = Red[c];
            xR = c;
        }
        if( Green[c] > maxG ){
            maxG = Green[c];
            xG = c;
        }
        if( Blue[c] > maxB ){
            maxB = Blue[c];
            xB = c;
        }
        //qDebug() << "xR: " << xR << "xG: " << xG << "xB: " << xB;
    }

    //Move slides
    //..
    //ui->slideRedLen->setMaximum(tmpImg.width());
    //ui->slideGreenLen->setMaximum(tmpImg.width());
    //ui->slideBlueLen->setMaximum(tmpImg.width());

    //ui->slideRedLen->setValue(xR);
    //ui->slideGreenLen->setValue(xG);
    //ui->slideBlueLen->setValue(xB);

    //qDebug() << "c" << c << "maxR:"<<maxR<<" maxG:"<<maxG<<" maxB:"<<maxB;
    //qDebug() << "c" << c << "xR:"<<xR<<" xG:"<<xG<<" xB:"<<xB;
    //qDebug() << "tmpImg.width(): " << tmpImg.width();
    //qDebug() << "tmpImg.height(): " << tmpImg.height();

    //Draw lines and locate lines
    //..
    int tmpPoint1, tmpPoint2, tmpHeight;
    //tmpHeight = _GRAPH_HEIGHT;
    tmpHeight = tmpImg.height();
    /*
    for(c=1;c<tmpImg.width();c++){
        if( ui->chbRed->isChecked() ){
            tmpPoint1 = tmpHeight-Red[c-1];
            tmpPoint2 = tmpHeight-Red[c];
            //tmpPoint1 = tmpImg.height()-Red[c-1];
            //tmpPoint2 = tmpImg.height()-Red[c];
            canvasSpec->scene()->addLine( c, tmpPoint1, c+1, tmpPoint2, QPen(QColor("#FF0000")) );
        }
        if( ui->chbGreen->isChecked() ){
            tmpPoint1 = tmpHeight-Green[c-1];
            tmpPoint2 = tmpHeight-Green[c];
            canvasSpec->scene()->addLine( c, tmpPoint1, c+1, tmpPoint2, QPen(QColor("#00FF00")) );
        }
        if( ui->chbBlue->isChecked() ){
            tmpPoint1 = tmpHeight-Blue[c-1];
            tmpPoint2 = tmpHeight-Blue[c];
            canvasSpec->scene()->addLine( c, tmpPoint1, c+1, tmpPoint2, QPen(QColor("#0000FF")) );
        }        
    }
    */
}

void MainWindow::funcDrawLines(int flag, int xR, int xG, int xB){
    //Draw lines
    //..
    /*
    ui->slideRedLen->setEnabled(true);
    ui->slideGreenLen->setEnabled(true);
    ui->slideBlueLen->setEnabled(true);

    if( flag == 0 ){//Get x from slides
        xR = ui->slideRedLen->value();
        xG = ui->slideGreenLen->value();
        xB = ui->slideBlueLen->value();
    }

    //qDebug() << "flag:" << flag << "xR:" << xR << " xG:" << xG << "xB: " << xB;

    canvasSpec->scene()->addLine( xR,1,xR,canvasSpec->height(),QPen(QColor("#FF0000")) );
    canvasSpec->scene()->addLine( xG,1,xG,canvasSpec->height(),QPen(QColor("#00FF00")) );
    canvasSpec->scene()->addLine( xB,1,xB,canvasSpec->height(),QPen(QColor("#0000FF")) );

    ui->slideRedLen->setValue(xR);
    ui->slideGreenLen->setValue(xG);
    ui->slideBlueLen->setValue(xB);
    */
}

void MainWindow::on_pbSpecCut_clicked()
{   /*
    //Prepare variables
    //..
    int w, h, W, H;
    QPixmap tmpPix("./snapshots/tmpUSB.png");
    W = tmpPix.width();
    H = tmpPix.height();
    w = canvasSpec->width();
    h = canvasSpec->height();
    //Extrapolate dimensions
    qDebug() << calStruct.x1 << ", " << calStruct.y1;
    qDebug() << calStruct.x2 << ", " << calStruct.y2;
    calStruct.X1 = round( ((float)W/(float)w)*(float)calStruct.x1 );
    calStruct.Y1 = round( ((float)H/(float)h)*(float)calStruct.y1 );
    calStruct.X2 = round( ((float)W/(float)w)*(float)calStruct.x2 );
    calStruct.Y2 = round( ((float)H/(float)h)*(float)calStruct.y2 );
    calStruct.lenW = abs(calStruct.X2-calStruct.X1);
    calStruct.lenH = abs(calStruct.Y2-calStruct.Y1);
    qDebug() << calStruct.X1 << ", " << calStruct.Y1;
    qDebug() << calStruct.X2 << ", " << calStruct.Y2;
    qDebug() << "lenW=" << calStruct.lenW;
    qDebug() << "lenH=" << calStruct.lenH;

    //Crop image
    //..
    //qDeleteAll(canvasSpec->scene()->items());
    QPixmap cropped = tmpPix.copy( QRect( calStruct.X1, calStruct.Y1, calStruct.lenW, calStruct.lenH ) );
    cropped.save("./snapshots/tmpThumb.png");
    cropped.save("./snapshots/tmpUSB.png");
    QGraphicsScene *scene = new QGraphicsScene(0,0,cropped.width(),cropped.height());
    canvasSpec->setBackgroundBrush(cropped);
    canvasSpec->setCacheMode(QGraphicsView::CacheNone);
    canvasSpec->setScene( scene );
    canvasSpec->resize(cropped.width(),cropped.height());
    canvasSpec->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvasSpec->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    ui->tab_5->layout()->addWidget(canvasSpec);
    ui->tab_5->layout()->setEnabled(false);
    ui->tab_5->layout()->setAlignment(Qt::AlignLeft);
    //Set slide max
    ui->slideRedLen->setMaximumWidth(cropped.width());
    ui->slideGreenLen->setMaximumWidth(cropped.width());
    ui->slideBlueLen->setMaximumWidth(cropped.width());



    //Update lines
    //..
    funcUpdateColorSensibilities();

    //Update lines
    //..
    funcDrawLines(0,0,0,0);

    //int tmpCanvW = _GRAPH_HEIGHT;
    int tmpCanvW = cropped.height();
    canvasSpec->resize(QSize(cropped.width(),tmpCanvW));


    ui->pbViewBack->setEnabled(true);
    ui->pbSnapCal->setEnabled(true);
    */


}

void MainWindow::on_slideRedLen_sliderReleased()
{/*
    int x1, x2, x3;
    x1 = ui->slideRedLen->value();
    x2 = ui->slideGreenLen->value();
    x3 = ui->slideBlueLen->value();
    funcUpdateColorSensibilities();
    funcDrawLines(1,x1,x2,x3);
    qDebug() << "Slide released";
    */
}


void MainWindow::on_slideBlueLen_sliderReleased()
{
    /*
    int x1, x2, x3;
    x1 = ui->slideRedLen->value();
    x2 = ui->slideGreenLen->value();
    x3 = ui->slideBlueLen->value();
    funcUpdateColorSensibilities();
    funcDrawLines(1,x1,x2,x3);
    qDebug() << "Slide released";
    */
}

void MainWindow::on_slideGreenLen_sliderReleased()
{/*
    int x1, x2, x3;
    x1 = ui->slideRedLen->value();
    x2 = ui->slideGreenLen->value();
    x3 = ui->slideBlueLen->value();
    funcUpdateColorSensibilities();
    funcDrawLines(1,x1,x2,x3);
    qDebug() << "Slide released";
    */
}


void MainWindow::on_pbViewBack_clicked()
{
    //canvasSpec->scene()->clear();
    //QGraphicsScene *scene = new QGraphicsScene(0,0,canvasSpec->scene()->width(),canvasSpec->scene()->height());
    //QPixmap backBlack("./imgResources/backBlack.png");
    //scene->setBackgroundBrush(backBlack);
    //canvasSpec->setScene(scene);
    canvasSpec->scene()->invalidate(1,1,200,100);

    canvasSpec->setStyleSheet("background-color: black;");


}

void MainWindow::on_pbSnapCal_clicked()
{
    /*
    QString imgCropPath = "./snapshots/tmp/calibCrop.png";

    //Get image from camera
    //..
    QString imgPath = "./snapshots/tmp/calib.png";
    if( _USE_CAM ){
        IplImage *imgCal = funcGetImgFromCam( ui->sbSpecUsb->value(), 500 );

        cv::imwrite( imgPath.toStdString(), cv::cvarrToMat(imgCal) );
        //cv::imwrite( imgPath.toStdString(), cv::Mat(imgCal, true) );
    }

    //Cut image
    //..
    QPixmap tmpOrigPix(imgPath);
    QPixmap tmpImgCrop = tmpOrigPix.copy( QRect( calStruct.X1, calStruct.Y1, calStruct.lenW, calStruct.lenH ) );
    tmpImgCrop.save(imgCropPath);

    //Analyze the image
    //..
    QImage *tmpImg = new QImage(imgCropPath);
    colorAnalyseResult *imgSumary = funcAnalizeImage(tmpImg);

    //Calculate wavelen
    //..
    //int tmpX;
    float tmpWave;
    float pixX[] = {
                        (float)ui->slideBlueLen->value(),
                        (float)ui->slideGreenLen->value(),
                        (float)ui->slideRedLen->value()
                   };
    linearRegresion *linReg = funcCalcLinReg( pixX );
    tmpWave = linReg->a + ( linReg->b * (float)imgSumary->maxMaxx );

    //Display scene with value calculated
    //..
    GraphicsView *wavelen = new GraphicsView(this);
    funcPutImageIntoGV(wavelen, imgCropPath);
    wavelen->setWindowTitle( "x("+ QString::number(imgSumary->maxMaxx) +
                             ") | Wavelen: " +
                             QString::number(tmpWave) );
    QString tmpColor = "#FF0000";
    if(imgSumary->maxMaxColor==2)tmpColor="#00FF00";
    if(imgSumary->maxMaxColor==3)tmpColor="#0000FF";
    wavelen->scene()->addLine(
                                    imgSumary->maxMaxx,
                                    1,
                                    imgSumary->maxMaxx,
                                    tmpImg->height(),
                                    QPen( QColor( tmpColor ) )
                             );
    wavelen->show();
    */
}







/*
void MainWindow::on_pbObtPar_2_clicked()
{
    //Select image
    //..
    auxQstring = QFileDialog::getOpenFileName(
                                                        this,
                                                        tr("Select image..."),
                                                        "./snapshots/Calib/",
                                                        "(*.ppm *.RGB888);;"
                                                     );
    if( auxQstring.isEmpty() ){
        return (void)NULL;
    }    

    //Create a copy of the image selected
    //..
    QImage origImg(auxQstring);
    qDebug() << "auxQstring: " << auxQstring;
    if( !origImg.save(_PATH_DISPLAY_IMAGE) ){
        funcShowMsg("ERROR","Creating image to display");
        return (void)NULL;
    }

    //Rotate if requires
    //..
    if( funcShowMsgYesNo("Alert","Rotate using saved rotation?") == 1 ){
        float rotAngle = getLastAngle();
        doImgRotation( rotAngle );
        globaIsRotated = true;
    }else{
        globaIsRotated = false;
    }

    //Refresh image in scene
    //..
    //Show image
    reloadImage2Display();

    //Load layout
    QLayout *layout = new QVBoxLayout();
    layout->addWidget(canvasCalib);
    layout->setEnabled(false);
    ui->tab_6->setLayout(layout);


    //qDebug() << "tres";
    //ui->tab_6->layout()->addWidget(canvasCalib);
    //qDebug() << "cuatro";
    //ui->tab_6->layout()->setEnabled(false);
    //ui->tab_6->layout()->setAlignment(Qt::AlignLeft);


    //refreshGvCalib( auxQstring );

    //It enables slides
    //..
    //ui->containerCalSave->setEnabled(true);
    ui->toolBarDraw->setEnabled(true);
    ui->toolBarDraw->setVisible(true);
    //ui->slide2AxCalThre->setEnabled(true);



}*/



void MainWindow::refreshGvCalib( QString fileName )
{
    //Add image to graphic view
    //
    QImage imgOrig(fileName);
    QImage tmpImg = imgOrig.scaled(QSize(640,480),Qt::KeepAspectRatio);
    QGraphicsScene *scene = new QGraphicsScene(0,0,tmpImg.width(),tmpImg.height());
    canvasCalib->originalW  = tmpImg.width();
    canvasCalib->originalH  = tmpImg.height();
    canvasCalib->scene()->setBackgroundBrush( QPixmap::fromImage(tmpImg) );
    canvasCalib->resize(tmpImg.width(),tmpImg.height());
    scene->setBackgroundBrush( QPixmap::fromImage(tmpImg) );
    canvasCalib->setScene( scene );
    canvasCalib->resize(tmpImg.width(),tmpImg.height());
    //canvasCalib->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    //canvasCalib->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
}

/*
void MainWindow::on_slide2AxCalPos_valueChanged(int value)
{
    value = value;
    updateCalibLine();
}*/

void MainWindow::updateCalibLine(){
    /*
    canvasCalib->scene()->clear();

    //Creates white line
    //..
    int x1,x2,y;
    y = canvasCalib->height() - ui->slide2AxCalPos->value();
    x1 = 0;
    x2 = canvasCalib->width();
    float rotAngle = -1.0*((float)ui->slide2AxCalRot->value()/5.0);
    canvasCalib->scene()->addLine(x1,y,x2,y,QPen(QColor("#FAE330")));
    ui->slide2AxCalPos->setToolTip( QString::number(ui->slide2AxCalPos->value()) );
    ui->slide2AxCalRot->setToolTip( QString::number(rotAngle) );
    ui->pbClearCalScene->setText("Clear line");
    */

}

/*
void MainWindow::on_slide2AxCalThre_valueChanged(int value)
{

    //Rotate image if requested
    //..
    if(globaIsRotated){
        float rotAngle = readAllFile( "./settings/calib/rotation.hypcam" ).trimmed().toFloat(0);
        QImage imgRot = funcRotateImage(auxQstring, rotAngle);
        imgRot.save(_PATH_DISPLAY_IMAGE);
    }
    //Apply threshold to the image
    //..
    QImage *imgThre = new QImage(auxQstring);
    funcImgThreshold( value, imgThre );
    QString tmpFilePaht = _PATH_DISPLAY_IMAGE;
    if( imgThre->save(tmpFilePaht) ){
        QtDelay(100);
        QPixmap pix(tmpFilePaht);
        pix = pix.scaledToHeight(_GRAPH_CALIB_HEIGHT);
        canvasCalib->setBackgroundBrush(pix);
        ui->slide2AxCalThre->setValue(value);
        ui->slide2AxCalThre->setToolTip(QString::number(value));
        ui->slide2AxCalThre->update();
        QtDelay(20);
    }

}
*/

void MainWindow::funcImgThreshold( int threshold, QImage *tmpImage ){
    int r, c;
    QRgb pix;
    for(r=0;r<tmpImage->height();r++){
        for(c=0;c<tmpImage->width();c++){
            pix = tmpImage->pixel(c,r);
            if( qRed(pix)<=threshold && qGreen(pix)<=threshold && qBlue(pix)<=threshold ){
                tmpImage->setPixel(QPoint(c,r),0);
            }
        }
    }
}


/*
void MainWindow::on_pbCalSaveTop_clicked()
{

    //Update image view
    //..
    QImage tmpImg( auxQstring );
    funcUpdateImgView( &tmpImg );

    //Crop image
    //..
    funcTransPix(
                    &calStruct,
                    canvasCalib->width(),
                    canvasCalib->height(),
                    tmpImg.width(),
                    tmpImg.height()
                );

    //Analize square selected
    //..
    QImage imgCrop = tmpImg.copy(
                                    calStruct.X1,
                                    calStruct.Y1,
                                    calStruct.lenW,
                                    calStruct.lenH
                                );
    colorAnalyseResult *tmpRes = funcAnalizeImage( &imgCrop );


    //Draw fluorescent RGB pixels
    //..
    int tmpX;
    //Clear scene
    canvasCalib->scene()->clear();
    //Regresh rec
    canvasCalib->scene()->addRect(calStruct.x1,calStruct.y1,calStruct.x2-calStruct.x1,calStruct.y2-calStruct.y1,QPen(QColor("#FF0000")));
    //Red
    tmpX = round(
                    (float)((calStruct.X1 + tmpRes->maxRx)*canvasCalib->width()) /
                    (float)tmpImg.width()
                );
    customLine *redLine = new customLine(QPoint(tmpX,0),QPoint(tmpX,canvasCalib->height()),QPen(Qt::red));
    canvasCalib->scene()->addItem(redLine);
    redLine->setToolTip("Red");
    redLine->parameters.name = "Vertical-Red-Right-Line";
    redLine->parameters.orientation = 2;
    redLine->parameters.lenght = canvasCalib->height();
    redLine->parameters.movible = false;
    //Green
    tmpX = round(
                    (float)((calStruct.X1 + tmpRes->maxGx)*canvasCalib->width()) /
                    (float)tmpImg.width()
                );
    customLine *GreenLine = new customLine(QPoint(tmpX,0),QPoint(tmpX,canvasCalib->height()),QPen(Qt::green));
    canvasCalib->scene()->addItem(GreenLine);
    GreenLine->setToolTip("Green");
    GreenLine->parameters.name = "Vertical-Green-Right-Line";
    GreenLine->parameters.orientation = 2;
    GreenLine->parameters.lenght = canvasCalib->height();
    GreenLine->parameters.movible = false;
    //Blue
    tmpX = round(
                    (float)((calStruct.X1 + tmpRes->maxBx)*canvasCalib->width()) /
                    (float)tmpImg.width()
                );
    customLine *BlueLine = new customLine(QPoint(tmpX,0),QPoint(tmpX,canvasCalib->height()),QPen(Qt::blue));
    canvasCalib->scene()->addItem(BlueLine);
    BlueLine->setToolTip("Blue");
    BlueLine->parameters.name = "Vertical-Blue-Right-Line";
    BlueLine->parameters.orientation = 2;
    BlueLine->parameters.lenght = canvasCalib->height();
    BlueLine->parameters.movible = false;



}
*/

/*
void MainWindow::funcUpdateImgView(QImage *tmpImg){
    //Applies rotation
    //..

    //Applies threshold
    //..
    if( ui->slide2AxCalThre->value()>0 ){
        funcImgThreshold( ui->slide2AxCalThre->value(), tmpImg );
    }
}
*/

void MainWindow::on_pbSpecLoadSnap_clicked()
{/*
    //Select image
    //..
    auxQstring = QFileDialog::getOpenFileName(
                                                        this,
                                                        tr("Select image..."),
                                                        "./snapshots/Calib/",
                                                        "(*.ppm);;"
                                                     );
    if( auxQstring.isEmpty() ){
        return (void)NULL;
    }

    //Rotate if requires
    //..
    if( funcShowMsgYesNo("Alert","Rotate using saved rotation?") == 1 ){
        float rotAngle = readAllFile( "./settings/calib/rotation.hypcam" ).trimmed().toFloat(0);
        QImage imgRot = funcRotateImage(auxQstring, rotAngle);
        imgRot.save(auxQstring);
    }


    //Create canvas
    //Display accum
    //..

    //Show image
    QPixmap pix(auxQstring);
    pix.save("./snapshots/tmpUSB.png");
    pix = pix.scaledToHeight( _GRAPH_HEIGHT );
    QGraphicsScene *scene = new QGraphicsScene(0,0,pix.width(),pix.height());
    canvasSpec->setBackgroundBrush(pix);
    canvasSpec->setScene( scene );
    canvasSpec->resize(pix.width(),pix.height());
    canvasSpec->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvasSpec->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    ui->tab_5->layout()->addWidget(canvasSpec);
    ui->tab_5->layout()->setEnabled(false);
    ui->tab_5->layout()->setAlignment(Qt::AlignLeft);
    ui->gridColors->setAlignment(Qt::AlignLeft);
    ui->gridColors->setAlignment(Qt::AlignLeft);


    ui->slideRedLen->setMaximumWidth(pix.width());
    ui->slideGreenLen->setMaximumWidth(pix.width());
    ui->slideBlueLen->setMaximumWidth(pix.width());

    pix.save("./snapshots/tmpThumb.png");

    */
}

void MainWindow::on_actionRect_triggered()
{
    /*
    selColor *selCol = new selColor(this);
    connect(selCol, SIGNAL(signalColorSelected(QString)), this, SLOT(addRect2Calib(QString)));
    selCol->setModal(true);
    selCol->exec();
    disconnect(selCol, SIGNAL(signalColorSelected(QString)), this, SLOT(addRect2Calib(QString)));
    */

    clearFreeHandPoligon();

    //Change mouse's cursor
    addRect2Calib("#F00");
    QApplication::setOverrideCursor(Qt::CrossCursor);
    ResetGraphToolBar("Rectangle");
    //Connect to calib double axis
    connect(
                canvasCalib,
                SIGNAL( signalMousePressed(QMouseEvent*) ),
                this,
                SLOT( funcBeginRect(QMouseEvent*) )
           );
    connect(
                canvasCalib,
                SIGNAL( signalMouseReleased(QMouseEvent*) ),
                this,
                SLOT( funcCalibMouseRelease(QMouseEvent*) )
           );


}

void MainWindow::ResetGraphToolBar( QString toEnable ){
    //Disable all
    //..



    if( toEnable=="Rectangle" ){

    }
}

void MainWindow::on_actionCircle_triggered()
{
    selColor *selCol = new selColor(this);
    connect(selCol, SIGNAL(signalColorSelected(QString)), this, SLOT(addCircle2Calib(QString)));
    selCol->setModal(true);
    selCol->exec();
    disconnect(selCol, SIGNAL(signalColorSelected(QString)), this, SLOT(addCircle2Calib(QString)));
}

void MainWindow::on_actionHorizontalLine_triggered()
{
    selColor *selHCol = new selColor(this);
    connect(selHCol, SIGNAL(signalColorSelected(QString)), this, SLOT(addHorLine2Calib(QString)));
    selHCol->setModal(true);
    selHCol->exec();
    disconnect(selHCol, SIGNAL(signalColorSelected(QString)), this, SLOT(addHorLine2Calib(QString)));
}

void MainWindow::on_actionVerticalLine_triggered()
{
    selColor *selVCol = new selColor(this);
    connect(selVCol, SIGNAL(signalColorSelected(QString)), this, SLOT(addVertLine2Calib(QString)));
    selVCol->setModal(true);
    selVCol->exec();
    disconnect(selVCol, SIGNAL(signalColorSelected(QString)), this, SLOT(addVertLine2Calib(QString)));
}

void MainWindow::addRect2Calib(QString colSeld){
    qDebug() << "Rect: " << colSeld;

}

void MainWindow::addCircle2Calib(QString colSeld){
    qDebug() << "Circle: " << colSeld;

}

void MainWindow::addVertLine2Calib(QString colSeld){
    int x = round( canvasCalib->width() / 2 );
    QPoint p1(x,0);
    QPoint p2(x, canvasCalib->height());
    customLine *tmpVLine = new customLine(p1, p2, QPen(QColor(colSeld)));
    tmpVLine->parameters.originalW      = canvasCalib->originalW;
    tmpVLine->parameters.originalH      = canvasCalib->originalH;
    tmpVLine->parameters.orientation    = _VERTICAL;
    tmpVLine->parentSize.setWidth(canvasCalib->width());
    tmpVLine->parentSize.setHeight(canvasCalib->height());
    globalCanvVLine = tmpVLine;
    canvasCalib->scene()->addItem( globalCanvVLine );
    canvasCalib->update();
}

void MainWindow::addHorLine2Calib(QString colSeld){
    int y = round( canvasCalib->height() / 2 );
    QPoint p1(0,y);
    QPoint p2( canvasCalib->width(), y);
    customLine *tmpHLine = new customLine(p1, p2, QPen(QColor(colSeld)));
    tmpHLine->parameters.orientation = _HORIZONTAL;
    tmpHLine->parameters.originalW  = canvasCalib->originalW;
    tmpHLine->parameters.originalH  = canvasCalib->originalH;
    tmpHLine->parentSize.setWidth(canvasCalib->width());
    tmpHLine->parentSize.setHeight(canvasCalib->height());
    globalCanvHLine = tmpHLine;
    canvasCalib->scene()->addItem( globalCanvHLine );
    canvasCalib->update();
}







void MainWindow::on_actionClear_triggered()
{
    //Clear scene
    canvasCalib->scene()->clear();

    //Clear image
    //globalEditImg = globalBackEditImg;
    //updateDisplayImage(globalBackEditImg);

    clearFreeHandPoligon();

    clearRectangle();

    //Mouse
    mouseCursorReset();

}

void MainWindow::clearFreeHandPoligon(){
    //Clear Free-hand points
    lstBorder->clear();
    lstSelPix->clear();
    lstPixSelAux->clear();

    //Disconnect signals
    disconnect(
               canvasAux,
               SIGNAL( signalMousePressed(QMouseEvent*) ),
               this,
               SLOT( funcAddPoint(QMouseEvent*) )
          );
    canvasAux->update();
}

void MainWindow::clearRectangle(){
    disconnect(
                canvasCalib,
                SIGNAL( signalMousePressed(QMouseEvent*) ),
                this,
                SLOT( funcBeginRect(QMouseEvent*) )
           );
    disconnect(
                canvasCalib,
                SIGNAL( signalMouseReleased(QMouseEvent*) ),
                this,
                SLOT( funcCalibMouseRelease(QMouseEvent*) )
           );
    canvasCalib->update();
}

void MainWindow::on_actionSelection_triggered()
{
    //Change cursor
    QApplication::restoreOverrideCursor();

    //Disconnect
    //..
    //Rectangle
    disconnect(
                canvasCalib,
                SIGNAL( signalMousePressed(QMouseEvent*) ),
                this,
                SLOT( funcBeginRect(QMouseEvent*) )
           );
    disconnect(
                canvasCalib,
                SIGNAL( signalMouseReleased(QMouseEvent*) ),
                this,
                SLOT( funcCalibMouseRelease(QMouseEvent*) )
           );

}

void MainWindow::on_actionDrawToolbar_triggered()
{
    disableAllToolBars();
    ui->toolBarDraw->setVisible(true);
}

void MainWindow::on_pbExpPixs_tabBarClicked(int index)
{
    disableAllToolBars();
    switch(index){
        case 3:
            ui->toolBarDraw->setVisible(true);
            break;
    }
}


/*
void MainWindow::on_slide2AxCalThre_sliderReleased()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    //int value = ui->slide2AxCalThre->value();
    //Rotate image if requested
    //..
    if(globaIsRotated){
        float rotAngle = readAllFile( "./settings/calib/rotation.hypcam" ).trimmed().toFloat(0);
        QImage imgRot = funcRotateImage(auxQstring, rotAngle);
        imgRot.save(_PATH_DISPLAY_IMAGE);
    }
    //Apply threshold to the image
    //..
    QImage *imgThre = new QImage(auxQstring);
    funcImgThreshold( value, imgThre );
    QString tmpFilePaht = _PATH_DISPLAY_IMAGE;
    if( imgThre->save(tmpFilePaht) ){
        QtDelay(100);
        QPixmap pix(tmpFilePaht);
        pix = pix.scaledToHeight(_GRAPH_CALIB_HEIGHT);
        canvasCalib->setBackgroundBrush(pix);
       // ui->slide2AxCalThre->setValue(value);
        //ui->slide2AxCalThre->setToolTip(QString::number(value));
        //ui->slide2AxCalThre->update();
        //QtDelay(20);
    }
    QApplication::restoreOverrideCursor();
}
*/

void MainWindow::on_actionDoubAxisDiff_triggered()
{
    genCalibXML *genCalib = new genCalibXML(this);
    genCalib->setModal(true);
    genCalib->show();

}




void MainWindow::on_actionRotateImg_triggered()
{
    DrawVerAndHorLines( canvasCalib, Qt::magenta );
    rotationFrm *doRot = new rotationFrm(this);
    doRot->setModal(false);
    connect(doRot,SIGNAL(angleChanged(float)),this,SLOT(doImgRotation(float)));
    doRot->show();
    doRot->move(QPoint(this->width(),0));
    doRot->raise();
    doRot->update();
}

void MainWindow::doImgRotation( float angle ){
    QTransform transformation;
    transformation.rotate(angle);
    QImage tmpImg(auxQstring);
    tmpImg = tmpImg.transformed(transformation);
    tmpImg.save(_PATH_DISPLAY_IMAGE);
    //reloadImage2Display();
    DrawVerAndHorLines( canvasCalib, Qt::magenta );
}

void MainWindow::DrawVerAndHorLines(GraphicsView *tmpCanvas, Qt::GlobalColor color){
    QPoint p1(0,(tmpCanvas->height()/2));
    QPoint p2(tmpCanvas->width(),(tmpCanvas->height()/2));
    customLine *hLine = new customLine(p1,p2,QPen(color));
    tmpCanvas->scene()->addItem(hLine);
    p1.setX((tmpCanvas->width()/2));
    p1.setY(0);
    p2.setX((tmpCanvas->width()/2));
    p2.setY(tmpCanvas->height());
    customLine *vLine = new customLine(p1,p2,QPen(color));
    tmpCanvas->scene()->addItem(vLine);
    tmpCanvas->update();
}

/*
void MainWindow::reloadImage2Display(){
    //Load image to display
    QPixmap pix( auxQstring );
    QRect calibArea = ui->pbExpPixs->geometry();
    int maxW, maxH;
    maxW = calibArea.width() - 3;
    maxH = calibArea.height() - 25;
    pix = pix.scaledToHeight(maxH);
    if( pix.width() > maxW )
        pix = pix.scaledToWidth(maxW);
    //It creates the scene to be loaded into Layout
    QGraphicsScene *sceneCalib = new QGraphicsScene(0,0,pix.width(),pix.height());    
    canvasCalib->setBackgroundBrush(QBrush(Qt::black));
    canvasCalib->setBackgroundBrush(pix);    
    canvasCalib->setScene( sceneCalib );
    canvasCalib->resize(pix.width(),pix.height());
    canvasCalib->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvasCalib->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvasCalib->update();
    //qDebug() << "CanvasSceneW: "<< canvasCalib->scene()->width();
    //qDebug() << "CanvasSceneH: "<< canvasCalib->scene()->height();
    //qDebug() << "CanvasW: "<< canvasCalib->width();
    //qDebug() << "CanvasH: "<< canvasCalib->height();
}*/

void MainWindow::on_actionLoadCanvas_triggered()
{

    QString lastPath = readFileParam(_PATH_LAST_IMG_OPEN);
    if( lastPath.isEmpty() )//First time using this parameter
    {
        lastPath = "./snapshots/Calib/";
    }

    //Select image
    //..
    auxQstring = QFileDialog::getOpenFileName(
                                                this,
                                                tr("Select image..."),
                                                lastPath,
                                                "(*.ppm *.RGB888 *.tif *.png *.jpg *.jpeg *.JPEG *.JPG *.bmp);;"
                                             );
    if( auxQstring.isEmpty() )
    {
        return (void)NULL;
    }
    else
    {
        //Save last file open
        saveFile(_PATH_LAST_USED_IMG_FILENAME,auxQstring);

        //Save Folder in order to Speed up File Selection
        lastPath = funcRemoveFileNameFromPath(auxQstring);
        saveFile(_PATH_LAST_IMG_OPEN,lastPath);
    }

    //Load Image Selected
    globalEditImg       = new QImage(auxQstring);
    globalBackEditImg   = new QImage(auxQstring);

    //Update Thumb and Edit Canvas
    updateDisplayImage(globalEditImg);

    //
    //Update Displayed Image Path
    //
    //saveFile(_PATH_DISPLAY_IMAGE,auxQstring);
    //qDebug() << "auxQstring: " << auxQstring;

    //Save Image to Display Path
    //mouseCursorWait();
    //globalEditImg->save(_PATH_DISPLAY_IMAGE);
    //mouseCursorReset();

}

void MainWindow::updateImageCanvasEdit(QString fileName){
    //
    //Refresh image in scene
    //
    //Load layout    
    QLayout *layout = new QVBoxLayout();
    layout->addWidget(canvasCalib);
    layout->setEnabled(false);
    ui->tab_6->setLayout(layout);

    //
    //It enables slides
    //
    ui->toolBarDraw->setEnabled(true);
    ui->toolBarDraw->setVisible(true);

    //
    //Load image to display
    //
    QPixmap pix( fileName );
    canvasCalib->originalW  = pix.width();
    canvasCalib->originalH  = pix.height();
    QRect calibArea = ui->pbExpPixs->geometry();
    int maxW, maxH;
    maxW = calibArea.width() - 3;
    maxH = calibArea.height() - 25;
    pix = pix.scaledToHeight(maxH);
    if( pix.width() > maxW )
        pix = pix.scaledToWidth(maxW);
    //It creates the scene to be loaded into Layout
    QGraphicsScene *sceneCalib = new QGraphicsScene(0,0,pix.width(),pix.height());    
    canvasCalib->setBackgroundBrush(QBrush(Qt::black));
    canvasCalib->setBackgroundBrush(pix);
    canvasCalib->setScene( sceneCalib );
    canvasCalib->resize(pix.width(),pix.height());
    canvasCalib->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvasCalib->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvasCalib->update();

}

void MainWindow::updateImageCanvasEdit(QImage* origImg)
{
    //
    //Refresh image in scene
    //
    //Load layout
    QLayout *layout = new QVBoxLayout();
    layout->addWidget(canvasCalib);
    layout->setEnabled(false);
    delete ui->tab_6->layout();
    ui->tab_6->setLayout(layout);

    //
    //It enables slides
    //
    ui->toolBarDraw->setEnabled(true);
    ui->toolBarDraw->setVisible(true);

    //
    //Load image to display
    //
    QPixmap pix = QPixmap::fromImage(*origImg);
    QRect calibArea = ui->pbExpPixs->geometry();
    int maxW, maxH;
    maxW = calibArea.width() - 3;
    maxH = calibArea.height() - 25;
    pix = pix.scaledToHeight(maxH);
    if( pix.width() > maxW )
        pix = pix.scaledToWidth(maxW);
    //It creates the scene to be loaded into Layout
    QGraphicsScene *sceneCalib = new QGraphicsScene(0,0,pix.width(),pix.height());
    canvasCalib->originalW = origImg->width();
    canvasCalib->originalH = origImg->height();
    canvasCalib->setBackgroundBrush(QBrush(Qt::black));
    canvasCalib->setBackgroundBrush(pix);
    canvasCalib->setScene( sceneCalib );
    canvasCalib->resize(pix.width(),pix.height());
    canvasCalib->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvasCalib->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvasCalib->update();

}

void MainWindow::on_actionApplyThreshold_triggered()
{
    recParamFrm *recParam = new recParamFrm(this);
    recParam->setModal(false);
    connect(recParam,SIGNAL(paramGenerated(QString)),this,SLOT(applyThreshol2Scene(QString)));
    recParam->setWindowTitle("Type the threshold [0-255]");
    recParam->show();
    recParam->raise();
    recParam->update();
}

void MainWindow::applyThreshol2Scene(QString threshold){
    QApplication::setOverrideCursor(Qt::WaitCursor);
    int value = threshold.toInt(0);
    //Apply threshold to the image
    //..
    funcImgThreshold( value, globalEditImg );
    updateDisplayImage(globalEditImg);
    //imgThre->save(_PATH_DISPLAY_IMAGE);
    //Rotate image if requestedbac
    //..
    //if(globaIsRotated){
    //    float rotAngle = getLastAngle();
    //    QImage imgRot = funcRotateImage(_PATH_DISPLAY_IMAGE, rotAngle);
    //    imgRot.save(_PATH_DISPLAY_IMAGE);
    //    qDebug() << "Rotate: " << rotAngle;
    //}
    //Update canvas
    //..
    //reloadImage2Display();

    /*
    if( imgThre->save(_PATH_DISPLAY_IMAGE) ){
        QtDelay(100);
        QPixmap pix(_PATH_DISPLAY_IMAGE);
        pix = pix.scaledToHeight(_GRAPH_CALIB_HEIGHT);
        canvasCalib->setBackgroundBrush(pix);
        ui->slide2AxCalThre->setValue(value);
        ui->slide2AxCalThre->setToolTip(QString::number(value));
        ui->slide2AxCalThre->update();
        //QtDelay(20);
    }
    */
    QApplication::restoreOverrideCursor();
}

float MainWindow::getLastAngle(){
    return readAllFile( _PATH_LAST_ROTATION ).trimmed().toFloat(0);
}


void MainWindow::mouseCursorWait(){
    QApplication::setOverrideCursor(Qt::WaitCursor);
}

void MainWindow::mouseCursorReset(){
    QApplication::restoreOverrideCursor();
}

void MainWindow::on_actionLoadSquareRectangle_triggered()
{
    //Obtaining square aperture params
    squareAperture *tmpSqAperture = (squareAperture*)malloc(sizeof(squareAperture));
    if( !funGetSquareXML( _PATH_SQUARE_APERTURE, tmpSqAperture ) ){
        funcShowMsg("ERROR","Loading _PATH_REGION_OF_INTERES");
        return (void)false;
    }

    int x, y, w, h;
    x = round(((float)tmpSqAperture->rectX / (float)tmpSqAperture->canvasW) * (float)canvasCalib->scene()->width());
    y = round(((float)tmpSqAperture->rectY / (float)tmpSqAperture->canvasH) * (float)canvasCalib->scene()->height());
    w = round(((float)tmpSqAperture->rectW / (float)tmpSqAperture->canvasW) * (float)canvasCalib->scene()->width());
    h = round(((float)tmpSqAperture->rectH / (float)tmpSqAperture->canvasH) * (float)canvasCalib->scene()->height());

    //Draw a rectangle of the square aperture
    QPoint p1(x,y);
    QPoint p2(w,h);
    customRect *tmpRect = new customRect(p1,p2,globalEditImg);
    tmpRect->setPen(QPen(Qt::red));
    tmpRect->parameters.W = canvasCalib->width();
    tmpRect->parameters.H = canvasCalib->height();
    canvasCalib->scene()->clear();
    canvasCalib->scene()->addItem(tmpRect);
    canvasCalib->update();

}

void MainWindow::on_actionLoadRegOfInteres_triggered()
{
    //Obtaining square aperture params
    squareAperture* squareDiff = (squareAperture*)malloc(sizeof(squareAperture));
    //if( !funGetSquareXML( _PATH_SQUARE_USABLE, squareDiff ) ){
    if( !funGetSquareXML( _PATH_REGION_OF_INTERES, squareDiff ) ){
        funcShowMsg("ERROR","Loading _PATH_REGION_OF_INTERES");
        return (void)false;
    }

    int x, y, w, h;
    x = round(((float)squareDiff->rectX / (float)squareDiff->canvasW) * (float)canvasCalib->scene()->width());
    y = round(((float)squareDiff->rectY / (float)squareDiff->canvasH) * (float)canvasCalib->scene()->height());
    w = round(((float)squareDiff->rectW / (float)squareDiff->canvasW) * (float)canvasCalib->scene()->width());
    h = round(((float)squareDiff->rectH / (float)squareDiff->canvasH) * (float)canvasCalib->scene()->height());

    //Draw a rectangle of the square aperture
    QPoint p1(x,y);
    QPoint p2(w,h);
    customRect *tmpRect = new customRect(p1,p2,globalEditImg);
    tmpRect->setPen(QPen(Qt::red));
    tmpRect->parameters.W = canvasCalib->width();
    tmpRect->parameters.H = canvasCalib->height();
    canvasCalib->scene()->clear();
    canvasCalib->scene()->addItem(tmpRect);
    canvasCalib->update();
}

int MainWindow::funcDrawRectangleFromXML(QString fileName)
{
    //Obtain square coordinates from XML file
    squareAperture *tmpSqAperture = (squareAperture*)malloc(sizeof(squareAperture));
    if( !funGetSquareXML( fileName, tmpSqAperture ) ){
        funcShowMsg("ERROR","Loading " + fileName);
        return -1;
    }

    //Draw a rectangle of the square aperture
    QPoint p1(tmpSqAperture->rectX,tmpSqAperture->rectY);
    QPoint p2(tmpSqAperture->rectW,tmpSqAperture->rectH);
    customRect *tmpRect = new customRect(p1,p2,globalEditImg);
    tmpRect->setPen(QPen(Qt::red));
    tmpRect->parameters.W = canvasCalib->width();
    tmpRect->parameters.H = canvasCalib->height();

    canvasCalib->scene()->addItem(tmpRect);
    canvasCalib->update();

    return 1;
}

/*
void MainWindow::on_slideShuterSpeedSmall_valueChanged(int value)
{
    QString qstrVal = QString::number(value + ui->slideShuterSpeed->value());
    ui->labelShuterSpeed->setText( "Diffraction Shuter Speed: " + qstrVal );
}*/

void MainWindow::on_actionToolPenArea_triggered()
{
    clearRectangle();
    mouseCursorCross();
    canvasAux = canvasCalib;
    connect(
                canvasCalib,
                SIGNAL( signalMousePressed(QMouseEvent*) ),
                this,
                SLOT( funcAddPoint(QMouseEvent*) )
           );
}

void MainWindow::mouseCursorHand(){
    QApplication::setOverrideCursor(Qt::PointingHandCursor);
}

void MainWindow::mouseCursorCross(){
    QApplication::setOverrideCursor(Qt::CrossCursor);
}

void MainWindow::on_actionSend_to_XYZ_triggered()
{
    //Validate that exist pixel selected
    //..
    if( lstSelPix->count()<1){
        funcShowMsg("Lack","Not pixels selected");
        return (void)false;
    }

    //Create xycolor space
    //..
    GraphicsView *xySpace = new GraphicsView(this);
    funcPutImageIntoGV(xySpace, "./imgResources/CIEManual.png");
    xySpace->setWindowTitle( "XY color space" );
    xySpace->show();

    //Transform each pixel from RGB to xy and plot it
    //..
    QImage tmpImg(_PATH_DISPLAY_IMAGE);
    int W = tmpImg.width();
    int H = tmpImg.height();
    int pixX, pixY;
    QRgb tmpPix;
    colSpaceXYZ *tmpXYZ = (colSpaceXYZ*)malloc(sizeof(colSpaceXYZ));
    int tmpOffsetX = -13;
    int tmpOffsetY = -55;
    int tmpX, tmpY;
    int i;
    qDebug() << "<lstSelPix->count(): " << lstSelPix->count();
    for( i=0; i<lstSelPix->count(); i++ ){
        //Get pixel position in real image
        pixX = (float)(lstSelPix->at(i).first * W) / (float)canvasAux->width();
        pixY = (float)(lstSelPix->at(i).second * H) / (float)canvasAux->height();
        tmpPix = tmpImg.pixel( pixX, pixY );
        funcRGB2XYZ( tmpXYZ, (float)qRed(tmpPix), (float)qGreen(tmpPix), (float)qBlue(tmpPix) );
        //funcRGB2XYZ( tmpXYZ, 255.0, 0, 0  );
        tmpX = floor( (tmpXYZ->x * 441.0) / 0.75 ) + tmpOffsetX;
        tmpY = 522 - floor( (tmpXYZ->y * 481.0) / 0.85 ) + tmpOffsetY;
        funcAddPoit2Graph( xySpace, tmpX, tmpY, 1, 1,
                           QColor(qRed(tmpPix),qGreen(tmpPix),qBlue(tmpPix)),
                           QColor(qRed(tmpPix),qGreen(tmpPix),qBlue(tmpPix))
                         );
    }

    //Save image plotted
    //..
    QPixmap pixMap = QPixmap::grabWidget(xySpace);
    pixMap.save("./Results/Miscelaneas/RGBPloted.png");

}

void MainWindow::on_actionSaveCanvas_triggered()
{
    recParamFrm *recParam = new recParamFrm(this);
    recParam->setModal(false);
    connect(recParam,SIGNAL(paramGenerated(QString)),this,SLOT(saveCalib(QString)));
    recParam->setWindowTitle("Type the filename...");
    recParam->show();
    recParam->raise();
    recParam->update();
}

void MainWindow::saveCalib(QString fileName){
    bool result = saveCanvas(canvasCalib,fileName);
    if( result ){
        funcShowMsg("Success","Canvas saved");
    }else{
        funcShowMsg("ERROR","Saving Canvas");
    }
}

bool MainWindow::saveCanvas(GraphicsView *tmpCanvas, QString fileName){
    //Save
    //..
    //Remove if exists
    QFile prevImg(fileName);
    if(prevImg.exists()){
        prevImg.remove();
    }
    prevImg.close();
    QPixmap pixMap = QPixmap::grabWidget(tmpCanvas);
    if(pixMap.save(fileName)){
        return true;
    }else{
        return false;
    }
    return true;
}

void MainWindow::on_actionExportPixelsSelected_triggered()
{
    if( lstSelPix->isEmpty() ){
        funcShowMsg("LACK","Not pixels to export");
    }else{
        int i;
        QString filePath = "./Results/lstPixels.txt";
        QFile fileLstPixels(filePath);
        if (!fileLstPixels.open(QIODevice::WriteOnly | QIODevice::Text)){
            funcShowMsg("ERROR","Creating file fileLstPixels");
        }else{
            QTextStream streamLstPixels(&fileLstPixels);
            for( i=0; i<lstSelPix->count(); i++ ){
                streamLstPixels << QString::number(lstSelPix->at(i).first) << " "<< QString::number(lstSelPix->at(i).second) << "\n";
            }
        }
        fileLstPixels.close();
        funcShowMsg("Success","List of pixels exported into: "+filePath);
    }
}

void MainWindow::on_pbLANConnect_clicked()
{
    ui->txtCommand->setText("sudo iwconfig wlan0 essid ESSID-NAME key s:PASS");
    ui->checkBlind->setChecked(true);
    funcShowMsg("Alert","Execute IWCONFIG setting BLIND mode");
}

void MainWindow::on_pbLANTestConn_clicked()
{
    ui->txtCommand->setText("iwconfig");
    ui->checkBlind->setChecked(false);
    ui->pbSendComm->click();
}

void MainWindow::on_actionGenHypercube_triggered()
{    

    //----------------------------------------
    // Validate lst of wavelengths selected
    //----------------------------------------
    QList<double> lstChoises;
    lstChoises  = getWavesChoised();
    if( lstChoises.size() <= 0 )
    {
        funcShowMsg("ERROR","Please, select wavelengths to extract");
        return (void)false;
    }

    //----------------------------------------
    // Select hypercube destination
    //----------------------------------------
    //Read Last Path
    QString lastHypercubePath;
    lastHypercubePath = readFileParam(_PATH_LAST_EXPORTED_HYPERCUBE);

    //Select Exported Hypercube
    QString fileName;
    fileName = QFileDialog::getSaveFileName(
                                                this,
                                                tr("Save Hypercube as..."),
                                                lastHypercubePath,
                                                tr("Documents (*.hypercube)")
                                            );
    if( fileName.isEmpty() )
    {
        return (void)NULL;
    }
    fileName.replace(".hypercube","");
    lastHypercubePath = fileName;
    fileName.append(".hypercube");
    saveFile(_PATH_LAST_EXPORTED_HYPERCUBE,lastHypercubePath);

    QTime timeStamp;
    timeStamp.start();
    int tmpEMNumIte;
    tmpEMNumIte = readFileParam(_PATH_SETTINGS_EM_ITERATIONS).toInt(0);
    qDebug() << "tmpEMNumIte: " << tmpEMNumIte;


    if( generatesHypcube(tmpEMNumIte, fileName) )
    {
        //Extracts hypercube
        extractsHyperCube(fileName);
        //Show time to extract files
        QString time;
        time = timeToQString( timeStamp.elapsed() );
        qDebug() << time;
        //Inform to the user
        funcShowMsg(" ", "Hypercube exported successfully\n\n"+time);

        QString exportedHypcbes(_PATH_TMP_HYPCUBES);
        funcUpdateSpectralPixels(&exportedHypcbes);
    }
    //exit(2);


}

bool MainWindow::generatesHypcube(int numIterations, QString fileName){

    mouseCursorWait();
    lstHypercubeImgs.clear();

    int i, l, aux, N;
    double *F, *fRed, *fGreen, *fBlue;
    QList<double> lstChoises;
    int hypW, hypH, hypL;
    lstDoubleAxisCalibration daCalib;    
    lstChoises  = getWavesChoised();
    funcGetCalibration(&daCalib);

    hypW        = daCalib.squareUsableW;
    hypH        = daCalib.squareUsableH;
    hypL        = lstChoises.count();
    N           = hypW * hypH * hypL;//Voxels

    F           = (double*)malloc(N*sizeof(double));    
    //calculatesF(numIterations,_RED,&daCalib);    

    ui->progBar->setVisible(true);
    ui->progBar->setValue(0);
    ui->progBar->update();

    fRed        = calculatesF(numIterations,_RED,&daCalib,0,30);
    fGreen      = calculatesF(numIterations,_GREEN,&daCalib,30,60);
    fBlue       = calculatesF(numIterations,_BLUE,&daCalib,60,90);



    //---------------------------------------------
    //Demosaicing hypercube BEFORE
    //---------------------------------------------
    if(true)
    {
        if(false)
        {
            for( i=0; i<SQUARE_BICUBIC_ITERATIONS; i++ )
            //for( i=0; i<2; i++ )
            {
                fRed    = demosaiseF2D(fRed,hypL,hypH,hypW);
                fGreen  = demosaiseF2D(fGreen,hypL,hypH,hypW);
                fBlue   = demosaiseF2D(fBlue,hypL,hypH,hypW);
            }
        }
        else
        {
            for( i=0; i<SQUARE_BICUBIC_ITERATIONS; i++ )
            //for( i=0; i<2; i++ )
            {
                fRed    = demosaiseF3D(fRed,hypL,hypH,hypW);
                fGreen  = demosaiseF3D(fGreen,hypL,hypH,hypW);
                fBlue   = demosaiseF3D(fBlue,hypL,hypH,hypW);
            }
        }
    }

    //---------------------------------------------
    //Extracting spectral measure
    //---------------------------------------------
    //Get hash to the corresponding sensitivity
    QList<double> Sr;
    QList<double> Sg;
    QList<double> Sb;
    for(l=0; l<hypL; l++)
    {        
        //qDebug() << "Hola1" << l << " of "<< hypL << " size: " << daCalib.Sr.size();
        aux = ((floor(lstChoises.at(l)) - floor(daCalib.minWavelength) )==0)?0:floor( (floor(lstChoises.at(l)) - floor(daCalib.minWavelength) ) / (double)daCalib.minSpecRes );
        Sr.append( daCalib.Sr.at(aux) );
        Sg.append( daCalib.Sg.at(aux) );
        Sb.append( daCalib.Sb.at(aux) );        
    }
    //qDebug() << "Hola13";
    int j, pixByImage;
    double min, max;
    int minPos, maxPos;
    min = 9999;
    max = -1;
    pixByImage = daCalib.squareUsableW * daCalib.squareUsableH;    
    i=0;
    //double tmpVector[3];
    for(l=0; l<hypL;l++)
    {

        funcUpdateProgBar( 90, 100, l, hypL );
        for(j=0; j<pixByImage; j++)
        {
            //F[i]    = (fRed[i]+fGreen[i]+fBlue[i]) / (Sr.at(l)+Sg.at(l)+Sb.at(l));
            //F[i]    = fRed[i]+fGreen[i]+fBlue[i];
            F[i]    = funcGetSpectralResponse(fRed[i],fGreen[i],fBlue[i],Sr.at(l),Sg.at(l),Sb.at(l));
            if(min>F[i])
            {
                min     = F[i];
                minPos  = i;
            }
            if(max<F[i])
            {
                max     = F[i];
                maxPos  = i;
            }
            i++;
        }
    }

    ui->progBar->setVisible(false);
    ui->progBar->setValue(0);
    ui->progBar->update();

    qDebug() << "min: " << min << " minPos: " << minPos << " max: " << max << " maxPos: " << maxPos;

    //printf("min(%lf,%d) max(%lf,%d)\n",min,minPos,max,maxPos);
    //fflush(stdout);


    //---------------------------------------------
    //Demosaicing hypercube AFTER
    //---------------------------------------------
    /*
    if(true)
    {
        if(false)
        {
            for( i=0; i<SQUARE_BICUBIC_ITERATIONS; i++ )
            {
                F    = demosaiseF2D(F,hypL,hypH,hypW);
            }
        }
        else
        {
            for( i=0; i<SQUARE_BICUBIC_ITERATIONS; i++ )
            {
                F    = demosaiseF3D(F,hypL,hypH,hypW);
            }
        }
    }*/

    //---------------------------------------------
    //Save hypercube
    //---------------------------------------------
    //Hypercube Format: Date,W,H,L,l1,...,lL,pix_1_l1,pix_2_l1,...pix_n_l1,pix_1_l2,pix_2_l2,...pix_n_l2,...,pix_1_L,pix_2_L,...pix_n_L
    QString hypercube;
    QDateTime dateTime = QDateTime::currentDateTime();
    hypercube.append(dateTime.toString("yyyy-MM-dd HH:mm:ss"));
    hypercube.append(","+QString::number(daCalib.squareUsableW));
    hypercube.append(","+QString::number(daCalib.squareUsableH));
    hypercube.append(","+QString::number(lstChoises.count()));
    for(l=0; l<lstChoises.count(); l++)
    {
        hypercube.append(","+QString::number(lstChoises.at(l)));
    }
    N = hypW * hypH * hypL;//Voxels
    for(i=0; i<N; i++)
    {
        hypercube.append(","+QString::number(F[i]));
    }
    fileName.replace(".hypercube","");
    saveFile(fileName+".hypercube",hypercube);

    hypercube.clear();
    Sr.clear();
    Sg.clear();
    Sb.clear();
    delete[] F;
    delete[] fRed;
    delete[] fGreen;
    delete[] fBlue;


    mouseCursorReset();





    /*

    if(false)
    {
        double min, max;
        int minPos, maxPos;
        min = 9999;
        max = -1;

        for(int n=0; n<N; n++)
        {
            if(min > F[n])
            {
                min = F[n];
                minPos = n;
            }
            if(max < F[n])
            {
                max = F[n];
                maxPos = n;
            }
           //    printf("F[%d] | %lf\n",n,F[n]);
        }
        printf("min(%lf,%d) max(%lf,%d)\n",min,minPos,max,maxPos);
        fflush(stdout);
    }
    */







    return true;

}

double MainWindow::funcGetSpectralResponse(double r,double g,double b,double rLambda,double gLambda,double bLambda)
{

    double val;
    int method;
    val         = 0.0;
    method      = 3; // 1:Max val | 2:Mejor Sensor
                     // 3:SumaDeSensores/SumaSensibilidades | 4: Suma

    if(method==1)
    {
        val = (r>g&&r>b)?r:(g>b)?g:b;
        //qDebug() <<  "Max: val: " << val;
    }
    if(method==2)
    {
        if(rLambda>=gLambda && rLambda>=bLambda)
        {
            //val = r * (1+ ((1-rLambda)*3.0));
            val = r;
        }
        else
        {
            if(gLambda>=rLambda && gLambda>=bLambda)
            {
                //val = g * (1+ ((1-gLambda)*3.0));
                val = g;
            }
            else
            {
                if(bLambda>=rLambda && bLambda>=gLambda)
                {
                    //val = b * (1+ ((1-bLambda)*3.0));
                    val = b;
                }
            }
        }
    }
    if(method==3)
    {
        val = (r+g+b) / (rLambda+gLambda+bLambda);
    }
    if(method==4)
    {
        val = (r+g+b);
    }

    return val;
}


double *MainWindow::demosaiseF2D(double *f, int L, int H, int W)
{
    //Variables
    int i, l, w, h;
    double ***aux;
    aux = (double***)malloc(L*sizeof(double**));
    for(l=0;l<L;l++)
    {
        aux[l] = (double**)malloc(H*sizeof(double*));
        for(h=0;h<H;h++)
        {
            aux[l][h] = (double*)malloc(W*sizeof(double));
        }
    }

    //Fill as 3D matrix
    i=0;
    for(l=0;l<L;l++)
    {
        for(h=0;h<H;h++)
        {
            for(w=0;w<W;w++)
            {
                aux[l][h][w] = f[i];
                i++;
            }
        }
    }

    //Demosaize
    i=0;
    for(l=0;l<L;l++)
    {
        for(h=0;h<H;h++)
        {
            for(w=0;w<W;w++)
            {
                if( h>0 && w>0 && h<H-1 && w<W-1 )
                {
                    f[i] = (    aux[l][h-1][w-1] +
                                aux[l][h+1][w-1] +
                                aux[l][h-1][w+1] +
                                aux[l][h+1][w+1]
                           ) / 4.0;
                }
                else
                {
                    //ROWS
                    if( h==0 || h==(H-1) )
                    {
                        if(h==0)//First row
                        {
                            if( w>0 && w<(W-1) )
                                f[i] = ( aux[l][h+1][w-1] + aux[l][h+1][w+1]) / 2.0;
                            else//Corners
                            {
                                if(w==0)//Up-Left
                                    f[i] = ( aux[l][h+1][w] + aux[l][h+1][w+1] + aux[l][h][w+1]) / 3.0;
                                if(w==(W-1))//Up-Right
                                    f[i] = ( aux[l][h][w-1] + aux[l][h+1][w-1] + aux[l][h+1][w]) / 3.0;
                            }
                        }
                        else
                        {
                            if(h==(H-1))//Last row
                            {
                                if( w>0 && w<(W-1) )
                                    f[i] = ( aux[l][h-1][w-1] + aux[l][h-1][w+1]) / 2.0;
                                else//Corners
                                {
                                    if(w==0)//Down-Left
                                        f[i] = ( aux[l][h-1][w] + aux[l][h-1][w+1] + aux[l][h][w+1]) / 3.0;
                                    if(w==(W-1))//Down-Right
                                        f[i] = ( aux[l][h][w-1] + aux[l][h-1][w-1] + aux[l][h-1][w]) / 3.0;
                                }
                            }
                            else
                            {
                                //COLS
                                if( w==0 || w==(W-1) )
                                {
                                    if(w==0)
                                        f[i] = ( aux[l][h-1][w+1] + aux[l][h+1][w+1]) / 2.0;
                                    else
                                        f[i] = ( aux[l][h-1][w-1] + aux[l][h+1][w-1]) / 2.0;
                                }
                            }
                        }
                    }
                }
                i++;
            }
        }
    }

    //Free memory
    for(l=0;l<L;l++)
    {
        for(h=0;h<H;h++)
        {
             delete[] aux[l][h];
        }
        delete[] aux[l];
    }
    delete[] aux;

    return f;
}


double *MainWindow::demosaiseF3D(double *f, int L, int H, int W)
{
    f = demosaiseF2D(f,L,H,W);

    //...............................................
    //Variables
    //...............................................
    int i, l, w, h;
    double ***aux;
    aux     = (double***)malloc(L*sizeof(double**));
    for(l=0;l<L;l++)
    {
        aux[l] = (double**)malloc(H*sizeof(double*));
        for(h=0;h<H;h++)
        {
            aux[l][h] = (double*)malloc(W*sizeof(double));
        }
    }

    //...............................................
    //Fill a 3D matrix
    //...............................................
    i=0;
    for(l=0;l<L;l++)
    {
        for(h=0;h<H;h++)
        {
            for(w=0;w<W;w++)
            {
                aux[l][h][w] = f[i];
                i++;
            }
        }
    }

    //...............................................
    //Demosaize 3D
    //...............................................
    trilinear tmpNode;
    tmpNode.L = L;
    tmpNode.W = W;
    tmpNode.H = H;
    i=0;
    for(tmpNode.l=0;tmpNode.l<L;tmpNode.l++)
    {
        for(tmpNode.h=0;tmpNode.h<H;tmpNode.h++)
        {
            for(tmpNode.w=0;tmpNode.w<W;tmpNode.w++)
            {
                //qDebug() << "i: " << i << "l: " << tmpNode.l << "w: " << tmpNode.w << "h: " << tmpNode.h << "W: " << tmpNode.W << "H: " << tmpNode.H;
                f[i] = calcTrilinearInterpolation(aux, &tmpNode);
                i++;                
            }
        }
    }

    //...............................................
    //Free memory
    //...............................................
    for(l=0;l<L;l++)
    {
        for(h=0;h<H;h++)
        {
             delete[] aux[l][h];
        }
        delete[] aux[l];
    }
    delete[] aux;

    //Finishes
    return f;
}


double MainWindow::calcTrilinearInterpolation(double ***cube, trilinear *node)
{

    double result = cube[node->l][node->h][node->w];
    if(
            node->l>0 && node->l<node->L-1 &&
            node->w>0 && node->w<node->W-1 &&
            node->h>0 && node->h<node->H-1
    )
    {
        result = (
                    cube[node->l-1][node->h-1][node->w-1]   +
                    cube[node->l-1][node->h-1][node->w]     +
                    cube[node->l-1][node->h-1][node->w+1]   +
                    cube[node->l-1][node->h][node->w-1]     +
                    cube[node->l-1][node->h][node->w]       +
                    cube[node->l-1][node->h][node->w+1]     +
                    cube[node->l-1][node->h+1][node->w-1]   +
                    cube[node->l-1][node->h+1][node->w]     +
                    cube[node->l-1][node->h+1][node->w+1]   +

                    cube[node->l][node->h-1][node->w-1]     +
                    cube[node->l][node->h-1][node->w]       +
                    cube[node->l][node->h-1][node->w+1]     +
                    cube[node->l][node->h][node->w-1]       +
                    cube[node->l][node->h][node->w]         +
                    cube[node->l][node->h][node->w+1]       +
                    cube[node->l][node->h+1][node->w-1]     +
                    cube[node->l][node->h+1][node->w]       +
                    cube[node->l][node->h+1][node->w+1]     +

                    cube[node->l+1][node->h-1][node->w-1]   +
                    cube[node->l+1][node->h-1][node->w]     +
                    cube[node->l+1][node->h-1][node->w+1]   +
                    cube[node->l+1][node->h][node->w-1]     +
                    cube[node->l+1][node->h][node->w]       +
                    cube[node->l+1][node->h][node->w+1]     +
                    cube[node->l+1][node->h+1][node->w-1]   +
                    cube[node->l+1][node->h+1][node->w]     +
                    cube[node->l+1][node->h+1][node->w+1]

                 ) / 27.0;
    }
    else
    {
        if(
            (node->l==0 || node->w==0 || node->h==0) &&
            ( node->l<node->L-1 && node->w<node->W-1 && node->h<node->H-1 )
        )
        {
            result = (
                        cube[node->l][node->h][node->w]         +
                        cube[node->l][node->h][node->w+1]       +
                        cube[node->l][node->h+1][node->w]       +
                        cube[node->l][node->h+1][node->w+1]     +

                        cube[node->l+1][node->h][node->w]       +
                        cube[node->l+1][node->h][node->w+1]     +
                        cube[node->l+1][node->h+1][node->w]     +
                        cube[node->l+1][node->h+1][node->w+1]

                     ) / 8.0;
        }
        else
        {
            if(
                    (node->l==node->L-1 || node->w==node->W-1 || node->h==node->H-1 ) &&
                    (node->l>0 && node->w>0 && node->h>0)
            )
            {
                result = (
                            cube[node->l][node->h][node->w]         +
                            cube[node->l][node->h][node->w-1]       +
                            cube[node->l][node->h-1][node->w]       +
                            cube[node->l][node->h-1][node->w-1]     +

                            cube[node->l-1][node->h][node->w]       +
                            cube[node->l-1][node->h][node->w-1]     +
                            cube[node->l-1][node->h-1][node->w]     +
                            cube[node->l-1][node->h-1][node->w-1]

                         ) / 8.0;
            }
        }

    }



    /*
    double result;
    if(node->l>0 && node->l<node->L-1)
    {
        //qDebug() << "l: " << node->l << " h: " << node->h << " w: " << node->w;
        result = (
                    cube[node->l-1][node->h][node->w] +
                    cube[node->l+1][node->h][node->w]
                 ) / 2.0;
    }
    else
    {
        if(node->l==0)
        {
            result = (
                        cube[node->l][node->h][node->w] +
                        cube[node->l+1][node->h][node->w]
                     ) / 2.0;
        }
        else
        {
            result = (
                        cube[node->l][node->h][node->w] +
                        cube[node->l-1][node->h][node->w]
                     ) / 2.0;
        }
    }
    */

    /*
    if(
            node->l > 0 && node->l < node->L-1 &&
            node->w > 0 && node->w < node->W-1 &&
            node->h > 0 && node->h < node->H-1
    )
    {//BOUNDED BY THE MARGIN
        if(true)
        {
            result = (  cube[node->l][node->h-1][node->w-1]     + cube[node->l][node->h-1][node->w+1]     +
                        cube[node->l][node->h+1][node->w-1]     + cube[node->l][node->h+1][node->w+1]     +
                        cube[node->l-1][node->h-1][node->w-1]   + cube[node->l-1][node->h-1][node->w+1]   +
                        cube[node->l-1][node->h+1][node->w-1]   + cube[node->l-1][node->h+1][node->w+1]   +
                        cube[node->l+1][node->h-1][node->w-1]   + cube[node->l+1][node->h-1][node->w+1]   +
                        cube[node->l+1][node->h+1][node->w-1]   + cube[node->l+1][node->h+1][node->w+1]
                     ) / 12.0;
        }
        else
        {
            result = (
                        cube[node->l-1][node->h-1][node->w-1]   + cube[node->l-1][node->h-1][node->w+1]   +
                        cube[node->l-1][node->h+1][node->w-1]   + cube[node->l-1][node->h+1][node->w+1]   +
                        cube[node->l+1][node->h-1][node->w-1]   + cube[node->l+1][node->h-1][node->w+1]   +
                        cube[node->l+1][node->h+1][node->w-1]   + cube[node->l+1][node->h+1][node->w+1]
                     ) / 8.0;
        }
    }
    else
    {
        result = cube[node->l][node->h][node->w];
    }
    */


    return result;
}


double *MainWindow::calculatesF(
        int numIterations,
        int sensor,
        lstDoubleAxisCalibration *daCalib,
        const double &percIni,
        const double &percEnd
){
    //Get original image
    //..
    int i, N, M;
    QImage img( _PATH_DISPLAY_IMAGE );
    M = img.width() * img.height();
    double tmpPerc;
    tmpPerc = percIni;

    //Creates and fills H
    // ..
    //Creates containers
    int hypW, hypH, hypL;
    QList<double> lstChoises;
    //myColPixel **Hcol;
    int **Hrow;

    lstChoises  = getWavesChoised();
    hypW        = daCalib->squareUsableW;
    hypH        = daCalib->squareUsableH;
    hypL        = lstChoises.count();
    N           = hypW * hypH * hypL;//Voxels

    //Reserves Memory for H
    //..
    myColPixel** Hcol = (myColPixel**)malloc(N*sizeof(myColPixel*));
    for(int n=0; n<N; n++)
    {
        Hcol[n] = (myColPixel*)malloc(5*sizeof(myColPixel));
    }
    Hrow        = (int**)malloc(M*sizeof(int*));
    for(int m=0; m<M; m++)
    {
        Hrow[m]     = (int*)malloc(sizeof(int));
        Hrow[m][0]  = 0;
    }

    //funcShowMsgYesNo("Hi_4_1","",this);
    //deleteHColAndHrow( Hcol, Hrow, N, M );
    //funcShowMsgYesNo("Hi_4_2","",this);
    //exit(0);



    //It creates H
    //..
    tmpPerc = percIni+((percEnd-percIni)*0.3);
    createsHColAndHrow( Hcol, Hrow, &img, daCalib, percIni, tmpPerc );
    /*
    if( validateHcolAndHrow(Hcol, Hrow, &img, daCalib) != _OK )
    {
        funcShowMsgERROR("Creating H",this);
        return static_cast<double*>(nullptr);
    }*/
    //exit(0);



    /*
    //
    // Muestra Projection
    //
    int numBoxelsInLambda, voxelI, tmpX, tmpY;
    voxelI = 0;
    numBoxelsInLambda = hypW * hypH;
    for(int l=1; l<=hypL; l++)
    {
        QImage tmpImg = *globalEditImg;
        for(int x=1; x<=hypW; x++)
        {
            for(int y=1; y<=hypH; y++)
            {
                tmpX=Hcol[voxelI][0].x;//Zero
                tmpY=Hcol[voxelI][0].y;
                tmpImg.setPixel(tmpX,tmpY,qRgb(255,0,0));
                //qDebug() << "voxelI: " << voxelI << " x: " << tmpX << " y: " << tmpY
                //         << " hypW: " << hypW << " hypH: " << hypH;

                tmpX=Hcol[voxelI][1].x;//Right
                tmpY=Hcol[voxelI][1].y;
                tmpImg.setPixel(tmpX,tmpY,qRgb(255,0,0));

                tmpX=Hcol[voxelI][2].x;//Up
                tmpY=Hcol[voxelI][2].y;
                tmpImg.setPixel(tmpX,tmpY,qRgb(255,0,0));

                tmpX=Hcol[voxelI][3].x;//Left
                tmpY=Hcol[voxelI][3].y;
                tmpImg.setPixel(tmpX,tmpY,qRgb(255,0,0));

                tmpX=Hcol[voxelI][4].x;//Down
                tmpY=Hcol[voxelI][4].y;
                tmpImg.setPixel(tmpX,tmpY,qRgb(255,0,0));

                voxelI++;
            }
        }

        displayImageFullScreen(tmpImg);
        funcShowMsgYesNo("Hola","Sigui",this);
        //exit(0);
    }
    */



    //funcShowMsgYesNo("Hi2","",this);

    //It creates image to proccess
    //..
    double *g, *gTmp, *f, *fKPlusOne;
    gTmp        = (double*)malloc(M*sizeof(double));
    fKPlusOne   = (double*)malloc(N*sizeof(double));
    //memset(fKPlusOne,'\0',(N*sizeof(double)));
    g           = serializeImageToProccess( img, sensor );//g
    f           = createsF0(Hcol, g, N);//f0
    for( i=0; i<numIterations; i++ )
    {
        createsGTmp( gTmp, g, Hrow, f, M );//(Hf)m
        improveF( fKPlusOne, Hcol, f, gTmp, N );
        memcpy(f,fKPlusOne,(N*sizeof(double)));
        //memset(fKPlusOne,'\0',(N*sizeof(double)));
        funcUpdateProgBar( tmpPerc, percEnd, i, numIterations );
        //qDebug() << "ui->progBar: " << ui->progBar->value();
    }
    //funcShowMsgYesNo("Hi3","",this);

    //Free memo
    delete[] g;
    delete[] gTmp;
    delete[] fKPlusOne;    
    deleteHColAndHrow( Hcol, Hrow, N, M );
    //funcShowMsgYesNo("Hi4","",this);

    //It finishes
    return f;
}






void MainWindow::improveF( double *fKPlusOne, myColPixel **Hcol, double *f, double *gTmp, int N )
{
    int n;
    double avgMeasure;//average measure
    double relevance;//How relevant it is respect to all voxels overlaped
                     //Error between g and g^ (g = original image)
    double numProj;//It is integer but is used double to evit many cast operations
    numProj = 1.0;//Ralf(2012) says #Projectios=5 | Takayuki (1993) says #Projectios=5
                  // I tested both and works better #Projectios=1
    for( n=0; n<N; n++ )
    {
        //fKPlusOne[n]    = 0.0;
        avgMeasure      = f[n] / numProj;
        //qDebug() << "n: " << n << " | avgMeasure: " << avgMeasure;

        relevance       = gTmp[Hcol[n][0].index] +
                          gTmp[Hcol[n][1].index] +
                          gTmp[Hcol[n][2].index] +
                          gTmp[Hcol[n][3].index] +
                          gTmp[Hcol[n][4].index];
        //qDebug() << "relevance: " << relevance;

        fKPlusOne[n]    = avgMeasure * relevance;
        //qDebug() << "fKPlusOne[" << n << "]: " << fKPlusOne[n];

    }

}

void MainWindow::createsGTmp(double *gTmp, double *g, int **Hrow, double *f, int M)
{
    int m, n;
    for( m=0; m<M; m++ )
    {
        gTmp[m] = 0.0;
        //if( Hrow[m][0] > 0 )
        if( Hrow[m][0] != 0 )
        {
            //qDebug() << "Hrow[m][0]: " << Hrow[m][0] << " m: " << m;
            for( n=1; n<=Hrow[m][0]; n++ )
            {
                gTmp[m] += f[Hrow[m][n]];
                //qDebug() << "Hrow[m][n=" << n << "]: " << Hrow[m][n];
            }
            //gTmp[m] = ( g[m] > 0 && gTmp[m] > 0 )?(g[m]/gTmp[m]):0;
            //qDebug() << "gTmp[m]: " << gTmp[m];
            gTmp[m] = ( g[m] != 0 && gTmp[m] != 0 )?(g[m]/gTmp[m]):0;
            //exit(0);
        }
    }
    //exit(0);
}


double *MainWindow::createsF0(myColPixel **Hcol, double *g, int N)
{
    double *f;
    f = (double*)malloc(N*sizeof(double));
    for( int n=0; n<N; n++ )
    {
        f[n] = g[Hcol[n][0].index] +//Zero
               g[Hcol[n][1].index] +//Right
               g[Hcol[n][2].index] +//Up
               g[Hcol[n][3].index] +//Left
               g[Hcol[n][4].index]; //Down
        //f0=H^T * g;
        //qDebug() << "f[" << n << "]: " << f[n];
    }
    return f;
}

double *MainWindow::serializeImageToProccess(QImage img, int sensor)
{
    int M, m;
    double *g;
    M = img.width() * img.height();
    g = (double*)malloc( M * sizeof(double) );

    QRgb rgb;
    m=0;
    for( int r=0; r<img.height(); r++ )
    {
        for( int c=0; c<img.width(); c++ )
        {
            if( sensor == _RED )
            {
                rgb     = img.pixel(QPoint(c,r));
                g[m]    = (double)qRed(rgb);
            }
            else
            {
                if( sensor == _RGB )
                {
                    rgb     = img.pixel(QPoint(c,r));
                    g[m]    = (double)(qRed(rgb) + qGreen(rgb) + qBlue(rgb));
                }
                else
                {
                    if( sensor == _GREEN )
                    {
                        rgb     = img.pixel(QPoint(c,r));
                        g[m]    = (double)qGreen(rgb);
                    }
                    else
                    {   //_BLUE
                        rgb     = img.pixel(QPoint(c,r));
                        g[m]    = (double)qBlue(rgb);
                    }
                }
            }
            m++;
        }
    }
    return g;
}

void MainWindow::deleteHColAndHrow(myColPixel** Hcol, int **Hrow, const int &colN, const int &rowM )
{
    int n, m;
    for(n=colN-1; n>=0; n--)
    {
        delete[] Hcol[n];
    }
    for(m=rowM-1; m>=0; m--)
    {
        delete[] Hrow[m];
    }
    delete Hcol;
    delete Hrow;
}

int MainWindow::validateHcolAndHrow(myColPixel** Hcol, int **Hrow, QImage *img, lstDoubleAxisCalibration *daCalib )
{
    //Prepares variables and constants
    //..
    int hypW, hypH, hypL, idVoxel;
    QList<double> lstChoises;
    lstChoises  = getWavesChoised();
    strDiffProj Pj;
    hypW        = daCalib->squareUsableW;
    hypH        = daCalib->squareUsableH;
    hypL        = lstChoises.count();

    //qDebug() << "hypW: " << hypW << " hypH: " << hypH << " hypL: " << hypL;

    for(int len=1; len<=hypL; len++)
    {
        for(int row=1; row<=hypH; row++)
        {
            for(int col=1; col<=hypW; col++)
            {
                Pj.x = col + daCalib->squareUsableX;
                Pj.y = row + daCalib->squareUsableY;
                calcDiffProj(&Pj,daCalib);
                img->setPixelColor(Pj.x,Pj.y,Qt::red);
                img->setPixelColor(Pj.rx,Pj.ry,Qt::red);
                img->setPixelColor(Pj.ux,Pj.uy,Qt::red);
                img->setPixelColor(Pj.lx,Pj.ly,Qt::red);
                img->setPixelColor(Pj.dx,Pj.dy,Qt::red);

                //qDebug() <<"x: " << Pj.x << " y: " << Pj.y;
            }
        }
        if(len==1)
        {
            displayImageFullScreen(*img);
        }
    }
    //displayImageFullScreen(*img);

    return _OK;
}

void MainWindow::createsHColAndHrow(
        myColPixel** Hcol,
        int **Hrow, QImage *img,
        lstDoubleAxisCalibration *daCalib,
        const double &percIni,
        const double &percEnd
){
    //Prepares variables and constants
    //..
    double tmpPerc;
    int hypW, hypH, hypL, idVoxel;
    QList<double> lstChoises;
    strDiffProj Pj;
    lstChoises  = getWavesChoised();
    hypW        = daCalib->squareUsableW;
    hypH        = daCalib->squareUsableH;
    hypL        = lstChoises.count();
    tmpPerc     = ui->progBar->value();

    QImage newImg;
    newImg = *img;

    //Fill Hcol
    //..
    idVoxel = 0;
    int shiftX, shiftY;
    for(int len=1; len<=hypL; len++)
    {
        funcUpdateProgBar( percIni, percEnd, len, hypL );

        Pj.wavelength = lstChoises.at(len-1);
        for(int row=1; row<=hypH; row++)
        {
            shiftX = static_cast<int>(daCalib->LR.vertA+(daCalib->LR.vertB*(row + daCalib->squareUsableY)));//Verticales
            for(int col=1; col<=hypW; col++)
            {
                //Ger projection
                Pj.x = col + shiftX;//Verticales
                Pj.y = row + static_cast<int>(daCalib->LR.horizA+(daCalib->LR.horizB*(Pj.x)));//Horizontales
                calcDiffProj(&Pj,daCalib);

                //Creates a new item in the c-th Hcol
                Hcol[idVoxel][0].x      = Pj.x;//Zero
                Hcol[idVoxel][0].y      = Pj.y;
                Hcol[idVoxel][0].index  = xyToIndex( Hcol[idVoxel][0].x, Hcol[idVoxel][0].y, img->width() );

                Hcol[idVoxel][1].x      = Pj.rx;//Right
                Hcol[idVoxel][1].y      = Pj.ry;
                Hcol[idVoxel][1].index  = xyToIndex( Hcol[idVoxel][1].x, Hcol[idVoxel][1].y, img->width() );

                Hcol[idVoxel][2].x      = Pj.ux;//Up
                Hcol[idVoxel][2].y      = Pj.uy;
                Hcol[idVoxel][2].index  = xyToIndex( Hcol[idVoxel][2].x, Hcol[idVoxel][2].y, img->width() );

                Hcol[idVoxel][3].x      = Pj.lx;//Left
                Hcol[idVoxel][3].y      = Pj.ly;
                Hcol[idVoxel][3].index  = xyToIndex( Hcol[idVoxel][3].x, Hcol[idVoxel][3].y, img->width() );

                Hcol[idVoxel][4].x      = Pj.dx;//Down
                Hcol[idVoxel][4].y      = Pj.dy;
                Hcol[idVoxel][4].index  = xyToIndex( Hcol[idVoxel][4].x, Hcol[idVoxel][4].y, img->width() );

                //Creates new item in Hrow
                insertItemIntoRow(Hrow,Hcol[idVoxel][0].index,idVoxel);
                insertItemIntoRow(Hrow,Hcol[idVoxel][1].index,idVoxel);
                insertItemIntoRow(Hrow,Hcol[idVoxel][2].index,idVoxel);
                insertItemIntoRow(Hrow,Hcol[idVoxel][3].index,idVoxel);
                insertItemIntoRow(Hrow,Hcol[idVoxel][4].index,idVoxel);

                idVoxel++;

                /*
                if(len==1)
                {
                    newImg.setPixelColor(Pj.x,Pj.y,Qt::red);
                    newImg.setPixelColor(Pj.rx,Pj.ry,Qt::red);
                    newImg.setPixelColor(Pj.ux,Pj.uy,Qt::red);
                    newImg.setPixelColor(Pj.lx,Pj.ly,Qt::red);
                    newImg.setPixelColor(Pj.dx,Pj.dy,Qt::red);
                }*/
            }
        }

        /*
        if(len==1)
        {
            displayImageFullScreen(newImg);
        }*/
    }
}

void MainWindow::funcUpdateProgBar( const double &percIni, const double &percEnd, const double &len, const double &hypL)
{
    double actPerc;
    actPerc = percIni + ( (percEnd-percIni) * (len/hypL) );
    ui->progBar->setValue(static_cast<int>(actPerc));
    ui->progBar->update();
}

void MainWindow::insertItemIntoRow(int **Hrow, int row, int col)
{
    int actualPos;
    actualPos = Hrow[row][0]+1;
    Hrow[row] = (int*)realloc(Hrow[row],((actualPos+1)*sizeof(int)));
    Hrow[row][actualPos] = col;
    Hrow[row][0]++;
}

QList<double> MainWindow::getWavesChoised()
{
    QList<double> wavelengths;
    QString waves;
    waves = readFileParam(_PATH_WAVE_CHOISES);
    QList<QString> choises;
    choises = waves.split(",");
    Q_FOREACH(const QString choise, choises)
    {
        if( !choise.isEmpty() && choise != " " && choise!="\n" )
        {
            wavelengths.append(choise.toDouble(0));
        }
    }
    return wavelengths;
}

void MainWindow::on_actionValidCal_triggered()
{
    //validateCalibration *frmValCal = new validateCalibration(this);
    //frmValCal->setModal(false);
    //frmValCal->show();
}

void MainWindow::on_actionValCal_triggered()
{
    selWathToCheck *tmpFrm = new selWathToCheck(globalEditImg,this);
    tmpFrm->setModal(false);
    tmpFrm->show();
}

void MainWindow::on_actionSquareUsable_triggered()
{
    //Read Calibration
    //Region of interes
    //..
    squareAperture *rectSquare = (squareAperture*)malloc(sizeof(squareAperture));
    if( fileExists(_PATH_SQUARE_USABLE) )
    {
        funGetSquareXML( _PATH_SQUARE_USABLE, rectSquare );
    }
    else
    {
        rectSquare->rectW = static_cast<int>(static_cast<double>(globalEditImg->width()) * 0.05);
        rectSquare->rectX = static_cast<int>(static_cast<double>(globalEditImg->width()) * 0.5)  - rectSquare->rectW;
        rectSquare->rectY = static_cast<int>(static_cast<double>(globalEditImg->height()) * 0.5) - rectSquare->rectW;
        rectSquare->rectW = rectSquare->rectW * 2;
        rectSquare->rectH = rectSquare->rectW;
    }

    selWathToCheck *checkCalib = new selWathToCheck(globalEditImg,this);
    checkCalib->showSqUsable(
                                rectSquare->rectX,
                                rectSquare->rectY,
                                rectSquare->rectW,
                                rectSquare->rectH,
                                Qt::magenta
                            );
}

void MainWindow::on_actionChoseWavelength_triggered()
{
    choseWaveToExtract *form = new choseWaveToExtract(this);
    form->show();
}

void MainWindow::on_actionFittFunction_triggered()
{
    //Select images
    //..
    QString originFileName;
    originFileName = QFileDialog::getOpenFileName(
                                                        this,
                                                        tr("Select image origin..."),
                                                        "./tmpImages/",
                                                        "(*.ppm *.RGB888 *.tif *.png *.jpg, *.jpeg *.gif);;"
                                                     );
    if( originFileName.isEmpty() )
    {
        return (void)NULL;
    }

    //Obtains dots
    //..
    int row, col, min, minPos, val, i;
    QRgb pixel;
    QImage img(originFileName);
    QImage tmpImg(originFileName);
    QString function;
    function = QString::number(img.height());
    i = 0;
    for( col=0; col<img.width(); col++ )
    {
        min     = 900;
        minPos  = 0;
        for( row=0; row<img.height(); row++ )
        {
            pixel   = img.pixel(col,row);
            val     = qRed(pixel)+qGreen(pixel)+qBlue(pixel);
            if( val < min )
            {
                min     = val;
                minPos  = row;
            }
            tmpImg.setPixelColor(QPoint(col,minPos),Qt::white);
        }
        function.append(","+QString::number(img.height()-minPos));
        tmpImg.setPixelColor(QPoint(col,minPos),Qt::magenta);
        i++;
    }
    //Save
    saveFile(_PATH_HALOGEN_FUNCTION,function);
    tmpImg.save(_PATH_AUX_IMG);

    funcShowMsg("Function saved successfully",_PATH_HALOGEN_FUNCTION);

}

void MainWindow::on_actionShow_hypercube_triggered()
{
    //Select images
    //..

    //Read Last Path
    QString lastHypercubePath;
    lastHypercubePath = readFileParam(_PATH_LAST_EXPORTED_HYPERCUBE);

    QString originFileName;
    originFileName = QFileDialog::getOpenFileName(
                                                        this,
                                                        tr("Select hypercube..."),
                                                        lastHypercubePath,
                                                        "(*.hypercube);;"
                                                     );
    qDebug() << originFileName;
    if( originFileName.isEmpty() )
    {
        return (void)NULL;
    }
    else
    {
        lastHypercubePath = originFileName;
        lastHypercubePath.replace(".hypercube","");
        lastHypercubePath.append(".hypercube");
        saveFile(_PATH_LAST_EXPORTED_HYPERCUBE,lastHypercubePath);
    }

    //
    // Change Mouse Status
    //
    mouseCursorWait();

    //
    // Generates hypercube
    //
    extractsHyperCube(originFileName);


    //
    // Displays Exported Hypercube
    //
    QString exportedHypcbes(_PATH_TMP_HYPCUBES);
    funcUpdateSpectralPixels(&exportedHypcbes);

    //
    // Change Mouse Status
    //
    mouseCursorReset();

    //
    // Notofies Successfull
    //
    funcShowMsg("Hypercube extracted successfully",_PATH_TMP_HYPCUBES);

}

void MainWindow::extractsHyperCube(QString originFileName)
{
    //Extracts information about the hypercube
    //..
    QString qstringHypercube;
    QList<QString> hypItems;
    QList<double> waves;
    qstringHypercube = readFileParam( originFileName );
    hypItems = qstringHypercube.split(",");
    QString dateTime;
    int W, H, L, l;
    dateTime = hypItems.at(0);  hypItems.removeAt(0);
    W = hypItems.at(0).toInt();         hypItems.removeAt(0);
    H = hypItems.at(0).toInt();         hypItems.removeAt(0);
    L = hypItems.at(0).toInt();         hypItems.removeAt(0);
    for(l=0;l<L;l++)
    {
        waves.append( hypItems.at(0).toDouble() );
        hypItems.removeAt(0);
    }

    //Generates the images into a tmpImage
    //..
    funcClearDirFolder(_PATH_TMP_HYPCUBES);
    QString tmpFileName;
    QImage tmpImg(W,H,QImage::Format_RGB32);
    int tmpVal;
    int col, row;

    //Calculate the max cal for each wavelength
    double max[L+1];
    int i;
    i=0;
    max[L] = 0;
    for( l=0; l<L; l++ )
    {
        max[l] = 0;
        for( row=0; row<H; row++ )
        {
            for( col=0; col<W; col++ )
            {
                if(hypItems.at(i).toDouble()>max[l])
                {
                    max[l] = hypItems.at(i).toDouble();
                }
                if(hypItems.at(i).toDouble()>max[L])
                {
                    max[L] = hypItems.at(i).toDouble();
                }
                i++;
            }
        }
    }

    //Normalize and export image
    i=0;
    for( l=0; l<L; l++ )
    {
        for( row=0; row<H; row++ )
        {
            for( col=0; col<W; col++ )
            {
                //tmpVal     = hypItems.at(i).toDouble();
                //qDebug() <<"Raw: " << hypItems.at(i) <<  " int: " <<  << " double: " << hypItems.at(i).toDouble();
                tmpVal     = static_cast<int>(round(hypItems.at(i).toDouble()));
                tmpImg.setPixelColor(QPoint(col,row),qRgb(tmpVal,tmpVal,tmpVal));
                i++;
            }
        }
        tmpFileName = _PATH_TMP_HYPCUBES +
                      QString::number(waves.at(l)) +
                      ".png";
        tmpImg.save(tmpFileName);
        tmpImg.fill(Qt::black);
    }

    hypItems.clear();
    qstringHypercube.clear();
}

void MainWindow::on_actionBilinear_interpolation_triggered()
{
    QString fileName;
    fileName = QFileDialog::getSaveFileName(
                                                this,
                                                tr("Save Hypercube as..."),
                                                _PATH_IMG_FILTERED,
                                                tr("Images (*.png *.jpg *.jpeg *.gif *.BMP)")
                                            );
    if( fileName.isEmpty() )
    {
        return (void)NULL;
    }

    mouseCursorWait();
    QImage tmpImgOrig(_PATH_DISPLAY_IMAGE);
    QImage imgBilInt;
    imgBilInt = bilinearInterpolationQImage(tmpImgOrig);
    fileName.replace(".png","");
    imgBilInt.save(fileName+".png");
    tmpImgOrig.save(fileName+"LastOrig.png");
    mouseCursorReset();

    funcShowMsg("Success","Bilinear interpolation applied");



}

/*
void MainWindow::on_slideSquareShuterSpeedSmall_valueChanged(int value)
{
    value = value;
    refreshSquareShootSpeed();
}

void MainWindow::on_slideSquareShuterSpeed_valueChanged(int value)
{
    value = value;
    refreshSquareShootSpeed();
}

void MainWindow::refreshSquareShootSpeed()
{
    QString qstrVal = QString::number(
                                        ui->slideSquareShuterSpeedSmall->value() +
                                        ui->slideSquareShuterSpeed->value()
                                     );
    ui->labelSquareShuterSpeed->setText( "Square Shuter Speed: " + qstrVal );
}*/



void MainWindow::on_pbCopyShutter_clicked()
{
    QString tmpFileName = "./XML/camPerfils/";
    tmpFileName.append(_PATH_LAST_SNAPPATH);
    funcGetRaspParamFromXML( raspcamSettings, tmpFileName );
    funcIniCamParam( raspcamSettings );
}

structCamSelected* MainWindow::getCameraSelected()
{
    return camSelected;
}

void MainWindow::on_actionslideHypCam_triggered()
{
    slideHypCam* frmSlide = new slideHypCam(this);
    frmSlide->setWindowModality(Qt::ApplicationModal);
    frmSlide->setWindowTitle("Slide HypCam");
    frmSlide->showMaximized();
    frmSlide->show();
}

int MainWindow::obtainFile( std::string fileToObtain, std::string fileNameDestine, QString txtBar )
{
    if( funcRaspFileExists( fileToObtain ) == 0 )
    {
        debugMsg("Remote-File does not exists");
        return 0;
    }
    else
    {
        QString tmpPath = QString::fromStdString(fileNameDestine);

        //Remove local file
        funcDeleteFile( tmpPath );

        //Get remote file
        int fileLen;
        u_int8_t* fileReceived = funcQtReceiveFile( fileToObtain, &fileLen, txtBar );
        saveBinFile_From_u_int8_T( fileNameDestine, fileReceived, fileLen );
        delete[] fileReceived;

        //Check if remote file was transmitted
        if( !fileExists( tmpPath ) )
        {
            debugMsg("Error acquiring Remote File: " + fileToObtain);
            return 0;
        }

    }

    return 1;
}


QImage MainWindow::obtainImageFile( std::string fileToObtain, QString txtBar )
{
    QImage img;
    if( funcRaspFileExists( fileToObtain ) == 0 )
    {
        debugMsg("File does not exists");
        return img;
    }
    else
    {
        int fileLen;
        u_int8_t* fileReceived = funcQtReceiveFile( fileToObtain, &fileLen, txtBar );

        ui->progBar->setVisible(true);
        ui->progBar->setValue(0);
        ui->progBar->update();
        progBarUpdateLabel("Saving image locally",0);

        if( saveBinFile_From_u_int8_T(_PATH_IMAGE_RECEIVED,fileReceived,fileLen) )
            img = QImage(_PATH_IMAGE_RECEIVED);
        progBarUpdateLabel("",0);
        ui->progBar->setVisible(false);
        ui->progBar->setValue(0);
        ui->progBar->update();

        /*
        const unsigned char* fileReceivedQt = reinterpret_cast<const unsigned char*>(&fileReceived[0]);
        std::string strTmp;
        strTmp.append(fileReceivedQt);
         */
        /*
        const char *fileReceivedQt = reinterpret_cast<char*>(fileReceived);
        QBuffer buffer;
        buffer.open(QBuffer::ReadWrite);
        buffer.write(fileReceivedQt);
        buffer.seek(0);
        qDebug() << "buffer.size(): " << buffer.size() << " of " << fileLen;
        img = QImage::fromData( buffer.buffer() );*/


        /*
        QBuffer buffer;
        buffer.setData(reinterpret_cast<const char*>(fileReceived), fileLen);
        QByteArray ba = QByteArray::fromRawData(buffer.data(),fileLen);
        QDataStream s1(ba);
        s1.setByteOrder(QDataStream::LittleEndian);
        s1 >> img;*/

        //qDebug() << "imgW: " << img.width();
        //QByteArray ba = QByteArray(reinterpret_cast<const char*>(fileReceived), fileLen);
        //QBuffer buffer(ba);
        //camRes = getCamRes();
        //img = QImage::fromData(camRes->width, camRes->height, QImage::Format_RGB888);
        //img.save(_PATH_IMAGE_RECEIVED);
        debugMsg("File received complete");

        /*
        ui->progBar->setValue(100.0);
        ui->progBar->update();
        ui->progBar->setVisible(false);
        progBarUpdateLabel("",0);*/

        return img;

    }

    return img;
}

u_int8_t* MainWindow::funcQtReceiveFile( std::string fileNameRequested, int* fileLen, QString txtBar )
{

    ui->progBar->setVisible(true);
    ui->progBar->setValue( 0.0 );
    ui->progBar->update();
    progBarUpdateLabel(txtBar,0);
    //It assumes that file exists and this was verified by command 10
    //
    //Receive the path of a file into raspCamera and
    //return the file if exists or empty arry otherwise

    //Connect to socket
    int socketID = funcRaspSocketConnect();
    if( socketID == -1 )
    {
        debugMsg("ERROR connecting to socket");
        return (u_int8_t*)NULL;
    }

    //Request file
    int n;
    strReqFileInfo *reqFileInfo = (strReqFileInfo*)malloc(sizeof(strReqFileInfo));
    memset( reqFileInfo, '\0', sizeof(strReqFileInfo) );
    reqFileInfo->idMsg          = 11;
    reqFileInfo->fileNameLen    = fileNameRequested.size();
    memcpy( &reqFileInfo->fileName[0], fileNameRequested.c_str(), fileNameRequested.size() );
    int tmpFrameLen = sizeof(strReqFileInfo);//IdMsg + lenInfo + fileLen + padding
    n = ::write( socketID, reqFileInfo, tmpFrameLen+1 );
    if (n < 0){
        debugMsg("ERROR writing to socket");
        return (u_int8_t*)NULL;
    }

    //Receive file's size
    *fileLen = funcReceiveOnePositiveInteger( socketID );
    std::cout << "fileLen: " << *fileLen << std::endl;
    fflush(stdout);

    //Prepare container
    u_int8_t* theFILE = (u_int8_t*)malloc(*fileLen);
    bzero( &theFILE[0], *fileLen );

    //Read file from system buffer
    int filePos     = 0;
    int remainder   = *fileLen;

    //Read image from buffer
    while( remainder > 0 )
    {
        n = read(socketID,(void*)&theFILE[filePos],remainder);
        remainder   -= n;
        filePos     += n;

        ui->progBar->setValue( ( ( 1.0 - ( (float)remainder/(float)*fileLen ) ) * 100.0) );
        ui->progBar->update();

        //std::cout << ( ( 1.0 - ( (float)remainder/(float)*fileLen ) ) * 100.0) << "%" << std::endl;

    }
    //std::cout << "(Last info) filePos: " << filePos << " remainder: " << remainder << " n: " << n << std::endl;
    //fflush(stdout);

    //Close connection
    ::close(socketID);

    ui->progBar->setValue(0);
    ui->progBar->setVisible(false);
    ui->progBar->update();
    progBarUpdateLabel("",0);

    //Return file
    return theFILE;


}

/*
void MainWindow::on_pbGetSlideCube_clicked()
{

    //obtainFile( "./timeLapse/duck.png", "./tmpImages/patito.png" );



    ui->pbStartVideo->setEnabled(false);


    mouseCursorWait();

    camRes = getCamRes();

    int numImages = 0;
    u_int8_t** lstImgs = funcGetSLIDESnapshot( &numImages, true );

    delete[] lstImgs;

    mouseCursorReset();

    ui->pbStartVideo->setEnabled(true);


}
*/

structCamSelected* MainWindow::funcGetCamSelected()
{
    return camSelected;
}

u_int8_t** MainWindow::funcGetSLIDESnapshot( int* numImages, bool saveFiles )
{
    int i, n;

    u_int8_t** lstImgs;

    //
    //Getting calibration
    //..
    lstDoubleAxisCalibration daCalib;
    funcGetCalibration(&daCalib);

    //
    //Save path
    //..
    //saveFile(_PATH_LAST_SNAPPATH,ui->txtSnapPath->text());

    //Save lastest settings
    if( saveRaspCamSettings( _PATH_LAST_SNAPPATH ) == false ){
        funcShowMsg("ERROR","Saving last snap-settings");
    }

    //
    //Set required image's settings
    //..
    strReqImg *reqImg       = (strReqImg*)malloc(sizeof(strReqImg));
    memset(reqImg,'\0',sizeof(strReqImg));
    reqImg->idMsg           = (unsigned char)9;
    reqImg->raspSett        = funcFillSnapshotSettings( reqImg->raspSett );

    //
    //Define slide' region
    //..
    reqImg->needCut     = false;
    reqImg->fullFrame   = false;
    reqImg->isSlide     = true;
    reqImg->imgCols     = camRes->width;//2592 | 640
    reqImg->imgRows     = camRes->height;//1944 | 480
    reqImg              = funcFillSLIDESettings(reqImg);

    //
    //Send Slide-cube Request-parameters and recibe ACK if all parameters
    //has been received are correct

    //Open socket
    int sockfd = connectSocket( camSelected );
    unsigned char bufferRead[frameBodyLen];
    qDebug() << "Socket opened";

    //Require Slide-cube size..
    n = ::write(sockfd,reqImg,sizeof(strReqImg));
    qDebug() << "Slide-cube requested";

    //Receive ACK with the camera status
    n = read(sockfd,bufferRead,frameBodyLen);
    if( bufferRead[1] == 1 ){//Beginning the image adquisition routine
        qDebug() << "Slide: Parameters received correctly";
    }else{//Do nothing becouse camera is not accessible
        qDebug() << "Slide: Parameter received with ERROR";
        u_int8_t** tmp  = (u_int8_t**)malloc(sizeof(u_int8_t*));
        tmp[0]          = (u_int8_t*)malloc(sizeof(u_int8_t));
        tmp[0][0]       = '\0';
        return tmp;
    }

    //
    //Send request to start time lapse and wait for ACK if time lapse
    //was saved correctly
    //
    n = ::write(sockfd,"beginTimelapse",14);
    if( n != 14 )
        qDebug() << "ERROR starting timelapse";
    else
        qDebug() << "Timelapse started";

    //Move motor
    arduinoMotor motor;
    motor.setAWalk(
                    reqImg->slide.degreeIni,
                    reqImg->slide.degreeEnd,
                    reqImg->slide.degreeJump,
                    reqImg->slide.speed
                  );
    //motor.doAWalk();
    motor.start();

    //Show prog bar
    int expecNumImgs;
    expecNumImgs =  ceil(
                            (float)(reqImg->slide.degreeEnd - reqImg->slide.degreeIni) /
                            (float)reqImg->slide.degreeJump
                         );
    progBarTimer( (expecNumImgs+1) * reqImg->slide.speed );
    QtDelay(10);//Forces to hide prog-bar

    //Receive ACK with the time lapse status
    memset(bufferRead,0,frameBodyLen);
    n = read(sockfd,bufferRead,frameBodyLen);
    strNumSlideImgs numImgs;
    memcpy( &numImgs, bufferRead, sizeof(strNumSlideImgs) );

    //Validate image generated    
    //expecNumImgs = ceil( (float)duration / (float)reqImg->slide.speed );
    qDebug() << "Imagery generated: " << numImgs.numImgs << " of " << expecNumImgs;




    //
    //Request to compute slide-diffractio (it is received each image in the cube)
    //
    float timeLapseRatio;
    timeLapseRatio = 100.0 * ( (float)numImgs.numImgs / (float)expecNumImgs );
    int getImagery = 1;
    if( timeLapseRatio < 90.0)
    {
        QString tmpError;
        tmpError.append("Insufficient time lapse imagery generated, timeLapseRatio: ");
        tmpError.append(QString::number(timeLapseRatio));
        tmpError.append("% of 90.0% required.\n\nDo you want to obtain only aquired imagery?");
        getImagery = funcShowMsgYesNo("ALERT", tmpError);

        if( !getImagery )
        {
            n = ::write(sockfd,"0",2);
            ::close(sockfd);
            lstImgs = (u_int8_t**)malloc(sizeof(u_int8_t*));
            delete[] lstImgs;
            *numImages = 0;
            return (u_int8_t**)NULL;
        }
    }


    if( getImagery )
    {

        QTime myTimer;



        //Start slide-cube generation process
        qDebug() << "Slide-cube generation process starting request";
        n = ::write(sockfd,"1",2);

        //Recuest all expected images
        numImgs.idMsg   = 1;
        int imgLen      = (reqImg->slide.rows1 * (reqImg->slide.cols1 +reqImg->slide.cols2) * 3);
        //expecNumImgs    = 1;
        funcClearDirFolder( _PATH_SLIDE_TMP_FOLDER );
        int nextImg;
        lstImgs         = (u_int8_t**)malloc(expecNumImgs*sizeof(u_int8_t*));
        *numImages      = 0;
        int framePos    = 0;
        int remainder   = imgLen+1;
        myTimer.start();
        int firstImageFlag = 0;
        std::string firstImageName;

        //Show Prog bar
        ui->progBar->setVisible(true);
        ui->progBar->setValue(0);
        ui->progBar->update();
        QtDelay(5);

        for( i=1; i<=expecNumImgs; i++ )
        {
            //Read next image status (Id or -1 if finishes)
            memset(bufferRead,'\0',sizeof(int)+1);
            n = read(sockfd,bufferRead,sizeof(int)+1);
            memcpy(&nextImg,bufferRead,sizeof(int));

            if( nextImg == -1 )
            {
                funcDebug("Camera does not have more images");
                break;
            }
            else
            {

                qDebug() << "Receiving image" << nextImg;

                //Frame memory allocation
                lstImgs[*numImages]  = (u_int8_t*)malloc((imgLen*sizeof(u_int8_t)));//Frame container
                bzero( lstImgs[*numImages], imgLen );
                framePos            = 0;
                remainder           = imgLen+1;

                //Read image from buffer
                while( remainder > 0 )
                {
                    n = read(sockfd,(void*)&lstImgs[*numImages][framePos],remainder);
                    remainder   -= n;
                    framePos    += n;
                    fflush(stdout);
                }
                *numImages = *numImages+1;




                if(saveFiles==true)
                {
                    //Variables
                    int numRows, numCols, r, c;
                    numRows = reqImg->slide.rows1;
                    numCols = reqImg->slide.cols1 + reqImg->slide.cols2;
                    QImage tmpImg(numCols, numRows, QImage::Format_RGB888);
                    //Slide part
                    int j = 0;
                    for( r=0; r<numRows; r++ )
                    {
                        for( c=0; c<reqImg->slide.cols1; c++ )
                        {
                            tmpImg.setPixel( c, r, qRgb( lstImgs[i-1][j], lstImgs[i-1][j+1], lstImgs[i-1][j+2] ) );
                            j += 3;
                        }
                    }
                    //Diffraction part
                    for( r=0; r<numRows; r++ )
                    {
                        for( c=reqImg->slide.cols1; c<numCols; c++ )
                        {
                            tmpImg.setPixel( c, r, qRgb( lstImgs[i-1][j], lstImgs[i-1][j+1], lstImgs[i-1][j+2] ) );
                            j += 3;
                        }
                    }
                    //Save
                    std::string tmpOutFileName(_PATH_SLIDE_TMP_FOLDER);
                    tmpOutFileName.append(QString::number(nextImg).toStdString());
                    tmpOutFileName.append(".png");//Slide Cube Part
                    tmpImg.save( tmpOutFileName.c_str() );

                    if( firstImageFlag==0 )
                    {
                        firstImageName.assign( tmpOutFileName.c_str() );
                        firstImageFlag = 1;
                    }

                }


                if(
                        ( numImgs.numImgs == *numImages ) &&
                        ( numImgs.numImgs < expecNumImgs )
                )
                {
                    qDebug() << "All possible images received";
                    break;
                }




            }



            //ui->labelProgBar->setText( QString::number(i) + " of " + QString::number(expecNumImgs) );
            ui->progBar->setValue( round( 100.0*((float)i/(float)expecNumImgs ) ) );
            ui->progBar->update();
            QtDelay(5);



        }

        ui->labelProgBar->setText("");
        ui->progBar->setValue(0);
        ui->progBar->setVisible(false);
        ui->progBar->update();
        QtDelay(5);

        qDebug() << "Slide-Cube received. Time(" << myTimer.elapsed()/1000 << "seconds)" ;


        if( firstImageFlag == 1 )
        {
            QImage tmpImg( firstImageName.c_str() );
            tmpImg.save( _PATH_DISPLAY_IMAGE );
            updateDisplayImage(_PATH_DISPLAY_IMAGE);
            //reloadImage2Display();
        }

        //
        //Save parameters into slide-imagery's folder
        //

        //Number of imagery
        QString tmpContain;
        QString tmpFileName;
        tmpContain = QString::number(*numImages);
        tmpFileName = _PATH_SLIDE_TMP_FOLDER;
        tmpFileName.append(_FILENAME_SLIDE_PARAM_NUM_IMGS);
        saveFile( tmpFileName, tmpContain );

        //Slide width
        tmpContain = QString::number( reqImg->slide.cols1 );
        tmpFileName = _PATH_SLIDE_TMP_FOLDER;
        tmpFileName.append(_FILENAME_SLIDE_PARAM_W);
        saveFile( tmpFileName, tmpContain );

        //Slide height
        tmpContain = QString::number( reqImg->slide.rows1 );
        tmpFileName = _PATH_SLIDE_TMP_FOLDER;
        tmpFileName.append(_FILENAME_SLIDE_PARAM_H);
        saveFile( tmpFileName, tmpContain );

    }

    //
    // Clear all, close all, and return
    //
    ::close(sockfd);


    return lstImgs;
}

int MainWindow::funcReceiveFrame( int sockfd, int idImg, int* frameLen, strReqImg *reqImg )
{
    //clock_t start, end;
    //start = clock();


    //--
    //It assumes that frame was not allocated
    //--

    int i, n, numMsgs;

    QTime myTimer;
    myTimer.start();
    //Get frame len
    *frameLen = funcReceiveOnePositiveInteger( sockfd );
    if( *frameLen < 0 )
    {
        qDebug() << "ERROR receiving frameLen";
        return -1;
    }
    qDebug() << "Frame len: " << *frameLen << "--" << myTimer.elapsed() << "ms";

    myTimer.start();
    //Get number of messages
    numMsgs = funcReceiveOnePositiveInteger( sockfd );
    if( numMsgs < 1 )
    {
        qDebug() << "ERROR receiving number of messages";
        return -1;
    }
    qDebug() << "Number of messages: " << numMsgs<< "--" << myTimer.elapsed() << "ms";


    myTimer.start();
    //Frame memory allocation
    u_int8_t* frame = (u_int8_t*)malloc((*frameLen*sizeof(u_int8_t))+(frameBodyLen*2));//Frame container
    bzero( frame, *frameLen );

    int framePos = 0;
    int remainder = *frameLen;

    while( framePos < *frameLen )
    {
        n = read(sockfd,(void*)&frame[framePos],remainder+1);
        //qDebug() << "ALERT: Received: " << n << " of " << remainder;
        remainder   -= n;
        framePos    += n;
    }
    qDebug() << "All copied"<< "--" << myTimer.elapsed() << "ms";

    /*
    for( i=0; i<numMsgs; i++ )
    {
        QtDelay(1);
        n = funcReceiveOneMessage( sockfd, (void*)&frame[i*frameBodyLen] );
        if( n != frameBodyLen )
        {
            qDebug() << "ALERT: Message: " << i << " Received: " << n << " of " << frameBodyLen;
            //return -1;
        }
        QtDelay(2);
    }
    //Receive last message if necessary
    int remainder = *frameLen%frameBodyLen;
    if( remainder > 0 )
    {
        n = funcReceiveOneMessage( sockfd, (void*)&frame[i*frameBodyLen] );
        printf("remainder: %d pos: %d n: %d\n",remainder,(i*frameBodyLen),n);
        fflush(stdout);
        if( n != remainder )
        {
            qDebug() << "ERROR: In remainder message: " << i << " Received: " << n << " of " << remainder;
            //return -1;
        }
        i++;
        n = funcReceiveOneMessage( sockfd, (void*)&frame[i*frameBodyLen] );
        printf("remainder2: %d pos: %d n: %d\n",remainder,(i*frameBodyLen),n);
        fflush(stdout);
        if( n != remainder )
        {
            qDebug() << "ERROR: In remainder2 message: " << i << " Received: " << n << " of " << remainder;
            //return -1;
        }

    }

    debugMsg("Antes clock");
    */





    //end = clock();
    //qDebug() << "Spend: " << (end-start)/1000 << " seconds";
    //fflush(stdout);
    //
    //Save frame as image
    //

    /*
    printf("Uno(%d,%d,%d,%d) Dos(%d,%d,%d,%d)",
                                                    reqImg->slide.x1,
                                                    reqImg->slide.y1,
                                                    reqImg->slide.rows1,
                                                    reqImg->slide.cols1,
                                                    reqImg->slide.x2,
                                                    reqImg->slide.y2,
                                                    reqImg->slide.rows2,
                                                    reqImg->slide.cols2
           );
    fflush(stdout);
    */


    //debugMsg("Antes QImage");

    int numRows, numCols, r, c;
    numRows = reqImg->slide.rows1;
    numCols = reqImg->slide.cols1 + reqImg->slide.cols2;
    //printf("Img(%d,%d)\n",numCols,numRows);
    //fflush(stdout);
    QImage tmpImg(numCols, numRows, QImage::Format_RGB888);

    //debugMsg("Antes for");

    //Slide part
    i = 0;
    for( r=0; r<numRows; r++ )
    {
        for( c=0; c<reqImg->slide.cols1; c++ )
        {
            tmpImg.setPixel( c, r, qRgb( frame[i], frame[i+1], frame[i+2] ) );
            i += 3;
        }
    }
    //debugMsg("antes diff");

    //Diffraction part
    for( r=0; r<numRows; r++ )
    {
        for( c=reqImg->slide.cols1; c<numCols; c++ )
        {
            tmpImg.setPixel( c, r, qRgb( frame[i], frame[i+1], frame[i+2] ) );
            i += 3;
        }
    }

    //debugMsg("antes save");
    //exit(0);


    //Save result
    if(0)
    {
        std::string tmpOutFileName(_PATH_SLIDE_TMP_FOLDER);
        tmpOutFileName.append(QString::number(idImg).toStdString());
        tmpOutFileName.append(".png");//Slide Cube Part
        tmpImg.save( tmpOutFileName.c_str() );
    }


    //debugMsg("Despues save");


    //
    //Save raw frame
    //
    //std::string tmpOutFileName(_PATH_SLIDE_TMP_FOLDER);
    //tmpOutFileName.append(QString::number(idImg).toStdString());
    //tmpOutFileName.append(".scp");//Slide Cube Part
    //saveBinFile_From_u_int8_T( tmpOutFileName, frame, *frameLen);


    //exit(0);
    return 1;
}

void MainWindow::funcDebug(QString msg)
{
    qDebug() << msg;
}

int MainWindow::funcRequestSubframe( int sockfd, strReqSubframe* subFrameParams )
{
    int n;

    //Validate input parameters
    if(
            (subFrameParams->posIni < 0 || subFrameParams->len < 1 ) &&
            ( subFrameParams->len != -6543 )
    )
    {
        //funcDebug("Parameter not setted");
        return -1;
    }

    //Evaluate that message was correctly sent
    //funcDebug("subFrameParams sent");
    //qDebug() << "Sending, pos: " << subFrameParams->posIni << " len: " << subFrameParams->len;

    n = ::write(sockfd,subFrameParams,sizeof(strReqSubframe)+1);
    if( n != sizeof(strReqSubframe)+1 )
    {
        //funcDebug("Write empty");
        return -1;
    }

    //funcDebug("Receiving ACK");
    n = funcReceiveACK( sockfd );

    //qDebug() << "ACK code received: " << n;

    return n;

}

int MainWindow::funcReceiveACK( int sockfd )
{

    //qDebug() << "funcReceiveACK";

    int n;
    u_int8_t buffer[3];
    n = read(sockfd,buffer,3);
    if( n < 0 )
        return -1;//Error receiving message

    //qDebug() << "n: " << n << " buffer[0]: " << buffer[0] << " buffer[1]: " << buffer[1];

    if( buffer[0] == 1 )
        return 1;
    if( buffer[0] == 0 )
        return 0;

    return -1;
}

int MainWindow::funcReceiveOneMessage( int sockfd, void* frame )
{
    //qDebug() << "funcReceiveOneMessage";

    //It assumess that frame has memory prviously allocated
    //Return the number n of bytes received

    int n;
    n = read(sockfd,frame,frameBodyLen);
    if( n < 0 )
        return 0;//Error receiving message


    return n;
}








strReqImg* MainWindow::funcFillSLIDESettings(strReqImg* reqImg)
{

    //
    //Obtain square coordinates from XML file
    //
    squareAperture *tmpSlide = (squareAperture*)malloc(sizeof(squareAperture));
    if( !funGetSquareXML( _PATH_SLIDE_APERTURE, tmpSlide ) ){
        funcShowMsg("ERROR","Loading Slide Area");
        return reqImg;
    }
    squareAperture *tmpSlideDiff = (squareAperture*)malloc(sizeof(squareAperture));
    if( !funGetSquareXML( _PATH_SLIDE_DIFFRACTION, tmpSlideDiff ) ){
        funcShowMsg("ERROR","Loading Slide Diffraction Area");
        return reqImg;
    }

    //Fit speed, duration and other parameters
    if( !funGetSlideSettingsXML( _PATH_SLIDE_SETTINGS, reqImg ) ){
        funcShowMsg("ERROR","Loading Slide Settings");
        return reqImg;
    }

    //Fit slide area
    reqImg->slide.x1        = round( ((float)tmpSlide->rectX / (float)tmpSlide->canvasW) * (float)reqImg->imgCols );
    reqImg->slide.y1        = round( ((float)tmpSlide->rectY / (float)tmpSlide->canvasH) * (float)reqImg->imgRows );
    reqImg->slide.cols1     = round( ((float)tmpSlide->rectW / (float)tmpSlide->canvasW) * (float)reqImg->imgCols );
    reqImg->slide.rows1     = round( ((float)tmpSlide->rectH / (float)tmpSlide->canvasH) * (float)reqImg->imgRows );

    //Fit diffraction area
    reqImg->slide.x2        = round( ((float)tmpSlideDiff->rectX / (float)tmpSlideDiff->canvasW) * (float)reqImg->imgCols );
    reqImg->slide.y2        = round( ((float)tmpSlideDiff->rectY / (float)tmpSlideDiff->canvasH) * (float)reqImg->imgRows );
    reqImg->slide.cols2     = round( ((float)tmpSlideDiff->rectW / (float)tmpSlideDiff->canvasW) * (float)reqImg->imgCols );
    reqImg->slide.rows2     = round( ((float)tmpSlideDiff->rectH / (float)tmpSlideDiff->canvasH) * (float)reqImg->imgRows );

    //slideSetting.speed      = 800;
    //slideSetting.duration   = 1700;//8000

    return reqImg;
}


void MainWindow::on_pbDrawSlide_triggered()
{
    if( funcShowMsgYesNo("I have to options...","Video Diffraction Area?") )
        funcDrawRectangleFromXML( _PATH_VIDEO_SLIDE_DIFFRACTION );
    else
        funcDrawRectangleFromXML( _PATH_SLIDE_DIFFRACTION );

    /*
    //
    //Obtain square coordinates from XML file
    //
    squareAperture *tmpSlide = (squareAperture*)malloc(sizeof(squareAperture));
    if( !funGetSquareXML( _PATH_SLIDE_APERTURE, tmpSlide ) ){
        funcShowMsg("ERROR","Loading Slide Area");
        return (void)NULL;
    }
    squareAperture *tmpSlideDiff = (squareAperture*)malloc(sizeof(squareAperture));
    if( !funGetSquareXML( _PATH_SLIDE_DIFFRACTION, tmpSlideDiff ) ){
        funcShowMsg("ERROR","Loading Slide Diffraction Area");
        return (void)NULL;
    }

    //
    // Fix coordinates
    //
    int minY;
    minY = (tmpSlide->rectY < tmpSlideDiff->rectY)?tmpSlide->rectY:tmpSlideDiff->rectY;
    tmpSlide->rectY     = minY;
    tmpSlideDiff->rectY = minY;

    int auxH1, auxH2, maxH;
    auxH1               = (tmpSlide->rectY + tmpSlide->rectH)-tmpSlide->rectY;
    auxH2               = (tmpSlideDiff->rectY + tmpSlideDiff->rectH)-tmpSlideDiff->rectY;
    maxH                = (auxH1 > auxH2)?auxH1:auxH2;
    tmpSlide->rectH     = maxH;
    tmpSlideDiff->rectH = maxH;

    //Save new coordinates
    saveRectangleAs( tmpSlide, _PATH_SLIDE_APERTURE );
    saveRectangleAs( tmpSlideDiff, _PATH_SLIDE_DIFFRACTION );

    //Draw into scene
    canvasCalib->scene()->clear();
    funcDrawRectangleFromXML( _PATH_SLIDE_APERTURE );
    funcDrawRectangleFromXML( _PATH_SLIDE_DIFFRACTION );*/
}

int MainWindow::saveRectangleAs( squareAperture *square, QString fileName )
{
    QString tmpContain;
    tmpContain.append( "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n" );
    tmpContain.append("<Variables>\n");
    tmpContain.append("\t<W>"+ QString::number( square->canvasW  ) +"</W>\n");
    tmpContain.append("\t<H>"+ QString::number( square->canvasH ) +"</H>\n");
    tmpContain.append("\t<x>"+ QString::number( square->rectX ) +"</x>\n");
    tmpContain.append("\t<y>"+ QString::number( square->rectY ) +"</y>\n");
    tmpContain.append("\t<w>"+ QString::number( square->rectW ) +"</w>\n");
    tmpContain.append("\t<h>"+ QString::number( square->rectH ) +"</h>\n");
    tmpContain.append("</Variables>");
    if( !saveFile( fileName, tmpContain ) )
    {
        return 1;
    }
    else
    {
        return 0;
    }
    return 0;
}

void MainWindow::on_actionSlide_settings_triggered()
{
    formSlideSettings* tmpForm = new formSlideSettings(this);
    tmpForm->setModal(true);
    tmpForm->show();
}

void MainWindow::on_pbShutdown_clicked()
{
    std::string cmdShutdown = "sudo shutdown -h now";

    if( !camSelected->isConnected){
        funcShowMsg("Alert","Camera not connected");
        return (void)NULL;
    }
    if( ui->tableLstCams->rowCount() == 0 ){
        funcShowMsg("Alert","Not camera detected");
        return (void)NULL;
    }
    ui->tableLstCams->setFocus();

    //Prepare message to send
    frameStruct *frame2send = (frameStruct*)malloc(sizeof(frameStruct));
    memset(frame2send,'\0',sizeof(frameStruct));
    frame2send->header.idMsg = (unsigned char)2;
    frame2send->header.numTotMsg = 1;
    frame2send->header.consecutive = 1;
    frame2send->header.trigeredTime = 0;
    frame2send->header.bodyLen   = cmdShutdown.length();
    bzero(frame2send->msg,cmdShutdown.length()+1);
    memcpy(
                frame2send->msg,
                cmdShutdown.c_str(),
                cmdShutdown.length()
          );

    //Prepare command message
    int sockfd, n, tmpFrameLen;
    //unsigned char bufferRead[sizeof(frameStruct)];
    struct sockaddr_in serv_addr;
    struct hostent *server;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    qDebug() << "Comm IP: " << QString((char*)camSelected->IP);
    if (sockfd < 0){
        qDebug() << "opening socket";
        return (void)NULL;
    }
    //server = gethostbyname( ui->tableLstCams->item(tmpRow,1)->text().toStdString().c_str() );
    server = gethostbyname( (char*)camSelected->IP );
    if (server == NULL) {
        qDebug() << "Not such host";
        return (void)NULL;
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
    serv_addr.sin_port = htons(camSelected->tcpPort);
    if (::connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
        qDebug() << "ERROR: connecting to socket";
        return (void)NULL;
    }


    //Request command result
    qDebug() << "idMsg: " << (int)frame2send->header.idMsg;
    qDebug() << "command: " << frame2send->msg;
    qDebug() << "tmpFrameLen: " << sizeof(frameHeader) + cmdShutdown.length();
    tmpFrameLen = sizeof(frameHeader) + cmdShutdown.length();
    n = ::write(sockfd,frame2send,tmpFrameLen+1);
    if(n<0){
        qDebug() << "ERROR: Sending command";
        return (void)NULL;
    }
    ::close(sockfd);

    ui->pbConnect->setEnabled(false);
    ui->tableLstCams->removeRow( ui->tableLstCams->currentRow() );
    ui->pbShutdown->setEnabled(false);



}

void MainWindow::on_pbSnapVid_clicked()
{

    bool ok;

    //-----------------------------------------------------
    // Record Remote Video
    //-----------------------------------------------------
    QString videoID;
    funcMainCall_RecordVideo(&videoID,true);

    //-----------------------------------------------------
    // Obtain Remote Video
    //-----------------------------------------------------
    if( obtainFile( _PATH_VIDEO_REMOTE_H264, _PATH_VIDEO_RECEIVED_H264, "Transmitting Remote Video" ) )
    {
        QString rmRemVidCommand;
        rmRemVidCommand.append("rm ");
        rmRemVidCommand.append(_PATH_VIDEO_REMOTE_H264);
        funcRemoteTerminalCommand( rmRemVidCommand.toStdString(), camSelected, 0, false, &ok );
    }
    else
    {
        return (void)false;
    }

    //-----------------------------------------------------
    // Extract Frames from Video
    //-----------------------------------------------------

    //Clear local folder
    QString tmpFramesPath;
    tmpFramesPath.append(_PATH_VIDEO_FRAMES);
    tmpFramesPath.append("tmp/");
    funcClearDirFolder( tmpFramesPath );

    //Extract Frames from Local Video
    QString locFrameExtrComm;
    locFrameExtrComm.append("ffmpeg -framerate ");
    locFrameExtrComm.append(QString::number(_VIDEO_FRAME_RATE));
    locFrameExtrComm.append(" -r ");
    locFrameExtrComm.append(QString::number(_VIDEO_FRAME_RATE));
    locFrameExtrComm.append(" -i ");
    locFrameExtrComm.append(_PATH_VIDEO_RECEIVED_H264);
    locFrameExtrComm.append(" ");
    locFrameExtrComm.append(tmpFramesPath);
    locFrameExtrComm.append("%07d");
    locFrameExtrComm.append(_FRAME_EXTENSION);
    qDebug() << locFrameExtrComm;
    progBarUpdateLabel("Extracting Frames from Video",0);
    if( !funcExecuteCommand(locFrameExtrComm) )
    {
        funcShowMsg("ERROR","Extracting Frames from Video");
    }
    progBarUpdateLabel("",0);

    //-----------------------------------------------------
    // Update mainImage from Frames
    //-----------------------------------------------------
    funcUpdateImageFromFolder(tmpFramesPath);

    funcResetStatusBar();

}

int MainWindow::funcUpdateImageFromFolder(const QString &folder)
{
    //-------------------------------------------------
    //Get List of Files in Directory
    //-------------------------------------------------
    QList<QFileInfo> lstImages = funcListFilesInDir( folder );
    if( lstImages.size() <= 0 )
        return _ERROR;

    //-------------------------------------------------
    //Get a Image
    //-------------------------------------------------
    int pos;
    pos = round( (float)lstImages.size() * 0.5 );
    QImage tmpImg( lstImages.at(pos).absoluteFilePath().trimmed() );

    //-------------------------------------------------
    //Update Image Displayed
    //-------------------------------------------------
    updateDisplayImage( &tmpImg );

    return _OK;
}


class MyVideoSurface : public QAbstractVideoSurface
{
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
            QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle) const
    {
        Q_UNUSED(handleType);

        // Return the formats you will support
        return QList<QVideoFrame::PixelFormat>() << QVideoFrame::Format_RGB565;
    }



    bool present(const QVideoFrame &frame)
    {
        Q_UNUSED(frame);
        // Handle the frame and do your processing

        return true;
    }
};

int MainWindow::funcVideoToFrames(QString videoSource)//_PATH_VIDEO_FRAMES, _PATH_VIDEO_RECEIVED_MP4
{

    ui->progBar->setValue(0);
    ui->progBar->setVisible(true);
    ui->progBar->update();

    numFrames = 0;
    //lstFrames.clear();
    funcClearDirFolder( _PATH_VIDEO_FRAMES );

    QMediaPlayer *player = new QMediaPlayer();
    QVideoProbe *probe = new QVideoProbe;
    MyVideoSurface* myVideoSurface = new MyVideoSurface;

    connect(probe, SIGNAL(videoFrameProbed(QVideoFrame)), this, SLOT(processFrame(QVideoFrame)));

    probe->setSource(player); // Returns true, hopefully.

    player->setVideoOutput(myVideoSurface);
    player->setMedia( QUrl::fromUserInput(videoSource, qApp->applicationDirPath()) );

    player->setPlaybackRate(0.1);

    player->play(); // Start receiving frames as they get presented to myVideoSurface

    //while(player->state() != QMediaPlayer::EndOfMedia ){QtDelay(20);}
    //
    // STATUS BAR
    //
    progBarUpdateLabel(" .mp4 to frames",0);

    connect(player, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)), this, SLOT(processEndOfPlayer(QMediaPlayer::MediaStatus)));


    return 1;
}

void MainWindow::processEndOfPlayer(QMediaPlayer::MediaStatus status)
{
    if( status == QMediaPlayer::EndOfMedia )
    {
        funcHideBarAndLabel();

        //
        //Show 25th frame (stabilized frame)
        int numFiles = funcAccountFilesInDir( _PATH_VIDEO_FRAMES );
        if( numFiles > 0 )
        {
            int idFrameToShow = ( numFiles > 25 )?25:1;
            QString imgFilename = _PATH_VIDEO_FRAMES + QString::number(idFrameToShow) + _FRAME_EXTENSION;
            QImage tmpImg(imgFilename);
            tmpImg.save( _PATH_DISPLAY_IMAGE );
            updateDisplayImage(&tmpImg);
        }
    }
}


void MainWindow::funcHideBarAndLabel()
{
    funcLabelProgBarHide();
    ui->progBar->setValue(0);
    ui->progBar->setVisible(false);
    ui->progBar->update();
}

void MainWindow::processFrame(QVideoFrame actualFrame)
{
    QImage img;
    QVideoFrame frame(actualFrame);  // make a copy we can call map (non-const) on
    frame.map(QAbstractVideoBuffer::ReadOnly);
    QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat(frame.pixelFormat());
    // BUT the frame.pixelFormat() is QVideoFrame::Format_Jpeg, and this is
    // mapped to QImage::Format_Invalid by
    // QVideoFrame::imageFormatFromPixelFormat
    if (imageFormat != QImage::Format_Invalid) {
        img = QImage(frame.bits(),
                     frame.width(),
                     frame.height(),
                     // frame.bytesPerLine(),
                     imageFormat);
    } else {
        // e.g. JPEG
        int nbytes = frame.mappedBytes();
        img = QImage::fromData(frame.bits(), nbytes);
    }

    frame.unmap();
    numFrames++;

    if( img.save( _PATH_VIDEO_FRAMES + QString::number(numFrames) + _FRAME_EXTENSION ) )
    {
        //qDebug() << "numFrames saved: " << numFrames;
        //int videoDuration = ui->spinBoxVideoDuration->value();//seconds
        //qint64 expectedFrames = round(videoDuration * 10 * 1.25);
        //ui->progBar->setValue( round( ((float)numFrames/(float)expectedFrames) * 100.0 ) );
        ui->progBar->update();
    }
    else
    {
        qDebug() << "Error saving frame " << numFrames;
    }
}

int MainWindow::rectangleInPixelsFromSquareXML( QString fileName, squareAperture *rectangle )
{
    //Get Camera Resolution
    int camMP = (ui->radioRes5Mp->isChecked())?5:8;
    camRes = getCamRes(camMP);

    //Get original rectangle properties
    if( !funGetSquareXML( fileName, rectangle ) )
    {
        funcShowMsg("ERROR","Loading in pixels: "+fileName);
        return 0;
    }

    //Translate coordinates into pixels    
    rectangle->rectX    = round( ((float)rectangle->rectX / (float)rectangle->canvasW) * (float)camRes->width );
    rectangle->rectW    = round( ((float)rectangle->rectW / (float)rectangle->canvasW) * (float)camRes->width );
    rectangle->rectY    = round( ((float)rectangle->rectY / (float)rectangle->canvasH) * (float)camRes->height );
    rectangle->rectH    = round( ((float)rectangle->rectH / (float)rectangle->canvasH) * (float)camRes->height );

    rectangle->canvasW  = camRes->width;
    rectangle->canvasH  = camRes->height;

    return 1;
}

int MainWindow::rectangleInPixelsFromSquareXML( QString fileName, QString respectTo, squareAperture *rectangle )
{
    squareAperture *rectangleRespecTo = (squareAperture*)malloc(sizeof(squareAperture));

    //Get original rectangle properties
    if( !funGetSquareXML( fileName, rectangle ) )
    {
        funcShowMsg("ERROR","Loading in pixels: "+fileName);
        return 0;
    }

    //Get reference rectangle properties
    if( !rectangleInPixelsFromSquareXML( respectTo, rectangleRespecTo ) )
    {
        funcShowMsg("ERROR","Loading in pixels: "+respectTo);
        return 0;
    }

    //funcPrintRectangle("Area de interes",rectangleRespecTo);

    //Translate coordinates into pixels
    rectangle->rectX    = round( ((float)rectangle->rectX / (float)rectangle->canvasW) * (float)rectangleRespecTo->rectW );
    rectangle->rectW    = round( ((float)rectangle->rectW / (float)rectangle->canvasW) * (float)rectangleRespecTo->rectW );
    rectangle->rectY    = round( ((float)rectangle->rectY / (float)rectangle->canvasH) * (float)rectangleRespecTo->rectH );
    rectangle->rectH    = round( ((float)rectangle->rectH / (float)rectangle->canvasH) * (float)rectangleRespecTo->rectH );

    rectangle->canvasW  = rectangleRespecTo->rectW;
    rectangle->canvasH  = rectangleRespecTo->rectH;

    return 1;

}

int MainWindow::createSubimageRemotelly(bool squareArea )
{
    int status = 0;

    //
    // Get cam resolution
    //
    int camMP = (ui->radioRes5Mp->isChecked())?5:8;
    camRes = getCamRes(camMP);

    //
    //Getting calibration
    //..
    lstDoubleAxisCalibration daCalib;
    funcGetCalibration(&daCalib);

    //
    //Generates Camera Command
    //..
    std::string tmp;
    structSubimage* subimage    = (structSubimage*)malloc(sizeof(structSubimage));
    memset(subimage,'\0',sizeof(structSubimage));
    subimage->idMsg             = (unsigned char)5;
    if( squareArea == true )
    {
        subimage->frame.canvasW     = daCalib.W;
        subimage->frame.canvasH     = daCalib.H;
        subimage->frame.rectX       = round( daCalib.squareX * (double)camRes->width );
        subimage->frame.rectY       = round( daCalib.squareY * (double)camRes->height );
        subimage->frame.rectW       = round( daCalib.squareW * (double)camRes->width );
        subimage->frame.rectH       = round( daCalib.squareH * (double)camRes->height );
    }
    else
    {
        subimage->frame.canvasW     = daCalib.W;
        subimage->frame.canvasH     = daCalib.H;
        subimage->frame.rectX       = round( daCalib.bigX * (double)camRes->width );
        subimage->frame.rectY       = round( daCalib.bigY * (double)camRes->height );
        subimage->frame.rectW       = round( daCalib.bigW * (double)camRes->width );
        subimage->frame.rectH       = round( daCalib.bigH * (double)camRes->height );
    }

    tmp.assign(_PATH_REMOTE_SNAPSHOT);
    memcpy( subimage->fileName, tmp.c_str(), tmp.length() );

    //
    // Send Command and Wait for ACK
    //

    //Open socket
    int n;
    int sockfd = connectSocket( camSelected );
    unsigned char bufferRead[frameBodyLen];
    qDebug() << "Socket opened";
    n = ::write(sockfd,subimage,sizeof(structSubimage));
    qDebug() << "Img request sent";

    //Receive ACK with the camera status
    memset(bufferRead,'\0',3);
    n = read(sockfd,bufferRead,2);

    if( bufferRead[1] == 1 )
    {
        qDebug() << "Image cropped remotelly";
        status = 1;
    }
    else
    {
        qDebug() << "ERROR cropping image";
        status = 0;
    }
    n=n;
    ::close(sockfd);
    delete[] subimage;

    return status;
}

void MainWindow::funcMediaAcquisitionTime(const int &delaySeconds, const QString &Message, const QColor &Color)
{
    if( delaySeconds > 0 )
    {
        formTimerTxt* timerTxt = new formTimerTxt(this,Message,delaySeconds,Color);
        timerTxt->setModal(true);
        timerTxt->show();
        QtDelay(200);
        timerTxt->startMyTimer(delaySeconds);
    }
}

int MainWindow::takeRemoteSnapshot( QString fileDestiny, bool squareArea )
{
    if( camSelected->isConnected == false )
    {
        funcShowMsgERROR("Camera Offline",this);
        return _ERROR;
    }

    //
    //Time Before Take Media
    //
    if(!squareArea)
    {
        funcMediaAcquisitionTime(ui->spinBoxDelayTime->value(),"Delaying...",QColor("#333333"));//Qt::black
    }

    //
    // Get cam resolution
    //
    int camMP = (ui->radioRes5Mp->isChecked())?5:8;
    camRes = getCamRes(camMP);

    //
    //Getting calibration
    //..
    //lstDoubleAxisCalibration daCalib;
    //funcGetCalibration(&daCalib);

    //Save lastest settings
    if( saveRaspCamSettings( _PATH_LAST_SNAPPATH ) == false ){
        funcShowMsg("ERROR","Saving last snap-settings");
        return _ERROR;
    }


    //
    //Generates Camera Command
    //..
    structRaspistillCommand* structRaspiCommand = (structRaspistillCommand*)malloc(sizeof(structRaspistillCommand));
    strReqImg *reqImg                           = (strReqImg*)malloc(sizeof(strReqImg));
    memset(reqImg,'\0',sizeof(strReqImg));
    memset(structRaspiCommand,'\0',sizeof(structRaspistillCommand));
    structRaspiCommand->idMsg                   = (unsigned char)4;
    reqImg->squApert                            = squareArea;
    reqImg->raspSett                            = funcFillSnapshotSettings( reqImg->raspSett );
    reqImg->imgCols                             = camRes->width;//2592 | 640
    reqImg->imgRows                             = camRes->height;//1944 | 480
    reqImg->raspSett.Flipped                    = (ui->cbFlipped->isChecked())?1:0;
    reqImg->raspSett.horizontalFlipped          = (ui->cbFlippedHorizontal->isChecked())?1:0;

    //--------------------------------------
    //Create Command
    //--------------------------------------
    QString tmpCommand;
    tmpCommand = genCommand(reqImg, fileDestiny.toStdString())->c_str();

    //--------------------------------------
    //Take Remote Photo
    //--------------------------------------
    bool executedCommand;
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,0,false,&executedCommand);    
    if( !executedCommand )
    {
        funcShowMsgERROR_Timeout("Applying Remote Snapshot Command",this);
        return _ERROR;
    }
    else
    {
        funcMediaAcquisitionTime(ui->spinBoxTrigeringTime->value(),"Stabilizing...",QColor("#FF6600"));//Qt::black
    }

    /*
    progBarUpdateLabel("Stabilizing Remote Camera...",0);
    progBarTimer((ui->slideTriggerTime->value()+1)*1000);
    progBarUpdateLabel("",0);*/

    //Get Remote File
    //obtainFile( _PATH_REMOTE_SNAPSHOT, _PATH_IMAGE_RECEIVED );

    /*
    //
    //Generates Camera Command
    //..
    std::string tmp;
    structRaspistillCommand* structRaspiCommand = (structRaspistillCommand*)malloc(sizeof(structRaspistillCommand));
    strReqImg *reqImg                           = (strReqImg*)malloc(sizeof(strReqImg));
    memset(reqImg,'\0',sizeof(strReqImg));
    memset(structRaspiCommand,'\0',sizeof(structRaspistillCommand));
    structRaspiCommand->idMsg                   = (unsigned char)4;
    reqImg->squApert                            = squareArea;
    reqImg->raspSett                            = funcFillSnapshotSettings( reqImg->raspSett );
    reqImg->imgCols                             = camRes->width;//2592 | 640
    reqImg->imgRows                             = camRes->height;//1944 | 480

    tmp.assign(_PATH_REMOTE_SNAPSHOT);
    memcpy( structRaspiCommand->fileName, tmp.c_str(), tmp.length() );
    tmp = genCommand(reqImg, _PATH_REMOTE_SNAPSHOT)->c_str();
    memcpy( structRaspiCommand->raspiCommand, tmp.c_str(), tmp.length() );
    qDebug() << structRaspiCommand->raspiCommand;    



    //
    // Send Command and Wait for ACK
    //

    //Open socket
    int n;
    int sockfd = connectSocket( camSelected );
    unsigned char bufferRead[frameBodyLen];
    qDebug() << "Socket opened";
    n = ::write(sockfd,structRaspiCommand,sizeof(structRaspistillCommand));
    qDebug() << "Img request sent";

    //Show progbar
    progBarUpdateLabel("Stabilizing camera",0);
    progBarTimer( (ui->slideTriggerTime->value() + 1) * 1000 );
    ui->progBar->setValue(0);
    ui->progBar->setVisible(true);
    ui->progBar->update();
    progBarUpdateLabel("Transferring image",0);


    //Receive ACK with the camera status
    memset(bufferRead,'\0',3);
    n = read(sockfd,bufferRead,2);

    if( bufferRead[1] == 1 )
    {
        qDebug() << "Image taked remotelly";
        status = 1;
    }
    else
    {
        qDebug() << "ERROR taking image";
        status = 0;
    }
    n=n;

    ::close(sockfd);
    delete[] structRaspiCommand;
    delete[] reqImg;
    */

    return _OK;
}

void MainWindow::on_pbSnapshotSquare_clicked()
{

    if( camSelected->isConnected == false )
    {
        funcShowMsgERROR("Camera Offline",this);
        return (void)false;
    }

    functionTakeComposedSquarePicture();

    /*
    mouseCursorWait();

    if( !takeRemoteSnapshot(_PATH_REMOTE_SNAPSHOT,false) )
    {
        qDebug() << "ERROR: Taking Diffration Area";
        return (void)NULL;
    }
    else
    {
        QImage diffImage = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
        if( diffImage.isNull() )
        {
            qDebug() << "ERROR: Obtaining Diffration Area";
            return (void)NULL;
        }
        else
        {
            if( !takeRemoteSnapshot(_PATH_REMOTE_SNAPSHOT,true) )
            {
                qDebug() << "ERROR: Taking Aperture Area";
                return (void)NULL;
            }
            else
            {
                QImage apertureImage = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
                if( apertureImage.isNull() )
                {
                    qDebug() << "ERROR: Obtaining Aperture Area";
                    return (void)NULL;
                }
                else
                {
                    //-------------------------------------------
                    //Merge image
                    //-------------------------------------------

                    squareAperture *aperture = (squareAperture*)malloc(sizeof(squareAperture));

                    //
                    //Crop original image to release the usable area
                    //
                    //Get usable area coordinates
                    memset(aperture,'\0',sizeof(squareAperture));
                    if( !rectangleInPixelsFromSquareXML( _PATH_REGION_OF_INTERES, aperture ) )
                    {
                        funcShowMsg("ERROR","Loading Usable Area in Pixels: _PATH_SQUARE_USABLE");
                        return (void)false;
                    }
                    diffImage       = diffImage.copy(QRect( aperture->rectX, aperture->rectY, aperture->rectW, aperture->rectH ));
                    apertureImage   = apertureImage.copy(QRect( aperture->rectX, aperture->rectY, aperture->rectW, aperture->rectH ));

                    //
                    //Get square aperture coordinates
                    //
                    memset(aperture,'\0',sizeof(squareAperture));
                    if( !rectangleInPixelsFromSquareXML( _PATH_SQUARE_APERTURE, _PATH_REGION_OF_INTERES, aperture ) )
                    {
                        funcShowMsg("ERROR","Loading Rectangle in Pixels: _PATH_SQUARE_APERTURE");
                        return (void)false;
                    }
                    funcPrintRectangle( "Square Aperture", aperture );

                    //
                    //Copy square aperture into diffraction image
                    //
                    for( int y=aperture->rectY; y<=(aperture->rectY+aperture->rectH); y++ )
                    {
                        for( int x=aperture->rectX; x<=(aperture->rectX+aperture->rectW); x++ )
                        {
                            diffImage.setPixel( x, y, apertureImage.pixel( x, y ) );
                        }
                    }



                    //
                    //Save cropped image
                    //
                    diffImage.save(_PATH_DISPLAY_IMAGE);
                    updateDisplayImage(&diffImage);
                }
            }
        }
    }

    funcResetStatusBar();
    mouseCursorReset();
    */
}

void MainWindow::on_pbSaveImage_clicked()
{
    //
    //Read the filename
    //
    QString fileName;
    QString lastPath = readFileParam(_PATH_LAST_IMG_SAVED);
    if( lastPath.isEmpty() )//First time using this parameter
    {
        lastPath = "./snapshots/";
    }
    fileName = QFileDialog::getSaveFileName(
                                                this,
                                                tr("Save Snapshot as..."),
                                                lastPath,
                                                tr("Documents (*.png)")
                                            );
    if( fileName.isEmpty() )
    {
        qDebug() << "Filename not typed";
        return (void)false;
    }
    else
    {
        lastPath = funcRemoveFileNameFromPath(fileName);
        saveFile(_PATH_LAST_IMG_SAVED,lastPath);
    }

    //
    //Validate filename
    //
    fileName = funcRemoveImageExtension(fileName);

    //
    //Save image
    //
    mouseCursorWait();
    globalEditImg->save(fileName);
    mouseCursorReset();

}

void MainWindow::on_pbOneShotSnapshot_clicked()
{

    if( camSelected->isConnected == false )
    {
        funcShowMsgERROR("Camera Offline",this);
        return (void)false;
    }

    mouseCursorWait();    

    if( takeRemoteSnapshot(_PATH_REMOTE_SNAPSHOT,false) == _ERROR )
    {
        qDebug() << "ERROR: Taking Diffration Area";
        funcResetStatusBar();
        mouseCursorReset();
        return (void)NULL;
    }
    else
    {
        QImage diffImage = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
        if( diffImage.isNull() )
        {
            qDebug() << "ERROR: Obtaining Diffration Area";
            funcResetStatusBar();
            mouseCursorReset();
            return (void)NULL;
        }
        else
        {
            funcResetStatusBar();
            //
            //Crop original image to release the usable area
            //
            //Get usable area coordinates
            squareAperture *aperture = (squareAperture*)malloc(sizeof(squareAperture));
            memset(aperture,'\0',sizeof(squareAperture));
            if( !rectangleInPixelsFromSquareXML( _PATH_REGION_OF_INTERES, aperture ) )
            {
                funcShowMsg("ERROR","Loading Usable Area in Pixels: _PATH_REGION_OF_INTERES");
                return (void)false;
            }
            //Crop and save
            diffImage = diffImage.copy(QRect( aperture->rectX, aperture->rectY, aperture->rectW, aperture->rectH ));
            diffImage.save(_PATH_DISPLAY_IMAGE);
            qDebug() << "Images saved";

            updateDisplayImage(&diffImage);
        }
    }

    funcResetStatusBar();

    mouseCursorReset();
}

void MainWindow::funcResetStatusBar()
{
    ui->progBar->setValue(0);
    ui->progBar->setVisible(false);
    ui->labelProgBar->setText("");
    ui->progBar->update();
    ui->labelProgBar->update();
}

void MainWindow::on_actionsquareSettings_triggered()
{
    formSquareApertureSettings* squareSettings = new formSquareApertureSettings(this);
    squareSettings->setModal(true);
    squareSettings->show();
}

void MainWindow::on_pbSelectFolder_clicked()
{
    //---------------------------------------
    //Get directory
    //---------------------------------------
    QString pathSource = funcShowSelDir(_PATH_TMP_HYPCUBES);
    if( pathSource.isNull() )
    {
        qDebug() << "Dir not selected";
        return (void)false;
    }
    funcUpdateSpectralPixels(&pathSource);

}

void MainWindow::funcUpdateSpectralPixels(QString* pathSource)
{
    int l, r, c;

    //---------------------------------------
    //List files in directory
    //---------------------------------------
    lstImages = funcListFilesInDirSortByNumberName(*pathSource);
    if( lstImages.size() == 0 )
    {
        funcShowMsg("ERROR","Invalid directory");
        return (void)false;
    }

    //---------------------------------------
    //Validate imagery in Dir (HDD) and Copu
    //into Memory
    //---------------------------------------
    QImage tmpImg;
    tmpImg  = QImage(lstImages.at(0).absoluteFilePath());
    int W, H, L;
    W = tmpImg.width();
    H = tmpImg.height();
    L = lstImages.size();
    lstHypercubeImgs.clear();
    for( l=0; l<L; l++ )
    {
        //qDebug() << "Llegot 1, size: " << lstImages.size() << " l: " << l << " - " << lstImages.at(l).absoluteFilePath();
        QImage tmpImg(lstImages.at(l).absoluteFilePath());
        lstHypercubeImgs.append( tmpImg );
        if( tmpImg.width() != W || tmpImg.height() != H )
        {
            funcShowMsg("ERROR","Dir selected contains image with different size");
            return (void)false;
        }
    }

    //---------------------------------------
    //Create Cube from Memory
    //---------------------------------------
    tmpHypercube = (int***)funcAllocInteger3DMatrixMemo( H, W, L, tmpHypercube );
    int lstMaxVals[L+1];
    int lstMinVals[L+1];
    lstMaxVals[L] = 0;
    for( l=0; l<=L; l++ )
    {
        lstMinVals[l] = 255;
    }
    for( l=0; l<L; l++ )
    {
        lstMaxVals[l] = 0;
        //qDebug() << "Llegot 6, size: " << lstHypercubeImgs.size() << " l: " << l << " " << lstImages.at(l).absoluteFilePath();
        tmpImg = lstHypercubeImgs.at(l);
        //qDebug() << "Llegot 7, size: " << lstHypercubeImgs.size() << " l: " << l << " " << lstImages.at(l).absoluteFilePath();
        for( r=0; r<H; r++ )
        {
            for( c=0; c<W; c++ )
            {
                tmpHypercube[r][c][l]    = pixelMaxValue( tmpImg.pixel(c,r) );
                lstMaxVals[l] = ( tmpHypercube[r][c][l] > lstMaxVals[l] )?tmpHypercube[r][c][l]:lstMaxVals[l];
                lstMaxVals[L] = ( tmpHypercube[r][c][l] > lstMaxVals[L] )?tmpHypercube[r][c][l]:lstMaxVals[L];
                lstMinVals[l] = ( tmpHypercube[r][c][l] < lstMinVals[l] )?tmpHypercube[r][c][l]:lstMinVals[l];
                lstMinVals[L] = ( tmpHypercube[r][c][l] < lstMinVals[L] )?tmpHypercube[r][c][l]:lstMinVals[L];
            }
        }
    }
    //qDebug() << "lstMinVals[1]: " << lstMinVals[1] << " lstMaxVals[1]: " << lstMaxVals[1];

    /*
    //---------------------------------------
    //Get the mean and std
    //---------------------------------------
    double lambdaMean   = 0.0;
    for( l=0; l<L; l++ )
    {
        lambdaMean      = lambdaMean + lstMaxVals[l];
    }
    lambdaMean          = lambdaMean / (double)L;

    double lstStdVals[L+1];
    for( l=0; l<L; l++ )
    {
        lstStdVals[l]   = sqrt( (lstStdVals[l]-lambdaMean) / (double)(L-1) );
        lstStdVals[L] = ( lstStdVals[l] < lstStdVals[L] )?lstStdVals[l]:lstStdVals[L];
    }
    */

    //---------------------------------------
    //Normalize if required
    //---------------------------------------
    int auxBefore;
    int maxNormedVal = 0;
    float tmpMinValL=0.0, tmpMaxValL=0.0;
    if(
            ui->RadioLoadHypcube_2->isChecked() ||
            ui->RadioLoadHypcube_3->isChecked() ||
            ui->spinBoxAmplifyFactor->value() > 1.0
    ){
        for( l=0; l<L; l++ )
        {
            tmpImg = lstHypercubeImgs.at(l);
            for( r=0; r<H; r++ )
            {
                for( c=0; c<W; c++ )
                {
                    if( ui->RadioLoadHypcube_2->isChecked() && lstMaxVals[l] > 0 )//BAND
                    {
                        auxBefore   = tmpHypercube[r][c][l];
                        tmpMinValL  = lstMinVals[l];
                        tmpMaxValL  = lstMaxVals[l];
                        tmpHypercube[r][c][l]   = round((((float)tmpHypercube[r][c][l] - tmpMinValL) / (tmpMaxValL-tmpMinValL))*255.0);
                    }
                    if( ui->RadioLoadHypcube_3->isChecked() && lstMaxVals[L] > 0 )//CUBE
                    {

                        tmpMinValL  = lstMinVals[L];
                        tmpMaxValL  = lstMaxVals[L];
                        tmpHypercube[r][c][l]   = round((((float)tmpHypercube[r][c][l] - tmpMinValL) / (tmpMaxValL-tmpMinValL))*255.0);
                        //qDebug() << "L: " << lstMaxVals[L];

                        //tmpHypercube[r][c][l] = round( ((double)tmpHypercube[r][c][l] - lambdaMean) / lstStdVals[L]);

                    }

                    /*
                    tmpHypercube[r][c][l]   = round((double)tmpHypercube[r][c][l] *
                                                    ui->spinBoxAmplifyFactor->value() );
                    tmpHypercube[r][c][l]   = (tmpHypercube[r][c][l]>255)?255:tmpHypercube[r][c][l];
                    */
                    tmpImg.setPixelColor(QPoint(c,r),qRgb(tmpHypercube[r][c][l],tmpHypercube[r][c][l],tmpHypercube[r][c][l]));



                    if(maxNormedVal < tmpHypercube[r][c][l])
                    {
                        maxNormedVal = tmpHypercube[r][c][l];
                    }

                }
            }
            lstHypercubeImgs.replace(l,tmpImg);
        }        
    }
    else
    {
        tmpMinValL=255.0;
        tmpMaxValL=0.0;
        for( l=0; l<L; l++ )
        {
            for( r=0; r<H; r++ )
            {
                for( c=0; c<W; c++ )
                {
                    tmpMaxValL = (tmpMaxValL>=tmpHypercube[r][c][l])?tmpMaxValL:tmpHypercube[r][c][l];
                    tmpMinValL = (tmpMinValL<=tmpHypercube[r][c][l])?tmpMinValL:tmpHypercube[r][c][l];
                }
            }
        }
    }

    qDebug() << "tmpMaxValL: " << tmpMaxValL << " tmpMinValL: " << tmpMinValL;



    //---------------------------------------
    //Amplify id Needed
    //---------------------------------------
    if( ui->spinBoxAmplifyFactor->value() > 0.0 )
    {
        for( l=0; l<L; l++ )
        {
            tmpImg = lstHypercubeImgs.at(l);
            for( r=0; r<H; r++ )
            {
                for( c=0; c<W; c++ )
                {
                    tmpHypercube[r][c][l]   = round((double)tmpHypercube[r][c][l] *
                                                    ui->spinBoxAmplifyFactor->value() );
                    tmpHypercube[r][c][l]   = (tmpHypercube[r][c][l]>255)?255:tmpHypercube[r][c][l];
                    tmpImg.setPixelColor(QPoint(c,r),qRgb(tmpHypercube[r][c][l],tmpHypercube[r][c][l],tmpHypercube[r][c][l]));
                }
            }
            lstHypercubeImgs.replace(l,tmpImg);
        }
    }
    qDebug() << "maxNormedVal: " << maxNormedVal;


    //---------------------------------------
    //Display first photo
    //---------------------------------------
    //qDebug() << "Current: " << ui->pbExpPixs->currentIndex();
    ui->pbExpPixs->setCurrentIndex(3);
    if( graphViewSmall == nullptr )
    {
        graphViewSmall = new GraphicsView(this);
        connect( graphViewSmall, SIGNAL( signalMouseMove(QMouseEvent*) ),this, SLOT( funcMouseMoveReaction(QMouseEvent*) ));
        graphViewSmall->resize(tmpImg.width(),tmpImg.height());
        QLayout *layout = new QVBoxLayout();
        layout->addWidget(graphViewSmall);
        layout->setEnabled(false);
        ui->tabShowPixels->setLayout(layout);
        graphViewSmall->move(20,50+20);
    }
    else
    {
        graphViewSmall->scene()->clear();
        //qDebug() << "Removiendo";
    }
    //QPixmap tmpPixmap(lstImages.at(0).absoluteFilePath());
    //funcLoadImageIntoGaphView(graphViewSmall,tmpPixmap);


    //---------------------------------------
    //Enable slide bar
    //---------------------------------------
    //funcLoadImageIntoGaphView(ui->gaphViewImg,lstImages.at(0).absoluteFilePath());
    ui->slideChangeImage->setMinimum(1);
    ui->slideChangeImage->setToolTip(QString::number(1));
    ui->slideChangeImage->setMaximum(lstImages.size());
    ui->slideChangeImage->setMaximumWidth( tmpImg.width() );
    ui->labelCubeImageName->setText( "(1/" + QString::number(lstImages.size()) + ") " + lstImages.at(0).fileName() );
    ui->slideChangeImage->setEnabled(true);
    ui->graphViewPlot->move(20,tmpImg.height()+115+20);
    ui->labelCubeImageName->move(20,tmpImg.height()+55+20);
    ui->slideChangeImage->move(20,tmpImg.height()+85+20);

    //---------------------------------------
    //Draw plot limits
    //---------------------------------------
    funcDrawPlotLimits();

    //---------------------------------------
    //Update Slide
    //---------------------------------------
    ui->slideChangeImage->setValue(1);
    ui->slideChangeImage->update();

}

void MainWindow::funcDrawSpectralPixelIntoSmall(int x, int y)
{
    /*
    const int frameX    = 40;       //left and right
    const int frameY    = 30;       //up and down
    const int rangeInit = 300;      //Wavelength(nm) at the origin
    const int rangeEnd  = 1100;     //Wavelength(nm) max limit
    const int rangeStep = 50;       //Wavelength(nm) between slides*/

    int l;
    qreal W, H, x1, y1, x2, y2, wavelength1, wavelength2, energy1, energy2;

    //------------------------------------------
    //Initialize canvas
    //------------------------------------------
    graphViewSmall->scene()->clear();
    funcDrawPlotLimits();

    //------------------------------------------
    //Prepare variables
    //------------------------------------------
    W = ui->graphViewPlot->width();
    H = ui->graphViewPlot->height();
    const float pixelsByNm      = (float)(W - (2*frameX)) /
                                  (float)(rangeEnd-rangeInit);
    const float energyRange     = (float)(H - (2*frameY));



    //------------------------------------------
    //Plot Spectral Pixel Componnets
    //------------------------------------------
    QPen penLine(Qt::magenta,1);
    QPen penEllipse(Qt::magenta,1);
    QBrush brushEllipse(Qt::magenta);
    for( l=1; l<lstImages.size(); l++ )
    {
        //Calculate coordinates
        wavelength1 = lstImages.at(l-1).completeBaseName().toFloat(0) - (float)rangeInit;
        wavelength2 = lstImages.at(l).completeBaseName().toFloat(0) - (float)rangeInit;
        energy1     = ((float)tmpHypercube[y][x][l-1] / 255.0) * energyRange;
        energy2     = ((float)tmpHypercube[y][x][l] / 255.0) * energyRange;
        x1          = (float)frameX + ( pixelsByNm * wavelength1 );
        y1          = H - (float)frameY - energy1;
        x2          = (float)frameX + ( pixelsByNm * wavelength2 );
        y2          = H - (float)frameY - energy2;
        //Plot Line
        QLineF line(x1,y1,x2,y2);
        ui->graphViewPlot->scene()->addLine(line,penLine);
        //Plot Ellipse
        ui->graphViewPlot->scene()->addEllipse(x1,y1-3,3,3,penEllipse,brushEllipse);
        ui->graphViewPlot->scene()->addEllipse(x2,y2-3,3,3,penEllipse,brushEllipse);
    }
}

void MainWindow::funcMouseMoveReaction(QMouseEvent* e)
{
    int x, y, W, H;
    W = graphViewSmall->width();
    H = graphViewSmall->height();
    x = (e->x()>=0)?e->x():0;
    y = (e->y()>=0)?e->y():0;
    x = (x<W)?x:W;
    y = (y<H)?y:H;
    ui->labelCubeCoordinates->setText( "x,y(" + QString::number(x+1) + "," + QString::number(y+1) + ")" );
    funcDrawSpectralPixelIntoSmall(x,y);
}

void MainWindow::funcLoadImageIntoGaphView( QGraphicsView* canvas, QString filePath )
{
    QPixmap pixMap(filePath);
    QGraphicsScene *scene = new QGraphicsScene(0,0,pixMap.width(),pixMap.height());
    scene->setBackgroundBrush(QBrush(Qt::black));
    canvas->setBackgroundBrush(pixMap);
    canvas->setScene(scene);
    canvas->resize(pixMap.width(),pixMap.height());
    canvas->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvas->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvas->update();
}

void MainWindow::funcLoadImageIntoGaphView( QGraphicsView* canvas, QPixmap* pixMap )
{
    //QPixmap pixMap(filePath);
    QGraphicsScene *scene = new QGraphicsScene(0,0,pixMap->width(),pixMap->height());
    scene->setBackgroundBrush(QBrush(Qt::black));
    canvas->setBackgroundBrush(*pixMap);
    canvas->setScene(scene);
    canvas->resize(pixMap->width(),pixMap->height());
    canvas->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvas->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    canvas->update();
}

void MainWindow::on_slideChangeImage_valueChanged(int value)
{
    ui->slideChangeImage->setToolTip( QString::number(value) );    
    ui->labelCubeImageName->setText( "("+ QString::number(value) +"/" + QString::number(lstImages.size()) + ") " + lstImages.at(value-1).fileName() );

    //Update Image
    QPixmap tmpPixmap = QPixmap::fromImage(lstHypercubeImgs.at(value-1));
    funcLoadImageIntoGaphView(graphViewSmall,&tmpPixmap);
}

void MainWindow::funcNormalizePixelmap(QImage *image, double maxValue)
{
    int x, y;
    QColor tmpPixel;
    for(y=0; y<image->height(); y++)
    {
        for(x=0; x<image->width(); x++)
        {
            tmpPixel  = image->pixelColor(x,y);
            //tmpPixel.setRed( round((tmpPixel.redF()/maxValue)*255.0) );
            //tmpPixel.setGreen(round((tmpPixel.greenF()/maxValue)*255.0) );
            //tmpPixel.setBlue( round((tmpPixel.blueF()/maxValue)*255.0) );
            tmpPixel.setRed(tmpPixel.red() * 3);
            tmpPixel.setGreen(tmpPixel.green() * 3);
            tmpPixel.setBlue(tmpPixel.blue() * 3);
            image->setPixelColor(x,y,tmpPixel);
        }
    }

}

void MainWindow::funcDrawPlotLimits()
{
    //Set scene
    int i, W, H;
    W = ui->graphViewPlot->width();
    H = ui->graphViewPlot->height();
    QGraphicsScene *scene = new QGraphicsScene(0,0,W,H);
    scene->setBackgroundBrush(QBrush(Qt::black));
    ui->graphViewPlot->setScene(scene);
    ui->graphViewPlot->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    ui->graphViewPlot->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

    //Prepare font
    QFont font;
    font.setPixelSize(12);
    font.setBold(false);
    font.setFamily("Calibri");

    //Set axis
    //int range, rangeStep, rangeInit, rangeEnd; //wavelength range
    //int frameX, frameY;
    int range = rangeEnd - rangeInit;


    QPen pen(Qt::white,1.5);
    QPen penTxt(Qt::white,1);
    QLineF lineX(0,H-frameY,W,H-frameY);
    scene->addLine(lineX,pen);
    QLineF lineY(frameX,0,frameX,H);
    scene->addLine(lineY,pen);

    //Set slides X
    qreal frameW, frameH, x1, y1, x2, y2, numLines, spaceBetwLines;
    frameW = W - (frameX*2);
    frameH = H - (frameY*2);
    int lineLen = 6;
    numLines = floor((float)range / (float)rangeStep);
    spaceBetwLines = (float)frameW / (float)numLines;
    for( i=0; i<=numLines; i++ )
    {
        //Line
        x1 = frameX + (i*spaceBetwLines);
        y1 = frameH + frameY - lineLen;
        x2 = x1;
        y2 = y1 + (2*lineLen);
        QLineF line(x1,y1,x2,y2);
        scene->addLine(line,pen);
        //Text
        QGraphicsSimpleTextItem* tmpTxt = new QGraphicsSimpleTextItem(QString::number(rangeInit+(i*rangeStep)) + QString::fromLatin1(" nm"));
        tmpTxt->setPos(x2-15,y2+5);
        tmpTxt->setFont(font);
        tmpTxt->setPen(penTxt);
        scene->addItem(tmpTxt);
    }

    //Set slides Y
    qreal rangeStepY;
    rangeStepY = 0.2;
    numLines = floor(1/rangeStepY);
    spaceBetwLines = (float)frameH / (float)numLines;
    for( i=1; i<=numLines; i++ )
    {
        //Line
        x1 = frameX - lineLen;
        y1 = H - frameY - (i*spaceBetwLines);
        x2 = x1 + (2*lineLen);
        y2 = y1;
        QLineF line(x1,y1,x2,y2);
        scene->addLine(line,pen);
        //Text
        QGraphicsSimpleTextItem* tmpTxt = new QGraphicsSimpleTextItem(QString::number(i*rangeStepY));
        tmpTxt->setPos(x1-25,y1-8);
        tmpTxt->setFont(font);
        tmpTxt->setPen(penTxt);
        scene->addItem(tmpTxt);
    }



}

void MainWindow::on_actionvideoToFrames_triggered()
{
    //-------------------------------------------------
    //Select video
    //-------------------------------------------------
    auxQstring = QFileDialog::getOpenFileName(
                                                        this,
                                                        tr("Select video..."),
                                                        _PATH_VIDEO_FRAMES,
                                                        "(*.mp4);;"
                                                     );
    if( auxQstring.isEmpty() )
    {
        return (void)NULL;
    }

    //-------------------------------------------------
    //Esport Video
    //-------------------------------------------------
    funcVideoToFrames(auxQstring);

}

void MainWindow::on_actionframesToCube_triggered()
{
    int sourceW         = 15;   //Diffraction width
    int waveResolution  = 50.0; //Spectral Resolution
    double minWave      = 430.0;//Spectral sensitivity
    double maxWave      = 750.0;//Spectral sensitivity

    int row, l;

    mouseCursorWait();

    //---------------------------------------
    //Get directory
    //---------------------------------------
    QString pathSource = funcShowSelDir(_PATH_VIDEO_FRAMES);
    if( pathSource.isNull() )
    {
        qDebug() << "Dir not selected";
        mouseCursorReset();
        return (void)false;
    }

    //---------------------------------------
    //List files in directory
    //---------------------------------------
    lstImages = funcListFilesInDir(pathSource);
    if( lstImages.size() == 0 )
    {
        funcShowMsg("ERROR","Invalid directory");
        mouseCursorReset();
        return (void)false;
    }

    //---------------------------------------
    //Validate imagery in Dir
    //---------------------------------------
    QImage tmpImg(lstImages.at(0).absoluteFilePath());
    int W, H, numSlides;
    W           = tmpImg.width();
    H           = tmpImg.height();
    numSlides   = lstImages.size();
    for( l=1; l<numSlides; l++ )
    {
        tmpImg = QImage(lstImages.at(l).absoluteFilePath());
        if( tmpImg.width() != W || tmpImg.height() != H )
        {
            qDebug() << "W: " << W << " H: " << H << " image: " << lstImages.at(l).absoluteFilePath();
            funcShowMsg("ERROR","Dir selected contains image with different size");
            mouseCursorReset();
            return (void)false;
        }
    }

    //---------------------------------------
    //1) Calculates aberration (main curve)
    //2) Define wavelength linear regression
    //3) Define Source Area
    //4) Define Rotation
    //---------------------------------------
    QList<int> mainCurveLeft, mainCurveRight;
    double aux;
    double fitLeft2, fitLeft1, fitLeft0;
    double fitRight2, fitRight1, fitRight0;
    fitLeft2 = -0.000086713; fitLeft1  = 0.099027;  fitLeft0  = 950.78;
    fitRight2 = 0.00014989;  fitRight1 = -0.16814; fitRight0 = 989.78;
    for( row=1; row<=tmpImg.height(); row++ )
    {
        //Left curve
        aux = ((double)row*(double)row*fitLeft2) + ((double)row*fitLeft1) + fitLeft0;
        mainCurveLeft.append( aux );
        //Right curve
        aux = ((double)row*(double)row*fitRight2) + ((double)row*fitRight1) + fitRight0;
        mainCurveRight.append( aux );
    }
    //Wavelegth linear regression
    double fitWave1, fitWave0;
    fitWave1 = 0.85972; fitWave0 = -23.49514;
    //Define source area
    int sourceH;
    int sourceY1, sourceY2;
    //sourceX1 = 966;
    sourceY1 = 200;
    sourceY2 = 600;//988
    //sourceX1 = 966;         sourceY1 = 400;
    //sourceX2 = 966+sourceW; sourceY2 = 700;//988
    sourceH  = sourceY2 -sourceY1;
    //Define Rotation
    double imgRotation;
    imgRotation = 0.8;

    //---------------------------------------
    //Creates Final Image Container
    //---------------------------------------
    int frankiImgW, frankiImgH;
    int L;
    frankiImgW = sourceW * lstImages.size();
    frankiImgH = sourceY2 - sourceY1;
    //QImage frankyImage(frankiImgW,frankiImgH,QImage::Format_RGB32);

    //---------------------------------------
    //Test: Draw RGB Lines Into Image
    //---------------------------------------
    if( 0 )
    {
        int testX, testY, testWaveDistance, offsetLeft, offsetRight;
        double waveR, waveG, waveB;
        waveR = 618.0;
        waveG = 548.0;
        waveB = 438.0;
        offsetLeft = 0;
        offsetRight = 0;
        QImage testImg = funcRotateImage( _PATH_DISPLAY_IMAGE, imgRotation );
        //Red
        testWaveDistance = (waveR*fitWave1)+fitWave0;
        for( testY=sourceY1; testY<sourceY2; testY++ )
        {
            //Right
            testX = mainCurveRight.at(testY) + testWaveDistance + offsetRight;
            testImg.setPixel(testX,testY,qRgb(255,0,0));
            //Left
            testX = mainCurveLeft.at(testY) - testWaveDistance - offsetLeft;
            testImg.setPixel(testX,testY,qRgb(255,0,0));
        }
        //Green
        testWaveDistance = (waveG*fitWave1)+fitWave0;
        for( testY=sourceY1; testY<sourceY2; testY++ )
        {
            //Right
            testX = mainCurveRight.at(testY) + testWaveDistance + offsetRight;
            testImg.setPixel(testX,testY,qRgb(0,255,0));
            //Left
            testX = mainCurveLeft.at(testY) - testWaveDistance - offsetLeft;
            testImg.setPixel(testX,testY,qRgb(0,255,0));
        }
        //Blue
        testWaveDistance = (waveB*fitWave1)+fitWave0;
        for( testY=sourceY1; testY<sourceY2; testY++ )
        {
            //Right
            testX = mainCurveRight.at(testY) + testWaveDistance + offsetRight;
            testImg.setPixel(testX,testY,qRgb(0,0,255));
            //Left
            testX = mainCurveLeft.at(testY) - testWaveDistance - offsetLeft;
            testImg.setPixel(testX,testY,qRgb(0,0,255));
        }
        testImg.save(_PATH_AUX_IMG);
    }

    //---------------------------------------
    //Define Spectral Resolution
    //---------------------------------------
    double tmpWave;

    //---------------------------------------
    //Fill each wavelength into frankiImage
    //---------------------------------------

    //.......................................
    //Creates one frankiImage for each
    //wavelength
    //.......................................
    funcClearDirFolder(_PATH_TMP_HYPCUBES);
    L = round((maxWave-minWave)/waveResolution);
    //qDebug() << "frankiImgH: " << frankiImgH << " frankiImgW: " << frankiImgW << " frankiImgL: " << frankiImgL;
    //int hypFranki[frankiImgH][frankiImgW][frankiImgL];
    //qDebug() << "frankiImgH: " << frankiImgH << " frankiImgW: " << frankiImgW << " frankiImgL: " << frankiImgL;
    int*** hypFranki;
    hypFranki = (int***)funcAllocInteger3DMatrixMemo( frankiImgH/*Rows*/, frankiImgW/*Cols*/, L, hypFranki );
    //qDebug() << "Después... frankiImgH: " << frankiImgH << " frankiImgW: " << frankiImgW << " L: " << L;

    //.......................................
    //List frankiImagery
    //.......................................
    QList<int> colorSensed;
    QRgb tmpPixelLelft,tmpPixelRight;
    int waveDistance, frankiID, frankiX, frankiY, frankiSlideXInit;
    int diffFrameY, tmpDiffFrameXL, tmpDiffFrameXR;
    int sourceX, sourceY;
    QList<QFileInfo> lstFrankiFrames = funcListFilesInDir(_PATH_VIDEO_FRAMES);
    if( lstFrankiFrames.size() == 0 )
    {
        funcShowMsg("ERROR","Invalid frankiDirectory");
        return (void)false;
    }

    for( frankiID=0; frankiID<lstFrankiFrames.size(); frankiID++ )
    {
        //Obtain frame
        QImage tmpImg = funcRotateImage( lstFrankiFrames.at(frankiID).filePath(), imgRotation );
        //Calculates initial position for this slide
        for(l=0; l<L; l++)
        {
            //Define x position in frankiImage
            frankiSlideXInit = frankiID*sourceW;
            //Define wavelength
            tmpWave      = minWave + (l*waveResolution);
            //Define distance respect to the source for that wavelength
            waveDistance = (tmpWave*fitWave1)+fitWave0;
            //qDebug() << "l: " << l << "waveDistance: " << waveDistance;
            for(sourceX=0; sourceX<sourceW; sourceX++)
            {
                frankiX = frankiSlideXInit + sourceX;
                for(sourceY=0; sourceY<sourceH; sourceY++)
                {
                    //Diffraction Y position
                    diffFrameY      = sourceY + sourceY1;
                    //Calculate position for diffraction wavelength in original image
                    tmpDiffFrameXL  = mainCurveLeft.at(diffFrameY) - waveDistance;//Left
                    tmpDiffFrameXR  = mainCurveRight.at(diffFrameY) + waveDistance;//Right
                    if(tmpDiffFrameXL<0 || tmpDiffFrameXR>=tmpImg.width() )
                    {
                        qDebug() << "Wrong coordinates: tmpDiffFrameXL: " << tmpDiffFrameXL << " tmpDiffFrameXR: " << tmpDiffFrameXR;
                        mouseCursorReset();
                        exit(0);
                    }


                    //Obtain the maximum value for diffraction on left and
                    //right in order to avoid emission light angle
                    if( tmpImg.pixel(tmpDiffFrameXL,diffFrameY) )
                    tmpPixelLelft = tmpImg.pixel(tmpDiffFrameXL,diffFrameY);
                    tmpPixelRight = tmpImg.pixel(tmpDiffFrameXR,diffFrameY);
                    colorSensed.clear();
                    colorSensed.append(qRed(tmpPixelLelft));
                    colorSensed.append(qGreen(tmpPixelLelft));
                    colorSensed.append(qBlue(tmpPixelLelft));
                    //colorSensed.append(qRed(tmpPixelRight));
                    //colorSensed.append(qGreen(tmpPixelRight));
                    //colorSensed.append(qBlue(tmpPixelRight));
                    //Save value
                    frankiY = sourceY;
                    //qDebug() << "(" << frankiX << "," << frankiY << "): " << *std::max_element(colorSensed.begin(),colorSensed.end());
                    hypFranki[frankiY][frankiX][l] = *std::max_element(colorSensed.begin(),colorSensed.end());
                }
            }
        }
    }
    tmpPixelRight = tmpPixelRight;

    //.......................................
    //Exports layers in the cube created
    //Each layer is a gray-scale image
    //.......................................
    //qDebug() << "Exportando... L: " << L;
    for(l=0; l<L; l++)
    {
        //Define wavelength
        tmpWave      = minWave + (l*waveResolution);
        //Copy values into a new gray-scale image
        QImage imgFromCube(frankiImgW,frankiImgH,QImage::Format_Grayscale8);
        for(sourceX=0; sourceX<sourceW; sourceX++)
        {
            for(sourceY=0; sourceY<sourceH; sourceY++)
            {
                imgFromCube.setPixel(sourceX,sourceY,hypFranki[sourceY][sourceX][l]);
            }
        }
        //qDebug() << _PATH_TMP_HYPCUBES + QString::number(tmpWave) + ".png";
        imgFromCube.save( _PATH_TMP_HYPCUBES + QString::number(tmpWave) + ".png" );
    }

    mouseCursorReset();

}

void MainWindow::on_pbTimeLapse_clicked()
{

    bool ok;
    QString tmpCommand;

    //Get Local Sync Folder
    QString syncFolder;
    syncFolder = funcGetSyncFolder();

    //--------------------------------------
    //Clear Temporal Remote Directory
    //--------------------------------------

    //CREATE Temporal Remote Directory
    tmpCommand.clear();
    tmpCommand.append("mkdir ");
    tmpCommand.append(_PATH_REMOTE_FOLDER_TEST_SLIDE);
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,0,false,&ok);
    qDebug() << "tmpCommand: " << tmpCommand;
    if( !ok )
    {
        funcShowMsgERROR_Timeout("Clearing Remote Folder",this);
        return (void)false;
    }

    //Clear Folder
    tmpCommand.clear();
    tmpCommand.append("sudo rm ");
    tmpCommand.append(_PATH_REMOTE_FOLDER_TEST_SLIDE);
    tmpCommand.append("*");    
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,0,false,&ok);
    if( !ok )
    {
        funcShowMsgERROR_Timeout("Clearing Remote Folder",this);
        return (void)false;
    }

    //--------------------------------------
    //Clear Temporal Local Folder
    //--------------------------------------
    QString tmpLocalFolder;
    tmpLocalFolder.clear();
    tmpLocalFolder.append(syncFolder);
    tmpLocalFolder.append(_PATH_LOCAL_FOLDER_SLIDELAPSE);
    tmpLocalFolder.append(_PATH_LOCAL_FOLDER_TEST_SLIDE);
    if( fileExists(tmpLocalFolder) )
    {
        tmpCommand.clear();
        tmpCommand.append("rm -R ");
        tmpCommand.append(tmpLocalFolder);
        tmpCommand.append("*");
        funcExecuteCommand(tmpCommand.toStdString().data());
        qDebug() << "tmpCommand: " << tmpCommand;
    }
    else
    {
        //Clear Local Folder's Parent
        tmpLocalFolder.clear();
        tmpLocalFolder.append(syncFolder);
        tmpLocalFolder.append(_PATH_LOCAL_FOLDER_SLIDELAPSE);
        qDebug() << "Checking if exists: " << tmpCommand;
        if( !fileExists(tmpLocalFolder) )
        {
            //Create Folder's Parent
            tmpCommand.clear();
            tmpCommand.append("mkdir ");
            tmpCommand.append(tmpLocalFolder);
            funcExecuteCommand(tmpCommand.toStdString().data());
            qDebug() << "tmpCommand: " << tmpCommand;

            //Create Local Folder
            tmpCommand.clear();
            tmpCommand.append("mkdir ");
            tmpCommand.append(syncFolder);
            tmpCommand.append(_PATH_LOCAL_FOLDER_SLIDELAPSE);
            tmpCommand.append(_PATH_LOCAL_FOLDER_TEST_SLIDE);
            funcExecuteCommand(tmpCommand.toStdString().data());
            qDebug() << "tmpCommand: " << tmpCommand;

        }
        else
        {
            //Create Local Folder
            tmpCommand.clear();
            tmpCommand.append("mkdir ");
            tmpCommand.append(syncFolder);
            tmpCommand.append(_PATH_LOCAL_FOLDER_SLIDELAPSE);
            tmpCommand.append(_PATH_LOCAL_FOLDER_TEST_SLIDE);
            funcExecuteCommand(tmpCommand.toStdString().data());
            qDebug() << "tmpCommand: " << tmpCommand;
        }
    }

    //--------------------------------------
    //Take Timelapse
    //--------------------------------------
    tmpCommand = genTimelapseCommand(_PATH_REMOTE_FOLDER_TEST_SLIDE);
    qDebug() << "tmpCommand: " << tmpCommand;
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,0,false,&ok);
    progBarUpdateLabel("Timelapsing...",0);
    progBarTimer((ui->spinBoxTrigeringTime->value()+1)*1000);

    //--------------------------------------
    //Obtain Remote Folder
    //--------------------------------------

    //Local Folder Path
    tmpLocalFolder.clear();
    tmpLocalFolder.append(syncFolder);
    tmpLocalFolder.append(_PATH_LOCAL_FOLDER_SLIDELAPSE);
    tmpLocalFolder.append(_PATH_LOCAL_FOLDER_TEST_SLIDE);

    //Tmp
    QString errorTrans, errorDel;
    int status;
    status = obtainRemoteFolder( _PATH_REMOTE_FOLDER_TEST_SLIDE, tmpLocalFolder, &errorTrans, &errorDel );
    if( status == _ERROR )
    {
        funcShowMsgERROR_Timeout("Obtaining Remote Slides",this);
        return (void)false;
    }

    //--------------------------------------
    //Update Image Displayed
    //--------------------------------------
    funcUpdateImageFromFolder(tmpLocalFolder);
    funcResetStatusBar();

}

QString MainWindow::genRemoteVideoCommand(QString remoteVideo,bool ROI)
{
    QString tmpCommand;
    tmpCommand.append("raspivid -n -t ");
    //tmpCommand.append( QString::number( ui->spinBoxVideoDuration->value() * 1000 ) );
    //tmpCommand.append( " -vf -b 50000000 -fps " );
    tmpCommand.append( " -b 50000000 -fps " );
    tmpCommand.append( QString::number(_VIDEO_FRAME_RATE) );
    tmpCommand.append( " -o " );
    tmpCommand.append( remoteVideo );
    //.................................
    //Diffraction Area ROI
    //.................................
    if( ROI == true )
    {
        double W, H;
        squareAperture *aperture = (squareAperture*)malloc(sizeof(squareAperture));
        memset(aperture,'\0',sizeof(squareAperture));
        if( !funGetSquareXML( _PATH_VIDEO_SLIDE_DIFFRACTION, aperture ) )
        {
            funcShowMsg("ERROR","Loading Usable Area in Pixels: _PATH_SLIDE_DIFFRACTION");
            tmpCommand.clear();
            return tmpCommand;
        }
        W = (double)aperture->rectW/(double)aperture->canvasW;
        H = (double)aperture->rectH/(double)aperture->canvasH;
        qDebug() << "W: " << W << " H: " << H;

        tmpCommand.append(" -roi ");
        tmpCommand.append(QString::number((double)aperture->rectX/(double)aperture->canvasW)+",");
        tmpCommand.append(QString::number((double)aperture->rectY/(double)aperture->canvasH)+",");
        tmpCommand.append(QString::number(W)+",");
        tmpCommand.append(QString::number(H));

        //.................................
        //Image Size
        //.................................
        int camMP = (ui->radioRes5Mp->isChecked())?5:8;
        camRes          = getCamRes(camMP);
        //Width
        tmpCommand.append(" -w ");
        tmpCommand.append(QString::number( round((double)camRes->videoW*W) ));
        //Height
        tmpCommand.append(" -h ");
        tmpCommand.append(QString::number( round((double)camRes->videoH*H) ));
    }
    else
    {
        //.................................
        //Image Size: Full Resolution
        //.................................
        int camMP       = (ui->radioRes5Mp->isChecked())?5:8;
        camRes          = getCamRes(camMP);
        //Width
        tmpCommand.append(" -w ");
        tmpCommand.append(QString::number( camRes->videoW ));
        //Height
        tmpCommand.append(" -h ");
        tmpCommand.append(QString::number( camRes->videoH ));
    }



    //.................................
    //Colour balance?
    //.................................
    if(ui->cbColorBalance->isChecked() ){
        tmpCommand.append(" -ifx colourbalance");
    }

    //.................................
    //Denoise?
    //.................................
    if( ui->cbDenoise->isChecked() ){
        tmpCommand.append(" -ifx denoise");
    }

    //.................................
    //Diffraction Shuter speed
    //.................................
    int shutSpeed = ui->spinBoxShuterSpeed->value();
    if( shutSpeed > 0 ){
        tmpCommand.append(" -ss " + QString::number(shutSpeed));
    }

    //.................................
    //AWB
    //.................................
    if( ui->cbAWB->currentText() != "none" ){
        tmpCommand.append(" -awb " + ui->cbAWB->currentText());
    }

    //.................................
    //Exposure
    //.................................
    if( ui->cbExposure->currentText() != "none" ){
        tmpCommand.append(" -ex " + ui->cbExposure->currentText());
    }

    //.................................
    //ISO
    //.................................
    if( ui->spinBoxISO->value() > 0 ){
        tmpCommand.append(" -ISO " + QString::number(ui->spinBoxISO->value()) );
    }

    //.................................
    //Flipped
    //.................................
    if( ui->cbFlipped->isChecked() ){
        tmpCommand.append(" -vf " );
    }

    //.................................
    //Horizontal Flipped
    //.................................
    if( ui->cbFlippedHorizontal->isChecked() ){
        tmpCommand.append(" -hf " );
    }


    return tmpCommand;
}



QString MainWindow::genTimelapseCommand(QString folder,bool setROI)
{
    QString tmpCommand;
    tmpCommand.clear();
    tmpCommand.append("raspistill -t");
    tmpCommand.append(" " + QString::number(ui->spinBoxTimelapseDuration->value()*1000));
    tmpCommand.append(" -tl ");
    tmpCommand.append(QString::number( ui->spinBoxTimelapse->value() ));
    tmpCommand.append(" -o "+ folder +"%d.RGB888");
    tmpCommand.append(" -n -q 100 -gc");

    //tmpCommand.append(" -tl 1000 -n -roi 0.221649485,0.313559322,0.416237113,0.372881356 -o ./tmpSnapVideos/%d.RGB888");
    //tmpCommand.append(" -tl 1000 -n -roi 0.2200,0.3136,0.4162,0.3729 -o ./tmpSnapVideos/%d.RGB888");
    //progBarTimer((ui->slideTriggerTime->value()+1)*1000);
    //funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected);

    if( setROI == true )
    {
        //.................................
        //Diffraction Area ROI
        //.................................
        double W, H;
        squareAperture *aperture = (squareAperture*)malloc(sizeof(squareAperture));
        memset(aperture,'\0',sizeof(squareAperture));
        if( !funGetSquareXML( _PATH_SLIDE_DIFFRACTION, aperture ) )
        {
            funcShowMsg("ERROR","Loading Usable Area in Pixels: _PATH_SLIDE_DIFFRACTION");
            tmpCommand.clear();
            return tmpCommand;
        }
        W = (double)aperture->rectW/(double)aperture->canvasW;
        H = (double)aperture->rectH/(double)aperture->canvasH;

        tmpCommand.append(" -roi ");
        tmpCommand.append(QString::number((double)aperture->rectX/(double)aperture->canvasW)+",");
        tmpCommand.append(QString::number((double)aperture->rectY/(double)aperture->canvasH)+",");
        tmpCommand.append(QString::number(W)+",");
        tmpCommand.append(QString::number(H));

        //.................................
        //Image Size ROI
        //.................................
        int camMP       = (ui->radioRes5Mp->isChecked())?5:8;
        camRes          = getCamRes(camMP);
        //Width
        tmpCommand.append(" -w ");
        tmpCommand.append(QString::number( round( W * (double)camRes->width ) ));
        //Height
        tmpCommand.append(" -h ");
        tmpCommand.append(QString::number( round( H * (double)camRes->height ) ));
    }
    else
    {
        //.................................
        //Image Size WITHOUT ROI
        //.................................
        int camMP       = (ui->radioRes5Mp->isChecked())?5:8;
        camRes          = getCamRes(camMP);
        //Width
        tmpCommand.append(" -w ");
        tmpCommand.append(QString::number( camRes->width ));
        //Height
        tmpCommand.append(" -h ");
        tmpCommand.append(QString::number( camRes->height ));
    }

    //.................................
    //Colour balance?
    //.................................
    if(ui->cbColorBalance->isChecked() ){
        tmpCommand.append(" -ifx colourbalance");
    }

    //.................................
    //Denoise?
    //.................................
    if( ui->cbDenoise->isChecked() ){
        tmpCommand.append(" -ifx denoise");
    }

    //.................................
    //Diffraction Shuter speed
    //.................................
    int shutSpeed = ui->spinBoxShuterSpeed->value();
    if( shutSpeed > 0 ){
        tmpCommand.append(" -ss " + QString::number(shutSpeed));
    }



    /*
    //Width
    ss.str("");
    ss<<reqImg->imgCols;
    tmpCommand->append(" -w " + ss.str());

    //Height
    ss.str("");
    ss<<reqImg->imgRows;
    tmpCommand->append(" -h " + ss.str());*/

    //.................................
    //AWB
    //.................................
    if( ui->cbAWB->currentText() != "none" ){
        tmpCommand.append(" -awb " + ui->cbAWB->currentText());
    }

    //.................................
    //Exposure
    //.................................
    if( ui->cbExposure->currentText() != "none" ){
        tmpCommand.append(" -ex " + ui->cbExposure->currentText());
    }

    //.................................
    //ISO
    //.................................
    if( ui->spinBoxISO->value() > 0 ){
        tmpCommand.append(" -ISO " + QString::number(ui->spinBoxISO->value()) );
    }

    //.................................
    //Flipped
    //.................................
    if( ui->cbFlipped->isChecked() ){
        tmpCommand.append(" -vf " );
    }

    //.................................
    //Horizontal Flipped
    //.................................
    if( ui->cbFlippedHorizontal->isChecked() ){
        tmpCommand.append(" -hf " );
    }


    return tmpCommand;
}


QString MainWindow::genSubareaRaspistillCommand( QString remoteFilename, QString subareaRectangle )
{
    //.................................
    //Basics Settings
    //.................................
    QString tmpCommand;
    tmpCommand.clear();
    tmpCommand.append("raspistill -o ");
    tmpCommand.append(remoteFilename);
    tmpCommand.append(" -t " + QString::number(ui->spinBoxTrigeringTime->value()*1000));
    tmpCommand.append(" -n -q 100 -gc");

    //.................................
    //Diffraction Area ROI
    //.................................
    double W, H;
    squareAperture *aperture = (squareAperture*)malloc(sizeof(squareAperture));
    memset(aperture,'\0',sizeof(squareAperture));
    if( !funGetSquareXML( subareaRectangle, aperture ) )
    {
        funcShowMsg("ERROR","Loading Usable Area in Pixels: _PATH_SLIDE_DIFFRACTION");
        tmpCommand.clear();
        return tmpCommand;
    }
    W = (double)aperture->rectW/(double)aperture->canvasW;
    H = (double)aperture->rectH/(double)aperture->canvasH;

    tmpCommand.append(" -roi ");
    tmpCommand.append(QString::number((double)aperture->rectX/(double)aperture->canvasW)+",");
    tmpCommand.append(QString::number((double)aperture->rectY/(double)aperture->canvasH)+",");
    tmpCommand.append(QString::number(W)+",");
    tmpCommand.append(QString::number(H));

    //.................................
    //Image Size ROI
    //.................................
    int camMP       = (ui->radioRes5Mp->isChecked())?5:8;
    camRes          = getCamRes(camMP);
    //Width
    tmpCommand.append(" -w ");
    tmpCommand.append(QString::number( round( W * (double)camRes->width ) ));
    //Height
    tmpCommand.append(" -h ");
    tmpCommand.append(QString::number( round( H * (double)camRes->height ) ));

    //.................................
    //Colour balance?
    //.................................
    if(ui->cbColorBalance->isChecked() ){
        tmpCommand.append(" -ifx colourbalance");
    }

    //.................................
    //Denoise?
    //.................................
    if( ui->cbDenoise->isChecked() ){
        tmpCommand.append(" -ifx denoise");
    }

    //.................................
    //Diffraction Shuter speed
    //.................................
    int shutSpeed = ui->spinBoxShuterSpeed->value();
    if( shutSpeed > 0 ){
        tmpCommand.append(" -ss " + QString::number(shutSpeed));
    }



    /*
    //Width
    ss.str("");
    ss<<reqImg->imgCols;
    tmpCommand->append(" -w " + ss.str());

    //Height
    ss.str("");
    ss<<reqImg->imgRows;
    tmpCommand->append(" -h " + ss.str());*/

    //.................................
    //AWB
    //.................................
    if( ui->cbAWB->currentText() != "none" ){
        tmpCommand.append(" -awb " + ui->cbAWB->currentText());
    }

    //.................................
    //Exposure
    //.................................
    if( ui->cbExposure->currentText() != "none" ){
        tmpCommand.append(" -ex " + ui->cbExposure->currentText());
    }

    //.................................
    //ISO
    //.................................
    if( ui->spinBoxISO->value() > 0 ){
        tmpCommand.append(" -ISO " + QString::number(ui->spinBoxISO->value()) );
    }

    //.................................
    //Flipped
    //.................................
    if( ui->cbFlipped->isChecked() ){
        tmpCommand.append(" -vf " );
    }

    //.................................
    //Horizontal Flipped
    //.................................
    if( ui->cbFlippedHorizontal->isChecked() ){
        tmpCommand.append(" -hf " );
    }


    return tmpCommand;
}


std::string MainWindow::funcRemoteTerminalCommand(
                                                    std::string command,
                                                    structCamSelected *camSelected,
                                                    int trigeredTime,
                                                    bool waitForAnswer,
                                                    bool* ok
){
    //* It is used when waitForAnswer==true
    //* Wait for answer when you need to know a parameter or the
    //  command result
    *ok = true;
    std::string tmpTxt;

    //--------------------------------------
    //Open socket
    //--------------------------------------
    int n;
    int sockfd = connectSocket( camSelected );
    qDebug() << "Socket opened";

    //--------------------------------------
    //Excecute command
    //--------------------------------------
    frameStruct *frame2send = (frameStruct*)malloc(sizeof(frameStruct));
    memset(frame2send,'\0',sizeof(frameStruct));    
    frame2send->header.idMsg        = (waitForAnswer==true)?(unsigned char)2:(unsigned char)5;    
    frame2send->header.numTotMsg    = 1;
    frame2send->header.consecutive  = 1;
    frame2send->header.trigeredTime = trigeredTime;
    frame2send->header.bodyLen      = command.length();
    bzero(frame2send->msg,command.length()+1);
    memcpy( frame2send->msg, command.c_str(), command.length() );

    //Request command result
    n = ::write(sockfd,frame2send,sizeof(frameStruct)+1);
    if(n<0){
        qDebug() << "ERROR: Excecuting Remote Command";
        *ok = false;
        return "";
    }

    //Receibing ack with file len    
    unsigned char bufferRead[frameBodyLen];
    n = read(sockfd,bufferRead,frameBodyLen);    

    if( waitForAnswer == true )
    {
        unsigned int fileLen;
        memcpy(&fileLen,&bufferRead,sizeof(unsigned int));
        fileLen = (fileLen<frameBodyLen)?frameBodyLen:fileLen;
        qDebug() << "fileLen: " << fileLen;
        //funcShowMsg("FileLen n("+QString::number(n)+")",QString::number(fileLen));

        //Receive File
        unsigned char tmpFile[fileLen];
        if( funcReceiveFile( sockfd, fileLen, bufferRead, tmpFile ) == false )
        {
            *ok = false;
        }
        tmpTxt.clear();
        tmpTxt.assign((char*)tmpFile);
        qDebug() <<tmpFile;
    }
    ::close(sockfd);

    //Return Command Result    
    return tmpTxt;
}

void MainWindow::on_actionRGB_to_XY_triggered()
{
    //Validate that exist pixel selected
    //..
    if( lstSelPix->count()<1){
        funcShowMsg("Lack","Not pixels selected");
        return (void)false;
    }

    //Create xycolor space
    //..
    GraphicsView *xySpace = new GraphicsView(this);
    funcPutImageIntoGV(xySpace, "./imgResources/CIEManual.png");
    xySpace->setWindowTitle( "XY color space" );
    xySpace->show();

    //Transform each pixel from RGB to xy and plot it
    //..
    QImage tmpImg(_PATH_DISPLAY_IMAGE);
    int W = tmpImg.width();
    int H = tmpImg.height();
    int pixX, pixY;
    QRgb tmpPix;
    colSpaceXYZ *tmpXYZ = (colSpaceXYZ*)malloc(sizeof(colSpaceXYZ));
    int tmpOffsetX = -13;
    int tmpOffsetY = -55;
    int tmpX, tmpY;
    int i;
    qDebug() << "<lstSelPix->count(): " << lstSelPix->count();
    for( i=0; i<lstSelPix->count(); i++ ){
        //Get pixel position in real image
        pixX = (float)(lstSelPix->at(i).first * W) / (float)canvasAux->width();
        pixY = (float)(lstSelPix->at(i).second * H) / (float)canvasAux->height();
        tmpPix = tmpImg.pixel( pixX, pixY );
        funcRGB2XYZ( tmpXYZ, (float)qRed(tmpPix), (float)qGreen(tmpPix), (float)qBlue(tmpPix) );
        //funcRGB2XYZ( tmpXYZ, 255.0, 0, 0  );
        tmpX = floor( (tmpXYZ->x * 441.0) / 0.75 ) + tmpOffsetX;
        tmpY = 522 - floor( (tmpXYZ->y * 481.0) / 0.85 ) + tmpOffsetY;
        funcAddPoit2Graph( xySpace, tmpX, tmpY, 1, 1,
                           QColor(qRed(tmpPix),qGreen(tmpPix),qBlue(tmpPix)),
                           QColor(qRed(tmpPix),qGreen(tmpPix),qBlue(tmpPix))
                         );
    }

    //Save image plotted
    //..
    QPixmap pixMap = QPixmap::grabWidget(xySpace);
    pixMap.save("./Results/Miscelaneas/RGBPloted.png");
}

void MainWindow::on_actionNDVI_triggered()
{
    mouseCursorWait();

    QString stringThreshold;
    QString stringBrilliant;
    QString infraredChanel;
    //QString infraredWeight;
    QString redChanel;
    stringThreshold = readFileParam(_PATH_NDVI_THRESHOLD);
    stringBrilliant = readFileParam(_PATH_NDVI_BRILLIANT);
    infraredChanel  = readFileParam(_PATH_NDVI_IR_CHANEL);
    //infraredWeight  = readFileParam(_PATH_NDVI_INFRARED_WEIGHT);//_PATH_NDVI_MIN_VALUE
    redChanel       = readFileParam(_PATH_NDVI_RED_CHANEL);

    int makeBrilliant = (stringBrilliant.toInt(0)==1)?1:0;
    funcNDVI( globalEditImg, stringThreshold.toDouble(0), makeBrilliant, infraredChanel, redChanel );
    updateDisplayImage(globalEditImg);

    mouseCursorReset();
}

void MainWindow::on_actionNDVI_Algorithm_triggered()
{
    formNDVISettings* ndviSettings = new formNDVISettings(this);
    ndviSettings->setModal(true);
    ndviSettings->show();
}

void MainWindow::on_actionFull_Screen_triggered()
{
    QImage tmpImg(auxQstring);
    displayImageFullScreen(tmpImg);
}

void MainWindow::on_actionDisplay_Original_triggered()
{
    updateDisplayImage(_PATH_DISPLAY_IMAGE);
}

void MainWindow::on_actionFull_photo_triggered()
{
    mouseCursorWait();

    if( !takeRemoteSnapshot(_PATH_REMOTE_SNAPSHOT,false) )
    {
        qDebug() << "ERROR: Taking Full Area";
        return (void)NULL;
    }

    QImage snapShot = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
    updateDisplayImage(&snapShot);
    snapShot.save(_PATH_DISPLAY_IMAGE);

    mouseCursorReset();
}

void MainWindow::on_actionDiffraction_triggered()
{
    mouseCursorWait();

    if( !takeRemoteSnapshot(_PATH_REMOTE_SNAPSHOT,false) )
    {
        qDebug() << "ERROR: Taking Diffration Area";
        return (void)NULL;
    }
    else
    {
        QImage diffImage = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
        if( diffImage.isNull() )
        {
            qDebug() << "ERROR: Obtaining Diffration Area";
            return (void)NULL;
        }
        else
        {
            //
            //Crop original image to release the usable area
            //
            //Get usable area coordinates
            squareAperture *aperture = (squareAperture*)malloc(sizeof(squareAperture));
            memset(aperture,'\0',sizeof(squareAperture));
            if( !rectangleInPixelsFromSquareXML( _PATH_REGION_OF_INTERES, aperture ) )
            {
                funcShowMsg("ERROR","Loading Usable Area in Pixels: _PATH_REGION_OF_INTERES");
                return (void)false;
            }
            //Crop and save
            diffImage = diffImage.copy(QRect( aperture->rectX, aperture->rectY, aperture->rectW, aperture->rectH ));
            diffImage.save(_PATH_DISPLAY_IMAGE);
            qDebug() << "Images saved";

            updateDisplayImage(&diffImage);
        }
    }

    mouseCursorReset();
}

void MainWindow::on_actionComposed_triggered()
{

    functionTakeComposedSquarePicture();
    /*
    mouseCursorWait();

    if( !takeRemoteSnapshot(_PATH_REMOTE_SNAPSHOT,false) )
    {
        qDebug() << "ERROR: Taking Diffration Area";
        return (void)NULL;
    }
    else
    {
        QImage diffImage = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
        if( diffImage.isNull() )
        {
            qDebug() << "ERROR: Obtaining Diffration Area";
            return (void)NULL;
        }
        else
        {
            if( !takeRemoteSnapshot(_PATH_REMOTE_SNAPSHOT,true) )
            {
                qDebug() << "ERROR: Taking Aperture Area";
                return (void)NULL;
            }
            else
            {
                QImage apertureImage = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
                if( apertureImage.isNull() )
                {
                    qDebug() << "ERROR: Obtaining Aperture Area";
                    return (void)NULL;
                }
                else
                {
                    //-------------------------------------------
                    //Merge image
                    //-------------------------------------------

                    squareAperture *aperture        = (squareAperture*)malloc(sizeof(squareAperture));
                    squareAperture *squareApertArea = (squareAperture*)malloc(sizeof(squareAperture));
                    memset(aperture,'\0',sizeof(squareAperture));
                    memset(squareApertArea,'\0',sizeof(squareApertArea));

                    //
                    //Crop original image to release the usable area
                    //
                    //Get Region of Interest
                    if( !rectangleInPixelsFromSquareXML( _PATH_REGION_OF_INTERES, aperture ) )
                    {
                        funcShowMsg("ERROR","Loading Usable Area in Pixels: _PATH_SQUARE_USABLE");
                        return (void)false;
                    }
                    //Get usable area coordinates
                    if( !rectangleInPixelsFromSquareXML( _PATH_SQUARE_APERTURE, squareApertArea ) )
                    {
                        funcShowMsg("ERROR","Loading Usable Area in Pixels: _PATH_SQUARE_USABLE");
                        return (void)false;
                    }


                    //
                    //Copy square aperture into diffraction image
                    //
                    for( int y=squareApertArea->rectY; y<=(squareApertArea->rectY+squareApertArea->rectH); y++ )
                    {
                        for( int x=squareApertArea->rectX; x<=(squareApertArea->rectX+squareApertArea->rectW); x++ )
                        {
                            diffImage.setPixel( x, y, apertureImage.pixel( x, y ) );
                        }
                    }

                    //
                    //Cut Resultant Image
                    //
                    diffImage       = diffImage.copy(QRect( aperture->rectX, aperture->rectY, aperture->rectW, aperture->rectH ));
                    //apertureImage   = apertureImage.copy(QRect( squareApertArea->rectX, squareApertArea->rectY, squareApertArea->rectW, squareApertArea->rectH ));

                    //
                    //Save cropped image
                    //
                    diffImage.save(_PATH_DISPLAY_IMAGE);
                    updateDisplayImage(&diffImage);
                }
            }
        }
    }

    //progBarUpdateLabel("",0);
    mouseCursorReset();
    */
}

void MainWindow::on_actionVideo_triggered()
{
    //Update camera resolution
    //..
    int camMP       = (ui->radioRes5Mp->isChecked())?5:8;
    camRes          = getCamRes(camMP);

    //Set required image's settings
    //..
    strReqImg *reqImg       = (strReqImg*)malloc(sizeof(strReqImg));
    memset(reqImg,'\0',sizeof(strReqImg));

    //Codec H264 or MJPEG
    QString videoLocalFilename;
    QString videoRemoteFilename;
    //const u_int8_t videoCodec = _MJPEG;
    const u_int8_t videoCodec = _H264;
    if( videoCodec == _H264 )
    {
        videoRemoteFilename.append(_PATH_VIDEO_REMOTE_H264);
        videoLocalFilename.append(_PATH_VIDEO_RECEIVED_H264);
    }
    else
    {
        memcpy( reqImg->video.cd, "MJPEG", 5 );
        qDebug() << "MJPEG";

        if( 1 )
        {
            videoRemoteFilename.append(_PATH_VIDEO_REMOTE_MJPEG);
            videoLocalFilename.append(_PATH_VIDEO_RECEIVED_MJPEG);
        }
        else
        {
            videoRemoteFilename.append(_PATH_VIDEO_REMOTE_H264);
            videoLocalFilename.append(_PATH_VIDEO_RECEIVED_H264);
        }
    }
    qDebug() << "videoRemoteFilename: " << videoRemoteFilename;
    memcpy( reqImg->video.o, videoRemoteFilename.toStdString().c_str(), videoRemoteFilename.size() );

    //Others
    reqImg->idMsg           = (unsigned char)12;
    //reqImg->video.t         = ui->spinBoxVideoDuration->value();
    reqImg->video.ss        = (int16_t)round(ui->spinBoxShuterSpeed->value());
    reqImg->video.awb       = (int16_t)ui->cbAWB->currentIndex();
    reqImg->video.ex        = (int16_t)ui->cbExposure->currentIndex();
    reqImg->video.md        = 2;//2
    reqImg->video.w         = 0;//1640
    reqImg->video.h         = 0;//1232
    reqImg->video.fps       = 5;//1-15

    //
    //Motor walk
    //
    // HELP
    //
    // (ERROR on binding: Address already in use) -> netstat -tulpn -> kill process
    // Para saber el USB -> ls -l /dev/tty (tab)
    reqImg->motorWalk.degreeIni     = 0;
    reqImg->motorWalk.degreeEnd     = 180;
    reqImg->motorWalk.durationMs    = 3000;
    reqImg->motorWalk.stabilizingMs = 1000;

    //Open socket
    int n;
    int sockfd = connectSocket( camSelected );
    unsigned char bufferRead[frameBodyLen];
    qDebug() << "Socket opened";
    //Require photo size
    //QtDelay(5);
    n = ::write(sockfd,reqImg,sizeof(strReqImg));
    qDebug() << "Video request sent";

    //Receive ACK with the camera status
    memset(bufferRead,'\0',3);
    n = read(sockfd,bufferRead,2);
    if( bufferRead[1] == 1 )
    {//Begin the video adquisition routine

        //
        // STATUS BAR
        //
        progBarUpdateLabel("Recording video",0);
        //progBarTimer( (ui->spinBoxVideoDuration->value() + 1) * 1000 );

        //Delete if file exists
        funcDeleteFile( _PATH_VIDEO_RECEIVED_H264 );
        funcDeleteFile( _PATH_VIDEO_RECEIVED_MJPEG );
        funcDeleteFile( _PATH_VIDEO_RECEIVED_MP4 );

        //Download new
        qDebug() << "Video recorded";
        progBarUpdateLabel("Transferring video",0);
        obtainFile( videoRemoteFilename.toStdString(), videoLocalFilename.toStdString(), "" );

        //Convert into MP4
        if( videoCodec == _H264 )
        {
            //Convert into .MP4
            QString converToMP4;
            converToMP4 = "";
            converToMP4.append( "MP4Box -add ");
            converToMP4.append(_PATH_VIDEO_RECEIVED_H264);
            converToMP4.append(" ");
            converToMP4.append(_PATH_VIDEO_RECEIVED_MP4);
            funcExecuteCommand( converToMP4 );
            //Update permissions
            //converToMP4 = "chmod +777 ";
            //converToMP4.append(_PATH_VIDEO_RECEIVED_MP4);
            //funcExecuteCommand( converToMP4 );
        }

        //
        //Send .mp4 to frames
        //
        //funcVideoToFrames(_PATH_VIDEO_FRAMES, _PATH_VIDEO_RECEIVED_MP4);




    }
    else
    {//Video does not generated by raspicam
        funcShowMsg("ERROR on Camera","Recording video");
    }

    n = n;
    ::close(sockfd);
}

void MainWindow::on_actionTimelapse_triggered()
{
    mouseCursorWait();
    QString tmpCommand;
    //--------------------------------------
    //Clear Remote Directory
    //--------------------------------------
    tmpCommand.clear();
    tmpCommand.append("sudo rm tmpSnapVideos/*");
    bool ok;
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,0,false,&ok);

    //--------------------------------------
    //Take Timelapse
    //--------------------------------------
    tmpCommand = genTimelapseCommand(_PATH_REMOTE_FOLDER_SLIDELAPSE);
    qDebug() << "tmpCommand: " << tmpCommand;
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,0,false,&ok);
    progBarUpdateLabel("Timelapsing...",0);
    progBarTimer((ui->spinBoxTrigeringTime->value()+1)*1000);

    //--------------------------------------
    //Get Number of Frames
    //--------------------------------------
    std::string stringNumOfFrames = funcRemoteTerminalCommand(
                                                                "ls ./tmpSnapVideos/ -a | wc -l",
                                                                camSelected,
                                                                0,
                                                                true,
                                                                &ok
                                                            );
    int numOfFrames = atoi(stringNumOfFrames.c_str()) - 2;
    //qDebug() << "numOfFrames: " << numOfFrames;

    //--------------------------------------
    //Get Frames
    //--------------------------------------
    if( numOfFrames > 0 )
    {
        funcClearDirFolder( _PATH_VIDEO_FRAMES );
        QString tmpRemoteFrame, tmpLocalFrame;
        int framesTransmited = 0;
        int i = 0;
        int filesNotFound = 0;
        int maxFilesError = 5;
        while( framesTransmited < numOfFrames )
        {
            tmpRemoteFrame = "./tmpSnapVideos/" + QString::number(i) + ".RGB888";
            tmpLocalFrame  = _PATH_VIDEO_FRAMES + QString::number(i) + ".RGB888";

            //Get Remote File
            obtainFile( tmpRemoteFrame.toStdString(), tmpLocalFrame.toStdString(), "" );

            //Check if was transmited
            if( fileExists(tmpLocalFrame) )
            {
                framesTransmited++;
                filesNotFound = 0;
            }
            else
            {
                filesNotFound++;
            }

            //Next image to check
            i++;

            //Break if error
            if( filesNotFound >= maxFilesError )
            {
                funcShowMsg("ERROR","maxFilesError Achieved");
                break;
            }
        }
    }

    progBarUpdateLabel("",0);
    mouseCursorReset();
}

void MainWindow::on_actionSave_triggered()
{
    //
    //Read the filename
    //
    QString fileName;
    QString lastPath = readFileParam(_PATH_LAST_IMG_SAVED);
    if( lastPath.isEmpty() )//First time using this parameter
    {
        lastPath = "./snapshots/";
    }
    fileName = QFileDialog::getSaveFileName(
                                                this,
                                                tr("Save Snapshot as..."),
                                                lastPath,
                                                tr("Documents (*.png)")
                                            );
    if( fileName.isEmpty() )
    {
        qDebug() << "Filename not typed";
        return (void)false;
    }
    else
    {
        lastPath = funcRemoveFileNameFromPath(fileName);
        saveFile(_PATH_LAST_IMG_SAVED,lastPath);
    }

    //
    //Validate filename
    //
    fileName = funcRemoveImageExtension(fileName);

    //
    //Save image
    //
    QImage tmpImg( _PATH_DISPLAY_IMAGE );
    tmpImg.save(fileName);
}

void MainWindow::on_actionSlideDiffraction_triggered()
{
    //----------------------------------------
    // Validate Camera Connection
    //----------------------------------------
    if( camSelected->isConnected == false )
    {
        funcShowMsg("ERROR","Not Camera Connection Found");
        return (void)false;
    }


    mouseCursorWait();
    QString tmpCommand;


    //......................................
    //Clear Remote Directory
    //......................................
    //tmpCommand.clear();
    //tmpCommand.append("sudo rm tmpSnapshots/*");
    //funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,false);

    //......................................
    //Generate Remote Command
    //......................................
    tmpCommand.clear();
    tmpCommand = genSubareaRaspistillCommand( _PATH_REMOTE_SNAPSHOT, _PATH_SLIDE_DIFFRACTION );
    qDebug() << "tmpCommand: " << tmpCommand;

    //......................................
    //Execute Remote Command
    //......................................
    bool ok;
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,0,false,&ok);
    progBarUpdateLabel(_MSG_PROGBAR_STABILIZING,0);
    progBarTimer((ui->spinBoxTrigeringTime->value()+1)*1000);

    //......................................
    //Get Remote File
    //......................................
    obtainImageFile( _PATH_REMOTE_SNAPSHOT, _PATH_IMAGE_RECEIVED );

    //......................................
    //Check if the image was transmited
    //......................................
    if( !fileExists(_PATH_IMAGE_RECEIVED) )
    {
        funcShowMsg("ERROR","Image was not received");
    }
    else
    {
        QImage tmpImg(_PATH_IMAGE_RECEIVED);
        updateDisplayImage(&tmpImg);
    }

    //Reset Mouse and Progress-Bar
    progBarUpdateLabel(_MSG_PROGBAR_RESET,0);
    mouseCursorReset();
}

void MainWindow::on_actionObtain_Folder_triggered()
{
    formObtainFolder* formObtRemoteFolder = new formObtainFolder(this);
    formObtRemoteFolder->setModal(true);    
    connect(
                formObtRemoteFolder,
                SIGNAL( signalObtainRemoteFolder(QString,QString) ),
                this,
                SLOT( obtainRemoteFolder(QString,QString) )
           );
    formObtRemoteFolder->show();
}

int MainWindow::obtainRemoteFolder(
                                        QString remoteFolder,
                                        QString localFolder,
                                        QString* errTrans,
                                        QString* errDel,
                                        bool delFolder
){

    bool delDir = true;

    //------------------------------------------------------
    // Obtain list of files in Remote Folder
    //------------------------------------------------------
    QString tmpCommand;
    tmpCommand.append("ls ");
    tmpCommand.append(remoteFolder);
    tmpCommand.append("*.*");
    bool executedCommand;
    std::string stringLstFiles = funcRemoteTerminalCommand(
                                                                tmpCommand.toStdString(),
                                                                camSelected,
                                                                0,
                                                                true,
                                                                &executedCommand
                                                           );
    if( !executedCommand )return _ERROR;

    //------------------------------------------------------
    // Create local folder if not exists
    //------------------------------------------------------
    QString remoteFile, localFile, barTxt, tmpFileName;
    QString tmpAnswer(stringLstFiles.data());
    QList<QString> tmpFileParts;
    QList<QString> lstFiles = tmpAnswer.split('\n');
    if( lstFiles.size() > 0 )
    {
        //qDebug() << "localFolder: " << localFolder;
        if( !fileExists(localFolder) )
        {
            if( !funcExecuteCommand( "mkdir " + localFolder ) )
            {
                funcShowMsgERROR_Timeout("Creating Local Folder: " + localFolder, this, 1800 );
                return _ERROR;
            }
        }
    }

    //------------------------------------------------------
    // Get each element into the folder
    //------------------------------------------------------
    int i;
    //qDebug() << "lstFiles.size(): " << lstFiles.size() << " remoteFolder: " << remoteFolder;
    for( i=0; i<lstFiles.size(); i++ )
    {
        tmpFileName = lstFiles.at(i).trimmed();
        if(
            !tmpFileName.isEmpty()  &&
            tmpFileName != "./"     &&
            tmpFileName != "../"    &&
            tmpFileName != "."      &&
            tmpFileName != ".."
        ){
            //Define File Names
            tmpFileParts.clear();
            tmpFileParts    = lstFiles.at(i).trimmed().split("/");
            localFile       = localFolder  + tmpFileParts.at(tmpFileParts.size()-1);
            remoteFile      = remoteFolder + tmpFileParts.at(tmpFileParts.size()-1);

            //qDebug() << "remoteFile: " << remoteFile;
            //qDebug() << "localFile: " << localFile;
            //exit(0);

            //Transmit Remote File
            barTxt.clear();
            barTxt.append("Transmitting ");
            barTxt.append(lstFiles.at(i));
            barTxt.append("...");
            obtainFile( remoteFile.toStdString(), localFile.toStdString(), barTxt );

            //Check if file was transmited successfully
            if( !fileExists(localFile) )
            {
                errTrans->append(remoteFile+"\n");
                delDir = false;
            }
            else
            {
                //Remove Remote File
                if( !funcRemoveRemoteFile(remoteFile) )
                {
                    errDel->append(remoteFile+"\n");
                    delDir = false;
                }
            }
        }
    }

    //------------------------------------------------------
    // Copy Folders Recursivelly
    //------------------------------------------------------

    //Get List of Folders
    tmpCommand.clear();
    tmpCommand.append("ls -d ");
    tmpCommand.append(remoteFolder);
    tmpCommand.append("*/");
    std::string stringLstFolders = funcRemoteTerminalCommand(
                                                                tmpCommand.toStdString(),
                                                                camSelected,
                                                                0,
                                                                true,
                                                                &executedCommand
                                                            );
    if( !executedCommand )return _ERROR;

    //......................................................
    // Get each folder into the main folder
    //......................................................
    //QString remoteFile, localFile, barTxt, tmpFileName;
    QString tmpRemoteSubfolder, tmpLocalSubfolder;
    QString tmpAnswerFolder(stringLstFolders.data());
    QList<QString> lstFolders = tmpAnswerFolder.split('\n');
    QList<QString> tmpFolderParts;
    //qDebug() << "lstFolders.size(): " << lstFolders.size() << " remoteFolder: " << remoteFolder;
    for( i=0; i<lstFolders.size(); i++ )
    {
        tmpRemoteSubfolder = lstFolders.at(i).trimmed();
        if(
            !tmpRemoteSubfolder.isEmpty()  &&
             tmpRemoteSubfolder != "./"    &&
             tmpRemoteSubfolder != "../"   &&
             tmpRemoteSubfolder != "."     &&
             tmpRemoteSubfolder != ".."
        ){
            //Extract folder name
            tmpFolderParts.clear();
            tmpFolderParts = tmpRemoteSubfolder.split('/');

            //Obtain Subfolder
            tmpLocalSubfolder  = localFolder  + tmpFolderParts.at(tmpFolderParts.size()-2)+"/";
            //qDebug() << "tmpRemoteSubfolder2: " << tmpRemoteSubfolder;
            //qDebug() << "tmpFolderParts.at(tmpFolderParts.size()): " << tmpFolderParts.at(tmpFolderParts.size()-2);
            //qDebug() << "tmpLocalSubfolder: " << tmpLocalSubfolder;
            //qDebug() << "tmpRemoteSubfolder: " << tmpRemoteSubfolder;
            //fflush(stdout);
            //exit(0);
            obtainRemoteFolder(tmpRemoteSubfolder,tmpLocalSubfolder,errTrans,errDel,true);
        }
    }

    if( delFolder && delDir )
    {
        tmpCommand.clear();
        tmpCommand.append("sudo rm -R " + remoteFolder );
        funcRemoteTerminalCommand(
                                        tmpCommand.toStdString(),
                                        camSelected,
                                        0,
                                        false,
                                        &executedCommand
                                 );
        if( !executedCommand )
        {
            errDel->append(remoteFolder+"\n");
        }
    }

    //Return
    return _OK;
}

bool MainWindow::funcRemoveRemoteFile( QString fileName )
{
    QString tmpCommand;
    tmpCommand.append("sudo rm " + fileName);
    bool executedCommand;
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,0,false,&executedCommand);
    if( !executedCommand )
        return _ERROR;
    return _OK;
}

void MainWindow::on_actionSlide_Build_Hypercube_triggered()
{


    //------------------------------------------------------
    //Show Form
    //------------------------------------------------------
    formBuildSlideHypeCubePreview* slidePreview = new formBuildSlideHypeCubePreview(this);
    slidePreview->setModal(true);
    slidePreview->show();


    /*
    //------------------------------------------------------
    //Select Directory
    //------------------------------------------------------
    QString workDir, lastSlideFrames;
    if( !readFileParam(_PATH_LAST_SLIDE_FRAMES_4CUBE,&lastSlideFrames) )
    {
        lastSlideFrames.clear();
        lastSlideFrames.append(_PATH_VIDEO_FRAMES);
    }
    if( !funcShowSelDir(lastSlideFrames,&workDir) )
    {
        return (void)false;
    }
    saveFile(_PATH_LAST_SLIDE_FRAMES_4CUBE,workDir);

    //------------------------------------------------------
    //Define List With Imagery to Be Considered
    //------------------------------------------------------

    //......................................................
    //Gel List of Files in Directory Selected
    //......................................................
    QList<QFileInfo> lstFiles = funcListFilesInDir( workDir, _FRAME_RECEIVED_EXTENSION );
    qDebug() << "lstFiles: " << lstFiles.length();
    if(lstFiles.size()==0)
    {
        funcShowMsgERROR("Empty Directory");
        return (void)false;
    }

    //......................................................
    //Read Hyperimage's Parameters
    //......................................................
    QString timelapseFile(workDir + _FILENAME_SLIDE_DIR_TIME);
    std::string tmpParam;
    if( !readParamFromFile( timelapseFile.toStdString(), &tmpParam) )
    {
        funcShowMsgERROR("Timelapse Parameter not found at: " + workDir);
        return (void)false;
    }
    int timelapse           = atoi(tmpParam.c_str());
    qDebug() << "numFrames: " << lstFiles.size() << " time: " << timelapse;

    //......................................................
    //Discard Stabilization Frames
    //......................................................
    int numFrame2Discard    = ceil( (float)_RASPBERRY_STABILIZATION_TIME / (float)timelapse );
    int i;
    for( i=0; i<numFrame2Discard; i++ )
    {
        //qDebug() << "file discarded: " << lstFiles.at(i).completeBaseName();
        lstFiles.removeAt(0);
    }*/


    //structSlideHypCube slideCubeSettings;
    //slideCubeSettings.rotateLeft  = true;
    //slideCubeSettings.width = 70;
    //buildHypercubeFromFilelist( lstFiles, slideCubeSettings );

}

void MainWindow::buildHypercubeFromFilelist(
                                                QList<QFileInfo> lstFrames,
                                                structSlideHypCube slideCubeSettings
){
    //[COMMENT]
    //It assumes that lstFrames contains only usable frames

    //----------------------------------------------------
    // Create Image Container
    //----------------------------------------------------
    int resultImgW;
    resultImgW = lstFrames.size() * slideCubeSettings.width;
    QImage tmpImg( lstFrames.at(0).absoluteFilePath() );
    QImage resultImg( resultImgW, tmpImg.height(), QImage::Format_RGB16 );

    //----------------------------------------------------
    // Read RGB Position
    //----------------------------------------------------
    QString tmpParam;
    if( !readFileParam(_PATH_SLIDE_HALOGEN_SENSITIVITIES,&tmpParam) )
    {
        if( !readFileParam(_PATH_SLIDE_FLUORESCENT,&tmpParam) )
        {
            funcShowMsgERROR("RGB Positions Not Found");
            return (void)false;
        }
    }
    int rPosX, gPosX, bPosX;
    //rPosX = tmpParam.split(",").at(0).toInt(0) - round((float)slideCubeSettings.width/2.0);
    //gPosX = tmpParam.split(",").at(1).toInt(0) - round((float)slideCubeSettings.width/2.0);
    //bPosX = tmpParam.split(",").at(2).toInt(0) - round((float)slideCubeSettings.width/2.0);
    rPosX = 500;
    gPosX = 500;
    bPosX = 500;

    //----------------------------------------------------
    // Create Image To Display
    //----------------------------------------------------
    int i, w, h, r, c, slideX, red, green, blue;
    w = slideCubeSettings.width;
    h = tmpImg.height()-1;
    for( i=0; i<lstFrames.size(); i++ )
    {
        tmpImg      = QImage( lstFrames.at(i).absoluteFilePath() );
        slideX      = i*w;
        for( r=0; r<h; r++ )
        {
            for(c=0; c<w; c++ )
            {
                red     = qRed(   tmpImg.pixel( rPosX+c, r ) );
                green   = qGreen( tmpImg.pixel( gPosX+c, r ) );
                blue    = qBlue(  tmpImg.pixel( bPosX+c, r ) );
                resultImg.setPixel( QPoint( slideX+c, r ), qRgb( red, green, blue ) );
            }
        }
    }
    if( slideCubeSettings.rotateLeft == true )
    {
        rotateQImage(&resultImg,90);
    }
    resultImg.save( _PATH_DISPLAY_IMAGE );
    updateDisplayImage(&resultImg);
}


int MainWindow::funcAccountFilesInFolder( QString folder )
{
    //Generate the command
    QString tmpCommand;
    tmpCommand.append("ls ");
    tmpCommand.append(folder);
    tmpCommand.append(" -a | wc -l");
    //qDebug() << "tmpCommand: " << tmpCommand;

    //Execute command
    QString numFrames;
    numFrames = funcExecuteCommandAnswer( (char*)tmpCommand.toStdString().data() );
    return numFrames.toInt(0);

}

void MainWindow::on_actionVideo_2_triggered()
{
    QString remoteVideoID;
    funcMainCall_RecordVideo(&remoteVideoID,false,true);
}

void MainWindow::on_actionTimelapse_2_triggered()
{

    funcStartRemoteTimelapse( false );

}

/*
void MainWindow::funcShowMsgERROR_Timeout(QString msg, int ms)
{
    QMessageBox *msgBox         = new QMessageBox(QMessageBox::Warning,"ERROR",msg,NULL);
    QTimer *msgBoxCloseTimer    = new QTimer(this);
    msgBoxCloseTimer->setInterval(ms);
    msgBoxCloseTimer->setSingleShot(true);
    connect(msgBoxCloseTimer, SIGNAL(timeout()), msgBox, SLOT(reject()));
    msgBoxCloseTimer->start();
    msgBox->exec();
}


void MainWindow::funcShowMsgSUCCESS_Timeout(QString msg, int ms)
{
    QMessageBox *msgBox         = new QMessageBox(QMessageBox::Information,"SUCCESS",msg,NULL);
    QTimer *msgBoxCloseTimer    = new QTimer(this);
    msgBoxCloseTimer->setInterval(ms);
    msgBoxCloseTimer->setSingleShot(true);
    connect(msgBoxCloseTimer, SIGNAL(timeout()), msgBox, SLOT(reject()));
    msgBoxCloseTimer->start();
    msgBox->exec();
}*/

void MainWindow::funcStartRemoteTimelapse( bool setROI )
{
    //---------------------------------------------------
    //Get Folder Destine
    //---------------------------------------------------
    QString folderID;
    folderID = funcGetParam("Folder-Name");
    if(folderID.isEmpty())
    {
        funcShowMsgERROR_Timeout("Invalid Folder",this);
        return (void)false;
    }

    //---------------------------------------------------
    //Validate File/Dir Name
    //---------------------------------------------------
    QString remoteFile, localFile;
    if( setROI == true )
    {
        remoteFile = _PATH_REMOTE_FOLDER_SLIDELAPSE + folderID + "/";

        //Local File
        QString syncFolder;
        syncFolder = funcGetSyncFolder();
        localFile.clear();
        localFile.append(syncFolder);
        localFile.append(_PATH_LOCAL_FOLDER_SLIDELAPSE);
        localFile.append(folderID);
        localFile.append("/");
    }
    else
    {
        remoteFile = _PATH_REMOTE_FOLDER_TIMELAPSE + folderID + "/";

        //Local File
        QString syncFolder;
        syncFolder = funcGetSyncFolder();
        localFile.clear();
        localFile.append(syncFolder);
        localFile.append(_PATH_LOCAL_FOLDER_TIMELAPSE);
        localFile.append(folderID);
        localFile.append("/");
    }
    int whereExists;
    whereExists = funcValidateFileDirNameDuplicated( remoteFile, localFile );
    if( whereExists == -1 )
    {
        funcShowMsgERROR_Timeout("ID Exists Locally: Please, use another",this);
        return (void)false;
    }
    if( whereExists == -2 )
    {
        funcShowMsgERROR_Timeout("ID Exists Remotelly: Please, use another",this);
        return (void)false;
    }

    //--------------------------------------
    //Create an Empty Remote Directory
    //--------------------------------------
    QString tmpCommand;
    bool commandExecuted;
    tmpCommand.clear();
    tmpCommand.append("mkdir "+remoteFile);
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,0,false,&commandExecuted);
    if( !commandExecuted )
    {
        funcShowMsgERROR_Timeout("Creating Remote Folder",this);
        return (void)false;
    }

    //--------------------------------------
    //Take Timelapse
    //--------------------------------------
    int triggeringTime;
    triggeringTime = ui->spinBoxTrigeringTime->value();
    tmpCommand = genTimelapseCommand(remoteFile,setROI);
    qDebug() << "tmpCommand: " << tmpCommand;
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,triggeringTime,false,&commandExecuted);
    if( !commandExecuted )
    {
        funcShowMsgERROR_Timeout("Starting Remote Timelapse",this);
        return (void)false;
    }

    //-----------------------------------------------------
    //Start Timer
    //-----------------------------------------------------
    funcDisplayTimer("Countdown to Recording...",triggeringTime,Qt::black);

}

void MainWindow::on_actionSnapvideos_triggered()
{
    funcStartRemoteTimelapse( true );
}

void MainWindow::on_actionSnapshot_triggered()
{
    funcMainCall_GetSnapshot();
}

void MainWindow::on_actionSynchronize_triggered()
{

    //-----------------------------------------------------
    // Get All New Files
    //-----------------------------------------------------
    int status;
    QString errorTrans, errorDel, syncLocalFolder;

    //Read Sync Destiny Folder
    syncLocalFolder = funcGetSyncFolder();

    //Videos
    status = obtainRemoteFolder( _PATH_REMOTE_FOLDER_VIDEOS, syncLocalFolder+_PATH_LOCAL_FOLDER_VIDEOS, &errorTrans, &errorDel );
    if( status == _ERROR )
    {
        funcShowMsgERROR_Timeout("Obtaining Videos",this);
        return (void)false;
    }

    //Timelapse
    status = obtainRemoteFolder( _PATH_REMOTE_FOLDER_TIMELAPSE, syncLocalFolder+_PATH_LOCAL_FOLDER_TIMELAPSE, &errorTrans, &errorDel );
    if( status == _ERROR )
    {
        funcShowMsgERROR_Timeout("Obtaining Timelapse",this);
        return (void)false;
    }

    //Snapshots
    status = obtainRemoteFolder( _PATH_REMOTE_FOLDER_SNAPSHOTS, syncLocalFolder+_PATH_LOCAL_FOLDER_SNAPSHOTS, &errorTrans, &errorDel );
    if( status == _ERROR )
    {
        funcShowMsgERROR_Timeout("Obtaining Snapshots",this);
        return (void)false;
    }

    //Slidelapses
    status = obtainRemoteFolder( _PATH_REMOTE_FOLDER_SLIDELAPSE, syncLocalFolder+_PATH_LOCAL_FOLDER_SLIDELAPSE, &errorTrans, &errorDel );
    if( status == _ERROR )
    {
        funcShowMsgERROR_Timeout("Obtaining Slidelapses",this);
        return (void)false;
    }

    //-----------------------------------------------------
    // Display Result
    //-----------------------------------------------------
    if( !errorTrans.isEmpty() || !errorDel.isEmpty() )
    {
        QString strDisplayStatus;
        strDisplayStatus.append("Some Files/Dirs Produced Errors...\n\n\n");
        strDisplayStatus.append("Transmission: \n\n" + errorTrans);
        strDisplayStatus.append("Delations: \n\n" + errorDel);
        funcShowMsgERROR_Timeout(strDisplayStatus,this);
    }
    else
    {
        funcShowMsg("SUCCESS","Remote Camera Syncronized Successfully");
    }

}


int MainWindow::funcValidateFileDirNameDuplicated(QString remoteFile, QString localFile)
{
    qDebug() << "remoteFile: " << remoteFile;
    qDebug() << "localFile: " << localFile;

    //Check if exists locally
    if( fileExists(localFile) )
    {
        qDebug() << "Exists Locally";
        return -1;
    }

    //Check if exists remotelly
    if( funcRaspFileExists( remoteFile.toStdString() ) == 1 )
    {
        qDebug() << "Exists Remotelly";
        return -2;
    }

    //File does not exists locally or remotelly
    return _OK;
}

void MainWindow::on_actionSync_Folder_triggered()
{
    //--------------------------------------------------
    // Read Parameter
    //--------------------------------------------------
    QString syncFolder;
    syncFolder = funcGetSyncFolder();

    //--------------------------------------------------
    // Require Folder Destiny
    //--------------------------------------------------
    QString newDir;
    newDir = funcShowSelDir(syncFolder);
    if( !newDir.isEmpty() )
    {
        saveFile(_PATH_LAST_SYNC_FOLDER,newDir+"/");
    }

}

QString MainWindow::funcGetSyncFolder()
{
    QString tmpParam;
    if( !readFileParam( _PATH_LAST_SYNC_FOLDER, &tmpParam) )
    {
        saveFile(_PATH_LAST_SYNC_FOLDER,_PATH_LOCAL_SYNC_FOLDERS);
        tmpParam.clear();
        tmpParam.append(_PATH_LOCAL_SYNC_FOLDERS);
    }
    return tmpParam;
}


void MainWindow::funcMainCall_RecordVideo(QString* videoID, bool defaultPath, bool ROI)
{
    bool commandExecuted;

    videoID->clear();

    //---------------------------------------------------
    //Get Video-ID Destine
    //---------------------------------------------------
    if( defaultPath )
    {
        videoID->append(_PATH_VIDEO_REMOTE_H264);
    }
    else
    {
        *videoID = funcGetParam("Video-ID");
        if(videoID->isEmpty())
        {
            funcShowMsgERROR_Timeout("Invalid Video-ID",this);
            return (void)false;
        }

        //---------------------------------------------------
        //Validate File/Dir Name
        //---------------------------------------------------
        QString remoteFile, localFile;
        remoteFile.append(_PATH_REMOTE_FOLDER_VIDEOS);
        remoteFile.append(*videoID);
        remoteFile.append(_VIDEO_EXTENSION);

        //Local File
        QString syncLocalFolder = funcGetSyncFolder();
        localFile.clear();
        localFile.append(syncLocalFolder);
        localFile.append(_PATH_REMOTE_FOLDER_VIDEOS);
        localFile.append(videoID);
        localFile.append(_VIDEO_EXTENSION);

        //Check if exists
        int itExists = funcValidateFileDirNameDuplicated( remoteFile, localFile );
        if( itExists != _OK )
        {
            if( itExists == -1 )
                funcShowMsgERROR_Timeout("Video-ID Exists Locally: Please, use another",this);
            else
                funcShowMsgERROR_Timeout("Video-ID Exists Remotelly: Please, use another",this);
            return (void)false;
        }

        //---------------------------------------------------
        //Prepare Remote Scenary
        //---------------------------------------------------

        //Delete Remote File if Exists
        *videoID = _PATH_REMOTE_FOLDER_VIDEOS + *videoID + _VIDEO_EXTENSION;
        QString tmpCommand;
        tmpCommand.clear();
        tmpCommand.append("sudo rm "+ *videoID);
        funcRemoteTerminalCommand(
                                    tmpCommand.toStdString(),
                                    camSelected,
                                    0,
                                    false,
                                    &commandExecuted
                                );
        if( !commandExecuted )
        {
            funcShowMsgERROR_Timeout("Deleting Remote videoID",this);
            return (void)false;
        }
    }

    //-----------------------------------------------------
    // Save snapshots settings
    //-----------------------------------------------------
    // Get camera resolution
    int camMP       = (ui->radioRes5Mp->isChecked())?5:8;
    camRes          = getCamRes(camMP);

    //Getting calibration
    lstDoubleAxisCalibration daCalib;
    funcGetCalibration(&daCalib);

    //Save lastest settings
    if( saveRaspCamSettings( _PATH_LAST_SNAPPATH ) == false ){
        funcShowMsg("ERROR","Saving last snap-settings");
        return (void)false;
    }

    //-----------------------------------------------------
    //Start to Record Remote Video
    //-----------------------------------------------------

    // Generate Video Command
    QString getRemVidCommand = genRemoteVideoCommand(*videoID,ROI);

    // Execute Remote Command
    funcRemoteTerminalCommand(
                                getRemVidCommand.toStdString(),
                                camSelected,
                                ui->spinBoxTrigeringTime->value(),
                                false,
                                &commandExecuted
                            );
    if( !commandExecuted )
    {
        funcShowMsgERROR_Timeout("Starting Remote Recording",this);
        return (void)false;
    }

    //-----------------------------------------------------
    //Display Timer
    //-----------------------------------------------------
    //Before to Start Recording
    funcDisplayTimer("Countdown to Recording...",ui->spinBoxTrigeringTime->value(),Qt::black);

    //During Recording
    //funcDisplayTimer("Recording...",ui->spinBoxVideoDuration->value(),Qt::red);
}

void MainWindow::funcMainCall_GetSnapshot()
{
    //---------------------------------------------------
    //Get Snapshot-ID Destine
    //---------------------------------------------------
    QString fileName;
    fileName = funcGetParam("Snapshot-ID");
    if(fileName.isEmpty())
    {
        funcShowMsgERROR_Timeout("Invalid Snapshot-ID",this);
        return (void)false;
    }

    //---------------------------------------------------
    //Validate File/Dir Name
    //---------------------------------------------------
    QString remoteFile, localFile;
    remoteFile = _PATH_REMOTE_FOLDER_SNAPSHOTS + fileName + _SNAPSHOT_REMOTE_EXTENSION;

    //Local File
    QString syncFolder;
    syncFolder = funcGetSyncFolder();
    localFile.clear();
    localFile.append(syncFolder);
    localFile.append(_PATH_LOCAL_FOLDER_SNAPSHOTS);
    localFile.append(fileName);
    localFile.append(_SNAPSHOT_REMOTE_EXTENSION);

    if( !funcValidateFileDirNameDuplicated( remoteFile, localFile ) )
    {
        funcShowMsgERROR_Timeout("Snapshot-ID Exists: Please, use another",this);
        return (void)false;
    }

    //--------------------------------------
    //Prepare Remote Scenary
    //--------------------------------------

    //Delete Remote File if Exists
    fileName = _PATH_REMOTE_FOLDER_SNAPSHOTS + fileName + _SNAPSHOT_REMOTE_EXTENSION;
    QString tmpCommand;
    tmpCommand.clear();
    tmpCommand.append("sudo rm "+ fileName);
    bool commandExecuted;
    funcRemoteTerminalCommand(tmpCommand.toStdString(),camSelected,0,false,&commandExecuted);
    if( !commandExecuted )
    {
        funcShowMsgERROR_Timeout("Deleting Remote Snapshot-ID",this);
        return (void)false;
    }

    //-----------------------------------------------------
    // Save snapshots settings
    //-----------------------------------------------------

    // Get camera resolution
    int camMP       = (ui->radioRes5Mp->isChecked())?5:8;
    camRes          = getCamRes(camMP);

    //Getting calibration
    lstDoubleAxisCalibration daCalib;
    funcGetCalibration(&daCalib);

    //Save lastest settings
    if( saveRaspCamSettings( _PATH_LAST_SNAPPATH ) == false )
    {
        funcShowMsgERROR_Timeout("Saving last Snap-settings",this);
        return (void)false;
    }

    //-----------------------------------------------------
    // Take Remote Snapshot
    //-----------------------------------------------------
    if( !takeRemoteSnapshot(fileName,false) )
    {
        funcShowMsgERROR_Timeout("Taking Full Area Snapshot",this);
        return (void)false;
    }

    //-----------------------------------------------------
    //Start Timer
    //-----------------------------------------------------
    int triggeringTime;
    triggeringTime = ui->spinBoxTrigeringTime->value();
    if( triggeringTime > 0 )
    {
        formTimerTxt* timerTxt = new formTimerTxt(this,"Remainning Time to Shoot...",triggeringTime);
        timerTxt->setModal(true);
        timerTxt->show();
        QtDelay(200);
        timerTxt->startMyTimer(triggeringTime);
    }
}

void MainWindow::on_actionFull_Video_triggered()
{
    //Record a video and extracts frames

    bool ok;

    //-----------------------------------------------------
    // Record Remote Video
    //-----------------------------------------------------
    QString videoID;
    funcMainCall_RecordVideo(&videoID,true,false);

    //-----------------------------------------------------
    // Obtain Remote Video
    //-----------------------------------------------------
    if( obtainFile( _PATH_VIDEO_REMOTE_H264, _PATH_VIDEO_RECEIVED_H264, "Transmitting Remote Video" ) )
    {
        QString rmRemVidCommand;
        rmRemVidCommand.append("rm ");
        rmRemVidCommand.append(_PATH_VIDEO_REMOTE_H264);
        funcRemoteTerminalCommand( rmRemVidCommand.toStdString(), camSelected, 0, false, &ok );
    }
    else
    {
        return (void)false;
    }

    //-----------------------------------------------------
    // Extract Frames from Video
    //-----------------------------------------------------

    //Clear local folder
    QString tmpFramesPath;
    tmpFramesPath.append(_PATH_VIDEO_FRAMES);
    tmpFramesPath.append("tmp/");
    funcClearDirFolder( tmpFramesPath );

    //Extract Frames from Local Video
    QString locFrameExtrComm;
    locFrameExtrComm.append("ffmpeg -framerate ");
    locFrameExtrComm.append(QString::number(_VIDEO_FRAME_RATE));
    locFrameExtrComm.append(" -r ");
    locFrameExtrComm.append(QString::number(_VIDEO_FRAME_RATE));
    locFrameExtrComm.append(" -i ");
    locFrameExtrComm.append(_PATH_VIDEO_RECEIVED_H264);
    locFrameExtrComm.append(" ");
    locFrameExtrComm.append(tmpFramesPath);
    locFrameExtrComm.append("%07d");
    locFrameExtrComm.append(_FRAME_EXTENSION);
    qDebug() << locFrameExtrComm;
    progBarUpdateLabel("Extracting Frames from Video",0);
    if( !funcExecuteCommand(locFrameExtrComm) )
    {
        funcShowMsg("ERROR","Extracting Frames from Video");
    }
    progBarUpdateLabel("",0);


    //-----------------------------------------------------
    // Update mainImage from Frames
    //-----------------------------------------------------
    //funcUpdateImageFromFolder(tmpFramesPath);



    funcResetStatusBar();
}


int MainWindow::funcDisplayTimer(QString title, int timeMs, QColor color)
{
    if( timeMs > 0 )
    {
        formTimerTxt* timerTxt = new formTimerTxt(this,title,timeMs,color);
        timerTxt->setModal(true);
        timerTxt->show();
        QtDelay(200);
        timerTxt->startMyTimer(timeMs);
    }
    return 1;
}

int MainWindow::func_MultiImageMerge( QString type )
{
    mouseCursorWait();

    QStringList lstImages;
    if( func_getMultiImages(&lstImages, this) == _OK && lstImages.size() < 2 )
    {
        funcShowMsgERROR_Timeout("Insufficient Selected Images",this,2000);
        mouseCursorReset();
        return _ERROR;
    }
    else
    {
        QImage imgDestine(lstImages.first());
        lstImages.removeFirst();

        //Merge Images
        while( !lstImages.isEmpty() )
        {
            QImage imgSource(lstImages.first());
            lstImages.removeFirst();
            if( func_MergeImages(&imgSource,&imgDestine,type) == _ERROR )
            {
                mouseCursorReset();
                return _ERROR;
            }
        }
        //Get Filename
        mouseCursorReset();
        QString fileName;
        if(
                func_getFilenameFromUser(
                                            &fileName,
                                            _PATH_LAST_LINE_SAVED,
                                            "./snapshots/",
                                            this
                                        ) == _OK
        ){
            mouseCursorWait();
            imgDestine.save(fileName);
            mouseCursorReset();
        }
        return _OK;
    }
}

void MainWindow::on_actionMultiImageAverage_triggered()
{
    if( func_MultiImageMerge("Average") == _ERROR )
    {
        funcShowMsgERROR("Merging Images");
    }
}

void MainWindow::on_actionMultiImageMinimum_triggered()
{
    if( func_MultiImageMerge("Minimum") == _ERROR )
    {
        funcShowMsgERROR("Merging Images");
    }
}

void MainWindow::on_actionMultiImageMaximum_triggered()
{
    if( func_MultiImageMerge("Maximum") == _ERROR )
    {
        funcShowMsgERROR("Merging Images");
    }
}

void MainWindow::on_actionSlide_Settings_triggered()
{
    //Get Slide Area
    QString slideArea;
    slideArea = readAllFile(_PATH_SLIDE_DIFFRACTION);
    if( slideArea == _ERROR_FILE_NOTEXISTS || slideArea == _ERROR_FILE )
    {
        funcShowMsgERROR_Timeout(slideArea,this);
        return (void)false;
    }


    //Create File-Contain
    QString fileContain;
    fileContain.append("echo '");
    fileContain.append(slideArea);
    fileContain.append("' > ");
    fileContain.append(_PATH_REMOTE_TMPSETTINGS);

    //Excecute Remote Command
    bool ok;
    funcRemoteTerminalCommand( fileContain.toStdString(), camSelected, 0, false, &ok );
    if(ok==true)
        funcShowMsgSUCCESS_Timeout("Settings Send",this);
    else
        funcShowMsgERROR_Timeout("Sending Settings",this);





}

void MainWindow::on_actionLinear_Regression_triggered()
{
    formGenLinearRegression* tmpForm = new formGenLinearRegression(this);
    tmpForm->setModal(true);
    tmpForm->show();
}

void MainWindow::on_actionDiffraction_Origin_triggered()
{
    //Get Filename
    QString filePath = QFileDialog::getOpenFileName(
                                                        this,
                                                        tr("Select File..."),
                                                        _PATH_CALIB,
                                                        "(*.hypcam);;"
                                                     );
    //Add Row
    if( !filePath.isEmpty() )
    {
        if( saveFile(_PATH_CALIB_LR_TMP_ORIGIN,filePath) == false )
        {
            funcShowMsgERROR("Saving: " + QString(_PATH_CALIB_LR_TMP_ORIGIN) );
        }
        else
            funcShowMsgSUCCESS_Timeout("Diffraction Origin Set",this);
    }
    else
    {
        funcShowMsgERROR("Origin was not Set");
    }
}

void MainWindow::on_openLine_triggered()
{
    //---------------------------------------
    //Let the user select the file
    //---------------------------------------
    QString fileOrigin;
    if( funcLetUserSelectFile(&fileOrigin,"Select Line...",this) != _OK )
    {
        return (void)false;
    }

    //---------------------------------------
    //Draw Line
    //---------------------------------------
    funcDrawLineIntoCanvasEdit(fileOrigin);
}

void MainWindow::on_actionSlide_Linear_Regression_triggered()
{
    formSlideLinearRegression* tmpForm = new formSlideLinearRegression(this);
    tmpForm->setModal(true);
    tmpForm->show();
}

void MainWindow::on_actionarcLine_triggered()
{
    funcShowMsg_Timeout("Ups!!","Command not implemented yet.",this);
}

bool MainWindow::funcDrawLineIntoCanvasEdit(const QString &filePath)
{
    //---------------------------------------
    //Read the line from XML
    //---------------------------------------
    structLine tmpLineData;
    if( funcReadLineFromXML((QString*)&filePath, &tmpLineData) != _OK )
    {
        funcShowMsgERROR_Timeout("Opening: " + filePath, this);
        return _ERROR;
    }

    //---------------------------------------
    //Draw Line into canvasEdit
    //---------------------------------------    
    tmpLineData.x1 = round(
                            (
                                (float)tmpLineData.x1 /
                                (float)tmpLineData.originalW
                            ) * (float)canvasCalib->width()
                          );
    tmpLineData.y1 = round(
                            (
                                (float)tmpLineData.y1 /
                                (float)tmpLineData.originalH
                            ) * (float)canvasCalib->height()
                          );
    tmpLineData.x2 = round(
                            (
                                (float)tmpLineData.x2 /
                                (float)tmpLineData.originalW
                            ) * (float)canvasCalib->width()
                          );
    tmpLineData.y2 = round(
                            (
                                (float)tmpLineData.y2 /
                                (float)tmpLineData.originalH
                            ) * (float)canvasCalib->height()
                          );
    QPoint p1(tmpLineData.x1,tmpLineData.y1);
    QPoint p2(tmpLineData.x2,tmpLineData.y2);
    customLine* tmpLine = new customLine(
                                            p1,
                                            p2,
                                            QPen(QColor(qRgb(
                                                                tmpLineData.colorR,
                                                                tmpLineData.colorG,
                                                                tmpLineData.colorB
                                            )))
                                        );
    tmpLine->parameters.orientation = tmpLineData.oritation;
    tmpLine->parameters.originalW   = tmpLineData.originalW;
    tmpLine->parameters.originalH   = tmpLineData.originalH;
    tmpLine->parameters.wavelength  = tmpLineData.wavelength;
    tmpLine->parentSize.setWidth(canvasCalib->scene()->width());
    tmpLine->parentSize.setHeight(canvasCalib->scene()->height());
    canvasCalib->scene()->addItem( tmpLine );
    canvasCalib->update();
    return _OK;

}

void MainWindow::on_actionPlot_Calculated_Line_triggered()
{
    //---------------------------------------
    //Get Line from User
    //---------------------------------------
    //Get filename
    QString fileName;
    if( funcLetUserSelectFile(&fileName,"Select Horizontal Calibration...",this) != _OK )
    {
        funcShowMsgERROR_Timeout("Getting File from User");
        return (void)false;
    }
    //Get Line parameters from .XML
    structHorizontalCalibration horizCalib;
    funcReadHorizCalibration(&fileName,&horizCalib);

    //---------------------------------------
    //Plot Calculated Line
    //---------------------------------------
    int newX, newY;
    for(newX=1; newX<canvasCalib->width(); newX+=5)
    {
        newY    = round( (horizCalib.b*newX) + horizCalib.a );
        //std::cout << "(" << horizCalib.b << "*" << newX << ") + " << horizCalib.a << " = " << newY << std::endl;
        newY    = round( ((float)newY/(float)horizCalib.imgH) * (float)canvasCalib->height());
        double rad = 1.0;
        QGraphicsEllipseItem* item = new QGraphicsEllipseItem(newX, newY, rad, rad );
        item->setPen( QPen(Qt::white) );
        item->setBrush( QBrush(Qt::white) );
        canvasCalib->scene()->addItem(item);
    }

}

void MainWindow::on_actionPlot_Upper_Line_Rotation_triggered()
{
    //---------------------------------------
    //Get Line from User
    //---------------------------------------
    //Get filename
    QString fileName;
    if( funcLetUserSelectFile(&fileName,"Select Vertical Calibration",this) != _OK )
    {
        funcShowMsgERROR_Timeout("Getting File from User");
        return (void)false;
    }
    //Get Calibration Parameters from .XML
    structVerticalCalibration tmpVertCal;
    funcReadVerticalCalibration(&fileName,&tmpVertCal);

    //---------------------------------------
    //Plot Calculated Line
    //---------------------------------------
    int newX, newY;
    for(newY=1; newY<canvasCalib->height(); newY+=5)
    {
        newX    = (tmpVertCal.vertLR.b*newY) + tmpVertCal.vertLR.a;
        newX    = ((float)newX/(float)tmpVertCal.imgW) * canvasCalib->width();//Fit to Canvas size
        double rad = 1.0;
        QGraphicsEllipseItem* item = new QGraphicsEllipseItem(newX, newY, rad, rad );
        item->setPen( QPen(Qt::white) );
        item->setBrush( QBrush(Qt::white) );
        canvasCalib->scene()->addItem(item);
    }

}

/*
int MainWindow::funcReadVerticalCalibration(
                                                QString* filePath,
                                                structVerticalCalibration* vertCal
){
    //---------------------------------------
    //Extract XML
    //---------------------------------------
    QFile *xmlFile = new QFile(*filePath);
    if (!xmlFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        funcShowMsg("ERROR","Opening: "+*filePath);
        return _ERROR;
    }
    QXmlStreamReader *xmlReader = new QXmlStreamReader(xmlFile);


    //Parse the XML until we reach end of it
    while(!xmlReader->atEnd() && !xmlReader->hasError())
    {
        // Read next element
        QXmlStreamReader::TokenType token = xmlReader->readNext();
        //If token is just StartDocument - go to next
        if(token == QXmlStreamReader::StartDocument)
        {
            continue;
        }
        //If token is StartElement - read it
        if(token == QXmlStreamReader::StartElement)
        {
            if( xmlReader->name()=="imgW" )
                vertCal->imgW = xmlReader->readElementText().trimmed().toInt(0);
            if( xmlReader->name()=="imgH" )
                vertCal->imgH = xmlReader->readElementText().trimmed().toInt(0);
            if( xmlReader->name()=="x1" )
                vertCal->x1 = xmlReader->readElementText().trimmed().toInt(0);
            if( xmlReader->name()=="y1" )
                vertCal->y1 = xmlReader->readElementText().trimmed().toInt(0);
            if( xmlReader->name()=="x2" )
                vertCal->x2 = xmlReader->readElementText().trimmed().toInt(0);
            if( xmlReader->name()=="y2" )
                vertCal->y2 = xmlReader->readElementText().trimmed().toInt(0);
            if( xmlReader->name()=="lowerBoundWave" )
                vertCal->minWave = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="higherBoundWave" )
                vertCal->maxWave = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="wavelengthA" )
                vertCal->wavelengthLR.a = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="wavelengthB" )
                vertCal->wavelengthLR.b = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="dist2WaveA" )
                vertCal->dist2WaveLR.a = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="dist2WaveB" )
                vertCal->dist2WaveLR.b = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="wave2DistA" )
                vertCal->wave2DistLR.a = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="wave2DistB" )
                vertCal->wave2DistLR.b = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="vertA" )
                vertCal->vertLR.a = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="vertB" )
                vertCal->vertLR.b = xmlReader->readElementText().trimmed().toFloat(0);
        }
    }
    if(xmlReader->hasError()) {
        funcShowMsg("Parse Error",xmlReader->errorString());
        return _ERROR;
    }
    xmlReader->clear();
    xmlFile->close();

    return _OK;
}*/

int MainWindow::funcReadHorizCalibration(QString* filePath, structHorizontalCalibration* horizCalib)
{
    //---------------------------------------
    //Extract XML
    //---------------------------------------
    QFile *xmlFile = new QFile(*filePath);
    if (!xmlFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        funcShowMsg("ERROR","Opening: "+*filePath);
        return _ERROR;
    }
    QXmlStreamReader *xmlReader = new QXmlStreamReader(xmlFile);


    //Parse the XML until we reach end of it
    while(!xmlReader->atEnd() && !xmlReader->hasError())
    {
        // Read next element
        QXmlStreamReader::TokenType token = xmlReader->readNext();
        //If token is just StartDocument - go to next
        if(token == QXmlStreamReader::StartDocument)
        {
            continue;
        }
        //If token is StartElement - read it
        if(token == QXmlStreamReader::StartElement)
        {
            if( xmlReader->name()=="imgW" )
                horizCalib->imgW = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="imgH" )
                horizCalib->imgH = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="H" )
                horizCalib->H = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="a" )
                horizCalib->a = xmlReader->readElementText().trimmed().toFloat(0);
            if( xmlReader->name()=="b" )
                horizCalib->b = xmlReader->readElementText().trimmed().toFloat(0);
        }
    }
    if(xmlReader->hasError()) {
        funcShowMsg("Parse Error",xmlReader->errorString());
        return _ERROR;
    }
    xmlReader->clear();
    xmlFile->close();

    return _OK;
}

void MainWindow::on_actionOrigin_triggered()
{
    //---------------------------------------
    //Get Horizontal Calibration
    //---------------------------------------
    //Get filename
    QString fileName;
    if( funcLetUserSelectFile(&fileName, "Select Horizontal Calibration",this) != _OK )
    {
        funcShowMsgERROR_Timeout("Getting Horizontal Calibration");
        return (void)false;
    }
    //Get Line parameters from .XML
    structHorizontalCalibration tmpHorizCal;
    funcReadHorizCalibration(&fileName,&tmpHorizCal);

    //---------------------------------------
    //Get Vertical Calibration
    //---------------------------------------
    //Get filename
    if( funcLetUserSelectFile(&fileName,"Select Vertical Calibration",this) != _OK )
    {
        funcShowMsgERROR_Timeout("Getting Vertical Calibration");
        return (void)false;
    }
    //Get Calibration Parameters from .XML
    structVerticalCalibration tmpVertCal;
    funcReadVerticalCalibration(&fileName,&tmpVertCal);

    //---------------------------------------
    //Calc Origin
    //---------------------------------------
    QPoint origin = originFromCalibration( tmpHorizCal, tmpVertCal );
    std::cout << "oX: " << origin.x() << " oY: " << origin.y() << std::endl;


    //---------------------------------------
    //Add Big Point
    //---------------------------------------
    float x, y;
    x   = origin.x();
    y   = origin.y();
    x   = round( ((float)x/(float)tmpHorizCal.imgW) * (float)canvasCalib->width() );
    y   = round( ((float)y/(float)tmpHorizCal.imgH) * (float)canvasCalib->height() );
    //std::cout << x << "," << y << std::endl;
    double rad = 10.0;
    QGraphicsEllipseItem* item = new QGraphicsEllipseItem(x-(rad/2.0), y-(rad/2.0), rad, rad );
    item->setPen( QPen(Qt::magenta) );
    item->setBrush( QBrush(Qt::white) );
    canvasCalib->scene()->addItem(item);

}

void MainWindow::on_actionBuld_HypImg_triggered()
{












    /*
    //--------------------------------
    //Read Calibrations
    //--------------------------------
    QString calFilename;
    calFilename = readAllFile(_PATH_SLIDE_ACTUAL_CALIB_PATH).trimmed();
    std::cout << "fileContain: " << calFilename.toStdString() << std::endl;
    structSlideCalibration slideCalibration;
    if( funcReadSlideCalib(calFilename,&slideCalibration) != _OK )
    {
        funcShowMsgERROR_Timeout("Reading Slide Calibration");
        return (void)false;
    }

    //--------------------------------
    //Define Video Origin
    //--------------------------------
    QString videoPath;
    videoPath.append(_PATH_LOCAL_SYNC_FOLDERS);
    videoPath.append(_PATH_LOCAL_FOLDER_VIDEOS);
    if(
            funcLetUserSelectFile(
                                    &videoPath,
                                    "Select Hyperspectral Image Source..."
                                 ) != _OK
    ){
        return (void)false;
    }

    //-----------------------------------------------------
    // Extract Frames from Video
    //-----------------------------------------------------
    //Clear local folder
    QString tmpFramesPath;
    tmpFramesPath.append(_PATH_VIDEO_FRAMES);
    tmpFramesPath.append("tmp/");
<<<<<<< HEAD
    if( 1 )
    {
        funcClearDirFolder( tmpFramesPath );
        //Extract Frames from Local Video
        QString locFrameExtrComm;
        locFrameExtrComm.append("ffmpeg -framerate ");
        locFrameExtrComm.append(QString::number(_VIDEO_FRAME_RATE));
        locFrameExtrComm.append(" -r ");
        locFrameExtrComm.append(QString::number(_VIDEO_FRAME_RATE));
        locFrameExtrComm.append(" -i ");
        locFrameExtrComm.append(videoPath);
        locFrameExtrComm.append(" ");
        locFrameExtrComm.append(tmpFramesPath);
        locFrameExtrComm.append("%07d");
        locFrameExtrComm.append(_FRAME_EXTENSION);
        qDebug() << locFrameExtrComm;
        progBarUpdateLabel("Extracting Frames from Video",0);
        if( funcExecuteCommand(locFrameExtrComm) != _OK )
        {
            funcShowMsg("ERROR","Extracting Frames from Video");
            progBarUpdateLabel("",0);
            return (void)false;
        }
=======
    funcClearDirFolder( tmpFramesPath );
    //Extract Frames from Local Video
    QString locFrameExtrComm;
    locFrameExtrComm.append("ffmpeg -framerate ");
    locFrameExtrComm.append(QString::number(_VIDEO_FRAME_RATE));
    locFrameExtrComm.append(" -r ");
    locFrameExtrComm.append(QString::number(_VIDEO_FRAME_RATE));
    locFrameExtrComm.append(" -i ");
    locFrameExtrComm.append(videoPath);
    locFrameExtrComm.append(" ");
    locFrameExtrComm.append(tmpFramesPath);
    locFrameExtrComm.append("%d");
    locFrameExtrComm.append(_FRAME_EXTENSION);
    qDebug() << locFrameExtrComm;
    progBarUpdateLabel("Extracting Frames from Video",0);
    if( funcExecuteCommand(locFrameExtrComm) != _OK )
    {
        funcShowMsg("ERROR","Extracting Frames from Video");
>>>>>>> refs/heads/master
        progBarUpdateLabel("",0);
        return (void)false;
    }
    progBarUpdateLabel("",0);

    //--------------------------------
    //List Files in Folder
    //--------------------------------
    QList<QFileInfo> lstFrames = funcListFilesInDir(tmpFramesPath);
    std::cout << "numFrames: " << lstFrames.size() << std::endl;

    //--------------------------------
    //Update Progress Bar
    //--------------------------------
    progBarUpdateLabel("Building Slide Hyperspectral Image",0);
    ui->progBar->setVisible(true);
    ui->progBar->setValue(0);
    ui->progBar->update();
    QtDelay(10);

    //============================================================
    //Build Slide Hyperspectral Image
    //============================================================
    int x, y, z;
    int hypX    = lstFrames.size();
    int hypY    = slideCalibration.originH;
    int hypZ    = slideCalibration.maxNumCols;

    //--------------------------------
    //Reserve HypImg Dynamic Memory
    //--------------------------------
    int*** HypImg;//[hypX][hypY][hypZ];
    HypImg = (int***)malloc(hypX*sizeof(int**));
    for(x=0; x<hypX; x++)
    {
        HypImg[x] = (int**)malloc( hypY*sizeof(int*) );
        for(y=0; y<hypY; y++)
        {
            HypImg[x][y] = (int*)malloc( hypZ*sizeof(int) );
        }
    }

    //--------------------------------
    //Copy values int HypImg
    //--------------------------------
    QImage tmpActImg;    
    float pixQE;
    for(x=0; x<hypX; x++)
    {
        //Load Image (Column in the HypImg)
        tmpActImg = QImage(lstFrames.at(x).absoluteFilePath().trimmed());

        //Update Progress Bar
        ui->progBar->setValue(100 - round( ((float)x/(float)hypX)*100.0 ));
        ui->progBar->update();

        //Copy Diffraction Into Slide Hyperspectral Image
        for(y=0; y<hypY; y++)
        {
            for(z=0; z<hypZ; z++)
            {
                pixQE = 0.0;
                if(
                        funcGetPixQE(
                                        z,
                                        y,
                                        &pixQE,
                                        tmpActImg,
                                        slideCalibration
                                    ) != _OK
                ){
                    funcShowMsgERROR_Timeout("Pixel Coordinates Out of Range");
                    //Free Dynamic Memory
                    for(x=0; x<hypX; x++)
                    {
                        for(y=0; y<hypY; y++)
                        {
                            free( HypImg[x][y] );
                        }
                        free( HypImg[x] );
                    }
                    free(HypImg);
                    //Reset Progress Bar
                    funcResetStatusBar();
                    //Finish
                    return (void)false;
                }
                HypImg[x][y][z] = (int)pixQE;
            }
        }
    }

    //--------------------------------
    //Let User Select Folder
    //--------------------------------
    QString lastPath = readAllFile(_PATH_LAST_PATH_OPENED);
    QString slideHypDestiny;
    if(
            funcShowSelDir(
                                "Select Slide HypCube Destiny Directory...",
                                lastPath,
                                &slideHypDestiny
                          ) != _OK
    ){
        slideHypDestiny.clear();
        slideHypDestiny.append(_PATH_LOCAL_SLIDE_HYPIMG);
        funcShowMsg(
                        "Alert",
                        "Slide HypCube Saved into: "+
                        slideHypDestiny,
                        this
                   );
    }
    funcClearDirFolder(slideHypDestiny);

    //--------------------------------
    //Save Slide HypImg Layers
    //--------------------------------
    //Update Progress Bar
    progBarUpdateLabel("Exporting Hyperspectral Image",0);
    ui->progBar->setVisible(true);
    ui->progBar->setValue(0);
    ui->progBar->update();
    QtDelay(10);
    //Clear folder destine
    funcClearDirFolder( slideHypDestiny );
    //Copy Layer into Image and Save Later
    QString imgOutname;
    QImage tmpLayer(QSize(hypX,hypY),QImage::Format_RGB32);
    for(z=0; z<hypZ; z++)
    {
        //Update Progbar
        ui->progBar->setValue(round( ((float)z/(float)hypZ)*100.0 ));
        ui->progBar->update();
        //Fill Image Pixels
        for(x=0; x<hypX; x++)
        {
            for(y=0; y<hypY; y++)
            {
                tmpLayer.setPixelColor(x,y,QColor(HypImg[x][y][z],HypImg[x][y][z],HypImg[x][y][z]));
            }
        }
        //Image Horizontal Mirrored
        tmpLayer = tmpLayer.mirrored(true,false);
        //Save image
        imgOutname.clear();
        imgOutname.append(slideHypDestiny);
        imgOutname.append(QString::number(z+1));
        imgOutname.append(_FRAME_EXTENSION);
        tmpLayer.save(imgOutname);
    }

    //--------------------------------
    //Free Dynamic Memory
    //--------------------------------
    for(x=0; x<hypX; x++)
    {
        for(y=0; y<hypY; y++)
        {
            free( HypImg[x][y] );
        }
        free( HypImg[x] );
    }
    free(HypImg);

    //Reset Progress Bar
    funcResetStatusBar();
    */

    //Finish
    funcShowMsgSUCCESS_Timeout("Hyperspectral Image Exported Successfully");

}

void MainWindow::on_actionMerge_Calibration_triggered()
{
    formMergeSlideCalibrations* tmpForm = new formMergeSlideCalibrations(this);
    tmpForm->setModal(true);
    tmpForm->show();
}


int MainWindow::getCamMP()
{
    if( ui->radioRes5Mp->isChecked() )
        return 5;
    if( ui->radioRes8Mp->isChecked() )
        return 8;
    return 8;
}

void MainWindow::on_actionPlot_over_Real_triggered()
{
    //----------------------------------------------
    //Let the user select Slide Calibration File
    //----------------------------------------------
    QString calibPath;
    if( funcLetUserSelectFile(&calibPath,"Select Slide Calibration File...",this) != _OK )
    {
        return (void)false;
    }

    //----------------------------------------------
    //Read Slide Calibration File
    //----------------------------------------------
    structSlideCalibration slideCalibration;//_PATH_SLIDE_ACTUAL_CALIB_PATH
    if( funcReadSlideCalib( calibPath, &slideCalibration ) != _OK )
    {
        funcShowMsgERROR_Timeout("Reading Slide Calibration File");
        return (void)false;
    }
    //funcShowMsg("Fill",QString::number(slideCalibration.sensitivities.filled),this);
    //funcShowMsg("origR[0]",QString::number( slideCalibration.sensitivities.originalSR.at(0) ),this);
    //return (void)false;

    //**********************************************
    //Draw Over Real Image
    //**********************************************

    //----------------------------------------------
    //Apply Affine Transformation
    //----------------------------------------------
    *globalEditImg  = globalEditImg->transformed(slideCalibration.translation);

    //----------------------------------------------
    //Validate Calibration-Size and Image-Size
    //----------------------------------------------
    if(
            slideCalibration.imgW != globalEditImg->width()     ||
            slideCalibration.imgH != globalEditImg->height()
    ){
        funcShowMsgERROR("Image Size and Clibration Size are Different canvImage("+
                         QString::number(slideCalibration.imgW)+
                         ","+
                         QString::number(slideCalibration.imgH)+
                         ") imgCalib("+
                         QString::number(globalEditImg->width())+
                         ","+
                         QString::number(globalEditImg->height())+
                         ")"
                         );
        return (void)false;
    }

    //----------------------------------------------
    //Draw Vertical Lower Bound Line
    //----------------------------------------------
    QPoint newPoint;
    int x, y;
    x=0;
    for( y=0; y<=slideCalibration.originH; y++ )
    {
        newPoint = funcGetCoor(x,y,slideCalibration);
        globalEditImg->setPixelColor(newPoint,QColor(255,255,255));
    }

    //----------------------------------------------
    //Draw Vertical Upper Bound Line
    //----------------------------------------------
    x=slideCalibration.maxNumCols;
    for( y=0; y<=slideCalibration.originH; y++ )
    {
        newPoint = funcGetCoor(x,y,slideCalibration);
        globalEditImg->setPixelColor(newPoint,QColor(255,255,255));
    }

    //----------------------------------------------
    //Draw Horizontal Upper Line
    //----------------------------------------------
    int x1, y1, x2, y2, len;
    y=0;
    for( x=0; x<=slideCalibration.maxNumCols; x++ )
    {
        newPoint = funcGetCoor(x,y,slideCalibration);
        globalEditImg->setPixelColor(newPoint,QColor(255,255,255));
    }

    //----------------------------------------------
    //Draw Horizontal Lower Line
    //----------------------------------------------
    y = slideCalibration.originH;
    for( x=0; x<=slideCalibration.maxNumCols; x++ )
    {
        QPoint newPoint = funcGetCoor(x,y,slideCalibration);
        globalEditImg->setPixelColor(newPoint,QColor(255,255,255));
    }

    //----------------------------------------------
    //Draw Origen
    //----------------------------------------------
    len  = 2;
    x1   = slideCalibration.originX - len;
    y1   = slideCalibration.originY - len;
    x2   = slideCalibration.originX + len;
    y2   = slideCalibration.originY + len;
    for( x=x1; x<=x2; x++ )
    {
        for( y=y1; y<=y2; y++ )
        {
            globalEditImg->setPixelColor(x,y,QColor(255,255,255));
        }
    }

    //----------------------------------------------
    //Draw Internal in order to check
    //----------------------------------------------
    x=slideCalibration.maxNumCols;
    for( x=0; x<=slideCalibration.maxNumCols; x+=10 )
    {
        for( y=0; y<=slideCalibration.originH; y+=10 )
        {
            newPoint = funcGetCoor(x,y,slideCalibration);
            globalEditImg->setPixelColor(newPoint,QColor(255,0,0));
        }
    }

    //----------------------------------------------
    //Update Image Displayed in Canvas
    //----------------------------------------------
    updateDisplayImage(globalEditImg);

    //----------------------------------------------
    //Save Image
    //----------------------------------------------
    if( funcShowMsgYesNo( "Saving Image", "Save image?", this ) )
    {
        //filePath:         File output, filename selected by the user
        //title:            Showed to User, what kind of file is the user selecting
        //pathLocation:     Where is saved the last path location saved
        //pathOfInterest:   If it is the first time, what path will be saved as default
        //parent:           In order to use this Dialog
        QString fileName;
        if(
                funcLetUserDefineFile(
                                        &fileName,
                                        "Define Image Filename",
                                        ".png",
                                        new QString(_PATH_LAST_IMG_OPEN),
                                        new QString(_PATH_LAST_IMG_OPEN),
                                        this
                                     ) != _OK
        ){
            return (void)false;
        }
        //Save Image
        globalEditImg->save(fileName);
        //Notify Success
        funcShowMsgSUCCESS_Timeout("Image Saved Successfully");
    }
}

void MainWindow::on_actionPlot_Line_at_Wavelength_triggered()
{
    //----------------------------------------------
    //Let the user define Wavelength to Show
    //----------------------------------------------
    float wavelength;
    wavelength = funcGetParam("Wavelength","450").trimmed().toFloat(0);
    if( wavelength < 300 || wavelength > 1200 )
    {
        funcShowMsgERROR_Timeout("Wavelength incorrect",this);
        return (void)false;
    }

    //**********************************************
    //Display the Wavalength Selected by User
    //**********************************************    

    //----------------------------------------------
    //Let the user select Slide Calibration File
    //----------------------------------------------
    //QString calibPath("./XML/slideCalibration/slideCam_002.xml");
    QString calibPath;
    if( funcLetUserSelectFile(&calibPath,"Select Slide Calibration File...",this) != _OK )
    {
        return (void)false;
    }

    //----------------------------------------------
    //Read Slide Calibration File
    //----------------------------------------------
    structSlideCalibration slideCalibration;
    if( funcReadSlideCalib( calibPath, &slideCalibration ) != _OK )
    {
        funcShowMsgERROR_Timeout("Reading Slide Calibration File",this);
        return (void)false;
    }
    //std::cout << "calibPath: " << calibPath.toStdString() << std::endl;
    //std::cout << "1) slideCalibration.originWave: " << slideCalibration.originWave << std::endl;

    //**********************************************
    //Draw Over Real Image
    //**********************************************

    //----------------------------------------------
    //Apply Affine Transformation
    //----------------------------------------------
    *globalEditImg  = globalEditImg->transformed(slideCalibration.translation);

    //----------------------------------------------
    //Validate Calibration-Size and Image-Size
    //----------------------------------------------
    if(
            slideCalibration.imgW != globalEditImg->width()     ||
            slideCalibration.imgH != globalEditImg->height()
    ){
        funcShowMsgERROR_Timeout("Image Size and Clibration Size are Different",this);
        return (void)false;
    }

    //----------------------------------------------
    //Calculate Distance from Lower Bound
    //----------------------------------------------
    int distPixFromLower;
    wavelength = wavelength - slideCalibration.originWave;
    distPixFromLower = round( funcApplyLR(wavelength,slideCalibration.wave2DistLR,true) );
    std::cout << "originWave: " << slideCalibration.originWave << "nm" << std::endl;
    std::cout << "distPixFromLower: " << distPixFromLower << "px" << std::endl;

    //----------------------------------------------
    //Draw Vertical Line
    //----------------------------------------------
    QPoint newPoint;
    int x = distPixFromLower;
    int y;
    for( y=0; y<=slideCalibration.originH; y++ )
    {
        newPoint = funcGetCoor(x,y,slideCalibration);
        //std::cout << "A-> x: " << newPoint.x() << " y: " << newPoint.y() << std::endl;
        globalEditImg->setPixelColor(newPoint,QColor(255,255,255));
    }

    //----------------------------------------------
    //Update Image Displayed in Canvas
    //----------------------------------------------
    //Update Image Preview
    updatePreviewImage(globalEditImg);

    //Update Edit View
    updateImageCanvasEdit(globalEditImg);

    //----------------------------------------------
    //Save Image
    //----------------------------------------------
    if( funcShowMsgYesNo("Saving Image","Save image?",this) )
    {
        //filePath:         File output, filename selected by the user
        //title:            Showed to User, what kind of file is the user selecting
        //pathLocation:     Where is saved the last path location saved
        //pathOfInterest:   If it is the first time, what path will be saved as default
        //parent:           In order to use this Dialog
        QString fileName;
        if(
                funcLetUserDefineFile(
                                        &fileName,
                                        "Define Image Filename",
                                        ".png",
                                        new QString(_PATH_LAST_IMG_OPEN),
                                        new QString(_PATH_LAST_IMG_OPEN),
                                        this
                                     ) != _OK
        ){
            return (void)false;
        }
        //Save Image
        globalEditImg->save(fileName);
        //Notify Success
        funcShowMsgSUCCESS_Timeout("Image Saved Successfully",this);
    }

}

void MainWindow::on_actionSlide_Calibration_File_triggered()
{
    //---------------------------------------
    //Select Slide Calibration Origin
    //---------------------------------------
    QString slideFile;
    if( funcLetUserSelectFile(&slideFile,"Select Slide Calibration File...",this) != _OK )
    {
        return (void)false;
    }

    //---------------------------------------
    //Save Parameter
    //---------------------------------------
    if( saveFile(_PATH_SLIDE_ACTUAL_CALIB_PATH,slideFile) == true )
    {
        funcShowMsgSUCCESS_Timeout("File Set Successfully");
    }
}

void MainWindow::on_actionMerge_into_RGB_triggered()
{
    formMerge3GrayIntoARgb* tmpForm = new formMerge3GrayIntoARgb(this);
    tmpForm->setModal(true);
    tmpForm->show();
}


/*
void saveNewArea::on_btnSave_clicked()
{
    msgWorking* frmWorking = new msgWorking(this);
    frmWorking->setModal(true);
    frmWorking->show();

    bool allOK = true;

    //Folder name
    if(!allOK || ui->txtFolderName->text().isEmpty()){
        func_ShowMsg("[ERROR]","Folder name wrong");
        allOK = false;
    }
    //Description
    if(!allOK || ui->txtDescription->text().isEmpty()){
        func_ShowMsg("[ERROR]","'Description' can not be empty.");
        allOK = false;
    }

    //-----------------------------------
    // SAVE PART AREA
    //-----------------------------------
    if(allOK){

        unsigned int x2, y2, xlen, ylen;

        //Calculate vertec frontier
        x2   = savingAreaCoord.x + savingAreaCoord.xlen;
        y2   = savingAreaCoord.y + savingAreaCoord.ylen;
        x2   = (x2>savingAreaCoord.width)?savingAreaCoord.width:x2;
        y2   = (y2>savingAreaCoord.heigth)?savingAreaCoord.heigth:y2;
        xlen = x2-savingAreaCoord.x;
        ylen = y2-savingAreaCoord.y;


        //QString msg = QString( "(%1,%2) (%3,%4) xlen(%5) ylen(%6) width(%7) bandas(%8)")
        //                .arg(savingAreaCoord.x)
        //                .arg(savingAreaCoord.y)
        //                .arg(x2)
        //                .arg(y2)
        //                .arg(xlen)
        //                .arg(ylen)
        //                .arg(savingAreaCoord.width)
        //                .arg(savingScenAct.bandsNumber)
        //                ;
        //func_ShowMsg("Coordenadas", msg );

        //Calculate pixels befor the square
        unsigned int despBeforeSize, rows, cols;
        rows                = (savingAreaCoord.y-1) * savingAreaCoord.width;
        cols                = savingAreaCoord.x - 1;
        despBeforeSize      = savingScenAct.bandsNumber * (rows+cols);

        //Calculate buffer size and create it
        unsigned int bufferSize;
        bufferSize          = savingScenAct.bandsNumber * xlen;

        //Calculate reading areas
        unsigned int despBetweenSize, despBAfter, despBBefore;
        despBBefore             = savingAreaCoord.x;
        despBAfter              = savingAreaCoord.width - x2;
        despBetweenSize         = savingScenAct.bandsNumber *
                                  (despBBefore+despBAfter); //17952

        //despBeforeSize, bufferSize, despBetweenSize

        //count is a predefined int
        QByteArray fileMem;
        QFile file( savingScenAct.hypImg );
        unsigned int i;
        if ( file.open(QIODevice::ReadOnly) ){
            file.read(despBeforeSize * sizeof(qint16));
            for(i=1;i<=ylen;i++){
                fileMem.append(file.read(bufferSize * sizeof(qint16)));
                file.read(despBetweenSize * sizeof(qint16));
            }
            file.close();
        }else{
            func_ShowMsg("[ERROR]","Can not open hyperspectral file");
            return;
        }


        //------------------
        // Save file Hyp
        //------------------

        //Create folder
        QString Path = savingScenAct.path + "/" + ui->txtFolderName->text();
        if( QDir(Path).exists() ){
            func_ShowMsg("[ALERTA]","Folder exists, please chose other directory");
            ui->txtFolderName->setFocus();
            return;
        }else{
            QDir().mkdir(Path);
            Path = Path + "/__Originals";
            QDir().mkdir(Path);
        }

        //Create hyperspectral file
        QString subAreaName = Path + "/HypImgSub.hyp";
        //func_ShowMsg("To write",QString::number(fileMem.size()) + " -> "+subAreaName);
        QFile subArea(subAreaName);
        if ( subArea.open(QIODevice::WriteOnly) ){
            subArea.write(fileMem,fileMem.size());        // write to stderr
            subArea.close();
        }

        //------------------
        // Save file RGB
        //------------------
        QString subAreaRGBName = Path + "/RGBImgSub.png";
        QImage image(savingScenAct.rgbImg);
        QImage copy;
        copy = image.copy( savingAreaCoord.x, savingAreaCoord.y, xlen, ylen );
        copy.save(subAreaRGBName);

        //------------------
        // Save DB registry
        //------------------
        Path                = savingScenAct.path + "/" + ui->txtFolderName->text();
        QSqlDatabase db = SQLiteConn();
        QSqlQueryModel query;
        QString ISubArea    = "Insert into subarea (description,path,rgbImg,hypImg,x,xlen,y,ylen,id_project) VALUES (";
        ISubArea.append("'"+ ui->txtDescription->text() +"'");
        ISubArea.append(",'"+ Path +"'");
        ISubArea.append(",'"+ subAreaRGBName +"'");
        ISubArea.append(",'"+ subAreaName +"'");
        ISubArea.append(",'"+ QString::number(savingAreaCoord.x) +"'");
        ISubArea.append(",'"+ QString::number(xlen) +"'");
        ISubArea.append(",'"+ QString::number(savingAreaCoord.y) +"'");
        ISubArea.append(",'"+ QString::number(ylen) +"'");
        ISubArea.append(",'"+ QString::number(savingScenAct.id_project) +"'");
        ISubArea.append(")");
        query.setQuery( ISubArea );
        db.close();

        //------------------
        // FINISH
        //------------------
        frmWorking->destroyThis();
        ui->btnCancel->click();

    }

}
*/

/*
void MainWindow::on_actionTesting_triggered()
{

    //-------------------------------------
    //Read lines
    //-------------------------------------
    structLine lowerLine, upperLine;
    QString linePath;
    linePath = "/home/jairo/Documentos/DESARROLLOS/build-HypCam-Desktop_Qt_5_8_0_GCC_64bit-Release/XML/lines/slideV1_002/2ndAttempt/LowerBound.xml";
    funcReadLineFromXML(&linePath,&lowerLine);
    linePath.clear();
    linePath.append("/home/jairo/Documentos/DESARROLLOS/build-HypCam-Desktop_Qt_5_8_0_GCC_64bit-Release/XML/lines/slideV1_002/2ndAttempt/UpperBound.xml");
    funcReadLineFromXML(&linePath,&upperLine);

    //-------------------------------------
    //Define the quad transformation
    //-------------------------------------
    //Original Points
    QVector<QPointF> originPoints;
    originPoints.append( QPointF(upperLine.x1,upperLine.y1) );
    originPoints.append( QPointF(upperLine.x2,upperLine.y2) );
    originPoints.append( QPointF(lowerLine.x1,lowerLine.y1) );
    originPoints.append( QPointF(lowerLine.x2,lowerLine.y2) );
    //Destine Points
    QVector<QPointF> destinePoints;
    destinePoints.append( QPointF(upperLine.x1,upperLine.y1) );
    destinePoints.append( QPointF(upperLine.x2,upperLine.y1) );
    destinePoints.append( QPointF(lowerLine.x1,lowerLine.y1) );
    destinePoints.append( QPointF(lowerLine.x2,lowerLine.y1) );
    //Transformation Quads
    QPolygonF originQuad(originPoints);
    QPolygonF destineQuad(destinePoints);

    //-------------------------------------
    //Build Transformation
    //-------------------------------------
    //Vanashing Point
    QTransform tmpTrans;
    if( tmpTrans.quadToQuad(originQuad,destineQuad,tmpTrans) == false )
    {
        funcShowMsgERROR_Timeout("Transformation does not exists");
        return (void)false;
    }
    //Rotate
    float degree;
    QString tmpRotation;
    tmpRotation = readAllFile(_PATH_LAST_ONEAXIS_ROTATION).trimmed();
    degree = (tmpRotation.isEmpty())?0.0:tmpRotation.toFloat(0);
    tmpTrans.rotate(degree);

    //-------------------------------------
    //Display Image Transformation
    //-------------------------------------
    *globalEditImg  = globalEditImg->transformed(tmpTrans);

    //Update Image Preview
    updatePreviewImage(globalEditImg);

    //Update Edit View
    updateImageCanvasEdit(globalEditImg);

    std::cout << "Finished successfully" << std::endl;


}*/

void MainWindow::on_actionApply_Rotation_triggered()
{
    //structLine rotationLine;
    //QString rotLinePath;

    //------------------------------------------
    //Get Rotation Line from User
    //------------------------------------------
    QString tmpParam;
    tmpParam = funcGetParam("Degree","0.0").trimmed();
    if( tmpParam.isEmpty() )
    {
        return (void)false;
    }
    float degree;
    degree = tmpParam.toFloat(0);

    //-------------------------------------
    //Save Calibration Degree
    //-------------------------------------
    saveFile(_PATH_LAST_ONEAXIS_ROTATION,tmpParam);

    //-------------------------------------
    //Display Transformated Image
    //-------------------------------------
    //Copy Original Image
    *globalEditImg = *globalBackEditImg;
    //Build Transformation
    QTransform tmpTrans;
    tmpTrans.rotate(degree);
    *globalEditImg      = globalEditImg->transformed(tmpTrans);

    //Update Image Preview
    updatePreviewImage(globalEditImg);

    //Update Edit View
    updateImageCanvasEdit(globalEditImg);

}

void MainWindow::on_actionApply_Transformation_triggered()
{
    QTransform tmpTrans;
    if( funcGetTranslation( &tmpTrans, this ) != _OK )
    {
        funcShowMsgERROR_Timeout("Reading Slide Calibration");
        return (void)false;
    }

    //-------------------------------------
    //Display Image Transformation
    //-------------------------------------
    *globalEditImg  = globalEditImg->transformed(tmpTrans);

    //Update Image Preview
    updatePreviewImage(globalEditImg);

    //Update Edit View
    updateImageCanvasEdit(globalEditImg);

    std::cout << "Finished successfully" << std::endl;
}

void MainWindow::on_actionRestore_Original_triggered()
{
    //Return Backup
    *globalEditImg = *globalBackEditImg;

    //Update Image Preview
    updatePreviewImage(globalEditImg);

    //Update Edit View
    updateImageCanvasEdit(globalEditImg);
}

void MainWindow::on_actionExtract_frames_2_triggered()
{
    mouseCursorWait();
    funcExtractFramesFromH264();
    mouseCursorReset();
}

int MainWindow::funcExtractFramesFromH264(bool updateImage)
{
    //-----------------------------------------------
    //Read Video Source
    //-----------------------------------------------
    QString videoPath;
    if(
            funcLetUserSelectFile(
                                    &videoPath,
                                    "Select Video Source...",
                                    _PATH_LAST_VIDEO_OPENED,
                                    "./tmpImages/",
                                    this
                                 ) != _OK
    ){
        return _FAILURE;
    }

    //-----------------------------------------------
    //Extract video Frames
    //-----------------------------------------------
    //-----------------------------------------------------
    // Extract Frames from Video
    //-----------------------------------------------------

    //Clear local folder
    QString tmpFramesPath;
    tmpFramesPath.append(_PATH_VIDEO_FRAMES);
    tmpFramesPath.append("tmp/");
    funcClearDirFolder( tmpFramesPath );

    //Extract Frames from Local Video
    QString locFrameExtrComm;
    locFrameExtrComm.append("ffmpeg -framerate ");
    locFrameExtrComm.append(QString::number(_VIDEO_FRAME_RATE));
    locFrameExtrComm.append(" -r ");
    locFrameExtrComm.append(QString::number(_VIDEO_FRAME_RATE));
    locFrameExtrComm.append(" -i ");
    locFrameExtrComm.append( videoPath.trimmed() );
    locFrameExtrComm.append(" ");
    locFrameExtrComm.append(tmpFramesPath);
    locFrameExtrComm.append("%07d");
    locFrameExtrComm.append(_FRAME_EXTENSION);
    qDebug() << locFrameExtrComm;
    progBarUpdateLabel("Extracting Frames from Video",0);
    if( !funcExecuteCommand(locFrameExtrComm) )
    {
        funcShowMsg("ERROR","Extracting Frames from Video");
        progBarUpdateLabel("",0);
        return _FAILURE;
    }
    progBarUpdateLabel("",0);

    //-----------------------------------------------------
    // Update mainImage from Frames
    //-----------------------------------------------------
    if( updateImage==true )
    {
        funcUpdateImageFromFolder(tmpFramesPath);
    }

    funcResetStatusBar();

    return _OK;
}














void MainWindow::on_actionSlide_Max_Wavelength_triggered()
{
    //-----------------------------------------
    //Get Last Max Wavelength
    //-----------------------------------------
    float lastMaxWave;
    lastMaxWave = readAllFile(_PATH_SLIDE_MAX_WAVELENGTH).trimmed().toFloat(0);
    lastMaxWave = ( lastMaxWave <= 0.0 )?_RASP_CAM_MAX_WAVELENGTH:lastMaxWave;

    //-----------------------------------------
    //Get Max Wavelength
    //-----------------------------------------
    QString maxWavelen;
    maxWavelen = funcGetParam("Max Wavelength",QString::number(lastMaxWave));
    if( maxWavelen.isEmpty() )
    {
        funcShowMsgERROR_Timeout("Reading Max Wavelength");
        return (void)false;
    }

    //-----------------------------------------
    //Save Max Wavelength
    //-----------------------------------------
    if( saveFile(_PATH_SLIDE_MAX_WAVELENGTH,maxWavelen) == true )
    {
        funcShowMsgSUCCESS_Timeout("File Save Successfully");
    }
    else
    {
        funcShowMsgERROR_Timeout("Saving Max Wavelength");
    }

}

void MainWindow::on_actionBuild_HypCube_triggered()
{
    QList<QImage> lstTransImages;
    structSlideCalibration slideCalibration;
    funcOpticalCorrection(
                              &lstTransImages,
                              &slideCalibration,
                              true,
                              true
                         );
}

void MainWindow::funcOpticalCorrection(
                                            QList<QImage>* lstTransImages,
                                            structSlideCalibration* slideCalibration,
                                            bool tryToSave,
                                            bool refreshImage
){
    //--------------------------------------------
    //Read Slide Calibration
    //--------------------------------------------
    //Get Default Slide Calibration Path
    QString defaCalibPath;
    defaCalibPath = readAllFile(_PATH_SLIDE_ACTUAL_CALIB_PATH).trimmed();
    if( defaCalibPath.isEmpty() )
    {
        funcShowMsg("Alert!","Please, Set Default Slide Calibration");
        return (void)false;
    }
    //Read Default Calibration Path
    //structSlideCalibration slideCalibration;
    if( funcReadSlideCalib( defaCalibPath, slideCalibration ) != _OK )
    {
        funcShowMsgERROR_Timeout("Reading Default Slide Calibration File");
        return (void)false;
    }

    //--------------------------------------------
    //Define Max Wavelength
    //--------------------------------------------    
    double maxWavelen;
    maxWavelen = slideCalibration->maxWave;
    std::cout << "maxWavelen: " << maxWavelen << std::endl;
    //exit(0);
    //maxWavelen = readAllFile(_PATH_SLIDE_MAX_WAVELENGTH).trimmed().toDouble(0);
    //if( maxWavelen < _RASP_CAM_MIN_WAVELENGTH || maxWavelen > _RASP_CAM_MAX_WAVELENGTH )
    //{
    //    maxWavelen = _RASP_CAM_MAX_WAVELENGTH;
    //}

    //--------------------------------------------
    //Define Flat Plane
    //--------------------------------------------
    QPoint P1, P2;
    int distPixFromLower;
    maxWavelen = maxWavelen - slideCalibration->originWave;
    distPixFromLower = round( funcApplyLR(maxWavelen,slideCalibration->wave2DistLR,true) );
    P1.setX( slideCalibration->originX );
    P1.setY( slideCalibration->originY );
    P2.setX( slideCalibration->originX + distPixFromLower );
    P2.setY( slideCalibration->originY + slideCalibration->originH );
    std::cout << "(" << P1.x() << ", " << P1.y() << ") to (" << P2.x() << ", " << P2.y() << ")" << std::endl;

    //--------------------------------------------
    //Get List of Files in Default Directory
    //--------------------------------------------
    //Get Files from Dir
    QString tmpFramesPath;
    tmpFramesPath.append(_PATH_VIDEO_FRAMES);
    tmpFramesPath.append("tmp/");
    //Calc Number of Stabilization Frames
    int numStabFrames;
    numStabFrames = (_STABILIZATION_SECS * _VIDEO_FRAME_RATE)-1;
    //List All Files in Directory
    QList<QFileInfo> lstFrames = funcListFilesInDir( tmpFramesPath );
    std::cout << "numFrames: " << lstFrames.size() << std::endl;
    std::cout << "stabilizationFrames: " << numStabFrames << std::endl;
    if( lstFrames.size() < numStabFrames )
    {
        funcShowMsgERROR_Timeout("Number of Files Insufficient");
        mouseCursorReset();
        return (void)false;
    }

    //--------------------------------------------
    //Get, Transform and Put-back Transformed File
    //--------------------------------------------
    //Define subimage
    QRect subImgRec;
    subImgRec.setX( P1.x() );
    subImgRec.setY( P1.y() );
    subImgRec.setWidth( P2.x() - P1.x() );
    subImgRec.setHeight( P2.y() - P1.y() );
    //Update Progess Bar
    ui->progBar->setVisible(true);
    ui->progBar->setValue(0);
    ui->progBar->update();
    progBarUpdateLabel("Putting Transformed Frames into Memory...",0);
    //Process and Save Frames
    QImage tmpImg;
    int i, progVal;
    progVal = 0;
    //Remove Stabilization Frames
    if(0)
    {
        for( i=0; i<numStabFrames; i++ )
        {
            //std::cout << "DEL-> i: " << i << " Modified: " << lstFrames.at(i).absoluteFilePath().toStdString() << std::endl;
            if( funcDeleteFile(lstFrames.at(i).absoluteFilePath()) != _OK )
            {
                funcShowMsgERROR("Deleting: "+lstFrames.at(i).absoluteFilePath());
                break;
            }
        }
    }

    //-------------------------------------------------
    //GET LIST OF FILES
    //-------------------------------------------------
    lstFrames = funcListFilesInDir( tmpFramesPath );


    //-------------------------------------------------
    //DEFINE ROI
    //-------------------------------------------------
    QString rect;
    if( funcLetUserSelectFile(&rect,"Select ROI",this) != _OK )
    {
        return (void)false;
    }
    squareAperture *tmpSqAperture = (squareAperture*)malloc(sizeof(squareAperture));
    if( !funGetSquareXML( rect, tmpSqAperture ) )
    {
        funcShowMsg("ERROR","Loading _PATH_REGION_OF_INTERES");
        return (void)false;
    }
    QImage firstImg(lstFrames.at(0).absoluteFilePath().trimmed());
    int xROI, yROI, wROI, hROI;
    xROI = round(((float)tmpSqAperture->rectX / (float)tmpSqAperture->canvasW) * (float)firstImg.width());
    yROI = round(((float)tmpSqAperture->rectY / (float)tmpSqAperture->canvasH) * (float)firstImg.height());
    wROI = round(((float)tmpSqAperture->rectW / (float)tmpSqAperture->canvasW) * (float)firstImg.width());
    hROI = round(((float)tmpSqAperture->rectH / (float)tmpSqAperture->canvasH) * (float)firstImg.height());

    //-------------------------------------------------
    //Get into Memory Transformed Frames
    //-------------------------------------------------    
    //QList<QImage> lstTransImages;
    lstTransImages->clear();
    for( i=0; i<lstFrames.size(); i++ )
    //for( i=0; i<1; i++ )
    {
        //std::cout << "MOD-> i: " << i << " Modified: " << lstFrames.at(i).absoluteFilePath().toStdString() << std::endl;
        //std::cout << "m11: " << slideCalibration->translation.m11() << std::endl;
        tmpImg  = QImage( lstFrames.at(i).absoluteFilePath() );
        //ROI
        tmpImg  = tmpImg.copy(xROI,yROI,wROI,hROI);
        //displayImageFullScreen(tmpImg);
        tmpImg  = tmpImg.transformed( slideCalibration->translation );
        //displayImageFullScreen(tmpImg);
        tmpImg  = tmpImg.copy(subImgRec);
        //displayImageFullScreen(tmpImg);
        lstTransImages->append( tmpImg );
        //Update Progress Bar
        progVal = round( ( (float)(i) / (float)lstFrames.size() ) * 100.0 );
        ui->progBar->setValue(progVal);
        ui->progBar->update();

    }

    //-------------------------------------------------
    //Update Image Displayed
    //-------------------------------------------------
    if( refreshImage == true )
    {
        int pos;
        pos = round( (float)lstTransImages->size() * 0.5 );
        tmpImg = lstTransImages->at(pos);
        updateDisplayImage( &tmpImg );
    }

    //--------------------------------------------
    //Save Transformed Images From Memory
    //--------------------------------------------
    //Ask to the User
    if( tryToSave == true )
    {
        if( funcShowMsgYesNo("Alert!","Save Frames into HDD",this) == 0 )
        {
            //Reset Progress Bar
            ui->progBar->setValue(0);
            ui->progBar->setVisible(false);
            ui->progBar->update();
            progBarUpdateLabel("",0);
            return (void)true;
        }

        //Hide Progress Bar
        progVal = 0;
        ui->progBar->setValue(progVal);
        ui->progBar->update();
        progBarUpdateLabel("Saving Transformed Images Into HDD",0);
        for( i=0; i<lstTransImages->size(); i++ )
        {
            //Save Image Into HDD
            lstTransImages->at(i).save( lstFrames.at(i).absoluteFilePath() );
            //Update Progress Bar
            progVal = round( ( (float)(i) / (float)lstFrames.size() ) * 100.0 );
            ui->progBar->setValue(progVal);
            ui->progBar->update();
        }
    }

    //Reset Progress Bar
    ui->progBar->setValue(0);
    ui->progBar->setVisible(false);
    ui->progBar->update();
    progBarUpdateLabel("",0);
    if( tryToSave == true )
    {
        //Notify Success
        funcShowMsgSUCCESS_Timeout("Optical Correction Complete Successfully",this);
    }
}

void MainWindow::on_actionBuild_HypCube_2_triggered()
{
    formBuildSlideHypeCubePreview slideHypeCubePreview;
    QString destineDir;
    if( funcLetUserSelectDirectory(_PATH_LAST_SLIDE_HYPCUBE_EXPORTED,&destineDir) != _OK )
    {
        return (void)false;
    }
    slideHypeCubePreview.exportSlideHypCube(
                                                &destineDir,
                                                ui->progBar,
                                                ui->labelProgBar,
                                                this
                                           );
}

void MainWindow::on_actionSlide_HypCube_Building_triggered()
{
    formHypcubeBuildSettings* tmpForm = new formHypcubeBuildSettings(this);
    tmpForm->setModal(true);
    tmpForm->show();
}

void MainWindow::on_actionCamera_Sensitivities_triggered()
{
    //------------------------------------------
    //Read Slide Calibration
    //------------------------------------------

    //Get Default Slide Calibration Path
    QString defaCalibPath;
    defaCalibPath = readAllFile(_PATH_SLIDE_ACTUAL_CALIB_PATH).trimmed();
    if( defaCalibPath.isEmpty() )
    {
        funcShowMsg("Alert!","Please, Set Default Slide Calibration");
        return (void)false;
    }
    //Read Default Calibration Path
    structSlideCalibration slideCalibration;
    if( funcReadSlideCalib( defaCalibPath, &slideCalibration ) != _OK )
    {
        funcShowMsgERROR_Timeout("Reading Default Slide Calibration File");
        return (void)false;
    }
    //Get Spectral Resolution
    float specRes;//, specMin;
    //specMin = slideCalibration.originWave;
    specRes = (slideCalibration.maxWave - slideCalibration.originWave) /
              (float)globalEditImg->width();
    //funcShowMsg("specRes",QString::number(specRes));

    //------------------------------------------
    //Get halogen function
    //------------------------------------------
    QList<double> halogenFunction;
    halogenFunction = getNormedFunction( _PATH_HALOGEN_FUNCTION );//[0,3500]nm | Real [0,1] | 1=null

    //------------------------------------------
    //Prepare Variables
    //------------------------------------------
    int W, H;
    int x, y;
    W = globalEditImg->width();
    H = globalEditImg->height();
    float sensR[W];
    float sensG[W];
    float sensB[W];
    float halog[W];

    float backSensR[W];
    float backSensG[W];
    float backSensB[W];

    float wR[W];
    float wG[W];
    float wB[W];

    float tmpR, tmpG, tmpB;
    float maxR, maxG, maxB, maxH;
    QColor tmpColor;
    int specPos;

    //------------------------------------------
    //AVERAGE SENSED VALUE
    //------------------------------------------
    maxR = 0;   maxG = 0;   maxB = 0;    maxH = 0;
    for( x=0; x<W; x++ )
    {
        //......................................
        //Calc Sesitivities
        //......................................
        tmpR = 0;
        tmpG = 0;
        tmpB = 0;
        for( y=0; y<H; y++ )
        {
            tmpColor = globalEditImg->pixelColor(x,y);
            tmpR    += (float)tmpColor.red();
            tmpG    += (float)tmpColor.green();
            tmpB    += (float)tmpColor.blue();
        }
        sensR[x]     = tmpR / (float)H;
        sensG[x]     = tmpG / (float)H;
        sensB[x]     = tmpB / (float)H;

        //......................................
        //Get Halogen Value
        //......................................
        specPos      = floor( slideCalibration.originWave + ( x * specRes ) );
        halog[x]     = halogenFunction.at(specPos);

        //......................................
        //Calc Maximums
        //......................................
        maxR = (sensR[x]>maxR)?sensR[x]:maxR;
        maxG = (sensG[x]>maxG)?sensG[x]:maxG;
        maxB = (sensB[x]>maxB)?sensB[x]:maxB;
        maxH = (halog[x]>maxH)?halog[x]:maxH;
    }
    //------------------------------------------
    //CUSTOM
    //------------------------------------------
    if( 1 )
    {
        //Delete Red Noise
        int n = 100;
        float tmpVal = sensR[n];
        for( x=0; x<n; x++ )
        {
            sensR[x]    = tmpVal;
        }

        //Fix nose added by HypCam
        sensR[0]        = sensR[1];
        sensG[0]        = sensG[1];
        sensB[0]        = sensB[1];

        sensR[W-1]      = sensR[W-2];
        sensG[W-1]      = sensG[W-2];
        sensB[W-1]      = sensB[W-2];

    }

    //------------------------------------------
    //NORMALIZE SENSITIVITIES
    //------------------------------------------
    for( x=0; x<W; x++ )
    {
        sensR[x]    = sensR[x] / maxR;
        sensG[x]    = sensG[x] / maxG;
        sensB[x]    = sensB[x] / maxB;

        halog[x]    = halog[x] / maxH;
    }

    //------------------------------------------
    //BACKUP BEFOR APPLY RALPH's PROPOSAL
    //------------------------------------------
    for( x=0; x<W; x++ )
    {
        backSensR[x] = sensR[x];
        backSensG[x] = sensG[x];
        backSensB[x] = sensB[x];

        wR[x]        = sensR[x] / (sensR[x]+sensG[x]+sensB[x]) ;
        wG[x]        = sensG[x] / (sensR[x]+sensG[x]+sensB[x]) ;
        wB[x]        = sensB[x] / (sensR[x]+sensG[x]+sensB[x]) ;
    }

    //------------------------------------------
    //Apply Ralph's Proposed Method to Calc
    //Sensor Sensitivities
    //------------------------------------------
    maxR = 0;   maxG = 0;   maxB = 0;
    for( x=0; x<W; x++ )
    {
        sensR[x] = sensR[x] / halog[x];
        sensG[x] = sensG[x] / halog[x];
        sensB[x] = sensB[x] / halog[x];
        maxR     = (sensR[x]>maxR)?sensR[x]:maxR;
        maxG     = (sensG[x]>maxG)?sensG[x]:maxG;
        maxB     = (sensB[x]>maxB)?sensB[x]:maxB;
    }

    //------------------------------------------
    //NORMALIZE FINAL SENSITIVITIES
    //------------------------------------------
    for( x=0; x<W; x++ )
    {
        sensR[x] = sensR[x] / maxR;
        sensG[x] = sensG[x] / maxG;
        sensB[x] = sensB[x] / maxB;
    }

    //------------------------------------------
    //Concatenate Sensitivities Vectors
    //------------------------------------------
    QString SR, SG, SB;
    QString wSR, wSG, wSB;
    QString backSR, backSG, backSB;
    QString halogNorm;

    SR.append(QString::number(sensR[0]));
    SG.append(QString::number(sensG[0]));
    SB.append(QString::number(sensB[0]));

    wSR.append(QString::number(wR[0]));
    wSG.append(QString::number(wG[0]));
    wSB.append(QString::number(wB[0]));

    backSR.append(QString::number(backSensR[0]));
    backSG.append(QString::number(backSensG[0]));
    backSB.append(QString::number(backSensB[0]));
    halogNorm.append(QString::number(halog[0]));

    for( x=1; x<W; x++ )
    {
        SR.append( "," + QString::number(sensR[x]) );
        SG.append( "," + QString::number(sensG[x]) );
        SB.append( "," + QString::number(sensB[x]) );

        wSR.append( "," + QString::number(wR[x]) );
        wSG.append( "," + QString::number(wG[x]) );
        wSB.append( "," + QString::number(wB[x]) );

        backSR.append( "," + QString::number(backSensR[x]) );
        backSG.append( "," + QString::number(backSensG[x]) );
        backSB.append( "," + QString::number(backSensB[x]) );

        halogNorm.append( "," + QString::number(halog[x]) );
    }

    //------------------------------------------
    //Save Sensitivities File
    //------------------------------------------
    //Prepare Output File
    QList<QString> fixtures;
    QList<QString> values;

    fixtures << "ralphSR"       << "ralphSG"        << "ralphSB";
    fixtures << "wSR"           << "wSG"            << "wSB";
    fixtures << "originalSR"    << "originalSG"     << "originalSB";
    fixtures << "halogNorm";

    values << SR        << SG       << SB;
    values << wSR       << wSG      << wSB;
    values << backSR    << backSG   << backSB;
    values << halogNorm;

    //Save File
    //funcSaveXML(_PATH_SLIDE_RGB_SENSITIVITIES,&fixtures,&values,true);

}

void MainWindow::on_actionCalc_Sensitivities_triggered()
{
    //------------------------------------------
    //Read Slide Calibration
    //------------------------------------------

    //Get Default Slide Calibration Path
    QString defaCalibPath;
    defaCalibPath = readAllFile(_PATH_SLIDE_ACTUAL_CALIB_PATH).trimmed();
    if( defaCalibPath.isEmpty() )
    {
        funcShowMsg("Alert!","Please, Set Default Slide Calibration");
        return (void)false;
    }
    //Read Default Calibration Path
    structSlideCalibration slideCalibration;
    if( funcReadSlideCalib( defaCalibPath, &slideCalibration ) != _OK )
    {
        funcShowMsgERROR_Timeout("Reading Default Slide Calibration File");
        return (void)false;
    }
    //Get Spectral Resolution
    float specRes;//, specMin;
    //specMin = slideCalibration.originWave;
    specRes = (slideCalibration.maxWave - slideCalibration.originWave) /
              (float)globalEditImg->width();
    //funcShowMsg("specRes",QString::number(specRes));

    //------------------------------------------
    //Get Halogen Function
    //------------------------------------------
    QList<double> halogenFunction;
    halogenFunction = getNormedFunction( _PATH_HALOGEN_FUNCTION );//[0,3500]nm | Real [0,1] | 1=null

    //------------------------------------------
    //Prepare Variables
    //------------------------------------------
    int W, H;
    int x, y;
    W = globalEditImg->width();
    H = globalEditImg->height();

    float originalR[W];
    float originalG[W];
    float originalB[W];
    float originalH[W];

    float ralfR[W];
    float ralfG[W];
    float ralfB[W];

    float normedOrigR[W];
    float normedOrigG[W];
    float normedOrigB[W];
    float normedOrigH[W];

    float normedRalfR[W];
    float normedRalfG[W];
    float normedRalfB[W];
    float normedRalfH[W];

    float wR[W];
    float wG[W];
    float wB[W];

    float tmpR, tmpG, tmpB;
    float maxR, maxG, maxB, maxH, maxMax;
    QColor tmpColor;
    int specPos;

    //------------------------------------------
    //AVERAGE SENSED VALUE
    //------------------------------------------
    maxR = 0;   maxG = 0;   maxB = 0;    maxH = 0;   maxMax = 0;
    for( x=0; x<W; x++ )
    {
        //......................................
        //Calc Sesitivities
        //......................................
        tmpR = 0;
        tmpG = 0;
        tmpB = 0;
        for( y=0; y<H; y++ )
        {
            tmpColor = globalEditImg->pixelColor(x,y);
            tmpR    += (float)tmpColor.red();
            tmpG    += (float)tmpColor.green();
            tmpB    += (float)tmpColor.blue();
        }
        originalR[x]    = tmpR / (float)H;
        originalG[x]    = tmpG / (float)H;
        originalB[x]    = tmpB / (float)H;

        //......................................
        //Get Halogen Value
        //......................................
        specPos      = floor( slideCalibration.originWave + ( x * specRes ) );
        originalH[x] = halogenFunction.at(specPos);

        //......................................
        //Calc Maximums
        //......................................
        maxR = (originalR[x]>maxR)?originalR[x]:maxR;
        maxG = (originalG[x]>maxG)?originalG[x]:maxG;
        maxB = (originalB[x]>maxB)?originalB[x]:maxB;
        maxH = (originalH[x]>maxH)?originalH[x]:maxH;
    }
    maxMax = (maxR>maxG)?maxR:maxG;
    maxMax = (maxMax>maxB)?maxMax:maxB;

    //------------------------------------------
    //CUSTOM
    //------------------------------------------
    if( 1 )
    {
        //Delete Red Noise
        int n = round(0.26 * (float)W);//Arbitrary
        float tmpVal = 0.0;
        if( 1 )
        {
            for( x=0; x<n; x++ )
            {
                originalR[x]    = tmpVal;
            }
        }

        //Fix nose added by HypCam
        originalR[0]    = originalR[1];
        originalG[0]    = originalG[1];
        originalB[0]    = originalB[1];

        originalR[W-1]  = originalR[W-2];
        originalG[W-1]  = originalG[W-2];
        originalB[W-1]  = originalB[W-2];
    }

    //------------------------------------------
    //NORMALIZE SENSITIVITIES
    //------------------------------------------
    for( x=0; x<W; x++ )
    {
        normedOrigR[x]  = originalR[x] / maxMax;
        normedOrigG[x]  = originalG[x] / maxMax;
        normedOrigB[x]  = originalB[x] / maxMax;
        normedOrigH[x]  = originalH[x] / maxH;
    }

    //------------------------------------------
    //CALC SENSOR WEIGHTS
    //------------------------------------------
    for( x=0; x<W; x++ )
    {
        wR[x]        = originalR[x] / (originalR[x]+originalG[x]+originalB[x]);
        wG[x]        = originalG[x] / (originalR[x]+originalG[x]+originalB[x]);
        wB[x]        = originalB[x] / (originalR[x]+originalG[x]+originalB[x]);
    }

    //------------------------------------------
    //Apply Ralph's Proposed Method to Calc
    //Sensor Sensitivities
    //------------------------------------------
    float aux = maxMax;
    maxR = 0;   maxG = 0;   maxB = 0;
    for( x=0; x<W; x++ )
    {
        ralfR[x] = normedOrigR[x] / normedOrigH[x];
        ralfG[x] = normedOrigG[x] / normedOrigH[x];
        ralfB[x] = normedOrigB[x] / normedOrigH[x];
        maxR     = (ralfR[x]>maxR)?ralfR[x]:maxR;
        maxG     = (ralfG[x]>maxG)?ralfG[x]:maxG;
        maxB     = (ralfB[x]>maxB)?ralfB[x]:maxB;
    }
    maxMax = (maxR>maxG)?maxR:maxG;
    maxMax = (maxMax>maxB)?maxMax:maxB;
    //maxMax = (maxMax>aux)?maxMax:aux;

    //------------------------------------------
    //NORMALIZE RALF SENSITIVITIES
    //------------------------------------------
    for( x=0; x<W; x++ )
    {
        normedRalfR[x]  = ralfR[x] / maxMax;
        normedRalfG[x]  = ralfG[x] / maxMax;
        normedRalfB[x]  = ralfB[x] / maxMax;
        normedRalfH[x]  = normedOrigH[x] / maxMax;

        normedOrigR[x]  = normedOrigR[x] / maxMax;
        normedOrigG[x]  = normedOrigG[x] / maxMax;
        normedOrigB[x]  = normedOrigB[x] / maxMax;
    }

    //------------------------------------------
    //Concatenate Sensitivities Vectors
    //------------------------------------------
    QString strOriginalR, strOriginalG, strOriginalB, strOriginalH;
    QString strRalfR, strRalfG, strRalfB;
    QString strNormedOrigR, strNormedOrigG, strNormedOrigB, strNormedOrigH;
    QString strNormedRalfR, strNormedRalfG, strNormedRalfB, strNormedRalfH;
    QString strWR, strWG, strWB;

    strOriginalR.append(QString::number(originalR[0]));
    strOriginalG.append(QString::number(originalG[0]));
    strOriginalB.append(QString::number(originalB[0]));
    strOriginalH.append(QString::number(originalH[0]));

    strRalfR.append(QString::number(ralfR[0]));
    strRalfG.append(QString::number(ralfG[0]));
    strRalfB.append(QString::number(ralfB[0]));

    strNormedOrigR.append(QString::number(normedOrigR[0]));
    strNormedOrigG.append(QString::number(normedOrigG[0]));
    strNormedOrigB.append(QString::number(normedOrigB[0]));
    strNormedOrigH.append(QString::number(normedOrigH[0]));

    strNormedRalfR.append(QString::number(normedRalfR[0]));
    strNormedRalfG.append(QString::number(normedRalfG[0]));
    strNormedRalfB.append(QString::number(normedRalfB[0]));
    strNormedRalfH.append(QString::number(normedRalfH[0]));

    strWR.append(QString::number(wR[0]));
    strWG.append(QString::number(wG[0]));
    strWB.append(QString::number(wB[0]));

    for( x=1; x<W; x++ )
    {
        strOriginalR.append( "," + QString::number(originalR[x]) );
        strOriginalG.append( "," + QString::number(originalG[x]) );
        strOriginalB.append( "," + QString::number(originalB[x]) );
        strOriginalH.append( "," + QString::number(originalH[x]) );

        strRalfR.append( "," + QString::number(ralfR[x]) );
        strRalfG.append( "," + QString::number(ralfG[x]) );
        strRalfB.append( "," + QString::number(ralfB[x]) );

        strNormedOrigR.append( "," + QString::number(normedOrigR[x]) );
        strNormedOrigG.append( "," + QString::number(normedOrigG[x]) );
        strNormedOrigB.append( "," + QString::number(normedOrigB[x]) );
        strNormedOrigH.append( "," + QString::number(normedOrigH[x]) );

        strNormedRalfR.append( "," + QString::number(normedRalfR[x]) );
        strNormedRalfG.append( "," + QString::number(normedRalfG[x]) );
        strNormedRalfB.append( "," + QString::number(normedRalfB[x]) );
        strNormedRalfH.append( "," + QString::number(normedRalfH[x]) );

        strWR.append( "," + QString::number(wR[x]) );
        strWG.append( "," + QString::number(wG[x]) );
        strWB.append( "," + QString::number(wB[x]) );
    }

    //------------------------------------------
    //Get Output Filename
    //------------------------------------------
    QString fileName;
    //Get Last Calib Open
    if(
            funcLetUserDefineFile(
                                    &fileName,
                                    "Define Sensitivities Output File",
                                    ".xml",
                                    new QString( _PATH_CALIB_LAST_OPEN ),
                                    new QString( _PATH_DEFA_CALIB ),
                                    this
                                 ) != _OK
    ){
        funcShowMsgERROR_Timeout("Reading Last Cal Path Opened");
        return (void)false;
    }

    //------------------------------------------
    //Export to Octave
    //------------------------------------------
    QString forOctave;
    forOctave.append( "\noriginalR=["+ strOriginalR +"];\n" );
    forOctave.append( "originalG=["+ strOriginalG +"];\n" );
    forOctave.append( "originalB=["+ strOriginalB +"];\n" );
    forOctave.append( "originalH=["+ strOriginalH +"];\n" );

    forOctave.append( "ralfR=["+ strRalfR +"];\n" );
    forOctave.append( "ralfG=["+ strRalfG +"];\n" );
    forOctave.append( "ralfB=["+ strRalfB +"];\n" );

    forOctave.append( "normedOrigR=["+ strNormedOrigR +"];\n" );
    forOctave.append( "normedOrigG=["+ strNormedOrigG +"];\n" );
    forOctave.append( "normedOrigB=["+ strNormedOrigB +"];\n" );
    forOctave.append( "normedOrigH=["+ strNormedOrigH +"];\n" );

    forOctave.append( "normedRalfR=["+ strNormedRalfR +"];\n" );
    forOctave.append( "normedRalfG=["+ strNormedRalfG +"];\n" );
    forOctave.append( "normedRalfB=["+ strNormedRalfB +"];\n" );
    forOctave.append( "normedRalfH=["+ strNormedRalfH +"];\n" );

    forOctave.append( "wR=["+ strWR +"];\n" );
    forOctave.append( "wG=["+ strWG +"];\n" );
    forOctave.append( "wB=["+ strWB +"];\n" );

    //------------------------------------------
    //Save Sensitivities File
    //------------------------------------------
    //Prepare Output File
    QList<QString> fixtures;
    QList<QString> values;

    fixtures << "originalR"     << "originalG"      << "originalB"      << "originalH";
    fixtures << "ralfR"         << "ralfG"          << "ralfB";
    fixtures << "normedOrigR"   << "normedOrigG"    << "normedOrigB"    << "normedOrigH";
    fixtures << "normedRalfR"   << "normedRalfG"    << "normedRalfB"    << "normedRalfH";
    fixtures << "wR"            << "wG"             << "wB";

    values << strOriginalR      << strOriginalG     << strOriginalB     << strOriginalH;
    values << strRalfR          << strRalfG         << strRalfB;
    values << strNormedOrigR    << strNormedOrigG   << strNormedOrigB   << strNormedOrigH;
    values << strNormedRalfR    << strNormedRalfG   << strNormedRalfB   << strNormedRalfH;
    values << strWR             << strWG            << strWB;

    //If you want to plot
    if( 1 )
    {
        fixtures << "octave";
        values << forOctave;
    }

    //Save File
    funcSaveXML( fileName, &fixtures, &values, true );
}

void MainWindow::on_actionSlide_Min_Wavelength_triggered()
{
    //-----------------------------------------
    //Get Last Min Wavelength
    //-----------------------------------------
    float lastMinWave;
    lastMinWave = readAllFile(_PATH_SLIDE_MIN_WAVELENGTH).trimmed().toFloat(0);
    lastMinWave = ( lastMinWave <= 0.0 )?_RASP_CAM_MIN_WAVELENGTH:lastMinWave;

    //-----------------------------------------
    //Get Min Wavelength
    //-----------------------------------------
    QString minWavelen;
    minWavelen = funcGetParam("Min Wavelength",QString::number(lastMinWave));
    if( minWavelen.isEmpty() )
    {
        funcShowMsgERROR_Timeout("Reading Min Wavelength");
        return (void)false;
    }

    //-----------------------------------------
    //Save Max Wavelength
    //-----------------------------------------
    if( saveFile(_PATH_SLIDE_MIN_WAVELENGTH,minWavelen) == true )
    {
        funcShowMsgSUCCESS_Timeout("File Save Successfully");
    }
    else
    {
        funcShowMsgERROR_Timeout("Saving Min Wavelength");
    }
}

void MainWindow::on_actionHypCube_From_H264_triggered()
{
    //==============================================
    //Define Output Dir
    //==============================================
    QString destineDir;
    if( funcLetUserSelectDirectory(_PATH_LAST_SLIDE_HYPCUBE_EXPORTED,&destineDir) != _OK )
    {
        std::cout << "ERROR: Selecting Directory..." << std::endl;
        return (void)false;
    }

    //==============================================
    //Extract Video Frames From H264
    //==============================================
    if( funcExtractFramesFromH264( false ) != _OK )
    {
        std::cout << "ERROR: Extracting frames..." << std::endl;
        return (void)false;
    }

    //==============================================
    //Apply Optical Correction
    //==============================================
    QList<QImage> lstTransImages;
    structSlideCalibration slideCalibration;
    funcOpticalCorrection(
                              &lstTransImages,
                              &slideCalibration,
                              false,
                              false
                          );
    std::cout << "Optical Correction Applied" << std::endl;
    std::cout << "NumImgs: "<< lstTransImages.size() << std::endl;
    std::cout << " imgW: " << lstTransImages.at(0).width()
              << " imgH: " << lstTransImages.at(0).height() << std::endl;

    //==============================================
    //Export Hypercube
    //==============================================
    formBuildSlideHypeCubePreview slideHypeCubePreview(this);
    slideHypeCubePreview.hide();
    slideHypeCubePreview.lstImgs = lstTransImages;
    slideHypeCubePreview.exportSlideHypCube(
                                                &destineDir,
                                                ui->progBar,
                                                ui->labelProgBar,
                                                this
                                            );
    std::cout << "Cube Exported" << std::endl;

}

void MainWindow::on_actionCube_Analysis_triggered()
{
    //-------------------------------------------------
    //Prepare GV
    //-------------------------------------------------
    formHypercubeAnalysis* tmpForm = new formHypercubeAnalysis(this);
    tmpForm->setModal(true);
    tmpForm->showMaximized();
    //GraphicsView* mainGV = new GraphicsView(this);
    //mainGV->displayHypercubeAnalysisScenary();

}

void MainWindow::on_actionApply_Affine_Transformation_triggered()
{
    //----------------------------------------------
    //Load Affine Transformation
    //----------------------------------------------
    // Select Affine Transformation File
    QString transFilename;
    if( funcLetUserSelectFile(&transFilename,"Select Affine Transformation...",this) != _OK )
    {
        return (void)false;
    }
    //Read Affine Transformation
    QTransform T;
    if( funcReadAffineTransXML( transFilename.trimmed(), &T ) != _OK )
    {
        funcShowMsgERROR_Timeout("Reading Affine Transformation");
        return (void)false;
    }

    //-------------------------------------
    //Apply Transformation
    //-------------------------------------
    *globalEditImg  = globalEditImg->transformed(T);

    //-------------------------------------
    //Display Image Transformed
    //-------------------------------------

    //Update Image Preview
    updatePreviewImage(globalEditImg);

    //Update Edit View
    updateImageCanvasEdit(globalEditImg);

    std::cout << "Finished successfully" << std::endl;

}

void MainWindow::on_actionApply_Optical_Correction_triggered()
{

    //---------------------------------------
    //Get Horizontal Calibration
    //---------------------------------------
    //Get filename
    QString fileName;
    if( funcLetUserSelectFile(&fileName, "Select Horizontal Calibration",this) != _OK )
    {
        funcShowMsgERROR_Timeout("Getting Horizontal Calibration");
        return (void)false;
    }
    //Get Line parameters from .XML
    structHorizontalCalibration tmpHorizCal;
    funcReadHorizCalibration(&fileName,&tmpHorizCal);

    //---------------------------------------
    //Get Vertical Calibration
    //---------------------------------------
    //Get filename
    if( funcLetUserSelectFile(&fileName,"Select Vertical Calibration",this) != _OK )
    {
        funcShowMsgERROR_Timeout("Getting Vertical Calibration");
        return (void)false;
    }
    //Get Calibration Parameters from .XML
    structVerticalCalibration tmpVertCal;
    funcReadVerticalCalibration(&fileName,&tmpVertCal);

    //----------------------------------------------
    //Load Affine Transformation
    //----------------------------------------------
    // Select Affine Transformation File
    QString transFilename;
    if( funcLetUserSelectFile(&transFilename,"Select Affine Transformation...",this) != _OK )
    {
        return (void)false;
    }
    //Read Affine Transformation
    QTransform T;
    if( funcReadAffineTransXML( transFilename.trimmed(), &T ) != _OK )
    {
        funcShowMsgERROR_Timeout("Reading Affine Transformation");
        return (void)false;
    }

    //-------------------------------------
    //Get Subimage
    //-------------------------------------
    //Calc Origin
    QPoint originP = originFromCalibration( tmpHorizCal, tmpVertCal );
    //Defina Region of Interest
    QPoint P1, P2;
    int distPixFromLower;
    float maxWavelen  = tmpVertCal.maxWave;
    maxWavelen  = maxWavelen - tmpVertCal.minWave;
    distPixFromLower = round( funcApplyLR(maxWavelen,tmpVertCal.wave2DistLR,true) );
    P1.setX( originP.x() );
    P1.setY( originP.y() );
    P2.setX( originP.x() + distPixFromLower );
    P2.setY( originP.y() + tmpHorizCal.H );
    std::cout << "distPixFromLower: " << distPixFromLower << std::endl;
    //std::cout << "a: " << tmpVertCal.wavelengthLR.a << " b: " << tmpVertCal.wavelengthLR.b << std::endl;
    std::cout << "(" << P1.x() << ", " << P1.y() << ") to (" << P2.x() << ", " << P2.y() << ")" << std::endl;
    //Define Subimage Rect
    QRect subImgRec;
    subImgRec.setX( P1.x() );
    subImgRec.setY( P1.y() );
    subImgRec.setWidth( P2.x() - P1.x() );
    subImgRec.setHeight( P2.y() - P1.y() );

    //-------------------------------------
    //Extract Region of Interest
    //-------------------------------------
    *globalEditImg  = globalEditImg->transformed( T );
    *globalEditImg  = globalEditImg->copy( subImgRec );

    //-------------------------------------
    //Display Image Transformed
    //-------------------------------------

    //Update Image Preview
    updatePreviewImage(globalEditImg);

    //Update Edit View
    updateImageCanvasEdit(globalEditImg);

    std::cout << "Finished successfully" << std::endl;




}

void MainWindow::on_actionExtract_ROI_triggered()
{
    QString rect;
    if( funcLetUserSelectFile(&rect,"Select ROI",this) != _OK )
    {
        return (void)false;
    }

    mouseCursorWait();
    //--------------------------------------------
    //Get List of Files in Default Directory
    //--------------------------------------------
    //Get Files from Dir
    QString tmpFramesPath;
    tmpFramesPath.append(_PATH_VIDEO_FRAMES);
    tmpFramesPath.append("tmp/");
    //List All Files in Directory
    QList<QFileInfo> lstFrames = funcListFilesInDir( tmpFramesPath );
    std::cout << "numFrames: " << lstFrames.size() << std::endl;
    //Read first Image
    QImage firstImg( lstFrames.at(0).absoluteFilePath() );

    //--------------------------------------------
    //Obtaining square aperture params
    //--------------------------------------------
    squareAperture *tmpSqAperture = (squareAperture*)malloc(sizeof(squareAperture));
    if( !funGetSquareXML( rect, tmpSqAperture ) )
    {
        funcShowMsg("ERROR","Loading _PATH_REGION_OF_INTERES");
        return (void)false;
    }
    int x, y, w, h;
    x = round(((float)tmpSqAperture->rectX / (float)tmpSqAperture->canvasW) * (float)firstImg.width());
    y = round(((float)tmpSqAperture->rectY / (float)tmpSqAperture->canvasH) * (float)firstImg.height());
    w = round(((float)tmpSqAperture->rectW / (float)tmpSqAperture->canvasW) * (float)firstImg.width());
    h = round(((float)tmpSqAperture->rectH / (float)tmpSqAperture->canvasH) * (float)firstImg.height());

    //--------------------------------------------
    //Save ROIS
    //--------------------------------------------
    int i;
    ui->progBar->setVisible(true);
    ui->labelProgBar->setVisible(true);
    ui->labelProgBar->setText("Extracting ROI...");
    for(i=0; i<lstFrames.size(); i++)
    {
        QImage tmpImg( lstFrames.at(i).absoluteFilePath() );
        tmpImg = tmpImg.copy( x, y, w, h );
        tmpImg.save( lstFrames.at(i).absoluteFilePath() );

        //Update Progress Bar
        ui->progBar->setValue(round( ( (float)(i) / (float)lstFrames.size() ) * 100.0 ));
        ui->progBar->update();
        QtDelay(1);
    }

    ui->labelProgBar->setText("");
    ui->progBar->setVisible(false);
    ui->progBar->setValue(0);
    ui->progBar->update();
    mouseCursorReset();
    QtDelay(1);

    //-------------------------------------------------
    //Update Displayed Image
    //-------------------------------------------------
    int pos;
    pos = round( (float)lstFrames.size() * 0.5 );
    QImage tmpImg(lstFrames.at(pos).absoluteFilePath());
    *globalEditImg = tmpImg;
    *globalBackEditImg = tmpImg;
    updateDisplayImage( &tmpImg );

}

void MainWindow::resizeEvent(QResizeEvent* event)
{
   QMainWindow::resizeEvent(event);

   // Resize Area
   const QRect mainWind = this->geometry();
   QRect gtGeo;
   gtGeo.setWidth(mainWind.width()-45);
   gtGeo.setHeight(mainWind.height()-65);
   ui->pbExpPixs->setGeometry(gtGeo);
   ui->pbExpPixs->update();

   // Update Displayed Image
   //QString lastFileOpen    = readFileParam(_PATH_LAST_USED_IMG_FILENAME);
   //globalEditImg           = new QImage(lastFileOpen);
   //globalBackEditImg       = new QImage();
   //*globalBackEditImg      = *globalEditImg;
   //updatePreviewImage(globalBackEditImg);
   //*globalEditImg = diffImage;
   updateDisplayImage(globalEditImg);

}

void MainWindow::on_actionApply_Region_of_Interes_triggered()
{
    QString lastUsedImgFilename = readAllFile(_PATH_LAST_USED_IMG_FILENAME).trimmed();
    //qDebug() << "_PATH_LAST_USED_IMG_FILENAME: " << lastUsedImgFilename;
    QImage diffImage(lastUsedImgFilename);
    if( diffImage.isNull() )
    {
        qDebug() << "ERROR: Obtaining _PATH_LAST_USED_IMG_FILENAME";
        return (void)NULL;
    }
    else
    {
        //
        //Crop original image to release the usable area
        //
        //Get usable area coordinates
        squareAperture aperture;
        if( !rectangleInPixelsFromSquareXML( _PATH_REGION_OF_INTERES, &aperture ) )
        {
            funcShowMsg("ERROR","Loading _PATH_REGION_OF_INTERES");
            return (void)false;
        }

        //Crop and save
        //qDebug() << "x: " << aperture.rectX<< "y: " << aperture.rectY << "w: " << aperture.rectW<< "h: " << aperture.rectH;

        diffImage = diffImage.copy(QRect( aperture.rectX, aperture.rectY, aperture.rectW, aperture.rectH ));
        diffImage.save(_PATH_DISPLAY_IMAGE);

        //Save last image used
        saveFile(_PATH_LAST_USED_IMG_FILENAME,_PATH_DISPLAY_IMAGE);

        //Load Image Selected
        globalEditImg       = new QImage(_PATH_DISPLAY_IMAGE);
        globalBackEditImg   = new QImage(_PATH_DISPLAY_IMAGE);

        //Update Thumb and Edit Canvas
        updateDisplayImage(globalEditImg);


    }
}

void MainWindow::functionTakeComposedSquarePicture()
{
    mouseCursorWait();

    if( !takeRemoteSnapshot(_PATH_REMOTE_SNAPSHOT,false) )
    {
        qDebug() << "ERROR: Taking Diffration Area";
        return (void)NULL;
    }
    else
    {
        QImage diffImage = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
        if( diffImage.isNull() )
        {
            qDebug() << "ERROR: Obtaining Diffration Area";
            return (void)NULL;
        }
        else
        {
            if( !takeRemoteSnapshot(_PATH_REMOTE_SNAPSHOT,true) )
            {
                qDebug() << "ERROR: Taking Aperture Area";
                return (void)NULL;
            }
            else
            {
                QImage apertureImage = obtainImageFile( _PATH_REMOTE_SNAPSHOT, "" );
                if( apertureImage.isNull() )
                {
                    qDebug() << "ERROR: Obtaining Aperture Area";
                    return (void)NULL;
                }
                else
                {

                    //
                    //Merge Areas
                    //
                    funcMergeSquareAperture(&diffImage,&apertureImage);//Result is saved in diffImage

                    //
                    //Save cropped image
                    //
                    *globalEditImg = diffImage;
                    saveFile(_PATH_LAST_USED_IMG_FILENAME,_PATH_DISPLAY_IMAGE);
                    updateDisplayImage(globalEditImg);
                }
            }
        }
    }

    //progBarUpdateLabel("",0);
    mouseCursorReset();
}

int MainWindow::funcMergeSquareAperture(QImage* diffImage, QImage* apertureImage)
{
    //-------------------------------------------
    //Merge image
    //-------------------------------------------
    squareAperture *aperture        = (squareAperture*)malloc(sizeof(squareAperture));
    squareAperture *squareApertArea = (squareAperture*)malloc(sizeof(squareAperture));
    memset(aperture,'\0',sizeof(squareAperture));
    memset(squareApertArea,'\0',sizeof(squareApertArea));

    //
    //Crop original image to release the usable area
    //
    //Get Region of Interest
    if( !rectangleInPixelsFromSquareXML( _PATH_REGION_OF_INTERES, aperture ) )
    {
        funcShowMsg("ERROR","Loading Usable Area in Pixels: _PATH_SQUARE_USABLE");
        return _ERROR;
    }
    //Get usable area coordinates
    if( !rectangleInPixelsFromSquareXML( _PATH_SQUARE_APERTURE, _PATH_REGION_OF_INTERES, squareApertArea ) )
    {
        funcShowMsg("ERROR","Loading Usable Area in Pixels: _PATH_SQUARE_USABLE");
        return _ERROR;
    }

    //
    // Crop taken images
    //
    *diffImage       = diffImage->copy(QRect( aperture->rectX, aperture->rectY, aperture->rectW, aperture->rectH ));
    *apertureImage   = apertureImage->copy(QRect( aperture->rectX, aperture->rectY, aperture->rectW, aperture->rectH ));

    //
    //Copy square aperture into diffraction image
    //
    for( int y=squareApertArea->rectY; y<=(squareApertArea->rectY+squareApertArea->rectH); y++ )
    {
        for( int x=squareApertArea->rectX; x<=(squareApertArea->rectX+squareApertArea->rectW); x++ )
        {
            diffImage->setPixel( x, y, apertureImage->pixel( x, y ) );
        }
    }

    return _OK;

}

void MainWindow::on_actionAbout_this_triggered()
{
    QDesktopServices::openUrl(QUrl("https://dotechmx.wordpress.com/category/his-diy/square/", QUrl::TolerantMode));
}

void MainWindow::on_pbHyperRefresh_clicked()
{
    mouseCursorWait();

    QString exportedHypcbes(_PATH_TMP_HYPCUBES);
    funcUpdateSpectralPixels(&exportedHypcbes);

    ui->slideChangeImage->valueChanged( ui->slideChangeImage->value() );

    mouseCursorReset();
}

void MainWindow::on_actionSquare_aperture_triggered()
{

    QString tmpPath;
    if( funcLetUserSelectFile( &tmpPath, "Select ROI image", this ) != _OK )
    {
        funcShowMsgERROR_Timeout("Select ROI Source");
        return (void)false;
    }
    QImage imgDiff(tmpPath);

    tmpPath.clear();
    if( funcLetUserSelectFile( &tmpPath, "Select APERTURE image", this ) != _OK )
    {
        funcShowMsgERROR_Timeout("Select APERTURE Source");
        return (void)false;
    }
    QImage imgAperture(tmpPath);

    //
    //Merge Areas
    //
    funcMergeSquareAperture(&imgDiff,&imgAperture);//Result is saved in diffImage

    //
    //Save cropped image
    //
    *globalEditImg = imgDiff;
    saveFile(_PATH_LAST_USED_IMG_FILENAME,_PATH_DISPLAY_IMAGE);
    updateDisplayImage(globalEditImg);

}

void MainWindow::updateGlobalImgEdith(QImage* newImage)
{
    *globalEditImg = *newImage;
}

void MainWindow::on_actionExtract_Aperture_triggered()
{
    mouseCursorWait();

    //Get Calibration
    lstDoubleAxisCalibration daCalib;
    funcGetCalibration(&daCalib);

    //Get lower wavelength
    strDiffProj diffProj;
    diffProj.wavelength = daCalib.minWavelength;

    //Display the areas usable an the correspondign reflected area for selected wavelenght
    QImage origImg( _PATH_DISPLAY_IMAGE );

    //New size
    double lstX[4];
    double lstY[4];
    int newW, newH;
    //----------Left-Up
    diffProj.x  = 1;
    diffProj.y  = 1;
    funcInternCalcDiff(&diffProj, &daCalib);
    lstX[0]     = diffProj.x;
    lstY[0]     = diffProj.y;
    //----------Right-Up
    diffProj.x  = daCalib.squareUsableW;
    diffProj.y  = 1;
    funcInternCalcDiff(&diffProj, &daCalib);
    lstX[1]     = diffProj.x;
    lstY[1]     = diffProj.y;
    //----------Left-Down
    diffProj.x  = 1;
    diffProj.y  = daCalib.squareUsableH;
    funcInternCalcDiff(&diffProj, &daCalib);
    lstX[2]     = diffProj.x;
    lstY[2]     = diffProj.y;
    //----------Right-Down
    diffProj.x  = daCalib.squareUsableW;
    diffProj.y  = daCalib.squareUsableH;
    funcInternCalcDiff(&diffProj, &daCalib);
    lstX[3]     = diffProj.x;
    lstY[3]     = diffProj.y;
    //----------Min and Max
    int minX, maxX, minY, maxY;
    minX = static_cast<int>(vectorMin(lstX,4));
    minY = static_cast<int>(vectorMin(lstY,4));
    maxX = static_cast<int>(vectorMax(lstX,4));
    maxY = static_cast<int>(vectorMax(lstY,4));
    newW        = maxX - minX + 1;
    newH        = maxY - minY + 1;
    //QImage sA(newW,newH,QImage::Format_ARGB32);
    QImage sA(daCalib.squareUsableW,daCalib.squareUsableH,QImage::Format_ARGB32);
    for(int i=0; i<sA.width(); i++)
        for(int j=0; j<sA.height(); j++)
            sA.setPixel(i,j,qRgba(255,0,0,0));


    //Verticales
    int x, y, saX, saY;
    for(y=1; y<=daCalib.squareUsableH; y++)
    {
        for(x=1; x<=daCalib.squareUsableW; x++)
        {
            diffProj.x = x;
            diffProj.y = y;
            funcInternCalcDiff(&diffProj, &daCalib);
            saX = diffProj.x - minX;
            saY = diffProj.y - minY;
            //sA.setPixel(saX,saY,origImg.pixel(diffProj.x,diffProj.y));
            sA.setPixel(x-1,y-1,origImg.pixel(diffProj.x,diffProj.y));
            //origImg.setPixel(diffProj.x,diffProj.y,qRgb(255,0,0));
        }
    }

    //Save if required
    QString savingPath;
    QString fileName;
    if(
            funcLetUserDefineFile(
                                    &savingPath,
                                    "Save as...",
                                    ".png",
                                    new QString(_PATH_LAST_IMG_SAVED),
                                    new QString(_PATH_LAST_IMG_SAVED),
                                    this
                                 ) == _OK
    ){
        sA.save(savingPath);
    }
    mouseCursorReset();
    displayImageFullScreen(sA);
}

int MainWindow::funcInternCalcDiff(strDiffProj* diffProj, lstDoubleAxisCalibration *daCalib)
{
    int shiftX      = static_cast<int>(daCalib->LR.vertA+(daCalib->LR.vertB*(diffProj->y + daCalib->squareUsableY)));//Verticales
    diffProj->x     = diffProj->x + shiftX;//Verticales
    diffProj->y     = diffProj->y + static_cast<int>(daCalib->LR.horizA+(daCalib->LR.horizB*(diffProj->x)));//Horizontales
    calcDiffProj(diffProj,daCalib);
    return _OK;
}
