#include "sendthread.h"
#include<QDebug>
#include <windows.h>
SendThread::SendThread()
{

}

void SendThread::getaddress(Widget *p)
{
    w=p;//将主线程Widegt对象的地址赋值给子线程中的Widget对象，这样就可以访问主线程中的所有数据方法而不受线程的限制
}


void SendThread::run()
{
    //直接发送全部的dat文件数据

    int packetNum = 0;
    int lastPaketSize = 0;
    packetNum = w->nFileBytes / w->MAX_UDP_LEN;//分包的数量
    lastPaketSize = w->nFileBytes % w->MAX_UDP_LEN;//最后一个包的数据长度
    int currentPacketIndex = 0;
    if (lastPaketSize != 0)
    {
        packetNum = packetNum + 1;
    }
//    qDebug()<<"分包数量："<<QString::number(packetNum);
    while (currentPacketIndex < packetNum)//当还没有到达包尾
    {
        //延时避免丢包
        for (int i = 0 ;i < 100000;i++){}
        //每一帧之间做间隔延迟
//        if (currentPacketIndex % 128 == 0)
//        {
//            for (int i = 0 ;i < 25600000;i++){}
//        }

        if (currentPacketIndex < (packetNum-1))//当不是最后一个包
        {
            memcpy(w->send_once_data,w->bf+currentPacketIndex*w->MAX_UDP_LEN,w->MAX_UDP_LEN);
            //发送
            int length=w->udpSocket->writeDatagram(
                        (const char*)w->send_once_data, w->MAX_UDP_LEN,
                        QHostAddress(w->receiver_ip), w->receiver_port);//udp发送数据

            if(length!=w->MAX_UDP_LEN)//如果发送数据长度！=包头+包数据长度，说明发送失败
            {
              qDebug()<<"Failed to send image";
            }

        }
        else//当为最后一个包
        {
            memcpy(w->send_once_data,w->bf+currentPacketIndex*w->MAX_UDP_LEN,w->MAX_UDP_LEN);
            //发送
            int length=w->udpSocket->writeDatagram(
                        (const char*)w->send_once_data, w->nFileBytes-currentPacketIndex*w->MAX_UDP_LEN,
                        QHostAddress(w->receiver_ip), w->receiver_port);//udp发送数据

            if(length!=w->MAX_UDP_LEN)//如果发送数据长度！=包头+包数据长度，说明发送失败
            {
              qDebug()<<"Failed to send image";
            }

        }
        currentPacketIndex++;//包序号+1
    }



}
