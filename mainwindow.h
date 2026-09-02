#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <serialporthandler.h>
#include <QMessageBox>
#include <QFile>
#include <QDateTime>
#include <QTimer>
#include <windows.h>
#include <psapi.h>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QApplication>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QLabel>
#include <QScreen>
#include <QInputDialog>

#include <QScrollArea>

#include <enlargeplot.h>
#include "xlsxdocument.h"   // QXlsx header

#include <complex>
#include <vector>
#include <cmath>
#include "kiss_fft.h"
#include <QString>

#include <windows.h>
#include <psapi.h>

#include <functional>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


// Forward declaration of serialPortHandler
class serialPortHandler;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void refreshPorts();

    //For Saving log Data
    void resetLogFile();
    static void writeToNotes(const QString &data);
    void initializeLogFile();
    void closeLogFile();

    quint8 calculateChecksum(const QByteArray &data);
    QString hexBytes(QByteArray &cmd);

    //Extra features
    void printMemoryUsage();

    void elapseStart();
    void elapseEnd(bool goFurther = false, const QString &label = "");

    QDialog* createPleaseWaitDialog(const QString &text, int timeSeconds = 0);

    inline void pauseFor(int milliseconds) {
        QEventLoop loop;
        QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);  // After delay, quit the event loop
        loop.exec();  // Start the event loop and wait for it to quit
        QApplication::processEvents();  // Keep UI healthy
    }

    void applyScrollArea();

    float bytesToFloatMSB(const QByteArray &bytes,bool Endian = false)
    {
        if(bytes.size() < 4)
            return 0.0f;

        quint32 raw;
        if(Endian == false)
        {
            raw =
                    (static_cast<quint8>(bytes[3]) << 24) |
                    (static_cast<quint8>(bytes[2]) << 16) |
                    (static_cast<quint8>(bytes[1]) << 8)  |
                    static_cast<quint8>(bytes[0]);
        }

        if(Endian == true)
        {
            raw =
                    (static_cast<quint8>(bytes[0]) << 24) |
                    (static_cast<quint8>(bytes[1]) << 16) |
                    (static_cast<quint8>(bytes[2]) << 8)  |
                    static_cast<quint8>(bytes[3]);
        }

        float value;
        memcpy(&value, &raw, sizeof(float));

        return value;
    }

    void initializeAllPlots();

    void makePacket32UI(QList<QByteArray> &rawPacket32List);
    void makePacket2048AdxlTempListPressureList(QList<QByteArray> &rawPacket4100AdxlList,
                                                QList<QByteArray> &rawPacketTemperatureList,QList<QByteArray> &rawPacketPressureList);



   // FFT helpers as class member functions

    void on_pushButton_clearPoints_fft_clicked();

    void on_pushButton_fitToScreen_fft_clicked();

    void blinkLabel(QLabel *label,
                    int durationMs,
                    const QString &text);

    // CSV Dumping For New Kumar's Application
    QString createAdxlCsvPath(bool live = false);

    void startAdxlCsvSaving(
            std::function<void()> onFinished);

    bool saveAdxlToCsv(
            const QVector<double> &sampleIndex,

            const QVector<double> &x1Adxl,
            const QVector<double> &y1Adxl,
            const QVector<double> &z1Adxl,

            const QVector<double> &x2Adxl,
            const QVector<double> &y2Adxl,
            const QVector<double> &z2Adxl,

            const QVector<double> &tempIndex,
            const QVector<double> &temperature,

            const QVector<double> &pressureIndex,
            const QVector<double> &pressure,

            const QString &filePath);

    //Live plotting functions
    void processLivePacket(const QByteArray &payload);

    // Live CSV member functions
    void startLiveCsv();

    void appendLiveCsv();

    void finishLiveCsv();

    void clearLiveCsvBuffer();

    void updateDisplayBuffer(
            const QVector<double> &source,
            QVector<double> &display,
            int currentWriteIndex);

    // FFT Concurrent
    struct FFTResult
    {
        QVector<double> freq;
        QVector<double> mag;
        QCustomPlot *plot;
    };

    FFTResult computeFFT(const QVector<double>& signal,
                                     double Fs,
                                     QCustomPlot *plot);

    // FFT Saving Concurrent
    struct FFTCsvData
    {
        QString name;
        QVector<double> freq;
        QVector<double> mag;
    };

    void clearPeakValues();

    void debugMalformedLivePacket(
            const QByteArray &packet);

