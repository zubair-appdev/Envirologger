#include "mainwindow.h"
#include "ui_mainwindow.h"

QFile MainWindow::logFile;
QTextStream MainWindow::logStream;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    serialObj =   new serialPortHandler(this);

    ui->dateTimeEdit->setDateTime(QDateTime(QDate(2025, 1, 1),
                                            QTime(0, 0, 0)));

    ui->spinBox_logTime->setRange(-3.402823466e+38F, 3.402823466e+38F);
    ui->spinBox_threshold->setRange(-3.402823466e+38F, 3.402823466e+38F);
    ui->spinBox_samplingfrequency->setRange(-3.402823466e+38F, 3.402823466e+38F);
    ui->spinBox_Inclinometer->setRange(-3.402823466e+38F, 3.402823466e+38F);

    ui->spinBox_logTime->setToolTip("Enter value from 1 to 10");
    ui->spinBox_threshold->setToolTip("Enter value from -200 to +200");
    ui->spinBox_samplingfrequency->setToolTip("Enter value from 1 to 20000");
    ui->spinBox_Inclinometer->setToolTip("Enter value from 1 to 1000");

    connect(ui->pushButton_clear,&QPushButton::clicked,ui->textEdit_rawBytes,&QTextEdit::clear);

    ui->comboBox_ports->addItems(serialObj->availablePorts());

    connect(ui->pushButton_portsRefresh,&QPushButton::clicked,this,&MainWindow::refreshPorts);

    connect(ui->comboBox_ports,SIGNAL(activated(const QString &)),this,SLOT(onPortSelected(const QString &)));

    connect(this,&MainWindow::sendMsgId,serialObj,&serialPortHandler::recvMsgId);
    //writeToNotes from serial class
    connect(serialObj,&serialPortHandler::executeWriteToNotes,this,&MainWindow::writeToNotes);

    //debugging signals
    connect(serialObj,&serialPortHandler::portOpening,this,&MainWindow::portStatus,Qt::QueuedConnection);

    //gui display signal
    connect(serialObj,&serialPortHandler::guiDisplay,this,&MainWindow::showGuiData);

    connect(ui->pushButton_fitToScreen_fft,&QPushButton::clicked,
            this,
            &MainWindow::on_pushButton_fitToScreen_fft_clicked);
    connect(ui->pushButton_clearPoints_fft,
            &QPushButton::clicked,
            this,
            &MainWindow::on_pushButton_clearPoints_fft_clicked);


    //reset previous notes #Notes things : Logging file
    resetLogFile();
    writeToNotes(+"    ******    "+QCoreApplication::applicationName() +
                 "     Application Started");
    //#################################################

    //Response Timer *********************************************##############
    responseTimer = new QTimer(this);
    responseTimer->setSingleShot(true); // Ensure it fires only once per use

    // Connect the timer's timeout signal to a slot that handles the timeout
    connect(responseTimer, &QTimer::timeout, this, &MainWindow::handleTimeout);

    connect(serialObj, &serialPortHandler::dataReceived, this, &MainWindow::onDataReceived);
    //************************************************************##############

    writeToNotes("Pointer Size: "+QString::number(sizeof(void *))+" If it is 8 : 64 bit else 4 means 32 bit");

    setWindowTitle("Envirologger");

    showMaximized();

    ui->tabWidget->setCurrentWidget(ui->tab_logger);


    initializeAllPlots();


    // Setting Table Get Log Events
    ui->tableWidget_getLogEvents->setColumnCount(3);
    ui->tableWidget_getLogEvents->setHorizontalHeaderLabels({"Event ID", "Start Time and Date", "End Time and Date"});

    ui->tableWidget_getLogEvents->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tableWidget_getLogEvents->setAlternatingRowColors(true);

    auto header = ui->tableWidget_getLogEvents->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);



}

MainWindow::~MainWindow()
{
    writeToNotes(+"    ******    "+QCoreApplication::applicationName() +
                 "     Application Closed");
    delete ui;
    delete serialObj;
    delete responseTimer;
    closeLogFile();
}

void MainWindow::initializeLogFile() {
    if (!logFile.isOpen()) {
        logFile.setFileName("debug_notes.txt");
        if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
            qCritical() << "Failed to open log file.";
        } else {
            logStream.setDevice(&logFile);
        }
    }
}

void MainWindow::resetLogFile() {
    // Close the log file if it is open
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }

    // Check if the file exists and delete it
    QFile::remove("debug_notes.txt");

    // Reinitialize the log file
    initializeLogFile();
}


void MainWindow::writeToNotes(const QString &data) {
    if (!logFile.isOpen()) {
        qCritical() << "Log file is not open.";
        return;
    }

    // Add a timestamp for each entry
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    logStream << "[" << timestamp << "] " << data << Qt::endl;
    logStream.flush(); // Ensure immediate write to disk
}

void MainWindow::closeLogFile() {
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }
}

quint8 MainWindow::calculateChecksum(const QByteArray &data)
{
    quint8 checkSum = 0;
    for(quint8 byte : data)
    {
        checkSum ^= byte;
    }

    return checkSum;
}

void MainWindow::refreshPorts()
{
    QString currentPort = ui->comboBox_ports->currentText();

    qDebug()<<"Refreshing ports...";
    ui->comboBox_ports->clear();
    QStringList availablePorts;
    ui->comboBox_ports->addItems(serialObj->availablePorts());

    ui->comboBox_ports->setCurrentText(currentPort);
}

void MainWindow::onPortSelected(const QString &portName)
{
    serialObj->setPORTNAME(portName);
}

void MainWindow::handleTimeout()
{
    QMessageBox::warning(this, "Timeout", "Hardware Not Responding!");
}

void MainWindow::onDataReceived()
{
    // Stop the timer since data has been received
    qDebug()<<"Hello stop it";
    if (responseTimer->isActive()) {
        responseTimer->stop();
    }
}


//The below function is intended for providing space between hex bytes
QString MainWindow::hexBytes(QByteArray &cmd)
{
    //**************************Visuals*******************
    QString hexOutput = cmd.toHex().toUpper();
    QString formattedHexOutput;

    for (int i = 0; i < hexOutput.size(); i += 2) {
        if (i > 0) {
            formattedHexOutput += " ";
        }
        formattedHexOutput += hexOutput.mid(i, 2);
    }
    return formattedHexOutput;
    //**************************Visuals*******************
}

void MainWindow::printMemoryUsage()
{
    PROCESS_MEMORY_COUNTERS_EX memInfo;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&memInfo, sizeof(memInfo))) {
        SIZE_T workingSet = memInfo.WorkingSetSize;
        SIZE_T privateUsage = memInfo.PrivateUsage;

        qDebug() << "Working Set (RAM):"
                 << workingSet / 1024 << "KB ("
                 << QString::number(workingSet / (1024.0 * 1024.0), 'f', 2) << "MB)";

        qDebug() << "Private Bytes:"
                 << privateUsage / 1024 << "KB ("
                 << QString::number(privateUsage / (1024.0 * 1024.0), 'f', 2) << "MB)";
    } else {
        qDebug() << "Failed to get memory info!";
    }
}

void MainWindow::elapseStart()
{
    elapsedTimer.start();
}

void MainWindow::elapseEnd(bool goFurther, const QString &label)
{
    qint64 ns = elapsedTimer.nsecsElapsed();
    double ms = ns / 1000000.0;

    if (label.isEmpty()) {
        qDebug() << "Time taken from elapseStart() to elapseEnd():"
                 << ns << "ns (" << ms << "ms)";
    } else {
        qDebug() << "Elapsed [" << label << "]:"
                 << ns << "ns (" << ms << "ms)";
    }

    if (!goFurther)
        elapsedTimer.restart();
}

