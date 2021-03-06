// Copyright (C) 2014 - 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn>

#include "jadedbus.h"
#include "globaldeclarations.h"
#include "util.h"

//begin
#include <QCoreApplication>
#include <QObject>
#include <iostream>
#include <string>
#include <QString>
#include <QTimer>
#include <unistd.h>
#include "globallist.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* ICON_CACHE_DIR && struct icon_cache_info alse defined in daemon.cpp */
#define  ICON_CACHE_DIR "/var/cache/isoftapp/qjade"
struct icon_cache_info {
    char name[128];
    char icon[512];
    char title[128];
    char desc[512];
    char cate[128];
};

QList <TaskQueue> m_taskQueue;
static QString g_doingPkgName ="";
/* The following enums defined in src/daemon.h;*/
typedef enum {
    STATUS_INSTALL,
    STATUS_UPGRADE,
    STATUS_REMOVE,
    STATUS_INSTALLED,
    STATUS_REMOVED,
    STATUS_UPDATED,
    STATUS_UPGRADED,
    STATUS_SEARCHED,
    STATUS_INSTALL_ERROR,
    STATUS_REMOVE_ERROR,
    STATUS_UPGRADE_ERROR,
    STATUS_UNKNOWN,
} Status;

typedef enum {
    INSTALLED,
    UNINSTALLED,
    SEARCH_UNKNOWN,
} SearchStatus;


typedef enum {
    ERROR_FAILED,
    ERROR_PERMISSION_DENIED,
    ERROR_NOT_SUPPORTED,
    ERROR_SEARCH,
    ERROR_TASK_LOCKED,
    ERROR_CHECK,
    ERROR_INSTALL,
    ERROR_REMOVE,
    ERROR_UPDATE,
    ERROR_PKG_NOT_EXIST,
    ERROR_PKG_NOT_REMOVED,
    ERROR_PKG_NOT_UPGRADED,
    NUM_ERRORS
} Error;

typedef enum {
    ERROR_CODE_NETWORK,
    ERROR_CODE_DOWNLOAD,
    ERROR_CODE_INSTALL,
    ERROR_CODE_REMOVE,
    ERROR_CODE_SEARCH,
    ERROR_CODE_UPDATEING,
    ERROR_CODE_OTHERS
} ErrorCode;
//end

static bool g_hasInited = false;
static org::isoftlinux::Isoftapp *m_isoftapp = Q_NULLPTR;
JadedBus::JadedBus(QObject *parent) 
  : QObject(parent)
  ////  m_jaded(NULL)
{
#if 0
    m_jaded = new cn::com::isoft::JadedInterface("cn.com.isoft.Jaded", "/Jaded", 
            QDBusConnection::sessionBus(), this);
#endif

    if (!m_isoftapp) {
    m_isoftapp = new org::isoftlinux::Isoftapp("org.isoftlinux.Isoftapp",
                                                   "/org/isoftlinux/Isoftapp",
                                                   QDBusConnection::systemBus());
    printf("trace:%s,%d.[m_isoftapp = new org::isoftlinux::Isoftapp]\n",__FUNCTION__,__LINE__);

    }

    connect(m_isoftapp,
                     &org::isoftlinux::Isoftapp::PercentChanged,
                     this,
                     &JadedBus::percentChged);

    connect(m_isoftapp,
                     &org::isoftlinux::Isoftapp::SearchChanged,
                     this,
                     &JadedBus::searchChged);

    connect(m_isoftapp,
            &org::isoftlinux::Isoftapp::ListChanged,
            this,
            &JadedBus::listChged);

    connect(m_isoftapp,
            &org::isoftlinux::Isoftapp::Finished,
            this,
            &JadedBus::getFinished);

    connect(m_isoftapp,
            &org::isoftlinux::Isoftapp::ListAllChanged,
            this,
            &JadedBus::getAllPkgList);
    connect(m_isoftapp,
            &org::isoftlinux::Isoftapp::Error,
            this,
            &JadedBus::errorChged);

    connect(m_isoftapp,
            &org::isoftlinux::Isoftapp::SettingsChanged,
            this,
            &JadedBus::getSettingChanged);

    if (!g_hasInited) {
        connect(m_isoftapp,
                &org::isoftlinux::Isoftapp::ListUpdateFinished,
                this,
                &JadedBus::getUpdateFinished);

        usleep(500);
        m_isoftapp->ListAll("pkgs");
        usleep(500);

        g_hasInited = true;

        runTaskTimer = new QTimer(this);
        connect(runTaskTimer, SIGNAL(timeout()), this, SLOT(runTaskTimeOut()));
        runTaskTimer->start(20000);

        getPkgListTimer = new QTimer(this);
        connect(getPkgListTimer, SIGNAL(timeout()), this, SLOT(getPkgListTimeOut()));
        getPkgListTimer->start(1000);


        getIconTimer= new QTimer(this);
        connect(getIconTimer, SIGNAL(timeout()), this, SLOT(getIconTimeOut()));
        getIconTimer->start(30000);


        m_isoftapp->GetPathMode();

    }

#if 0
    connect(m_jaded, 
            &cn::com::isoft::JadedInterface::errored, 
            this, 
            &JadedBus::m_errored);
    connect(m_jaded,                                                               
            &cn::com::isoft::JadedInterface::getUpdateFinished,
            this,                                                                  
            &JadedBus::getUpdateFinished);
    connect(m_jaded,                                                               
            &cn::com::isoft::JadedInterface::getInstalledFinished,                    
            this,                                                                  
            &JadedBus::getInstalledFinished);
    connect(m_jaded, 
            &cn::com::isoft::JadedInterface::taskStarted, 
            this, 
            &JadedBus::m_taskStarted);
    connect(m_jaded, 
            &cn::com::isoft::JadedInterface::taskFinished, 
            this, 
            &JadedBus::taskFinished);
    connect(m_jaded, 
            &cn::com::isoft::JadedInterface::taskQueueChanged, 
            this, 
            &JadedBus::m_taskQueueChanged);
#endif
}

