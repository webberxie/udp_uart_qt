#include "widget.h"
#include "ui_widget.h"
#include<QDebug>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QThread>
#include <QHostAddress>
#include <QLibrary>
#include <QDir>
#include<QBuffer>
#include<QImageReader>
#include<QFileDialog>
#include<iostream>
#include<QString>
#include <mbstring.h>
#include <cstdio>
#include <windows.h>
#include<QDateTime>
#include "sendthread.h"
typedef unsigned char BYTE;
using namespace std;
//#include <string.h>
#pragma execution_character_set("utf-8")
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);


    //串口设置
    //波特率
    ui->comboBoxBaudRate->addItem("19200");
    ui->comboBoxBaudRate->addItem("38400");
    ui->comboBoxBaudRate->addItem("56000");
    ui->comboBoxBaudRate->addItem("57600");
    ui->comboBoxBaudRate->addItem("115200");
    ui->comboBoxBaudRate->addItem("230400");
    ui->comboBoxBaudRate->addItem("460800");
    ui->comboBoxBaudRate->addItem("921600");
    //数据位
    ui->comboBoxDataBits->addItem("5");
    ui->comboBoxDataBits->addItem("6");
    ui->comboBoxDataBits->addItem("7");
    ui->comboBoxDataBits->addItem("8");
    //检验位
    ui->comboBoxParity->addItem("no parity");
    ui->comboBoxParity->addItem("even parity");
    ui->comboBoxParity->addItem("odd parity");
    //停止位
    ui->comboBoxStopBits->addItem("1");
    ui->comboBoxStopBits->addItem("1.5");
    ui->comboBoxStopBits->addItem("2");
    serial = new QSerialPort(this);
    PortListInit(); //初始化串口列表


    //串口命令设置
    //装订参数
    //同步方式
    ui->comboBox_bindparm2->addItem("内同步");
    ui->comboBox_bindparm2->addItem("外同步");
    //输出图像模式
    ui->comboBox_bindparm3->addItem("校正后图像");
    ui->comboBox_bindparm3->addItem("灰阶测试图");
    ui->comboBox_bindparm3->addItem("棋盘格测试图");
    ui->comboBox_bindparm3->addItem("原始图像");
    ui->comboBox_bindparm3->addItem("增益");
    ui->comboBox_bindparm3->addItem("偏置");
    ui->comboBox_bindparm3->addItem("盲元位置");
    //LVDS开窗，全窗切换
    ui->comboBox_bindparm4->addItem("开窗");
    ui->comboBox_bindparm4->addItem("全窗");

    //采集命令
    //红外位控指令
    ui->comboBox_collectparm10->addItem("红外非均匀校正");
    ui->comboBox_collectparm10->addItem("红外star观测");
    ui->comboBox_collectparm10->addItem("捕获地面红外光源");
    ui->comboBox_collectparm10->addItem("红外目标预检测");
    ui->comboBox_collectparm10->addItem("末制导探测");
    //分割门限系数标志
    ui->comboBox_collectparm16->addItem("中末制导交班前使用");
    ui->comboBox_collectparm16->addItem("中末交班后使用");
    //轨控开机状态
    ui->comboBox_collectparm17->addItem("未开机");
    ui->comboBox_collectparm17->addItem("开机");
    //弹目距离有效标志
    ui->comboBox_collectparm13->addItem("无效");
    ui->comboBox_collectparm13->addItem("有效");


    //当有数据发送至自己的串口端口，调用dealMsg函数进行数据的读取
    connect(serial,SIGNAL(readyRead()),this,SLOT(UartReadData()));

    //初始化udp通信
    udpSocket = new QUdpSocket(this);

    udpSocket->bind(QHostAddress::Any,9000);//绑定当前网口

    setWindowTitle("端口：9000");

    //    视频保存路径 没有就新建
    dat_save_path = "./video/";
    QDir dir(dat_save_path);
    if(!dir.exists())
    {
        dir.mkpath("./video/");
        qDebug()<<"新建目录成功";
    }

    //创建存储接收文件
    QString save_file = dat_save_path+QDateTime::currentDateTime().toString("yyyyMMdd") +".dat";
    receive_stream = fopen(save_file.toLatin1().data(),"wb+");

    //部分参数初始化
    receive_times = 0;//接收udp包的次数
    MAX_UDP_LEN = 512;//udp包的最大字节数
    current_receive_size = 0;//接收文件长度
    iheight = 256;//图像高度
    iwidth = 256;//图像宽度
    rec_data=new BYTE[iwidth*iheight];//用于显示接收图像的数据，提前定义

    //建立发送数据子线程
    sendthread = new SendThread();

    //当有数据发送至自己的端口，调用receiveFigData函数进行数据的读取
    connect(udpSocket,&QUdpSocket::readyRead,this,&Widget::receiveFigData);


    //定时器到时间后，连接到图像显示槽函数，进行本地要发送的视频的显示
    connect(&fps_timer,SIGNAL(timeout()),this,SLOT(VideoSend()));


}


