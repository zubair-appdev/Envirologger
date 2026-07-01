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

    ui->dateTimeEdit->setDateTime(QDateTime(QDate(2025, 1, 1),
                                            QTime(0, 0, 0)));

    ui->spinBox_logTime->setRange(INT_MIN, INT_MAX);
    ui->spinBox_threshold->setRange(INT_MIN, INT_MAX);
    ui->spinBox_samplingfrequency->setRange(INT_MIN, INT_MAX);
    ui->spinBox_Inclinometer->setRange(INT_MIN, INT_MAX);

    ui->spinBox_logTime->setToolTip("Enter value from 1 to 65535");
    ui->spinBox_threshold->setToolTip("Enter value from -200 to +200");
    ui->spinBox_samplingfrequency->setToolTip("Enter value from 1 to 10000");
    ui->spinBox_Inclinometer->setToolTip("Enter value from 1 to 1000");

    uiUpdateTimer = new QTimer(this);
    uiUpdateTimer->setInterval(uiUpdateIntervalMs);
    //    connect(uiUpdateTimer, &QTimer::timeout, this, &MainWindow::onUiUpdateTimer);
    //    uiUpdateTimer->start();


    connect(ui->pushButton_clear,&QPushButton::clicked,ui->textEdit_rawBytes,&QTextEdit::clear);

    ui->comboBox_ports->addItems(serialObj->availablePorts());

    connect(ui->pushButton_portsRefresh,&QPushButton::clicked,this,&MainWindow::refreshPorts);

    connect(ui->comboBox_ports,SIGNAL(activated(const QString &)),this,SLOT(onPortSelected(const QString &)));

    connect(this,&MainWindow::sendMsgId,serialObj,&serialPortHandler::recvMsgId);
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
    if(!in.atEnd())
    {
        in.readLine();
    }

    // Empty row
    if(!in.atEnd())
    {
        in.readLine();
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

        if(values.size() < 9)
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
        // Temperature
        //------------------------------------------------

        if(values.size() > 7 &&
                !values[7].trimmed().isEmpty())
        {
            bool okTemp = false;

            double temp =
                    values[7].toDouble(&okTemp);

            if(okTemp)
            {
                result.tempLoaded.append(temp);
            }
        }

        //------------------------------------------------
        // Pressure
        //------------------------------------------------

        if(values.size() > 8 &&
                !values[8].trimmed().isEmpty())
        {
            bool okPressure = false;

            double pressure =
                    values[8].toDouble(&okPressure);

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

    plot->xAxis->setLabel(xLabel);
    plot->yAxis->setLabel("Amplitude(g)");
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

    // ---------- ADD EMPTY GRAPH ----------
    plot->addGraph();
    plot->graph(0)->setPen(QPen(neonGreen, 1));

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

    QString adxlFreqLabel = QString("Time (1 = %1)").arg("N/A");

    // ============================================================
    // ADXL PLOTS
    // ============================================================

    setupPlot(ui->customPlot_adxl_x,
              QString("Ax_100 %1").arg(adxlFreqLabel),
              "Voltage (g)");

    setupPlot(ui->customPlot_adxl_y,
              QString("Ay_100 %1").arg(adxlFreqLabel),
              "Voltage (g)");

    setupPlot(ui->customPlot_adxl_z,
              QString("Az_100 %1").arg(adxlFreqLabel),
              "Voltage (g)");

    ui->customPlot_adxl_x->addGraph();
    ui->customPlot_adxl_x->graph(0)->setPen(QPen(adxlColors[0], 1));

    ui->customPlot_adxl_y->addGraph();
    ui->customPlot_adxl_y->graph(0)->setPen(QPen(adxlColors[1], 1));

    ui->customPlot_adxl_z->addGraph();
    ui->customPlot_adxl_z->graph(0)->setPen(QPen(adxlColors[2], 1));

    // ============================================================
    // SECOND SET OF ADXL PLOTS
    // ============================================================

    setupPlot(ui->customPlot_adxl_x2,
              QString("Ax_500 %1").arg(adxlFreqLabel),
              "Voltage (g)");

    setupPlot(ui->customPlot_adxl_y2,
              QString("Ay_500 %1").arg(adxlFreqLabel),
              "Voltage (g)");

    setupPlot(ui->customPlot_adxl_z2,
              QString("Az_500 %1").arg(adxlFreqLabel),
              "Voltage (g)");

    ui->customPlot_adxl_x2->addGraph();
    ui->customPlot_adxl_x2->graph(0)->setPen(QPen(adxlColors[0], 1));

    ui->customPlot_adxl_y2->addGraph();
    ui->customPlot_adxl_y2->graph(0)->setPen(QPen(adxlColors[1], 1));

    ui->customPlot_adxl_z2->addGraph();
    ui->customPlot_adxl_z2->graph(0)->setPen(QPen(adxlColors[2], 1));

    // ============================================================
    // TEMPERATURE
    // ============================================================

    setupPlot(ui->customPlot_new_Temp,
              "Samples",
              "Temperature (°C)");

    ui->customPlot_new_Temp->addGraph();
    ui->customPlot_new_Temp->graph(0)->setPen(QPen(tempColor, 1));

    // ============================================================
    // PRESSURE
    // ============================================================

    setupPlot(ui->customPlot_new_Pressure,
              "Samples",
              "Pressure");

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

    adxl100Range =
            ui->lineEdit_ADXL_100g->text().toFloat();

    adxl500Range =
            ui->lineEdit_ADXL_500g->text().toFloat();

    pressureRange =
            ui->lineEdit_pressureRange->text().toFloat();

    // ADXL 100g
    ui->customPlot_adxl_x_live->yAxis->setRange(
                -adxl100Range,
                adxl100Range);

    ui->customPlot_adxl_y_live->yAxis->setRange(
                -adxl100Range,
                adxl100Range);

    ui->customPlot_adxl_z_live->yAxis->setRange(
                -adxl100Range,
                adxl100Range);

    // ADXL 500g
    ui->customPlot_adxl_x2_live->yAxis->setRange(
                -adxl500Range,
                adxl500Range);

    ui->customPlot_adxl_y2_live->yAxis->setRange(
                -adxl500Range,
                adxl500Range);

    ui->customPlot_adxl_z2_live->yAxis->setRange(
                -adxl500Range,
                adxl500Range);

    // Temperature & Pressure (keep fixed for now)
    ui->customPlot_new_Temp_live->yAxis->setRange(0,100);
    ui->customPlot_new_Pressure_live->yAxis->setRange(
                -pressureRange,pressureRange);

    // X-axis and replot
    for(auto plot : livePlots)
    {
        plot->xAxis->setRange(0, LIVE_WINDOW);
        plot->replot();
    }

    // ============================================================
    // FFT PLOTS
    // ============================================================

    setupFFTPlot(ui->customPlot_adxl_x_FFT, "ADXL X Frequency (Hz)");
    setupFFTPlot(ui->customPlot_adxl_y_FFT, "ADXL Y Frequency (Hz)");
    setupFFTPlot(ui->customPlot_adxl_z_FFT, "ADXL Z Frequency (Hz)");

    ui->customPlot_adxl_x_FFT->addGraph();
    ui->customPlot_adxl_x_FFT->graph(0)->setPen(QPen(tempColor, 1));

    ui->customPlot_adxl_y_FFT->addGraph();
    ui->customPlot_adxl_y_FFT->graph(0)->setPen(QPen(tempColor, 1));

    ui->customPlot_adxl_z_FFT->addGraph();
    ui->customPlot_adxl_z_FFT->graph(0)->setPen(QPen(tempColor, 1));

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

    ui->customPlot_adxl_x_live->addGraph();
    ui->customPlot_adxl_x_live->graph(0)->setPen(QPen(adxlColors[0],1));

    ui->customPlot_adxl_y_live->addGraph();
    ui->customPlot_adxl_y_live->graph(0)->setPen(QPen(adxlColors[1],1));

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

    ui->customPlot_adxl_x2_live->addGraph();
    ui->customPlot_adxl_x2_live->graph(0)->setPen(QPen(adxlColors[0],1));

    ui->customPlot_adxl_y2_live->addGraph();
    ui->customPlot_adxl_y2_live->graph(0)->setPen(QPen(adxlColors[1],1));

    ui->customPlot_adxl_z2_live->addGraph();
    ui->customPlot_adxl_z2_live->graph(0)->setPen(QPen(adxlColors[2],1));

    // ============================================================
    // LIVE TEMPERATURE
    // ============================================================

    setupPlot(ui->customPlot_new_Temp_live,
              "Samples",
              "Temperature (°C)");

    ui->customPlot_new_Temp_live->addGraph();
    ui->customPlot_new_Temp_live->graph(0)->setPen(QPen(tempColor,1));

    // ============================================================
    // LIVE PRESSURE
    // ============================================================

    setupPlot(ui->customPlot_new_Pressure_live,
              "Samples",
              "Pressure");

    ui->customPlot_new_Pressure_live->addGraph();
    ui->customPlot_new_Pressure_live->graph(0)->setPen(QPen(pressureColor,1));
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


        // Extracting frequency for Adxl and Inclinometer
        quint8 highByteAdxl = static_cast<quint8>(Item1[4]);
        quint8 lowByteAdxl  = static_cast<quint8>(Item1[5]);

        quint16 adxlFreq = (highByteAdxl << 8) | lowByteAdxl;
        this->adxlFreq=adxlFreq;

        quint8 highByteInclinometer = static_cast<quint8>(Item1[6]);
        quint8 lowByteInclinometer  = static_cast<quint8>(Item1[7]);

        quint16 InclinometerFreq = (highByteInclinometer << 8) | lowByteInclinometer;
        this->InclinometerFreq=InclinometerFreq;

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
            this->formattedStart=formattedStart;
            this->formattedEnd=formattedEnd;

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
                displayInclinometerfreq=QString::number(1.0/InclinometerFreq)+" s";
            }
            else if(InclinometerFreq > 100 and InclinometerFreq < 1001 )
            {
                displayInclinometerfreq=QString::number((1.0/InclinometerFreq)*1000)+" ms";
            }
            else
            {
                qDebug()<<"Invalid inclinometer frequency";
            }

            setupPlot(ui->customPlot_adxl_x,QString("ADXL X Time(1 = %1)").arg(displayAdxlfreq),"Acceleration(g)",1);
            setupPlot(ui->customPlot_adxl_y,QString("ADXL Y Time(1 = %1)").arg(displayAdxlfreq),"Acceleration(g)",1);
            setupPlot(ui->customPlot_adxl_z,QString("ADXL Z Time(1 = %1)").arg(displayAdxlfreq),"Acceleration(g)",1);


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

void MainWindow::makePacket2048AdxlTempListPressureList(
        QList<QByteArray> &rawPacket2048AdxlList,
        QList<QByteArray> &rawPacketTemperatureList,
        QList<QByteArray> &rawPacketPressureList)
{
    QVector<double> sampleIndex;

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

    int globalSample = 1;

    //----------------------------------------------------
    // ADXL DATA
    //----------------------------------------------------

    for(int p = 0; p < rawPacket2048AdxlList.size(); p++)
    {
        QByteArray packet = rawPacket2048AdxlList[p];

        if(packet.size() < (3 + 2040 + 8 + 3))
        {
            qDebug() << "Skipping short packet:" << packet.size();
            continue;
        }

        // Remove Header
        QByteArray payload = packet.mid(3);

        // Remove Footer
        payload.chop(3);

        // ADXL Region = First 2040 Bytes
        QByteArray adxlBytes = payload.left(2040);

        for(int i = 0; i + 11 < adxlBytes.size(); i += 12)
        {
            quint16 x1 =
                    (static_cast<quint8>(adxlBytes[i+1]) << 8) |
                    static_cast<quint8>(adxlBytes[i]);
            float x1f =  (x1 / 65535.0 ) * 5.12;

            quint16 y1 =
                    (static_cast<quint8>(adxlBytes[i+3]) << 8) |
                    static_cast<quint8>(adxlBytes[i+2]);
            float y1f =  (y1 / 65535.0 ) * 5.12;

            quint16 z1 =
                    (static_cast<quint8>(adxlBytes[i+5]) << 8) |
                    static_cast<quint8>(adxlBytes[i+4]);
            float z1f =  (z1 / 65535.0 ) * 5.12;


            quint16 x2 =
                    (static_cast<quint8>(adxlBytes[i+7]) << 8) |
                    static_cast<quint8>(adxlBytes[i+6]);
            float x2f =  (x2 / 65535.0 ) * 5.12;

            quint16 y2 =
                    (static_cast<quint8>(adxlBytes[i+9]) << 8) |
                    static_cast<quint8>(adxlBytes[i+8]);
            float y2f =  (y2 / 65535.0 ) * 5.12;

            quint16 z2 =
                    (static_cast<quint8>(adxlBytes[i+11]) << 8) |
                    static_cast<quint8>(adxlBytes[i+10]);
            float z2f =  (z2 / 65535.0 ) * 5.12;

            sampleIndex.append(globalSample++);

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

        float tempValue =
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

        float pressureValue =
                bytesToFloatMSB(pressureBytes);

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

bool MainWindow::saveAllSensorDataToExcel(
        const QVector<double> &adxlIndex,
        const QVector<double> &xAdxl,
        const QVector<double> &yAdxl,
        const QVector<double> &zAdxl,
        const QVector<double> &tempIndex,
        const QVector<double> &temperature,
        const QString &fullPath)
{
    QXlsx::Document xlsx;

    constexpr int MAX_ROWS = 1048576;
    constexpr int DATA_START_ROW = 6;

    // =====================================================
    // HEADER FORMATS ONLY
    // =====================================================

    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setHorizontalAlignment(
                QXlsx::Format::AlignHCenter);
    headerFormat.setBorderStyle(
                QXlsx::Format::BorderThin);

    QXlsx::Format titleFormat;
    titleFormat.setFontBold(true);
    titleFormat.setFontSize(16);
    titleFormat.setHorizontalAlignment(
                QXlsx::Format::AlignHCenter);

    int sheetNumber = 1;

    auto setupCommonHeaders =
            [&]()
    {
        xlsx.write("A5",
                   "Samples",
                   headerFormat);

        xlsx.write("B5",
                   "ADXL X (g)",
                   headerFormat);

        xlsx.write("C5",
                   "ADXL Y (g)",
                   headerFormat);

        xlsx.write("D5",
                   "ADXL Z (g)",
                   headerFormat);

        xlsx.write("F5",
                   "Temp Index",
                   headerFormat);

        xlsx.write("G5",
                   "Temperature (°C)",
                   headerFormat);

        xlsx.write("I5",
                   "Incl Index",
                   headerFormat);

        xlsx.write("J5",
                   "Incl X (deg)",
                   headerFormat);

        xlsx.write("K5",
                   "Incl Y (deg)",
                   headerFormat);

        xlsx.setColumnWidth(1,1,12);
        xlsx.setColumnWidth(2,4,16);
        xlsx.setColumnWidth(6,7,16);
        xlsx.setColumnWidth(9,11,16);
    };

    auto createNewSheet =
            [&](bool firstSheet = false)
    {
        QString sheetName =
                QString("Sheet_%1")
                .arg(sheetNumber++);

        if(firstSheet)
        {
            xlsx.renameSheet(
                        "Sheet1",
                        sheetName);
        }
        else
        {
            xlsx.addSheet(
                        sheetName);
        }

        xlsx.selectSheet(
                    sheetName);

        if(firstSheet)
        {
            xlsx.mergeCells(
                        "A1:B1");

            xlsx.write(
                        "A1",
                        "Raw Sensor Data",
                        titleFormat);

            xlsx.write(
                        "A2",
                        "Event ID",
                        headerFormat);

            xlsx.write(
                        "B2",
                        eventId);

            xlsx.write(
                        "D2",
                        "StartTime",
                        headerFormat);

            xlsx.write(
                        "E2",
                        formattedStart);

            xlsx.write(
                        "G2",
                        "EndTime",
                        headerFormat);

            xlsx.write(
                        "H2",
                        formattedEnd);

            xlsx.write(
                        "A3",
                        "ADXL freq",
                        headerFormat);

            xlsx.write(
                        "B3",
                        adxlFreq);

            xlsx.write(
                        "D3",
                        "Inclinometer freq",
                        headerFormat);

            xlsx.write(
                        "E3",
                        InclinometerFreq);
        }

        setupCommonHeaders();
    };

    // =====================================================
    // CREATE FIRST SHEET
    // =====================================================

    createNewSheet(true);

    int row = DATA_START_ROW;

    // Cache sizes once
    const int adxlSize =
            xAdxl.size();

    const int tempSize =
            temperature.size();


    const int maxSize =
            qMax(adxlSize,tempSize);

    // =====================================================
    // WRITE DATA (FAST VERSION)
    // =====================================================

    for(int i = 0;
        i < maxSize;
        ++i)
    {
        if(row > MAX_ROWS)
        {
            createNewSheet(false);
            row = DATA_START_ROW;
        }

        // ---------------- ADXL ----------------
        if(i < adxlSize)
        {
            xlsx.write(
                        row,1,
                        adxlIndex[i]);

            xlsx.write(
                        row,2,
                        xAdxl[i]);

            xlsx.write(
                        row,3,
                        yAdxl[i]);

            xlsx.write(
                        row,4,
                        zAdxl[i]);
        }

        // ---------------- TEMP ----------------
        if(i < tempSize)
        {
            xlsx.write(
                        row,6,
                        tempIndex[i]);

            xlsx.write(
                        row,7,
                        temperature[i]);
        }


    }
    return xlsx.saveAs(fullPath);
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

    //--- Live Data----

    full_xAdxl.clear();
    full_yAdxl.clear();
    full_zAdxl.clear();
    fullInclXL.clear();
    fullInclYL.clear();
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

    ui->textEdit_rawBytes->append(data);
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

                // ---------------------------------------------
                // FF FF FF FF FF FF Special Condition
                // ---------------------------------------------
                if(packet2048.contains(
                            QByteArray::fromHex(
                                "FF FF FF FF FF FF")))
                {
                    QByteArray specialPacket =
                            packet2048;

                    int fIndex =
                            specialPacket.indexOf(
                                QByteArray::fromHex(
                                    "FF FF FF FF FF FF"));

                    qDebug()
                            << "Consecutive FF's detected at packet [ADXL]:"
                            << packet2048AdxlList.size();

                    writeToNotes(
                                "Consecutive FF's detected at packet [ADXL]: "
                                + QString::number(
                                    packet2048AdxlList.size()));

                    qDebug()
                            << "fIndex:"
                            << fIndex;

                    writeToNotes(
                                "fIndex (start of FFs) [ADXL]: "
                                + QString::number(fIndex));

                    int bytesRemoved =
                            (specialPacket.size() - fIndex) - 3;

                    qDebug()
                            << "Removing FF bytes count [ADXL]:"
                            << bytesRemoved;

                    writeToNotes(
                                "Removing FF bytes count [ADXL]: "
                                + QString::number(bytesRemoved));

                    specialPacket.remove(
                                fIndex,
                                bytesRemoved);

                    packet2048AdxlList.append(
                                specialPacket);

                    writeToNotes(
                                "specialPacket [ADXL]: "
                                + specialPacket
                                .toHex(' ')
                                .toUpper());
                }
                else
                {
                    packet2048AdxlList.append(
                                packet2048);
                }

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

            QMessageBox::information(
                        this,
                        "Success",
                        "CSV data saved successfully.");
        });

        // NEW CODE : CSV DUMP ONLY --------------------------------- END

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

        QString defaultName =
                QString("SensorData_%1.xlsx")
                .arg(QDateTime::currentDateTime()
                     .toString("yyyyMMdd_HHmmss"));

        QString desktopPath =
                QStandardPaths::writableLocation(
                    QStandardPaths::DesktopLocation);

        QString fullPath =
                QFileDialog::getSaveFileName(
                    this,
                    "Save Sensor Data",
                    desktopPath + "/" + defaultName,
                    "Excel Files (*.xlsx)");

        if(fullPath.isEmpty())
        {
            return;
        }

        saveAllSensorDataToExcel(
                    finalAdxlIndex, finalXAdxl, finalYAdxl, finalZAdxl,
                    finalTempIndex, finalTemperature,
                    fullPath);

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
    else if(data==QByteArray::fromHex("54 53 41 43 4C")){
        eraseDlg = createPleaseWaitDialog("⏳ Please Wait... !!!");

    }
    else if(data.startsWith(QByteArray::fromHex("54 53 44 4F"))){
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
    else if(data.startsWith("LIVE"))
    {
        // For live plot of 2054 each packet
        QByteArray packet = data.mid(4);      // Remove "LIVE"

        // Packet =
        // CC DD FF
        // 2048 Bytes Payload
        // FF EE FF

        if(packet.size() != 2054)
        {
            qDebug() << "Invalid LIVE Packet Size:"
                     << packet.size();

            return;
        }

        // Remove Header + Footer
        QByteArray payload = packet.mid(3,2048);

        processLivePacket(payload);
    }
    else if(data.startsWith("STOP_LIVE"))
    {

        ui->pushButton_stopLivePlot->setText("Stopped");
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

    dlgPlot = createPleaseWaitDialog("⌛ Please Wait Loading Plot !!!");

    serialObj->writeData(command);

}

void MainWindow::on_pushButton_startLog_clicked()
{
    if(ui->spinBox_logTime->value()==0){
        QMessageBox::warning(this,"Failed","Please set the log time");
        return;
    }


    responseTimer->start(2000); // 2 Sec timer

    QByteArray command;

    initializeSensorVectors();

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

    QString defaultName =
            QString("SensorData_%1.xlsx")
            .arg(QDateTime::currentDateTime()
                 .toString("yyyyMMdd_HHmmss"));

    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString fullPath =
            QFileDialog::getSaveFileName(
                this,
                "Save Sensor Data",
                desktopPath + "/" + defaultName,
                "Excel Files (*.xlsx)");

    if(fullPath.isEmpty())
    {
        return;
    }

    if (!finalAdxlIndex.isEmpty() &&
            !finalXAdxl.isEmpty() &&
            !finalYAdxl.isEmpty() &&
            !finalZAdxl.isEmpty() &&
            !finalTempIndex.isEmpty() &&
            !finalTemperature.isEmpty())
    {
        saveAllSensorDataToExcel(
                    finalAdxlIndex,
                    finalXAdxl,
                    finalYAdxl,
                    finalZAdxl,
                    finalTempIndex,
                    finalTemperature,
                    fullPath);
    }
    else
    {
        QMessageBox::warning(this,
                             "No Data",
                             "Vectors are empty!");
    }
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
    if(ui->spinBox_logTime->value()>65535||ui->spinBox_logTime->value()<1){
        QMessageBox::information(this,"out of Range","Enter the value between 1 and 65535");
        return;
    }
    QByteArray logTime = QByteArray::fromHex("53 54 44");

    quint16 value = ui->spinBox_logTime->value();

    logTime.append(value & 0xFF); //LSB
    logTime.append( (value >> 8) & 0xFF); //MSB

    logTime.append(static_cast<char>(0xFF));

    emit sendMsgId(0x10);
    serialObj->writeData(logTime);
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

    emit sendMsgId(0x10);
    serialObj->writeData(threshold);


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

    emit sendMsgId(0x10);
    serialObj->writeData(packet);



}

void MainWindow::on_pushButton_ADXLfrequency_clicked()
{
    if(ui->spinBox_samplingfrequency->value()>10000 || ui->spinBox_samplingfrequency->value()<1){
        QMessageBox::information(this,"out of Range","Enter the value between 1 and 10000");
        return;
    }

    responseTimer->start(2000);
    QByteArray packet=QByteArray::fromHex("535452");
    quint16 value = ui->spinBox_samplingfrequency->value();
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
        QMessageBox::information(this,"Cancelled","User cancel the erase logs");
    }
}
void MainWindow::on_pushButton_on_clicked()
{
    responseTimer->start(2000);
    QByteArray packet=QByteArray::fromHex("535447");
    emit sendMsgId(0x08);
    serialObj->writeData(packet);


}

//void MainWindow::on_pushButton_off_clicked()
//{
//    responseTimer->start(2000);
//    QByteArray packet=QByteArray::fromHex("535448");
//    emit sendMsgId(0x09);

//    serialObj->writeData(packet);

//}

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

void MainWindow::lpf_secondOrder(double xn,
                                 double yn,
                                 double zn)
{
    // X Axis
    x_y_0 = (b0 * xn) +
            (b1 * x_1) +
            (b2 * x_2) -
            (a1 * x_y_1) -
            (a2 * x_y_2);

    x_2 = x_1;
    x_1 = xn;

    x_y_2 = x_y_1;
    x_y_1 = x_y_0;


    // Y Axis
    y_y_0 = (b0 * yn) +
            (b1 * y_1) +
            (b2 * y_2) -
            (a1 * y_y_1) -
            (a2 * y_y_2);

    y_2 = y_1;
    y_1 = yn;

    y_y_2 = y_y_1;
    y_y_1 = y_y_0;


    // Z Axis
    z_y_0 = (b0 * zn) +
            (b1 * z_1) +
            (b2 * z_2) -
            (a1 * z_y_1) -
            (a2 * z_y_2);

    z_2 = z_1;
    z_1 = zn;

    z_y_2 = z_y_1;
    z_y_1 = z_y_0;
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
            QMessageBox::critical(
                        this,
                        "Error",
                        "Failed to save CSV file.");

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
            << ",Start Time,"
            << formattedStart
            << ",End Time,"
            << formattedEnd
            << "\n";

    out << "\n";

    //----------------------------------------------------
    // Combined Data Header
    //----------------------------------------------------

    out
            << "Sample Number,"
            << "Ax_100(g),"
            << "Ay_100(g),"
            << "Az_100(g),"
            << "Ax_500(g),"
            << "Ay_500(g),"
            << "Az_500(g),"
            << "Temperature,"
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
                << sampleIndex[i] << ","
                << x1Adxl[i] << ","
                << y1Adxl[i] << ","
                << z1Adxl[i] << ","
                << x2Adxl[i] << ","
                << y2Adxl[i] << ","
                << z2Adxl[i] << ",";

        // Temperature Column
        if(i < temperature.size())
        {
            out << temperature[i];
        }

        out << ",";

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
    if(payload.size() != 2048)
        return;

    //-------------------------------------------------------
    // ADXL DATA
    //-------------------------------------------------------

    QByteArray adxlBytes = payload.left(2040);

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
        double y1f = (y1 / 65535.0) * 5.12;
        double z1f = (z1 / 65535.0) * 5.12;

        double x2f = (x2 / 65535.0) * 5.12;
        double y2f = (y2 / 65535.0) * 5.12;
        double z2f = (z2 / 65535.0) * 5.12;

        liveSampleNumber++;

        double timeUS =
                liveSampleNumber *
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
        // Live Plot
        //-------------------------------------------------------

        ui->customPlot_adxl_x_live->graph(0)->addData(liveSampleNumber,x1f);
        ui->customPlot_adxl_y_live->graph(0)->addData(liveSampleNumber,y1f);
        ui->customPlot_adxl_z_live->graph(0)->addData(liveSampleNumber,z1f);

        ui->customPlot_adxl_x2_live->graph(0)->addData(liveSampleNumber,x2f);
        ui->customPlot_adxl_y2_live->graph(0)->addData(liveSampleNumber,y2f);
        ui->customPlot_adxl_z2_live->graph(0)->addData(liveSampleNumber,z2f);
    }

    //-------------------------------------------------------
    // Temperature
    //-------------------------------------------------------

    QByteArray tempBytes = payload.mid(2040,4);

    double temp = bytesToFloatMSB(tempBytes);

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

    livePressureSampleNumber++;

    ui->customPlot_new_Pressure_live
            ->graph(0)
            ->addData(livePressureSampleNumber,pressure);

    //-------------------------------------------------------
    // Store Temp & Pressure for CSV
    //-------------------------------------------------------

    for(int i = 0; i < 170; i++)
    {
        liveCsvData.tempLoaded.append(temp);
        liveCsvData.pressureLoaded.append(pressure);
    }

    //-------------------------------------------------------
    // Remove old samples
    //-------------------------------------------------------

    quint64 lower = 0;

    if(liveSampleNumber > LIVE_WINDOW)
    {
        lower = liveSampleNumber - LIVE_WINDOW;

        ui->customPlot_adxl_x_live->graph(0)->data()->removeBefore(lower);
        ui->customPlot_adxl_y_live->graph(0)->data()->removeBefore(lower);
        ui->customPlot_adxl_z_live->graph(0)->data()->removeBefore(lower);

        ui->customPlot_adxl_x2_live->graph(0)->data()->removeBefore(lower);
        ui->customPlot_adxl_y2_live->graph(0)->data()->removeBefore(lower);
        ui->customPlot_adxl_z2_live->graph(0)->data()->removeBefore(lower);
    }

    if(liveTempSampleNumber > LIVE_WINDOW)
    {
        ui->customPlot_new_Temp_live
                ->graph(0)
                ->data()
                ->removeBefore(
                    liveTempSampleNumber-LIVE_WINDOW);
    }

    if(livePressureSampleNumber > LIVE_WINDOW)
    {
        ui->customPlot_new_Pressure_live
                ->graph(0)
                ->data()
                ->removeBefore(
                    livePressureSampleNumber-LIVE_WINDOW);
    }

    //-------------------------------------------------------
    // Move X Axis
    //-------------------------------------------------------

    for(auto plot : livePlots)
    {
        plot->xAxis->setRange(
                    liveSampleNumber,
                    LIVE_WINDOW,
                    Qt::AlignRight);
    }

    ui->customPlot_new_Temp_live->xAxis->setRange(
                liveTempSampleNumber,
                LIVE_WINDOW,
                Qt::AlignRight);

    ui->customPlot_new_Pressure_live->xAxis->setRange(
                livePressureSampleNumber,
                LIVE_WINDOW,
                Qt::AlignRight);

    //-------------------------------------------------------
    // Refresh
    //-------------------------------------------------------

    if(liveSampleNumber % 1 == 0)
    {
        for(auto plot : livePlots)
        {
            plot->replot(QCustomPlot::rpQueuedReplot);
        }
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

    liveCsvStream.setRealNumberPrecision(3);

    //-------------------------------------------------------
    // Metadata
    //-------------------------------------------------------

    liveCsvStream
            << "Event ID,"
            << eventId
            << ",Start Time,"
            << formattedStart
            << "\n\n";

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
            << "Pressure(mbar),"
            << "Temperature(deg)\n";

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

                << liveCsvData.pressureLoaded[i] << ","

                << liveCsvData.tempLoaded[i]

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
    qDebug()<<"Debug 3";
    removeDC(processed);      //1.Remove mean
    applyHanning(processed);   // 2. apply window

    qDebug()<<"Debug 4";
    QVector<double> mag, freq;

    qDebug()<<"Debug 5";
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


void MainWindow::livePlot(QCustomPlot *plot,
                          const QVector<double> &xValues,
                          const QVector<double> &yValues,
                          int Window,
                          int graphIndex)
{
    if (!plot) return;
    if (xValues.isEmpty() || yValues.isEmpty()) return;
    if (xValues.size() != yValues.size()) return;

    // If graphIndex is out of range → create graph
    if (graphIndex >= plot->graphCount())
        plot->addGraph();

    plot->graph(graphIndex)->data()->clear();
    plot->graph(graphIndex)->addData(xValues, yValues);

    if (Window > 0)
        plot->xAxis->setRange(xValues.last() - Window, xValues.last());

    plot->graph(graphIndex)->rescaleValueAxis(true);
    plot->replot();
}


void MainWindow::on_pushButton_stopLivePlot_clicked()
{

    responseTimer->start(2000);

    finishLiveCsv();

    QByteArray stopPlot = QByteArray::fromHex("535458");
    writeToNotes("stop command send:"+stopPlot.toHex(' ').toUpper());
    emit sendMsgId(0x11);
    serialObj->writeData(stopPlot);

}

void MainWindow::saveLiveData(const QVector<double> &xAdxl,
                              const QVector<double> &yAdxl,
                              const QVector<double> &zAdxl,
                              const QVector<double> &inclX,
                              const QVector<double> &inclY)
{
    // ---------------- SIMPLE FILE DIALOG FIRST ----------------
    QString defaultName = QString("SensorLiveData_%1.xlsx")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    QString fullPath = QFileDialog::getSaveFileName(
                this,
                "Save Live Data",
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

    qDebug()<<"dilaog created";
    // NOW show your "Please Wait" dialog after choosing folder
    QDialog* waitDlg = createPleaseWaitDialog("Data Saving... Please wait");

    qDebug()<<"saving started";
    // ---------------- CREATE XLSX ----------------
    QXlsx::Document xlsx;

    const int MAX_EXCEL_ROWS = 1048576;
    int sheetNumber = 1;

    auto setupSheetHeader = [&](QXlsx::Document &xlsx) {
        qDebug()<<"sheet created";
        QXlsx::Format headerFormat;
        headerFormat.setFontBold(true);
        headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
        headerFormat.setBorderStyle(QXlsx::Format::BorderThin);

        QXlsx::Format headerFormat1;
        headerFormat1.setFontBold(true);
        headerFormat1.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
        headerFormat1.setBorderStyle(QXlsx::Format::BorderThin);
        headerFormat1.setFontSize(16);

        xlsx.mergeCells("A1:B1");
        xlsx.write("A1", "Raw Sensor Data", headerFormat1);
        xlsx.write("A2", "ADXL freq", headerFormat);
        xlsx.write("B2", adxlFreqL);
        xlsx.write("D2", "Inclinometer freq", headerFormat);
        xlsx.write("E2", inclFreqL);

        xlsx.write("A4", "Samples", headerFormat);
        xlsx.write("B4", "ADXL X (g)", headerFormat);
        xlsx.write("C4", "ADXL Y (g)", headerFormat);
        xlsx.write("D4", "ADXL Z (g)", headerFormat);

        xlsx.write("F4", "Incl Index", headerFormat);
        xlsx.write("G4", "Incl X (deg)", headerFormat);
        xlsx.write("H4", "Incl Y (deg)", headerFormat);

        xlsx.setColumnWidth(1, 1, 12);
        xlsx.setColumnWidth(2, 4, 16);
        xlsx.setColumnWidth(6, 8, 16);
    };

    setupSheetHeader(xlsx);

    QXlsx::Format dataFormat;
    dataFormat.setBorderStyle(QXlsx::Format::BorderThin);

    int row = 5;
    int iRow = 5;

    int maxCount = std::max(xAdxl.size(), inclX.size());

    for (int i = 0; i < maxCount; i++)
    {
        if (row > MAX_EXCEL_ROWS - 5)
        {
            sheetNumber++;
            QString sheetName = QString("Sheet%1").arg(sheetNumber);
            xlsx.addSheet(sheetName);
            xlsx.selectSheet(sheetName);

            setupSheetHeader(xlsx);

            row = 5;
            iRow = 5;
        }

        if (i < xAdxl.size()) {
            xlsx.write(row, 1, i+1, dataFormat);
            xlsx.write(row, 2, xAdxl[i], dataFormat);
            xlsx.write(row, 3, yAdxl[i], dataFormat);
            xlsx.write(row, 4, zAdxl[i], dataFormat);
            row++;
        }
        if (i < inclX.size()) {
            xlsx.write(iRow, 6, i+1, dataFormat);
            xlsx.write(iRow, 7, inclX[i], dataFormat);
            xlsx.write(iRow, 8, inclY[i], dataFormat);
            iRow++;
        }
    }

    bool ok = xlsx.saveAs(fullPath);
    if(waitDlg)
    {
        waitDlg->close();
        waitDlg=nullptr;
    }

    if (ok)
        QMessageBox::information(this, "Success", "Saved successfully:\n" + fullPath);
    else
        QMessageBox::critical(this, "Failed", "Unable to save Excel file.");
}


void MainWindow::on_pushButton_startLive_clicked()
{

    responseTimer->start(2000);

    //Initializing X and Y Ranges
    LIVE_WINDOW = ui->lineEdit_windowSize->text().toInt();

    initializeAllPlots();

    clearLiveCsvBuffer();

    liveSampleNumber = 0;
    liveTempSampleNumber = 0;
    livePressureSampleNumber = 0;

    int sampleRate =
            ui->spinBox_samplingfrequency->value();

    liveSamplePeriodUS =
            1000000.0 / sampleRate;

    startLiveCsv();

    QByteArray command;

    command.append(0x53);
    command.append(0x54);
    command.append(0x41);
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

        QMessageBox::information(
                    this,
                    "Success",
                    "CSV loaded successfully.");
    });

    watcher->setFuture(
                QtConcurrent::run(
                    [=]()
    {
        return loadAdxlCsv(
                    filePath);
    }));
}