QDialog* MainWindow::createPleaseWaitDialog(const QString &text, int timeSeconds)
{
    // --- Create dialog ---
    QDialog *dlg = new QDialog(this);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModal(true);

    // --- Styling ---
    dlg->setStyleSheet(R"(
        QDialog {
            background-color: #f8f8f8;
            border: 2px solid #0078D7;
            border-radius: 8px;
        }
        QLabel {
            font-size: 16px;
            padding: 10px;
        }
    )");

    // --- Layout and main label ---
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    QLabel *mainLabel = new QLabel(text, dlg);
    layout->addWidget(mainLabel);

    QLabel *timerLabel = nullptr;

    // --- Optional countdown ---
    if (timeSeconds > 0)
    {
        timerLabel = new QLabel(QString("Remaining: %1s").arg(timeSeconds), dlg);
        timerLabel->setAlignment(Qt::AlignCenter);
        timerLabel->setStyleSheet("color: #0078D7; font-weight: bold;");
        layout->addWidget(timerLabel);

        QTimer *countdown = new QTimer(dlg);
        countdown->setInterval(1000);

        int *remaining = new int(timeSeconds);

        QObject::connect(countdown, &QTimer::timeout, dlg, [countdown, remaining, timerLabel]() {
            (*remaining)--;
            if (*remaining <= 0)
            {
                countdown->stop();
                delete remaining;
            }
            else
            {
                timerLabel->setText(QString("Remaining: %1s").arg(*remaining));
            }
        });

        countdown->start();
    }

    dlg->setLayout(layout);
    dlg->adjustSize();
    dlg->setFixedSize(dlg->sizeHint());
    dlg->show();

    QApplication::processEvents(); // ensures dialog appears immediately

    return dlg;
}
void MainWindow::setupPlot(QCustomPlot *plot, const QString &xLabel, const QString &yLabel,bool noClearGraph)
{
    if(noClearGraph==true)
    {
        qDebug()<<"No need to clear graphs";
    }
    else
    {
        plot->clearGraphs();
    }

    plot->xAxis->setLabel(xLabel);
    plot->yAxis->setLabel(yLabel);
    plot->legend->setVisible(false);

    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    QFont labelFont("Segoe UI", 9, QFont::Bold);
    QFont tickFont("Segoe UI", 8);
    plot->xAxis->setLabelFont(labelFont);
    plot->yAxis->setLabelFont(labelFont);
    plot->xAxis->setTickLabelFont(tickFont);
    plot->yAxis->setTickLabelFont(tickFont);

    plot->setBackground(QColor(10, 20, 10));              // outer area - deep green
    plot->axisRect()->setBackground(QColor(15, 35, 15));  // inner plotting area

    QColor neonGreen(0, 255, 150);
    QColor softGreen(150, 255, 180);
    plot->xAxis->setLabelColor(neonGreen);
    plot->yAxis->setLabelColor(neonGreen);
    plot->xAxis->setTickLabelColor(softGreen);
    plot->yAxis->setTickLabelColor(softGreen);

    plot->xAxis->setBasePen(QPen(neonGreen, 1));
    plot->yAxis->setBasePen(QPen(neonGreen, 1));
    plot->xAxis->setTickPen(QPen(neonGreen, 1));
    plot->yAxis->setTickPen(QPen(neonGreen, 1));

    plot->xAxis->grid()->setPen(QPen(QColor(Qt::lightGray)));
    plot->yAxis->grid()->setPen(QPen(QColor(30, 60, 30)));
    plot->xAxis->grid()->setSubGridPen(QPen(QColor(20, 40, 20)));
    plot->yAxis->grid()->setSubGridPen(QPen(QColor(20, 40, 20)));
    plot->xAxis->grid()->setSubGridVisible(true);
    plot->yAxis->grid()->setSubGridVisible(true);

    plot->replot();
}
void MainWindow::setupFFTPlot(QCustomPlot *plot, const QString &xLabel)
{
    if (!plot) return;

    // ---------- NEON THEME ----------
    plot->setBackground(QColor(10, 20, 10));
    plot->axisRect()->setBackground(QColor(15, 35, 15));

    QColor neonGreen(0, 255, 150);
    QColor softGreen(150, 255, 180);

    plot->xAxis->setLabel(xLabel);
    plot->yAxis->setLabel("Amplitude(g)");

    // ---- Bold Axis Labels ----
    QFont labelFont("Arial", 10, QFont::Bold);
    plot->xAxis->setLabelFont(labelFont);
    plot->yAxis->setLabelFont(labelFont);

    plot->xAxis->setLabelColor(neonGreen);
    plot->yAxis->setLabelColor(neonGreen);
    plot->xAxis->setTickLabelColor(softGreen);
    plot->yAxis->setTickLabelColor(softGreen);

    plot->xAxis->setBasePen(QPen(neonGreen, 1));
    plot->yAxis->setBasePen(QPen(neonGreen, 1));
    plot->xAxis->setTickPen(QPen(neonGreen, 1));
    plot->yAxis->setTickPen(QPen(neonGreen, 1));

    plot->xAxis->grid()->setPen(QPen(QColor(30, 60, 30)));
    plot->yAxis->grid()->setPen(QPen(QColor(30, 60, 30)));
    plot->xAxis->grid()->setSubGridPen(QPen(QColor(20, 40, 20)));
    plot->yAxis->grid()->setSubGridPen(QPen(QColor(20, 40, 20)));
    plot->xAxis->grid()->setSubGridVisible(true);
    plot->yAxis->grid()->setSubGridVisible(true);

    // ---------- ADD EMPTY GRAPH ----------
    plot->addGraph();
    plot->graph(0)->setPen(QPen(neonGreen, 1));

    // ---------- INTERACTIONS ----------
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // ---------- CLICK-POINT SELECTION ----------
    connect(plot, &QCustomPlot::plottableClick,
            this,
            [this, plot](QCPAbstractPlottable *plottable, int index, QMouseEvent *)
    {
        if (!plottable) return;
        QCPGraph *g = qobject_cast<QCPGraph*>(plottable);
        if (!g) return;

        double x = g->data()->at(index)->key;
        double y = g->data()->at(index)->value;

        // ---- CREATE NEW TRACER ----
        QCPItemTracer *tr = new QCPItemTracer(plot);
        tr->setGraph(g);
        tr->setGraphKey(x);
        tr->setStyle(QCPItemTracer::tsCircle);
        tr->setPen(QPen(Qt::red));
        tr->setBrush(Qt::red);
        tr->setSize(7);

        // ---- CREATE NEW LABEL ----
        QCPItemText *lb = new QCPItemText(plot);
        lb->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
        lb->position->setParentAnchor(tr->position);
        lb->position->setCoords(10, -10);
        lb->setText(QString("f = %1 Hz\ng = %2")
                    .arg(x, 0, 'f', 2)
                    .arg(y, 0, 'f', 4));
        lb->setColor(QColor(0, 255, 150));
        lb->setFont(QFont("Arial", 10));

        // ---- STORE FOR LATER CLEAR ----
        fftTracers.append(tr);
        fftLabels.append(lb);

        plot->replot();
    });
}


void MainWindow::initializeAllPlots()
{
    // Common setup lambda


    //  Graph color themes — tuned for neon-green background
    QColor adxlColors[] = {
        QColor(255, 60, 60),    // ADXL X - Bright Red
        QColor(255, 180, 0),    // ADXL Y - Deep Amber
        QColor(120, 180, 255)   // ADXL Z - Soft Sky Blue
    };

    QColor inclinometerColors[] = {
        QColor(255, 100, 255),  // Inclinometer X - Vivid Magenta
        QColor(0, 255, 220)     // Inclinometer Y - Aqua Cyan
    };

    QColor tempColor(255, 255, 100); // Temperature - Bright Yellow

     //  ADXL X, Y, Z — voltages vs samples (1 unit = 100 ms)

    QString adxlFreqLabel = QString("Time (1 = %1 )").arg("N/A");

    // Apply to all 3 plots
    setupPlot(ui->customPlot_adxl_x, QString("ADXL X %1").arg(adxlFreqLabel), "Voltage (g)");
    setupPlot(ui->customPlot_adxl_y, QString("ADXL Y %1").arg(adxlFreqLabel), "Voltage (g)");
    setupPlot(ui->customPlot_adxl_z, QString("ADXL Z %1").arg(adxlFreqLabel), "Voltage (g)");


    ui->customPlot_adxl_x->addGraph(); ui->customPlot_adxl_x->graph(0)->setPen(QPen(adxlColors[0], 1));
    ui->customPlot_adxl_y->addGraph(); ui->customPlot_adxl_y->graph(0)->setPen(QPen(adxlColors[1], 1));
    ui->customPlot_adxl_z->addGraph(); ui->customPlot_adxl_z->graph(0)->setPen(QPen(adxlColors[2], 1));

    //  Inclinometer X, Y — degrees vs time (1 unit = 1)
    QString InclinometerFreqLabel = QString("Time (1 = %1)").arg("N/A");


    setupPlot(ui->customPlot_inclinometer_x, QString("Inclinometer Time X %1").arg(InclinometerFreqLabel), "Degrees (°)");
    setupPlot(ui->customPlot_inclinometer_y,  QString("Inclinometer Time Y %1").arg(InclinometerFreqLabel), "Degrees (°)");

    ui->customPlot_inclinometer_x->addGraph(); ui->customPlot_inclinometer_x->graph(0)->setPen(QPen(inclinometerColors[0], 1));
    ui->customPlot_inclinometer_y->addGraph(); ui->customPlot_inclinometer_y->graph(0)->setPen(QPen(inclinometerColors[1], 1));

    //  Temperature — Celsius vs samples

    setupPlot(ui->customPlot_temperature, QString("Samples %1").arg(adxlFreqLabel), "Temperature (°C)");

    ui->customPlot_temperature->addGraph(); ui->customPlot_temperature->graph(0)->setPen(QPen(tempColor, 1));

    setupFFTPlot(ui->customPlot_adxl_x_FFT, "ADXL X Frequency (Hz)");
    setupFFTPlot(ui->customPlot_adxl_y_FFT, "ADXL Y Frequency (Hz)");
    setupFFTPlot(ui->customPlot_adxl_z_FFT, "ADXL Z Frequency (Hz)");

    ui->customPlot_adxl_x_FFT->addGraph(); ui->customPlot_adxl_x_FFT->graph(0)->setPen(QPen(tempColor, 1));
    ui->customPlot_adxl_y_FFT->addGraph(); ui->customPlot_adxl_y_FFT->graph(0)->setPen(QPen(tempColor, 1));
    ui->customPlot_adxl_z_FFT->addGraph(); ui->customPlot_adxl_z_FFT->graph(0)->setPen(QPen(tempColor, 1));





    // Axis ranges start clean
    for (auto plot : {ui->customPlot_adxl_x, ui->customPlot_adxl_y, ui->customPlot_adxl_z,
                      ui->customPlot_inclinometer_x, ui->customPlot_inclinometer_y,
                      ui->customPlot_temperature,ui->customPlot_adxl_x_FFT,ui->customPlot_adxl_y_FFT,ui->customPlot_adxl_z_FFT})
    {
        plot->xAxis->setRange(0, 100);
        plot->yAxis->setRange(-5, 5);
        plot->replot();
    }
}

