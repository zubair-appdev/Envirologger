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

    ui->spinBox_logTime->setToolTip("Enter value from 1 to 10");
    ui->spinBox_threshold->setToolTip("Enter value from -200 to +200");
    ui->spinBox_samplingfrequency->setToolTip("Enter value from 1 to 20000");
    ui->spinBox_Inclinometer->setToolTip("Enter value from 1 to 1000");

    uiUpdateTimer = new QTimer(this);
    uiUpdateTimer->setInterval(uiUpdateIntervalMs);
    //    connect(uiUpdateTimer, &QTimer::timeout, this, &MainWindow::onUiUpdateTimer);
    //    uiUpdateTimer->start();


    saveLimitTimer = new QTimer(this);
    saveLimitTimer->setSingleShot(true);

    connect(saveLimitTimer, &QTimer::timeout, this, [this]() {
        saveLive = false;
        QMessageBox::information(this,"Limitation reached",
                                 "Data saving is stopped due to memory limitation");
    });

    ui->pushButton_startLog->hide();


    livePlotEnabled = ui->checkBox_livePlot->isChecked();
    connect(ui->checkBox_livePlot, &QCheckBox::stateChanged, this, [this](int) {
        livePlotEnabled = ui->checkBox_livePlot->isChecked();
    });

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

    connect(serialObj,&serialPortHandler::liveData,this,&MainWindow::dataProcessing);

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

    // -----------------------------------
    // Skip metadata row
    // -----------------------------------

    if(!in.atEnd())
    {
        in.readLine();
    }

    // -----------------------------------
    // Skip empty row
    // -----------------------------------

    if(!in.atEnd())
    {
        in.readLine();
    }

    // -----------------------------------
    // Skip actual header row
    // -----------------------------------

    if(!in.atEnd())
    {
        in.readLine();
    }

    // -----------------------------------
    // Read actual data
    // -----------------------------------

    while(!in.atEnd())
    {
        QString line =
                in.readLine()
                .trimmed();

        if(line.isEmpty())
        {
            continue;
        }

        QStringList values =
                line.split(",");

        if(values.size() < 4)
        {
            continue;
        }

        bool ok1 = false;
        bool ok2 = false;
        bool ok3 = false;
        bool ok4 = false;

        double sample =
                values[0]
                .toDouble(&ok1);

        double x =
                values[1]
                .toDouble(&ok2);

        double y =
                values[2]
                .toDouble(&ok3);

        double z =
                values[3]
                .toDouble(&ok4);

        // Ignore corrupted rows
        if(!(ok1 &&
             ok2 &&
             ok3 &&
             ok4))
        {
            continue;
        }

        result.sampleIndex
                .append(sample);

        result.xLoaded
                .append(x);

        result.yLoaded
                .append(y);

        result.zLoaded
                .append(z);
    }

    file.close();

    qDebug()
            << "CSV Load Complete:"
            << result.sampleIndex.size()
            << "samples";

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

    setupPlot(ui->customPlot_adxl_x_live,QString("ADXL X"),"Voltage (g)");
    setupPlot(ui->customPlot_adxl_y_live,QString("ADXL Y"),"Voltage (g)");
    setupPlot(ui->customPlot_adxl_z_live,QString("ADXL Z"),"Voltage (g)");

    ui->customPlot_adxl_x_live->addGraph(); ui->customPlot_adxl_x_live->graph(0)->setPen(QPen(adxlColors[0], 1));
    ui->customPlot_adxl_y_live->addGraph(); ui->customPlot_adxl_y_live->graph(0)->setPen(QPen(adxlColors[1], 1));
    ui->customPlot_adxl_z_live->addGraph(); ui->customPlot_adxl_z_live->graph(0)->setPen(QPen(adxlColors[2], 1));

    //  Inclinometer X, Y — degrees vs time (1 unit = 1)
    QString InclinometerFreqLabel = QString("Time (1 = %1)").arg("N/A");


    setupPlot(ui->customPlot_inclinometer_x, QString("Inclinometer Time X %1").arg(InclinometerFreqLabel), "Degrees (°)");
    setupPlot(ui->customPlot_inclinometer_y,  QString("Inclinometer Time Y %1").arg(InclinometerFreqLabel), "Degrees (°)");

    ui->customPlot_inclinometer_x->addGraph(); ui->customPlot_inclinometer_x->graph(0)->setPen(QPen(inclinometerColors[0], 1));
    ui->customPlot_inclinometer_y->addGraph(); ui->customPlot_inclinometer_y->graph(0)->setPen(QPen(inclinometerColors[1], 1));

    setupPlot(ui->customPlot_incl_x_live,QString("Inclinometer"),"Degrees(°)");

    ui->customPlot_incl_x_live->addGraph();
    ui->customPlot_incl_x_live->graph(0)->setPen(QPen(inclinometerColors[0], 1));
    ui->customPlot_incl_x_live->graph(0)->setName("Inclinometer X");

    ui->customPlot_incl_x_live->addGraph();
    ui->customPlot_incl_x_live->graph(1)->setPen(QPen(tempColor, 1));
    ui->customPlot_incl_x_live->graph(1)->setName("Inclinometer Y");

    ui->customPlot_incl_x_live->legend->setVisible(true);
    ui->customPlot_incl_x_live->legend->setIconSize(8, 8);

    QFont legendFont = ui->customPlot_incl_x_live->legend->font();
    legendFont.setPointSize(8);
    ui->customPlot_incl_x_live->legend->setFont(legendFont);



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

    x_0 = x_1 = x_2 = 0.0;
    x_y_0 = x_y_1 = x_y_2 = 0.0;

    y_0 = y_1 = y_2 = 0.0;
    y_y_0 = y_y_1 = y_y_2 = 0.0;

    z_0 = z_1 = z_2 = 0.0;
    z_y_0 = z_y_1 = z_y_2 = 0.0;

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

    for(int g=0;g<xAdxl.size();g++)
    {
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
    auto plotGraph = [](QCustomPlot *plot,
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

        // Clear old graph data
        plot->graph(0)->data()->clear();

        constexpr int CHUNK_SIZE = 5000;

        // Add data in chunks
        for(int i = 0; i < x.size(); i += CHUNK_SIZE)
        {
            int count =
                qMin(CHUNK_SIZE,
                     x.size() - i);

            // Add partial chunk
            plot->graph(0)->addData(
                x.mid(i, count),
                y.mid(i, count));

            // Give UI breathing space
            if(i > 0 && i % 10000 == 0)
            {
                QCoreApplication::processEvents();
            }
        }

        // Set axis range
        plot->xAxis->setRange(
            x.first(),
            x.last());

        plot->graph(0)->rescaleValueAxis();

        plot->setUpdatesEnabled(true);

        // Single replot at end
        plot->replot(
            QCustomPlot::rpQueuedReplot);
    };


    // --- Apply LPF on all samples ---
    xFiltered.clear();
    yFiltered.clear();
    zFiltered.clear();

    for(int i = 0; i < xAdxl.size(); i++)
    {
        lpf_secondOrder(xAdxl[i],
                        yAdxl[i],
                        zAdxl[i]);

        // Save filtered outputs
        xFiltered.append(x_y_0);
        yFiltered.append(y_y_0);
        zFiltered.append(z_y_0);

        // Give UI breathing space every 5000 samples
        if(i % 5000 == 0)
        {
            QCoreApplication::processEvents();
        }
    }

    // ------------------------------------
    // Choose RAW or LPF data
    // ------------------------------------

    QVector<double> *xToPlot = &xAdxl;
    QVector<double> *yToPlot = &yAdxl;
    QVector<double> *zToPlot = &zAdxl;

    if(ui->checkBox_lowPass->isChecked())
    {
        xToPlot = &xFiltered;
        yToPlot = &yFiltered;
        zToPlot = &zFiltered;
    }


    // --- Plot ADXL ---
    plotGraph(ui->customPlot_adxl_x,
              sampleIndex,
              *xToPlot);

    QCoreApplication::processEvents();

    plotGraph(ui->customPlot_adxl_y,
              sampleIndex,
              *yToPlot);

    QCoreApplication::processEvents();

    plotGraph(ui->customPlot_adxl_z,
              sampleIndex,
              *zToPlot);


    // --- Plot Temperature ---
    plotGraph(ui->customPlot_temperature,
              tempIndex,
              temperatureValues);


    // --- Save global values ---
    this->finalAdxlIndex = sampleIndex;
    this->finalXAdxl = *xToPlot;
    this->finalYAdxl = *yToPlot;
    this->finalZAdxl = *zToPlot;

    this->finalTempIndex = tempIndex;
    this->finalTemperature = temperatureValues;

    // --- FFT Plot ---
    double Fs = adxlFreq;
    qDebug() << "Debug 1";

    try {
        computeAndPlotFFT(*xToPlot, Fs, ui->customPlot_adxl_x_FFT);
    }
    catch (std::exception &ex) {
        qCritical() << "computeAndPlotFFT exception:" << ex.what();
    }
    catch (...) {
        qCritical() << "computeAndPlotFFT unknown crash";
    }

    qDebug() << "Debug 2";


    computeAndPlotFFT(*yToPlot, Fs, ui->customPlot_adxl_y_FFT);
    computeAndPlotFFT(*zToPlot, Fs, ui->customPlot_adxl_z_FFT);
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
            plot->replot(QCustomPlot::rpQueuedReplot);
        }
    };

    plotGraph(ui->customPlot_inclinometer_x, sampleIndex, inclX);
    plotGraph(ui->customPlot_inclinometer_y, sampleIndex, inclY);

    // --- Passing local values to global values
    this->finalInclIndex = sampleIndex;
    this->finalInclX = inclX;
    this->finalInclY = inclY;

}

