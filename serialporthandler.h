#ifndef SERIALPORTHANDLER_H
#define SERIALPORTHANDLER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include <QMutexLocker>
#include <QMutex>

// Forward declaration of MainWindow
class MainWindow;
class serialPortHandler : public QObject
{
    Q_OBJECT
public:
    explicit serialPortHandler(QObject *parent = nullptr);
     ~serialPortHandler();

    void writeData(const QByteArray &data)
    {
        if(!serial->isOpen())
        {
            // Emit a signal to stop the timeout (just like dataReceived() signal)
            emit dataReceived();  // This will stop the timeout, similar to the data receiving case

            qDebug() << "Serial object is not initialized";
            emit portOpening("Serial object is not initialized/port not selected");
            return;
        }
        else
        {
            if(serial->isOpen())
            {
                buffer.clear();
                serial->write(data);
            }
        }
    }

    QStringList availablePorts();

    void setPORTNAME(const QString &portName);

    float convertBytesToFloat(const QByteArray &data);

    quint8 chkSum(const QByteArray &data);

    QString hexBytesSerial(QByteArray &cmd);


signals:

    void portOpening(const QString &); //signal for dumping data from serialPortHandler to textEdit_RawBytes : QString

    void guiDisplay(const QByteArray &byteArrayData); //signal for sending byteArray data to GUI : QByteArray

    void liveData(const QByteArray &byteArrayData);

    void dataReceived();

    void executeWriteToNotes(const QString &dataNotes);

private slots:

    void readData();

public slots:

    void recvMsgId(quint8 id);

private:
    QSerialPort *serial;
    QByteArray  buffer;

    quint8 id;

    //mutex variable
    QMutex bufferMutex; // Mutex for thread-safe access to the buffer
};

#endif // SERIALPORTHANDLER_H