void MainWindow::makePacket32UI(QList<QByteArray> &rawPacket32List)
{
    if(rawPacket32List.size() == 1)
    {
        QByteArray Item1 = rawPacket32List[0];
        qDebug()<<Item1.toHex(' ').toUpper();

        // Extracting eventId
        quint8 highByte = static_cast<quint8>(Item1[2]);
        quint8 lowByte  = static_cast<quint8>(Item1[3]);

        quint16 eventId = (highByte << 8) | lowByte;

        ui->lineEdit_eventId->setText(QString::number(eventId));

        ui->lineEdit_eventId->setStyleSheet("background-color:yellow");

        QTimer::singleShot(500,[this](){
            ui->lineEdit_eventId->setStyleSheet("");
        });


        // Extracting frequency for Adxl and Inclinometer
        quint8 highByteAdxl = static_cast<quint8>(Item1[4]);
        quint8 lowByteAdxl  = static_cast<quint8>(Item1[5]);

        quint16 adxlFreq = (highByteAdxl << 8) | lowByteAdxl;
        this->adxlFreq=adxlFreq;

        quint8 highByteInclinometer = static_cast<quint8>(Item1[6]);
        quint8 lowByteInclinometer  = static_cast<quint8>(Item1[7]);

        quint16 InclinometerFreq = (highByteInclinometer << 8) | lowByteInclinometer;
        this->inclinometerFreq=inclinometerFreq;

        qDebug()<<adxlFreq<<" :adxlFreq";
        qDebug()<<InclinometerFreq<<" :InclinometerFreq";

        //Extracting Start Time and End Time

        // Bytes extraction
        QByteArray startTimeBytes = Item1.mid(20,6);
        QByteArray endTimeBytes = Item1.mid(26,6);

        // Bytes to Decimals conversion
        QVector<quint8> startTimeDecimals;
        for(auto eachByte : startTimeBytes)
        {
            startTimeDecimals.append(static_cast<quint8>(eachByte));
        }
        qDebug()<<startTimeDecimals<<" :startTimeDecimals";

        QVector<quint8> endTimeDecimals;
        for(auto eachByte : endTimeBytes)
        {
            endTimeDecimals.append(static_cast<quint8>(eachByte));
        }
        qDebug()<<endTimeDecimals<<" :endTimeDecimals";

        // Decimals to string conversion
        QStringList startTimeStringList;
        for(auto parts : startTimeDecimals)
        {
            startTimeStringList.append(QString::number(parts));
        }

        QString uiStartTime = startTimeStringList.join(":");
        qDebug()<<uiStartTime<<" :uiStartTime";

        QStringList endTimeStringList;
        for(auto parts : endTimeDecimals)
        {
            endTimeStringList.append(QString::number(parts));
        }

        QString uiEndTime = endTimeStringList.join(":");
        qDebug()<<uiEndTime<<" :uiEndTime";

        // Formatting the strings
        // Split the string by ':'
        QStringList startParts = uiStartTime.split(":");
        QStringList endParts   = uiEndTime.split(":");

        if (startParts.size() == 6 && endParts.size() == 6)
        {
            QString formattedStart = QString("%1:%2:%3_%4/%5/%6")
                    .arg(startParts[0]).arg(startParts[1]).arg(startParts[2])
                    .arg(startParts[3]).arg(startParts[4]).arg(startParts[5]);

            QString formattedEnd = QString("%1:%2:%3_%4/%5/%6")
                    .arg(endParts[0]).arg(endParts[1]).arg(endParts[2])
                    .arg(endParts[3]).arg(endParts[4]).arg(endParts[5]);

            qDebug() << formattedStart << " :formattedStart";
            qDebug() << formattedEnd   << " :formattedEnd";

            ui->lineEdit_startTime->setText(formattedStart);
            ui->lineEdit_endTime->setText(formattedEnd);

            QString displayAdxlfreq;
            QString displayInclinometerfreq;
            // Updating Labels
            if(adxlFreq < 101)
            {
                displayAdxlfreq=QString::number(1.0/adxlFreq)+" s";
            }
            else if(adxlFreq > 100 and adxlFreq < 5001 )
            {
                displayAdxlfreq=QString::number((1.0/adxlFreq)*1000)+" ms";
            }
            else if(adxlFreq > 5000 and adxlFreq < 20001)
            {
                displayAdxlfreq=QString::number((1.0/adxlFreq)*1000000)+" µs";
            }
            else
            {
                qDebug()<<"Invalid Adxl frequency";
            }


            if(InclinometerFreq < 101)
            {
                displayInclinometerfreq=QString::number(1.0/adxlFreq)+" s";
            }
            else if(InclinometerFreq > 100 and InclinometerFreq < 1001 )
            {
                displayInclinometerfreq=QString::number((1.0/adxlFreq)*1000)+" ms";
            }
            else
            {
                qDebug()<<"Invalid inclinometer frequency";
            }

            setupPlot(ui->customPlot_adxl_x,QString("ADXL X Time(1 = %1)").arg(displayAdxlfreq),"Acceleration(g)",1);
            setupPlot(ui->customPlot_adxl_y,QString("ADXL Y Time(1 = %1)").arg(displayAdxlfreq),"Acceleration(g)",1);
            setupPlot(ui->customPlot_adxl_z,QString("ADXL Z Time(1 = %1)").arg(displayAdxlfreq),"Acceleration(g)",1);


            setupPlot(ui->customPlot_temperature,QString("Samples Time(1 = %1 s)").arg(682.0/adxlFreq, 0, 'f', 4),"Temperature(°)",1);

            setupPlot(ui->customPlot_inclinometer_x,QString("Inclinometer X Time(1 = %1)").arg(displayInclinometerfreq),"Degrees(°)",1);
            setupPlot(ui->customPlot_inclinometer_y,QString("Inclinometer Y Time(1 = %1)").arg(displayInclinometerfreq),"Degrees(°)",1);

            // Ui blinking
            ui->lineEdit_startTime->setStyleSheet("background-color:yellow");

            QTimer::singleShot(500,[this](){
                ui->lineEdit_startTime->setStyleSheet("");
            });

            ui->lineEdit_endTime->setStyleSheet("background-color:yellow");

            QTimer::singleShot(500,[this](){
                ui->lineEdit_endTime->setStyleSheet("");
            });

        }
        else
        {
            qDebug() << "Invalid time format!";
        }
    }
    else
    {
       QMessageBox::warning(this,"Error","packet32List size is more than 1");
    }
}

void MainWindow::makePacket4100AdxlTempList(QList<QByteArray> &rawPacket4100AdxlList,
                                            QList<QByteArray> &rawPacketTemperatureList)
{
    QVector<double> sampleIndex;
    QVector<double> xAdxl, yAdxl, zAdxl;
    int globalSample = 1;

    // --- ADXL Data Processing ---
    for (int p = 0; p < rawPacket4100AdxlList.size(); ++p)
    {
        QByteArray packet = rawPacket4100AdxlList[p];

        if (packet.size() < 20)
        {
            qDebug() << "Skipping too short ADXL packet:" << packet.size();
            continue;
        }

        QByteArray trimmed = packet.mid(3);
        if (trimmed.size() > 3) trimmed.chop(3); // remove footer
        if (trimmed.size() > 2) trimmed.chop(2); // remove temperature bytes

        int usableSize = trimmed.size();
        if (usableSize < 6)
        {
            qDebug() << "Packet too short after trimming:" << usableSize;
            continue;
        }

        for (int i = 0; i + 5 < usableSize; i += 6)
        {
            qint16 xRaw = (static_cast<quint8>(trimmed[i])     << 8) | static_cast<quint8>(trimmed[i + 1]);
            qint16 yRaw = (static_cast<quint8>(trimmed[i + 2]) << 8) | static_cast<quint8>(trimmed[i + 3]);
            qint16 zRaw = (static_cast<quint8>(trimmed[i + 4]) << 8) | static_cast<quint8>(trimmed[i + 5]);

            // Keep last 12 bits only first 4 bits eliminate in a 16 bit integer
            xRaw &= 0x0FFF;
            yRaw &= 0x0FFF;
            zRaw &= 0x0FFF;

            sampleIndex.append(globalSample++);
            xAdxl.append((xRaw * 3.3 * 2) / 4096.0);
            yAdxl.append((yRaw * 3.3 * 2) / 4096.0);
            zAdxl.append((zRaw * 3.3 * 2) / 4096.0);
        }


        qDebug() << "Processed ADXL packet" << p << ", extracted" << usableSize / 6 << "samples";
    }
    for(int g=0;g<xAdxl.size();g++){
        xAdxl[g]=(xAdxl[g]-1.65)/0.0063;
        yAdxl[g]=(yAdxl[g]-1.65)/0.0063;
        zAdxl[g]=(zAdxl[g]-1.65)/0.0063;

    }

    qDebug() << "Total ADXL samples:" << sampleIndex.size();


    // --- Temperature Data Processing ---
    QVector<double> tempIndex;
    QVector<double> temperatureValues;

    for (int i = 0; i < rawPacketTemperatureList.size(); ++i)
    {
        QByteArray tempBytes = rawPacketTemperatureList[i];
        if (tempBytes.size() < 2)
        {
            qDebug() << "Skipping short temperature packet:" << tempBytes.size();
            continue;
        }

        quint16 tempRaw = (static_cast<quint8>(tempBytes[0]) << 8) | static_cast<quint8>(tempBytes[1]);
        tempIndex.append(i+1);

        // Keep first 14 bits only last 2 bits eliminate in a 16 bit integer
        tempRaw &= ~0x0003;

        temperatureValues.append(-46.85 + (175.72 * tempRaw) / 65536.0);
    }

    qDebug() << "Total temperature samples:" << temperatureValues.size();

    // --- Plotting Helper ---
    auto plotGraph = [](QCustomPlot *plot, const QVector<double> &x, const QVector<double> &y)
    {
        if (plot->graphCount() > 0)
        {
            plot->graph(0)->setData(x, y);
            plot->rescaleAxes();
            plot->replot();
        }
    };

    // --- Plot ADXL ---
   // QVector<double>freq=generateSineWave(500, 7.5, 2.0, 10000.0);


    plotGraph(ui->customPlot_adxl_x, sampleIndex, xAdxl);
    plotGraph(ui->customPlot_adxl_y, sampleIndex, yAdxl);
    plotGraph(ui->customPlot_adxl_z, sampleIndex, zAdxl);

    // --- Plot Temperature ---
    plotGraph(ui->customPlot_temperature, tempIndex, temperatureValues);

    // --- Passing local values to global values

    this->finalAdxlIndex = sampleIndex;
    this->finalXAdxl = xAdxl;
    this->finalYAdxl = yAdxl;
    this->finalZAdxl = zAdxl;

    this->finalTempIndex = tempIndex;
    this->finalTemperature = temperatureValues;

    // --- FFT Plot ---
    double Fs = adxlFreq;

   // testPureSineFFT(200000, 10000.0, 1000.0, ui->customPlot_adxl_z_FFT);

    qDebug() << "Debug 1";

    try {
        computeAndPlotFFT(xAdxl, Fs, ui->customPlot_adxl_x_FFT);
    }
    catch (std::exception &ex) {
        qCritical() << "computeAndPlotFFT exception:" << ex.what();
    }
    catch (...) {
        qCritical() << "computeAndPlotFFT unknown crash";
    }

    qDebug() << "Debug 2";


    computeAndPlotFFT(yAdxl, Fs, ui->customPlot_adxl_y_FFT);
    computeAndPlotFFT(zAdxl, Fs, ui->customPlot_adxl_z_FFT);

}
void MainWindow::makePacket4100InclList(QList<QByteArray> &rawPacket4100InclList)
{
    QVector<double> sampleIndex;
    QVector<double> inclX, inclY;
    int globalSample = 1;

    for (int p = 0; p < rawPacket4100InclList.size(); ++p)
    {
        QByteArray packet = rawPacket4100InclList[p];

        if (packet.size() < 20)
        {
            qDebug() << "Skipping too short Inclinometer packet:" << packet.size();
            continue;
        }

        // Remove header (3 bytes)
        QByteArray trimmed = packet.mid(3);

        // Remove footer (3 bytes)
        if (trimmed.size() > 3)
            trimmed.chop(3);

        // Remove last 2 dummy bytes before footer
        if (trimmed.size() > 2)
            trimmed.chop(2);

        int usableSize = trimmed.size();
        if (usableSize < 4)
        {
            qDebug() << "Packet too short after trimming:" << usableSize;
            continue;
        }

        // Process each 4-byte sample (Xg, Yg)
        for (int i = 0; i + 3 < usableSize; i += 4)
        {
            qint16 xRaw = (static_cast<quint8>(trimmed[i + 1])     << 8) | static_cast<quint8>(trimmed[i]);
            qint16 yRaw = (static_cast<quint8>(trimmed[i + 3]) << 8) | static_cast<quint8>(trimmed[i + 2]);

            // Convert to g-values
            double xg = (xRaw * 0.031) / 1000.0;
            double yg = (yRaw * 0.031) / 1000.0;

            // Clamp to [-1, 1]
            xg = std::max(-1.0, std::min(1.0, xg));
            yg = std::max(-1.0, std::min(1.0, yg));

            // Convert to degrees
            double xDeg = std::asin(xg) * (180.0 / M_PI);
            double yDeg = std::asin(yg) * (180.0 / M_PI);

            sampleIndex.append(globalSample++);
            inclX.append(xDeg);
            inclY.append(yDeg);
        }

        qDebug() << "Processed Incl packet" << p << ", extracted" << usableSize / 4 << "samples";
    }

    qDebug() << "Total Incl samples:" << sampleIndex.size();

    // --- Plotting ---
    auto plotGraph = [](QCustomPlot *plot, const QVector<double> &x, const QVector<double> &y)
    {
        if (plot->graphCount() > 0)
        {
            plot->graph(0)->setData(x, y);
            plot->rescaleAxes();
            plot->replot();
        }
    };

    plotGraph(ui->customPlot_inclinometer_x, sampleIndex, inclX);
    plotGraph(ui->customPlot_inclinometer_y, sampleIndex, inclY);

    // --- Passing local values to global values
    this->finalInclIndex = sampleIndex;
    this->finalInclX = inclX;
    this->finalInclY = inclY;

}

