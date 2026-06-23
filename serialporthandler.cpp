#include "serialporthandler.h"

serialPortHandler::serialPortHandler(QObject *parent) : QObject(parent)
{
    serial = new QSerialPort;
    connect(serial, &QSerialPort::readyRead, this, &serialPortHandler::readData);

}

serialPortHandler::~serialPortHandler()
{
    delete serial;
}

QStringList serialPortHandler::availablePorts()
{
    QStringList ports;
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ports<<info.portName();
    }
    return ports;
}

void serialPortHandler::setPORTNAME(const QString &portName)
{
    buffer.clear();

    if(serial->isOpen())
    {
        serial->close();
    }

    serial->setPortName(portName);
    serial->setBaudRate(921600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);


    if(!serial->open(QIODevice::ReadWrite))
    {
        qDebug()<<"Failed to open port"<<serial->portName();
        emit portOpening("Failed to open port "+serial->portName());
    }
    else
    {
        qDebug() << "Serial port "<<serial->portName()<<" opened successfully at baud rate 921600";
        emit portOpening("Serial port "+serial->portName()+" opened successfully at baud rate 921600");
    }
}

float serialPortHandler::convertBytesToFloat(const QByteArray &data)
{
    if(data.size() != 4)
    {
        qDebug()<<"Insuffient data to convert into float";
    }

    // Assuming little-endian format
    QByteArray floatBytes = data;
    std::reverse(floatBytes.begin(), floatBytes.end()); // Convert to big-endian if needed

    float value;
    memcpy(&value, floatBytes.constData(), sizeof(float));
    return value;
}

quint8 serialPortHandler::chkSum(const QByteArray &data)
{
    // Ensure the QByteArray has at least two bytes (data + checksum)
    if (data.size() < 2) {
        throw std::invalid_argument("Data size must be at least 2 for checksum calculation.");
    }

    // Initialize checksum to 0
    quint8 checksum = 0;

    // Perform XOR for all bytes except the last one
    for (int i = 0; i < data.size() - 1; ++i) {
        checksum ^= static_cast<quint8>(data[i]);
    }

    qDebug()<<hex<<checksum<<"DEBUG_CHKSUM";
    return checksum;
}

QString serialPortHandler::hexBytesSerial(QByteArray &cmd)
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

