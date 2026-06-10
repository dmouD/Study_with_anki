#include "mainwindow.h"

#include <QApplication>

/*
 * 程序入口。
 *
 * Qt Widgets 程序通常都从 QApplication 开始：
 * 1. 创建 QApplication 对象，负责事件循环和全局界面资源。
 * 2. 创建并显示主窗口。
 * 3. 调用 app.exec() 进入事件循环，等待用户操作。
 */
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.resize(1000, 650);
    window.show();

    return app.exec();
}