JadedBus::~JadedBus() 
{
#if 0
    if (m_jaded) {
        delete m_jaded;
        m_jaded = NULL;
    }
#endif
}


/*
* 接受isoftappdaemon list-all 返回的所有包列表
* 0.const QString &pkgName 的格式：[0abc|1efg|0xyz|1ttt]
* 1.更新所有包列表中对应包的状态
*
*/
void JadedBus::getAllPkgList(const QString &pkgName,qlonglong status)
{
    char name[256]="";
    QStringList pkgList = pkgName.split("|", QString::SkipEmptyParts);

    if (pkgName.isEmpty()) {
        printf("trace:%s,%d. Data from isoftapp daemon error!!!\n",__FUNCTION__,__LINE__);
        return;
    }
    AllPkgList.clear();

    for (int i = 0; i < pkgList.size(); i++ ) {
        QString pkgInfo = pkgList.at(i);
        QStringList infoList = pkgInfo.split(",", QString::SkipEmptyParts);
        t_PKGINFO pkg;
        // [1,nqp,2906867,0,0.0.2015.11-2]
        // [status,name,size,datetime,version|status,...]
        if(infoList.size() == 5 ) {
            snprintf(name,sizeof(name),"%s",qPrintable(infoList[0]));
            pkg.status = atoi(name) ? 2 : 1; // name==[0abc]--abc is installed
            pkg.pkgName = infoList[1];
            pkg.size = infoList[2];
            pkg.datetime = infoList[3];
            pkg.version = infoList[4];

            AllPkgList.append(pkg);
            memset(name,0,sizeof(name));

            #if 0
            printf("trace:%s,%d.get all pkg info[%s][%s][%s][%s] from daemon.\n",__FUNCTION__,__LINE__,
                   qPrintable(pkg.pkgName),
                   qPrintable(pkg.size),
                   qPrintable(pkg.datetime),
                   qPrintable(pkg.version));
            #endif
        }
    }
    return;
}