bool MainWindow::saveAllSensorDataToExcel(
        const QVector<double> &adxlIndex,
        const QVector<double> &xAdxl,
        const QVector<double> &yAdxl,
        const QVector<double> &zAdxl,
        const QVector<double> &tempIndex,
        const QVector<double> &temperature,
        const QVector<double> &inclIndex,
        const QVector<double> &inclX,
        const QVector<double> &inclY,
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

    const int inclSize =
            inclX.size();

    const int maxSize =
            qMax(qMax(adxlSize,
                      tempSize),
                 inclSize);

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

        // ------------- INCL ------------------
        if(i < inclSize)
        {
            xlsx.write(
                        row,9,
                        inclIndex[i]);

            xlsx.write(
                        row,10,
                        inclX[i]);

            xlsx.write(
                        row,11,
                        inclY[i]);
        }

        ++row;
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

    // --- Inclinometer ---
    finalInclIndex.clear();
    finalInclX.clear();
    finalInclY.clear();

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

        // NEW CODE : 18May2026 --------------------------------- BEGIN

        // Save ADXL CSV
        startAdxlCsvSaving(
                    [=]()
        {
            // This runs ONLY after CSV completes

            blinkLabel(ui->label_csv,300,"CSV OFF");

            if(dlgPlot)
            {
                dlgPlot->close();
                dlgPlot = nullptr;
            }

            // ---------------------------------
            // Excel code continues here
            // ---------------------------------

            QString defaultName =
                    QString("SensorData_%1")
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

            QDialog *excelSavingDialog =
                    createPleaseWaitDialog(
                        "⏳ Please Wait, Data Saving ...");

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

                if(excelSavingDialog)
                {
                    excelSavingDialog->close();
                    excelSavingDialog->deleteLater();
                }

                watcher->deleteLater();

                if(ok)
                {
                    QMessageBox::information(
                                this,
                                "Success",
                                "Excel data saved successfully.");
                }
                else
                {
                    QMessageBox::critical(
                                this,
                                "Error",
                                "Failed to save excel file.");
                }
            });

            watcher->setFuture(
                        QtConcurrent::run(
                            [=]()
            {
                return saveAllSensorDataToExcel(
                            finalAdxlIndex,
                            finalXAdxl,
                            finalYAdxl,
                            finalZAdxl,
                            finalTempIndex,
                            finalTemperature,
                            finalInclIndex,
                            finalInclX,
                            finalInclY,
                            fullPath);
            }));
        });

        // NEW CODE : 18May2026 --------------------------------- END


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
        if(!ui->checkBox_livePlot->isChecked())
        {
            dlg = createPleaseWaitDialog("⏳ Please Wait Data Logging ...",ui->spinBox_logTime->value());
        }
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
                    finalInclIndex, finalInclX, finalInclY,
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
        !finalTemperature.isEmpty() &&
        !finalInclIndex.isEmpty() &&
        !finalInclX.isEmpty() &&
        !finalInclY.isEmpty())
    {
        saveAllSensorDataToExcel(
                    finalAdxlIndex,
                    finalXAdxl,
                    finalYAdxl,
                    finalZAdxl,
                    finalTempIndex,
                    finalTemperature,
                    finalInclIndex,
                    finalInclX,
                    finalInclY,
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
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm", "Do you want to Erase logs?",
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
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

QString MainWindow::createAdxlCsvPath()
{
    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString folderPath =
            desktopPath
            + "/ADXL_CSV";

    QDir dir;

    // Create folder if missing
    if(!dir.exists(folderPath))
    {
        dir.mkpath(folderPath);

        qDebug()
                << "Created folder:"
                << folderPath;
    }

    QString timestamp =
            QDateTime::currentDateTime()
            .toString(
                "yyyyMMdd_HHmmss");

    return folderPath
            + "/ADXL_Data_"
            + timestamp
            + ".csv";
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

        // Continue flow
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
                    finalAdxlIndex,
                    finalXAdxl,
                    finalYAdxl,
                    finalZAdxl,
                    csvPath);
    }));
}