Widget::~Widget()
{
    delete ui;
}


//--------------------------------udp图像收发相关模块-------------------------------------


//将接收者的ip与端口地址存入receiver_ip(QString),receiver_ip(quint16)，并显示
void Widget::on_btn_udp_init_clicked()
{
    if(nullptr == ui->lineEditIP || nullptr== ui->lineEditPort)
    {
        return;
    }
    receiver_ip = ui->lineEditIP->text();
    receiver_port = ui->lineEditPort->text().toInt();
    qDebug()<<"对方IP："<<receiver_ip<<"对方端口："<<receiver_port;
    ui->label_ip_show->setText(receiver_ip);
    ui->label_port_show->setText(QString::number(receiver_port));

}


//点击导入视频流，开始在本地载入视频流(dat格式文件)，并存储到QByteArray中
//求取首帧图像的均值，方差，最大值，最小值；方便后续进行图像显示
void Widget::on_btn_import_clicked()
{

    //获得选择的dat文件名
    QString datfilename = QFileDialog :: getOpenFileName(this,"open file","","text file(*.dat)");

    if(datfilename == "")
    {
       return;
    }


//    qDebug()<<datfilename;

    ui->label_video_show->setText(datfilename);
    QFile readFile(datfilename);
    readFile.open(QIODevice::ReadOnly);
    send_data = readFile.readAll(); //读出所有数据,存入QbyteArray格式的send_data中

    bf = send_data.data();
    qDebug() << "文件长度为："<<readFile.size();
    readFile.close(); //关闭文件




    sizePixel = sizeof(unsigned short);//每个像素占据多少字节单位(2)
    idata=new BYTE[iwidth*iheight];//初始化显示本地图像的数组

    //利用首帧图像进行均值，方差，最大值，最小值的求取
    stream = fopen(datfilename.toLatin1().data(),"rb");	//读入的视频流文件
    fseek( stream,  0, SEEK_END );//查找结尾
    nFileBytes = ftell( stream );//统计流总长度
    total_frame_num = nFileBytes/(sizePixel*iwidth*iheight);//计算总共有多少帧
    qDebug()<<"本视频总帧数："<<QString::number(total_frame_num);


    fseek( stream,  0, SEEK_SET );//查找开头
    fread( &srcData, sizePixel, iwidth*iheight, stream );
    dMax = 0;
    dMin = 0;
    nCount = 0;
    dMean = 0;
    dStd = 0;
    int m,n,y;
    for(m=1;m<iheight;m++)
    {
        for(n=1;n<iwidth;n++)
        {
            if (srcData[m*iwidth+n]>2000)
            {
                dMean = dMean + srcData[m*iwidth+n];
                nCount++;
            }
        }
    }
    dMean = dMean/nCount;//计算均值
    qDebug()<<QString::number(nCount);

    for(m=1;m<iheight;m++)
    {
        for(n=1;n<iwidth;n++)
        {
            if (srcData[m*iwidth+n]>2000)
            {
                dStd = dStd + (srcData[m*iwidth+n]-dMean)*(srcData[m*iwidth+n]-dMean);
            }
        }
    }
    dStd = sqrt(dStd/nCount);//计算方差

    dMax = dMean+dStd*8;//设置最大值与最小值
    dMin = dMean-dStd*8;
    qDebug()<<"均值："<<QString::number(dMean)<<"方差："<<QString::number(dStd)<<"最大值："<<QString::number(dMax)<<"最小值："<<QString::number(dMin);

    fseek( stream,  0, SEEK_SET );//查找开头
    qDebug()<<"stream init";
    currnet_frame_num = -1;


}



