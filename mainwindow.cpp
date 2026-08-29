#include "mainwindow.h"
#include "ui_mainwindow.h"

QFile MainWindow::logFile;
QTextStream MainWindow::logStream;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    serialObj = new serialPortHandler(this);

    ui->dateTimeEdit->setDateTime(QDateTime(QDate(2026, 7, 1),
                                            QTime(0, 0, 0)));


    ui->spinBox_logTime->setRange(INT_MIN, INT_MAX);
    ui->spinBox_samplingfrequency->setRange(INT_MIN, INT_MAX);

    ui->spinBox_logTime->setToolTip("Enter value from 1 to 65535");
    ui->spinBox_samplingfrequency->setToolTip("Enter value from 1 to 10000");


    connect(ui->pushButton_clear,&QPushButton::clicked,ui->textEdit_rawBytes,&QTextEdit::clear);

    ui->comboBox_ports->addItems(serialObj->availablePorts());

    connect(ui->pushButton_portsRefresh,&QPushButton::clicked,this,&MainWindow::refreshPorts);

    connect(ui->comboBox_ports,SIGNAL(activated(const QString &)),this,SLOT(onPortSelected(const QString &)));

    connect(this,&MainWindow::sendMsgId,serialObj,&serialPortHandler::recvMsgId);

    //Battery Timer
    batteryTimer = new QTimer(this);

    connect(batteryTimer,&QTimer::timeout,
            this,&MainWindow::batteryCommand);

    //Start timer when port is connected

    QString detectedPort =
                    serialObj->detectDevicePort();

            if (!detectedPort.isEmpty())
            {

                ui->comboBox_ports
                        ->setCurrentText(detectedPort);

                serialObj->setPORTNAME(detectedPort);


                batteryTimer->start(3000);
                qDebug() << "Auto connected to"
                         << detectedPort;
            }

    //writeToNotes from serial class
    connect(serialObj,&serialPortHandler::executeWriteToNotes,this,&MainWindow::writeToNotes);

    //debugging signals
    connect(serialObj,&serialPortHandler::portOpening,this,&MainWindow::portStatus);

    //gui display signal
    connect(serialObj,&serialPortHandler::guiDisplay,this,&MainWindow::showGuiData);

    applyScrollArea();

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

    setWindowTitle("DSVDL");

    showMaximized();

    ui->tabWidget->setCurrentWidget(ui->tab_settings);

    // Hiding Temperature Plot Start ---------------------------------------
    ui->customPlot_new_Temp->setMinimumHeight(100);
    ui->customPlot_new_Temp_live->setMinimumHeight(100);


    ui->customPlot_new_Temp_live->hide();
    ui->customPlot_new_Temp->hide();

    qDebug() << ui->customPlot_new_Temp->size();
    qDebug() << ui->customPlot_new_Temp_live->size();
    // Hiding Temperature Plot Stop -----------------------------------------

    initializeAllPlots();

    // ============================================================
    // FFT PLOTS
    // ============================================================
    QColor tempColor(255, 255, 100);

    if(ui->customPlot_adxl_x_FFT->graphCount() == 0)
        ui->customPlot_adxl_x_FFT->addGraph();
    ui->customPlot_adxl_x_FFT->graph(0)->setPen(QPen(tempColor,1));

    if(ui->customPlot_adxl_y_FFT->graphCount() == 0)
        ui->customPlot_adxl_y_FFT->addGraph();
    ui->customPlot_adxl_y_FFT->graph(0)->setPen(QPen(tempColor,1));

    if(ui->customPlot_adxl_z_FFT->graphCount() == 0)
        ui->customPlot_adxl_z_FFT->addGraph();
    ui->customPlot_adxl_z_FFT->graph(0)->setPen(QPen(tempColor,1));

    if(ui->customPlot_adxl_x_FFT_2->graphCount() == 0)
        ui->customPlot_adxl_x_FFT_2->addGraph();
    ui->customPlot_adxl_x_FFT_2->graph(0)->setPen(QPen(tempColor,1));

    if(ui->customPlot_adxl_y_FFT_2->graphCount() == 0)
        ui->customPlot_adxl_y_FFT_2->addGraph();
    ui->customPlot_adxl_y_FFT_2->graph(0)->setPen(QPen(tempColor,1));

    if(ui->customPlot_adxl_z_FFT_2->graphCount() == 0)
        ui->customPlot_adxl_z_FFT_2->addGraph();
    ui->customPlot_adxl_z_FFT_2->graph(0)->setPen(QPen(tempColor,1));

    setupFFTPlot(ui->customPlot_adxl_x_FFT, "Ax_100 Frequency (Hz)");
    setupFFTPlot(ui->customPlot_adxl_y_FFT, "Ay_100 Frequency (Hz)");
    setupFFTPlot(ui->customPlot_adxl_z_FFT, "Az_100 Frequency (Hz)");

    setupFFTPlot(ui->customPlot_adxl_x_FFT_2, "Ax_500 Frequency (Hz)");
    setupFFTPlot(ui->customPlot_adxl_y_FFT_2, "Ay_500 Frequency (Hz)");
    setupFFTPlot(ui->customPlot_adxl_z_FFT_2, "Az_500 Frequency (Hz)");

    // Setting Table Get Log Events
    ui->tableWidget_getLogEvents->setColumnCount(3);
    ui->tableWidget_getLogEvents->setHorizontalHeaderLabels({"Event ID", "Start Time and Date", "End Time and Date"});

    ui->tableWidget_getLogEvents->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tableWidget_getLogEvents->setAlternatingRowColors(true);

    auto header = ui->tableWidget_getLogEvents->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);

    // If any port doesn't send CHECK response
    QTimer::singleShot(2000,this,[=](){
        if(detectedPort.isEmpty())
        {
            QMessageBox::warning(this,"Not Connected","Port is not connected");
        }
    });

    //Hiding widgets
    ui->pushButton_remainingLogs->hide();
    ui->doubleSpinBox_availableStorage->hide();

    //Load bias values from config.txt
    loadAdxlBiasValues();
}

