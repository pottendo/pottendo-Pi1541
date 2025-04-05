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
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
using namespace std;

extern Options options;
bool webserver_upload = false;
static string def_prefix = "SD:/1541/";

#define MAX_CONTENT_SIZE	4000000

// our content
static const char s_Index[] =
#include "webcontent/index.h"
;

static const char s_config[] =
#include "webcontent/config.h"
;

static const char s_edit_config[] =
#include "webcontent/edit-config.h"
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

static std::string urlEncode(const std::string& value) {
    std::ostringstream encoded;
    for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;  // Keep safe characters as they are
        } else {
            encoded << '%' << std::uppercase << std::hex << int(c);
        }
    }
    return encoded.str();
}

static std::string urlDecode(const std::string& value) {
    std::ostringstream decoded;
    size_t len = value.length();

    for (size_t i = 0; i < len; i++) {
        if (value[i] == '%' && i + 2 < len) {
            // Convert %XX to character
            int hexValue;
            std::istringstream(value.substr(i + 1, 2)) >> std::hex >> hexValue;
            decoded << static_cast<char>(hexValue);
            i += 2;  // Skip the next two hex digits
        } else if (value[i] == '+') {
            decoded << ' ';  // Convert '+' to space
        } else {
            decoded << value[i];  // Keep normal characters
        }
    }

    return decoded.str();
}

static string print_human_readable_time(WORD ftime) {
	char tmp[256];
    int year   = ((ftime >> 9) & 0x7F) + 1980;
    int month  = (ftime >> 5) & 0x0F;
    int day    = ftime & 0x1F;
    int hour   = (ftime >> 11) & 0x1F;
    int minute = (ftime >> 5) & 0x3F;
    int second = (ftime & 0x1F) * 2;

    snprintf(tmp, 255, "%04d-%02d-%02d %02d:%02d:%02d\n",
           		year, month, day, hour, minute, second);
	return string(tmp);
}

static void direntry_table(string &res, string &path, int type_filter)
{
	res = "<table>\
		<tr>\
    		<th>Name</th>\
    		<th>Type</th>\
    		<th>Date</th>\
  		</tr>";
   	FRESULT fr;              // Result code
    FILINFO fno;             // File information structure
    DIR dir;                 // Directory object
	vector<FILINFO> list;
    // Open the directory
	
	const char *x = (def_prefix + path).c_str();
    fr = f_opendir(&dir, x);
    if (fr != FR_OK) {
        DEBUG_LOG("Failed to open directory '%s': %d", x, fr);
        return;
    }
    while (1) {
        // Read a directory item
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break; // Break on error or end of dir
		if (fno.fattrib & type_filter)
			list.push_back(fno);
    }
    f_closedir(&dir);
	
	sort(list.begin(), list.end(), 
		[](const FILINFO &a, const FILINFO &b) {
			string sa = string(a.fname);
			string sb = string(b.fname);
			transform(sa.begin(), sa.end(), sa.begin(), ::tolower);
			transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
			return sa < sb;
		});
	string sep;
	if (path != "")
	{
		sep = '/';
		size_t pos = path.find_last_of('/');
		if (pos == string::npos) pos = 0;
		res += "<tr><td>" + 
						string("<a href=index.html?") + path.substr(0, pos) + ">..</a>" +
					"</td>" +
					"<td>[DIR]</td>" +
					"<td>" + "--" + "</td>" +
				"</tr>";
	}
	for (auto it : list)
	{
	    if (it.fattrib & type_filter) {
            //DEBUG_LOG("[DIR]  %s", (path + sep + it.fname).c_str());
			res += 	"<tr><td>" + 
						string("<a href=index.html?") + urlEncode(path + sep + it.fname) + ">" + it.fname + "</a>" +
					"</td>" +
					"<td>[DIR]</td>" +
					"<td>" + print_human_readable_time(it.fdate) + "</td>" +
					"</tr>";
        } else {
			/* file*/
            //DEBUG_LOG("[FILE] %s\t(%lu bytes)", it.fname, (unsigned long)it.fsize);
        }
	}

	res += "</table>";

}