/*
* 接受isoftappdaemon finished 信号
* 1.更新所有包列表中对应包的状态
* 2.给每个页面发送taskfinished信号
* 3.更新taskqueue队列：
*   a、删除当前任务
*   b、开始新任务
*/
void JadedBus::getFinished(const QString &pkgName,qlonglong status)
{
    if (status != STATUS_REMOVED &&
        status != STATUS_UPDATED &&
        status != STATUS_INSTALLED &&
        status != STATUS_INSTALL &&
        status != STATUS_UPGRADED) {
        return;
    }

    if(pkgName.isEmpty()) {
        return;
    }

    int i =0;
    for (i = 0; i < AllPkgList.size(); ++i) {
        if (AllPkgList.at(i).pkgName  == pkgName) {
            if (status == STATUS_INSTALLED) {
                AllPkgList[i].status = 1;
                QDateTime local(QDateTime::currentDateTime());
                AllPkgList[i].datetime = local.toString("yyyy-MM-dd hh:mm:ss");
            } else if (status == STATUS_REMOVED) {
                AllPkgList[i].status = 2;
                // my pkgs will list it
                QDateTime local(QDateTime::currentDateTime());
                AllPkgList[i].datetime = local.toString("yyyy-MM-dd hh:mm:ss");
            }
            getMyPkgNumber();

            break;
        }
    }

    taskFinished(pkgName);

    for (i = 0; i < m_taskQueue.size(); i++) {
        if (0&&m_taskQueue[i].status == "doing") {
            printf("\n trace %s,%d,pkg[%s][%d] is doing.[%s] \n",__FUNCTION__,__LINE__,
                   qPrintable(m_taskQueue[i].name),i,qPrintable(pkgName));
        }
        if (m_taskQueue[i].status == "doing" &&
            pkgName == m_taskQueue[i].name) {
            printf("trace:%s,%d,name[%s],index[%d] task will be finished.\n",__FUNCTION__,__LINE__,
                   qPrintable(m_taskQueue[i].name),i);
            m_taskQueue[i].status = "done";
            g_doingPkgName = "";
            m_taskQueue.removeFirst();
            // to restart task...
            m_runTask();
            return;
        }
    }

    return;
}

/*
*
* get data from isoftapp daemon and
* update AllPkgList every 8 seconds automatically.
*
*/
void JadedBus::listChged(const QString &pkgName)
{
    if (pkgName.isEmpty()) {
        return;
    }
    bool found = false;
    char buf[256]="";
    snprintf(buf,sizeof(buf),"%s",qPrintable(pkgName));
    int status = atoi(buf);
    QString name((char *)(buf+2));

    for (int i = 0; i < AllPkgList.size(); ++i) {
        if (AllPkgList.at(i).pkgName  == name) {
            found = true;
            AllPkgList[i].status = status ? 1 : 2;
            break;
        }
    }

    if (!found) {
        t_PKGINFO pkg;
        pkg.pkgName = name;
        pkg.status = status ? 1 : 2;
        AllPkgList.append(pkg);
    }
}

void JadedBus::errorChged(qlonglong error, const QString &details, qlonglong errcode)
{
#if 0
    printf("\n trace %s,%d,error[%d]vs[%d] errcode[%d]vs[%d],details[%s] \n",__FUNCTION__,__LINE__,
           error,ERROR_PKG_NOT_UPGRADED,
           errcode ,ERROR_CODE_OTHERS,qPrintable(details));
#endif
    if(error == ERROR_PKG_NOT_UPGRADED) {
        if (errcode == ERROR_CODE_OTHERS) {
            //Not need upgradation
            emit errored(details,QString("lastest"));
        }
        return;
    }

    return;

}


void JadedBus::percentChged(qlonglong status, const QString &file, double percent)
{

    if (status == STATUS_INSTALLED) {
        return;
    } else if (status == STATUS_REMOVED) {
        return;
    }

    int Percent = 100 * percent;

    // use original pkg name for other's percent

    QString name = "";
    if(g_doingPkgName.isEmpty()) {
        name = file.section('-',0,0);
    } else {
        name = g_doingPkgName;
    }
    emit perChanged(name,Percent);

}

void JadedBus::searchChged(const QString &pkgName,qlonglong status)
{
    //todo...
    //printf("\n trace %s,%d,name[%s]vs[%s],[%d]\n",__FUNCTION__,__LINE__,
    //  qPrintable(name),qPrintable(file),Percent );
    //emit perChanged(name,Percent);

}


void JadedBus::m_errored(const QString &name,const QString &detail)
{
    emit errored(name,detail);
}