MainWindow::~MainWindow()
{
    writeToNotes("    ******    "+QCoreApplication::applicationName() +
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
    qDebug() << "Refreshing ports...";

    QStringList ports =
            serialObj->availablePorts();


    QString detectedPort =
            serialObj->detectDevicePort();

    ui->comboBox_ports->clear();
    ui->comboBox_ports->addItems(ports);

    if (!detectedPort.isEmpty())
    {
        ui->comboBox_ports ->setCurrentText(detectedPort);

        serialObj->setPORTNAME(detectedPort);

        qDebug() << "Auto connected to"
                 << detectedPort;
    }
}


void MainWindow::onPortSelected(const QString &portName)
{
    serialObj->setPORTNAME(portName);
}

void MainWindow::handleTimeout()
{
    QTimer::singleShot(0, this, [this](){
        QMessageBox::warning(this, "Timeout", "Hardware Not Responding!");
    });

    if(dlgPlot){
        dlgPlot->close();
        dlgPlot=nullptr;
    }
}

void MainWindow::onDataReceived()
{
    // Stop the timer since data has been received
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
    dlg->setModal(false);

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

void MainWindow::applyScrollArea()
{
    //In scroll area
    // Only retrieve and embed the central widget in a scroll area
    QWidget *existingCentralWidget = takeCentralWidget(); // Take the existing central widget
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(existingCentralWidget);         // Embed it in the scroll area
    scrollArea->setWidgetResizable(true);                 // Allow resizing within the scroll area

    // Set the scroll area as the new central widget
    setCentralWidget(scrollArea);
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

MainWindow::CsvPlotData
MainWindow::loadAdxlCsv(
        const QString &filePath)
{
    CsvPlotData result;

    QFile file(filePath);

    if(!file.open(
                QIODevice::ReadOnly |
                QIODevice::Text))
    {
        qCritical()
                << "Failed to open CSV:"
                << filePath;

        return result;
    }

    QTextStream in(&file);

    // Metadata row
    QString metadataLine;

    if(!in.atEnd())
    {
        metadataLine = in.readLine().trimmed();
    }

    // Extracting accFrequency
    QStringList meta = metadataLine.split(",");

    for(int i = 0; i < meta.size() - 1; ++i)
    {
        if(meta[i].trimmed() == "Acceleration Frequency (Hz)")
        {
            bool ok = false;

            int freq = meta[i + 1].toInt(&ok);

            if(ok)
            {
                result.accFrequency = freq;
            }

            break;
        }
    }


    // Header row
    if(!in.atEnd())
    {
        in.readLine();
    }

    while(!in.atEnd())
    {
        QString line =
                in.readLine().trimmed();

        if(line.isEmpty())
        {
            continue;
        }

        QStringList values =
                line.split(",");

        if(values.size() < 8)
        {
            continue;
        }

        bool okSample = false;
        bool okX1 = false;
        bool okY1 = false;
        bool okZ1 = false;
        bool okX2 = false;
        bool okY2 = false;
        bool okZ2 = false;

        double sample =
                values[0].toDouble(&okSample);

        double x1 =
                values[1].toDouble(&okX1);

        double y1 =
                values[2].toDouble(&okY1);

        double z1 =
                values[3].toDouble(&okZ1);

        double x2 =
                values[4].toDouble(&okX2);

        double y2 =
                values[5].toDouble(&okY2);

        double z2 =
                values[6].toDouble(&okZ2);

        if(!(okSample &&
             okX1 &&
             okY1 &&
             okZ1 &&
             okX2 &&
             okY2 &&
             okZ2))
        {
            continue;
        }


        result.sampleIndex.append(sample);

        result.x1Loaded.append(x1);
        result.y1Loaded.append(y1);
        result.z1Loaded.append(z1);

        result.x2Loaded.append(x2);
        result.y2Loaded.append(y2);
        result.z2Loaded.append(z2);


        //------------------------------------------------
        // Pressure
        //------------------------------------------------

        if(values.size() > 7 &&
                !values[7].trimmed().isEmpty())
        {
            bool okPressure = false;

            double pressure =
                    values[7].toDouble(&okPressure);

            if(okPressure)
            {
                result.pressureLoaded.append(
                            pressure);
            }
        }
    }

    file.close();

    qDebug()
            << "CSV Load Complete";

    qDebug()
            << "Samples:"
            << result.sampleIndex.size();

    qDebug()
            << "X1:"
            << result.x1Loaded.size();

    qDebug()
            << "X2:"
            << result.x2Loaded.size();

    qDebug()
            << "Temp:"
            << result.tempLoaded.size();

    qDebug()
            << "Pressure:"
            << result.pressureLoaded.size();

    return result;
}

void MainWindow::setupFFTPlot(QCustomPlot *plot, const QString &xLabel)
{
    if (!plot) return;

    // ---------- NEON THEME ----------
    plot->setBackground(QColor(10, 20, 10));
    plot->axisRect()->setBackground(QColor(15, 35, 15));

    QColor neonGreen(0, 255, 150);
    QColor softGreen(150, 255, 180);
    QColor tempColor(255, 255, 100);

    plot->xAxis->setLabel(xLabel);
    plot->yAxis->setLabel("Magnitude(g)");
    plot->legend->setVisible(false);

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

    // ---------- ADD PEN  ----------
    plot->graph(0)->setPen(QPen(tempColor, 1));

    // ---------- INTERACTIONS ----------
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // ----------CLICK-POINT SELECTION----------
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
    // ============================================================
    // COLORS
    // ============================================================

    QColor adxlColors[] =
    {
        QColor(255, 60, 60),      // X
        QColor(255, 180, 0),      // Y
        QColor(120, 180, 255)     // Z
    };

    QColor tempColor(255, 255, 100);
    QColor pressureColor(0, 255, 220);

    QString adxlFreqLabel;

    if(accFrequency > 0)
    {
        double samplePeriodUS = 1000000.0 / accFrequency;

        adxlFreqLabel =
                QString("Sample Number (1 Sample = %1 µs)")
                .arg(samplePeriodUS, 0, 'f', 2);
    }
    else
    {
        adxlFreqLabel = "Sample Number";
    }

    // ============================================================
    // ADXL PLOTS
    // ============================================================

    setupPlot(ui->customPlot_adxl_x,
              QString("%1").arg(adxlFreqLabel),
              "Ax_100 (g)");

    setupPlot(ui->customPlot_adxl_y,
              QString("%1").arg(adxlFreqLabel),
              "Ay_100  (g)");

    setupPlot(ui->customPlot_adxl_z,
              QString("%1").arg(adxlFreqLabel),
              "Az_100  (g)");

    if(ui->customPlot_adxl_x->graphCount() == 0)
        ui->customPlot_adxl_x->addGraph();
    ui->customPlot_adxl_x->graph(0)->setPen(QPen(adxlColors[0],1));


    if(ui->customPlot_adxl_y->graphCount() == 0)
        ui->customPlot_adxl_y->addGraph();
    ui->customPlot_adxl_y->graph(0)->setPen(QPen(adxlColors[1],1));

    if(ui->customPlot_adxl_z->graphCount() == 0)
        ui->customPlot_adxl_z->addGraph();
    ui->customPlot_adxl_z->graph(0)->setPen(QPen(adxlColors[2],1));

    // ============================================================
    // SECOND SET OF ADXL PLOTS
    // ============================================================

    setupPlot(ui->customPlot_adxl_x2,
              QString("%1").arg(adxlFreqLabel),
              "Ax_500  (g)");

    setupPlot(ui->customPlot_adxl_y2,
              QString("%1").arg(adxlFreqLabel),
              "Ay_500  (g)");

    setupPlot(ui->customPlot_adxl_z2,
              QString("%1").arg(adxlFreqLabel),
              "Az_500  (g)");

    if(ui->customPlot_adxl_x2->graphCount() == 0)
        ui->customPlot_adxl_x2->addGraph();
    ui->customPlot_adxl_x2->graph(0)->setPen(QPen(adxlColors[0],1));

    if(ui->customPlot_adxl_y2->graphCount() == 0)
        ui->customPlot_adxl_y2->addGraph();
    ui->customPlot_adxl_y2->graph(0)->setPen(QPen(adxlColors[1],1));

    if(ui->customPlot_adxl_z2->graphCount() == 0)
        ui->customPlot_adxl_z2->addGraph();
    ui->customPlot_adxl_z2->graph(0)->setPen(QPen(adxlColors[2],1));

    // ============================================================
    // TEMPERATURE
    // ============================================================

    setupPlot(ui->customPlot_new_Temp,
              "Samples",
              "Temperature (°C)");

    if(ui->customPlot_new_Temp->graphCount() == 0)
        ui->customPlot_new_Temp->addGraph();
    ui->customPlot_new_Temp->graph(0)->setPen(QPen(tempColor,1));

    ui->customPlot_new_Temp->graph(0)->setPen(QPen(tempColor, 1));

    // ============================================================
    // PRESSURE
    // ============================================================

    setupPlot(ui->customPlot_new_Pressure,
              "Samples",
              "Pressure (mbar)");

    if(ui->customPlot_new_Pressure->graphCount() == 0)
        ui->customPlot_new_Pressure->addGraph();

    ui->customPlot_new_Pressure->graph(0)->setPen(QPen(pressureColor, 1));

    // ============================================================
    // INITIAL AXIS RANGES
    // ============================================================

    QList<QCustomPlot*> normalPlots =
    {
        ui->customPlot_adxl_x,
        ui->customPlot_adxl_y,
        ui->customPlot_adxl_z,

        ui->customPlot_adxl_x2,
        ui->customPlot_adxl_y2,
        ui->customPlot_adxl_z2,

        ui->customPlot_new_Temp,
        ui->customPlot_new_Pressure
    };

    for(auto plot : normalPlots)
    {
        plot->xAxis->setRange(0, 100);
        plot->yAxis->setRange(-5, 5);
        plot->replot();
    }

    livePlots =
    {
        ui->customPlot_adxl_x_live,
        ui->customPlot_adxl_y_live,
        ui->customPlot_adxl_z_live,

        ui->customPlot_adxl_x2_live,
        ui->customPlot_adxl_y2_live,
        ui->customPlot_adxl_z2_live,

        ui->customPlot_new_Temp_live,
        ui->customPlot_new_Pressure_live
    };

    adxl100RangeMin =
            ui->lineEdit_ADXL_100g->text().toFloat();

    adxl500RangeMin =
            ui->lineEdit_ADXL_500g->text().toFloat();

    pressureRangeMin =
            ui->lineEdit_pressureRange->text().toFloat();

    adxl100RangeMax =
            ui->lineEdit_ADXL_100g_2->text().toFloat();

    adxl500RangeMax =
            ui->lineEdit_ADXL_500g_2->text().toFloat();

    pressureRangeMax =
            ui->lineEdit_pressureRange_2->text().toFloat();

    // ADXL 100g
    ui->customPlot_adxl_x_live->yAxis->setRange(
                adxl100RangeMin,
                adxl100RangeMax);

    ui->customPlot_adxl_y_live->yAxis->setRange(
                adxl100RangeMin,
                adxl100RangeMax);

    ui->customPlot_adxl_z_live->yAxis->setRange(
                adxl100RangeMin,
                adxl100RangeMax);

    // ADXL 500g
    ui->customPlot_adxl_x2_live->yAxis->setRange(
                adxl500RangeMin,
                adxl500RangeMax);

    ui->customPlot_adxl_y2_live->yAxis->setRange(
                adxl500RangeMin,
                adxl500RangeMax);

    ui->customPlot_adxl_z2_live->yAxis->setRange(
                adxl500RangeMin,
                adxl500RangeMax);

    // Temperature & Pressure (keep fixed for now)
    ui->customPlot_new_Temp_live->yAxis->setRange(0,100);
    ui->customPlot_new_Pressure_live->yAxis->setRange(
                pressureRangeMin,pressureRangeMax);

    // X-axis and replot
    for(auto plot : livePlots)
    {
        plot->xAxis->setRange(0, LIVE_WINDOW);
        plot->replot();
    }


    // ============================================================
    // LIVE ADXL PLOTS
    // ============================================================

    setupPlot(ui->customPlot_adxl_x_live,
              "Samples",
              "Ax_100 (g)");

    setupPlot(ui->customPlot_adxl_y_live,
              "Samples",
              "Ay_100 (g)");

    setupPlot(ui->customPlot_adxl_z_live,
              "Samples",
              "Az_100 (g)");

    if(ui->customPlot_adxl_x_live->graphCount() == 0)
        ui->customPlot_adxl_x_live->addGraph();
    ui->customPlot_adxl_x_live->graph(0)->setPen(QPen(adxlColors[0],1));

    if(ui->customPlot_adxl_y_live->graphCount() == 0)
        ui->customPlot_adxl_y_live->addGraph();
    ui->customPlot_adxl_y_live->graph(0)->setPen(QPen(adxlColors[1],1));

    if(ui->customPlot_adxl_z_live->graphCount() == 0)
        ui->customPlot_adxl_z_live->addGraph();
    ui->customPlot_adxl_z_live->graph(0)->setPen(QPen(adxlColors[2],1));

    // ============================================================
    // LIVE SECOND ADXL PLOTS
    // ============================================================

    setupPlot(ui->customPlot_adxl_x2_live,
              "Samples",
              "Ax_500 (g)");

    setupPlot(ui->customPlot_adxl_y2_live,
              "Samples",
              "Ay_500 (g)");

    setupPlot(ui->customPlot_adxl_z2_live,
              "Samples",
              "Az_500 (g)");

    if(ui->customPlot_adxl_x2_live->graphCount() == 0)
        ui->customPlot_adxl_x2_live->addGraph();
    ui->customPlot_adxl_x2_live->graph(0)->setPen(QPen(adxlColors[0],1));

    if(ui->customPlot_adxl_y2_live->graphCount() == 0)
        ui->customPlot_adxl_y2_live->addGraph();
    ui->customPlot_adxl_y2_live->graph(0)->setPen(QPen(adxlColors[1],1));

    if(ui->customPlot_adxl_z2_live->graphCount() == 0)
        ui->customPlot_adxl_z2_live->addGraph();
    ui->customPlot_adxl_z2_live->graph(0)->setPen(QPen(adxlColors[2],1));

    // ============================================================
    // LIVE TEMPERATURE
    // ============================================================

    setupPlot(ui->customPlot_new_Temp_live,
              "Samples",
              "Temperature (°C)");

    if(ui->customPlot_new_Temp_live->graphCount() == 0)
        ui->customPlot_new_Temp_live->addGraph();
    ui->customPlot_new_Temp_live->graph(0)->setPen(QPen(tempColor,1));

    ui->customPlot_new_Temp_live->graph(0)->setPen(QPen(tempColor,1));

    // ============================================================
    // LIVE PRESSURE
    // ============================================================

    setupPlot(ui->customPlot_new_Pressure_live,
              "Samples",
              "Pressure (mbar)");

    if(ui->customPlot_new_Pressure_live->graphCount() == 0)
        ui->customPlot_new_Pressure_live->addGraph();
    ui->customPlot_new_Pressure_live->graph(0)->setPen(QPen(pressureColor,1));

    ui->customPlot_new_Pressure_live->graph(0)->setPen(QPen(pressureColor,1));


    //-------------------------------------------------------
    // Live Circular Buffer Initialization
    //-------------------------------------------------------

    writeIndex = 0;

    // X-axis
    plotX.resize(LIVE_WINDOW);

    for(quint64 i = 0; i < LIVE_WINDOW; ++i)
    {
        plotX[i] = i;
    }

    // ADXL Buffers
    plotAx100.fill(0.0, LIVE_WINDOW);
    plotAy100.fill(0.0, LIVE_WINDOW);
    plotAz100.fill(0.0, LIVE_WINDOW);

    plotAx500.fill(0.0, LIVE_WINDOW);
    plotAy500.fill(0.0, LIVE_WINDOW);
    plotAz500.fill(0.0, LIVE_WINDOW);

    displayAx100.fill(0.0, LIVE_WINDOW);
    displayAy100.fill(0.0, LIVE_WINDOW);
    displayAz100.fill(0.0, LIVE_WINDOW);

    displayAx500.fill(0.0, LIVE_WINDOW);
    displayAy500.fill(0.0, LIVE_WINDOW);
    displayAz500.fill(0.0, LIVE_WINDOW);

    pressureWriteIndex = 0;
    plotPressure.fill(0.0, LIVE_WINDOW);
    displayPressure.fill(0.0, LIVE_WINDOW);

    peakAx100 = peakAy100 = peakAz100 = std::numeric_limits<double>::lowest();
    peakAx500 = peakAy500 = peakAz500 = std::numeric_limits<double>::lowest();
    peakPressure = std::numeric_limits<double>::lowest();

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
        this->eventId=eventId;

        ui->lineEdit_eventId->setText(QString::number(eventId));

        ui->lineEdit_eventId->setStyleSheet("background-color:yellow");

        QTimer::singleShot(500,[this](){
            ui->lineEdit_eventId->setStyleSheet("");
        });

        // Unit no and sampling freq extraction
        quint8 unitNo = static_cast<quint8>(Item1[6]);
        this->unitNo = unitNo;

        quint16 accFrequency =
            (static_cast<quint8>(Item1[4]) << 8) |
             static_cast<quint8>(Item1[5]);

        this->accFrequency = accFrequency;

        QString adxlFreqLabel;

        if(accFrequency > 0)
        {
            double samplePeriodUS = 1000000.0 / accFrequency;

            adxlFreqLabel =
                    QString("Sample Number (1 Sample = %1 µs)")
                    .arg(samplePeriodUS, 0, 'f', 2);
        }
        else
        {
            adxlFreqLabel = "Sample Number";
        }

        // Updating x axis lables start -----------------------------
        setupPlot(ui->customPlot_adxl_x,
                  QString("%1").arg(adxlFreqLabel),
                  "Ax_100 (g)",1);

        setupPlot(ui->customPlot_adxl_y,
                  QString("%1").arg(adxlFreqLabel),
                  "Ay_100  (g)",1);

        setupPlot(ui->customPlot_adxl_z,
                  QString("%1").arg(adxlFreqLabel),
                  "Az_100  (g)",1);



        setupPlot(ui->customPlot_adxl_x2,
                  QString("%1").arg(adxlFreqLabel),
                  "Ax_500  (g)",1);

        setupPlot(ui->customPlot_adxl_y2,
                  QString("%1").arg(adxlFreqLabel),
                  "Ay_500  (g)",1);

        setupPlot(ui->customPlot_adxl_z2,
                  QString("%1").arg(adxlFreqLabel),
                  "Az_500  (g)",1);

        // Updating x axis lables end -----------------------------

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
            this->formattedStart=formattedStart;
            this->formattedEnd=formattedEnd;


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
        QTimer::singleShot(0, this, [this](){
            QMessageBox::warning(this,"Error","packet32List size is more than 1");
        });
    }
}

void MainWindow::makePacket2048AdxlTempListPressureList(
        QList<QByteArray> &rawPacket2048AdxlList,
        QList<QByteArray> &rawPacketTemperatureList,
        QList<QByteArray> &rawPacketPressureList)
{
    QVector<double> sampleIndex;
    QVector<double> hardwarePacketNumList;

    QVector<double> x1Adxl;
    QVector<double> y1Adxl;
    QVector<double> z1Adxl;

    QVector<double> x2Adxl;
    QVector<double> y2Adxl;
    QVector<double> z2Adxl;

    QVector<double> tempIndex;
    QVector<double> temperatureValues;

    QVector<double> pressureIndex;
    QVector<double> pressureValues;

    //----------------------------------------------------
    // ADXL DATA
    //----------------------------------------------------

    for(int p = 0; p < rawPacket2048AdxlList.size(); p++)
    {
        QByteArray packet = rawPacket2048AdxlList[p];

        constexpr int HEADER_SIZE = 3;
        constexpr int PACKET_NUMBER_SIZE = 4;
        constexpr int ADXL_SIZE = 2040;
        constexpr int TEMP_SIZE = 4;
        constexpr int PRESSURE_SIZE = 4;
        constexpr int FOOTER_SIZE = 3;

        constexpr int PACKET_SIZE =
                HEADER_SIZE +
                PACKET_NUMBER_SIZE +
                ADXL_SIZE +
                TEMP_SIZE +
                PRESSURE_SIZE +
                FOOTER_SIZE;

        if(packet.size() != PACKET_SIZE)
        {
            QString msg =
                    QString("Invalid/Missed Packet | Size: %1 | Expected: %2\n"
                            "Packet Data:\n%3")
                    .arg(packet.size())
                    .arg(PACKET_SIZE)
                    .arg(QString(packet.toHex(' ').toUpper()));

            writeToNotes(msg);

            continue;
        }

        // Remove Header
        QByteArray payload = packet.mid(3);

        // Remove Footer
        payload.chop(3);

        // Hardware Packet Number
        QByteArray packetNumberBytes = payload.left(4);

        quint32 hardwarePacketNumber =
                (static_cast<quint8>(packetNumberBytes[0]) << 24) |
                (static_cast<quint8>(packetNumberBytes[1]) << 16) |
                (static_cast<quint8>(packetNumberBytes[2]) << 8)  |
                static_cast<quint8>(packetNumberBytes[3]);

        hardwarePacketNumList.append(hardwarePacketNumber);

        QString msg =
                QString("Hardware Packet Number: %1")
                .arg(hardwarePacketNumber);

        writeToNotes(msg);

        // ADXL Region = First 2040 Bytes
        QByteArray adxlBytes = payload.mid(4,2040);

        for(int i = 0; i + 11 < adxlBytes.size(); i += 12)
        {
            quint16 x1 =
                    (static_cast<quint8>(adxlBytes[i+1]) << 8) |
                    static_cast<quint8>(adxlBytes[i]);
            double x1f =  (x1 / 65535.0 ) * 5.12;
            x1f = ( x1f - x1Bias ) / 0.012563;

            quint16 y1 =
                    (static_cast<quint8>(adxlBytes[i+3]) << 8) |
                    static_cast<quint8>(adxlBytes[i+2]);
            double y1f =  (y1 / 65535.0 ) * 5.12;
            y1f = ( y1f - y1Bias ) / 0.011812;

            quint16 z1 =
                    (static_cast<quint8>(adxlBytes[i+5]) << 8) |
                    static_cast<quint8>(adxlBytes[i+4]);
            double z1f =  (z1 / 65535.0 ) * 5.12;
            z1f = ( z1f - z1Bias ) / 0.012346;


            quint16 x2 =
                    (static_cast<quint8>(adxlBytes[i+7]) << 8) |
                    static_cast<quint8>(adxlBytes[i+6]);
            double x2f =  (x2 / 65535.0 ) * 5.12;
            x2f = ( x2f - x2Bias ) / 0.002576;

            quint16 y2 =
                    (static_cast<quint8>(adxlBytes[i+9]) << 8) |
                    static_cast<quint8>(adxlBytes[i+8]);
            double y2f =  (y2 / 65535.0 ) * 5.12;
            y2f = ( y2f - y2Bias ) / 0.002556;

            quint16 z2 =
                    (static_cast<quint8>(adxlBytes[i+11]) << 8) |
                    static_cast<quint8>(adxlBytes[i+10]);
            double z2f =  (z2 / 65535.0 ) * 5.12;
            z2f = ( z2f - z2Bias ) / 0.002917;

            int localSampleNumber = (i / 12) + 1;

            double hardwareSampleIndex =
                    (hardwarePacketNumber * 170)
                    + localSampleNumber;

            sampleIndex.append(hardwareSampleIndex);

            x1Adxl.append(x1f);
            y1Adxl.append(y1f);
            z1Adxl.append(z1f);

            x2Adxl.append(x2f);
            y2Adxl.append(y2f);
            z2Adxl.append(z2f);
        }
    }

    qDebug() << "Total ADXL Samples:" << sampleIndex.size();

    //----------------------------------------------------
    // TEMPERATURE
    //----------------------------------------------------

    for(int i = 0; i < rawPacketTemperatureList.size(); i++)
    {
        QByteArray tempBytes = rawPacketTemperatureList[i];

        if(tempBytes.size() < 4)
            continue;

        double tempValue =
                bytesToFloatMSB(tempBytes);

        tempIndex.append(i + 1);
        temperatureValues.append(tempValue);
    }

    qDebug() << "Temperature Samples:"
             << temperatureValues.size();

    //----------------------------------------------------
    // PRESSURE
    //----------------------------------------------------

    for(int i = 0; i < rawPacketPressureList.size(); i++)
    {
        QByteArray pressureBytes =
                rawPacketPressureList[i];

        if(pressureBytes.size() < 4)
            continue;

        double pressureValue =
                bytesToFloatMSB(pressureBytes);

        pressureValue = pressureValue / 100.0;

        pressureIndex.append(i + 1);
        pressureValues.append(pressureValue);
    }

    qDebug() << "Pressure Samples:"
             << pressureValues.size();

    //----------------------------------------------------
    // Plot Helper
    //----------------------------------------------------

    auto plotGraph =
            [](QCustomPlot *plot,
            const QVector<double> &x,
            const QVector<double> &y)
    {
        if(plot->graphCount() == 0 ||
                x.isEmpty() ||
                y.isEmpty())
        {
            return;
        }

        plot->setUpdatesEnabled(false);

        plot->graph(0)->data()->clear();

        constexpr int CHUNK_SIZE = 5000;

        for(int i = 0; i < x.size(); i += CHUNK_SIZE)
        {
            int count =
                    qMin(CHUNK_SIZE,
                         x.size() - i);

            plot->graph(0)->addData(
                        x.mid(i,count),
                        y.mid(i,count));
        }

        plot->xAxis->setRange(
                    x.first(),
                    x.last());

        plot->graph(0)->rescaleValueAxis();

        plot->setUpdatesEnabled(true);

        plot->replot(
                    QCustomPlot::rpQueuedReplot);
    };

    //----------------------------------------------------
    // PLOTS
    //----------------------------------------------------

    plotGraph(
                ui->customPlot_adxl_x,
                sampleIndex,
                x1Adxl);

    plotGraph(
                ui->customPlot_adxl_y,
                sampleIndex,
                y1Adxl);

    plotGraph(
                ui->customPlot_adxl_z,
                sampleIndex,
                z1Adxl);

    plotGraph(
                ui->customPlot_adxl_x2,
                sampleIndex,
                x2Adxl);

    plotGraph(
                ui->customPlot_adxl_y2,
                sampleIndex,
                y2Adxl);

    plotGraph(
                ui->customPlot_adxl_z2,
                sampleIndex,
                z2Adxl);

    plotGraph(
                ui->customPlot_new_Temp,
                tempIndex,
                temperatureValues);

    plotGraph(
                ui->customPlot_new_Pressure,
                pressureIndex,
                pressureValues);

    //----------------------------------------------------
    // Debug
    //----------------------------------------------------

    qDebug() << "X1:" << x1Adxl.size();
    qDebug() << "Y1:" << y1Adxl.size();
    qDebug() << "Z1:" << z1Adxl.size();

    qDebug() << "X2:" << x2Adxl.size();
    qDebug() << "Y2:" << y2Adxl.size();
    qDebug() << "Z2:" << z2Adxl.size();

    qDebug() << "Temp:" << temperatureValues.size();
    qDebug() << "Pressure:" << pressureValues.size();

    this->finalSampleIndexNew = sampleIndex;

    //Peak Detection
    double peakAx100 = *std::max_element(x1Adxl.begin(), x1Adxl.end());
    double peakAy100 = *std::max_element(y1Adxl.begin(), y1Adxl.end());
    double peakAz100 = *std::max_element(z1Adxl.begin(), z1Adxl.end());

    double peakAx500 = *std::max_element(x2Adxl.begin(), x2Adxl.end());
    double peakAy500 = *std::max_element(y2Adxl.begin(), y2Adxl.end());
    double peakAz500 = *std::max_element(z2Adxl.begin(), z2Adxl.end());

    double peakPressure = *std::max_element(
            pressureValues.begin(),
            pressureValues.end());

    ui->lineEdit_Ax_100_peak_2->setText(QString::number(peakAx100, 'f', 3));
    ui->lineEdit_Ay_100_peak_2->setText(QString::number(peakAy100, 'f', 3));
    ui->lineEdit_Az_100_peak_2->setText(QString::number(peakAz100, 'f', 3));

    ui->lineEdit_Ax_500_peak_2->setText(QString::number(peakAx500, 'f', 3));
    ui->lineEdit_Ay_500_peak_2->setText(QString::number(peakAy500, 'f', 3));
    ui->lineEdit_Az_500_peak_2->setText(QString::number(peakAz500, 'f', 3));

    ui->lineEdit_pressure_peak_2->setText(
            QString::number(peakPressure, 'f', 3));

    this->finalX1AdxlNew = x1Adxl;
    this->finalY1AdxlNew = y1Adxl;
    this->finalZ1AdxlNew = z1Adxl;

    this->finalX2AdxlNew = x2Adxl;
    this->finalY2AdxlNew = y2Adxl;
    this->finalZ2AdxlNew = z2Adxl;

    this->finalTempIndexNew = tempIndex;
    this->finalTemperatureNew = temperatureValues;

    this->finalPressureIndexNew = pressureIndex;
    this->finalPressureNew = pressureValues;

    qDebug() << "Stored Samples :" << finalSampleIndexNew.size();

    qDebug() << "Stored X1 :" << finalX1AdxlNew.size();
    qDebug() << "Stored Y1 :" << finalY1AdxlNew.size();
    qDebug() << "Stored Z1 :" << finalZ1AdxlNew.size();

    qDebug() << "Stored X2 :" << finalX2AdxlNew.size();
    qDebug() << "Stored Y2 :" << finalY2AdxlNew.size();
    qDebug() << "Stored Z2 :" << finalZ2AdxlNew.size();

    qDebug() << "Stored Temp :" << finalTemperatureNew.size();
    qDebug() << "Stored Pressure :" << finalPressureNew.size();
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

        QTimer::singleShot(0, this, [this](){
            QMessageBox::critical(this,"Port Error","Please Select Port Using Above Dropdown");
        });

        batteryTimer->stop();
    }

    if(data.startsWith("Serial port ") && data.endsWith(" opened successfully at baud rate 921600"))
    {
        QTimer::singleShot(0, this, [this,data](){
            QMessageBox::information(this,"Success",data);
        });
        batteryTimer->start(3000);
    }

    if(data.startsWith("Failed to open port"))
    {
        if(dlgPlot)
        {
            dlgPlot->close();
            dlgPlot = nullptr;
        }

        batteryTimer->stop();
        QTimer::singleShot(0, this, [this,data](){
            QMessageBox::critical(this,"Error",data);
        });
    }

}

void MainWindow::showGuiData(const QByteArray &byteArrayData)
{
    QByteArray data = byteArrayData;

    // Get Event Data Command msgId 0x01
    if(data.startsWith(QByteArray::fromHex("AA BB")) &&
            data.endsWith(QByteArray::fromHex("AA BB CC DD FF")))
    {
        int i = 2; // Skip Ultimate Header (AA BB)

        QList<QByteArray> packet32List;
        QList<QByteArray> packet2048AdxlList;

        QList<QByteArray> packetTemperatureList;
        QList<QByteArray> packetPressureList;

        int invalidHeaderCount = 0;

        while(i < data.size() - 5) // ignore ultimate footer
        {
            // -------------------------------------------------
            // CASE 1 : 32 BYTE PACKET
            // Header = AA BB
            // Total Size = 32 bytes
            // -------------------------------------------------
            if(data.mid(i, 2) == QByteArray::fromHex("AA BB"))
            {
                if(i + 32 <= data.size())
                {
                    QByteArray packet32 = data.mid(i, 32);

                    packet32List.append(packet32);

                    qDebug() << "Packet32 Found, Size:"
                             << packet32.size();

                    i += 32;
                    continue;
                }
                else
                {
                    writeToNotes(
                                "Incomplete Packet32 found.");
                    break;
                }
            }

            // -------------------------------------------------
            // CASE 2 : ADXL PACKET
            // Header = CC DD FF
            // Footer = FF EE FF
            // -------------------------------------------------
            else if(data.mid(i, 3) == QByteArray::fromHex("CC DD FF"))
            {
                int footerPos =
                        data.indexOf(
                            QByteArray::fromHex("FF EE FF"),
                            i);

                if(footerPos < 0)
                {
                    writeToNotes(
                                "ADXL footer not found.");

                    invalidHeaderCount++;
                    break;
                }

                QByteArray packet2048 =
                        data.mid(
                            i,
                            footerPos - i + 3);

                // FF Filtering is removed now @29Aug2026 for DSVDL project
//                // ---------------------------------------------
//                // FF FF FF FF FF FF Special Condition
//                // ---------------------------------------------
//                if(packet2048.contains(
//                            QByteArray::fromHex(
//                                "FF FF FF FF FF FF")))
//                {
//                    QByteArray specialPacket =
//                            packet2048;

//                    int fIndex =
//                            specialPacket.indexOf(
//                                QByteArray::fromHex(
//                                    "FF FF FF FF FF FF"));

//                    qDebug()
//                            << "Consecutive FF's detected at packet [ADXL]:"
//                            << packet2048AdxlList.size();

//                    writeToNotes(
//                                "Consecutive FF's detected at packet [ADXL]: "
//                                + QString::number(
//                                    packet2048AdxlList.size()));

//                    qDebug()
//                            << "fIndex:"
//                            << fIndex;

//                    writeToNotes(
//                                "fIndex (start of FFs) [ADXL]: "
//                                + QString::number(fIndex));

//                    int bytesRemoved =
//                            (specialPacket.size() - fIndex) - 3;

//                    qDebug()
//                            << "Removing FF bytes count [ADXL]:"
//                            << bytesRemoved;

//                    writeToNotes(
//                                "Removing FF bytes count [ADXL]: "
//                                + QString::number(bytesRemoved));

//                    specialPacket.remove(
//                                fIndex,
//                                bytesRemoved);

//                    packet2048AdxlList.append(
//                                specialPacket);

//                    writeToNotes(
//                                "specialPacket [ADXL]: "
//                                + specialPacket
//                                .toHex(' ')
//                                .toUpper());
//                }
//                else
//                {
                    packet2048AdxlList.append(
                                packet2048);
//                }

                // ---------------------------------------------
                // Extract Temperature & Pressure
                //
                // Last layout:
                //
                // Temp(4)
                // Pressure(4)
                // FF EE FF
                // ---------------------------------------------
                if(packet2048.size() >= 14)
                {
                    int footerIndex =
                            packet2048.size() - 3;

                    QByteArray tempBytes =
                            packet2048.mid(
                                footerIndex - 8,
                                4);

                    QByteArray pressureBytes =
                            packet2048.mid(
                                footerIndex - 4,
                                4);

                    packetTemperatureList
                            .append(tempBytes);

                    packetPressureList
                            .append(pressureBytes);
                }
                else
                {
                    writeToNotes(
                                "ADXL packet too small for Temp/Pressure extraction.");
                }


                i = footerPos + 3;
                continue;
            }

            // -------------------------------------------------
            // CASE 3 : Unknown Header
            // -------------------------------------------------
            else
            {
                QByteArray unknownHeader =
                        data.mid(i, 3);

                qDebug()
                        << "Unknown Header:"
                        << unknownHeader.toHex();

                writeToNotes(
                            "Unknown Header: "
                            + unknownHeader
                            .toHex(' ')
                            .toUpper());

                invalidHeaderCount++;

                // move one byte forward and keep searching
                i++;
            }
        }

        // -------------------------------------------------
        // Summary Logs
        // -------------------------------------------------
        writeToNotes(
                    "Packet32 count: "
                    + QString::number(
                        packet32List.size()));


        writeToNotes(
                    "Packet2048 ADXL count: "
                    + QString::number(
                        packet2048AdxlList.size()));

        writeToNotes(
                    "Temperature samples: "
                    + QString::number(
                        packetTemperatureList.size()));

        writeToNotes(
                    "Pressure samples: "
                    + QString::number(
                        packetPressureList.size()));

        writeToNotes(
                    "Invalid headers: "
                    + QString::number(
                        invalidHeaderCount));

        // -------------------------------------------------
        // UI Updates
        // -------------------------------------------------
        makePacket32UI(packet32List);


        //Display Purpose Start ----------------------------------------
        writeToNotes("========== Packet Summary ==========");
        // First Packet32
        if(!packet32List.isEmpty())
        {
            writeToNotes("First Packet32 : "
                         + packet32List.first().toHex(' ').toUpper());
        }

        // First ADXL Packet
        if(!packet2048AdxlList.isEmpty())
        {
            writeToNotes("First ADXL Packet Size : "
                         + QString::number(packet2048AdxlList.first().size()));

            writeToNotes("First ADXL Packet : "
                         + packet2048AdxlList.first().toHex(' ').toUpper());
        }

        // First Temperature
        if(!packetTemperatureList.isEmpty())
        {
            writeToNotes("First Temperature Bytes : "
                         + packetTemperatureList.first().toHex(' ').toUpper());
        }

        // First Pressure
        if(!packetPressureList.isEmpty())
        {
            writeToNotes("First Pressure Bytes : "
                         + packetPressureList.first().toHex(' ').toUpper());
        }

        writeToNotes("====================================");
        //Display Purpose End ----------------------------------------

        makePacket2048AdxlTempListPressureList(
                    packet2048AdxlList,
                    packetTemperatureList,
                    packetPressureList);

        // Closing dialog after plotting
        if(dlgPlot)
        {
            dlgPlot->close();
            dlgPlot = nullptr;
        }


        // NEW CODE : CSV DUMP ONLY --------------------------------- BEGIN

        startAdxlCsvSaving(
                    [=]()
        {
            blinkLabel(ui->label_csv,
                       300,
                       "CSV OFF");

            if(dlgPlot)
            {
                dlgPlot->close();
                dlgPlot = nullptr;
            }

            QTimer::singleShot(0, this, [this](){
                QMessageBox::information(
                            this,
                            "Success",
                            "CSV data saved successfully on Desktop.");
            });
        });

        // NEW CODE : CSV DUMP ONLY --------------------------------- END

        batteryTimer->start();

    }
    // Get Event Data Command Nack Condition mdgId 0x01
    else if(data.startsWith(QByteArray::fromHex("53 54 45 FF")))
    {
        QTimer::singleShot(0, this, [this](){
            QMessageBox::warning(this,"Error","Invalid Event Id");
        });

        writeToNotes(" ### Invalid Event Id ###");
        if(dlgPlot)
        {
            dlgPlot->close();
            dlgPlot = nullptr;
        }
    }

    // Start Log Initial Command msgId 0x02
    else if(data==QByteArray::fromHex("54 53 41 43 4B"))
    {
        dlg = createPleaseWaitDialog("⏳ Please Wait Data Logging ...",ui->spinBox_logTime->value());
    }

    // Start Log End Initial Command msgId 0x02
    else if(data.startsWith(QByteArray::fromHex("54 53 50")))
    {
        if(dlg)
        {
            dlg->close();
            dlg = nullptr;
            QTimer::singleShot(0, this, [this](){
                QMessageBox::information(this,"Success","Successfully data logged !");
            });
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

    else if(data.startsWith("NO_EVENTS"))
    {
        //mdgId = 0x03
        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this,"No Events",
                                     "No Events Detected");
        });
    }

    else if(data.startsWith(QByteArray::fromHex("53 54 54"))&& data.size()==6)
    {
        quint8 third = static_cast<quint8>(data[4]);
        quint8 fourth  = static_cast<quint8>(data[3]);

        quint16 remainingLogs = (third << 8) | fourth;

        QTimer::singleShot(0, this, [remainingLogs](){
            QMessageBox::information(nullptr,
                                     "Remaining Logs",
                                     "Remaining log count: " + QString::number(remainingLogs));
        });

    }
    //msg Id = 0x06
    else if (data.startsWith("PARAM"))
    {
        QByteArray payload = data.mid(8, 14);

        quint8 sNo = static_cast<quint8>(payload[0]);

        quint16 logTime =
                (static_cast<quint8>(payload[1]) << 8) |
                 static_cast<quint8>(payload[2]);

        quint16 samplingFreq =
                (static_cast<quint8>(payload[3]) << 8) |
                 static_cast<quint8>(payload[4]);

        quint8 loginMode = static_cast<quint8>(payload[5]);

        quint32 requiredPages =
            (static_cast<quint8>(payload[9]) << 24) |
            (static_cast<quint8>(payload[8]) << 16) |
            (static_cast<quint8>(payload[7]) << 8)  |
            (static_cast<quint8>(payload[6]));

        quint32 availablePages =
            (static_cast<quint8>(payload[13]) << 24) |
            (static_cast<quint8>(payload[12]) << 16) |
            (static_cast<quint8>(payload[11]) << 8)  |
            (static_cast<quint8>(payload[10]));

        qDebug()<<requiredPages<<" :requiredPages";


        qDebug()<<availablePages<<" :availablePages";

        int totalPages = 261115;

        double availableStorage =
            (static_cast<double>(availablePages) / totalPages) * 100.0;

        ui->spinBox_unitNumber->setValue(sNo);
        ui->spinBox_logTime->setValue(logTime);
        ui->spinBox_samplingfrequency->setValue(samplingFreq);
        ui->doubleSpinBox_availableStorage->setValue(availableStorage);

        if(loginMode == static_cast<quint8>(0xAB))
            ui->radioButton_powerON->setChecked(true);

        if(loginMode == static_cast<quint8>(0xAF))
            ui->radioButton_GPIO->setChecked(true);

        blinkWidget(ui->spinBox_logTime);
        blinkWidget(ui->spinBox_samplingfrequency);
        blinkWidget(ui->spinBox_unitNumber);

        if (requiredPages >= availablePages)
        {
            ui->doubleSpinBox_availableStorage->setStyleSheet(
                        "QSpinBox { background-color: red; }");
        }
        else
        {
            blinkWidget(ui->doubleSpinBox_availableStorage);
        }

        //Progress Bar Indication
        int storagePercent = qRound(availableStorage);

        ui->progressBar_storage->setValue(storagePercent);

        if (storagePercent < 10)
        {
            ui->progressBar_storage->setStyleSheet(
                "QProgressBar {"
                "    border: 1px solid gray;"
                "    border-radius: 4px;"
                "    text-align: center;"
                "    background-color: #F0F0F0;"
                "    font-weight: bold;"
                "}"
                "QProgressBar::chunk {"
                "    background-color: red;"
                "}");
        }
        else
        {
            ui->progressBar_storage->setStyleSheet(
                "QProgressBar {"
                "    border: 1px solid gray;"
                "    border-radius: 4px;"
                "    text-align: center;"
                "    background-color: #F0F0F0;"
                "    font-weight: bold;"
                "}"
                "QProgressBar::chunk {"
                "    background-color: #a3ff99;"
                "}");
        }

    }
    else if(data==QByteArray::fromHex("54 53 41 43 4C"))
    {
        eraseDlg = createPleaseWaitDialog("⏳ Please Wait... !!!");

    }
    else if(data.startsWith(QByteArray::fromHex("54 53 44 4F")))
    {
        eraseDlg->close();
        eraseDlg = nullptr;
        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this,"erased","Data Erased successfully");
        });
    }
    else if(data.startsWith("NO_ERASE"))
    {
        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this,"No Events","No Events To Erase");
        });

    }

    else if(data.startsWith("LIVE"))
    {
        QByteArray packet = data.mid(4);

        if(packet.size() != 2058)
        {
            qDebug() << "Invalid LIVE Packet Size:"
                     << packet.size();

            return;
        }

        // Print complete packet only if config.txt contains 1
        if(isPacketLoggingEnabled())
        {
            livePacketCount++;

            QString packetHex = packet.toHex(' ').toUpper();

            writeToNotes(
                QString("Packet %1:\n%2")
                    .arg(livePacketCount)
                    .arg(packetHex)
            );
        }

        // Remove Header
        // Extract 2052-byte Payload
        QByteArray payload = packet.mid(3, 2052);

        processLivePacket(payload);
    }

    else if(data.startsWith("STOP_LIVE"))
    {

        ui->pushButton_stopLivePlot->setText("Stopped");
        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this,"Success","CSV File generated on Desktop");
        });
    }
    // msgId = 0x10
    else if(data.startsWith("ACK_1"))
    {
        constexpr quint32 TOTAL_PAGES      = 261115;
        constexpr quint32 SAMPLES_PER_PAGE = 170;

        quint64 totalSamples =
                static_cast<quint64>(ui->spinBox_samplingfrequency->value()) *
                static_cast<quint64>(ui->spinBox_logTime->value());

        // One page stores exactly 170 samples
        quint64 requiredPages =
                (totalSamples + SAMPLES_PER_PAGE - 1) / SAMPLES_PER_PAGE;

        double requiredPercentage =
                (static_cast<double>(requiredPages) * 100.0) / TOTAL_PAGES;

        double availableStorage =
                ui->doubleSpinBox_availableStorage->value();

        QTimer::singleShot(0, this,
                           [this, requiredPages,
                            requiredPercentage, availableStorage]()
        {
            if(requiredPercentage > availableStorage)
            {
                ui->pushButton_startLog->setEnabled(false);
                QMessageBox::warning(
                            this,
                            "Insufficient Storage",
                            QString("Required Storage : %1 %\n"
                                    "Available Storage : %2 %\n\n"
                                    "Please reduce the Log Time or Sampling Frequency.")
                            .arg(QString::number(requiredPercentage, 'f', 2))
                            .arg(QString::number(availableStorage, 'f', 2)));
            }
            else
            {
                ui->pushButton_startLog->setEnabled(true);
                QMessageBox::information(
                            this,
                            "Success",
                            QString("Parameters Set Successfully\n\n"
                                    "Required Pages : %1\n"
                                    "Required Storage : %2 %")
                            .arg(requiredPages)
                            .arg(QString::number(requiredPercentage, 'f', 2)));
            }
        });

    }
    else if(data.startsWith("BATT"))
    {
        QByteArray battBytes = data.mid(6);
        qDebug()<<"battBytes: "<<battBytes.toHex();

        showBatteryInUi(battBytes);
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
        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this, "Calibration Removed",
                                     "Screen DPI reset to system default.\nRestart app to apply.");
        });
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

    QTimer::singleShot(0, this, [this,width,height,diagonalInches,ppi](){
        QMessageBox::information(this, "Calibration Done",
                                 QString("Resolution: %1 x %2\nDiagonal: %3 in\nDPI set to %4.\nRestart app to apply.")
                                 .arg(width).arg(height).arg(diagonalInches).arg(ppi, 0, 'f', 2));
    });
}


