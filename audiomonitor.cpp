#include <QBuffer>
#include <QDebug>
#include <QThread>
#include "audiomonitor.h"
#include "awaken/msp_cmn.h"
#include "awaken/qivw.h"
#include "awaken/msp_errors.h"
#include "audiomonitor.h"

static const char *lgi_param = "appid = 5ce36fac, work_dir = .";
static const char *ssb_param = "ivw_threshold=0:1450,sst=wakeup,ivw_res_path =fo|D:\\yuyin\\sdk\\bin\\msc\\res\\ivw\\wakeupresource.jet";
static const int FRAME_LEN = 640; //16k采样率的16bit音频，一帧的大小为640B, 时长20ms
static const int MAX_PARAMS_LEN =1024;
static const char * ASR_RES_PATH = "fo|res/asr/common.jet";  //离线语法识别资源路径
static const char * GRM_BUILD_PATH= "res/asr/GrmBuilld_x64";  //构建离线语法识别网络生成数据保存路径
static const int SAMPLE_RATE_16K=16000;


AudioMonitor::AudioMonitor(QBuffer* buf) :
    _audio_stat(MSP_AUDIO_SAMPLE_CONTINUE),
    _buf(buf),
    _session_id(nullptr)
{
    auto ret = MSPLogin(nullptr, nullptr, lgi_param); //第一个参数是用户名，第二个参数是密码，第三个参数是登录参数，用户名和密码可在http://www.xfyun.cn注册获取
    if (MSP_SUCCESS != ret) {
        QString msg = QStringLiteral("登录失败，错误代码：");
        msg += QString::number(ret);
        active_monitor()->error(msg);
    }
    _state=init;
    UserData asr_data;
    ret = build_grammar(&asr_data);  //第一次使用某语法进行识别，需要先构建语法网络，获取语法ID，之后使用此语法进行识别，无需再次构建
    //离线语法识别参数设置
    _asr_params="engine_type = local, asr_res_path = "+ASR_RES_PATH+", sample_rate ="+QString::number(16000)+"grm_build_path ="
            +GRM_BUILD_PATH+", local_grammar ="+udata->grammar_id+", result_type = xml, result_encoding = GB2312,";


}
int build_grm_cb(int ecode, const char *info, void *udata)
{
    UserData *grm_data = (UserData *)udata;

    if (NULL != grm_data) {
        grm_data->build_fini = 1;
        grm_data->errcode = ecode;
    }

    if (MSP_SUCCESS == ecode && NULL != info) {
        QString msg=QString::fromLocal8Bit("构建语法成功！ 语法ID:");
        msg+=info;
        qDebug()<<msg<<endl;
        if (NULL != grm_data)
            grm_data->grammar_id=info;
            //_snprintf(grm_data->grammar_id, MAX_GRAMMARID_LEN - 1, info);
    }
    else
        QString msg=QString::fromLocal8Bit("构建语法失败！ 代码:");
        msg+=QString::number(ecode);
        qDebug()<<msg<<endl;

    return 0;
}

int build_grammar(UserData *udata)
{
    FILE *grm_file                           = NULL;
    char *grm_content                        = NULL;
    unsigned int grm_cnt_len                 = 0;
    char grm_build_params[MAX_PARAMS_LEN]    = {NULL};
    int ret                                  = 0;

    grm_file = fopen(GRM_FILE, "rb");
    if(NULL == grm_file) {
        printf("打开\"%s\"文件失败！[%s]\n", GRM_FILE, strerror(errno));
        return -1;
    }

    fseek(grm_file, 0, SEEK_END);
    grm_cnt_len = ftell(grm_file);
    fseek(grm_file, 0, SEEK_SET);

    grm_content = (char *)malloc(grm_cnt_len + 1);
    if (NULL == grm_content)
    {
        printf("内存分配失败!\n");
        fclose(grm_file);
        grm_file = NULL;
        return -1;
    }
    fread((void*)grm_content, 1, grm_cnt_len, grm_file);
    grm_content[grm_cnt_len] = '\0';
    fclose(grm_file);
    grm_file = NULL;

    QString grmBuildParams="engine_type = local, asr_res_path ="+ASR_RES_PATH+", sample_rate ="
            +QString::fromLocal8Bit(SAMPLE_RATE_16K)+",grm_build_path = "+GRM_BUILD_PATH;
   QByteArray ba=grmBuildParams.toLocal8Bit();
    ret = QISRBuildGrammar("bnf", grm_content, grm_cnt_len, ba.data(), build_grm_cb, udata);

    free(grm_content);
    grm_content = NULL;

    return ret;
}

