#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->txtReadData->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    statusTimer = new QTimer(this);

    emu = new EmuNWAccessClient(this);
    connect(emu, &EmuNWAccessClient::connected,    this, &MainWindow::on_emu_connected);
    connect(emu, &EmuNWAccessClient::disconnected, this, &MainWindow::on_emu_disconnected);
    connect(emu, &EmuNWAccessClient::connectError, this, &MainWindow::on_emu_connectError);
    connect(emu, &EmuNWAccessClient::readyRead,    this, &MainWindow::on_emu_readyRead);

    on_emu_disconnected();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnConnect_clicked()
{
    ui->btnConnect->setEnabled(false);
    // TODO: show spinner?

    if (emu->isConnected()) {
        ui->statusbar->showMessage("Disconnecting ...");
        emu->disconnectFromHost();
    } else {
        ui->statusbar->showMessage("Connecting ...");
        emu->connectToHost(ui->txtIP->text(), 65400);
        emu->waitForConnected();
    }
}

void MainWindow::on_emu_connected()
{
    ui->statusbar->showMessage("Connected");

    ui->grpCores->setEnabled(true);
    ui->grpEmu->setEnabled(true);
    ui->grpDebug->setEnabled(true);
    ui->grpMemory->setEnabled(true);
    ui->cbxReadMemory->setEnabled(false);
    ui->cbxWriteMemory->setEnabled(false);
    ui->btnRead->setEnabled(false);
    ui->btnWrite->setEnabled(false);
    ui->btnReadWriteTest->setEnabled(false);

    ui->btnConnect->setText("Disconnect");
    ui->btnConnect->setEnabled(true);
    ui->txtIP->setEnabled(false);

    emu->cmdEmuInfo();
    emu->cmdCoresList();
    emu->cmdCoreMemories();

    statusTimer->connect(statusTimer, &QTimer::timeout, this, [this](){
        emu->cmdEmuStatus();
        emu->cmdGameInfo();
    });
    statusTimer->start(1000);
}

void MainWindow::on_emu_disconnected()
{
    if (!emu || emu->error().isEmpty())
        ui->statusbar->showMessage("Disconnected");
    else
        ui->statusbar->showMessage("Disconnected: "+emu->error());

    ui->grpCores->setEnabled(false);
    ui->grpEmu->setEnabled(false);
    ui->grpDebug->setEnabled(false);
    ui->grpMemory->setEnabled(false);
    ui->lstCores->clear();
    ui->cbxReadMemory->clear();
    ui->cbxWriteMemory->clear();
    ui->lblEmu->setText("");

    ui->btnConnect->setText("Connect");
    ui->btnConnect->setEnabled(true);    
    ui->txtIP->setEnabled(true);

    statusTimer->disconnect();
    statusTimer->stop();

    lastEmuState.clear();
    lastGameInfo.clear();
}

void MainWindow::on_emu_connectError()
{
    on_emu_disconnected();
    ui->statusbar->showMessage("Error: "+emu->error());
}