static char *extract_field(const char *field, const char *pPartHeader, char *filename, char *extension)
{
	assert(pPartHeader != 0);
	char *startpos = strstr(pPartHeader, field);
	if (startpos != 0)
	{
		startpos += strlen(field);
		u16 fnl = 0;
		u16 exl = 0;
		u16 extensionStart = 0;
		while ((startpos[fnl] != '\"') && (fnl < 254))
		{
			filename[fnl] = startpos[fnl];
			if (filename[fnl] == '.')
				extensionStart = fnl + 1;
			fnl++;
		}
		filename[fnl] = '\0';
		while (extensionStart > 0 && extensionStart < fnl && exl < 9)
		{
			u8 tmp = filename[extensionStart + exl];
			if (tmp >= 65 && tmp <= 90)
				tmp += 32; // strtolower
			extension[exl] = tmp;
			exl++;
		}
		extension[exl] = '\0';
		Kernel.log("found filename: '%s' %i", filename, strlen(filename));
		Kernel.log("found extension: '%s' %i", extension, strlen(extension));
	}
	return filename;
}

static bool write_file(const char *fn, const u8 *pPartData, unsigned nPartLength)
{
	FILINFO fi;
	UINT bw;
	bool ret = false;

	if (f_stat(fn, &fi) == FR_OK)
	{
		DEBUG_LOG("%s: file exists '%s', unlinking", __FUNCTION__, fn);
		if (f_unlink(fn) != FR_OK)
			DEBUG_LOG("%s: unlink of '%s' failed", __FUNCTION__, fn);
	}
	else
		DEBUG_LOG("%s: '%s' doesn't exist", __FUNCTION__, fn);
	FIL fp;
	if (f_open(&fp, fn, FA_CREATE_NEW | FA_WRITE) != FR_OK)
		DEBUG_LOG("%s: open of '%s' failed", __FUNCTION__, fn);
	else if (f_write(&fp, pPartData, nPartLength, &bw) != FR_OK)
		DEBUG_LOG("%s: write of '%s' failed", __FUNCTION__, fn);
	else
	{
		DEBUG_LOG("%s: write of '%s', %d/%d bytes successful.", __FUNCTION__, fn, nPartLength, bw);
		ret = true;
	}
	f_close(&fp);
	return ret;
}

