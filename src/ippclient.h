//
// ippclient.h
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
//

#ifndef __ippclient_h_
#define __ippclient_h_

#include <circle/net/in.h>
#include <circle/net/dnsclient.h>
#include <circle/net/ipaddress.h>
#include <circle-mbedtls/tlssimpleclientsocket.h>
#include <stdint.h>
#include <string>
#include <vector>

using namespace std;

enum {
    PR_INV = 0x0,
    PR_COL = 0x1,
    PR_BW  = 0x2
};

class IPPAttr {
    string attr_name;
    string attr_value;
    vector<char> bin;

    void w16(uint16_t v) { bin.push_back((v >> 8) & 0xff); bin.push_back(v & 0xff); }
    void wstr(string s) { bin.insert(bin.end(), s.begin(), s.end()); }
public:
    IPPAttr(char a, string attr, string val) { 
        bin.push_back(a); w16(attr.length()); wstr(attr); w16(val.length()); wstr(val); 
    }
    IPPAttr(uint8_t v) { bin.push_back(v); }
    IPPAttr(uint16_t v) { w16(v); }
    IPPAttr(uint32_t v) { w16((v >> 16) & 0xffff); w16(v & 0xffff); }

    ~IPPAttr() = default;
    char *get_bin(void) { return bin.data(); }
    vector<char> &get_binr(void) { return bin; }
    size_t get_len(void) { return bin.size(); }
};

class IPPClient
{
    const uint16_t port = 631;
    const size_t nMaxSize = 16*1024;
    const char *printer;
    CDNSClient dns;
    CIPAddress IPPrinter;
    CString ip;
    CircleMbedTLS::CTLSSimpleClientSocket socket;
    int dprint_supported;
    IPPAttr uri;
public:
    IPPClient(CircleMbedTLS::CTLSSimpleSupport *mNet, const char *printer);
    ~IPPClient();
    bool Connect(void);
    bool GetAttributes(char *buf, size_t len, size_t &hsize, size_t &bsize);
    inline int dprint_support(void) { return dprint_supported; }
    bool Print(const char *buf, size_t len);
};

#endif