QString JadedBus::getInfo(QString name) 
{
    //m_isoftapp->Search(qPrintable(name));
    int i = 0,j = 0;
    for (i = 0; i < AllPkgList.size(); ++i) {
        if (AllPkgList.at(i).pkgName == name) {

            int status = AllPkgList.at(i).status;
            if (status == 1) {
                return "InfoInstalled";
            } else if (status == 2) {
                for (j = 0; j < m_taskQueue.size(); j++) {
                    if (name == m_taskQueue[j].name)
                        return "InfoWaiting";
                }
                return "InfoAvailable";
            } else {
                for (j = 0; j < m_taskQueue.size(); j++) {
                    if (name == m_taskQueue[j].name)
                        return "InfoWaiting";
                }
            }
        }
    }

    printf("\n trace %s,%d,name[%s] is unknown\n",__FUNCTION__,__LINE__, qPrintable(name));
    return "UnknownInfo";
    //return m_jaded->getInfo(name);

}

static QString m_updateInfo;

void JadedBus::getUpdateFinished(const QString &pkgInfo)
{

    m_updateInfo = pkgInfo;
    printf("%s,%d,all update pkg list:[%s]\n",__FUNCTION__,__LINE__,qPrintable(m_updateInfo));
#if 0
    foreach (QString str, update) {
        QStringList result = str.split("|");
        if (result.size() != 5) 
            continue;
        QString iconPath = absIconPath(result[2]);
        if (iconPath == "") {
            iconPath = USR_SHARE_ICON_DIR + OXYGEN_THEME + 
                "/48x48/categories/applications-system.png";
        }
        m_updateList.append(new JadedPackageObject(result[0], result[1],
        "file://" + iconPath, result[3], result[4],"category","",""));
    }
    emit updateChanged(m_updateList.size());
#endif

}

void JadedBus::getUpdateTimeout() 
{
    printf("\n trace %s,%d,get upt info[%s]\n",__FUNCTION__,__LINE__,qPrintable(m_updateInfo));
    if (m_updateInfo.isEmpty()) {
        emit getUpdateError();
        return;
    }
    m_updateList.clear();

    // m_updateInfo = [firefox,firefox-41.0.2-15,firefox-43.0-2|]-->from isoftapp-daemon:listUpdate()
    // [pkgname,pre version,lastest version|pkgname,pre v,lastest v]
    QStringList pkgList = m_updateInfo.split("|", QString::SkipEmptyParts);
    for (int i = 0; i < pkgList.size(); i++ ) {
        QString name = "";
        QString preVer = "";
        QString lastestVer = "";
        QString icon = "",size ="",desc = "";
        QString pkgInfo = pkgList.at(i);
        QStringList infoList = pkgInfo.split(",", QString::SkipEmptyParts);
        char str[256]="";
        // [vlc,1.2.0,1.2.9]
        // name,pre version,lastest version
        if(infoList.size() == 3 ) {
            name = infoList[0]; //firefox
            preVer = infoList[1]; //firefox-41.0.2-15 need to remove name from prever
            snprintf(str,sizeof(str),"%s",qPrintable(preVer) + name.length() +1);
            preVer = QString(str);
            lastestVer = infoList[2]; //firefox-43.0-2
            snprintf(str,sizeof(str),"%s",qPrintable(lastestVer) + name.length() +1);
            lastestVer = QString(str);
            // to get icon ... by name
            for (int j = 0; j < g_qjadePkgList.size(); ++j) {
                if (name == g_qjadePkgList.at(j).name) {
                    icon = "",size ="",desc = "";
                    icon = g_qjadePkgList.at(j).icon;
                    desc = g_qjadePkgList.at(j).description;
                    for (int k = 0; k < AllPkgList.size(); ++k) {
                        if (name == AllPkgList.at(k).pkgName) {
                            size = AllPkgList.at(k).size;
                            break;
                        }
                    }
                    break;
                }
            }
        }

        if(name.isEmpty()) {
            printf("\nERROR:%s,%d,can not find pkg name\n",__FUNCTION__,__LINE__);
            continue;
        }

        QString dstSize ="";
        getStrSize(size,dstSize);

        // version:prever;--->used in .qml
        // datetime:lastest ver;-->used in .qml
        m_updateList.append(new JadedPackageObject(QString(i),
            name,icon,preVer,desc,"category",dstSize,lastestVer));

            #if 1
            printf("trace:%s,%d.get update pkg info name[%s]icon[%s],perver[%s]"
                   "last[%s],size[%s]desc[%s] from daemon.\n",
                   __FUNCTION__,__LINE__,
                   qPrintable(name),
                   qPrintable(icon),
                   qPrintable(preVer),
                   qPrintable(lastestVer),
                   qPrintable(size),
                   qPrintable(desc));
            #endif
    }

    printf("\n trace %s,%d,go here,size[%d]\n",__FUNCTION__,__LINE__,m_updateList.size());

    if (m_updateList.size() >0) {
        emit updateChanged(m_updateList.size());
    } else {
        emit getUpdateError();
    }
}

