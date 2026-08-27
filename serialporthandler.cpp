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
QString serialPortHandler::detectDevicePort()
{
    // Already connected to the device
        if (serial->isOpen())
        {
            return serial->portName();
        }

    foreach (const QSerialPortInfo &info,
             QSerialPortInfo::availablePorts())
    {
        QSerialPort tempSerial;

        tempSerial.setPort(info);

        tempSerial.setBaudRate(921600);
        tempSerial.setDataBits(QSerialPort::Data8);
        tempSerial.setParity(QSerialPort::NoParity);
        tempSerial.setStopBits(QSerialPort::OneStop);
        tempSerial.setFlowControl(QSerialPort::NoFlowControl);

        if (tempSerial.open(QIODevice::ReadWrite))
        {
            qDebug() << "Checking:"
                     << info.portName();

            tempSerial.clear();
            QByteArray packet=QByteArray::fromHex("43 48 45 43 4B");

            tempSerial.write(packet);

            if (tempSerial.waitForBytesWritten(50))
            {
                if (tempSerial.waitForReadyRead(300))
                {
                    QByteArray response =
                            tempSerial.readAll();

                    while (tempSerial.waitForReadyRead(100))
                    {
                        response += tempSerial.readAll();
                    }

                    qDebug() << "Response:"
                             << response;

                    if (response==QByteArray::fromHex("43 48 45 43 4B"))
                    {
                        tempSerial.close();

                        return info.portName();
                    }
                }
            }

            tempSerial.close();
        }
    }

    return "";
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
    if(id == 0x14)
    {
        // do nothing
    }
    else
    {
        qDebug()<<"------------------------------------------------------------------------------------";
    }

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
    else if(msgId == 0x14)
    {
        // do nothing
    }
    else
    {
        qDebug()<<buffer.size()<<" :size";
        qDebug()<<buffer.toHex()<<" Raw buffer data";
    }


    // Handle asynchronous Battery packet (83 84 xx xx xx xx)
    // This packet can arrive anytime independent of msgId.

    if (buffer.contains(QByteArray::fromHex("83 84")))
    {
        while (true)
        {
            int index = buffer.indexOf(QByteArray::fromHex("83 84"));

            if (index == -1)
                break;

            // Wait until complete 6-byte packet arrives
            if (buffer.size() < index + 6)
                break;

            QByteArray batteryRAW = buffer.mid(index, 6);

            emit guiDisplay("BATT" + batteryRAW);

            // Remove only this packet
            buffer.remove(index, 6);
        }
        msgId = 255;
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
        else if(buffer == QByteArray::fromHex(
                    "65 6E 64 FF EF EE"))
        {
            powerId = 0x03;
            emit guiDisplay("NO_EVENTS");
        }
        else
        {
            executeWriteToNotes("Required  bytes with header AA BB and footer 65 6E 64 FF EF EE, bytes Received bytes: "+QString::number(buffer.size()));
        }
    }
    else if(msgId == 0x05)
    {
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
    else if(msgId ==0x06)
    {
        qDebug()<<hex<<msgId;
        if(buffer.startsWith(QByteArray::fromHex("AA BB")) &&
           buffer.endsWith(QByteArray::fromHex("EE FF")))
        {
            powerId = 0x06;
            ResponseData = buffer;
            buffer.clear();
            executeWriteToNotes("Current Parameters cmd received bytes: "+ResponseData.toHex(' ').toUpper());

        }
        else{
            executeWriteToNotes("Required 16, bytes Received bytes: "+QString::number(buffer.size()));
        }

    }
    else if(msgId==0x07)
    {
        qDebug()<<"msg Id:"<<hex<<msgId;
        if(buffer.startsWith(QByteArray::fromHex("54 53 41 43 4C")))
        {
            powerId=0x07;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("Erase command Received bytes:"+ResponseData.toHex(' ').toUpper());
        }
        else if(buffer==(QByteArray::fromHex("54 53 44 4F 4E 45")))
        {
            powerId=0x07;
            ResponseData=buffer;
            buffer.clear();
            executeWriteToNotes("Erase command Received bytes:"+ResponseData.toHex(' ').toUpper());

        }
        else if(buffer == QByteArray::fromHex("4E 4F 5F 65 76 65 6E 74 73"))
        {
            powerId=0x07;
            emit guiDisplay("NO_ERASE");
        }
    }

    else if(msgId==0x10)
    {
        qDebug()<<"msg Id:"<<hex<<msgId;
        if(buffer.startsWith("ACK_1"))
        {
            powerId=0x10;
            ResponseData = buffer;
            emit guiDisplay(ResponseData);
            buffer.clear();
            executeWriteToNotes("Set Paramters Response Received bytes:"+ResponseData.toHex(' ').toUpper());
        }
        else
        {
            executeWriteToNotes("Required 5, bytes Received bytes: "+QString::number(buffer.size()));
        }
    }
    else if(msgId == 0x11)
    {
        qDebug() << "msgId:" << hex << msgId;

        constexpr int LIVE_PACKET_SIZE = 2054;

        while(buffer.size() >= 6)
        {
            //-------------------------------------------------------
            // STOP ACK : AA BB CC DD EE FF
            //-------------------------------------------------------

            if(buffer.size() >= 6 &&
               static_cast<quint8>(buffer[0]) == 0xAA &&
               static_cast<quint8>(buffer[1]) == 0xBB &&
               static_cast<quint8>(buffer[2]) == 0xCC &&
               static_cast<quint8>(buffer[3]) == 0xDD &&
               static_cast<quint8>(buffer[4]) == 0xEE &&
               static_cast<quint8>(buffer[5]) == 0xFF)
            {
                ResponseData = buffer.left(6);

                executeWriteToNotes(
                            "Live Plot STOP ACK received: "
                            + ResponseData.toHex(' ').toUpper());

                emit guiDisplay("STOP_LIVE" + ResponseData);

                buffer.remove(0,6);

                break;
            }

            //-------------------------------------------------------
            // Leftover LIVE Packet
            //-------------------------------------------------------

            else if(buffer.size() >= LIVE_PACKET_SIZE &&
                    static_cast<quint8>(buffer[0]) == 0xCC &&
                    static_cast<quint8>(buffer[1]) == 0xDD &&
                    static_cast<quint8>(buffer[2]) == 0xFF &&
                    static_cast<quint8>(buffer[2051]) == 0xFF &&
                    static_cast<quint8>(buffer[2052]) == 0xEE &&
                    static_cast<quint8>(buffer[2053]) == 0xFF)
            {
                ResponseData = buffer.left(LIVE_PACKET_SIZE);

                emit guiDisplay("LIVE" + ResponseData);

                buffer.remove(0, LIVE_PACKET_SIZE);
            }

            //-------------------------------------------------------
            // Garbage
            //-------------------------------------------------------

            else
            {
                executeWriteToNotes(
                            "Dropped invalid LIVE byte: "
                            + buffer.left(1).toHex());

                buffer.remove(0,1);
            }
        }
    }
    else if(msgId == 0x14)
    {
        // Start Live Plot
        constexpr int HeaderSize  = 3;
        constexpr int PayloadSize = 2048;
        constexpr int FooterSize  = 3;
        constexpr int PacketSize  = HeaderSize + PayloadSize + FooterSize;

        powerId = 0x14;

        while(buffer.size() >= PacketSize)
        {
            // Valid LIVE packet
            if(static_cast<quint8>(buffer[0]) == 0xCC &&
               static_cast<quint8>(buffer[1]) == 0xDD &&
               static_cast<quint8>(buffer[2]) == 0xFF &&
               static_cast<quint8>(buffer[2051]) == 0xFF &&
               static_cast<quint8>(buffer[2052]) == 0xEE &&
               static_cast<quint8>(buffer[2053]) == 0xFF)
            {
                ResponseData = buffer.left(PacketSize);

                emit guiDisplay("LIVE" + ResponseData);

                buffer.remove(0, PacketSize);
            }
            else
            {
                executeWriteToNotes(
                            "Invalid LIVE byte dropped: "
                            + buffer.left(1).toHex());

                buffer.remove(0,1);
            }
        }
    }
    else
    {
        if(msgId == 255)
        {
            // do nothing reserved for battery
        }
        else
        {
            executeWriteToNotes("unknowm msg id");
        }

    }

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

    case 0x05:
    {
        emit guiDisplay(ResponseData);
    }
        break;

    case 0x06:
    {
        emit guiDisplay("PARAM"+ResponseData);
    }
        break;

    case 0x07:
    {
        emit guiDisplay(ResponseData);
    }
        break;

    case 0x10:
    {
        // do nothing
    }
        break;

    case 0x11:
    {
        // do nothing : stop plot is happening
    }
        break;

    case 0x14:
    {
        // do nothing : live plot is happening
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
