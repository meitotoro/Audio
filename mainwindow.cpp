#include <QAudioInput>
#include <QMessageBox>
#include <QDebug>
#include <QThread>
#include <QBuffer>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "audiomonitor.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    _audioOut(nullptr),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QAudioFormat format;
    // Set up the desired format, for example:
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::UnSignedInt);

    auto devs = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    if (devs.empty()) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("找不到可用录音设备"));
    }
    auto info = QAudioDeviceInfo::defaultInputDevice();
    if (!info.isFormatSupported(format)) {
        qWarning() << "Default format not supported, trying to use the nearest.";
        format = info.nearestFormat(format);
    }

    _audio = new QAudioInput(format, this);
    _audio_buf = new QBuffer();
    _audio_buf->open(QIODevice::ReadWrite);
    _isPlay=false;

    connect(_audio, &QAudioInput::stateChanged, this, &MainWindow::audio_handle_state_changed);

}

MainWindow::~MainWindow()
{
    delete _audio;
    delete ui;
    if (_audioOut)
        delete _audioOut;
}

void MainWindow::on_start_clicked()
{
    _monitor = new AudioMonitor(_audio_buf);
    set_active_monitor(_monitor);
    _monitor_thread = new QThread;
    _monitor->moveToThread(_monitor_thread);
    connect(_monitor, &AudioMonitor::error, this, &MainWindow::on_error);
    connect(_monitor, &AudioMonitor::awaken, this, &MainWindow::on_awaken);
    connect(_monitor_thread, &QThread::started, _monitor, &AudioMonitor::run);
    connect(_monitor, &AudioMonitor::stopped, _monitor, &AudioMonitor::deleteLater);
    connect(_monitor, &AudioMonitor::stopped, _audio, &QAudioInput::stop);
    connect(_monitor, &AudioMonitor::stopped, _monitor_thread, &QThread::quit);
    connect(_monitor_thread, &QThread::finished, _monitor_thread, &QThread::deleteLater);
    connect(_audio_buf, &QIODevice::bytesWritten, _monitor, &AudioMonitor::on_notify);
    //_audio->setNotifyInterval(200);

    _audio->start(_audio_buf);
     qDebug() << _audio->state();
}

void MainWindow::on_stop_clicked()
{
    _monitor->stop();
    //_audio_dev = nullptr;
}

void MainWindow::audio_handle_state_changed(QAudio::State newState)
{
    switch (newState) {
        case QAudio::StoppedState:
            if (_audio->error() != QAudio::NoError) {
                // Error handling
                qDebug() << "QAudioInput error: " << _audio->error();
            } else {
                // Finished
                _monitor->stop();
                qDebug() << "audio state changed: " << _audio->state();
            }
            break;

        case QAudio::ActiveState:
            // Started recording - read from IO device
            _monitor_thread->start();
            qDebug() << "audio state changed: " << _audio->state();
            break;

        default:
            qDebug() << "audio state changed: " << _audio->state();
            // ... other cases as appropriate
            break;
    }
}

void MainWindow::on_error(QString msg)
{
    QMessageBox::warning(this, QStringLiteral("错误"), msg);
}

void MainWindow::on_awaken(QString msg)
{
    QMessageBox::information(this, QStringLiteral("唤醒成功"), msg);
}

void MainWindow::on_startRec_clicked()
{
    if (_outFile.isOpen())
    {
        _outFile.close();
    }
    QString appPath=qApp->applicationDirPath();
    _outFile.setFileName(qApp->applicationDirPath()+"/test.raw");
    _outFile.open(QIODevice::WriteOnly|QIODevice::Truncate);
    _audio->start(&_outFile);
}

void MainWindow::on_stopRec_clicked()
{
    _audio->stop();
    _outFile.close();
}


void MainWindow::on_playRec_clicked()
{
    if (_isPlay)
        {
            on_stopPlay_clicked();
        }
        _isPlay=true;
        _outFile.open(QIODevice::ReadOnly);
        QAudioFormat format;
        format.setSampleRate(16000);
        format.setChannelCount(1);
        format.setSampleSize(16);
        format.setCodec("audio/pcm");
        format.setByteOrder(QAudioFormat::LittleEndian); //设置高低位，LittleEndIan（低位优先）/LargeEndIan（高位优先）
        format.setSampleType(QAudioFormat::UnSignedInt);
        QAudioDeviceInfo devInfo(QAudioDeviceInfo::defaultOutputDevice());
        qDebug()<<devInfo.supportedCodecs();
        if(!devInfo.isFormatSupported(format))
        {
            qWarning()<<"Raw audio format not supported by backend, cannot play audio";
            return;
        }
       _audioOut=new QAudioOutput(format,this);
      //connect(_audioOutput,SIGNAL(stateChanged(QAudio::State)),this,SLOT(handleStateChanged(QAudio::State)));
       _audioOut->start(&_outFile);
}



void MainWindow::on_stopPlay_clicked()
{
    if (_isPlay)
    {
        _isPlay=false;
        if (_audioOut!=NULL)
        {
            _audioOut->stop();
            _outFile.close();
            delete _audioOut;
            _audioOut=nullptr;
        }
    }
}