void MainWindow::on_pushButton_getEventData_clicked()
{
    bool ok;
    QString text = ui->lineEdit_enterEventId->text().trimmed();
    int eventId = text.toInt(&ok);

    if(!ok || eventId < 0 || eventId > 65535)
    {
        QTimer::singleShot(0, this, [this](){
            QMessageBox::warning(this, "Error", "Please enter a valid Event ID (0–65535)");
        });
        return;
    }

    initializeAllPlots();

    on_pushButton_clearPoints_fft_clicked();

    csvLoaded = false;
    loadedCsvData = CsvPlotData();

    // Start the timeout timer
    responseTimer->start(2000); // 2 Sec timer

    batteryTimer->stop();

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

    dlgPlot = createPleaseWaitDialog("⌛ Please Wait Loading Plot !!!");

    serialObj->writeData(command);

}

void MainWindow::on_pushButton_startLog_clicked()
{
    if(ui->spinBox_logTime->value() == 0){
        QTimer::singleShot(0, this, [this](){
            QMessageBox::warning(this,"Failed","Please set the log time");
        });
        return;
    }


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


void MainWindow::on_pushButton_enlargePlot_clicked()
{
    QString selected = ui->comboBox_enlargePlot->currentText();
    QCustomPlot *plot = nullptr;

    if (selected == "ADXL_X")          plot = ui->customPlot_adxl_x;
    else if (selected == "ADXL_Y")     plot = ui->customPlot_adxl_y;
    else if (selected == "ADXL_Z")     plot = ui->customPlot_adxl_z;
    else if (selected == "ADXL_X2") plot = ui->customPlot_adxl_x2;
    else if (selected == "ADXL_Y2") plot = ui->customPlot_adxl_y2;
    else if (selected == "ADXL_Z2") plot = ui->customPlot_adxl_z2;
    else if (selected == "Temperature") plot = ui->customPlot_new_Temp;
    else if (selected == "Pressure") plot = ui->customPlot_new_Pressure;

    if (!plot)
    {
        QTimer::singleShot(0, this, [this](){
            QMessageBox::warning(this, "Warning", "Please select a valid plot to enlarge!");
        });
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
        ui->customPlot_adxl_x2,
        ui->customPlot_adxl_y2,
        ui->customPlot_adxl_z2,
        ui->customPlot_new_Temp,
        ui->customPlot_new_Pressure
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
        QTimer::singleShot(0, this, [this,selectedFile](){
            QMessageBox::information(this, "Success",
                                     " Log data saved successfully at:\n" + selectedFile);
        });
    }
    else
    {
        QTimer::singleShot(0, this, [this](){
            QMessageBox::critical(this, "Error",
                                  " Failed to save log data.\nPlease check permissions or path.");
        });
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
        ui->customPlot_adxl_x2,
        ui->customPlot_adxl_y2,
        ui->customPlot_adxl_z2,
        ui->customPlot_new_Temp,
        ui->customPlot_new_Pressure
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
        ui->customPlot_adxl_z_FFT,
        ui->customPlot_adxl_x_FFT_2,
        ui->customPlot_adxl_y_FFT_2,
        ui->customPlot_adxl_z_FFT_2


    };

    for(QCustomPlot *plot : allPlots)
    {
        if(!plot)
            continue;

        bool hasData = false;

        for(int i = 0; i < plot->graphCount(); ++i)
        {
            if(plot->graph(i)->dataCount() > 0)
            {
                hasData = true;
                break;
            }
        }

        if(!hasData)
            continue;


        plot->rescaleAxes(true);
        plot->replot(QCustomPlot::rpQueuedReplot);
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
    clearPlot(ui->customPlot_adxl_x_FFT_2);
    clearPlot(ui->customPlot_adxl_y_FFT_2);
    clearPlot(ui->customPlot_adxl_z_FFT_2);

    fftTracers.clear();
    fftLabels.clear();
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
    QByteArray packet
            = QByteArray::fromHex("AA BB 71 FF");

    qDebug() << "Get Current Parameters cmd sent: " + hexBytes(packet);
    writeToNotes("Get Current Parameters cmd sent: " + hexBytes(packet));

    emit sendMsgId(0x06);
    serialObj->writeData(packet);
}
void MainWindow::on_pushButton_erase_clicked()
{
    QByteArray eraseCmd=QByteArray::fromHex("535441");
    QMessageBox::StandardButton reply;

    reply = QMessageBox::question(this, "Confirm", "Do you want to Erase logs?",
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
    {
        responseTimer->start(2000);
        emit sendMsgId(0x07);
        serialObj->writeData(eraseCmd);
    }
    else{
        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this,"Cancelled","User cancel the erase logs");
        });
    }
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


void MainWindow::blinkLabel(QLabel *label,
                            int durationMs,
                            const QString &text)
{
    if (!label)
        return;

    // Create timer if not exists for this label
    if (!blinkTimers.contains(label)) {
        QTimer *timer = new QTimer(this);
        timer->setSingleShot(true);

        connect(timer, &QTimer::timeout, this, [=]() {
            label->setStyleSheet("");
            label->setText("Status");
        });

        blinkTimers[label] = timer;
    }

    QTimer *timer = blinkTimers[label];

    // 🔥 KEY: cancel previous pending reset
    timer->stop();

    // Update UI immediately
    label->setText(text);

    label->setStyleSheet("background-color: yellow;");

    // Start fresh timer
    timer->start(durationMs);
}

QString MainWindow::createAdxlCsvPath(bool live)
{
    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString folderPath =
            desktopPath +
            (live ? "/ADXL_CSV_LIVE"
                  : "/ADXL_CSV");

    QDir dir;

    if(!dir.exists(folderPath))
        dir.mkpath(folderPath);

    QString timestamp =
            QDateTime::currentDateTime()
            .toString("yyyyMMdd_HHmmss");

    return folderPath +
            "/" +
            (live ?
                 "ADXL_LIVE_"
               : "ADXL_Data_")
            + timestamp +
            ".csv";
}

void MainWindow::startAdxlCsvSaving(
        std::function<void()> onFinished)
{
    QString csvPath =
            createAdxlCsvPath();

    qDebug()
            << "Saving ADXL CSV:"
            << csvPath;

    QFutureWatcher<bool> *watcher =
            new QFutureWatcher<bool>(
                this);

    connect(watcher,
            &QFutureWatcher<bool>::finished,
            this,
            [=]()
    {
        bool ok =
                watcher->result();

        qDebug()
                << "CSV Save Finished:"
                << ok;

        watcher->deleteLater();

        if(!ok)
        {
            QTimer::singleShot(0, this, [this](){
                QMessageBox::critical(
                            this,
                            "Error",
                            "Failed to save CSV file.");
            });

            return;
        }

        if(onFinished)
        {
            onFinished();
        }
    });

    watcher->setFuture(
                QtConcurrent::run(
                    [=]()
    {
        return saveAdxlToCsv(
                    finalSampleIndexNew,

                    finalX1AdxlNew,
                    finalY1AdxlNew,
                    finalZ1AdxlNew,

                    finalX2AdxlNew,
                    finalY2AdxlNew,
                    finalZ2AdxlNew,

                    finalTempIndexNew,
                    finalTemperatureNew,

                    finalPressureIndexNew,
                    finalPressureNew,

                    csvPath);
    }));
}

bool MainWindow::saveAdxlToCsv(
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

        const QString &filePath)
{

    Q_UNUSED(tempIndex);
    Q_UNUSED(pressureIndex);

    QFile file(filePath);

    if(!file.open(
                QIODevice::WriteOnly |
                QIODevice::Text))
    {
        qCritical()
                << "Failed to create CSV:"
                << filePath;

        return false;
    }

    QTextStream out(&file);

    out.setRealNumberNotation(
                QTextStream::FixedNotation);

    out.setRealNumberPrecision(6);

    //----------------------------------------------------
    // Metadata
    //----------------------------------------------------

    out
            << "Event ID,"
            << eventId
            << ",Unit Number,"
            << static_cast<int>(unitNo)
            << ",Acceleration Frequency (Hz),"
            << accFrequency
            << ",Start Time,"
            << formattedStart
            << ",End Time,"
            << formattedEnd
            << "\n";

    //----------------------------------------------------
    // Combined Data Header
    //----------------------------------------------------

    out
            << "Time in (Us),"
            << "Ax_100(g),"
            << "Ay_100(g),"
            << "Az_100(g),"
            << "Ax_500(g),"
            << "Ay_500(g),"
            << "Az_500(g),"
            << "Pressure\n";

    //----------------------------------------------------
    // Combined Data
    //----------------------------------------------------

    const int adxlRows =
            sampleIndex.size();

    for(int i = 0;
        i < adxlRows;
        ++i)
    {
        out
                << sampleIndex[i] * (1000000.0 / accFrequency) << ","
                << x1Adxl[i] << ","
                << y1Adxl[i] << ","
                << z1Adxl[i] << ","
                << x2Adxl[i] << ","
                << y2Adxl[i] << ","
                << z2Adxl[i] << ",";


        // Pressure Column
        if(i < pressure.size())
        {
            out << pressure[i];
        }

        out << "\n";

        if(i > 0 &&
                i % 100000 == 0)
        {
            out.flush();

            qDebug()
                    << "CSV Rows Written:"
                    << i;
        }
    }

    //----------------------------------------------------
    // Finish
    //----------------------------------------------------

    out.flush();

    file.flush();
    file.close();

    qDebug()
            << "CSV save completed:"
            << filePath;

    qDebug()
            << "ADXL Rows:"
            << adxlRows;

    qDebug()
            << "Temperature Rows:"
            << temperature.size();

    qDebug()
            << "Pressure Rows:"
            << pressure.size();

    return true;
}

void MainWindow::processLivePacket(const QByteArray &payload)
{
    if(payload.size() != 2052)
           return;

       //-------------------------------------------------------
       // HARDWARE PACKET NUMBER
       //-------------------------------------------------------

       QByteArray packetNumberBytes =
               payload.left(4);

       quint32 hardwarePacketNumber =
               (static_cast<quint8>(packetNumberBytes[0]) << 24) |
               (static_cast<quint8>(packetNumberBytes[1]) << 16) |
               (static_cast<quint8>(packetNumberBytes[2]) << 8)  |
               static_cast<quint8>(packetNumberBytes[3]);

    //-------------------------------------------------------
    // ADXL DATA
    //-------------------------------------------------------

    QByteArray adxlBytes = payload.mid(4, 2040);

    for(int i = 0; i + 11 < adxlBytes.size(); i += 12)
    {
        quint16 x1 =
                (static_cast<quint8>(adxlBytes[i+1]) << 8) |
                static_cast<quint8>(adxlBytes[i]);

        quint16 y1 =
                (static_cast<quint8>(adxlBytes[i+3]) << 8) |
                static_cast<quint8>(adxlBytes[i+2]);

        quint16 z1 =
                (static_cast<quint8>(adxlBytes[i+5]) << 8) |
                static_cast<quint8>(adxlBytes[i+4]);

        quint16 x2 =
                (static_cast<quint8>(adxlBytes[i+7]) << 8) |
                static_cast<quint8>(adxlBytes[i+6]);

        quint16 y2 =
                (static_cast<quint8>(adxlBytes[i+9]) << 8) |
                static_cast<quint8>(adxlBytes[i+8]);

        quint16 z2 =
                (static_cast<quint8>(adxlBytes[i+11]) << 8) |
                static_cast<quint8>(adxlBytes[i+10]);

        double x1f = (x1 / 65535.0) * 5.12;
        x1f = ( x1f - x1Bias ) / 0.012563;

        double y1f = (y1 / 65535.0) * 5.12;
        y1f = ( y1f - y1Bias ) / 0.011812;

        double z1f = (z1 / 65535.0) * 5.12;
        z1f = ( z1f - z1Bias ) / 0.012346;

        double x2f = (x2 / 65535.0) * 5.12;
        x2f = ( x2f - x2Bias ) / 0.002576;

        double y2f = (y2 / 65535.0) * 5.12;
        y2f = ( y2f - y2Bias ) / 0.002556;

        double z2f = (z2 / 65535.0) * 5.12;
        z2f = ( z2f - z2Bias ) / 0.002917;

        peakAx100 = qMax(peakAx100, x1f);
        peakAy100 = qMax(peakAy100, y1f);
        peakAz100 = qMax(peakAz100, z1f);

        peakAx500 = qMax(peakAx500, x2f);
        peakAy500 = qMax(peakAy500, y2f);
        peakAz500 = qMax(peakAz500, z2f);

        liveSampleNumber++;

        int localSampleNumber =
                (i / 12) + 1;

        double globalSampleNumber =
                (hardwarePacketNumber * 170)
                + localSampleNumber;

        double timeUS =
                globalSampleNumber *
                liveSamplePeriodUS;

        //-------------------------------------------------------
        // Store for CSV
        //-------------------------------------------------------

        liveCsvData.sampleIndex.append(timeUS);

        liveCsvData.x1Loaded.append(x1f);
        liveCsvData.y1Loaded.append(y1f);
        liveCsvData.z1Loaded.append(z1f);

        liveCsvData.x2Loaded.append(x2f);
        liveCsvData.y2Loaded.append(y2f);
        liveCsvData.z2Loaded.append(z2f);

        //-------------------------------------------------------
        // Update Circular Buffer
        //-------------------------------------------------------

        plotAx100[writeIndex] = x1f;
        plotAy100[writeIndex] = y1f;
        plotAz100[writeIndex] = z1f;

        plotAx500[writeIndex] = x2f;
        plotAy500[writeIndex] = y2f;
        plotAz500[writeIndex] = z2f;

        writeIndex++;

        if(writeIndex >= LIVE_WINDOW)
        {
            writeIndex = 0;
        }

    }

    // Peaks
    ui->lineEdit_Ax_100_peak->setText(QString::number(peakAx100, 'f', 3));
    ui->lineEdit_Ay_100_peak->setText(QString::number(peakAy100, 'f', 3));
    ui->lineEdit_Az_100_peak->setText(QString::number(peakAz100, 'f', 3));

    ui->lineEdit_Ax_500_peak->setText(QString::number(peakAx500, 'f', 3));
    ui->lineEdit_Ay_500_peak->setText(QString::number(peakAy500, 'f', 3));
    ui->lineEdit_Az_500_peak->setText(QString::number(peakAz500, 'f', 3));

    //-------------------------------------------------------
    // Temperature
    //-------------------------------------------------------

    QByteArray tempBytes = payload.mid(2040,4);

    double temp = bytesToFloatMSB(tempBytes);

    // Changing temperature bytes to battery bytes
    showBatteryInUi(tempBytes,true);

    liveTempSampleNumber++;

    ui->customPlot_new_Temp_live
            ->graph(0)
            ->addData(liveTempSampleNumber,temp);

    //-------------------------------------------------------
    // Pressure
    //-------------------------------------------------------

    QByteArray pressureBytes = payload.mid(2044,4);

    double pressure =
            bytesToFloatMSB(pressureBytes);

    pressure = pressure/100.0;

    peakPressure = qMax(peakPressure, pressure);
    ui->lineEdit_pressure_peak->setText(QString::number(peakPressure, 'f', 3));

    plotPressure[pressureWriteIndex] = pressure;
    pressureWriteIndex++;

    if (pressureWriteIndex >= LIVE_WINDOW)
        pressureWriteIndex = 0;

    //-------------------------------------------------------
    // Store Temp & Pressure for CSV
    //-------------------------------------------------------

    for(int i = 0; i < 170; i++)
    {
        liveCsvData.tempLoaded.append(temp);
        liveCsvData.pressureLoaded.append(pressure);
    }

    //-------------------------------------------------------
    // Remove old samples for pressure and temperature
    //-------------------------------------------------------

    if(liveTempSampleNumber > LIVE_WINDOW)
    {
        ui->customPlot_new_Temp_live
                ->graph(0)
                ->data()
                ->removeBefore(
                    liveTempSampleNumber-LIVE_WINDOW);
    }

    //-------------------------------------------------------
    // Move X Axis for temperature and pressure
    //-------------------------------------------------------

    ui->customPlot_new_Temp_live->xAxis->setRange(
                liveTempSampleNumber,
                LIVE_WINDOW,
                Qt::AlignRight);

    //-------------------------------------------------------
    // Refresh
    //-------------------------------------------------------

    updateDisplayBuffer(plotAx100, displayAx100, writeIndex);
    updateDisplayBuffer(plotAy100, displayAy100, writeIndex);
    updateDisplayBuffer(plotAz100, displayAz100, writeIndex);

    updateDisplayBuffer(plotAx500, displayAx500, writeIndex);
    updateDisplayBuffer(plotAy500, displayAy500, writeIndex);
    updateDisplayBuffer(plotAz500, displayAz500, writeIndex);

    updateDisplayBuffer(plotPressure, displayPressure, pressureWriteIndex);

    //-------------------------------------------------------
    // Update Graph Data
    //-------------------------------------------------------

    ui->customPlot_adxl_x_live->graph(0)->setData(plotX, displayAx100);
    ui->customPlot_adxl_y_live->graph(0)->setData(plotX, displayAy100);
    ui->customPlot_adxl_z_live->graph(0)->setData(plotX, displayAz100);

    ui->customPlot_adxl_x2_live->graph(0)->setData(plotX, displayAx500);
    ui->customPlot_adxl_y2_live->graph(0)->setData(plotX, displayAy500);
    ui->customPlot_adxl_z2_live->graph(0)->setData(plotX, displayAz500);

    ui->customPlot_new_Pressure_live
            ->graph(0)
            ->setData(plotX, displayPressure);


    for(auto plot : livePlots)
    {
        plot->replot(QCustomPlot::rpQueuedReplot);
    }

    //-------------------------------------------------------
    // Flush CSV every 500 samples
    //-------------------------------------------------------

    constexpr int CSV_FLUSH_SIZE = 500;

    if(liveCsvData.sampleIndex.size() >=
            CSV_FLUSH_SIZE)
    {
        appendLiveCsv();
    }
}

void MainWindow::startLiveCsv()
{
    liveCsvPath = createAdxlCsvPath(true);

    liveCsvFile.setFileName(liveCsvPath);

    if(!liveCsvFile.open(QIODevice::WriteOnly |
                         QIODevice::Text))
    {
        qDebug() << "Unable to create Live CSV";

        return;
    }

    liveCsvStream.setDevice(&liveCsvFile);

    liveCsvStream.setRealNumberNotation(
                QTextStream::FixedNotation);

    liveCsvStream.setRealNumberPrecision(6);

    //-------------------------------------------------------
    // Header
    //-------------------------------------------------------

    liveCsvStream
            << "Time(micro second),"
            << "Ax_100(g),"
            << "Ay_100(g),"
            << "Az_100(g),"
            << "Ax_500(g),"
            << "Ay_500(g),"
            << "Az_500(g),"
            << "Pressure(mbar)\n";

    liveCsvStream.flush();

    liveCsvStarted = true;

    qDebug() << "Live CSV Started";
    qDebug() << liveCsvPath;
}

void MainWindow::appendLiveCsv()
{
    if(!liveCsvStarted)
        return;

    const int rows =
            liveCsvData.sampleIndex.size();

    for(int i=0;i<rows;i++)
    {
        liveCsvStream

                << liveCsvData.sampleIndex[i] << ","

                << liveCsvData.x1Loaded[i] << ","
                << liveCsvData.y1Loaded[i] << ","
                << liveCsvData.z1Loaded[i] << ","

                << liveCsvData.x2Loaded[i] << ","
                << liveCsvData.y2Loaded[i] << ","
                << liveCsvData.z2Loaded[i] << ","

                << liveCsvData.pressureLoaded[i]

                   << "\n";
    }

    liveCsvStream.flush();

    liveCsvFile.flush();

    qDebug()
            << "Flushed"
            << rows
            << "Rows";

    clearLiveCsvBuffer();
}

void MainWindow::finishLiveCsv()
{
    if(!liveCsvStarted)
        return;

    if(!liveCsvData.sampleIndex.isEmpty())
    {
        appendLiveCsv();
    }

    liveCsvStream.flush();

    liveCsvFile.flush();

    liveCsvFile.close();

    liveCsvStarted = false;

    qDebug()
            << "Live CSV Finished";
}

void MainWindow::clearLiveCsvBuffer()
{
    liveCsvData.sampleIndex.clear();

    liveCsvData.x1Loaded.clear();
    liveCsvData.y1Loaded.clear();
    liveCsvData.z1Loaded.clear();

    liveCsvData.x2Loaded.clear();
    liveCsvData.y2Loaded.clear();
    liveCsvData.z2Loaded.clear();

    liveCsvData.tempLoaded.clear();
    liveCsvData.pressureLoaded.clear();
}

void MainWindow::updateDisplayBuffer(
        const QVector<double> &source,
        QVector<double> &display,
        int currentWriteIndex)
{
    int startIndex;

    if(ui->checkBox_oscilloscopeMode->isChecked())
    {
        // Temporary fixed trigger point
        startIndex = LIVE_WINDOW / 4;
    }
    else
    {
        // Existing behaviour
        startIndex = currentWriteIndex;
    }

    const int tailSize = LIVE_WINDOW - startIndex;

    // Tail
    std::copy(source.begin() + startIndex,
              source.end(),
              display.begin());

    // Head
    std::copy(source.begin(),
              source.begin() + startIndex,
              display.begin() + tailSize);
}

MainWindow::FFTResult MainWindow::computeFFT(const QVector<double>& signal,
                                 double Fs,
                                 QCustomPlot *plot)
{
    FFTResult result;
    result.plot = plot;

    QVector<double> processed = signal;

    removeDC(processed);
    applyHanning(processed);

    performFFT(processed,
               result.mag,
               result.freq,
               Fs);

    return result;
}

void MainWindow::clearPeakValues()
{
    peakAx100 = 0;
    peakAy100 = 0;
    peakAz100 = 0;

    peakAx500 = 0;
    peakAy500 = 0;
    peakAz500 = 0;

    peakPressure = 0;

    ui->lineEdit_Ax_100_peak->clear();
    ui->lineEdit_Ay_100_peak->clear();
    ui->lineEdit_Az_100_peak->clear();
    ui->lineEdit_Ax_500_peak->clear();
    ui->lineEdit_Ay_500_peak->clear();
    ui->lineEdit_Az_500_peak->clear();
    ui->lineEdit_pressure_peak->clear();
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


    // --- Execute safely ---
    kiss_fft(cfg, timeData.data(), freqData.data());

    qDebug() << "FFT complete";

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

    removeDC(processed);      //1.Remove mean
    applyHanning(processed);   // 2. apply window


    QVector<double> mag, freq;

    performFFT(processed, mag, freq, Fs);  // 3. FFT

    // ---- Plot (correct way) ----
    if (plot->graphCount() > 0)
    {
        plot->graph(0)->setData(freq, mag);

        plot->xAxis->setRange(0, Fs/2);   // do NOT auto-rescale X
        plot->yAxis->rescale();           // only Y auto-scale

        plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

quint64 getCurrentProcessMemoryMB()
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(),
                         (PROCESS_MEMORY_COUNTERS*)&pmc,
                         sizeof(pmc));
    return pmc.WorkingSetSize / (1024 * 1024);
}


void MainWindow::on_pushButton_stopLivePlot_clicked()
{

    responseTimer->start(2000);

    batteryTimer->start(3000);

    finishLiveCsv();

    QByteArray stopPlot = QByteArray::fromHex("535458");
    writeToNotes("stop command send:"+stopPlot.toHex(' ').toUpper());
    emit sendMsgId(0x11);
    serialObj->writeData(stopPlot);

}

void MainWindow::on_pushButton_startLive_clicked()
{
    if(ui->lineEdit_windowSize->text().toInt() > 30000)
    {
        QTimer::singleShot(0, this, [this](){
            QMessageBox::warning(this,"Error","Please Use Sample Number Below 30000");
        });
        return;
    }

    // Clearing debug live packet counter
    livePacketCount = 0;

    //Clearing previous peak values
    clearPeakValues();

    responseTimer->start(2000);

    //Initializing X and Y Ranges
    LIVE_WINDOW = ui->lineEdit_windowSize->text().toInt();

    on_pushButton_currentParameters_clicked();
    pauseFor(100);

    batteryTimer->stop();

    initializeAllPlots();

    clearLiveCsvBuffer();

    liveSampleNumber = 0;
    liveTempSampleNumber = 0;

    int sampleRate =
            ui->spinBox_samplingfrequency->value();

    liveSamplePeriodUS =
            1000000.0 / sampleRate;

    startLiveCsv();

    QByteArray command;

    command.append(0x53);
    command.append(0x54);
    command.append(0x40);
    command.append(0x52);
    command.append(0x54);

    qDebug() << "Start Log cmd sent in liveplot : " + hexBytes(command);
    writeToNotes("Start Log cmd sent in livePlot: " + hexBytes(command));
    emit sendMsgId(0x14);
    serialObj->writeData(command);

    ui->pushButton_stopLivePlot->setText("Stop Plot");
}

void MainWindow::on_pushButton_fitToScreenLive_clicked()
{
    const double windowSize =
            ui->lineEdit_windowSize->text().toDouble();

    //---------------- ADXL 100 ----------------

    double adxl100Min =
            ui->lineEdit_ADXL_100g->text().toDouble();

    double adxl100Max =
            ui->lineEdit_ADXL_100g_2->text().toDouble();

    ui->customPlot_adxl_x_live->xAxis->setRange(0, windowSize);
    ui->customPlot_adxl_y_live->xAxis->setRange(0, windowSize);
    ui->customPlot_adxl_z_live->xAxis->setRange(0, windowSize);

    ui->customPlot_adxl_x_live->yAxis->setRange(adxl100Min, adxl100Max);
    ui->customPlot_adxl_y_live->yAxis->setRange(adxl100Min, adxl100Max);
    ui->customPlot_adxl_z_live->yAxis->setRange(adxl100Min, adxl100Max);

    //---------------- ADXL 500 ----------------

    double adxl500Min =
            ui->lineEdit_ADXL_500g->text().toDouble();

    double adxl500Max =
            ui->lineEdit_ADXL_500g_2->text().toDouble();

    ui->customPlot_adxl_x2_live->xAxis->setRange(0, windowSize);
    ui->customPlot_adxl_y2_live->xAxis->setRange(0, windowSize);
    ui->customPlot_adxl_z2_live->xAxis->setRange(0, windowSize);

    ui->customPlot_adxl_x2_live->yAxis->setRange(adxl500Min, adxl500Max);
    ui->customPlot_adxl_y2_live->yAxis->setRange(adxl500Min, adxl500Max);
    ui->customPlot_adxl_z2_live->yAxis->setRange(adxl500Min, adxl500Max);


    //---------------- Pressure ----------------

    double pressureMin =
            ui->lineEdit_pressureRange->text().toDouble();

    double pressureMax =
            ui->lineEdit_pressureRange_2->text().toDouble();

    ui->customPlot_new_Pressure_live->xAxis->setRange(0, windowSize);
    ui->customPlot_new_Pressure_live->yAxis->setRange(pressureMin, pressureMax);

    //---------------- Replot ----------------

    for(auto plot : livePlots)
    {
        plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void MainWindow::on_pushButton_openFiles_clicked()
{
    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString folderPath =
            desktopPath
            + "/ADXL_CSV";

    QString filePath =
            QFileDialog::getOpenFileName(
                this,
                "Open ADXL CSV File",
                folderPath,
                "CSV Files (*.csv)");

    if(filePath.isEmpty())
    {
        return;
    }


    //------------------------------------------------
    // Loading Dialog
    //------------------------------------------------

    QDialog *loadingDialog =
            createPleaseWaitDialog(
                "⏳ Loading CSV File...");

    QFutureWatcher<CsvPlotData>
            *watcher =
            new QFutureWatcher<CsvPlotData>(
                this);

    connect(watcher,
            &QFutureWatcher<CsvPlotData>::finished,
            this,
            [=]()
    {
        CsvPlotData data =
                watcher->result();

        accFrequency = data.accFrequency;

        QString adxlFreqLabel;

        if(accFrequency > 0)
        {
            double samplePeriodUS = 1000000.0 / accFrequency;

            adxlFreqLabel =
                    QString("Sample Number (1 Sample = %1 µs)")
                    .arg(samplePeriodUS, 0, 'f', 2);
        }
        else
        {
            adxlFreqLabel = "Sample Number";
        }

        setupPlot(ui->customPlot_adxl_x,
                  adxlFreqLabel,
                  "Ax_100 (g)",1);

        setupPlot(ui->customPlot_adxl_y,
                  adxlFreqLabel,
                  "Ay_100 (g)",1);

        setupPlot(ui->customPlot_adxl_z,
                  adxlFreqLabel,
                  "Az_100 (g)",1);

        setupPlot(ui->customPlot_adxl_x2,
                  adxlFreqLabel,
                  "Ax_500 (g)",1);

        setupPlot(ui->customPlot_adxl_y2,
                  adxlFreqLabel,
                  "Ay_500 (g)",1);

        setupPlot(ui->customPlot_adxl_z2,
                  adxlFreqLabel,
                  "Az_500 (g)",1);

        watcher->deleteLater();

        //------------------------------------------------
        // Plot Helper
        //------------------------------------------------

        auto plotGraph =
                [](QCustomPlot *plot,
                const QVector<double> &x,
                const QVector<double> &y)
        {
            if(plot->graphCount() == 0 ||
                    x.isEmpty() ||
                    y.isEmpty())
            {
                return;
            }

            plot->setUpdatesEnabled(false);

            plot->graph(0)
                    ->data()
                    ->clear();

            constexpr int CHUNK_SIZE = 5000;

            for(int i = 0;
                i < x.size();
                i += CHUNK_SIZE)
            {
                int count =
                        qMin(CHUNK_SIZE,
                             x.size() - i);

                plot->graph(0)->addData(
                            x.mid(i, count),
                            y.mid(i, count));
            }

            plot->xAxis->setRange(
                        x.first(),
                        x.last());

            plot->graph(0)
                    ->rescaleValueAxis();

            plot->setUpdatesEnabled(true);

            plot->replot(
                        QCustomPlot::rpQueuedReplot);
        };

        //------------------------------------------------
        // ADXL #1
        //------------------------------------------------

        plotGraph(
                    ui->customPlot_adxl_x,
                    data.sampleIndex,
                    data.x1Loaded);

        plotGraph(
                    ui->customPlot_adxl_y,
                    data.sampleIndex,
                    data.y1Loaded);

        plotGraph(
                    ui->customPlot_adxl_z,
                    data.sampleIndex,
                    data.z1Loaded);

        //------------------------------------------------
        // ADXL #2
        //------------------------------------------------

        plotGraph(
                    ui->customPlot_adxl_x2,
                    data.sampleIndex,
                    data.x2Loaded);

        plotGraph(
                    ui->customPlot_adxl_y2,
                    data.sampleIndex,
                    data.y2Loaded);

        plotGraph(
                    ui->customPlot_adxl_z2,
                    data.sampleIndex,
                    data.z2Loaded);

        //------------------------------------------------
        // Temperature
        //------------------------------------------------

        QVector<double> tempIndex;

        for(int i = 0;
            i < data.tempLoaded.size();
            i++)
        {
            tempIndex.append(i + 1);
        }

        plotGraph(
                    ui->customPlot_new_Temp,
                    tempIndex,
                    data.tempLoaded);

        //------------------------------------------------
        // Pressure
        //------------------------------------------------

        QVector<double> pressureIndex;

        for(int i = 0;
            i < data.pressureLoaded.size();
            i++)
        {
            pressureIndex.append(i + 1);
        }

        plotGraph(
                    ui->customPlot_new_Pressure,
                    pressureIndex,
                    data.pressureLoaded);

        //------------------------------------------------
        // Close Loading Dialog
        //------------------------------------------------

        if(loadingDialog)
        {
            loadingDialog->close();
            loadingDialog->deleteLater();
        }

        //------------------------------------------------
        // Debug
        //------------------------------------------------

        qDebug() << "Loaded Samples:"
                 << data.sampleIndex.size();

        qDebug() << "Loaded X1:"
                 << data.x1Loaded.size();

        qDebug() << "Loaded Y1:"
                 << data.y1Loaded.size();

        qDebug() << "Loaded Z1:"
                 << data.z1Loaded.size();

        qDebug() << "Loaded X2:"
                 << data.x2Loaded.size();

        qDebug() << "Loaded Y2:"
                 << data.y2Loaded.size();

        qDebug() << "Loaded Z2:"
                 << data.z2Loaded.size();

        qDebug() << "Loaded Temp:"
                 << data.tempLoaded.size();

        qDebug() << "Loaded Pressure:"
                 << data.pressureLoaded.size();

        loadedCsvData = data;
        csvLoaded = true;

        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(
                        this,
                        "Success",
                        "CSV loaded successfully.");
        });
    });

    watcher->setFuture(
                QtConcurrent::run(
                    [=]()
    {
        return loadAdxlCsv(
                    filePath);
    }));
}

void MainWindow::on_pushButton_setCurrentParameters_clicked()
{
    responseTimer->start(2000);

    if(ui->spinBox_logTime->value()>65535||ui->spinBox_logTime->value()<1){
        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this,"Out Of Range","Enter the value between 1 and 65535 for log time");
        });
        return;
    }

    if(ui->spinBox_samplingfrequency->value()>10000 || ui->spinBox_samplingfrequency->value()<1){
        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this,"Out Of Range","Enter the value between 1 and 10000 for ADXL Sampling Frequency");
        });
        return;
    }

    QByteArray command;

    command.append(static_cast<quint8>(0xAA)); //1
    command.append(static_cast<quint8>(0xBB)); //2

    command.append(static_cast<quint8>(0x61)); //3

    command.append(static_cast<quint8>(ui->spinBox_unitNumber->value())); //4

    quint16 logtime = ui->spinBox_logTime->value();
    command.append(static_cast<quint8>((logtime >> 8) & 0xFF)); //MSB 5
    command.append(static_cast<quint8>(logtime & 0xFF)); //LSB 6

    quint16 samplingFreq = ui->spinBox_samplingfrequency->value();
    command.append(static_cast<quint8>((samplingFreq >> 8) & 0xFF)); //7
    command.append(static_cast<quint8>(samplingFreq & 0xFF)); //8

    QDateTime dt = QDateTime::currentDateTime();

    ui->dateTimeEdit->setDateTime(dt);

    quint8 year  = dt.date().year();
    quint8 month = dt.date().month();
    quint8 day   = dt.date().day();

    quint8 hour   = dt.time().hour();
    quint8 minute = dt.time().minute();
    quint8 second = dt.time().second();

    command.append(static_cast<quint8>(hour));
    command.append(static_cast<quint8>(minute));
    command.append(static_cast<quint8>(second));
    command.append(static_cast<quint8>(day));
    command.append(static_cast<quint8>(month));
    command.append(static_cast<quint8>(year - 2000)); // if protocol needs 2-digit year 14

    quint8 mode = ui->radioButton_powerON->isChecked() ? 0xAB : 0xAF;
    command.append(static_cast<quint8>(mode)); // 15

    command.append(static_cast<quint8>(0xEE)); //16
    command.append(static_cast<quint8>(0xFF)); //17

    qDebug() << "Set Current Parameters cmd sent: " + hexBytes(command);
    writeToNotes("Set Current Parameters cmd sent: " + hexBytes(command));

    emit sendMsgId(0x10);
    serialObj->writeData(command);
}

