#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <list>
#include <string>
#include <circle/spinlock.h>

class logger_t
{
private:
    CSpinLock lock;
    typedef struct logger_entry {
        std::string timestamp;
        std::string message;
    } logger_entry_t;
    size_t len;
    int boot;
    static std::string bmsg;
    static std::string msg;
    std::list<logger_entry_t> logs;
    std::list<logger_entry_t> bootlogs;
    void consolidate(std::list<logger_entry_t> &l, std::string &m, bool html = true);
public:
    logger_t(size_t length = 1000, int boot = 4);
    ~logger_t() = default;
    void log(const char *message, const char* t = nullptr);
    void finished_booting(const char *func);
    size_t get_log_count() const { return logs.size(); }
    const std::string& get_logs(bool html = true) { consolidate(logs, msg, html); return msg; }
    const std::string& get_bootlogs(bool html = true) { consolidate(bootlogs, bmsg, html); return bmsg; }
};

extern logger_t logger;
#endif