void MainWindow::on_emu_readyRead()
{
    EmuNWAccessClient::Reply reply = emu->readReply();
    qDebug() << reply;

    if (reply.cmd == "CORES_LIST") ui->btnCoresList->setEnabled(true);

    // ignore OK reply for simple commands
    if (!reply.isError && (reply.cmd == "EMU_STOP" || reply.cmd == "EMU_PAUSE" ||
                           reply.cmd == "EMU_RESUME" || reply.cmd == "EMU_RESET" ||
                           reply.cmd == "EMU_RELOAD" || reply.cmd == "CORE_RESET" ||
                           reply.cmd == "LOAD_GAME" || reply.cmd == "LOAD_CORE" ||
                           reply.cmd == "CORE_WRITE" ||
                           reply.cmd == "DEBUG_BREAK" || reply.cmd == "DEBUG_CONTINUE"))
        return;

    bool stateChanged = false;

    if (reply.cmd == "CORES_LIST" && reply.isAscii && !reply.isError) {
        ui->lstCores->clear();
        for (auto& map: reply.toMapList()) {
            if (map.contains("name")) {
                ui->lstCores->addItem("[" + map["platform"] + "] " + map["name"]);
            }
        }
    }
    else if (reply.cmd == "EMU_INFO" && reply.isAscii && !reply.isError) {
        ui->lblEmu->setText(reply.ascii.join(", "));
    }
    else if (reply.cmd == "EMU_STATUS" && reply.isAscii && !reply.isError) {
        auto newState = reply.toMap()["state"];
        if (lastEmuState != newState) {
            lastEmuState = newState;
            stateChanged = true;
        }
    }
    else if (reply.cmd == "GAME_INFO" && reply.isAscii && !reply.isError) {
        auto map = reply.toMap();
        auto newGameInfo = map["name"];
        if (map.contains("region")) newGameInfo += " [" + map["region"] + "]";
        if (lastGameInfo != newGameInfo) {
            lastGameInfo = newGameInfo;
            stateChanged = true;
        }
    }
    else if (reply.cmd == "CORE_MEMORIES" && reply.isAscii && !reply.isError) {
        for (auto& map: reply.toMapList()) {
            if (map.contains("name")) {
                QString access = map.contains("access") ? map["access"] : "rw";
                if (access.contains('r')) ui->cbxReadMemory->addItem(map["name"]);
                if (access.contains('w')) ui->cbxWriteMemory->addItem(map["name"]);
                if (map["name"] == "WRAM" && access.contains('w'))
                    ui->btnReadWriteTest->setEnabled(true);
            }
        }
        ui->cbxReadMemory->setEnabled(ui->cbxReadMemory->count()>0);
        ui->cbxWriteMemory->setEnabled(ui->cbxWriteMemory->count()>0);
        ui->btnRead->setEnabled(ui->cbxReadMemory->count()>0);
        ui->btnWrite->setEnabled(ui->cbxWriteMemory->count()>0);
    }
    else {
        if (reply.isBinary) {
            // print binary data to textbox
            QByteArray tmp = reply.binary.toHex(' ');
            QString s;
            while (tmp.length()>16*3) {
                s += QString::fromLatin1(tmp.left(16*3)+"\n");
                tmp = tmp.mid(16*3);
            }
            s += QString::fromLatin1(tmp.left(16*3));
            if (ui->chkReadAppend->isChecked()) {
                ui->txtReadData->appendPlainText(s);
            } else {
                ui->txtReadData->document()->setPlainText(s);
            }
        } else if (!reply.isError) {
            // print ascii data to textbox
            ui->txtReadData->document()->setPlainText(reply.ascii.join("\n"));
        } else {
            QMessageBox::warning(this, "Error", reply.error);
        }
    }

    if (stateChanged) {
        QString s = "State: " + lastEmuState;
        if (!lastGameInfo.isEmpty()) s += ", Game: " + lastGameInfo;
        ui->statusbar->showMessage(s);
    }
}

void MainWindow::on_btnEmuInfo_clicked()
{
    emu->cmdEmuInfo();
    emu->cmdEmuStatus();
}

void MainWindow::on_btnLoadGame_clicked()
{
    QString f = QFileDialog::getOpenFileName(this, "Select game");
    if (!f.isEmpty()) {
        emu->cmdLoadGame(f);
        emu->cmdEmuStatus();
        emu->cmdGameInfo();
    }
}

void MainWindow::on_btnEmuReset_clicked()
{
    emu->cmdEmuReset();
    emu->cmdEmuStatus();
}

void MainWindow::on_btnEmuStop_clicked()
{
    emu->cmdEmuStop();
    emu->cmdEmuStatus();
}

void MainWindow::on_btnEmuPause_clicked()
{
    emu->cmdEmuPause();
    emu->cmdEmuStatus();
}

void MainWindow::on_btnEmuResume_clicked()
{
    emu->cmdEmuResume();
    emu->cmdEmuStatus();
}

void MainWindow::on_btnEmuReload_clicked()
{
    emu->cmdEmuReload();
    emu->cmdEmuStatus();
}

void MainWindow::on_btnDebugBreak_clicked()
{
    emu->cmdDebugBreak();
    emu->cmdEmuStatus();
}

void MainWindow::on_btnDebugContinue_clicked()
{
    emu->cmdDebugContinue();
    emu->cmdEmuStatus();
}

void MainWindow::on_btnCoresList_clicked()
{
    ui->btnCoresList->setEnabled(false);
    emu->cmdCoresList();
}

void MainWindow::on_btnCoreInfo_clicked()
{
    emu->cmdCoreInfo(ui->lstCores->currentRow()>=0 ? ui->lstCores->currentItem()->text().split(" ")[1] : "");
}

void MainWindow::on_btnCoreLoad_clicked()
{
    ui->cbxReadMemory->setEnabled(false);
    ui->cbxWriteMemory->setEnabled(false);
    ui->cbxReadMemory->clear();
    ui->cbxWriteMemory->clear();
    ui->btnRead->setEnabled(false);
    ui->btnWrite->setEnabled(false);
    ui->btnReadWriteTest->setEnabled(false);
    emu->cmdLoadCore(ui->lstCores->currentRow()>=0 ? ui->lstCores->currentItem()->text().split(" ")[1] : "");
    emu->cmdCoreMemories();
}

