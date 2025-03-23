//
// webserver.cpp
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
#include "webserver.h"
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>
#include <assert.h>
#include "circle-kernel.h"
#include "options.h"
extern Options options;
#define MAX_CONTENT_SIZE	1000000

// our content
static const char s_Index[] =
#include "webcontent/index.h"
;

static const u8 s_Style[] =
#include "webcontent/style.h"
;

static const u8 s_Favicon[] =
{
#include "webcontent/favicon.h"
};

static const u8 s_logo[] =
{
#include "webcontent/pi1541-logo.h"
};

static const char FromWebServer[] = "webserver";

CWebServer::CWebServer (CNetSubSystem *pNetSubSystem, CActLED *pActLED, CSocket *pSocket)
:	CHTTPDaemon (pNetSubSystem, pSocket, MAX_CONTENT_SIZE, 80, MAX_CONTENT_SIZE),
	m_pActLED (pActLED)
{
	m_nMaxMultipartSize = MAX_CONTENT_SIZE;
}

CWebServer::~CWebServer (void)
{
	m_pActLED = 0;
}

CHTTPDaemon *CWebServer::CreateWorker (CNetSubSystem *pNetSubSystem, CSocket *pSocket)
{
	return new CWebServer (pNetSubSystem, m_pActLED, pSocket);
}

THTTPStatus CWebServer::GetContent (const char  *pPath,
				    const char  *pParams,
				    const char  *pFormData,
				    u8	        *pBuffer,
				    unsigned    *pLength,
				    const char **ppContentType)
{
	assert (pPath != 0);
	assert (ppContentType != 0);
	assert (m_pActLED != 0);
	Kernel.log("%s: %s, fdata = '%s', fn = %s", __FUNCTION__, pPath, pFormData, options.GetAutoMountImageName());

	CString String;
	const u8 *pContent = 0;
	unsigned nLength = 0;
	if (strcmp (pPath, "/") == 0 ||
		strcmp (pPath, "/index.html") == 0)
	{
		assert (pFormData != 0);
		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;
		
		char filename[255];
		char extension[10];
		u8 startChar = 0;
		filename[0] = '\0';
		extension[0] = '\0';

		bool noFormParts = true;
		bool textFileTransfer = false;
		
		if (GetMultipartFormPart (&pPartHeader, &pPartData, &nPartLength))
		{
			noFormParts = false;
			assert (pPartHeader != 0);
			char * startpos = strstr (pPartHeader, "filename=\"");
			if ( startpos != 0)
			{
				startpos += 10;
				u16 fnl = 0;
				u16 exl = 0;
				u16 extensionStart = 0;
				while ( (startpos[fnl] != '\"') && (fnl < 254) )
				{
					filename[fnl] = startpos[fnl];
					if ( filename[fnl] == '.' )
						extensionStart = fnl+1;
					fnl++;
				}
				filename[fnl] = '\0';
				while ( extensionStart > 0 && extensionStart < fnl && exl < 9)
				{
					u8 tmp = filename[extensionStart + exl];
					if (tmp >=65 && tmp <=90) tmp+=32; //strtolower
					extension[exl] = tmp;
					exl++;
				}
				extension[exl] = '\0';
				Kernel.log("found filename: '%s' %i", filename, strlen(filename));
				Kernel.log("found extension: '%s' %i", extension, strlen(extension));
			}
			if ((strstr(pPartHeader, "name=\"diskimage\"") != 0) &&
				(nPartLength > 0))
			{
				Kernel.log("%s: uploading...\n", __FUNCTION__);
			}
		}
		String.Format (s_Index, options.GetAutoMountImageName(), options.GetAutoMountImageName());

		pContent = (const u8 *) (const char *) String;
		nLength = String.GetLength ();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp (pPath, "/style.css") == 0)
	{
		pContent = s_Style;
		nLength = sizeof s_Style-1;
		*ppContentType = "text/css";
	}	
	else if (strcmp (pPath, "/pi1541-logo.png") == 0)
	{
		pContent = s_logo;
		nLength = sizeof s_logo;
		*ppContentType = "image/x-icon";
	}
	else if (strcmp (pPath, "/favicon.ico") == 0)
	{
		pContent = s_Favicon;
		nLength = sizeof s_Favicon;
		*ppContentType = "image/x-icon";
	}
	else
	{
		return HTTPNotFound;
	}

	Kernel.log("%s: pLength = %d, nLength = %d\n", __FUNCTION__, *pLength, nLength);
	assert (pLength != 0);
	if (*pLength < nLength)
	{
		CLogger::Get ()->Write (FromWebServer, LogError, "Increase MAX_CONTENT_SIZE to at least %u", nLength);
		Kernel.log("%s: Increase MAX_CONTENT_SIZE to at least %u", __FUNCTION__, nLength);

		return HTTPInternalServerError;
	}

	assert (pBuffer != 0);
	assert (pContent != 0);
	assert (nLength > 0);
	memcpy (pBuffer, pContent, nLength);

	*pLength = nLength;

	return HTTPOK;
}