void MainWindow::saveAllSensorDataToExcel(const QVector<double> &adxlIndex,
                                          const QVector<double> &xAdxl,
                                          const QVector<double> &yAdxl,
                                          const QVector<double> &zAdxl,
                                          const QVector<double> &tempIndex,
                                          const QVector<double> &temperature,
                                          const QVector<double> &inclIndex,
                                          const QVector<double> &inclX,
                                          const QVector<double> &inclY)
{
    QXlsx::Document xlsx;

    // ---------- HEADER FORMAT ----------
    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    headerFormat.setBorderStyle(QXlsx::Format::BorderThin);

    // ---------- DATA FORMAT ----------
    QXlsx::Format dataFormat;
    dataFormat.setBorderStyle(QXlsx::Format::BorderThin);

    // ---------------- HEADERS ----------------
    xlsx.write("A1", "Samples", headerFormat);
    xlsx.write("B1", "ADXL X (g)",   headerFormat);
    xlsx.write("C1", "ADXL Y (g)",   headerFormat);
    xlsx.write("D1", "ADXL Z (g)",   headerFormat);

    xlsx.write("F1", "Temp Index",   headerFormat);
    xlsx.write("G1", "Temperature (°C)", headerFormat);

    xlsx.write("I1", "Incl Index",   headerFormat);
    xlsx.write("J1", "Incl X (deg)", headerFormat);
    xlsx.write("K1", "Incl Y (deg)", headerFormat);

    // ---------- COLUMN WIDTHS ----------
    xlsx.setColumnWidth(1, 1, 12);   // Index
    xlsx.setColumnWidth(2, 4, 15);   // ADXL X,Y,Z
    xlsx.setColumnWidth(6, 7, 15);   // Temperature
    xlsx.setColumnWidth(9, 11, 15);  // Inclinometer

    int row = 2;

    // ------------ ADXL Values --------------
    for (int i = 0; i < xAdxl.size(); i++)
    {
        xlsx.write(row, 1, adxlIndex[i], dataFormat);
        xlsx.write(row, 2, xAdxl[i],     dataFormat);
        xlsx.write(row, 3, yAdxl[i],     dataFormat);
        xlsx.write(row, 4, zAdxl[i],     dataFormat);
        row++;
    }

    // ------------ Temperature Values --------------
    int tRow = 2;
    for (int i = 0; i < temperature.size(); i++)
    {
        xlsx.write(tRow, 6, tempIndex[i],   dataFormat);
        xlsx.write(tRow, 7, temperature[i], dataFormat);
        tRow++;
    }

    // ------------ Inclinometer Values --------------
    int iRow = 2;
    for (int i = 0; i < inclX.size(); i++)
    {
        xlsx.write(iRow, 9,  inclIndex[i], dataFormat);
        xlsx.write(iRow, 10, inclX[i],     dataFormat);
        xlsx.write(iRow, 11, inclY[i],     dataFormat);
        iRow++;
    }

    // ---------------- SIMPLE FILE DIALOG ----------------
    QString defaultName = QString("SensorData_%1.xlsx")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    QString fullPath = QFileDialog::getSaveFileName(
                this,
                "Save Sensor Data",
                desktopPath + "/" + defaultName,
                "Excel Files (*.xlsx)"
    );

    if (fullPath.isEmpty()) {
        QMessageBox::information(this, "Save Cancelled",
                                 "User cancelled the file save operation.");
        return;
    }

    if (!fullPath.endsWith(".xlsx", Qt::CaseInsensitive))
        fullPath += ".xlsx";

    // ---------------- SAVE ----------------
    if (xlsx.saveAs(fullPath)) {
        QMessageBox::information(this, "Success",
                                 "Sensor data saved successfully at:\n" + fullPath);
    } else {
        QMessageBox::critical(this, "Save Failed",
                              "Failed to save Excel file.\nCheck permissions or try another location.");
    }
}


void MainWindow::initializeSensorVectors()
{
    // --- ADXL ---
    finalAdxlIndex.clear();
    finalXAdxl.clear();
    finalYAdxl.clear();
    finalZAdxl.clear();

    // --- Temperature ---
    finalTempIndex.clear();
    finalTemperature.clear();

    // --- Inclinometer ---
    finalInclIndex.clear();
    finalInclX.clear();
    finalInclY.clear();
}




void MainWindow::portStatus(const QString &data)
{
    if(data.startsWith("Serial object is not initialized/port not selected"))
    {
        if(dlgPlot)
        {
            dlgPlot->close();
            dlgPlot = nullptr;
        }

        QMessageBox::critical(this,"Port Error","Please Select Port Using Above Dropdown");
    }

    if(data.startsWith("Serial port ") && data.endsWith(" opened successfully at baud rate 921600"))
    {
        QMessageBox::information(this,"Success",data);
    }

    if(data.startsWith("Failed to open port"))
    {
        if(dlgPlot)
        {
            dlgPlot->close();
            dlgPlot = nullptr;
        }
        QMessageBox::critical(this,"Error",data);
    }

    if (data.startsWith("START"))
    {

        dlgPlot = createPleaseWaitDialog("⌛ Please Wait Loading Plot !!!");
        qDebug() << dlgPlot << " :dlgPlot start";
    }

    if (data.startsWith("STOP"))
    {

        if(dlgPlot)
        {
            dlgPlot->close();
            dlgPlot = nullptr;
        }
        qDebug() << dlgPlot << " :dlgPlot stop";
    }

    ui->textEdit_rawBytes->append(data);
}

