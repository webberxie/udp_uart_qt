#ifndef WIDGET_H
#define WIDGET_H
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QUdpSocket>
#include <QWidget>
#include <QTimer>
#include<QBuffer>
#include<QFile>

typedef unsigned char BYTE;
QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE
class SendThread;//前置声明
class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

    void receiveFigData();
    QString ByteArrayToHexString(QByteArray &ba);
    int nFileBytes;//要发送的数据总大小（字节）
    int MAX_UDP_LEN;
    char* bf;//要发送的数据
    unsigned char send_once_data[512];
    QUdpSocket * udpSocket;
    QString receiver_ip;
    quint16 receiver_port;
    union intdata{
        char b[2];
        int a;
    };
    union int4data{
        char b[4];
        int a;
    };
    union floatdata{
        char b[4];
        float a;
    };



private slots:

    void UartReadData();//串口读取接收数据

    void on_btn_series_clicked();

    void on_btnSeriesSend_clicked();

    void on_btn_udp_init_clicked();

    void on_btn_begin_clicked();

    void on_btn_import_clicked();

    void VideoSend();

    void on_btn_end_clicked();

    void on_btn_udp_send_clicked();

    void on_btn_selfcheck_clicked();

    void on_btn_selfcheckanswer_clicked();

    void on_btn_bind_clicked();

    void on_btn_collect_clicked();

    void on_btn_selfcheck_send_clicked();

    void on_btn_selfcheckanswer_send_clicked();

    void on_btn_bind_send_clicked();

    void on_btn_collect_send_clicked();

private:
    Ui::Widget *ui;
    void PortListInit();    //串口列表初始化
    void UartInit();    //串口初始化

    QSerialPort *serial;  //fpga串口通信


    QTimer fps_timer;//定时器
    QByteArray send_data;//要发送的数据



    int total_frame_num;//要发送的帧总数
    int currnet_frame_num;//当前要发送的帧数

    int iwidth;
    int iheight;


    float dMax;
    float dMin;
    int nCount;
    float dMean;
    double dStd;
    int sizePixel;
    FILE * stream;
    BYTE * idata;
    unsigned short srcData[256*256];
    QByteArray byte;

    SendThread * sendthread;  //线程对象

    //udp接收有关
    QString dat_save_path;
    QByteArray buff;
    QHostAddress adrr ;
    quint16 port;
    quint64 receive_size;
    qint64 receive_len;
    FILE * receive_stream;
    int current_receive_size;
    int receive_times;
    unsigned short rec_Data[256*256];
    float rec_dMax;
    float rec_dMin;
    int rec_nCount;
    float rec_dMean;
    double rec_dStd;
    BYTE * rec_data;


};
#endif // WIDGET_H