//接收由对方udp通信传来的图像数据，并进行显示，保存到本地
//QByteArray--char*--QImage(显示);接收每一包为1024字节，接收128包恰好为1帧
//QByteArray--char*--dat(存储)
void Widget::receiveFigData()
{
    receive_times += 1;
//    if (receive_times % 100000 ==0){
//        qDebug()<<"接收次数："<<QString::number(receive_times);
//    }

//    qDebug()<<"正在接收，本端口9000";
    receive_size = udpSocket->pendingDatagramSize();
//    quint64 size = 262144000;
//    qDebug()<<"初始化接收buff的大小："<<QString::number(size);


    //让buff设置为DatagramSize的大小，保证不丢包
    buff.resize(receive_size);
    //对方的IP，端口
    receive_len = udpSocket->readDatagram(buff.data(),buff.size(),&adrr,&port); //接收数据到buff

//    qDebug()<<QString::number(receive_len);
    // 如果接收非空，进行存储与显示
    if (receive_len>0)
    {
        //如果是较短文本，直接在文本框显示
        if (receive_len<10){
            QString str = QString(buff);
            ui->textEdit_udp_receive->setText(str);
            receive_times--;
        }
        //如果接收的是视频数据
        else
        {
            current_receive_size+=receive_len;//统计视频接收的字节
            char* re_bf = buff.data();
            fwrite(re_bf,1,receive_len,receive_stream);//存储到接收文件中
            if (receive_times % 256 == 0)
            {
                //当接收256包，进行图像显示
                fseek(receive_stream,-256*256*2,SEEK_CUR);
                fread(&rec_Data, 2, 256*256, receive_stream);
                fseek(receive_stream,0,SEEK_END);
                //接收第一帧
                if (receive_times == 256)
                {
                    rec_dMax = 0;
                    rec_dMin = 0;
                    rec_nCount = 0;
                    rec_dMean = 0;
                    rec_dStd = 0;
                    int m,n,y;
                    for(m=1;m<iheight;m++)
                    {
                        for(n=1;n<iwidth;n++)
                        {
                            if (rec_Data[m*iwidth+n]>2000)
                            {
                                rec_dMean = rec_dMean + rec_Data[m*iwidth+n];
                                rec_nCount++;
                            }
                        }
                    }
                    rec_dMean = rec_dMean/rec_nCount;//计算均值


                    for(m=1;m<iheight;m++)
                    {
                        for(n=1;n<iwidth;n++)
                        {
                            if (rec_Data[m*iwidth+n]>2000)
                            {
                                rec_dStd = rec_dStd + (rec_Data[m*iwidth+n]-rec_dMean)*(rec_Data[m*iwidth+n]-rec_dMean);
                            }
                        }
                    }
                    rec_dStd = sqrt(rec_dStd/rec_nCount);//计算方差

                    rec_dMax = rec_dMean+rec_dStd*8;//设置最大值与最小值
                    rec_dMin = rec_dMean-rec_dStd*8;
//                    qDebug()<<"均值："<<QString::number(rec_dMean)<<"方差："<<QString::number(rec_dStd)<<"最大值："<<QString::number(rec_dMax)<<"最小值："<<QString::number(rec_dMin);

                }

                //图像归一化，显示
                int m1,n1,y1;
                for(m1=0;m1<iheight;m1++)
                {
                    for(n1=0;n1<iwidth;n1++)
                    {
                        y1 = m1;
                        float temp = rec_Data[m1*iwidth+n1];
                        temp=(temp-rec_dMin)*255/(rec_dMax-rec_dMin);//归一化到0-255
                        if (temp>255)
                        {
                            temp = 255;
                        }
                        if (temp<0)
                        {
                            temp = 0;
                        }
                        rec_data[y1*iwidth+n1] = temp;
                    }
                }
                QImage image(rec_data,iheight,iwidth,QImage::Format_Indexed8);
                ui->label_receive->setPixmap(QPixmap::fromImage(image));
                ui->label_receive->resize(image.width(),image.height());
                ui->label_receive->show();


            }

//            if (receive_times > 255900 ){
//                qDebug()<<"接收长度："<<QString::number(current_receive_size);
//                qDebug()<<"接收次数："<<QString::number(receive_times);
//            }

        }
    }

//    if (receive_times==256000){
//        fclose(receive_stream);
//        qDebug()<<"保存成功";
//    }

}