AudioMonitor::~AudioMonitor()
{
    MSPLogout(); //退出登录
}

void AudioMonitor::stop()
{
    _audio_stat = MSP_AUDIO_SAMPLE_LAST;
    qDebug() << "monitor stopping";
}

void AudioMonitor::on_notify(qint64 bytes)
{
    static bool first = true;
    static int count = 0;
    qDebug() << "on notify: received " << bytes << " bytes";
    qDebug() << "buf size: " << _buf->size() << " bytes";


    if (_buf->size() < 10 * FRAME_LEN)
        return;

    if (first)
    {
        _audio_stat = MSP_AUDIO_SAMPLE_FIRST;
        first = false;
    }

    qDebug() << "csid=" << _session_id << ", count=" << count++ << ", aus=" << _audio_stat;
    int err_code = QIVWAudioWrite(_session_id, (const void *)_buf->data().constData(), 10 * FRAME_LEN, _audio_stat);
    _buf->buffer().remove(0, 10 * FRAME_LEN);
    _buf->seek(0);
    if (MSP_SUCCESS != err_code) {
        QString msg("QIVWAudioWrite failed! error code:");
        msg += QString::number(err_code);
        active_monitor()->error(msg);
        if (NULL != _session_id)
        {
            QIVWSessionEnd(_session_id, msg.toLocal8Bit().data());
        }
        return;
    }
    if (_audio_stat == MSP_AUDIO_SAMPLE_LAST) {
        QIVWSessionEnd(_session_id, "success");
        emit stopped();
        qDebug() << "monitor: emit stop signal";
    } else {
        _audio_stat = MSP_AUDIO_SAMPLE_CONTINUE;
    }
}

void AudioMonitor::run()
{
    int err_code = MSP_SUCCESS;
    _session_id=QIVWSessionBegin(nullptr, ssb_param, &err_code);
    if (err_code != MSP_SUCCESS) {
        QString msg("QIVWSessionBegin failed! error code: ");
        msg += QString::number(err_code);
        active_monitor()->error(msg);
        return;
    }

    err_code = QIVWRegisterNotify(_session_id, cb_ivw_msg_proc, nullptr);
    if (err_code != MSP_SUCCESS) {
        QString msg("QIVWRegisterNotify failed! error code: ");
        msg += QString::number(err_code);
        active_monitor()->error(msg);
        if (NULL != _session_id)
        {
            QIVWSessionEnd(_session_id, msg.toLocal8Bit().data());
        }
        return;
    }
}

int cb_ivw_msg_proc( const char *sessionID, int msg, int param1, int param2, const void *info, void *userData )
{
    if (MSP_IVW_MSG_ERROR == msg) //唤醒出错消息
    {
        QString msg("MSP_IVW_MSG_ERROR errCode = ");
        msg += QString::number(param1);
        active_monitor()->error(msg);
    }
    else if (MSP_IVW_MSG_WAKEUP == msg) //唤醒成功消息
    {
        QString msg("MSP_IVW_MSG_WAKEUP result = ");
        msg += (char*)info;
        active_monitor()->awaken(msg);
        _state=awaken;

    }
    return 0;
}
 void startRecognize()
 {

 }
static AudioMonitor* s_monitor;
void set_active_monitor(AudioMonitor *mon)
{
    s_monitor = mon;
}

AudioMonitor *active_monitor()
{
    return s_monitor;
}