private slots:
        void onPortSelected(const QString &portName);

        void portStatus(const QString&);

        void showGuiData(const QByteArray &byteArrayData);

        //response time handling

        void handleTimeout();

        void onDataReceived();

        void on_pushButton_calibrateScreen_clicked();

        void on_pushButton_getEventData_clicked();

        void on_pushButton_startLog_clicked();

        void on_pushButton_getLogEvents_clicked();

        void on_pushButton_enlargePlot_clicked();

        void on_pushButton_fitToScreen_clicked();

        void on_pushButton_saveLogPlots_clicked();

        void on_pushButton_clearLogPlots_clicked();

        void on_pushButton_clearPlots_clicked();

        void on_pushButton_remainingLogs_clicked();

        void blinkWidget(QWidget *w);

        void on_pushButton_currentParameters_clicked();

        //fft functions
        void applyHanning(QVector<double> &signal);

        void performFFT(const QVector<double> &input,
                        QVector<double> &magnitude,
                        QVector<double> &freqAxis,
                        double sampleRate);

        void removeDC(QVector<double> &x);

        // The below function is not calling (Because due to multithreading we are using computeFFT)
        // : just keeping it for reference cuz it just need signal vector
        // of time domain, and Fs computes from sample and our QCustomPlot
        void computeAndPlotFFT(const QVector<double>& signal,
                               double Fs,
                               QCustomPlot *plot);

       void setupFFTPlot(QCustomPlot *plot, const QString &xLabel);


       void on_pushButton_erase_clicked();

       void on_pushButton_stopLivePlot_clicked();
       
       void on_pushButton_startLive_clicked();

       void on_pushButton_fitToScreenLive_clicked();

       void on_pushButton_openFiles_clicked();

       void on_pushButton_setCurrentParameters_clicked();

       void on_pushButton_LoadFFT_clicked();

       void on_pushButton_clearFFTplots_clicked();

       void on_pushButton_saveFFTplots_clicked();


       void on_pushButton_clearLivePlots_clicked();

       void batteryCommand();

       void showBatteryInUi(const QByteArray& battBytes, bool strangeCase = false);

       //Debug live packets
       bool isPacketLoggingEnabled()
       {
           QString configPath =
                   QCoreApplication::applicationDirPath() + "/config.txt";

           QFile file(configPath);

           if(!file.exists())
           {
               return false;
           }

           if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
           {
               return false;
           }

           QString value =
                   QString::fromUtf8(file.readLine()).trimmed();

           return value == "1";
       }

       void loadAdxlBiasValues();

signals:
    void sendMsgId(quint8 id);