void MainWindow::showGuiData(const QByteArray &byteArrayData)
{
    QByteArray data = byteArrayData;

    // Get Event Data Command mdgId 0x01
    if(data.startsWith(QByteArray::fromHex("AA BB")) && data.endsWith(QByteArray::fromHex("AA BB CC DD FF")))
    {
        int i = 0;

        QList<QByteArray> packet32List;
        QList<QByteArray> packet4100AdxlList;
        QList<QByteArray> packet4100InclList;
        QList<QByteArray> packetTemperatureList;

        int invalidHeaderCount = 0;

        while (i < data.size())
        {
            //  Make sure we have at least 3 bytes for a header
            if (i + 3 > data.size())
                break;

            QByteArray header = data.mid(i, 3);

            // --- Case 1: Packet32 ---
            if (header.startsWith(QByteArray::fromHex("AA BB")))
            {
                if (i + 32 <= data.size())
                {
                    QByteArray packet32 = data.mid(i, 32);

                    packet32List.append(packet32);
                    i += 32;
                    continue;
                }
                else break; // incomplete packet at end
            }

            // --- Case 2: Packet4100_ADXL ---
            else if (header == QByteArray::fromHex("CC DD FF"))
            {
                if (i + 4100 <= data.size())
                {
                    QByteArray packet4100 = data.mid(i, 4100);
                    if (packet4100.endsWith(QByteArray::fromHex("FF EE FF")))
                    {
                        if(packet4100.contains(QByteArray::fromHex("FF FF FF FF FF FF")))
                        {
                            // Special condition FF's checking
                            QByteArray specialPacket = packet4100;

                            qDebug()<<"Consecutive FF's detected at packet [ADXL]: "+QString::number(packet4100AdxlList.size());
                            writeToNotes("Consecutive FF's detected at packet [ADXL]: " + QString::number(packet4100AdxlList.size()));


                            int fIndex = specialPacket.indexOf(QByteArray::fromHex("FF FF FF FF FF FF"));
                            qDebug()<<fIndex<<" :fIndex";

                            qDebug()<< "Removing ff bytes count [ADXL]: " << (specialPacket.size() - fIndex) - 5;
                            writeToNotes("Removing ff bytes count [ADXL]: " + QString::number((specialPacket.size() - fIndex) - 5));

                            specialPacket.remove(fIndex,(specialPacket.size() - fIndex) - 5);


                            packet4100AdxlList.append(specialPacket);
                            qDebug()<<specialPacket.toHex(' ').toUpper()<<" :specialPacket";

                            // writeToNotes Log
                            writeToNotes("fIndex (start of FFs) [ADXL]: " + QString::number(fIndex));
                            writeToNotes("specialPacket [ADXL]: " + specialPacket.toHex(' ').toUpper());
                        }
                        else
                        {
                            // Normal condition
                            packet4100AdxlList.append(packet4100);
                        }

                        // Extract last 2 bytes before footer as temperature
                        QByteArray tempBytes = packet4100.mid(4100 - 5, 2);
                        packetTemperatureList.append(tempBytes);
                    }
                    else
                    {
                        invalidHeaderCount++;
                    }
                    i += 4100;
                    continue;
                }
                else break;
            }

            // --- Case 3: Packet4100_INCLINOMETER ---
            else if (header == QByteArray::fromHex("EE FF FF"))
            {
                if (i + 4100 <= data.size())
                {
                    QByteArray packet4100 = data.mid(i, 4100);
                    if (packet4100.endsWith(QByteArray::fromHex("FF CC DD")))
                    {
                        if(packet4100.contains(QByteArray::fromHex("FF FF FF FF FF FF")))
                        {
                            // Special condition FF's checking
                            QByteArray specialPacket = packet4100;

                            qDebug()<<"Consecutive FF's detected at packet [INCLINOMETER]: "+QString::number(packet4100InclList.size());
                            writeToNotes("Consecutive FF's detected at packet [INCLINOMETER]: " + QString::number(packet4100InclList.size()));


                            int fIndex = specialPacket.indexOf(QByteArray::fromHex("FF FF FF FF FF FF"));
                            qDebug()<<fIndex<<" :fIndex";

                            qDebug()<< "Removing ff bytes count [INCLINOMETER]: " << (specialPacket.size() - fIndex) - 5;
                            writeToNotes("Removing ff bytes count [INCLINOMETER]: " + QString::number((specialPacket.size() - fIndex) - 5));

                            specialPacket.remove(fIndex,(specialPacket.size() - fIndex) - 5);


                            packet4100InclList.append(specialPacket);
                            qDebug()<<specialPacket.toHex(' ').toUpper()<<" :specialPacket";

                            // writeToNotes Log
                            writeToNotes("fIndex (start of FFs) [INCLINOMETER]: " + QString::number(fIndex));
                            writeToNotes("specialPacket [INCLINOMETER]: " + specialPacket.toHex(' ').toUpper());
                        }
                        else
                        {
                            // Normal condition
                            packet4100InclList.append(packet4100);
                        }
                    }
                    else
                    {
                        invalidHeaderCount++;
                    }
                    i += 4100;
                    continue;
                }
                else break;
            }

            // --- Case 4: Unknown header ---
            else
            {
                // Unknown header found — treat as invalid frame
                int next = qMin(i + 4100, data.size());  // move by full packet size
                QByteArray maybeFooter = data.mid(next - 3, 3);

                qDebug() << "Unknown header" << header.toHex()
                         << "possible footer" << maybeFooter.toHex();

                writeToNotes("Unknown header: " + header.toHex(' ').toUpper() +
                              " | possible footer: " + maybeFooter.toHex(' ').toUpper());

                invalidHeaderCount++;
                i = next;  // skip full 4100 bytes
            }
        }

        // Summary logs
        qDebug() << " Packet32 count:" << packet32List.size();
        qDebug() << " Packet4100 ADXL count:" << packet4100AdxlList.size();
        qDebug() << " Packet4100 Inclinometer count:" << packet4100InclList.size();
        qDebug() << " Temperature samples:" << packetTemperatureList.size();
        qDebug() << " Invalid headers:" << invalidHeaderCount;

        if(packetTemperatureList.size() != 147)
        {
            qDebug()<<"Lesser Adxl/Temperature Packets Detected With Size : "<<packetTemperatureList.size();
            writeToNotes("Lesser Adxl/Temperature Packets Detected With Size : "+QString::number(packetTemperatureList.size()));

        }

        // writeToNotes log
        writeToNotes("Packet32 count: " + QString::number(packet32List.size()));
        writeToNotes("Packet4100 ADXL count: " + QString::number(packet4100AdxlList.size()));
        writeToNotes("Packet4100 Inclinometer count: " + QString::number(packet4100InclList.size()));
        writeToNotes("Temperature samples: " + QString::number(packetTemperatureList.size()));
        writeToNotes("Invalid headers: " + QString::number(invalidHeaderCount));


        //Making Packets
        makePacket32UI(packet32List);
        makePacket4100AdxlTempList(packet4100AdxlList,packetTemperatureList);
        makePacket4100InclList(packet4100InclList);

           if (dlgPlot) {
                qDebug()<<"Closing dialog: "<<dlgPlot;
                dlgPlot->close();
                dlgPlot = nullptr;
                qDebug()<<"Finished dialog: "<<dlgPlot;
              }


        QDialog *excelSavingDialog = createPleaseWaitDialog("⏳ Please Wait, Data Saving ...");

        saveAllSensorDataToExcel(
            finalAdxlIndex, finalXAdxl, finalYAdxl, finalZAdxl,
            finalTempIndex, finalTemperature,
            finalInclIndex, finalInclX, finalInclY
        );

        if(excelSavingDialog)
        {
            excelSavingDialog->close();
            excelSavingDialog = nullptr;
        }

    }
    // Get Event Data Command Nack Condition mdgId 0x01
    else if(data.startsWith(QByteArray::fromHex("53 54 45 FF")))
    {
        QMessageBox::warning(this,"Error","Invalid Event Id");
        writeToNotes(" ### Invalid Event Id ###");
        if(dlgPlot)
        {
            dlgPlot->close();
            dlgPlot = nullptr;
        }
    }

    // Start Log Initial Command msgId 0x02
    else if(data.startsWith(QByteArray::fromHex("54 53 41 43 4B"))&&data.endsWith("BAL"))
    {
        dlg = createPleaseWaitDialog("⏳ Please Wait Data Logging ...",ui->spinBox_logTime->value());
//        QTimer::singleShot(12000,[this](){
//            if(dlg)
//            {
//                dlg->close();
//                dlg = nullptr;
//                QMessageBox::critical(this,"Failed","Failed To Log Data !");
//            }
//        });
    }

    // Start Log End Initial Command msgId 0x02
    else if(data.startsWith(QByteArray::fromHex("54 53 50")))
    {
        if(dlg)
        {
            dlg->close();
            dlg = nullptr;
            QMessageBox::information(this,"Success","Successfully data logged !");
        }
    }

    // Get Log Events Command msgId 0x03
    else if (data.contains(QByteArray::fromHex("AA BB")) && data.contains(QByteArray::fromHex("65 6E 64 FF EF EE")))
    {
        QByteArray allData = data;

        // Clear and setup table
        ui->tableWidget_getLogEvents->clear();
        ui->tableWidget_getLogEvents->setRowCount(0);
        ui->tableWidget_getLogEvents->setColumnCount(3);
        ui->tableWidget_getLogEvents->setHorizontalHeaderLabels(
            QStringList() << "Event ID" << "Start Time and Date" << "End Time and Date"
        );

        int segmentCount = 0;
        int totalPacketsParsed = 0;
        int totalFFCount = 0;

        int startIndex = 0;
        while ((startIndex = allData.indexOf(QByteArray::fromHex("AA BB"), startIndex)) != -1)
        {
            int endIndex = allData.indexOf(QByteArray::fromHex("65 6E 64"), startIndex);
            if (endIndex == -1)
                break;

            QByteArray segment = allData.mid(startIndex, endIndex - startIndex + 3);
            startIndex = endIndex + 3;
            segmentCount++;

            // Find every 32-byte packet starting with AA BB
            int packetStart = 0;
            while ((packetStart = segment.indexOf(QByteArray::fromHex("AA BB"), packetStart)) != -1)
            {
                if (packetStart + 32 > segment.size())
                    break; // not enough data

                QByteArray packet = segment.mid(packetStart, 32);
                packetStart += 2; // move forward to avoid infinite loop

                // Stop on all FFs (padding area)
                if (packet.count(char(0xFF)) == 32)
                    break;

                // --- Extract Event ID ---
                quint8 msb = static_cast<quint8>(packet[2]);
                quint8 lsb = static_cast<quint8>(packet[3]);
                quint16 eventId = (msb << 8) | lsb;

                // --- Extract Start Time ---
                QByteArray startTimeBytes = packet.mid(20, 6);
                QStringList startParts;
                for (auto b : startTimeBytes)
                    startParts.append(QString("%1").arg(static_cast<quint8>(b), 2, 10, QChar('0')));
                QString formattedStart = QString("%1:%2:%3 %4/%5/%6")
                    .arg(startParts[0]).arg(startParts[1]).arg(startParts[2])
                    .arg(startParts[3]).arg(startParts[4]).arg(startParts[5]);

                // --- Extract End Time ---
                QByteArray endTimeBytes = packet.mid(26, 6);
                QStringList endParts;
                for (auto b : endTimeBytes)
                    endParts.append(QString("%1").arg(static_cast<quint8>(b), 2, 10, QChar('0')));
                QString formattedEnd = QString("%1:%2:%3 %4/%5/%6")
                    .arg(endParts[0]).arg(endParts[1]).arg(endParts[2])
                    .arg(endParts[3]).arg(endParts[4]).arg(endParts[5]);

                // --- Insert into Table ---
                int row = ui->tableWidget_getLogEvents->rowCount();
                ui->tableWidget_getLogEvents->insertRow(row);
                ui->tableWidget_getLogEvents->setItem(row, 0, new QTableWidgetItem(QString::number(eventId)));
                ui->tableWidget_getLogEvents->setItem(row, 1, new QTableWidgetItem(formattedStart));
                ui->tableWidget_getLogEvents->setItem(row, 2, new QTableWidgetItem(formattedEnd));

                totalPacketsParsed++;
            }

            // --- Count trailing FFs before the footer ---
            int footerIndex = segment.indexOf(QByteArray::fromHex("65 6E 64"));
            if (footerIndex > 0)
            {
                for (int i = footerIndex - 1; i >= 0; --i)
                {
                    if (static_cast<quint8>(segment[i]) == 0xFF)
                        totalFFCount++;
                    else
                        break;
                }
            }

        }

        qDebug() << "Segments found:" << segmentCount;
        qDebug() << "Total packets parsed:" << totalPacketsParsed;
        qDebug() << "Trailing FF count:" << totalFFCount;

        writeToNotes("Segments found: " + QString::number(segmentCount));
        writeToNotes("Total packets parsed: " + QString::number(totalPacketsParsed));
        writeToNotes("Trailing FF bytes count: " + QString::number(totalFFCount));
    }



    // Stop Plot Command msgId 0x04
    else if(data == QByteArray::fromHex("53 54 46"))
    {
        QDialog *excelSavingDialog = createPleaseWaitDialog("⏳ Please Wait, Data Saving ...");

        saveAllSensorDataToExcel(
            finalAdxlIndex, finalXAdxl, finalYAdxl, finalZAdxl,
            finalTempIndex, finalTemperature,
            finalInclIndex, finalInclX, finalInclY
        );

        if(excelSavingDialog)
        {
            excelSavingDialog->close();
            excelSavingDialog = nullptr;
        }

        QMessageBox::information(this,"Success","Plot stopped/saved successfully !");

    }
    else if(data.startsWith(QByteArray::fromHex("53 54 54"))&& data.size()==6){
            quint8 third = static_cast<quint8>(data[3]);
            quint8 fourth  = static_cast<quint8>(data[4]);

            quint16 remainingLogs = (third << 8) | fourth;

            QMessageBox::information(nullptr,
                                     "Remaining Logs",
                                     "Remaining log count: " + QString::number(remainingLogs));

    }
    else if (data.startsWith(QByteArray::fromHex("53 54 55")) && data.size() == 17)
    {
        QByteArray payload = data.mid(3);
        qDebug()<<payload.size()<<"**********************";
        quint8 logTime = static_cast<quint8>(payload[0]);

        QByteArray dtBytes = payload.mid(1, 6);
        qDebug()<<dtBytes.size()<<"timebytes";

        // ----------- 6 byte Date-Time ---------------
        // Change order to match dd-MM-yy HH:mm:ss

        quint8 day     = static_cast<quint8>(dtBytes[3]);
        quint8 month   = static_cast<quint8>(dtBytes[4]);
        quint8 year    = static_cast<quint8>(dtBytes[5]);   // year since 2000

        quint8 hour    = static_cast<quint8>(dtBytes[0]);
        quint8 minutes = static_cast<quint8>(dtBytes[1]);
        quint8 seconds = static_cast<quint8>(dtBytes[2]);

        // Build QDate and QTime
        QDate date(2000 + year, month, day);
        QTime time(hour, minutes, seconds);

        ui->dateTimeEdit->setDateTime(QDateTime(date, time));

        QByteArray g = payload.mid(7, 2);

        quint8 lsb = static_cast<quint8>(g[0]);  // byte0
        quint8 msb = static_cast<quint8>(g[1]);  // byte1

        qint16 threshold = (msb << 8) | lsb;
        QByteArray ADXL = payload.mid(9, 2);
        quint8 lsb_ADXL  = static_cast<quint8>(ADXL[0]);   // LSB
        quint8 msb_ADXL = static_cast<quint8>(ADXL[1]);
         quint16 ADXL_freq = (msb_ADXL<< 8) | lsb_ADXL;
         qDebug()<<"ADXL Received"<<ADXL.toHex();

        QByteArray inclinometer = payload.mid(11, 2);
        quint8 lsb_inc  = static_cast<quint8>(inclinometer[0]);   // LSB
        quint8 msb_inc = static_cast<quint8>(inclinometer[1]);
        quint16 inclinometer_val = lsb_inc | (msb_inc << 8);



        ui->spinBox_logTime->setValue(logTime);
        ui->spinBox_threshold->setValue(threshold);
         ui->spinBox_samplingfrequency->setValue(ADXL_freq);
        ui->spinBox_Inclinometer->setValue(inclinometer_val);

        blinkWidget(ui->spinBox_logTime);
        blinkWidget(ui->spinBox_threshold);
        blinkWidget(ui->dateTimeEdit);
        blinkWidget(ui->spinBox_samplingfrequency);
        blinkWidget(ui->spinBox_Inclinometer);


    }
    else if(data.startsWith(QByteArray::fromHex("54 53 41 43 4B"))&&data.endsWith(("RAJ"))){
        eraseDlg = createPleaseWaitDialog("⏳ Please Wait... !!!");

    }
    else if((data.startsWith(QByteArray::fromHex("54 53 44 4F")) && data.endsWith("RAJ"))){
        eraseDlg->close();
        eraseDlg = nullptr;
        QMessageBox::information(this,"erased","Data Erased successfully");
    }
    else if(data==QByteArray::fromHex("53 54 47")){
        QMessageBox::information(this,"Battery On","Battery in on condition");
    }
    else if(data==QByteArray::fromHex("53 54 48")){
        QMessageBox::information(this,"Battery off","Battery in off condition");
    }
     else if(data.startsWith(QByteArray::fromHex("53 54 44"))){
        QMessageBox::information(this,"logTime","Log time has set Successfully");

    }
    else if(data.startsWith(QByteArray::fromHex("53 54 51"))){
       QMessageBox::information(this,"Threshold","Threshold has set successfully.");
    }
    else if(data.startsWith(QByteArray::fromHex("53 54 49")))
    {
     QMessageBox::information(this,"DateTime","Date time has set successfully.");
    }
     else if(data.startsWith(QByteArray::fromHex("53 54 52")))
    {
       QMessageBox::information(this,"ADXL Frequency","ADXL frequency has set successfully.");
    }
    else if(data.startsWith(QByteArray::fromHex("53 54 53"))){
         QMessageBox::information(this,"Inclinometer","Inclinometer frequency has set successfully.");

    }
}




