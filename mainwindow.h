#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QAudio>
#include <QAudioInput>
#include <QAudioOutput>
#include <QFile>

class QAudioInput;
class AudioMonitor;
class QThread;
class QBuffer;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void on_error(QString msg);
    void on_awaken(QString msg);

private slots:
    void on_start_clicked();
    void audio_handle_state_changed(QAudio::State newState);
    void on_stop_clicked();

    void on_startRec_clicked();

    void on_stopRec_clicked();

    void on_playRec_clicked();

    void on_stopPlay_clicked();

private:
    Ui::MainWindow *ui;
    QAudioInput* _audio;
    QBuffer* _audio_buf;
    AudioMonitor* _monitor;
    QThread* _monitor_thread;
    QFile _outFile;
    bool _isPlay;
    QAudioOutput* _audioOut;
};

#endif // MAINWINDOW_H
