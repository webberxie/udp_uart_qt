#ifndef SENDTHREAD_H
#define SENDTHREAD_H

#include <QObject>
#include <QThread>
#include "widget.h"
class SendThread : public QThread
{
    Q_OBJECT
public:
    SendThread();

    void getaddress(Widget *p);

protected:
    //QThread的虚函数
    //线程处理函数
    //不能直接调用，通过start()间接调用
    void run();
signals:
    void isDone();  //处理完成信号
private:
    Widget *w;
};

#endif // SENDTHREAD_H
