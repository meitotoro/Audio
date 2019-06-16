#ifndef AUDIOMONITOR_H
#define AUDIOMONITOR_H

#include <QObject>
static const int MAX_GRAMMARID_LEN=32;

class QBuffer;

typedef struct _UserData {
    int     build_fini;  //标识语法构建是否完成
    int     update_fini; //标识更新词典是否完成
    int     errcode; //记录语法构建或更新词典回调错误码
    char    grammar_id[MAX_GRAMMARID_LEN]; //保存语法构建返回的语法ID
}UserData;

class AudioMonitor : public QObject
{
    Q_OBJECT

public:
    AudioMonitor(QBuffer* buf);
    ~AudioMonitor();

public slots:
    void run();
    void stop();
    void on_notify(qint64);

signals:
    void error(QString msg);
    void awaken(QString msg);
    void stopped();

private:
    int _audio_stat;
    QBuffer* _buf;
    const char* _session_id;
    enum _state{init,awaken,recognizing,choose};
private:
    void startRecognize();
    int build_grammar(UserData *udata);
};


void set_active_monitor(AudioMonitor* mon);
AudioMonitor* active_monitor();

extern "C" int cb_ivw_msg_proc( const char *sessionID, int msg, int param1, int param2, const void *info, void *userData );
extern "C" int build_grm_cb(int ecode, const char *info, void *udata);

#endif // AUDIOMONITOR_H
