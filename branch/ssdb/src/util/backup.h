/*
 * backup.h
 *
 *  Created on: Sep 21, 2017
 *      Author: zhangpeng
 */

#ifndef SSDB_SRC_SSDB_BACKUP_H_
#define SSDB_SRC_SSDB_BACKUP_H_

#include "../util/config.h"
#include "../util/log.h"
#include "../util/file.h"
#include "../slave.h"

#include <string>

#include <pthread.h>

struct file_properties
{
    pthread_mutex_t          * mutex;
    int                        index;
    std::vector<std::string> * files;
    std::string                dst_dir_path;
};

class BackupServer
{
public:
    BackupServer();
    virtual ~BackupServer();
public:
    void init(Config *conf, volatile bool * is_runing);
    void start();
    void stop();
    void destory();
    static BackupServer * get_instance();
public:
    bool get_pre_backup();
    void set_is_network_over();
    bool get_is_network_over();
    pthread_mutex_t * get_mutex();
    pthread_cond_t  * get_cond();
    void static copy_files(const std::string& dst, FOLDER& f);
public:
    static void *work_thread(void *data);
    static void *copy_thread(void *data);
public:
    volatile bool is_backup;
    volatile bool is_work;
    volatile bool is_network_over;
    volatile bool *is_runing;
    volatile bool pre_backup;
    std::string   work_dir;
    std::string   backup_dir;
    time_t        last_time;

private:
    pthread_t       thread_id;
    pthread_cond_t  cond;
    pthread_mutex_t mutex;
private:
    static BackupServer    *instance;

};

#endif /* SSDB_SRC_SSDB_BACKUP_H_ */