//10ms显示一次正在发送视频图像函数
//QByteArray--QImage--QPixMap
void Widget::VideoSend()
{


    const int iwidth = 256;
    const int iheight = 256;
    int m,n,y;
//    unsigned short srcData[iwidth*iheight];//新建存放图像的一维数组

//    idata=new BYTE[iwidth*iheight*3];//存放最终的数据

    if(!feof(stream))//当到达流的末尾，feof会返回true
    {
        fread( &srcData, sizePixel, iwidth*iheight, stream );//读入当前帧图像数据，显示帧序号
        currnet_frame_num++;
        ui->label_sendmsg->setText(QString::number(currnet_frame_num));
        //归一化，显示
        for(m=0;m<iheight;m++)
        {
            for(n=0;n<iwidth;n++)
            {
                y = m;
                //y=iheight-n-1;
                float temp = srcData[m*iwidth+n];
                temp=(temp-dMin)*255/(dMax-dMin);//归一化到0-255
                if (temp>255)
                {
                    temp = 255;
                }
                if (temp<0)
                {
                    temp = 0;
                }
                idata[y*iwidth+n] = temp;
            }
        }
        //显示图像
//        QImage image(idata,iheight,iwidth,QImage::Format_RGB32);
        QImage image(idata,iheight,iwidth,QImage::Format_Indexed8);
        ui->label_send->setPixmap(QPixmap::fromImage(image));
        ui->label_send->resize(image.width(),image.height());
        ui->label_send->show();
        if(currnet_frame_num==total_frame_num){
            fps_timer.stop();
        }
    }


}


//点击发送，视频流进行分包之后，直接全部发送，设置显示图像定时器周期为10ms
void Widget::on_btn_begin_clicked()
{
//    qDebug()<<"正在发送，端口9000";
//    QString start_flag = "begin";
//    udpSocket->writeDatagram(start_flag.toUtf8(),QHostAddress(receiver_ip),receiver_port);
//    for (int i = 0 ;i < 200000;i++){}

    //启动线程，处理数据
    sendthread->getaddress(this);
    sendthread->start();


    //设置定时器
    fps_timer.start(20);

}


//点击结束，定时器停止，停止显示图像
void Widget::on_btn_end_clicked()
{
    fps_timer.stop();

}

void Widget::on_btn_udp_send_clicked()
{
    //读取编辑区内容
    if(nullptr==ui->textEdit_udp_send)
    {
        return;
    }
    QString str= ui->textEdit_udp_send->toPlainText();
    qDebug()<<"要发送的数据："<<str;


    //写入套接字
    qint64 sucess_len = udpSocket->writeDatagram(str.toUtf8(),QHostAddress(receiver_ip),receiver_port);
    qDebug()<<"文本发送成功的字节数："<<QString::number(sucess_len);


}

//-----------------------------串口通信相关模块----------------------------------------------------

void Widget::UartReadData()
{
    QByteArray buf;
    buf = serial->readAll();
//    qDebug()<<"receive data: "<<buf;
    if(!buf.isEmpty())
    {
        char * temp = buf.data();
        int len = buf.size();
        QString str;
        for(int i=0;i<len;i++)
        {
            str += QString::number(*temp,16);
            str += " ";
            temp++;
        }
        ui->textEditReceive->setText(str);

    }
    buf.clear();
}

void Widget::PortListInit()
{
    //获取每一个串口的名称，并将其存入m_listport
    QStringList m_listport;
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts()){
        m_listport<<info.portName();
        qDebug()<<"name:"<<info.portName();
    }
    //将检测到的端口添加到端口号下拉菜单
    for(int i = 0;i < m_listport.count(); i++){
        ui->comboBoxPort->addItem(m_listport[i]);
    }
//    if(serial->isOpen()){//如果已经打开，则先关闭
//        serial->clear();
//        serial->close();
//    }
}



//串口初始化
void Widget::UartInit()
{
//    QSerialPort *serial = new QSerialPort;
    if(serial->isOpen()){   //如果已经打开，则先关闭
        serial->clear();
        serial->close();
    }
    serial->setPortName(ui->comboBoxPort->currentText());
    //检查串口是否成功打开
    if(serial->open(QIODevice::ReadWrite)==true){
        qDebug()<<"open ok!";
        ui->label_close_or_open->setText("当前打开");
    }
    else{
        qDebug()<<"open not ok!";
    }
    serial->setBaudRate(ui->comboBoxBaudRate->currentText().toInt());//设置波特率为115200
    //设置数据位
    if (ui->comboBoxDataBits->currentText()=="5")
    {
        serial->setDataBits(QSerialPort::Data5);
    }
    else if (ui->comboBoxDataBits->currentText()=="6")
    {
        serial->setDataBits(QSerialPort::Data6);
    }
    else if (ui->comboBoxDataBits->currentText()=="7")
    {
        serial->setDataBits(QSerialPort::Data7);
    }
    else
    {
        serial->setDataBits(QSerialPort::Data8);    //默认数据位是8
    }
    //设置校验位
    if (ui->comboBoxParity->currentText()=="even parity")
    {
        serial->setParity(QSerialPort::EvenParity);
    }
    else if (ui->comboBoxParity->currentText()=="odd parity")
    {
        serial->setParity(QSerialPort::OddParity);
    }
    else
    {
        serial->setParity(QSerialPort::NoParity);    //默认校验位是0
    }
    //设置停止位
    if (ui->comboBoxStopBits->currentText()=="2")
    {
        serial->setStopBits(QSerialPort::TwoStop);
    }
    else if (ui->comboBoxStopBits->currentText()=="1.5")
    {
        serial->setStopBits(QSerialPort::OneAndHalfStop);
    }
    else
    {
        serial->setStopBits(QSerialPort::OneStop);    //默认停止位是1
    }

    serial->setFlowControl(QSerialPort::NoFlowControl);//设置为无流控制

    bool debug = true;
    if(debug){
        qDebug()<<"baudRate: "<<serial->baudRate();
        qDebug()<<"portName: "<<serial->portName();
        qDebug()<<"parity: "<<serial->parity();
        qDebug()<<"stopBits: "<<serial->stopBits();
        qDebug()<<"dataBits: "<<serial->dataBits();
    }


}