static bool read_file(string &dfn, string &msg, string &fcontent)
{
	FIL fp;
	FILINFO fi;
	if (f_stat(dfn.c_str(), &fi) != FR_OK) 
	{
		msg = "Can't stat <i>" + dfn + "</i>";
		return false;

	}
	if (f_open(&fp, dfn.c_str(), FA_READ) != FR_OK)
	{
		msg = "Can't open <i>" + dfn + "</i>";
		return false;
	}
	UINT br, fl = 0;
	char buf[1025];
	buf[1024] = '\0';
	do {
		if (f_read(&fp, buf, 1024, &br) != FR_OK)
			break;
		buf[br] = '\0';
		fcontent += string(buf);
		fl += br;
	} while(fl < fi.fsize);
	bool ret = false;
	if (fl != fi.fsize)
	{
		msg = "Read error reading <i>" + dfn + "</i>";
		DEBUG_LOG("%s: read %dbytes, expected %dbytes", __FUNCTION__, fl, fi.fsize);
	}
	else
	{
		msg = "Successfully read <i>" + dfn + "</i>";
		ret = true;
	}
	f_close(&fp);
	return ret;
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
	CString String;
	const u8 *pContent = 0;
	unsigned nLength = 0;
	char filename[255];
	char extension[10];
	filename[0] = '\0';
	extension[0] = '\0';

	DEBUG_LOG("%s: pPath = '%s'", __FUNCTION__, pPath);
	if (strcmp (pPath, "/") == 0 ||
		strcmp (pPath, "/index.html") == 0)
	{
		assert (pFormData != 0);
		char msg[1024];
		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;
		string curr_dir;
		string curr_path = urlDecode(pParams);

		bool noFormParts = true;
		snprintf(msg, 1023, "Automount image-name: %s, path-prefix: %s", options.GetAutoMountImageName(), def_prefix.c_str());
		DEBUG_LOG("curr_path = %s", curr_path.c_str());
		direntry_table(curr_dir, curr_path, AM_DIR);

		if (GetMultipartFormPart (&pPartHeader, &pPartData, &nPartLength))
		{
			noFormParts = false;
			extract_field("filename=\"", pPartHeader, filename, extension);
			const char *pPartHeaderCB;
			const u8 *pPartDataCB;
			unsigned nPartLengthCB;
			const char *targetfn = filename;
			bool do_remount = false;

			if (GetMultipartFormPart (&pPartHeaderCB, &pPartDataCB, &nPartLengthCB))
			{
				if ((strstr(pPartHeaderCB, "name=\"am-cb1\"") != 0) &&
					(pPartDataCB[0] == '1'))
				{
					const char *_t = options.GetAutoMountImageName();
					if (_t && (_t = strrchr(_t, '.')) && (strcasecmp(_t + 1, extension) == 0))
					{
						Kernel.log("%s: image '%s' shall be used as automount image", __FUNCTION__, filename);
						targetfn = options.GetAutoMountImageName();
						do_remount = true;
					}
					else
					{
						DEBUG_LOG("%s: automount image extension doesn't match '%s' vs. '.%s'", __FUNCTION__, _t, extension);
						targetfn = filename;
					}
				}
			}

			if (GetMultipartFormPart (&pPartHeaderCB, &pPartDataCB, &nPartLengthCB))
			{
				if (strstr(pPartHeaderCB, "name=\"xpath\"") != 0)
				{
					char tmp[256];
					memcpy(tmp, pPartDataCB, nPartLengthCB);
					tmp[nPartLengthCB] = '\0';
					curr_path = string(tmp);
					direntry_table(curr_dir, curr_path, AM_DIR);
				}
			}			

			if ((strstr(pPartHeader, "name=\"diskimage\"") != 0) &&
				(nPartLength > 0))
			{
				snprintf(msg, 1023, "file operation failed!");
				if ((strcmp(extension, "d64") == 0) ||
					(strcmp(extension, "g64") == 0) ||
					(strcmp(extension, "prg") == 0))
				{
					DEBUG_LOG("%s: received %d bytes.", __FUNCTION__, nPartLength);
					const char *fn;
					FILINFO fi;
					UINT bw;
					string sep="";
					if (curr_path != "") sep = "/";
					string _x = string(def_prefix + curr_path + sep + targetfn);
					if (write_file(_x.c_str(), pPartData, nPartLength))
					{
						snprintf(msg, 1023, "successfully wrote: %s", _x.c_str());
						if (do_remount)
							webserver_upload = true;	// this re-mounts the automount image
					}
					else
						snprintf(msg, 255, "file operation failed.");
				}
				else
				{
					Kernel.log("%s: invalid filetype: '.%s'...\n", __FUNCTION__, extension);
					snprintf(msg, 1023, "invalid image extension!");
				}
			}
			else
			{
				Kernel.log("%s: upload failed due to unkown reasons...\n", __FUNCTION__);
				snprintf(msg, 1023, "upload failed!");
			}
		}

		String.Format(s_Index, msg, curr_path.c_str(), (def_prefix + curr_path).c_str(), curr_dir.c_str(), Kernel.get_version());

		pContent = (const u8 *)(const char *)String;
		nLength = String.GetLength();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp(pPath, "/config.html") == 0)
	{
		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;

		string kernelname = "Unknown";
		string modelstr = "Unknown";
		string msg = "unknown error";

		CMachineInfo *mi = CMachineInfo::Get();
		if (mi)
		{
			TMachineModel model = mi->GetMachineModel();
			switch (model) {
				case MachineModelZero2W:
				case MachineModel3B:
				case MachineModel3BPlus:
					modelstr = "Model Pi3B,3B+ or Zero2W";
					kernelname = "kernel8-32.img";
					break;
				case MachineModel4B:
				modelstr = "Model Pi4";
#if AARCH == 32
					kernelname = "kernel7l.img";
#else
					kernelname = "kernel8-rpi4.img";
#endif	
					break;
				case MachineModel5:
					kernelname = "kernel_2712.img";
				default:
					break;
			}
		}

		if (GetMultipartFormPart(&pPartHeader, &pPartData, &nPartLength))
		{
			extract_field("filename=\"", pPartHeader, filename, extension);
			if (kernelname != string(filename))
			{
				DEBUG_LOG("upload filename mismatch: %s != %s", kernelname.c_str(), filename);
				msg = string("Filename mismatch: <i>" + kernelname + "</i> != <i>" + string(filename) + "</i>, refusing to upload!");
			}
			else
			{
				string dfn = string("SD:/") + kernelname;
				if (write_file(dfn.c_str(), pPartData, nPartLength))
					msg = string("Successfully wrote <i>") + dfn + "</i>";
				else
					msg = string("Failed to write <i>") + dfn + "</i>";
			}
		}
		else
		{
			msg = (modelstr + ", Kernelname = <i>" + kernelname + "</i>");
		}
		String.Format(s_config, msg.c_str(), Kernel.get_version());
		pContent = (const u8 *)(const char *)String;
		nLength = String.GetLength();
		*ppContentType = "text/html; charset=iso-8859-1";		
	}
	else if (strcmp(pPath, "/options.html") == 0)
	{
		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;
		string msg = "unknown error!";
		if (GetMultipartFormPart(&pPartHeader, &pPartData, &nPartLength))
		{
			if (!extract_field("filename=\"", pPartHeader, filename, extension))
			{
				DEBUG_LOG("%s: can't find filename in header '%s'", __FUNCTION__, pPartHeader);
				msg = string("internal error filename in header not found");
			}
			else
			{
				string dfn = string("SD:/") + filename;
				if (write_file(dfn.c_str(), pPartData, nPartLength))
					msg = string("Successfully wrote <i>") + dfn + "</i>";
				else
					msg = string("Failed to write <i>") + dfn + "</i>";
			}
		}
		String.Format(s_config, msg.c_str(), Kernel.get_version());
		pContent = (const u8 *)(const char *)String;
		nLength = String.GetLength();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp(pPath, "/edit-config.html") == 0)
	{
		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;
		string msg = "", msg2 = "";
		if (GetMultipartFormPart(&pPartHeader, &pPartData, &nPartLength))
		{
			DEBUG_LOG("%s: options upload requested, header = '%s'", __FUNCTION__, pPartHeader);
			extract_field("name=\"", pPartHeader, filename, extension);
			string dfn = string("SD:/") + filename;			

			f_unlink((dfn + ".BAK").c_str());	// unconditionally remove backup
			f_rename(dfn.c_str(), (dfn + ".BAK").c_str());
			if (write_file(dfn.c_str(), pPartData, nPartLength))
				msg = string("Successfully wrote <i>") + dfn + "</i>, ";
			else
				msg = string("Failed to write <i>") + dfn + "</i>, ";
		}
		string options = "";
		string config = "";
		string dfn = "SD:/options.txt";
		read_file(dfn, msg2, options);
		msg += msg2 + ", ";
		dfn = "SD:/config.txt";
		read_file(dfn, msg2, config);
		msg += msg2;
		String.Format(s_edit_config, msg.c_str(), options.c_str(), config.c_str(), Kernel.get_version());
		pContent = (const u8 *)(const char *)String;
		nLength = String.GetLength();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp(pPath, "/style.css") == 0)
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
