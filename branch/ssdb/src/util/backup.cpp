/*
 * backup.cpp
 *
 *  Created on: Sep 21, 2017
 *      Author: zhangpeng
 */

#include "backup.h"

BackupServer *BackupServer::instance = NULL;

BackupServer::BackupServer()
 :is_backup(false),
 is_work(true),
 is_network_over(false),
 is_runing(NULL),
 pre_backup(false),
 work_dir(""),
 backup_dir("")
{
    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_settype(&mutexattr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex,&mutexattr);
    pthread_mutexattr_destroy(&mutexattr);

    pthread_cond_init(&cond, NULL);

    struct tm tp;
    last_time = time(NULL);
    localtime_r(&last_time, &tp);
    tp.tm_hour = 0;
    last_time = mktime(&tp);
    last_time = last_time - last_time % 3600;
}

BackupServer::~BackupServer()
{
}


void BackupServer::init(Config *conf, volatile bool * is_runing)
{
    //init backup_dir
    backup_dir.assign(conf->get_str("backup_dir"));
    if(backup_dir.empty()){
        log_warn("[backup] backup_dir is empty");
        is_backup = false;
    }else{
        is_backup = true;
        if(!is_dir(backup_dir.c_str())){
            fprintf(stderr, "backup_dir '%s' is not a directory or not exists!\n", backup_dir.c_str());
            exit(1);
        }
    }

    this->is_runing = is_runing;
    work_dir   = conf->get_str("work_dir");
}

void BackupServer::start()
{
    pthread_create(&thread_id, NULL, work_thread, (void *)BackupServer::get_instance());
}

void BackupServer::stop()
{
    is_work = false;
    pthread_join(thread_id, NULL);
}

void BackupServer::copy_files(const std::string& dst, FOLDER& all_files)
{

    std::string dst_path = dst;

    if(!all_files.dir_name.empty())
    {
        dst_path = dst + "/" + all_files.dir_name;
        std::string cmd = "mkdir -p " + dst_path;
        if(system(cmd.c_str()) == -1)
        {
            return;
        }
    }


    if(all_files.file_paths.empty())
    {
        return;
    }

    pthread_mutexattr_t mutexattr;
    pthread_mutex_t     mux;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mux,&mutexattr);

    struct file_properties f;
    f.index    = 0;
    f.files    = &(all_files.file_paths);
    f.mutex    = &mux;
    f.dst_dir_path = dst_path;

    int cpu_num = sysconf(_SC_NPROCESSORS_ONLN);
    int thread_num = cpu_num < all_files.file_paths.size() ? cpu_num:all_files.file_paths.size();

    pthread_t tid[thread_num];

    for(int i = 0; i < thread_num; i++)
    {
        pthread_create(&tid[i], NULL, copy_thread, &f);
    }

    for(int i = 0; i < thread_num; i++)
    {
        void *p = NULL;
        pthread_join(tid[i], &p);
    }

    pthread_mutexattr_destroy(&mutexattr);
    pthread_mutex_destroy(&mux);

}

void * BackupServer::copy_thread(void *data)
{
    if(!data) return (void *)NULL;

    file_properties *f = (file_properties *)data;

    std::string src_file, dst_file;
    int file_size = f->files->size();

    while(1)
    {
        pthread_mutex_lock(f->mutex);
        int index = f->index++;
        pthread_mutex_unlock(f->mutex);

        if(index >= file_size) return (void *)NULL;

        src_file = (*f->files)[index];
        dst_file = f->dst_dir_path + "/" + basename((char *)src_file.c_str());
        copy_file(dst_file, src_file);
    }

    return (void * ) NULL;
}

void * BackupServer::work_thread(void *data)
{
    if(!data) return (void *)NULL;

    BackupServer *p = (BackupServer *) data;

    while (p->is_work) {

// for test
//        if((p->is_runing && (*(p->is_runing) == true)) && p->is_backup)
        if(!p->is_runing || !*p->is_runing || !p->is_backup)
        {
            sleep(10);
            continue;
        }

        time_t t = time(NULL);
        std::string backup_dir = p->backup_dir;
        std::string work_dir = p->work_dir;
        if(t - p->last_time >= 86400)
        {
            log_debug("now: %lu, last_time::%lu", t, p->last_time);
            p->pre_backup = true;
        }
        else
        {
            log_debug("pre_backup false");
        }

        if (p->is_network_over && p->pre_backup) {
            //backup
            std::string cmd = "rm -fr " + backup_dir + "/*";
            log_debug("cmd:%s", cmd.c_str());
            system(cmd.c_str());


            std::string src_dir_data =  work_dir + "/data";
            std::string dst_dir_data =  backup_dir + "/data";

            std::string src_dir_meta =  work_dir + "/meta";
            std::string dst_dir_meta =  backup_dir + "/meta";

            FOLDERS all_data_files;
            FOLDERS all_meta_files;

            get_all_files(src_dir_data, all_data_files);
            get_all_files(src_dir_meta, all_meta_files);

            for(FOLDER_ITER iter = all_data_files.begin(); iter != all_data_files.end(); iter++)
            {
                copy_files(dst_dir_data, *iter);
            }

            for(FOLDER_ITER iter = all_meta_files.begin(); iter != all_meta_files.end(); iter++)
            {
                copy_files(dst_dir_meta, *iter);
            }


            cmd = "cp -r " + work_dir + "/data " + work_dir + "/meta " + backup_dir;
            log_debug("cmd:%s", cmd.c_str());
            system(cmd.c_str());

            p->is_network_over = false;
            p->pre_backup      = false;
            p->last_time = t;
            p->last_time = p->last_time - p->last_time % 3600;


            std::string finsish_file = work_dir + "/finish.txt";
            FILE *f = fopen(finsish_file.c_str(), "w+");
            if(f)
            {

                char data[32];
                snprintf(data, sizeof(data), "%lu", (uint64_t)p->last_time);
                fwrite(data, strlen(data), 1, f);
                fflush(f);
                fclose(f);
            }
            else
            {
                log_error("finish file create error!");
            }

            pthread_cond_signal(&(p->cond));
        }
        sleep(2);
    }

    return (void *)NULL;
}

bool BackupServer::get_pre_backup()
{
    return pre_backup;
}
void BackupServer::set_is_network_over()
{
    is_network_over = true;
}

bool BackupServer::get_is_network_over()
{
    return is_network_over;
}

pthread_mutex_t * BackupServer::get_mutex()
{
    return &mutex;
}

pthread_cond_t * BackupServer::get_cond()
{
    return &cond;
}

BackupServer * BackupServer::get_instance()
{
    if(!instance)
    {
        instance = new BackupServer();
    }
    return instance;
}

void BackupServer::destory()
{
    if(instance)
    {
        delete instance;
        instance = NULL;
    }
}