void MainWindow::on_btnWrite_clicked()
{
    QString mem = ui->cbxWriteMemory->currentText();
    QString addrs = ui->txtWriteAddrLen->text();
    QByteArray data = QByteArray::fromHex(ui->txtWriteData->text().toLatin1());
    emu->cmdCoreWriteMemory(mem, data, addrs);
}

void MainWindow::on_btnRead_clicked()
{
    QString mem = ui->cbxReadMemory->currentText();
    QString addrs = ui->txtReadAddrLen->text();
    emu->cmdCoreReadMemory(mem, addrs);
}

void MainWindow::on_lstCores_currentRowChanged(int currentRow)
{
    ui->btnCoreLoad->setEnabled(currentRow>=0);
    ui->btnCoreInfo->setEnabled(currentRow>=0);
}

void MainWindow::on_btnCoreUnload_clicked()
{
    ui->cbxReadMemory->setEnabled(false);
    ui->cbxWriteMemory->setEnabled(false);
    ui->cbxReadMemory->clear();
    ui->cbxWriteMemory->clear();
    ui->btnRead->setEnabled(false);
    ui->btnWrite->setEnabled(false);
    ui->btnReadWriteTest->setEnabled(false);
    emu->cmdLoadCore("");
    emu->cmdCoreMemories();
}

void MainWindow::on_btnCoreCurrentInfo_clicked()
{
    emu->cmdCoreCurrentInfo();
}

void MainWindow::on_btnCoreReset_clicked()
{
    emu->cmdCoreReset();
    emu->cmdEmuStatus();
    emu->cmdGameInfo();
}

void MainWindow::on_btnReadWriteTest_clicked()
{
    auto oldChk = ui->chkReadAppend->checkState();
    // Secret of Evermore call beads = WRAM 0x231c
    emu->waitForBytesWritten(100);
    ui->txtReadData->clear();
    ui->chkReadAppend->setCheckState(Qt::CheckState::Checked);
    // single write, single read
    emu->cmdCoreWriteMemory("WRAM", {"\x01\x00",2}, 0x231C);
    emu->cmdCoreReadMemory ("WRAM", 0x231C, 1); // should give 01
    // multi-write 1, multi-read
    emu->cmdCoreWriteMemory("WRAM", { {0x231C, {"\x63",1}},{0x231B, {"\x00\x02",2}} } );
    emu->cmdCoreReadMemory ("WRAM", {{ 0x231C,1 }, { 0x231B,2 }}); // should give 02 00 02
    // multi-write 2, multi-read
    QList<QPair<int,int>> tmp = { { 0x231C,1 }, { 0x231B,3 } };
    emu->cmdCoreWriteMemory("WRAM", {"\x63\x00\x03\x00",4}, tmp);
    emu->cmdCoreReadMemory ("WRAM", {{ 0x231C,1 }, { 0x231B,2 }}); // should give 03 00 03
    // memory read/write from start
    emu->cmdCoreWriteMemory("WRAM", {"\x00",1});
    emu->cmdCoreReadMemory ("WRAM", 0, 1); // will give 00
    emu->cmdCoreWriteMemory("WRAM", {"\x01",1});
    emu->cmdCoreReadMemory ("WRAM", -1, 1); // should give 01
    // memory read/write to end
    emu->cmdCoreWriteMemory("WRAM", {"\x00",1}, 0x1ffff);
    emu->cmdCoreReadMemory ("WRAM", 0x1ffff, 1); // will give 00
    emu->cmdCoreWriteMemory("WRAM", {"\xaa\x77",2}, 0x1ffff);
    emu->cmdCoreReadMemory ("WRAM", 0x1ffff); // should give aa
    // memory read with padding
    emu->cmdCoreReadMemory ("WRAM", { { 0x1ffff,3 }, {0x1ffff,3} }); // should give aa 00 00 aa

    QTimer::singleShot(100, this, [this, oldChk](){
        ui->chkReadAppend->setCheckState(oldChk);
        if (emu->isConnected()) {
            if (ui->txtReadData->document()->toPlainText() !=
                    "01\n"
                    "02 00 02\n"
                    "03 00 03\n"
                    "00\n"
                    "01\n"
                    "00\n"
                    "aa\n"
                    "aa 00 00 aa")
            {
                QMessageBox::critical(this, "Error", "Tests failed. Did you load a compatible ROM?");
                return;
            }
        }
        QMessageBox::information(this, "Success", "Read/write tests succeeded!");
    });
}