private:
    Ui::MainWindow *ui;
    serialPortHandler *serialObj;
    QTimer timer;

    QCustomPlot *fftPlot;
    QList<QCPItemTracer*> fftTracers;
    QList<QCPItemText*>   fftLabels;


    //blinkLabel timers
    QHash<QLabel*, QTimer*> blinkTimers;


    //Log handling
    static QFile logFile;
    static QTextStream logStream;
    void setupPlot(QCustomPlot *plot, const QString &xLabel, const QString &yLabel,bool noClearGraph=0);


    //Response Time waiting timer
     QTimer *responseTimer = nullptr; // Timer to track response timeout

    //Extras
     QElapsedTimer elapsedTimer;

     QDialog *dlg = nullptr;

     QDialog *dlgPlot = nullptr;

     QDialog *eraseDlg=nullptr;


     //New Code Adxl*2 code variables Start------------------------------------

     QVector<double> finalSampleIndexNew;

     QVector<double> finalX1AdxlNew;
     QVector<double> finalY1AdxlNew;
     QVector<double> finalZ1AdxlNew;

     QVector<double> finalX2AdxlNew;
     QVector<double> finalY2AdxlNew;
     QVector<double> finalZ2AdxlNew;

     QVector<double> finalTempIndexNew;
     QVector<double> finalTemperatureNew;

     QVector<double> finalPressureIndexNew;
     QVector<double> finalPressureNew;

     //New Code Adxl*2 code variables End------------------------------------


     QList<QByteArray> packet32List;
     QList<QByteArray> packet2048AdxlList;

     QList<QByteArray> packetTemperatureList;
     QList<QByteArray> packetPressureList;

     quint16 eventId;
     QString formattedStart;
     QString formattedEnd;
     quint8 unitNo;
     quint16 accFrequency = 0;

     // CSV load files
     struct CsvPlotData
     {
         bool isLiveCsv = false;
         bool isDownloadedCsv = false;

         int accFrequency = 0;
         QVector<double> sampleIndex;

         QVector<double> x1Loaded;
         QVector<double> y1Loaded;
         QVector<double> z1Loaded;

         QVector<double> x2Loaded;
         QVector<double> y2Loaded;
         QVector<double> z2Loaded;

         QVector<double> tempLoaded;
         QVector<double> pressureLoaded;
     };

     CsvPlotData loadAdxlCsv(
             const QString &filePath);

     // Live Plot Variables
     quint64 liveSampleNumber = 0;

     quint64 liveTempSampleNumber = 0;

     quint64 LIVE_WINDOW = 10000;

     float adxl100RangeMin = -100.0;
     float adxl500RangeMin = -500.0;
     float pressureRangeMin = 0;

     float adxl100RangeMax = 100.0;
     float adxl500RangeMax = 500.0;
     float pressureRangeMax = 2000.0;

     QList<QCustomPlot*> livePlots;

     // Circular Buffers
     QVector<double> plotX;

     QVector<double> plotAx100;
     QVector<double> plotAy100;
     QVector<double> plotAz100;

     QVector<double> plotAx500;
     QVector<double> plotAy500;
     QVector<double> plotAz500;

     QVector<double> displayAx100;
     QVector<double> displayAy100;
     QVector<double> displayAz100;

     QVector<double> displayAx500;
     QVector<double> displayAy500;
     QVector<double> displayAz500;

     quint64 writeIndex;

     QVector<double> plotPressure;
     QVector<double> displayPressure;
     quint64 pressureWriteIndex;

     // Live CSV Things
     QFile liveCsvFile;

     QTextStream liveCsvStream;

     QString liveCsvPath;

     CsvPlotData liveCsvData;

     bool liveCsvStarted = false;

     double liveSamplePeriodUS = 1.0;

     // For Bias values reading from config.txt
     double x1Bias = 1.6531875;
     double y1Bias = 1.654625;
     double z1Bias = 1.654375;

     double x2Bias = 1.66025;
     double y2Bias = 1.6666875;
     double z2Bias = 1.6663125;

     // For Peak Detection
     double peakAx100 = std::numeric_limits<double>::lowest();
     double peakAy100 = std::numeric_limits<double>::lowest();
     double peakAz100 = std::numeric_limits<double>::lowest();

     double peakAx500 = std::numeric_limits<double>::lowest();
     double peakAy500 = std::numeric_limits<double>::lowest();
     double peakAz500 = std::numeric_limits<double>::lowest();

     double peakPressure = std::numeric_limits<double>::lowest();

     // FFT Offline loading
     CsvPlotData loadedCsvData;
     bool csvLoaded = false;

     // Battery
     QTimer *batteryTimer;

     // For live plot debug
     int livePacketCount = 0;

     // Timer dialog flag
     bool timerDialogFlag = false;
};  
#endif // MAINWINDOW_H