//打开串口与关闭串口切换
void Widget::on_btn_series_clicked()
{

    if(ui->btn_series->text() == "open series")
    {
        UartInit();
        ui->btn_series->setText("close series");
    }
    else if(ui->btn_series->text()=="close series")
    {
        serial->close();
        ui->btn_series->setText("open series");
        ui->label_close_or_open->setText("当前关闭");
    }
}

void Widget::on_btnSeriesSend_clicked()
{
    //读取编辑区内容
    if(nullptr==ui->textEditSend)
    {
        return;
    }
    QString send_msg= ui->textEditSend->toPlainText();
    serial->write(send_msg.toLatin1().data(),send_msg.length());
    qDebug()<<send_msg.toLatin1().data();
//    qDebug()<<send_msg.length();
//    serial->write(send_msg.toLatin1().data(),send_msg.length());
//    static unsigned char send_list[3] = {1,2,3};//{0x01,0x01,0x55};
//    serial->write((const char*)send_list,3);


}



//ByteArray转HexString
QString Widget:: ByteArrayToHexString(QByteArray &ba)
{
    QDataStream out(&ba,QIODevice::ReadWrite);   //将str的数据 读到out里面去
    QString buf;
    while(!out.atEnd())
    {
         qint8 outChar = 0;
         out >> outChar;   //每次一个字节的填充到 outchar
         QString str = QString("%1").arg(outChar&0xFF,2,16,QLatin1Char('0')).toUpper() + QString(" ");   //2 字符宽度
         buf += str;
    }
    return buf;
}



//按下串口命令部分，跳转到其参数列表页

void Widget::on_btn_selfcheck_clicked()
{
    ui->tab_parm->setCurrentIndex(0);
}

void Widget::on_btn_selfcheckanswer_clicked()
{
    ui->tab_parm->setCurrentIndex(1);
}

void Widget::on_btn_bind_clicked()
{
    ui->tab_parm->setCurrentIndex(2);
}

void Widget::on_btn_collect_clicked()
{
    ui->tab_parm->setCurrentIndex(3);
}