void JadedBus::getUpdate() 
{
    printf("%s,%d,will get upt info[%s]\n",__FUNCTION__,__LINE__,qPrintable(m_updateInfo));

    m_updateInfo = "";
    m_isoftapp->ListUpdate();
    ////m_jaded->getUpdate();
    QTimer::singleShot(6000, this, SLOT(getUpdateTimeout()));
}

void JadedBus::getMyPkgNumTimeout()
{
    int count = 0;
    for (int i = 0; i < AllPkgList.size(); ++i) {
        if(AllPkgList.at(i).datetime == "0") {
            continue;
        }
        for(int j =0;j< g_qjadePkgList.size();j++) {
            if (AllPkgList.at(i).pkgName == g_qjadePkgList.at(j).name) {
                count ++;
            }
        }
    }
    emit myPkgNumChanged(count);
}
void JadedBus::getMyPkgNumber()
{
    QTimer::singleShot(1000, this, SLOT(getMyPkgNumTimeout()));
}

void JadedBus::getInstalledFinished(const QStringList &installed) 
{
    m_installedList.clear();
    foreach (QString str, installed) {
        QStringList result = str.split("|");
        if (result.size() != 5) 
            continue;
        QString iconPath = absIconPath(result[2]);
        if (iconPath == "") 
            continue;
        m_installedList.append(new JadedPackageObject(result[0], result[1],
        "file://" + iconPath, result[3], result[4],"category","",""));
    }
    emit installedChanged();
}

/*
* 卸载页面展示内容：
* 1、从全局列表 g_qjadePkgList 中获取
*/
void JadedBus::getInstalledTimeout() 
{
    int i = 0,j = 0;
    bool isUninstalled = false,isNotExist = true;
    QString datetime = "",size ="";
    for (i = 0; i < g_qjadePkgList.size(); ++i) {
        isUninstalled = false;
        isNotExist    = true;
        for (j = 0; j < AllPkgList.size(); ++j) {
            if (AllPkgList.at(j).pkgName == g_qjadePkgList.at(i).name) {
                datetime = AllPkgList.at(j).datetime;
                size = AllPkgList.at(j).size;

                isNotExist = false;
                int status = AllPkgList.at(j).status;
                if (status == 2) {
                    isUninstalled = true;
                    break;
                }
            }
        }

        if (isUninstalled || isNotExist) {
            continue;
        }

        QString dstSize ="";
        getStrSize(size,dstSize);
        m_installedList.append(new JadedPackageObject(QString(i),
                                       g_qjadePkgList.at(i).name,
                                       g_qjadePkgList.at(i).icon,
                                       g_qjadePkgList.at(i).title, //version is not used in qml.
                                       g_qjadePkgList.at(i).description,
                                       g_qjadePkgList.at(i).category,
                                       dstSize,
                                       datetime) );
    }

    // offline:get png and other info from local file
    if (m_installedList.size() < 1) {
        m_installedList.clear();

        for (j = 0; j < AllPkgList.size(); ++j) {
            char cfg_file[512]="";
            snprintf(cfg_file,sizeof(cfg_file),
                     "%s/.%s.cfg",ICON_CACHE_DIR,qPrintable(AllPkgList.at(j).pkgName) );
            QFile file_cfg(cfg_file);
            if (!file_cfg.exists()) {
                continue;
            }
            datetime = AllPkgList.at(j).datetime;
            size = AllPkgList.at(j).size;
            int status = AllPkgList.at(j).status;
            if (status == 2) {
                continue;
            }

            int fd = open(cfg_file,O_RDONLY);
            if (fd < 1) {
                continue;
            }

            struct icon_cache_info cfg_info;
            memset(&cfg_info,0,sizeof(struct icon_cache_info));
            read(fd,&cfg_info,sizeof(struct icon_cache_info));
            close(fd);

            QString dstSize ="";
            getStrSize(size,dstSize);

            m_installedList.append(new JadedPackageObject(QString(j),
                                           cfg_info.name,
                                           cfg_info.icon,
                                           cfg_info.title,
                                           cfg_info.desc,
                                           cfg_info.cate,
                                           dstSize,
                                           datetime) );
        }
    }



    printf("\n trace###### %s,%d,go here,jade[%d],all[%d]\n",__FUNCTION__,__LINE__,
           g_qjadePkgList.size(),AllPkgList.size());
    emit installedChanged();

    if (m_installedList.size() == 0)
        emit getInstalledError();
}