void MainWindow::on_pushButton_LoadFFT_clicked()
{
    //-------------------------------------------------------
    // Decide Data Source
    //-------------------------------------------------------

    const bool useLoadedCsv =
            csvLoaded &&
            !loadedCsvData.x1Loaded.isEmpty();

    if(!useLoadedCsv && finalX1AdxlNew.isEmpty())
    {
        QTimer::singleShot(0, this, [this](){
            QMessageBox::warning(this,
                                 "FFT",
                                 "No ADXL data available.");
        });
        return;
    }

    //-------------------------------------------------------
    // Sampling Frequency
    //-------------------------------------------------------

    const double sampleRate = accFrequency;

    if(sampleRate <= 0)
    {
        QTimer::singleShot(0, this, [this](){
            QMessageBox::warning(this,
                                 "FFT",
                                 "Invalid Sampling Frequency.");
        });
        return;
    }

    //-------------------------------------------------------
    // Data Selection
    //-------------------------------------------------------

    const QVector<double> &ax100 =
            useLoadedCsv ?
            loadedCsvData.x1Loaded :
            finalX1AdxlNew;

    const QVector<double> &ay100 =
            useLoadedCsv ?
            loadedCsvData.y1Loaded :
            finalY1AdxlNew;

    const QVector<double> &az100 =
            useLoadedCsv ?
            loadedCsvData.z1Loaded :
            finalZ1AdxlNew;

    const QVector<double> &ax500 =
            useLoadedCsv ?
            loadedCsvData.x2Loaded :
            finalX2AdxlNew;

    const QVector<double> &ay500 =
            useLoadedCsv ?
            loadedCsvData.y2Loaded :
            finalY2AdxlNew;

    const QVector<double> &az500 =
            useLoadedCsv ?
            loadedCsvData.z2Loaded :
            finalZ2AdxlNew;

    //-------------------------------------------------------
    // Selection
    //-------------------------------------------------------

    bool allSelected = ui->checkBox_All->isChecked();

    if(!allSelected &&
       !ui->checkBox_Ax_100->isChecked() &&
       !ui->checkBox_Ay_100->isChecked() &&
       !ui->checkBox_Az_100->isChecked() &&
       !ui->checkBox_Ax_500->isChecked() &&
       !ui->checkBox_Ay_500->isChecked() &&
       !ui->checkBox_Az_500->isChecked())
    {
        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this,
                                     "FFT",
                                     "Please select at least one parameter.");
        });

        return;
    }

    QList<QPair<QVector<double>, QCustomPlot*>> fftJobs;

    //-------------------------------------------------------
    // Loading Dialog
    //-------------------------------------------------------

    QDialog *fftDialog =
            createPleaseWaitDialog(
                "⏳ Please Wait ! FFT Plot Loading");

    //-------------------------------------------------------
    // Compute FFT
    //-------------------------------------------------------

    if(allSelected || ui->checkBox_Ax_100->isChecked())
    {
        fftJobs.append({ax100,
                          ui->customPlot_adxl_x_FFT});
    }

    if(allSelected || ui->checkBox_Ay_100->isChecked())
    {
        fftJobs.append({ay100,
                          ui->customPlot_adxl_y_FFT});
    }

    if(allSelected || ui->checkBox_Az_100->isChecked())
    {
        fftJobs.append({az100,
                          ui->customPlot_adxl_z_FFT});
    }

    if(allSelected || ui->checkBox_Ax_500->isChecked())
    {
        fftJobs.append({ax500,
                          ui->customPlot_adxl_x_FFT_2});
    }

    if(allSelected || ui->checkBox_Ay_500->isChecked())
    {
        fftJobs.append({ay500,
                          ui->customPlot_adxl_y_FFT_2});
    }

    if(allSelected || ui->checkBox_Az_500->isChecked())
    {
        fftJobs.append({az500,
                          ui->customPlot_adxl_z_FFT_2});
    }

    QString fftSource =
            useLoadedCsv ?
            "FFT generated from Loaded CSV." :
            "FFT generated from Live Acquisition.";

    qDebug() << fftSource;

    writeToNotes(fftSource);

    auto* fftWatcher = new QFutureWatcher<QList<FFTResult>>(this);

    connect(fftWatcher,
            &QFutureWatcher<QList<FFTResult>>::finished,
            this,
            [=]()
    {
        QList<FFTResult> results =
                fftWatcher->result();

        for(const FFTResult &r : results)
        {
            if(!r.plot)
                continue;

            r.plot->graph(0)->setData(r.freq,
                                      r.mag);

            r.plot->xAxis->setRange(0,
                                    sampleRate/2);

            r.plot->yAxis->rescale();

            r.plot->replot(
                QCustomPlot::rpQueuedReplot);
        }

        fftDialog->close();
        fftDialog->deleteLater();

        fftWatcher->deleteLater();

    });

    fftWatcher->setFuture(
        QtConcurrent::run(
            [=]()
    {
        QList<FFTResult> results;

        for(const auto &job : fftJobs)
        {
            results.append(
                computeFFT(job.first,
                           sampleRate,
                           job.second));
        }

        return results;
    }));

}