void serialPortHandler::readData()
{
    qDebug()<<"------------------------------------------------------------------------------------";
    emit portOpening("------------------------------------------------------------------------------------");
    QByteArray ResponseData;
    // Read data from the serial port
    if (serial->bytesAvailable() == 0)
    {
        qWarning() << "No bytes available from serial port";
        return;  // Early return if no data is available
    }

    // Create a QMutexLocker to manage the mutex
    QMutexLocker locker(&bufferMutex); // Lock the mutex


    if (serial->bytesAvailable() < std::numeric_limits<int>::max())
    {
        buffer.append(serial->readAll()); // Append only if it won't exceed max size
        if (!buffer.isEmpty())
        {
            emit dataReceived();
            //executeWriteToNotes("data Received:"+buffer.toHex());
        }
    }
    else
    {
        qWarning() << "Attempt to append too much data to QByteArray!";
        return;
    }


    //Direct taking msgId from mainWindow
    quint8 msgId = id;

    //powerId to avoid that warning QByteRef calling out of bond error
    quint8 powerId = 0x00;



    if(msgId == 0x01)
    {
        qDebug()<<buffer.size()<<" :size";
    }
    else
    {
        qDebug()<<buffer.size()<<" :size";
        qDebug()<<buffer.toHex()<<" Raw buffer data";
    }



    if(msgId == 0x01)
    {
        qDebug() << "msgId:" <<hex<<msgId;

        if(buffer.startsWith(QByteArray::fromHex("AA BB")) && buffer.endsWith(QByteArray::fromHex("AA BB CC DD FF")))
        {
            powerId = 0x01;
            ResponseData = buffer;
            buffer.clear();
            //executeWriteToNotes("Get Event data size: "+QString::number(ResponseData.size()));
            //executeWriteToNotes("Get Event Data cmd received bytes: "+ResponseData.toHex(' ').toUpper());
        }
        else if(buffer == QByteArray::fromHex("53 54 45 FF"))
        {
            powerId = 0x01;
            ResponseData = buffer;
            buffer.clear();
            executeWriteToNotes("Get Event Data cmd received bytes [NACK Condition]: "+ResponseData.toHex(' ').toUpper());
        }
        else
        {
            executeWriteToNotes("Required AA BB as header and AA BB CC DD FF as footer, bytes Received bytes: "+QString::number(buffer.size()));
        }

    }
    else if (msgId == 0x02)
    {
        if (buffer.isEmpty())
            return;

        const QByteArray START_LOG_INIT = QByteArray::fromHex("54 53 41 43 4B");
        const QByteArray START_LOG_END  = QByteArray::fromHex("54 53 50");

        const QByteArray LIVE_HEADER = QByteArray::fromHex("AA BB");
        const QByteArray LIVE_FOOTER = QByteArray::fromHex("FF FF");

        const QByteArray ADXL_HEADER = QByteArray::fromHex("CC DD FF");
        const QByteArray ADXL_FOOTER = QByteArray::fromHex("EE FF");

        const QByteArray INCL_HEADER = QByteArray::fromHex("EE FF FF");
        const QByteArray INCL_FOOTER = QByteArray::fromHex("CC DD");

        // ---------------- START LOG INIT ----------------
        if (buffer.startsWith(START_LOG_INIT))
        {
            ResponseData = START_LOG_INIT;
            buffer.remove(0, START_LOG_INIT.size());
            powerId = 0x02;

            executeWriteToNotes("Start Log Initial cmd received");

        }

        // ---------------- START LOG END ----------------
        else if (buffer.startsWith(START_LOG_END))
        {
            ResponseData = START_LOG_END;
            buffer.remove(0, START_LOG_END.size());
            powerId = 0x02;

            executeWriteToNotes("Start Log End cmd received");

        }

        // ---------------- LIVE FREQ PACKET ----------------
        else if (buffer.startsWith(LIVE_HEADER))
        {
            int footerPos = buffer.indexOf(LIVE_FOOTER, LIVE_HEADER.size());
            if (footerPos < 0) return;   // WAIT FOR FULL PACKET

            int packetSize = footerPos + LIVE_FOOTER.size();
            ResponseData = buffer.left(packetSize);
            buffer.remove(0, packetSize);
            powerId = 0x13;

            executeWriteToNotes("Live Frequency Packet: size = "
                                + QString::number(ResponseData.size()));


        }

        // ---------------- ADXL PACKET ----------------
        else if (buffer.startsWith(ADXL_HEADER))
        {
            int footerPos = buffer.indexOf(ADXL_FOOTER, ADXL_HEADER.size());
            if (footerPos < 0) return;  // WAIT FOR FULL PACKET

            int packetSize = footerPos + ADXL_FOOTER.size();
            ResponseData = buffer.left(packetSize);
            buffer.remove(0, packetSize);
            adxlPackets++;
            powerId = 0x13;

            executeWriteToNotes("ADXL Packet: size = "
                                + QString::number(ResponseData.size()));
//            executeWriteToNotes("ADXL Packet:"
//                                + ResponseData.toHex(' ').toUpper());


        }

        // ---------------- INCL PACKET ----------------
       else if (buffer.startsWith(INCL_HEADER))
        {
            int footerPos = buffer.indexOf(INCL_FOOTER, INCL_HEADER.size());
            if (footerPos < 0) return;  // WAIT FOR FULL PACKET

            int packetSize = footerPos + INCL_FOOTER.size();
            ResponseData = buffer.left(packetSize);
            buffer.remove(0, packetSize);
            inclPackets++;
            powerId = 0x13;

            executeWriteToNotes("Incl Packet: size = "
                                + QString::number(ResponseData.size()));
//            executeWriteToNotes("ADXL Packet:"
//                                + ResponseData.toHex(' ').toUpper());


        }
        else{
            executeWriteToNotes("The Packet:"
                                + buffer.toHex(' ').toUpper());
            executeWriteToNotes("Live Data with Invalid Header");
            buffer.clear();


        }

    }

    else if(msgId == 0x03)
    {
        qDebug() << "" <<hex<<msgId;

        if(buffer.startsWith(QByteArray::fromHex("AA BB")) && buffer.endsWith(QByteArray::fromHex("65 6E 64 FF EF EE")))
        {
            powerId = 0x03;
            ResponseData = buffer;
            buffer.clear();
            executeWriteToNotes("Get Log Events cmd received bytes: "+ResponseData.toHex(' ').toUpper());
        }
        else
        {
            executeWriteToNotes("Required  bytes with header AA BB and footer 65 6E 64 FF EF EE, bytes Received bytes: "+QString::number(buffer.size()));
        }
    }
    else if(msgId == 0x04)
    {
        qDebug() << "" <<hex<<msgId;

        if(buffer == QByteArray::fromHex("53 54 46"))
        {
            powerId = 0x04;
            ResponseData = buffer;
            buffer.clear();
            executeWriteToNotes("Stop Plot cmd received bytes: "+ResponseData.toHex(' ').toUpper());
        }
        else
        {
            executeWriteToNotes("Required 3, bytes Received bytes: "+QString::number(buffer.size()));
        }
    }
    else if(msgId == 0x05){
        qDebug() << "" <<hex<<msgId;

        if(buffer.startsWith(QByteArray::fromHex("53 54 54")))
        {
            powerId = 0x05;
            ResponseData = buffer;
            buffer.clear();
            executeWriteToNotes("Remaining cmd received bytes: "+ResponseData.toHex(' ').toUpper());
        }
        else
        {
            executeWriteToNotes("Required 3, bytes Received bytes: "+QString::number(buffer.size()));
        }

    }
    else if(msgId ==0x06){
          qDebug() << "" <<hex<<msgId;
        if(buffer.startsWith(QByteArray::fromHex("53 54 55"))){
            powerId = 0x06;
            ResponseData = buffer;
            buffer.clear();
            executeWriteToNotes("System on Data cmd received bytes: "+ResponseData.toHex(' ').toUpper());

        }
        else{
            executeWriteToNotes("Required 3, bytes Received bytes: "+QString::number(buffer.size()));
        }

    }
    else if(msgId==0x07){
        qDebug()<<"msg Id:"<<hex<<msgId;
        if(buffer.startsWith(QByteArray::fromHex("54 53 41 43 4C"))){
            powerId=0x07;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("Erase command Received bytes:"+ResponseData.toHex(' ').toUpper());
        }
        else if(buffer==(QByteArray::fromHex("54 53 44 4F 4E 45"))){
            powerId=0x07;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("Erase command Received bytes:"+ResponseData.toHex(' ').toUpper());

        }
    }
    else if(msgId==0x08){
        qDebug()<<"msg Id:"<<hex<<msgId;
        if(buffer.startsWith(QByteArray::fromHex("53 54 47"))){
            powerId=0x08;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("power on command Received bytes:"+ResponseData.toHex(' ').toUpper());
        }

    }
    else if(msgId==0x09){
        qDebug()<<"msg Id:"<<hex<<msgId;
        if(buffer.startsWith(QByteArray::fromHex("53 54 48"))){
            powerId=0x09;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("power off command Received bytes:"+ResponseData.toHex(' ').toUpper());
        }
    }
    else if(msgId==0x10){
        qDebug()<<"msg Id:"<<hex<<msgId;
        if(buffer.startsWith(QByteArray::fromHex("53 54 44"))){
            powerId=0x10;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("log Time Response Received bytes:"+ResponseData.toHex(' ').toUpper());
        }
        else if(buffer.startsWith(QByteArray::fromHex("53 54 51"))){
            powerId=0x10;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("Threshold Response Received bytes:"+ResponseData.toHex(' ').toUpper());
        }
        else if(buffer.startsWith(QByteArray::fromHex("53 54 49"))){
            powerId=0x10;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("Set Time Response Received bytes:"+ResponseData.toHex(' ').toUpper());
        }
        else if(buffer.startsWith(QByteArray::fromHex("53 54 52"))){
            powerId=0x10;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("ADXL Sampling frequency Response Received bytes:"+ResponseData.toHex(' ').toUpper());
        }
        else if(buffer.startsWith(QByteArray::fromHex("53 54 53"))){
            powerId=0x10;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("Inclinometer frequency Response Received bytes:"+ResponseData.toHex(' ').toUpper());
        }
    }
    else if(msgId==0x11){
         qDebug()<<"msg Id:"<<hex<<msgId;
         if(buffer.endsWith(QByteArray::fromHex("54 53 50")))
         {
             powerId=0x11;
             ResponseData=buffer;
             buffer.clear();
             executeWriteToNotes("LivePlot stop Response Received bytes:"+ResponseData.toHex(' ').toUpper());
             executeWriteToNotes("Total AdxlPackets:"+QString::number(adxlPackets));
             executeWriteToNotes("Total InclPackets:"+QString::number(inclPackets));
         }

    }
    else if(msgId==0x12)
        {
        qDebug()<<"1";
            if (buffer.isEmpty())
                return;
            const QByteArray liveCheck=QByteArray::fromHex("53 54 56");
            const QByteArray START_LOG_INIT = QByteArray::fromHex("54 53 41 43 4B");
            const QByteArray START_LOG_END  = QByteArray::fromHex("54 53 50");

            const QByteArray LIVE_HEADER = QByteArray::fromHex("AA BB");
            const QByteArray LIVE_FOOTER = QByteArray::fromHex("FF FF");

            const QByteArray ADXL_HEADER = QByteArray::fromHex("CC DD FF");
            const QByteArray ADXL_FOOTER = QByteArray::fromHex("EE FF");

            const QByteArray INCL_HEADER = QByteArray::fromHex("EE FF FF");
            const QByteArray INCL_FOOTER = QByteArray::fromHex("CC DD");

            // ---------------- START LOG INIT ----------------
            if (buffer.startsWith(liveCheck))
            {
                ResponseData = liveCheck;
                buffer.remove(0, liveCheck.size());

                executeWriteToNotes("Start Log Initial cmd received");

            }

            else if (buffer.startsWith(START_LOG_INIT))
            {
                ResponseData = START_LOG_INIT;
                buffer.remove(0, START_LOG_INIT.size());
                powerId = 0x02;

                executeWriteToNotes("Start Log Initial cmd received");

            }

            // ---------------- START LOG END ----------------
            else if (buffer.startsWith(START_LOG_END))
            {
                ResponseData = START_LOG_END;
                buffer.remove(0, START_LOG_END.size());
                powerId = 0x02;

                executeWriteToNotes("Start Log End cmd received");

            }

            // ---------------- LIVE FREQ PACKET ----------------
            else if (buffer.startsWith(LIVE_HEADER))
            {
                int footerPos = buffer.indexOf(LIVE_FOOTER, LIVE_HEADER.size());
                if (footerPos < 0) return;   // WAIT FOR FULL PACKET

                int packetSize = footerPos + LIVE_FOOTER.size();
                ResponseData = buffer.left(packetSize);
                buffer.remove(0, packetSize);
                powerId = 0x13;

                executeWriteToNotes("Live Frequency Packet: size = "
                                    + QString::number(ResponseData.size()));


            }

            // ---------------- ADXL PACKET ----------------
            else if (buffer.startsWith(ADXL_HEADER))
            {
                int footerPos = buffer.indexOf(ADXL_FOOTER, ADXL_HEADER.size());
                if (footerPos < 0) return;  // WAIT FOR FULL PACKET

                int packetSize = footerPos + ADXL_FOOTER.size();
                ResponseData = buffer.left(packetSize);
                buffer.remove(0, packetSize);
                adxlPackets++;
                powerId = 0x13;

                executeWriteToNotes("ADXL Packet: size = "
                                    + QString::number(ResponseData.size()));


            }

            // ---------------- INCL PACKET ----------------
            else if (buffer.startsWith(INCL_HEADER))
            {
                int footerPos = buffer.indexOf(INCL_FOOTER, INCL_HEADER.size());
                if (footerPos < 0) return;  // WAIT FOR FULL PACKET

                int packetSize = footerPos + INCL_FOOTER.size();
                ResponseData = buffer.left(packetSize);
                buffer.remove(0, packetSize);
                inclPackets++;
                powerId = 0x13;

                executeWriteToNotes("Incl Packet: size = "
                                    + QString::number(ResponseData.size()));


            }
            else{
                executeWriteToNotes("The Packet:"
                                    + buffer.toHex(' ').toUpper());
                executeWriteToNotes("Live Data with Invalid Header");
                buffer.clear();
            }
        }

    else{
        executeWriteToNotes("unknowm msg id");
       }
    executeWriteToNotes("powerId:"+QString::number(powerId));

    switch(powerId)
    {

    case 0x01:
    {
        emit guiDisplay(ResponseData);
    }
        break;

    case 0x02:
    {
        emit guiDisplay(ResponseData);
    }
        break;

    case 0x03:
    {
        emit guiDisplay(ResponseData);
    }
        break;

    case 0x04:
    {
        emit guiDisplay(ResponseData);
    }
        break;
    case 0x05:
    {
        emit guiDisplay(ResponseData);
    }
      break;
    case 0x06:
    {
        emit guiDisplay(ResponseData);
    }
        break;
     case 0x07:
    {
        emit guiDisplay(ResponseData);
     }
        break;
      case 0x08:
    {
        emit guiDisplay(ResponseData);
    }
        break;
    case 0x09:
    {
        emit guiDisplay(ResponseData);
    }
        break;
     case 0x10:
    {
        emit guiDisplay(ResponseData);
    }
        break;
     case 0x13:
    {
        emit liveData(ResponseData);

    }
        break;

    default:
    {
        qDebug() << "Unknown powerId: " <<hex << powerId << " with data: " << ResponseData.size();
    }

    }

}

void serialPortHandler::recvMsgId(quint8 id)
{
    qDebug() << "Received id:" <<hex<< id;
    this->id = id;
    buffer.clear();

}