void JadedBus::getInstalled() 
{
    ////m_jaded->getInstalled();
    QTimer::singleShot(6000, this, SLOT(getInstalledTimeout()));
}

void JadedBus::searchFinished(const QStringList &search)                     
{                                                                                  
    if (m_searchList.size()) {
        for (int i = 0; i < m_searchList.size(); i++) {
            delete m_searchList[i];
            m_searchList[i] = NULL;
        }
        m_searchList.clear();
    }
    foreach (QString str, search) {                                                
        QStringList result = str.split("|");                                      
        if (result.size() != 5)                                                   
            continue;                                                              
        QString iconPath = absIconPath(result[2]);                                
        if (iconPath == "") {                                                      
            iconPath = USR_SHARE_ICON_DIR + OXYGEN_THEME +                         
                "/48x48/categories/applications-system.png";                       
        }
        m_searchList.append(new JadedPackageObject(result[0], result[1],
                    "file://" + iconPath, result[3], result[4],"category","",""));
    }
    emit searchChanged(m_searchList.size());
}

void JadedBus::searchTimeout() 
{
    if (m_searchList.size() == 0)
        emit searchError();
}

void JadedBus::search(QString name) 
{
#if 0
    connect(m_jaded,                                                               
            &cn::com::isoft::JadedInterface::searchFinished,                       
            this,                                                                  
            &JadedBus::searchFinished);
    m_jaded->search(name);
#endif
    QTimer::singleShot(6000, this, SLOT(searchTimeout()));
}

void JadedBus::m_taskStarted(const QString &name) 
{
    emit taskStarted(name);
}


void JadedBus::taskFinished(const QString &name) 
{
    //to call () getAllPkgList
    emit taskChanged(name);
}

void JadedBus::m_taskQueueChanged(const QStringList &task) 
{
#if 0
    if (m_oldQueue.size()) {
        for (int i = 0; i < m_oldQueue.size(); i++) {
            if (m_oldQueue[i]) {
                delete m_oldQueue[i];
                m_oldQueue[i] = NULL;
            }
        }
        m_oldQueue.clear();
    }
    foreach (QString str, task) {
        QStringList result = str.split("|");
        if (result.size() != 5)
            continue;

        QString iconPath = absIconPath(result[2]);
        if (iconPath == "" || iconPath == "lib") {
            iconPath = USR_SHARE_ICON_DIR + OXYGEN_THEME +
                "/48x48/categories/applications-system.png";
        }

        m_oldQueue.append(new JadedPackageObject(result[0], result[1],
                    "file://" + iconPath, result[3], result[4],"category","",""));
    }
#endif
    m_oldQueue.clear();

    for (int i = 0; i < m_taskQueue.size(); i++) {
        if (1||m_taskQueue[i].status == "doing") {
            printf("trace:%s,%d,name[%s] is already in queue.\n",__FUNCTION__,__LINE__,
                   qPrintable(m_taskQueue[i].name));
            QString icon ="";
            QString desc ="";
            for(int j = 0;j <g_qjadePkgList.size();j++) {
                if (m_taskQueue[i].name == g_qjadePkgList.at(j).name) {
                    icon = "",desc = "";
                    icon = g_qjadePkgList.at(j).icon;
                    desc = g_qjadePkgList.at(j).description;
                    break;
                }
            }

            m_oldQueue.append(new JadedPackageObject(QString(i), m_taskQueue[i].name,
                icon,"version xx",desc,"category","",m_taskQueue[i].status));
        }
    }

    emit taskQueueChanged(m_oldQueue.size());
}

bool JadedBus::m_isTaskExist(QString name)
{
    if (name.isEmpty())
        return false;

    for (int i = 0; i < m_taskQueue.size(); i++) {
        if (m_taskQueue[i].name == name) {
            printf("trace:%s,%d,name[%s] is already in queue.\n",__FUNCTION__,__LINE__,qPrintable(name));
            return true;
        }
    }
    return false;
}