void MainWindow::on_pushButton_clearFFTplots_clicked()
{
    QList<QCustomPlot*> fftPlots =
    {
        ui->customPlot_adxl_x_FFT,
        ui->customPlot_adxl_y_FFT,
        ui->customPlot_adxl_z_FFT,

        ui->customPlot_adxl_x_FFT_2,
        ui->customPlot_adxl_y_FFT_2,
        ui->customPlot_adxl_z_FFT_2
    };

    //-------------------------------------------------------
    // Clear FFT Graphs
    //-------------------------------------------------------

    for(auto plot : fftPlots)
    {
        if(plot->graphCount() > 0)
        {
            plot->graph(0)->data()->clear();
        }

        plot->replot(QCustomPlot::rpQueuedReplot);
    }

    //-------------------------------------------------------
    // Remove Peak Markers / Labels
    //-------------------------------------------------------

    qDeleteAll(fftTracers);
    fftTracers.clear();

    qDeleteAll(fftLabels);
    fftLabels.clear();

    qDebug() << "FFT plots cleared.";
}


void MainWindow::on_pushButton_saveFFTplots_clicked()
{
    writeToNotes(QString::number(accFrequency)+" :accFrequency in saveFFTplots button");
    qDebug()<<accFrequency<<" :accFrequency";

    //-------------------------------------------------------
    // Check FFT Data Available
    //-------------------------------------------------------

    QList<QCustomPlot*> fftPlots =
    {
        ui->customPlot_adxl_x_FFT,
        ui->customPlot_adxl_y_FFT,
        ui->customPlot_adxl_z_FFT,
        ui->customPlot_adxl_x_FFT_2,
        ui->customPlot_adxl_y_FFT_2,
        ui->customPlot_adxl_z_FFT_2
    };

    bool hasFFTData = false;

    for(QCustomPlot *plot : fftPlots)
    {
        if(plot &&
           plot->graphCount() > 0 &&
           plot->graph(0)->dataCount() > 0)
        {
            hasFFTData = true;
            break;
        }
    }

    if(!hasFFTData)
    {
        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this,
                                     "FFT",
                                     "No FFT plot data available.\n"
                                     "Please load or generate FFT before saving.");
        });
        return;
    }

    //-------------------------------------------------------
    // Create Folder
    //-------------------------------------------------------

    QString desktop =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString folder =
            desktop + "/ADXL_FFT";

    QDir().mkpath(folder);

    //-------------------------------------------------------
    // File Name
    //-------------------------------------------------------

    QString fileName =
            folder +
            "/FFT_Data_" +
            QDateTime::currentDateTime()
            .toString("yyyyMMdd_hhmmss") +
            ".csv";

    //-------------------------------------------------------
    // Plot List
    //-------------------------------------------------------

    QList<QPair<QString,QCustomPlot*>> plots =
    {
        {"Ax_100", ui->customPlot_adxl_x_FFT},
        {"Ay_100", ui->customPlot_adxl_y_FFT},
        {"Az_100", ui->customPlot_adxl_z_FFT},
        {"Ax_500", ui->customPlot_adxl_x_FFT_2},
        {"Ay_500", ui->customPlot_adxl_y_FFT_2},
        {"Az_500", ui->customPlot_adxl_z_FFT_2}
    };

    // Collecting Data before QtConcurrent

    QList<FFTCsvData> fftData;

    for(const auto &p : plots)
    {
        FFTCsvData data;
        data.name = p.first;

        if(p.second &&
           p.second->graphCount() > 0)
        {
            auto graphData = p.second->graph(0)->data();

            data.freq.reserve(graphData->size());
            data.mag.reserve(graphData->size());

            for(auto it = graphData->constBegin();
                it != graphData->constEnd();
                ++it)
            {
                data.freq.append(it->key);
                data.mag.append(it->value);
            }
        }

        fftData.append(std::move(data));
    }


    QDialog *savingDialog =
            createPleaseWaitDialog(
                "⏳ Saving FFT CSV...");

    auto *watcher =
            new QFutureWatcher<bool>(this);

    connect(watcher,
            &QFutureWatcher<bool>::finished,
            this,
            [=]()
    {
        savingDialog->close();

        watcher->deleteLater();

        writeToNotes("FFT CSV Saved : " + fileName);

        QTimer::singleShot(0, this, [this](){
            QMessageBox::information(this,
                                     "Success",
                                     "FFT CSV saved successfully.");
        });
    });


    watcher->setFuture(
    QtConcurrent::run([=]() -> bool
    {
        QFile file(fileName);

        if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;

        QTextStream out(&file);
        out.setRealNumberNotation(QTextStream::FixedNotation);
        out.setRealNumberPrecision(6);

        //-------------------------------------------------------
        // Header
        //-------------------------------------------------------

        for(const auto &d : fftData)
        {
            out << d.name
                << " Frequency (Hz),"
                << d.name
                << " Magnitude (g),";
        }

        out << "\n";

        //-------------------------------------------------------
        // Max Rows
        //-------------------------------------------------------

        int maxRows = 0;

        for(const auto &d : fftData)
            maxRows = qMax(maxRows, d.freq.size());

        //-------------------------------------------------------
        // Write
        //-------------------------------------------------------

        for(int row = 0; row < maxRows; ++row)
        {
            for(const auto &d : fftData)
            {
                if(row < d.freq.size())
                {
                    out << d.freq[row] << ","
                        << d.mag[row] << ",";
                }
                else
                {
                    out << ",,";
                }
            }

            out << "\n";

            if(row > 0 && row % 20000 == 0)
            {
                out.flush();
                file.flush();
            }
        }

        out.flush();
        file.flush();
        file.close();

        return true;
    }));

}