//发送串口命令，将命令内容显示在textedit中
//自检
void Widget::on_btn_selfcheck_send_clicked()
{
    static unsigned char send_list1[4] = {0xAA,0x55,0x01,0x00};
    serial->write((const char*)send_list1,4);
    //显示
    QString show = "";
    for(int j = 0;j<4;j++)
    {
        show += QString::number( send_list1[j], 16);
        show += " ";
    }

    ui->textEdit->setText(show);
}
//自检结果
void Widget::on_btn_selfcheckanswer_send_clicked()
{
    static unsigned char send_list2[4] = {0xAA,0x55,0x02,0x00};
    serial->write((const char*)send_list2,4);
    //显示
    QString show = "";
    for(int j = 0;j<4;j++)
    {
        show += QString::number( send_list2[j], 16);
        show += " ";
    }

    ui->textEdit->setText(show);

}
//装订参数
void Widget::on_btn_bind_send_clicked()
{
    static unsigned char send_list3[19] = {0xAA,0x55,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    //红外积分时间
    if (ui->lineEdit_bindparm1 != nullptr)
    {
        send_list3[3] = ui->lineEdit_bindparm1->text().toInt();
    }
    else
    {
        send_list3[3] = 0xFF;//默认255
    }
//    qDebug()<<QString::number( send_list3[3], 16);
    //同步方式
    if (ui->comboBox_bindparm2->currentIndex()==0)
    {
        send_list3[4] = 0x00;
    }
    else
    {
        send_list3[4] = 0xFF;
    }
//    qDebug()<<QString::number( send_list3[4], 16);
    //输出图像模式
    if (ui->comboBox_bindparm3->currentIndex()==0)
    {
        send_list3[5] = 0x00;
    }
    else if (ui->comboBox_bindparm3->currentIndex()==1)
    {
        send_list3[5] = 0x01;
    }
    else if (ui->comboBox_bindparm3->currentIndex()==2)
    {
        send_list3[5] = 0x02;
    }
    else if (ui->comboBox_bindparm3->currentIndex()==3)
    {
        send_list3[5] = 0x03;
    }
    else if (ui->comboBox_bindparm3->currentIndex()==4)
    {
        send_list3[5] = 0x04;
    }
    else if (ui->comboBox_bindparm3->currentIndex()==5)
    {
        send_list3[5] = 0x05;
    }
    else
    {
        send_list3[5] = 0x06;
    }
//    qDebug()<<QString::number( send_list3[5], 16);

    //LVDS开窗，全窗切换
    if (ui->comboBox_bindparm4->currentIndex()==0)
    {
        send_list3[6] = 0x00;
    }
    else
    {
        send_list3[6] = 0x01;
    }
//    qDebug()<<QString::number( send_list3[6], 16);
    //红外图像分割门限系数
    if (ui->lineEdit_bindparm5 != nullptr)
    {
        send_list3[9] = ui->lineEdit_bindparm5->text().toInt();
    }
    else
    {
        send_list3[9] = 0xFF;
    }
//    qDebug()<<QString::number( send_list3[9], 16);
    //红外变积分时间信噪比系数
    if (ui->lineEdit_bindparm6 != nullptr)
    {
        send_list3[10] = ui->lineEdit_bindparm6->text().toInt();
    }
    else
    {
        send_list3[10] = 0xFF;
    }
//    qDebug()<<QString::number( send_list3[10], 16);
    //红外目标面积限制(14,15)14存低位，15存高位
    if (ui->lineEdit_bindparm7 != nullptr)
    {
        union intdata temp;
        temp.a = ui->lineEdit_bindparm7->text().toInt();
        send_list3[14] = temp.b[0];
        send_list3[15] = temp.b[1];
    }
    else
    {
        send_list3[14] = 0x00;
        send_list3[15] = 0x00;
    }
//    qDebug()<<QString::number( send_list3[14], 16);
    //可见变积分时间灰度门限(16,17)16存低位，17存高位
    if (ui->lineEdit_bindparm8 != nullptr)
    {
        union intdata temp2;
        temp2.a = ui->lineEdit_bindparm8->text().toInt();
        send_list3[16] = temp2.b[0];
        send_list3[17] = temp2.b[1];

//        send_list3[17] = temp2>>8;
//        send_list3[16] = temp2 - (send_list3[17]<<8);
    }
    else
    {
        send_list3[16] = 0x00;
        send_list3[17] = 0x00;
    }
//    qDebug()<<QString::number( send_list3[16], 16);


    //帧校验
    send_list3[18] = 0x00;
    for (int i = 2 ; i < 18 ; i++)
    {
        send_list3[18] += send_list3[i];
    }
    send_list3[18] = send_list3[18]&(0xff);//按校验和的规则:3-18位相加，取结果的低8位

    //检查
    QString show = "";
    for(int j = 0;j<19;j++)
    {
        show += QString::number( send_list3[j], 16);
        show += " ";
    }
    serial->write((const char*)send_list3,19);
    ui->textEdit->setText(show);

}

//采集命令
void Widget::on_btn_collect_send_clicked()
{
    static unsigned char send_list4[58] = {0xaa,0x55,0x04,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x00,0x00,0x00,0x00,
                                           0x11,0x00,0x00
                                          };
    //K发射惯性坐标系俯仰角
    if (ui->lineEdit_collectparm1 != nullptr)
    {
        union floatdata collectparm1;

        collectparm1.a = ui->lineEdit_collectparm1->text().toFloat();
        send_list4[3] = collectparm1.b[0];
        send_list4[4] = collectparm1.b[1];
        send_list4[5] = collectparm1.b[2];
        send_list4[6] = collectparm1.b[3];


    }
    else
    {
        send_list4[3] = 0x00;//默认0
        send_list4[4] = 0x00;
        send_list4[5] = 0x00;
        send_list4[6] = 0x00;
    }
    //K发射惯性坐标系方位角
    if (ui->lineEdit_collectparm2 != nullptr)
    {
        union floatdata collectparm2;

        collectparm2.a = ui->lineEdit_collectparm2->text().toFloat();
        send_list4[7] = collectparm2.b[0];
        send_list4[8] = collectparm2.b[1];
        send_list4[9] = collectparm2.b[2];
        send_list4[10] = collectparm2.b[3];


    }
    else
    {
        send_list4[7] = 0x00;//默认0
        send_list4[8] = 0x00;
        send_list4[9] = 0x00;
        send_list4[10] = 0x00;
    }
    //K发射惯性坐标系滚转角
    if (ui->lineEdit_collectparm3 != nullptr)
    {
        union floatdata collectparm3;

        collectparm3.a = ui->lineEdit_collectparm3->text().toFloat();
        send_list4[11] = collectparm3.b[0];
        send_list4[12] = collectparm3.b[1];
        send_list4[13] = collectparm3.b[2];
        send_list4[14] = collectparm3.b[3];


    }
    else
    {
        send_list4[11] = 0x00;//默认0
        send_list4[12] = 0x00;
        send_list4[13] = 0x00;
        send_list4[14] = 0x00;
    }
    //俯仰陀螺增量
    if (ui->lineEdit_collectparm4 != nullptr)
    {
        union floatdata collectparm4;

        collectparm4.a = ui->lineEdit_collectparm4->text().toFloat();
        send_list4[15] = collectparm4.b[0];
        send_list4[16] = collectparm4.b[1];
        send_list4[17] = collectparm4.b[2];
        send_list4[18] = collectparm4.b[3];


    }
    else
    {
        send_list4[15] = 0x00;//默认0
        send_list4[16] = 0x00;
        send_list4[17] = 0x00;
        send_list4[18] = 0x00;
    }
    //偏航陀螺增量
    if (ui->lineEdit_collectparm5 != nullptr)
    {
        union floatdata collectparm5;

        collectparm5.a = ui->lineEdit_collectparm5->text().toFloat();
        send_list4[19] = collectparm5.b[0];
        send_list4[20] = collectparm5.b[1];
        send_list4[21] = collectparm5.b[2];
        send_list4[22] = collectparm5.b[3];


    }
    else
    {
        send_list4[19] = 0x00;//默认0
        send_list4[20] = 0x00;
        send_list4[21] = 0x00;
        send_list4[22] = 0x00;
    }
    //滚转陀螺增量
    if (ui->lineEdit_collectparm6 != nullptr)
    {
        union floatdata collectparm6;

        collectparm6.a = ui->lineEdit_collectparm6->text().toFloat();
        send_list4[23] = collectparm6.b[0];
        send_list4[24] = collectparm6.b[1];
        send_list4[25] = collectparm6.b[2];
        send_list4[26] = collectparm6.b[3];


    }
    else
    {
        send_list4[23] = 0x00;//默认0
        send_list4[24] = 0x00;
        send_list4[25] = 0x00;
        send_list4[26] = 0x00;
    }
    //K发射惯性坐标系指令俯仰角
    if (ui->lineEdit_collectparm7 != nullptr)
    {
        union floatdata collectparm7;

        collectparm7.a = ui->lineEdit_collectparm7->text().toFloat();
        send_list4[27] = collectparm7.b[0];
        send_list4[28] = collectparm7.b[1];
        send_list4[29] = collectparm7.b[2];
        send_list4[30] = collectparm7.b[3];


    }
    else
    {
        send_list4[27] = 0x00;//默认0
        send_list4[28] = 0x00;
        send_list4[29] = 0x00;
        send_list4[30] = 0x00;
    }
    //K发射惯性坐标系指令方位角
    if (ui->lineEdit_collectparm8 != nullptr)
    {
        union floatdata collectparm8;

        collectparm8.a = ui->lineEdit_collectparm8->text().toFloat();
        send_list4[31] = collectparm8.b[0];
        send_list4[32] = collectparm8.b[1];
        send_list4[33] = collectparm8.b[2];
        send_list4[34] = collectparm8.b[3];


    }
    else
    {
        send_list4[31] = 0x00;//默认0
        send_list4[32] = 0x00;
        send_list4[33] = 0x00;
        send_list4[34] = 0x00;
    }
    //K发射惯性坐标系指令滚转角
    if (ui->lineEdit_collectparm9 != nullptr)
    {
        union floatdata collectparm9;

        collectparm9.a = ui->lineEdit_collectparm9->text().toFloat();
        send_list4[35] = collectparm9.b[0];
        send_list4[36] = collectparm9.b[1];
        send_list4[37] = collectparm9.b[2];
        send_list4[38] = collectparm9.b[3];


    }
    else
    {
        send_list4[35] = 0x00;//默认0
        send_list4[36] = 0x00;
        send_list4[37] = 0x00;
        send_list4[38] = 0x00;
    }
    //DSJ（测试设备）定时器帧号
    if (ui->lineEdit_collectparm11 != nullptr)
    {
        union int4data collectparm11;

        collectparm11.a = ui->lineEdit_collectparm11->text().toInt();
        send_list4[41] = collectparm11.b[0];
        send_list4[42] = collectparm11.b[1];
        send_list4[43] = collectparm11.b[2];
        send_list4[44] = collectparm11.b[3];


    }
    else
    {
        send_list4[41] = 0x00;//默认0
        send_list4[42] = 0x00;
        send_list4[43] = 0x00;
        send_list4[44] = 0x00;
    }
    //红外积分时间档位
    if (ui->lineEdit_collectparm12 != nullptr)
    {
        send_list4[45] = ui->lineEdit_collectparm12->text().toInt();
    }
    else
    {
        send_list4[45] = 0x00;//默认0
    }
    //弹目距离有效标志
    if (ui->comboBox_collectparm13->currentIndex()==0)
    {
        send_list4[46] = 0x00;
    }
    else
    {
        send_list4[46] = 0x01;//默认有效
    }
    //弹目距离
    if (ui->lineEdit_collectparm14 != nullptr)
    {
        union floatdata collectparm14;

        collectparm14.a = ui->lineEdit_collectparm14->text().toFloat();
        send_list4[47] = collectparm14.b[0];
        send_list4[48] = collectparm14.b[1];
        send_list4[49] = collectparm14.b[2];
        send_list4[50] = collectparm14.b[3];


    }
    else
    {
        send_list4[47] = 0x00;//默认0
        send_list4[48] = 0x00;
        send_list4[49] = 0x00;
        send_list4[50] = 0x00;
    }
    //弹目接近速度
    if (ui->lineEdit_collectparm15 != nullptr)
    {
        union floatdata collectparm15;

        collectparm15.a = ui->lineEdit_collectparm15->text().toFloat();
        send_list4[51] = collectparm15.b[0];
        send_list4[52] = collectparm15.b[1];
        send_list4[53] = collectparm15.b[2];
        send_list4[54] = collectparm15.b[3];


    }
    else
    {
        send_list4[51] = 0x00;//默认0
        send_list4[52] = 0x00;
        send_list4[53] = 0x00;
        send_list4[54] = 0x00;
    }

    //红外位控指令
    if (ui->comboBox_collectparm10->currentIndex()==0)
    {
        send_list4[40] = 0x20;
    }
    else if (ui->comboBox_collectparm10->currentIndex()==1)
    {
        send_list4[40] = 0x40;
    }
    else if (ui->comboBox_collectparm10->currentIndex()==2)
    {
        send_list4[40] = 0x50;
    }
    else if (ui->comboBox_collectparm10->currentIndex()==3)
    {
        send_list4[40] = 0x60;
    }
    else if (ui->comboBox_collectparm10->currentIndex()==4)
    {
        send_list4[40] = 0x80;
    }
    else
    {
        send_list4[40] = 0x00;//备份
    }
    //分割门限系数标志
    if (ui->comboBox_collectparm16->currentIndex()==0)
    {
        send_list4[55] = 0x11;
    }
    else
    {
        send_list4[55] = 0x22;//默认中末交班后使用
    }
    //轨控开机状态
    if (ui->comboBox_collectparm17->currentIndex()==0)
    {
        send_list4[56] = 0x00;
    }
    else
    {
        send_list4[56] = 0x55;//默认开机
    }
    //帧校验
    send_list4[57] = 0x00;
    for (int i = 2 ; i < 58 ; i++)
    {
        send_list4[57] += send_list4[i];
    }
    send_list4[57] = send_list4[57]&(0xff);//按校验和的规则:3-18位相加，取结果的低8位

    //显示
    QString show = "";
    for(int j = 0;j<58;j++)
    {
        show += QString::number( send_list4[j], 16);
        show += " ";
    }
    serial->write((const char*)send_list4,58);
    ui->textEdit->setText(show);

}