void JadedBus::m_runTask()
{
    if (m_taskQueue.size() == 0)
        return;

    QString name = m_taskQueue[0].name;
    QString action = m_taskQueue[0].action;
    QString status = m_taskQueue[0].status;
    if (status == "doing")
        return;

    m_taskQueue[0].status = "doing";
    g_doingPkgName = name;

    if (action == "install") {
        for (int i = 0; i < AllPkgList.size(); ++i) {
            if (AllPkgList.at(i).pkgName == name) {
                int ret = AllPkgList.at(i).status;
                if (ret == 1) {
                    printf("trace:%s,%d,name[%s] has already been installed.\n",__FUNCTION__,__LINE__,qPrintable(name));
                    // todo return???
                }

            }
        }
        printf("%s,%d, will install [%s].\n",__FUNCTION__,__LINE__,qPrintable(name));
        m_isoftapp->Install(qPrintable(name));
    } else if (action == "installfile") {
        //m_pkt->installFile(id, true);
    } else if (action == "update") {
        printf("%s,%d, will upgrade [%s].\n",__FUNCTION__,__LINE__,qPrintable(name));
        m_isoftapp->Upgrade(qPrintable(name));
        //m_pkt->updatePackage(package);
    } else if (action == "uninstall") {
        printf("%s,%d, will uninstall [%s].\n",__FUNCTION__,__LINE__,qPrintable(name));
        m_isoftapp->Remove(qPrintable(name),false);
    }

    m_taskStarted(name);
}

/*
* 命令行执行 isoftapp search/install/remove xxx
* 会干扰 dbus，因此采用定期重复执行的策络
* todo: 为 m_taskQueue 加锁
*/
void JadedBus::runTaskTimeOut()
{
    if (m_taskQueue.size() == 0)
        return;

    QString name = m_taskQueue[0].name;
    QString action = m_taskQueue[0].action;
    QString status = m_taskQueue[0].status;
    if (status != "doing")
        return;

    if (action == "install") {
        for (int i = 0; i < AllPkgList.size(); ++i) {
            if (AllPkgList.at(i).pkgName == name) {
                int ret = AllPkgList.at(i).status;
                if (ret == 1) {
                    printf("trace:%s,%d,name[%s] has already been installed.\n",
                           __FUNCTION__,__LINE__,qPrintable(name));
                    // todo return???
                }
            }
        }
        printf("%s,%d, will install [%s].\n",__FUNCTION__,__LINE__,qPrintable(name));
        m_isoftapp->Install(qPrintable(name));
    } else if (action == "installfile") {
        //m_pkt->installFile(id, true);
    } else if (action == "update") {
        printf("%s,%d, will upgrade [%s].\n",__FUNCTION__,__LINE__,qPrintable(name));
        m_isoftapp->Upgrade(qPrintable(name));
        //m_pkt->updatePackage(package);
    } else if (action == "uninstall") {
        printf("%s,%d, will uninstall [%s].\n",__FUNCTION__,__LINE__,qPrintable(name));
        m_isoftapp->Remove(qPrintable(name),false);
    }


}
int g_getPkgListCounter = 0;
void JadedBus::getPkgListTimeOut()
{
    g_getPkgListCounter ++;
    if (g_getPkgListCounter < 20) {
        if (g_getPkgListCounter %3 != 1) {
            return;
        }
    } else if (g_getPkgListCounter < 500) {
        if (g_getPkgListCounter %60 != 59 ) {
            return;
        }
    } else if (g_getPkgListCounter < 5000) {
        if (g_getPkgListCounter %501 != 500 ) {
            return;
        }
    } else {
        if (g_getPkgListCounter %2001 != 2000 ) {
            return;
        }
    }
    m_isoftapp->ListAll("pkgs");
}