void MainWindow::on_pushButton_calibrateScreen_clicked()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QSize res = screen->size();

    QSettings settings("settings.ini", QSettings::IniFormat);

    QMessageBox::StandardButton choice = QMessageBox::question(
        this, "Calibrate Screen",
        "Do you want to enter custom screen details (width, height, diagonal) or reset to system default?",
        QMessageBox::Yes | QMessageBox::No);

    if (choice == QMessageBox::No) {
        // Reset to default
        settings.remove("Display/calibratedDPI");
        settings.remove("Display/width");
        settings.remove("Display/height");
        settings.remove("Display/diagonal");
        QMessageBox::information(this, "Calibration Removed",
                                 "Screen DPI reset to system default.\nRestart app to apply.");
        return;
    }

    // Custom input
    bool ok = false;
    int width = QInputDialog::getInt(this, "Screen Width",
                                     "Enter screen width (pixels):",
                                     res.width(), 100, 10000, 1, &ok);
    if (!ok) return;

    int height = QInputDialog::getInt(this, "Screen Height",
                                      "Enter screen height (pixels):",
                                      res.height(), 100, 10000, 1, &ok);
    if (!ok) return;

    double diagonalInches = QInputDialog::getDouble(
        this, "Screen Diagonal",
        "Enter screen diagonal size (in inches):",
        settings.value("Display/diagonal", 14.0).toDouble(), 3.0, 100.0, 1, &ok);
    if (!ok) return;

    // Calculate DPI
    double ppi = std::sqrt(width * width + height * height) / diagonalInches;

    // Save all values
    settings.setValue("Display/width", width);
    settings.setValue("Display/height", height);
    settings.setValue("Display/diagonal", diagonalInches);
    settings.setValue("Display/calibratedDPI", static_cast<int>(ppi));

    QMessageBox::information(this, "Calibration Done",
                             QString("Resolution: %1 x %2\nDiagonal: %3 in\nDPI set to %4.\nRestart app to apply.")
                             .arg(width).arg(height).arg(diagonalInches).arg(ppi, 0, 'f', 2));
}


void MainWindow::on_pushButton_getEventData_clicked()
{
    bool ok;
    QString text = ui->lineEdit_enterEventId->text().trimmed();
    int eventId = text.toInt(&ok);

    if(!ok || eventId < 0 || eventId > 65535)
    {
        QMessageBox::warning(this, "Error", "Please enter a valid Event ID (0–65535)");
        return;
    }
    
    
    initializeAllPlots();

    initializeSensorVectors();
    on_pushButton_clearPoints_fft_clicked();

    // Start the timeout timer
    responseTimer->start(2000); // 2 Sec timer

    QByteArray command;

    command.append(0x53); //1
    command.append(0x54); //2
    command.append(0x45); //3

    // Split eventId into 2 bytes (big-endian)
    command.append(static_cast<quint8>((eventId >> 8) & 0xFF)); // High byte 4
    command.append(static_cast<quint8>(eventId & 0xFF)); // Low byte 5

    command.append(0xFF); //6
    command.append(0xFF); //7


    qDebug() << "Get Event Data cmd sent : " + hexBytes(command);
    writeToNotes("Get Event Data cmd sent : " + hexBytes(command));

    emit sendMsgId(0x01);

    serialObj->writeData(command);

}

void MainWindow::on_pushButton_startLog_clicked()
{
    if(ui->spinBox_logTime->value()==0){
        QMessageBox::warning(this,"Failed","Please set the log time");
        return;
    }
    // Start the timeout timer
    responseTimer->start(2000); // 2 Sec timer

    QByteArray command;

    command.append(0x53); //1
    command.append(0x54); //2
    command.append(0x42); //3


    qDebug() << "Start Log cmd sent : " + hexBytes(command);
    writeToNotes("Start Log cmd sent : " + hexBytes(command));


    emit sendMsgId(0x02);
    serialObj->writeData(command);
}

void MainWindow::on_pushButton_getLogEvents_clicked()
{
    // Start the timeout timer
    responseTimer->start(2000); // 2 Sec timer

    QByteArray command;

    command.append(0x53); //1
    command.append(0x54); //2
    command.append(0x43); //3


    qDebug() << "Get Log Events cmd sent : " + hexBytes(command);
    writeToNotes("Get Log Events cmd sent : " + hexBytes(command));


    emit sendMsgId(0x03);
    serialObj->writeData(command);
}

void MainWindow::on_pushButton_stopPlot_clicked()
{
    // Start the timeout timer
    //responseTimer->start(2000); // 2 Sec timer

    QByteArray command;

    command.append(0x53); //1
    command.append(0x54); //2
    command.append(0x46); //3


    qDebug() << "Stop Plot cmd sent : " + hexBytes(command);
    writeToNotes("Stop Plot Events cmd sent : " + hexBytes(command));


    emit sendMsgId(0x04);
    //serialObj->writeData(command);

    (!finalAdxlIndex.isEmpty() &&
     !finalXAdxl.isEmpty() &&
     !finalYAdxl.isEmpty() &&
     !finalZAdxl.isEmpty() &&
     !finalTempIndex.isEmpty() &&
     !finalTemperature.isEmpty() &&
     !finalInclIndex.isEmpty() &&
     !finalInclX.isEmpty() &&
     !finalInclY.isEmpty())? saveAllSensorDataToExcel(
                                 finalAdxlIndex, finalXAdxl, finalYAdxl, finalZAdxl,
                                 finalTempIndex, finalTemperature,
                                 finalInclIndex, finalInclX, finalInclY
                             ):

        (void)QMessageBox::warning(this, "No Data", "vectors are empty!");
}

