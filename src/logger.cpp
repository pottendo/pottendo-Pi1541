//
// logger.cpp
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// written by pottendo

#include <string>
#include "logger.h"
#include "time.h"

std::string logger_t::bmsg;
std::string logger_t::msg;

logger_t::logger_t(size_t l, int b)
    : len(l), boot(b)
{
    // Constructor code (if any)
    bmsg.clear();
    msg.clear();
}

void logger_t::log(const char *message, const char* t)
{
    // Logging implementation (e.g., write to file, console, etc.)
    if (t == nullptr) {
        // Get current time as string
        time_t now = time(0);
        struct tm tstruct;
        char buf[80];
        tstruct = *localtime(&now);
        strftime(buf, sizeof(buf), "%b %e %X...", &tstruct);
        t = buf;
    }
    lock.Acquire();
    if (boot) {
        bootlogs.push_back(logger_entry_t{std::string(t), std::string(message)});
    } else {
           logs.push_back(logger_entry_t{std::string(t), std::string(message)});
        if (logs.size() > len)
            logs.pop_front();
    }
    lock.Release();
}

void logger_t::consolidate(std::list<logger_entry_t> &l, std::string &m, bool html)
{
    // Consolidate boot logs into a single string
    lock.Acquire();
    m.clear();
    if (html) 
    {
        m = "<table class=\"dirs\"><tr><th>Time</th><th>Message</th>";
        for (const auto& entry : l) {
            m += "<tr><td>" + entry.timestamp + "</td><td>" + entry.message + "</td></tr>";
        }
        m += "</table>";
    } else {
        for (const auto& entry : l) {
            m += entry.timestamp + "\t" + entry.message + "\n";
        }   
    }
    lock.Release();
}

void logger_t::finished_booting(const char *func) 
{ 
    char t[256]; 
    snprintf(t, 256, "%s finished booting", func); 
    log(t); 
    if (--boot > 0) return;
    if (boot < 0) return;
}