/*
* get icon files from server, save *.png to /var/cache/isoftapp/qjade/.
* first-run after 30 seconds;
* then will run every 30 minutes;
*/
static int g_getIconCounter = 0;
void JadedBus::getIconTimeOut()
{
    g_getIconCounter ++;
    if (g_getIconCounter == 1) {
        if (g_qjadePkgList.size() < 1 || AllPkgList.size() <1) {
            g_getIconCounter = 0;
            return;
        }
        int i =0,j=0;
        for (i = 0; i < g_qjadePkgList.size(); ++i) {
            for (j = 0; j < AllPkgList.size(); ++j) {
                if (AllPkgList.at(j).pkgName == g_qjadePkgList.at(i).name) {
                    QString icon_path(ICON_CACHE_DIR);
                    icon_path += "/";
                    icon_path += g_qjadePkgList.at(i).name;
                    icon_path += ".png";
                    QFile file(icon_path);

                    if (file.exists()) {
                        QString cfg(ICON_CACHE_DIR);
                        cfg += "/.";
                        cfg += g_qjadePkgList.at(i).name;
                        cfg += ".cfg";

                        //printf("trace:%s,%d,cfg path[%s]\n",__FUNCTION__,__LINE__,qPrintable(cfg) );

                        QFile file_cfg(cfg);
                        if (file_cfg.exists()) {
                            continue;
                        }
                    }

                    m_isoftapp->GetIcons(g_qjadePkgList.at(i).name,
                                         g_qjadePkgList.at(i).icon,
                                         g_qjadePkgList.at(i).title,
                                         g_qjadePkgList.at(i).description,
                                         g_qjadePkgList.at(i).category);
                }
            }
        }
    } else if (g_getIconCounter > 1 && g_getIconCounter < 10) {
        return;
    } else {
        g_getIconCounter = 0;
    }
}

void JadedBus::setPathMode(QString path,QString mode)
{
    printf("trace:%s,%d,recieved:[%s][%s]\n",__FUNCTION__,__LINE__,qPrintable(path),qPrintable(mode));
    m_isoftapp->SetPathMode(qPrintable(path),qPrintable(mode));
}


void JadedBus::runCmd(QString cmd)
{
    QProcess *runProcess = new QProcess();
    runProcess->start(cmd);

    printf("trace:%s,%d,to run[%s]\n",__FUNCTION__,__LINE__,qPrintable(cmd) );
}

void JadedBus::install(QString name) 
{
    printf("trace:%s,%d, to install [%s]m[%d]\n",__FUNCTION__,__LINE__,
           qPrintable(name),m_taskQueue.size());

    if (m_isTaskExist(name))
        return;

    m_taskQueue.append(TaskQueue(name, "install"));
    if (m_taskQueue.size() == 1)
        m_runTask();

    return;

    //m_jaded->install(name);
}

void JadedBus::installFile(QString filePath) 
{
    ////m_jaded->installFile(filePath);
}

void JadedBus::uninstall(QString name)
{
    ////m_jaded->uninstall(id);

    printf("trace:%s,%d, to uninstall [%s]m[%d]\n",__FUNCTION__,__LINE__,
           qPrintable(name),m_taskQueue.size());

    if (m_isTaskExist(name))
        return;

    m_taskQueue.append(TaskQueue(name, "uninstall"));
    if (m_taskQueue.size() == 1)
        m_runTask();

    return;

    //emit taskQueueChanged(m_oldQueue.size());
}

void JadedBus::update(QString name) 
{
    printf("trace:%s,%d, to upgrade [%s]m[%d]\n",__FUNCTION__,__LINE__,
           qPrintable(name),m_taskQueue.size());

    if (m_isTaskExist(name))
        return;

    m_taskQueue.append(TaskQueue(name, "update"));
    if (m_taskQueue.size() == 1)
        m_runTask();
    ////m_jaded->update(name);
}

void JadedBus::cancel(QString name) 
{
    for (int i = 0; i < m_taskQueue.size(); i++) {
        if (m_taskQueue[i].name == name && m_taskQueue[i].status != "doing") {
            m_taskQueue.removeAt(i);
            break;
        }
    }
    ////_jaded->cancel(name);
}

void JadedBus::getTaskQueue() 
{
    const QStringList task;
    m_taskQueueChanged(task);
    ////m_jaded->getTaskQueue();
}

void JadedBus::getSettingChanged(const QString &pathMode)
{
    char path_mode[600]="";
    QStringList result = pathMode.split("|||");
    if (result.size() <2) {
        printf("error:%s,%d, get path_mode error\n",__FUNCTION__,__LINE__);
        return;
    }
    bool ok=false;
    emit settingChanged("file://" + result[0],result[1].toInt(&ok,10) );
}

void JadedBus::getPathMode()
{
    m_isoftapp->GetPathMode();

    printf("trace:%s,%d\n",__FUNCTION__,__LINE__);
}