void MainWindow::on_pushButton_enlargePlot_clicked()
{
    QString selected = ui->comboBox_enlargePlot->currentText();
    QCustomPlot *plot = nullptr;

    if (selected == "ADXL_X")          plot = ui->customPlot_adxl_x;
    else if (selected == "ADXL_Y")     plot = ui->customPlot_adxl_y;
    else if (selected == "ADXL_Z")     plot = ui->customPlot_adxl_z;
    else if (selected == "Temperature") plot = ui->customPlot_temperature;
    else if (selected == "Inclinometer_X") plot = ui->customPlot_inclinometer_x;
    else if (selected == "Inclinometer_Y") plot = ui->customPlot_inclinometer_y;

    if (!plot)
    {
        QMessageBox::warning(this, "Warning", "Please select a valid plot to enlarge!");
        return;
    }

    // Create enlargePlot dialog and load the selected plot into it
    enlargePlot *dlg = new enlargePlot(this);
    dlg->loadPlot(plot);
    dlg->setModal(false);
    dlg->show();
}


void MainWindow::on_pushButton_fitToScreen_clicked()
{
    // Collect all plots
    QList<QCustomPlot*> allPlots = {
        ui->customPlot_adxl_x,
        ui->customPlot_adxl_y,
        ui->customPlot_adxl_z,
        ui->customPlot_inclinometer_x,
        ui->customPlot_inclinometer_y,
        ui->customPlot_temperature
    };

    // Iterate through each and fit accordingly
    for (QCustomPlot *plot : allPlots)
    {
        if (!plot) continue;

        bool hasData = false;
        for (int i = 0; i < plot->graphCount(); ++i)
        {
            if (plot->graph(i)->dataCount() > 0)
            {
                hasData = true;
                break;
            }
        }

        if (hasData)
        {
            //  Auto-fit to existing data
            plot->rescaleAxes(true);

            // Small padding for aesthetics
            plot->xAxis->scaleRange(1.05, plot->xAxis->range().center());
            plot->yAxis->scaleRange(1.05, plot->yAxis->range().center());
        }
        else
        {
            //  No data — reset to initial default view
            plot->xAxis->setRange(0, 100);
            plot->yAxis->setRange(-5, 5);
        }

        plot->replot();
    }

    qDebug() << "All plots adjusted — data-fitted if available, otherwise reset to default view.";
    writeToNotes("All plots adjusted — data-fitted if available, otherwise reset to default view.");
}


void MainWindow::on_pushButton_saveLogPlots_clicked()
{
    // Prepare default filename (EventsLogData_yyyyMMdd_HHmmss.xlsx)
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString defaultFileName = QString("EventsLogData_%1.xlsx").arg(timestamp);

    // Get desktop path as default location
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString defaultFullPath = desktopPath + "/" + defaultFileName;

    // Ask user where to save (with default pre-filled)
    QString selectedFile = QFileDialog::getSaveFileName(
        this,
        tr("Save Log Data"),
        defaultFullPath,
        tr("Excel Files (*.xlsx)")
    );

    if (selectedFile.isEmpty())
        return; // user cancelled

    // Ensure file ends with .xlsx
    if (!selectedFile.endsWith(".xlsx", Qt::CaseInsensitive))
        selectedFile += ".xlsx";

    // Create an Excel document
    QXlsx::Document xlsx;

    // Header format (just bold)
    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);

    // Write headers
    xlsx.write(1, 1, "Event ID", headerFormat);
    xlsx.write(1, 2, "Start Time and Date", headerFormat);
    xlsx.write(1, 3, "End Time and Date", headerFormat);

    // Write table data
    int rowCount = ui->tableWidget_getLogEvents->rowCount();
    int colCount = ui->tableWidget_getLogEvents->columnCount();

    for (int r = 0; r < rowCount; ++r)
    {
        for (int c = 0; c < colCount; ++c)
        {
            QTableWidgetItem *item = ui->tableWidget_getLogEvents->item(r, c);
            if (!item) continue;

            if (c == 0) // Event ID column should be integer
            {
                bool ok;
                int eventId = item->text().toInt(&ok);
                if (ok)
                    xlsx.write(r + 2, c + 1, eventId);
                else
                    xlsx.write(r + 2, c + 1, item->text());
            }
            else
            {
                xlsx.write(r + 2, c + 1, item->text());
            }
        }
    }

    // Auto fit columns
    xlsx.currentWorksheet()->setColumnWidth(2, 3, 20);

    // Save file
    if (xlsx.saveAs(selectedFile))
    {
        QMessageBox::information(this, "Success",
                                 " Log data saved successfully at:\n" + selectedFile);
    }
    else
    {
        QMessageBox::critical(this, "Error",
                              " Failed to save log data.\nPlease check permissions or path.");
    }
}



void MainWindow::on_pushButton_clearLogPlots_clicked()
{
    ui->tableWidget_getLogEvents->setRowCount(0);
    writeToNotes("table data cleared.");
}

void MainWindow::on_pushButton_clearPlots_clicked()
{
    // Clear all plot graphs
     QList<QCustomPlot*> allPlots = {
         ui->customPlot_adxl_x,
         ui->customPlot_adxl_y,
         ui->customPlot_adxl_z,
         ui->customPlot_inclinometer_x,
         ui->customPlot_inclinometer_y,
         ui->customPlot_temperature
     };

     for (QCustomPlot *plot : allPlots)
     {
         if (plot) {
             for (int i = 0; i < plot->graphCount(); ++i)
                 plot->graph(i)->data()->clear();
             plot->replot();
         }
     }

     writeToNotes("All log plots are cleared.");
}
void MainWindow::on_pushButton_fitToScreen_fft_clicked()
{
    QList<QCustomPlot*> allPlots = {
        ui->customPlot_adxl_x_FFT,
        ui->customPlot_adxl_y_FFT,
        ui->customPlot_adxl_z_FFT
    };

    for (QCustomPlot *plot : allPlots)
    {
        if (!plot) continue;

            bool hasData = false;

            for (int i = 0; i < plot->graphCount(); ++i)
            {
                if (plot->graph(i)->dataCount() > 0)
                {
                    hasData = true;
                    break;
                }
            }

            if (!hasData)
            {
                qDebug() << "Fit to screen failed: No data!";
                return;
            }

            plot->xAxis->rescale(true);
            plot->yAxis->rescale(true);

            plot->replot();
}
}


void MainWindow::on_pushButton_clearPoints_fft_clicked()
{

    auto clearPlot = [](QCustomPlot *plot)
    {
        if (!plot) return;

        // Loop backwards because removeItem changes index order
        for (int i = plot->itemCount() - 1; i >= 0; --i)
        {
            QCPAbstractItem *item = plot->item(i);

            if (qobject_cast<QCPItemTracer*>(item) ||
                qobject_cast<QCPItemText*>(item))
            {
                plot->removeItem(item);   // correct way to delete item
            }
        }

        plot->replot();
    };

    clearPlot(ui->customPlot_adxl_x_FFT);
    clearPlot(ui->customPlot_adxl_y_FFT);
    clearPlot(ui->customPlot_adxl_z_FFT);

    fftTracers.clear();
    fftLabels.clear();
}


void MainWindow::on_pushButton_logTime_clicked()
{
    responseTimer->start(2000);
    if(ui->spinBox_logTime->value()>10||ui->spinBox_logTime->value()<1){
        QMessageBox::information(this,"out of Range","Enter the value between 1 and 10");
        return;
    }
    QByteArray logTime = QByteArray::fromHex("535444");

    quint8 value = ui->spinBox_logTime->value();
    logTime.append(static_cast<char>(value));      // append value (1 byte)
    logTime.append(static_cast<char>(0xFF));

    serialObj->writeData(logTime);
    emit sendMsgId(0x10);


}

void MainWindow::on_pushButton_setthreshold_clicked()
{
    responseTimer->start(2000);
    if(ui->spinBox_threshold->value()>200||ui->spinBox_threshold->value()<-200){
        QMessageBox::information(this,"out of Range","Enter the value between -200 and 200");
        return;
    }
    QByteArray threshold = QByteArray::fromHex("535451");

    int input = ui->spinBox_threshold->value();   // int (0–65535)
    qint16 value = static_cast<qint16>(input);  // convert safely to 2 bytes

    threshold.append(static_cast<char>((value >> 8) & 0xFF)); // high byte first
    threshold.append(static_cast<char>(value & 0xFF));        // low byte second
    threshold.append(static_cast<char>(0xFF));
    qDebug()<<"value send"<<value;
    serialObj->writeData(threshold);
    emit sendMsgId(0x10);

}

void MainWindow::on_pushButton_setTime_clicked()
{
    responseTimer->start(2000);
    QDateTime dt = ui->dateTimeEdit->dateTime();

    int year  = dt.date().year();
    int month = dt.date().month();
    int day   = dt.date().day();

    int hour   = dt.time().hour();
    int minute = dt.time().minute();
    int second = dt.time().second();
    qDebug()<<year<<"year send";
    qDebug()<<minute;
    // Example: append to packet
    QByteArray packet=QByteArray::fromHex("535449");
    packet.append(static_cast<char>(day));
     packet.append(static_cast<char>(month));
    packet.append(static_cast<char>(year - 2000)); // if protocol needs 2-digit year
    packet.append(static_cast<char>(hour));
    packet.append(static_cast<char>(minute));
    packet.append(static_cast<char>(second));
    packet.append(static_cast<char>(0xFF));
    serialObj->writeData(packet);
    emit sendMsgId(0x10);


}

