//
// webserver.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2015  R. Stange <rsta2@o2online.de>
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
#ifndef _webserver_h
#define _webserver_h

#include <circle/net/httpdaemon.h>
#include <circle-mbedtls/httpclient.h>
#include <circle/actled.h>
#include <string>

class CWebServer : public CHTTPDaemon
{
public:
	CWebServer (CNetSubSystem *pNetSubSystem, 
		    CActLED	  *pActLED,			// the LED to be controlled
		    CSocket	  *pSocket = 0,		// is 0 for 1st created instance (listener)
			unsigned max_content_size = 1000 * 1024, unsigned max_multipart_size = 20000 * 1024);		
	~CWebServer (void);

	// creates an instance of our derived webserver class
	CHTTPDaemon *CreateWorker (CNetSubSystem *pNetSubSystem, CSocket *pSocket);

	// provides our content
	THTTPStatus GetContent (const char  *pPath,		// path of the file to be sent
				const char  *pParams,		// parameters to GET ("" for none)
				const char  *pFormData, 	// form data from POST ("" for none)
		    	u8	    	*pBuffer,		// copy your content here
				unsigned    *pLength,		// in: buffer size, out: content length
				const char **ppContentType, // set this if not "text/html"
				const char **pHeader);	

	THTTPStatus pi1541_proxy_html(std::string &url, u8 *pBuffer, unsigned *pLength, const char **ppContentType, const char **pHeader);
	CircleMbedTLS::THTTPStatus proxy_fetch(std::string &url, u8 *pBuffer, unsigned *pLength, u8 *pRespHeader, unsigned *pRespHLen);

private:
	const size_t m_nMaxContentSize;
	const size_t m_nMaxMultipartSize;
	CActLED *m_pActLED;
	CNetSubSystem &m_NetSubSystem;
	char m_proxyHeader[8*1024];
	u8 m_respHeader[8*1024];
};

#endif