bool MainWindow::saveAdxlToCsv(
        const QVector<double> &sampleIndex,
        const QVector<double> &xAdxl,
        const QVector<double> &yAdxl,
        const QVector<double> &zAdxl,
        const QString &filePath)
{
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

    // -----------------------------------
    // Row 1 : Metadata
    // -----------------------------------

    out
    << "Event ID,"
    << eventId
    << ",Start Time,"
    << formattedStart
    << ",End Time,"
    << formattedEnd
    << "\n";

    // Empty row for readability
    out << "\n";

    // -----------------------------------
    // Row 3 : Header
    // -----------------------------------

    out
    << "Sample Number,"
       "ADXL X (g),"
       "ADXL Y (g),"
       "ADXL Z (g)\n";

    const int size =
            xAdxl.size();

    // -----------------------------------
    // Data
    // -----------------------------------

    for(int i = 0;
        i < size;
        ++i)
    {
        out
        << sampleIndex[i] << ","
        << xAdxl[i] << ","
        << yAdxl[i] << ","
        << zAdxl[i]
        << "\n";

        // Flush periodically
        if(i > 0 &&
           i % 100000 == 0)
        {
            out.flush();

            ui->label_csv->setText(
                        QString(
                            "Rows : %1")
                        .arg(i));
        }
    }

    out.flush();
    file.flush();
    file.close();

    qDebug()
            << "CSV save completed:"
            << filePath;

    return true;
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

void MainWindow::dataProcessing(const QByteArray &byteArrayData)
{
    ui->tabWidget->tabBar()->setEnabled(false);


    QByteArray data=byteArrayData;

    int invalidHeaderCount=0;
    QByteArray packet4100Adxl;
    QByteArray packet4100Incl;
    QByteArray temperaturePacket;

    if(data.startsWith(QByteArray::fromHex("AA BB")) and data.endsWith(QByteArray::fromHex("FF FF")))
    {
        quint8 adxlOne=static_cast<quint8>(data[2]);
        quint8 adxlTwo=static_cast<quint8>(data[3]);
        quint16 adxlFreqL=adxlOne<<8|adxlTwo;
        this->adxlFreqL=adxlFreqL;

        QString displayAdxlfreq;
        QString displayInclinometerfreq;

        if(adxlFreqL < 101)
        {
            displayAdxlfreq=QString::number(1.0/adxlFreqL)+" s";
        }
        else if(adxlFreqL > 100 and adxlFreqL < 5001 )
        {
            displayAdxlfreq=QString::number((1.0/adxlFreqL)*1000)+" ms";
        }
        else if(adxlFreqL > 5000 and adxlFreqL < 20001)
        {
            displayAdxlfreq=QString::number((1.0/adxlFreqL)*1000000)+" µs";
        }
        else
        {
            qDebug()<<"Invalid Adxl frequency";
        }


        setupPlot(ui->customPlot_adxl_x_live,QString("ADXL X Time(1 = %1)").arg(displayAdxlfreq),"Acceleration(g)",1);
        setupPlot(ui->customPlot_adxl_y_live,QString("ADXL Y Time(1 = %1)").arg(displayAdxlfreq),"Acceleration(g)",1);
        setupPlot(ui->customPlot_adxl_z_live,QString("ADXL Z Time(1 = %1)").arg(displayAdxlfreq),"Acceleration(g)",1);


        quint8 inclOne=static_cast<quint8>(data[4]);
        quint8 inclTwo=static_cast<quint8>(data[5]);
        quint16 inclFreqL=inclOne<<8|inclTwo;
        this->inclFreqL=inclFreqL;

        if(inclFreqL < 101)
        {
            displayInclinometerfreq=QString::number(1.0/inclFreqL)+" s";
        }
        else if(inclFreqL > 100 and inclFreqL < 1001 )
        {
            displayInclinometerfreq=QString::number((1.0/inclFreqL)*1000)+" ms";
        }
        else
        {
            qDebug()<<"Invalid inclinometer frequency";
        }
        setupPlot(ui->customPlot_incl_x_live,QString("Inclinometer X&Y Time(1 = %1)").arg(displayInclinometerfreq),"Degrees(°)",1);

    }

    else if(data.startsWith(QByteArray::fromHex("CC DD FF"))&&data.endsWith(QByteArray::fromHex("EE FF")))
    {
        QByteArray data=byteArrayData;
        if (4160 <= data.size())
        {
            QByteArray packet4100 = data;
            if (packet4100.endsWith(QByteArray::fromHex("EE FF")))
            {
                packet4100.remove(packet4100.size()-62,60);
                if(packet4100.contains(QByteArray::fromHex("FF FF FF FF FF FF")))
                {
                    // Special condition FF's checking
                    QByteArray specialPacket = packet4100;

                    qDebug()<<"Consecutive FF's detected at packet [ADXL]: "+QString::number(packet4100Adxl.size());
                    writeToNotes("Consecutive FF's detected at packet [ADXL]: " + QString::number(packet4100Adxl.size()));

                    int fIndex = specialPacket.indexOf(QByteArray::fromHex("FF FF FF FF FF FF"));
                    qDebug()<<fIndex<<" :fIndex";

                    qDebug()<< "Removing ff bytes count [ADXL]: " << (specialPacket.size() - fIndex) - 5;
                    writeToNotes("Removing ff bytes count [ADXL]: " + QString::number((specialPacket.size() - fIndex) - 5));

                    specialPacket.remove(fIndex,(specialPacket.size() - fIndex) - 5);


                    packet4100Adxl.append(specialPacket);
                    qDebug()<<specialPacket.toHex(' ').toUpper()<<" :specialPacket";

                    // writeToNotes Log
                    writeToNotes("fIndex (start of FFs) [ADXL]: " + QString::number(fIndex));
                    writeToNotes("specialPacket [ADXL]: " + specialPacket.toHex(' ').toUpper());
                }
                else
                {
                    // Normal condition
                    packet4100Adxl.append(packet4100);
                }

                // Extract last 2 bytes before footer as temperature
                QByteArray tempBytes = packet4100Adxl.mid(packet4100Adxl.size() - 5, 2);
                temperaturePacket.append(tempBytes);

                quint8 templsb=static_cast<quint8>(temperaturePacket[0]);
                quint8 tempmsb=static_cast<quint8>(temperaturePacket[1]);
                quint16 temp=templsb<<8|tempmsb;
                temp &= ~0x0003;
                double finalTemp=(-46.85 + (175.72 * temp) / 65536.0);
                ui->lineEdit_temperature->setText(QString::number(finalTemp));
            }
            else
            {
                invalidHeaderCount++;
            }
        }

        makePacket4100AdxlLive(packet4100Adxl);

    }
    else if(data.startsWith(QByteArray::fromHex("EE FF FF")))
    {
        QByteArray packet4100 = data;
        if (packet4100.endsWith(QByteArray::fromHex("CC DD")))
        {
            packet4100.remove(packet4100.size()-62,60);

            if(packet4100.contains(QByteArray::fromHex("FF FF FF FF FF FF")))
            {
                // Special condition FF's checking
                QByteArray specialPacket = packet4100;

                qDebug()<<"Consecutive FF's detected at packet [INCLINOMETER]: "+QString::number(packet4100Incl.size());
                writeToNotes("Consecutive FF's detected at packet [INCLINOMETER]: " + QString::number(packet4100Incl.size()));


                int fIndex = specialPacket.indexOf(QByteArray::fromHex("FF FF FF FF FF FF"));
                qDebug()<<fIndex<<" :fIndex";

                qDebug()<< "Removing ff bytes count [INCLINOMETER]: " << (specialPacket.size() - fIndex) - 5;
                writeToNotes("Removing ff bytes count [INCLINOMETER]: " + QString::number((specialPacket.size() - fIndex) - 5));

                specialPacket.remove(fIndex,(specialPacket.size() - fIndex) - 5);


                packet4100Incl.append(specialPacket);
                qDebug()<<specialPacket.toHex(' ').toUpper()<<" :specialPacket";

                // writeToNotes Log
                writeToNotes("fIndex (start of FFs) [INCLINOMETER]: " + QString::number(fIndex));
                writeToNotes("specialPacket [INCLINOMETER]: " + specialPacket.toHex(' ').toUpper());
            }
            else
            {
                // Normal condition
                packet4100Incl.append(packet4100);
            }
        }
        else
        {
            invalidHeaderCount++;
        }
        makePacket4100InclLive(packet4100Incl);
    }
    else if(data==QByteArray::fromHex("53 54 50"))
    {
        qDebug()<<"stop command Received";
    }
    else{
        qDebug()<<"unknown data Received";
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

void MainWindow::makePacket4100AdxlLive(const QByteArray &rawPacket4100Adxl)
{
    QVector<double> sampleIndex;
    QVector<double> xAdxl, yAdxl, zAdxl;
    int globalSample = 0;
    qDebug()<<"Extracting bytes";

    // --- ADXL Data Processing ---

    QByteArray packet = rawPacket4100Adxl;

    if (packet.size() < 20)
    {
        qDebug() << "Skipping too short ADXL packet:" << packet.size();
        return;
    }

    QByteArray trimmed = packet.mid(3);
    if (trimmed.size() > 3) trimmed.chop(3); // remove footer
    if (trimmed.size() > 2) trimmed.chop(2); // remove temperature bytes

    int usableSize = trimmed.size();
    if (usableSize < 6)
    {
        qDebug() << "Packet too short after trimming:" << usableSize;
        return;
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
    for(int g=0;g<xAdxl.size();g++){
        xAdxl[g]=(xAdxl[g]-1.65)/0.0063;
        yAdxl[g]=(yAdxl[g]-1.65)/0.0063;
        zAdxl[g]=(zAdxl[g]-1.65)/0.0063;

    }
    if (adxlWindow < 0) {
        adxlWindow = sampleIndex.size();
        qDebug() << "Fixed X-axis window set =" << adxlWindow;
    }

    //    quint64 memMB = getCurrentProcessMemoryMB();
    //    qDebug()<<memMB<<"memory used";

    //        if (memMB > 4000)
    //        {
    //            saveLive=false;
    //            QMessageBox::warning(this,
    //                                 "Memory Warning",
    //                                 "Data saving is stopped due to memory limitation.");

    //        }

    //        QMutexLocker locker(&dataMutex);
    //        pending_sampleIndex += sampleIndex;
    //        pending_xAdxl += xAdxl;
    //        pending_yAdxl += yAdxl;
    //        pending_zAdxl += zAdxl;

    // optionally keep full history for later export
    if(saveLive){
        full_xAdxl += xAdxl;
        full_yAdxl += yAdxl;
        full_zAdxl += zAdxl;
    }

    if (!ui->checkBox_fft->isChecked())
    {
        // time-domain
        livePlot(ui->customPlot_adxl_x_live, sampleIndex, xAdxl,adxlWindow,0);
        livePlot(ui->customPlot_adxl_y_live, sampleIndex, yAdxl,adxlWindow,0);
        livePlot(ui->customPlot_adxl_z_live, sampleIndex, zAdxl,adxlWindow,0);
    }
    else
    {
        plotLiveFFT(xAdxl, adxlFreqL, ui->customPlot_adxl_x_live);
        plotLiveFFT(yAdxl, adxlFreqL, ui->customPlot_adxl_y_live);
        plotLiveFFT(zAdxl,adxlFreqL, ui->customPlot_adxl_z_live);
    }

    qDebug() << "Total ADXL samples:" << sampleIndex.size();
}
void MainWindow::makePacket4100InclLive(const QByteArray &rawPacket4100Incl)
{
    QByteArray packet = rawPacket4100Incl;
    QVector<double> inclXL, inclYL;
    QVector<double> sampleIndex;
    int index=0;

    if (packet.size() < 20)
    {
        qDebug() << "Skipping too short inclinometer packet:" << packet.size();
        return;
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
        return;
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

        sampleIndex.append(index++);
        inclXL.append(xDeg);
        inclYL.append(yDeg);


        if(inclWindow<0)
        {
            inclWindow=sampleIndex.size();
        }
    }
    if(saveLive)
    {
        fullInclXL+=inclXL;
        fullInclYL+=inclYL;
    }

    livePlot(ui->customPlot_incl_x_live, sampleIndex, inclXL,inclWindow,0);
    livePlot(ui->customPlot_incl_x_live, sampleIndex, inclYL,inclWindow,1);
}
//void MainWindow::onUiUpdateTimer()
//{
//    if (!livePlotEnabled) return; // respect live toggle; skip plotting

//    // Grab pending data atomically
//    QVector<double> sIdx, x, y, z;
//    {
//        QMutexLocker locker(&dataMutex);
//        if (pending_sampleIndex.isEmpty()) return;
//        sIdx = pending_sampleIndex; pending_sampleIndex.clear();
//        x = pending_xAdxl;
//        pending_xAdxl.clear();
//        y = pending_yAdxl;
//        pending_yAdxl.clear();
//        z = pending_zAdxl;
//        pending_zAdxl.clear();

//    }

// Now update plots on GUI thread (one batch per timer tick)

//}
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

void MainWindow::plotLiveFFT(const QVector<double>& signal,
                             double Fs,
                             QCustomPlot *plot)
{
    double maxPeak=0.0;
    if (signal.isEmpty() || plot == nullptr)
        return;
    if (plot == ui->customPlot_adxl_x_live)
    {
        maxPeak=maxPeak_x;
    }
    else if (plot == ui->customPlot_adxl_y_live)
    {
        maxPeak=maxPeak_y;

    }
    else if(plot==ui->customPlot_adxl_z_live)
    {
        maxPeak=maxPeak_z;

    }
    else
    {
        qDebug()<<"invalid Plot";
    }

    QVector<double> processed = signal;

    applyHanning(processed);

    QVector<double> magnitude, freqAxis;

    performFFT(processed, magnitude, freqAxis, Fs);

    for (int i = 0; i < magnitude.size(); i++)
    {
        if (magnitude[i] > maxPeak)
        {
            maxPeak = magnitude[i];

        }
    }



    plot->graph(0)->setData(freqAxis, magnitude);

    if (plot == ui->customPlot_adxl_x_live)
    {
        ui->lineEdit_fftPeak_x->setText(QString::number(maxPeak));
        maxPeak_x=maxPeak;
    }
    else if (plot == ui->customPlot_adxl_y_live)
    {
        ui->lineEdit_fftPeak_y->setText(QString::number(maxPeak));
        maxPeak_y=maxPeak;

    }
    else if(plot==ui->customPlot_adxl_z_live)
    {
        ui->lineEdit_fftPeak_z->setText(QString::number(maxPeak));
        maxPeak_z=maxPeak;

    }
    else
    {
        qDebug()<<"invalid Plot";
    }
    plot->xAxis->setRange(0, Fs/2);
    plot->graph(0)->rescaleValueAxis();
    plot->replot();
}


void MainWindow::on_checkBox_fft_stateChanged(int)
{
    // Temporarily disable live plotting to avoid races while switching labels
    bool prevLive = livePlotEnabled;
    livePlotEnabled = false;

    if (ui->checkBox_fft->isChecked())
    {
        setupPlot(ui->customPlot_adxl_x_live, "ADXL X Frequency (Hz)", "Amplitude (g)", true);
        setupPlot(ui->customPlot_adxl_y_live, "ADXL Y Frequency (Hz)", "Amplitude (g)", true);
        setupPlot(ui->customPlot_adxl_z_live, "ADXL Z Frequency (Hz)", "Amplitude (g)", true);
    }
    else
    {
        setupPlot(ui->customPlot_adxl_x_live, "ADXL X", "Voltage (g)", true);
        setupPlot(ui->customPlot_adxl_y_live, "ADXL Y", "Voltage (g)", true);
        setupPlot(ui->customPlot_adxl_z_live, "ADXL Z", "Voltage (g)", true);
    }

    // restore live plotting only if it was on and the checkbox for live plot is checked
    livePlotEnabled = prevLive && ui->checkBox_livePlot->isChecked();
}


void MainWindow::on_checkBox_livePlot_stateChanged(int arg1)
{
    Q_UNUSED(arg1);
    if (!ui->checkBox_livePlot->isChecked())
    {
        QByteArray livePlotUnCheck=QByteArray::fromHex("53 54 57");
        serialObj->writeData(livePlotUnCheck);
        writeToNotes("live checkbox unchecked");
        QMessageBox::information(this,"Live mode off","Live mode is stopped");
        ui->tabWidget->tabBar()->setEnabled(true);
    }
    else
    {
        QByteArray livePlotCheck=QByteArray::fromHex("53 54 56");
        serialObj->writeData(livePlotCheck);
        writeToNotes("live checkbox checked");
        QMessageBox::information(this,"Live Mode","Live mode is on");
        emit sendMsgId(0x12);
    }
}
void MainWindow::on_pushButton_stopLivePlot_clicked()
{

    ui->tabWidget->tabBar()->setEnabled(true);
    responseTimer->start(2000);

    QByteArray stopPlot = QByteArray::fromHex("535458");
    writeToNotes("stop command send:"+stopPlot.toHex(' ').toUpper());
    emit sendMsgId(0x11);
    serialObj->writeData(stopPlot);

    if (saveLimitTimer->isActive()) {
        saveLimitTimer->stop();
        qDebug() << "Timer stopped.";
    }

    saveLive=false;

    QTimer::singleShot(50, this, [this]() {
        if (!full_xAdxl.isEmpty() &&
                !full_yAdxl.isEmpty() &&
                !full_zAdxl.isEmpty() &&
                !fullInclXL.isEmpty() &&
                !fullInclYL.isEmpty())
        {
            saveLiveData(full_xAdxl, full_yAdxl, full_zAdxl,
                         fullInclXL, fullInclYL);
        }
        else
        {
            QMessageBox::warning(this, "No Data", "No data to save");
        }
    });
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


void MainWindow::on_pushButton_saveLive_clicked()
{

    saveLive = true;
    QMessageBox::information(this,"Save Started","Data saving is started");
    if (!saveLimitTimer->isActive()) {
        saveLimitTimer->start(480000);
        qDebug() << "Timer started.";
    } else {
        qDebug()<< "Timer already running. Not restarting.";
    }


}
void MainWindow::on_pushButton_startLive_clicked()
{
    maxPeak_x = 0.0;
    maxPeak_y = 0.0;
    maxPeak_z = 0.0;

    responseTimer->start(2000);

    QByteArray command;
    initializeSensorVectors();

    command.append(0x53);
    command.append(0x54);
    command.append(0x42);

    qDebug() << "Start Log cmd sent : " + hexBytes(command);
    writeToNotes("Start Log cmd sent in livePlot: " + hexBytes(command));
    emit sendMsgId(0x02);
    serialObj->writeData(command);
}

void MainWindow::on_pushButton_fitToScreenLive_clicked()
{
    QList<QCustomPlot*> allPlots = {
        ui->customPlot_adxl_x_live,
        ui->customPlot_adxl_y_live,
        ui->customPlot_adxl_z_live,
        ui->customPlot_incl_x_live
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

    // -------------------------------
    // Loading dialog
    // -------------------------------

    QDialog *loadingDialog =
            createPleaseWaitDialog(
                "⏳ Loading CSV File...");

    QFutureWatcher<CsvPlotData>
            *watcher =
            new QFutureWatcher<
            CsvPlotData>(this);

    connect(watcher,
            &QFutureWatcher<
            CsvPlotData>::finished,
            this,
            [=]()
    {
        CsvPlotData data =
                watcher->result();

        watcher->deleteLater();

        // -------------------------------
        // Plot helper
        // -------------------------------

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
                        QCustomPlot::
                        rpQueuedReplot);
        };

        // -------------------------------
        // Plot graphs
        // -------------------------------

        plotGraph(
                    ui->customPlot_adxl_x,
                    data.sampleIndex,
                    data.xLoaded);

        plotGraph(
                    ui->customPlot_adxl_y,
                    data.sampleIndex,
                    data.yLoaded);

        plotGraph(
                    ui->customPlot_adxl_z,
                    data.sampleIndex,
                    data.zLoaded);

        if(loadingDialog)
        {
            loadingDialog->close();
            loadingDialog
                    ->deleteLater();
        }

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