void MainWindow::on_pushButton_ADXLfrequency_clicked()
{
    responseTimer->start(2000);
    if(ui->spinBox_samplingfrequency->value()>20000||ui->spinBox_samplingfrequency->value()<1){
        QMessageBox::information(this,"out of Range","Enter the value between 1 and 20000");
        return;
    }
     QByteArray packet=QByteArray::fromHex("535452");
     quint16 value=ui->spinBox_samplingfrequency->value();
     packet.append(static_cast<char>((value >> 8) & 0xFF));
     packet.append(static_cast<char>(value & 0xFF));
     packet.append(static_cast<char>(0xFF));
     qDebug()<<"adxl sent"<<packet.toHex();
     serialObj->writeData(packet);
     emit sendMsgId(0x10);
}
void MainWindow::on_pushButton_inclinometerFrequency_clicked()
{
    responseTimer->start(2000);
  if(ui->spinBox_Inclinometer->value()>1000||ui->spinBox_Inclinometer->value()<1){
      QMessageBox::information(this,"out of Range","Enter the value between 1 and 1000");
      return;
  }
  QByteArray packet=QByteArray::fromHex("535453");
  quint16 value=ui->spinBox_Inclinometer->value();
  packet.append(static_cast<char>((value >> 8) & 0xFF));
  packet.append(static_cast<char>(value & 0xFF));
  packet.append(static_cast<char>(0xFF));
  serialObj->writeData(packet);
  emit sendMsgId(0x10);

}

void MainWindow::on_pushButton_remainingLogs_clicked()
{
    responseTimer->start(2000);
    QByteArray packet=QByteArray::fromHex("535454");
    serialObj->writeData(packet);
     emit sendMsgId(0x05);
}
void MainWindow::on_pushButton_currentParameters_clicked()
{
    responseTimer->start(2000);
    QByteArray packet=QByteArray::fromHex("535455");
    serialObj->writeData(packet);
     emit sendMsgId(0x06);

}
void MainWindow::on_pushButton_erase_clicked()
{
    responseTimer->start(2000);
    QByteArray eraseCmd=QByteArray::fromHex("535441");
    serialObj->writeData(eraseCmd);
    emit sendMsgId(0x07);

}
void MainWindow::on_pushButton_on_clicked()
{
    responseTimer->start(2000);
    QByteArray packet=QByteArray::fromHex("535447");
    serialObj->writeData(packet);
     emit sendMsgId(0x08);

}

void MainWindow::on_pushButton_off_clicked()
{
    responseTimer->start(2000);
    QByteArray packet=QByteArray::fromHex("535448");
    serialObj->writeData(packet);
    emit sendMsgId(0x09);

}

void MainWindow::blinkWidget(QWidget *w)
{
    if (!w) return;

    w->setStyleSheet("background-color: yellow;");

    QTimer::singleShot(200, [w]() {
        w->setStyleSheet("");
    });
}
void MainWindow::removeDC(QVector<double> &x)
{
    if (x.isEmpty()) return;

    double sum = 0;
    for (double v : x) sum += v;
    double mean = sum / x.size();

    for (double &v : x) v -= mean;
}



void MainWindow::applyHanning(QVector<double> &signal)
{
    const int N = signal.size();

    // --- Guard for invalid or trivial cases ---
    if (N <= 1)
    {
        qWarning() << "applyHanning: signal too short (N =" << N << ")";
        return;
    }

    // --- Precompute constant factor ---
    const double coeff = 2.0 * M_PI / static_cast<double>(N - 1);

    for (int n = 0; n < N; ++n)
    {
        const double w = 0.5 * (1.0 - std::cos(coeff * n));
        signal[n] *= w;
    }
}


void MainWindow::performFFT(const QVector<double> &input,
                            QVector<double> &magnitude,
                            QVector<double> &freqAxis,
                            double sampleRate)
{
    int N = input.size();

    if (N <= 1)
    {
        qWarning() << "performFFT: invalid N =" << N;
        return;
    }

    // --- Ensure power-of-two size ---
    if ((N & (N - 1)) != 0)
    {
        int nextPow2 = pow(2, ceil(log2(N)));
        qWarning() << "performFFT: non power-of-two size" << N << "-> padded to" << nextPow2;

        QVector<double> padded = input;
        padded.resize(nextPow2);
        for (int i = N; i < nextPow2; ++i)
            padded[i] = 0;

        // recurse safely
        performFFT(padded, magnitude, freqAxis, sampleRate);
        return;
    }

    // --- Prepare input ---
    std::vector<kiss_fft_cpx> timeData(N), freqData(N);
    for (int i = 0; i < N; ++i)
    {
        timeData[i].r = input[i];
        timeData[i].i = 0.0;
    }

    // --- Allocate FFT plan ---
    kiss_fft_cfg cfg = kiss_fft_alloc(N, 0, nullptr, nullptr);
    if (!cfg)
    {
        qCritical() << "performFFT: kiss_fft_alloc failed for N =" << N;
        return;
    }

    qDebug() << "Debug 9: performing FFT of size" << N;

    // --- Execute safely ---
    kiss_fft(cfg, timeData.data(), freqData.data());

    qDebug() << "Debug 10: FFT complete";

#ifdef kiss_fft_free
    kiss_fft_free(cfg);
#else
    free(cfg);
#endif

    // --- Prepare output ---
    int half = N / 2;
    magnitude.resize(half + 1);
    freqAxis.resize(half + 1);

    const double windowGain = 0.5;

    for (int k = 0; k <= half; ++k)
    {
        double re = freqData[k].r;
        double im = freqData[k].i;
        double mag = sqrt(re * re + im * im);

        magnitude[k] = ((k == 0 || k == half) ? (mag / N) : ((2.0 * mag) / N)) / windowGain;
        freqAxis[k] = (sampleRate * k) / N;
    }
}





void MainWindow::computeAndPlotFFT(const QVector<double>& signal,
                                   double Fs,
                                   QCustomPlot *plot)
{
    if (signal.isEmpty() || plot == nullptr)
        return;

    QVector<double> processed = signal;
  //  removeDC(processed);

    qDebug()<<"Debug 3";
    applyHanning(processed);   // 2. apply window

    qDebug()<<"Debug 4";
    QVector<double> magnitude, freqAxis;

    qDebug()<<"Debug 5";
    performFFT(processed, magnitude, freqAxis, Fs);  // 3. FFT

    // ---- Plot (correct way) ----
    if (plot->graphCount() > 0)
    {
        plot->graph(0)->setData(freqAxis, magnitude);

        plot->xAxis->setRange(0, Fs/2);   // do NOT auto-rescale X
        plot->yAxis->rescale();           // only Y auto-scale

        plot->replot();
    }
}


//QVector<double> MainWindow::generateSineWave(double freq1,
//                                             double amp1,
//                                             double freq2,
//                                             double amp2,
//                                             double durationSeconds,
//                                             double sampleRate)
//{
//    int N = durationSeconds * sampleRate;   // total number of samples
//    QVector<double> signal(N);

//    for (int n = 0; n < N; n++)
//    {
//        double t = n / sampleRate;  // time index

//        // Combine two sine waves
//        double s1 = amp1 * sin(2.0 * M_PI * freq1 * t);
//        double s2 = amp2 * sin(2.0 * M_PI * freq2 * t);

//        signal[n] = s1 + s2;   // final signal
//    }

//    return signal;
//}


void MainWindow::on_spinBox_samplingfrequency_valueChanged(int arg1)
{
    quint16 value1 = ui->spinBox_samplingfrequency->value();

    double timeUnit = (value1 == 0) ? 0.0 : (1.0/ value1);


    QString label = QString("Time (1 = %1 ms)").arg(timeUnit);

    if (value1 == 0)
        label = "Time (1 = N/A)";
    else
        label = QString("Time (1 = %1 ms)").arg(timeUnit, 0, 'f', 1);
    ui->customPlot_adxl_x->xAxis->setLabel(QString("ADXL X %1").arg(label));
    ui->customPlot_adxl_x->replot(QCustomPlot::rpQueuedReplot);
    ui->customPlot_adxl_y->xAxis->setLabel(QString("ADXL Y %1").arg(label));
    ui->customPlot_adxl_y->replot(QCustomPlot::rpQueuedReplot);
    ui->customPlot_adxl_z->xAxis->setLabel(QString("ADXL Z %1").arg(label));
    ui->customPlot_adxl_z->replot(QCustomPlot::rpQueuedReplot);

    double timeUnit_temp = (value1 == 0) ? 0.0 : (682.0/ value1);
    QString label_temp;

    if (value1 == 0)
        label_temp = "(1 = N/A)";
    else
        label_temp = QString("(1 = %1 ms)").arg(timeUnit_temp, 0, 'f', 1);

    ui->customPlot_temperature->xAxis->setLabel(QString("Samples %1").arg(label_temp));
    ui->customPlot_temperature->replot(QCustomPlot::rpQueuedReplot);
}

void MainWindow::on_spinBox_Inclinometer_valueChanged(int arg1)
{
    int inclinometer = ui->spinBox_Inclinometer->value();
       double timeUnit_inclinometer = (inclinometer == 0) ? 0.0 : (1.0 / inclinometer);
       QString label2 = (inclinometer == 0) ? "(1 = N/A)"
                                     : QString("(1 = %1 ms)").arg( timeUnit_inclinometer);
    ui->customPlot_inclinometer_x->xAxis->setLabel(QString("Inclinometer Time X %1").arg(label2));
    ui->customPlot_inclinometer_x->replot(QCustomPlot::rpQueuedReplot);
    ui->customPlot_inclinometer_y->xAxis->setLabel(QString("Inclinometer Time Y %1").arg(label2));
    ui->customPlot_inclinometer_y->replot(QCustomPlot::rpQueuedReplot);
}
// create a pure sine: Fs=10000, f=1000, N = whatever you use
//void MainWindow::testPureSineFFT(int N, double Fs, double f, QCustomPlot *plot)
//{
//    QVector<double> sig(N);
//    for (int n = 0; n < N; ++n)
//        sig[n] = sin(2*M_PI*f*n / Fs);

//    // Ensure no DC
//    // removeDC(sig); // not needed for pure sine

//    // Option A: windowed
//    QVector<double> win = sig;
//    applyHanning(win);

//    // Option B: unwindowed (for comparison)
//    QVector<double> magW, freqW, magU, freqU;
//    performFFT(win, magW, freqW, Fs);
//    performFFT(sig, magU, freqU, Fs);

//    // Plot windowed result
//    if (plot->graphCount() == 0) plot->addGraph();
//    plot->graph(0)->setData(freqW, magW);
//    plot->xAxis->setRange(0, Fs/2);
//    plot->yAxis->rescale();
//    plot->replot();

//    qDebug() << "PureSine test: N=" << N << " Fs=" << Fs << " f=" << f;
//}







