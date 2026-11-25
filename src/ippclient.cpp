//
// ippclient.cpp
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

#include "ippclient.h"
#include "circle-kernel.h"
#include "debug.h"
#include "string.h"
#include <list>

IPPClient::IPPClient(CircleMbedTLS::CTLSSimpleSupport *mNet, const char *pr)
: printer(pr)
, dns(mNet->GetNet())
, socket(mNet, IPPROTO_TCP)
, dprint_supported(PR_INV)
, uri(0x45, "printer-uri", string(pr) + "/ipp/print")
{
    string p(printer);
    size_t pos = p.find("ipp://");
    if (pos != string::npos)
        printer += 6;
    else if (p.find("ipps://") != string::npos)
        printer += 7;
    else
        uri = IPPAttr(0x45, "priner-uri", string("ipps://") + pr + "/ipp/print");
    DEBUG_LOG("%s: IPPClient for printer %s created.", __FUNCTION__, p.c_str());
}

IPPClient::~IPPClient()
{
    DEBUG_LOG("%s: IPPClient for printer %s deleted.", __FUNCTION__, printer);
}

bool IPPClient::Connect(void)
{
    int res;
    if (!dns.Resolve(printer, &IPPrinter))
    {
        DEBUG_LOG("%s: can't resolve %s", __FUNCTION__, printer);
        return false;
    }
    if ((res = socket.Setup(printer)) != 0)
    {
        DEBUG_LOG("%s: socket(%s) failed: %d", __FUNCTION__, printer, res);
        return false;
    }

    DEBUG_LOG("%s: connecting to: %s...", __FUNCTION__, printer);
    if ((res = socket.Connect(IPPrinter, port)) < 0)
    {
        DEBUG_LOG("%s: socket() failed: 0x%04x", __FUNCTION__, res);
        return false;
    }
    socket.SetOptionReceiveTimeout(10 * 1000 * 1000);
    CIPAddress ipb(socket.GetForeignIP());
    ipb.Format(&ip);
    DEBUG_LOG("%s: printer %s(IP: %s) connected", __FUNCTION__, printer, (const char*)ip);

    char pBuffer[nMaxSize];
 	size_t hsize, bsize;
	if (!GetAttributes(pBuffer, nMaxSize, hsize, bsize, 0x0B, nullptr, "keep-alive"))
		DEBUG_LOG("%s: failed to get printer attributes", __FUNCTION__);
    else
	{
        DEBUG_LOG("%s: Attributes:", __FUNCTION__);
        hexdump(pBuffer + hsize, bsize);
        char *t1, *t2, *t3;
        std::string prsup("");
        t1 = fbinary(&pBuffer[hsize], &pBuffer[hsize + bsize], "pwg-raster-document-type-supported", strlen("pwg-raster-document-type-supported"));

        if (t1)
        {
            t2 = fbinary(t1, &pBuffer[hsize + bsize], "sgray_8", strlen("sgray_8"));
            t3 = fbinary(t1, &pBuffer[hsize + bsize], "srgb_8", strlen("srgb_8"));
            if (t2)
            {
                dprint_supported |= PR_BW;
                prsup += "black&white ";
            }
            if (t3)
            {
                dprint_supported |= PR_COL;
                prsup += "color";
            }
            if (fbinary(&pBuffer[hsize], &pBuffer[hsize + bsize], "iso_a4_210x297mm", strlen("iso_a4_210x297mm")))
                DEBUG_LOG("%s: found A4 - iso_a4_210x297mm - support", __FUNCTION__);
            else
                DEBUG_LOG("%s: printer doesn't support A4 - iso_a4_210x297mm. Output may be cropped.", __FUNCTION__);
        }
        DEBUG_LOG("%s: direct print support: %s", __FUNCTION__, (dprint_supported ? prsup.c_str() : "none"));
	}
    return dprint_support();
}