void MainWindow::on_pushButton_clearLivePlots_clicked()
{
    // Clearing peak values
    clearPeakValues();

    //-------------------------------------------------------
    // Clear All Live Graph Data
    //-------------------------------------------------------

    for(auto plot : livePlots)
    {
        if(plot && plot->graphCount() > 0)
        {
            plot->graph(0)->data()->clear();
        }
    }

    //-------------------------------------------------------
    // Restore Axis Ranges
    //-------------------------------------------------------

    on_pushButton_fitToScreenLive_clicked();

    //-------------------------------------------------------
    // Replot
    //-------------------------------------------------------

    for(auto plot : livePlots)
    {
        plot->replot(QCustomPlot::rpQueuedReplot);
    }

    writeToNotes("Live plots cleared.");

    qDebug() << "Live plots cleared.";
}

void MainWindow::batteryCommand()
{
//    // Start the timeout timer
//    responseTimer->start(2000); // 2 Sec timer

    QByteArray command;

    command.append(0x80); //1
    command.append(0x81); //2
    command.append(0x82); //3


    qDebug() << "Battery cmd sent : " + hexBytes(command);
    //writeToNotes("Battery cmd sent : " + hexBytes(command));

    serialObj->writeData(command);
}

void MainWindow::showBatteryInUi(const QByteArray &battBytes, bool strangeCase)
{
    float battery = 0.0f;

    if (strangeCase)
        battery = bytesToFloatMSB(battBytes, false);
    else
        battery = bytesToFloatMSB(battBytes, true);

    qDebug() << "Battery Voltage:" << battery;

    // Clamp voltage to valid range
    if (battery <= 3.0f)
    {
        battery = 0.0f;
    }
    else if (battery >= 4.2f)
    {
        battery = 100.0f;
    }
    else if (battery >= 3.6f)
    {
        battery = 20.0f + ((battery - 3.6f) / (4.2f - 3.6f)) * 80.0f;
    }
    else
    {
        battery = ((battery - 3.0f) / (3.6f - 3.0f)) * 20.0f;
    }

    qDebug() << "Battery Percentage:" << battery;

    ui->label_battery->setText(
        QString("Battery %1 %").arg(battery, 0, 'f', 0));

    if (battery < 10.0f)
    {
        ui->label_battery->setStyleSheet(
            "QLabel {"
            "background-color: #ff5a54;"
            "color: white;"
            "border: 2px solid darkred;"
            "border-radius: 5px;"
            "}");
    }
    else
    {
        ui->label_battery->setStyleSheet(
            "QLabel {"
            "background-color: #4c85ff;"
            "color: white;"
            "border: 2px solid darkgreen;"
            "border-radius: 5px;"
            "}");
    }
}

