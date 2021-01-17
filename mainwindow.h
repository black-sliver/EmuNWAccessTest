#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <emunwaccessclient.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    EmuNWAccessClient *emu;
    QTimer *statusTimer;

    QString lastEmuState;
    QString lastGameInfo;

private slots:
    void on_emu_disconnected();
    void on_emu_connected();
    void on_emu_connectError();
    void on_emu_readyRead();

    void on_btnConnect_clicked();
    void on_btnLoadGame_clicked();
    void on_btnEmuInfo_clicked();
    void on_btnEmuReset_clicked();
    void on_btnEmuStop_clicked();
    void on_btnEmuPause_clicked();
    void on_btnEmuResume_clicked();
    void on_btnEmuReload_clicked();
    void on_btnDebugBreak_clicked();
    void on_btnDebugContinue_clicked();
    void on_btnCoresList_clicked();
    void on_btnCoreInfo_clicked();
    void on_btnCoreCurrentInfo_clicked();
    void on_btnCoreLoad_clicked();
    void on_btnCoreUnload_clicked();
    void on_btnWrite_clicked();
    void on_btnRead_clicked();
    void on_lstCores_currentRowChanged(int currentRow);
    void on_btnCoreReset_clicked();
};
#endif // MAINWINDOW_H