static bool SendAll(CircleMbedTLS::CTLSSimpleClientSocket &Socket, const void *pData,	unsigned nLength)
{
    int l;
	const u8 *p = (const u8 *)pData;
	while (nLength > 0)
	{
        l = (nLength > 1000) ? 1000 : nLength;
		int nSent = Socket.Send(p, l, 0);
		if (nSent <= 0)
        {
            DEBUG_LOG("%s: failed: %d", __FUNCTION__, nSent);
			return false;
        }
		p += nSent;
		nLength -= nSent;
	}
	return true;
}

static bool Receive(CircleMbedTLS::CTLSSimpleClientSocket  &Socket, 
                                char *pBuffer, size_t len, char **response, 
                                size_t &headerlen, size_t &responselen)

{
    char Temp[FRAME_BUFFER_SIZE];
    char *tp;
    int nReceived = 0;
    bool hasContentLength = false;
    bool chunked = false;

    if (len == 0)
    {
        DEBUG_LOG("%s: bufferlen is 0", __FUNCTION__);
        return false;
    }
    headerlen = 0;
    *response = pBuffer; // as start initialize it
    while (nReceived < (int)len - 1) 
    {
        unsigned nAvailable = len - nReceived - 1;
        unsigned nToRead = nAvailable < sizeof(Temp) ? nAvailable : sizeof(Temp);
        int n = Socket.Receive(Temp, nToRead, 0);
        //DEBUG_LOG("%s: received %d bytes", __FUNCTION__, n);
        if (n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
        {
            if (headerlen >= 0 && !hasContentLength)
            {
                responselen = nReceived - (*response - pBuffer);
                //DEBUG_LOG("%s: MBEDTLS_ERR CLOSE NOTIFY, headerlen = %dm content is empty", __FUNCTION__, headerlen);
                return true;
            }

            DEBUG_LOG("%s: peer closed TLS before completing HTTP response", __FUNCTION__);
            return false;
        }
        if ((n <= 0) || (n > (int)nAvailable))
        {
            DEBUG_LOG("%s: socket Receive returned %d", __FUNCTION__, n);
            return false;
        }
        //hexdump(Temp, n);

        memcpy(pBuffer + nReceived, Temp, n);
        nReceived += n;
        pBuffer[nReceived] = '\0';
        if (chunked) 
        {
            static const char chunked_end[5] = { 0x30, 0x0d, 0x0a, 0x0d, 0x0a };
            //DEBUG_LOG("%s: chunked encoding...", __FUNCTION__);
            if (fbinary(pBuffer, &pBuffer[nReceived], chunked_end, sizeof(chunked_end)))
            {
                responselen = nReceived - headerlen;
                //DEBUG_LOG("%s: found end of chunked body, len = %d", __FUNCTION__, responselen);
                return true;
            }
        }
        if (headerlen == 0)
        {
            tp = strstr(pBuffer, "\r\n\r\n");
            if (!tp)
                continue;
            *response = tp + 4;
            headerlen = *response - pBuffer;

            char *headerEnd = *response - 4;
            char saved = *headerEnd;
            *headerEnd = '\0';
            //DEBUG_LOG("%s: found header, len = %d\n%s", __FUNCTION__, headerlen, pBuffer);
            *headerEnd = saved;

            if ((tp = strstr(pBuffer, "Content-Length:")) != nullptr)
            {
                tp += strlen("Content-Length:");
                while ((*tp == ' ') || (*tp == '\t'))
                    tp++;
                char *end;
                long contentLength = strtol(tp, &end, 10);
                if (end == tp || contentLength < 0 || 
                    (size_t)contentLength > len - (*response - pBuffer))
                    return false;
                responselen = contentLength;
                hasContentLength = true;
            }
            else if ((tp = strstr(pBuffer, "Transfer-Encoding: chunked")) != nullptr)
            {
                chunked = true;
            }
            /* DEBUG_LOG("%s: headerlen = %d, bodylen = %d (-1 == chunked)", __FUNCTION__,
                      headerlen, hasContentLength ? (int)responselen : -1); */
        }

        if (hasContentLength
            && nReceived >= (*response - pBuffer) + (int)responselen)
        {
            return true;
        }
    }

    return false;
}

bool IPPClient::GetAttributes(char *buf, size_t blen, size_t &hsize, size_t &bsize, uint8_t code, char *data, char *conn)
{
    list<IPPAttr>ipp_get_attr;
    vector<char> ipp_cmd;
    ipp_get_attr.push_back(IPPAttr(static_cast<uint16_t>(0x200)));  // IPP 2.0
    ipp_get_attr.push_back(IPPAttr(static_cast<uint16_t>(code)));   // GetAttr 0x0b, JobAttr 0x09
    ipp_get_attr.push_back(IPPAttr(static_cast<uint32_t>(0x01)));   // Req. ID
    ipp_get_attr.push_back(IPPAttr(static_cast<uint8_t>(0x01)));    // Operations attibutes
    ipp_get_attr.push_back(IPPAttr(0x47, "attributes-charset", "utf-8"));
    ipp_get_attr.push_back(IPPAttr(0x48, "attributes-natural-language", "en"));
    if (code == 0x0b)
    {
        ipp_get_attr.push_back(uri);
        ipp_get_attr.push_back(IPPAttr(0x44, "requested-attributes", "pwg-raster-document-type-supported"));
        ipp_get_attr.push_back(IPPAttr(0x44, "requested-attributes", "media-supported"));
        ipp_get_attr.push_back(IPPAttr(0x44, "requested-attributes", "document-format-supported"));
        ipp_get_attr.push_back(IPPAttr(0x44, "requested-attributes", "pwg-raster-document-resolution-supported"));
    } 
    else if (code == 0x09)
    {
        ipp_get_attr.push_back(IPPAttr(0x45, "job-uri", string(data)));
    }
    ipp_get_attr.push_back(IPPAttr(static_cast<uint8_t>(0x03)));    // end marker
    for (auto attr : ipp_get_attr)
    {
        ipp_cmd.insert(ipp_cmd.end(), attr.get_binr().begin(), attr.get_binr().end());
    }
    //hexdump(ipp_cmd.data(), ipp_cmd.size());
    CString t = "";
	t.Format("POST /ipp/print HTTP/1.1\r\n"
			 "Host: %s:%d\r\n"
			 "Content-Type: application/ipp\r\n"
			 "Content-Length: %u\r\n"
			 "Connection: %s\r\n"
			 "\r\n",
			 ip.c_str(), port, ipp_cmd.size(), conn);
    //DEBUG_LOG("%s: sending header:\n%s", __FUNCTION__, t.c_str());
    if (!SendAll(socket, t.c_str(), t.GetLength()))
        return false;

    if (!SendAll(socket, ipp_cmd.data(), ipp_cmd.size()))
        return false;

    char *temp;
    //DEBUG_LOG("%s: receiving ...", __FUNCTION__);
    if (Receive(socket, buf, blen, &temp, hsize, bsize))
    {
        std::string header((char *)buf, temp - buf);
        //DEBUG_LOG("%s: response header\n%s", __FUNCTION__, buf, header.c_str());
    }
    return true;
}

bool IPPClient::Print(const char *out_buf, size_t blen)
{
    list<IPPAttr>ipp_print_job;
    vector<char> ipp_cmd;
    ipp_print_job.push_back(IPPAttr(static_cast<uint16_t>(0x200)));  // IPP 2.0
    ipp_print_job.push_back(IPPAttr(static_cast<uint16_t>(0x02)));   // Print Job
    ipp_print_job.push_back(IPPAttr(static_cast<uint32_t>(0x01)));   // Req. ID
    ipp_print_job.push_back(IPPAttr(static_cast<uint8_t>(0x01)));    // Operations attributes
    ipp_print_job.push_back(IPPAttr(0x47, "attributes-charset", "utf-8"));
    ipp_print_job.push_back(IPPAttr(0x48, "attributes-natural-language", "en"));
    ipp_print_job.push_back(uri);
    ipp_print_job.push_back(IPPAttr(0x42, "requesting-user-name", "pi1541"));
    ipp_print_job.push_back(IPPAttr(0x42, "job-name", "pi1541 raster job"));
    ipp_print_job.push_back(IPPAttr(0x49, "document-format", "image/pwg-raster"));
    ipp_print_job.push_back(IPPAttr(static_cast<uint8_t>(0x02)));    // Job attributes
    ipp_print_job.push_back(IPPAttr(0x44, "media", "iso_a4_210x297mm"));
    ipp_print_job.push_back(IPPAttr(static_cast<uint8_t>(0x03)));    // end marker
    for (auto attr : ipp_print_job)
    {
        ipp_cmd.insert(ipp_cmd.end(), attr.get_binr().begin(), attr.get_binr().end());
    }
    //hexdump(ipp_cmd.data(), ipp_cmd.size());

    std::string ipp_req = 
        "POST /ipp/print HTTP/1.1"
        "\r\nHost: " + std::string(printer) +
        "\r\nContent-Type: application/ipp"
        "\r\nContent-Length: "  + std::to_string(ipp_cmd.size() + blen) +
        "\r\nConnection: keep-alive"
        "\r\n\r\n";

    //DEBUG_LOG("%s: header:\n%s", __FUNCTION__, ipp_req.c_str());

    if (!SendAll(socket, ipp_req.c_str(), ipp_req.length()))
    {
        DEBUG_LOG("%s: send header failed", __FUNCTION__);
        return false;
    }
    if (!SendAll(socket, ipp_cmd.data(), ipp_cmd.size()))
    {
        DEBUG_LOG("%s: send ippreq failed", __FUNCTION__);
        return false;
    }
    if (!SendAll(socket, out_buf, blen))
    {
        DEBUG_LOG("%s: send body failed", __FUNCTION__);
        return false;
    }

    char *body;
    size_t hlen = 0, bl = 0;
    char pBuffer[nMaxSize];
    if (!Receive(socket, pBuffer, nMaxSize, &body, hlen, bl))
    {
        DEBUG_LOG("%s: receive header failed, hlen = %d, bl = %d", __FUNCTION__, hlen, bl);
        return false;
    }
#ifndef PRINT_DEBUG    
    char *j;
    uint8_t job_state = 0;
    j = fbinary(&pBuffer[hlen], &pBuffer[hlen + bl], "job-uri", strlen("job-uri"));
    if (j)
    {
        j += 7;
        uint16_t jl = (*j) * 256 + *(j+1);
        char juri[256];
        strncpy(juri, j+2, jl);
        juri[jl] = '\0';
        int retry = 10;
        DEBUG_LOG("%s: found job-uri(%d) %s", __FUNCTION__, jl, juri);
        do
        {
            if (!GetAttributes(pBuffer, nMaxSize, hlen, bl, 0x09, juri, "keep-alive"))
            {
                DEBUG_LOG("%s: failed to get printer job attributes", __FUNCTION__);
                break;
            }
            else
            {
                DEBUG_LOG("%s: job attributes:", __FUNCTION__);
                hexdump(pBuffer, hlen + bl);
                char *js = fbinary(&pBuffer[hlen], &pBuffer[hlen + bl], "job-state", strlen("job-state"));
                if (js)
                {
                    job_state = *(js + 9 + 2 + 3); // "job-state" 2b len 4b status, last byte of status 
                    DEBUG_LOG("%s: job-state = %d", __FUNCTION__, job_state);
                    if (job_state > 6)
                        break;
                }
            }
            MsDelay(1000);
        } while (retry--);
    }
    if (job_state == 9)
        DEBUG_LOG("%s: printing successful", __FUNCTION__);
    else
        DEBUG_LOG("%s: print-job failed: %d", __FUNCTION__, job_state);
#endif        
    // close connection
    GetAttributes(pBuffer, nMaxSize, hlen, bl, 0x0b, nullptr, "close"); // Ignore result
    return true;
}