void MainWindow::loadAdxlBiasValues()
{
    QString configPath =
            QCoreApplication::applicationDirPath()
            + "/config.txt";

    QFile file(configPath);

    if(!file.exists())
    {
        qDebug() << "config.txt not found. Using default ADXL bias values.";
        writeToNotes(
                    "config.txt not found. Using default ADXL bias values.");
        return;
    }

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Failed to open config.txt. Using default ADXL bias values.";
        writeToNotes(
                    "Failed to open config.txt. Using default ADXL bias values.");
        return;
    }

    // Row 1 = packet logging
    file.readLine();

    QStringList rows;

    while(!file.atEnd())
    {
        rows.append(
                    QString::fromUtf8(
                        file.readLine()).trimmed());
    }

    if(rows.size() < 6)
    {
        qDebug() << "Invalid ADXL bias configuration. Using defaults.";
        writeToNotes(
                    "Invalid ADXL bias configuration. Using defaults.");
        return;
    }

    bool okX1;
    bool okY1;
    bool okZ1;
    bool okX2;
    bool okY2;
    bool okZ2;

    double newX1Bias = rows[0].toDouble(&okX1);
    double newY1Bias = rows[1].toDouble(&okY1);
    double newZ1Bias = rows[2].toDouble(&okZ1);

    double newX2Bias = rows[3].toDouble(&okX2);
    double newY2Bias = rows[4].toDouble(&okY2);
    double newZ2Bias = rows[5].toDouble(&okZ2);

    if(!okX1 || !okY1 || !okZ1 ||
       !okX2 || !okY2 || !okZ2)
    {
        qDebug() << "Invalid ADXL bias value. Using defaults.";
        writeToNotes(
                    "Invalid ADXL bias value. Using defaults.");
        return;
    }

    x1Bias = newX1Bias;
    y1Bias = newY1Bias;
    z1Bias = newZ1Bias;

    x2Bias = newX2Bias;
    y2Bias = newY2Bias;
    z2Bias = newZ2Bias;

    qDebug() << "ADXL Bias Values Loaded:";
    qDebug() << "X1:" << x1Bias;
    qDebug() << "Y1:" << y1Bias;
    qDebug() << "Z1:" << z1Bias;
    qDebug() << "X2:" << x2Bias;
    qDebug() << "Y2:" << y2Bias;
    qDebug() << "Z2:" << z2Bias;

    writeToNotes(
        QString("ADXL Bias Values Loaded: "
                "X1=%1, Y1=%2, Z1=%3, "
                "X2=%4, Y2=%5, Z2=%6")
        .arg(x1Bias, 0, 'f', 7)
        .arg(y1Bias, 0, 'f', 7)
        .arg(z1Bias, 0, 'f', 7)
        .arg(x2Bias, 0, 'f', 7)
        .arg(y2Bias, 0, 'f', 7)
        .arg(z2Bias, 0, 'f', 7));
}

