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
#include <circle/memory.h>
#include <circle/timer.h>
#include <assert.h>
#include "circle-kernel.h"
#include "options.h"
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <list>
#include "FileBrowser.h"
#include "Petscii.h"
#include "iec_commands.h"
#include "logger.h"
using namespace std;

extern Options options;
bool webserver_upload = false;
char mount_img[256] = { 0 };
char mount_path[256] = { 0 };
int mount_new = 0;
extern IEC_Commands *_m_IEC_Commands;
static string def_prefix = "SD:/1541";
#define MAX_ICON_SIZE (512 * 1024)
static char icon_buf[MAX_ICON_SIZE];

// our content
static const char s_Index[] =
#include "webcontent/index.h"
;

static const char s_update[] =
#include "webcontent/update.h"
;

static const char s_edit_config[] =
#include "webcontent/edit-config.h"
;

static const char s_edit_file[] =
#include "webcontent/edit-file.h"
;

static const char s_mount[] =
#include "webcontent/mount-imgs.h"
;

static const char s_status[] =
{
#include "webcontent/status.h"
};

static const char s_logger[] =
{
#include "webcontent/logger.h"
};

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

static const u8 s_font[] =
{
#include "webcontent/C64_Pro_Mono-STYLE.h"
};

static const char FromWebServer[] = "webserver";

CWebServer::CWebServer (CNetSubSystem *pNetSubSystem, CActLED *pActLED, CSocket *pSocket, 
	unsigned max_content_size, unsigned max_multipart_size)
:	CHTTPDaemon (pNetSubSystem, pSocket, max_content_size, 80, max_multipart_size),
	m_nMaxContentSize(max_content_size),
	m_nMaxMultipartSize(max_multipart_size),
	m_pActLED (pActLED)
{
	
}

CWebServer::~CWebServer (void)
{
	m_pActLED = 0;
}

CHTTPDaemon *CWebServer::CreateWorker (CNetSubSystem *pNetSubSystem, CSocket *pSocket)
{
	return new CWebServer (pNetSubSystem, m_pActLED, pSocket, m_nMaxContentSize, m_nMaxMultipartSize);
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

static bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

static void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

static string print_human_readable_time(WORD fdate, WORD ftime)
{
	char tmp[256];
    int year   = ((fdate >> 9) & 0x7F) + 1980;
    int month  = (fdate >> 5) & 0x0F;
    int day    = fdate & 0x1F;
    int hour   = (ftime >> 11) & 0x1F;
    int minute = (ftime >> 5) & 0x3F;
    int second = (ftime & 0x1F) * 2;

    snprintf(tmp, 255, "%04d-%02d-%02d %02d:%02d\n",
           		year, month, day, hour, minute);
	return string(tmp);
}

static const string header_NTD = string("<tr><th>Name</th><th>Icon</th><th>Date</th>");
static const string header_NT = string("<tr><th>Name</th><th>Type</th>");

static int direntry_table(const string header, string &res, string &path, string &page, int type_filter, bool file_ops = false)
{
	res = string("<table class=\"dirs\">") + header + (file_ops ? "<th>File Ops</th>" : "") + "</tr>";
   	FRESULT fr;              // Result code
    FILINFO fno;             // File information structure
    DIR dir;                 // Directory object
	vector<FILINFO> list;
    // Open the directory
	
	string x = (def_prefix + "/" + path);
    fr = f_opendir(&dir, x.c_str());
    if (fr != FR_OK) {
        DEBUG_LOG("%s: Failed to open directory '%s': %d", __FUNCTION__, x.c_str(), fr);
        return -1;
    }
    while (1) {
        // Read a directory item
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break; // Break on error or end of dir
		if (fno.fattrib & type_filter)
			list.push_back(fno);
    }
	
	sort(list.begin(), list.end(), 
		[](const FILINFO &a, const FILINFO &b) {
			string sa = string(a.fname);
			string sb = string(b.fname);
			transform(sa.begin(), sa.end(), sa.begin(), ::tolower);
			transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
			return sa < sb;
		});
	f_closedir(&dir);
	string icon;
	string sep = "/";
	string file_type = (type_filter == AM_DIR) ? string("[DIR]") : string("[FILE]");
	if ((type_filter == AM_DIR) && (path != ""))
	{
		size_t pos = path.find_last_of('/');
		if (pos == string::npos) pos = 0;
		res += "<tr><td>" + 
						string("<a href=") + page + "?" + file_type + "&" + urlEncode(path.substr(0, pos)) + "&>..</a>" +
					"</td>" +
					"<td>" + file_type + "</td><td></td>" +
				"</tr>";
	}
	FILINFO fi;
	string fullname, png;
	size_t idx;
	for (auto it : list)
	{
	    if (it.fattrib & type_filter) {
            //DEBUG_LOG("'%s' - '%s'", file_type.c_str(), (path + sep + it.fname).c_str());
			if (type_filter != AM_DIR)
			{
				icon = "<td><img src=\"/favicon.png\"></td>";
				fullname = path + sep + string(it.fname);
				if ((idx = fullname.find_last_of('.')) != string::npos)
				{
					png = fullname.substr(0, idx) + ".png";
					//DEBUG_LOG("%s: png = '%s'", __FUNCTION__, png.c_str());
					if (f_stat((def_prefix + png).c_str(), &fi) == FR_OK)
					{
						icon = "<td><img src=\"" + urlEncode(png) + "\" style=\"height: 64px; width: 96px;\"></td>";
						//DEBUG_LOG("%s: icon html = '%s'", __FUNCTION__, icon.c_str());
					}
				}
			}
			res += 	"<tr><td>" + 
						string("<a href=") + 
						((file_ops && (type_filter != AM_DIR)) ? "mount-imgs.html" : page) + 
						"?" + file_type + "&" + urlEncode(path + sep + it.fname) + ">" + it.fname + "</a>" +
					"</td>" + 
					((type_filter != AM_DIR) ? icon : "") +
					((type_filter == AM_DIR) ? "<td>" + file_type + "</td>" : "") +
					((type_filter != AM_DIR) ? "<td>" + print_human_readable_time(it.fdate, it.ftime) + "</td>" : "");
			if (file_ops)
			{
				res += "<td>";
				if (type_filter != AM_DIR)
				{
					res += string("<a href=mount-imgs.html?[MOUNT]&") + urlEncode(path + sep + it.fname) + ">"
						"<button type=\"button\" class=\"btb btn-success\">Mount</button></a>";
				}
				res += string("<a href=") + page + "?[DIR]&"+ urlEncode(path) + "&[DEL]&" + urlEncode(it.fname) + ">"
					"<button type=\"button\" class=\"btb btn-success\" onClick=\"return delConfirm(event)\">Delete</button></a>";
				if (type_filter != AM_DIR)
				{
					res += string("<a href=") + page + "?[DIR]&"+ urlEncode(path) + "&[DOWNLOAD]&" + urlEncode(it.fname) + " download=\"" + it.fname + "\">"
					"<button type=\"button\" class=\"btb btn-success\" onClick=\"return delConfirm(event)\">Download</button></a>";
				}
				res += string("</td>");
			}
			res += string("</tr>");
        } else {
			/* file*/
            //DEBUG_LOG("[FILE] %s\t(%lu bytes)", it.fname, (unsigned long)it.fsize);
        }
	}

	res += "</table>";
	return 0;
}

static char *extract_field(const char *field, const char *pPartHeader, char *filename, char *extension = nullptr)
{
	assert(pPartHeader != 0);
	char *startpos = strstr(pPartHeader, field);

	// special filename handling
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
		if (!extension)
			return filename;
		while (extensionStart > 0 && extensionStart < fnl && exl < 9)
		{
			u8 tmp = filename[extensionStart + exl];
			if (tmp >= 65 && tmp <= 90)
				tmp += 32; // strtolower
			extension[exl] = tmp;
			exl++;
		}
		extension[exl] = '\0';
		//Kernel.log("found filename: '%s' %i", filename, strlen(filename));
		//Kernel.log("found extension: '%s' %i", extension, strlen(extension));
	}
	else
		filename[0] = '\0';
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

static bool write_image(string fn, char *extension, const u8 *pPartData, unsigned nPartLength, string &msg)
{
	bool ret = false;
	if ((strcmp(extension, "d64") == 0) ||
		(strcmp(extension, "g64") == 0) ||
		(strcmp(extension, "d81") == 0) ||
		(strcmp(extension, "nib") == 0) ||
		(strcmp(extension, "nbz") == 0) ||
		(strcmp(extension, "t64") == 0) ||
		(strcmp(extension, "png") == 0) ||
		(strcmp(extension, "lst") == 0) ||
		(strcmp(extension, "txt") == 0) ||
		(strcmp(extension, "nfo") == 0) ||
		(strcmp(extension, "zip") == 0) ||
		(strcmp(extension, "prg") == 0))
	{
		DEBUG_LOG("%s: received %d bytes.", __FUNCTION__, nPartLength);
		FILINFO fi;
		UINT bw;
		string _x = string(def_prefix + fn);
		if (write_file(_x.c_str(), pPartData, nPartLength))
		{
			msg += (string ("successfully wrote <i>") + _x + "</i><br />");
			ret = true;
		}
		else
			msg += (string("file operation failed for <i>") + _x + "</i><br />");
	}
	else
	{
		Kernel.log("%s: invalid filetype: '.%s'...", __FUNCTION__, extension);
		msg += (string("invalid filetype for <i>") + fn + "</i><br />");
	}
	return ret;
}

static bool read_file(string &dfn, string &msg, string &fcontent, UINT *nLength = nullptr)
{
	FIL fp;
	FILINFO fi;
	if (f_stat(dfn.c_str(), &fi) != FR_OK) 
	{
		DEBUG_LOG("%s: can't stat '%s'", __FUNCTION__, dfn.c_str());
		msg = "Can't stat <i>" + dfn + "</i>";
		return false;

	}
	if (f_open(&fp, dfn.c_str(), FA_READ) != FR_OK)
	{
		DEBUG_LOG("%s: can't open '%s'", __FUNCTION__, dfn.c_str());
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
	if (nLength)
		*nLength = fl;
	f_close(&fp);
	return ret;
}

#if 0
static void hexdump(const unsigned char *buf, int len)
{
    int i;
    int idx = 0;
    int lines = 0;
    char linestr[256] = {0};

    while (len > 0) {

        for (i = 0; i < 16; i++) {
            if (i < len) {
                char t[4];
                snprintf(t, 4, "%02x ", (uint8_t) buf[idx + i]);
                strcat(linestr, t);
            } else {
                strcat(linestr, "   ");
            }
        }
        strcat(linestr, "|");
        for (i = 0; i < 16; i++) {
            if (i < len) {
                char t[2];
                char c;
                if (((unsigned char)buf[idx + i] > 31) && /* avoid tabs and other printable ctrl chars) */
                    isprint((unsigned char)buf[idx + i])) {
                    c= buf[idx + i];
                } else {
                    c = '.';
                }
                snprintf(t, 2, "%c", c);
                strcat(linestr, t);
            } else {
                strcat(linestr, " ");
            }
        }
        strcat(linestr, "|");
        DEBUG_LOG("%s", linestr);
        idx += 16;
        len -= 16;
    }
}
#endif

static bool get_sector(unsigned char *img_buf, int track, int sector, unsigned char * &src)
{
	if ((track >= D81_TRACK_COUNT) || (sector >= 40))
		return false;
	track--;
	src = img_buf + (track * 40 * 256) + (sector * 256);
	//hexdump(src, 16);
	return true;
}

static const char* fileTypes[]=
{
	"DEL", "SEQ", "PRG", "USR", "REL", "CBM", "???", "???",
	"???", "???", "???", "???", "???", "???", "???", "???"
};

static bool D81DiskInfo(unsigned char *img_buf, list<string> *dir)
{
	unsigned track, sector;
	char linebuffer[32] = {0};
	bool ret = true;
	unsigned char *buffer;
	std::string dir_line;
	char name[32] = {0};
	if (get_sector(img_buf, 40, 0, buffer))
	{
		if ((buffer[0] != 40) || (buffer[1] != 3) || (buffer[2] != 'D'))
			DEBUG_LOG("D81 header warning: %d/%d", buffer[0], buffer[1]);
		memcpy(name, &buffer[4], 16);
		snprintf(linebuffer, 32, "0 \"%s\" %c%c%c%c%c", name, buffer[0x16], buffer[0x17], buffer[0x18], buffer[0x19], buffer[0x1a]);
		dir_line += std::string(linebuffer);	
		dir->push_back(dir_line);
	}
	unsigned no_entries = 0, size;
	const char *ftype;
	char locked = ' ';
	char closed = ' '; 
	track = 40;
	sector = 3;
	do {
		if (get_sector(img_buf, track, sector, buffer))
		{
			track = buffer[0];
			sector = buffer[1];
			for (int entry = 0; entry < 8; entry++)
			{
				no_entries++;
				if (buffer[2] == 0)
				{
					//DEBUG_LOG("scratched entry");
					continue;
				}
				ftype = fileTypes[(buffer[2] & 7)];
				if (buffer[2] & 0b01000000) locked = '<';
				if (!(buffer[2] & 0b10000000)) closed = '*';
				memcpy(name, &buffer[5], 16);
				name[16] = name[17] = '\0';
				char *_e = strchr(name, 0xa0);
				if (_e) 
				{
					(*_e) = '"';
					name[16] = 0xa0;
				}
				else
				{
					name[16] = '"';
				}				
				size = static_cast<int>(buffer[0x1e]) + static_cast<int>(buffer[0x1f]) * 256;
				snprintf(linebuffer, 31, "%-4u \"%s%c%s%c", size, name, closed, ftype, locked);
				dir->push_back(string(linebuffer));
				buffer += 0x20;
				locked = closed = ' ';
			}
		} 
		else 
			track = ret = false; 
	} while ((track != 0) && (no_entries < 296));
	size = 0;
	for (int sector = 1; sector < 3; sector++)
	{
		get_sector(img_buf, 40, sector, buffer);
		for (int i = 0x10; i < 0x100; i += 6)
		{
			if ((i == 250) && sector == 1)	// skip BAM free blocks entry for directory (track 40)
				continue;
			size += static_cast<int>(buffer[i]);
		}
	}
	snprintf(linebuffer, 31, "%u BLOCKS FREE", size);
	dir->push_back(string(linebuffer));
	return ret;
}

static FRESULT f_mkdir_full(const char *path, string &msg)
{
	FRESULT ret = FR_OK;
	FILINFO fi;
	size_t idx;
	string p = path;
	string cpath;
	bool done = false;
	idx = p.find_first_of('/');
	if (idx == string::npos) return ret;
	cpath = p.substr(0, idx);
	do
	{
		string ndir = def_prefix + cpath;
		idx = p.substr(cpath.length() + 1, p.length()).find_first_of('/');
		if (idx != string::npos)
			cpath += p.substr(cpath.length(), idx + 1);
		else
		{
			ndir = def_prefix + p; // finally we can do the full path;
			done = true;
		}
		//DEBUG_LOG("%s: need to create '%s'", __FUNCTION__, ndir.c_str());
		if (f_stat(ndir.c_str(), &fi) != FR_OK)
			if ((ret = f_mkdir(ndir.c_str())) != FR_OK)
			{
				msg += (string("failed to mkdir <i>") + ndir + "</i><br />");
				break;
			}
			else
				msg += (string("created <i>") + ndir + "</i><br />");

	} while (!done);
	return ret;
}

static FRESULT f_unlink_full(string path, string &msg)
{
	DIR dir;
	FILINFO fi;
	FRESULT res;
	string npath = path;

	res = f_opendir(&dir, path.c_str());
	if (res != FR_OK)
		goto out;
	while (((res = f_readdir(&dir, &fi)) == FR_OK) && (fi.fname[0] != 0))
	{	
		npath = path + '/' + fi.fname;
		if (fi.fattrib & AM_DIR)
			res = f_unlink_full(npath, msg);
		else
		{
			DEBUG_LOG("%s: unlink '%s'", __FUNCTION__, npath.c_str());
			res = f_unlink(npath.c_str());
		}
		if (res != FR_OK)
		{
			DEBUG_LOG("%s: failed to unlink '%s'", __FUNCTION__, npath.c_str());
			msg += (string("Failed to delete <i>") + npath + "</i><br />");
		}
		else
		{
			//DEBUG_LOG("%s: successfully unlinked '%s'", __FUNCTION__, npath.c_str());
			msg += (string("Deleted <i>") + npath + "</i><br />");
		}			
	}
	if (res == FR_OK)
	{
		//DEBUG_LOG("%s: unlink '%s'", __FUNCTION__, path.c_str());
		msg += (string("Deleted <i>") + npath + "</i><br />");
	out:
		res = f_unlink(path.c_str());
	}
	f_closedir(&dir);
	return res;
}

static unsigned char img_buf[READBUFFER_SIZE];
extern FileBrowser *fileBrowser;
extern DiskCaddy diskCaddy;

FRESULT download_file(string &fullndir, const u8 *&pContent, unsigned &nLength, string &msg)
{
	FILINFO fileinfo;
	FIL fp;
	FRESULT res = FR_OK;
	UINT bytesRead;
	msg = "";
	if ((res = f_open(&fp, fullndir.c_str(), FA_READ)) != FR_OK)
		msg = "Open of " + fullndir + " failed (" + to_string(res) + ")";
	else
	{
		SetACTLed(true);
		if ((res = f_read(&fp, img_buf, READBUFFER_SIZE, &bytesRead)) != FR_OK)
			msg = "Read failed for " + fullndir + " (" + to_string(res) + ")";
		SetACTLed(false);
		f_close(&fp);
	}
	if (res == FR_OK)
	{
		pContent = img_buf;
		nLength = bytesRead;
	}
	else
	{
		pContent = (const u8 *)msg.c_str();
		nLength = msg.length();
	}
	return res;
}

static FRESULT gen_index(string &index, string path)
{
	DIR dir;
	FILINFO fi;
	FRESULT res;
	string npath;
	res = f_opendir(&dir, (def_prefix + "/" + path).c_str());
	if (res != FR_OK)
		return res;
	while (((res = f_readdir(&dir, &fi)) == FR_OK) && (fi.fname[0] != 0))
	{	
		npath = path + '/' + fi.fname;
		if (DiskImage::IsDiskImageExtention(fi.fname) ||
			DiskImage::IsLSTExtention(fi.fname))
		{
			index += ("\n" + npath);
		}
		if (fi.fattrib & AM_DIR)
			res = gen_index(index, npath);
	}
	return f_closedir(&dir);
}

static int read_dir(string name, list<string> &dir)
{
	FILINFO fileinfo;
	FIL fp;
	int ret = -1;
	if (f_open(&fp, name.c_str(), FA_READ) != FR_OK)
		return -1;
	strncpy(fileinfo.fname, name.c_str(), 255);
	UINT bytesRead;
	SetACTLed(true);
	memset(img_buf, 0xff, READBUFFER_SIZE);
	f_read(&fp, img_buf, READBUFFER_SIZE, &bytesRead);
	SetACTLed(false);
	f_close(&fp);

	DiskImage* diskImage = new DiskImage();
	if (!diskImage)
	{
		DEBUG_LOG("%s: new DiskImage failed", __FUNCTION__);
		return -1;
	}

	DiskImage::DiskType diskType = DiskImage::GetDiskImageTypeViaExtention(fileinfo.fname);
	switch (diskType)
	{
		case DiskImage::D64:
			ret = diskImage->OpenD64(&fileinfo, img_buf, bytesRead);
			break;
		case DiskImage::G64:
			ret = diskImage->OpenG64(&fileinfo, img_buf, bytesRead);
			break;
#if defined(PI1581SUPPORT)				
		case DiskImage::D81:
			ret = D81DiskInfo(img_buf, &dir);
			goto out;
		break;
#endif				
		case DiskImage::T64:
			ret = diskImage->OpenT64(&fileinfo, img_buf, bytesRead);
			break;
		case DiskImage::PRG:
			ret = diskImage->OpenPRG(&fileinfo, img_buf, bytesRead);
			break;
		case DiskImage::NIB:
			ret = diskImage->OpenNIB(&fileinfo, img_buf, bytesRead);
			break;
		case DiskImage::NBZ:
			ret = diskImage->OpenNBZ(&fileinfo, img_buf, bytesRead);
			break;
		default:
			break;
	}	
	if (ret > 0)
	{
		//DEBUG_LOG("%s: successfully opened '%s'", __FUNCTION__, name.c_str());
		fileBrowser->DisplayDiskInfo(diskImage, nullptr, &dir);
	}
	else
		DEBUG_LOG("%s: failed to open '%s'", __FUNCTION__, name.c_str());
out:		
	diskImage->Close();
	delete diskImage;
	return ret;
}

void drives_html(string &drives)
{
	extern const char* VolumeStr[];
	string res, check, mp;
	FILINFO fi;
	char scwd[256];
	const char *sdr = def_prefix.substr(0, def_prefix.find_first_of(':')).c_str();
	f_getcwd(scwd, 255);
	//DEBUG_LOG("%s: current working drive/dir = '%s %s'", __FUNCTION__, sdr, scwd);

	for (int i = 0; i < FF_VOLUMES; i++)
	{	
		int r;
		if ((r = f_chdrive(VolumeStr[i])) == FR_OK)
		{
			mp = string(VolumeStr[i]) + ":/1541";
			if (mp == def_prefix)
				sdr = VolumeStr[i];
			if ((r = f_stat(mp.c_str(), &fi)) != FR_OK)
			{
				if (r != FR_NOT_ENABLED) DEBUG_LOG("%s: f_stat of mountpoint '%s' failed (%d)", __FUNCTION__, mp.c_str(), r);
				continue;
			}
			if (def_prefix.find(VolumeStr[i]) == 0)
				check = " checked";
			else
				check = "";
			res = "<label><input type=\"radio\" name=\"choice\" value=\"" + string(VolumeStr[i]) + "\"" +check + ">" + string(VolumeStr[i]) + "</label>";
			//DEBUG_LOG("%s: found volume '%s' - %s", __FUNCTION__, VolumeStr[i], res.c_str());
			drives += res;
		}
		else
		{
			DEBUG_LOG("%s: chdrive to '%s' failed (%d)", __FUNCTION__, VolumeStr[i], r);
		}
	}
	f_chdrive(sdr);
	f_chdir(scwd);
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
	const u8 *pContent = 0;
	unsigned nLength = 0;
	char filename[255];
	char extension[10];
	char field[255];
	filename[0] = '\0';
	extension[0] = '\0';
	CString String;
	string mem;
	// enable HEAP_DEBUG in circle
	//CMemorySystem::DumpStatus();
	mem_stat(pPath, mem, true);
	//DEBUG_LOG("%s: pPath = '%s'", __FUNCTION__, pPath);
	//DEBUG_LOG("%s: pParams = '%s'", __FUNCTION__, pParams); // Attention when blanks in filename this may crash here
	
	// serve content of /web
	if (strncmp(pPath, "/web", 4) == 0)
	{
		string index;
		string fn = string("SD:") + pPath;
		DEBUG_LOG("%s: serving '%s'", __FUNCTION__, fn.c_str());
		FIL fp;
		UINT br;
		if (strcmp(pPath, "/web/update-web.html") == 0)
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
					string dfn = string("SD:/web/") + filename;
					if (write_file(dfn.c_str(), pPartData, nPartLength))
						msg = string("Successfully wrote <i>") + dfn + "</i>";
					else
						msg = string("Failed to write <i>") + dfn + "</i>";
				}
			}
			DEBUG_LOG("%s: %s", __FUNCTION__, msg.c_str());
			fn = "SD:/web/index.html";
		}

		if (f_open(&fp, fn.c_str(), FA_READ) == FR_OK)
		{
			f_read(&fp, (void *)pBuffer, *pLength, &br);

			if (*pLength < br)
			{
				DEBUG_LOG("%s: buffer too small for '%s' (%d < %d)", __FUNCTION__, fn.c_str(), *pLength, nLength);
				return HTTPInternalServerError;
			}
			DEBUG_LOG("%s: successfully read '%s', %db", __FUNCTION__, fn.c_str(), br);
			*pLength = br;
			f_close(&fp);
			if (endsWith(fn, ".html"))
				*ppContentType = "text/html; charset=UTF-8";
			else if (endsWith(fn, ".css"))
				*ppContentType = "text/css; charset=UTF-8";
			else if (endsWith(fn, ".js"))
				*ppContentType = "text/javascript; charset=UTF-8";	
			else if (endsWith(fn, ".png"))
				*ppContentType = "image/x-icon";
			else 
				*ppContentType = "text/plain; charset=UTF-8";
			return HTTPOK;
		}
		else
			return HTTPNotFound;
	}
	if (strcmp (pPath, "/") == 0 ||
		strcmp (pPath, "/index.html") == 0)
	{
		assert (pFormData != 0);
		string msg = "";
		char msg_str[1024];
		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;
		const char *pPartHeaderCB;
		const u8 *pPartDataCB;
		unsigned nPartLengthCB;
		string curr_dir, files;
		string curr_path = urlDecode(pParams);
		string page = "index.html";
		stringstream ss(pParams);
		string type, fops, ndir;
		string drives = "";
		drives_html(drives);
		getline(ss, type, '&');
		getline(ss, curr_path, '&');
		getline(ss, fops, '&');
		getline(ss, ndir, '&');
		type = urlDecode(type);
		curr_path = urlDecode(curr_path);
		fops = urlDecode(fops);
		bool noFormParts = true;
		//DEBUG_LOG("curr_path = '%s'", curr_path.c_str());
		//DEBUG_LOG("type = '%s'", type.c_str());
		//DEBUG_LOG("fops = '%s'", fops.c_str());
		if (fops == "[MKDIR]")
		{	
			FRESULT ret;
			ndir = urlDecode(ndir);
			string fullndir = def_prefix + curr_path + "/" + ndir;
			if ((ret = f_mkdir(fullndir.c_str())) != FR_OK)
				snprintf(msg_str, 1023,"Failed to create <i>%s</i> (%d)", fullndir.c_str(), ret);
			else
				snprintf(msg_str, 1023,"Successfully created <i>%s</i>", fullndir.c_str());
			DEBUG_LOG("%s: mkdir '%s' returned %d", __FUNCTION__, fullndir.c_str(), ret);
			msg = msg_str;
		}
		if (fops == "[DEL]")
		{	
			FRESULT ret;
			msg = "";
			ndir = urlDecode(ndir);
			string fullndir = def_prefix + curr_path + "/" + ndir;
			//DEBUG_LOG("%s: unlink '%s'", __FUNCTION__, fullndir.c_str());
			if ((ret = f_unlink_full(fullndir, msg)) != FR_OK)
				snprintf(msg_str, 1023,"Failed to delete <i>%s</i> (%d)", fullndir.c_str(), ret);
			else
				snprintf(msg_str, 1023,"Deleted <i>%s</i>", fullndir.c_str());
			msg += msg_str;
		}
		if (fops == "[NEWDISK]")
		{
			extern IEC_Commands *_m_IEC_Commands;
			int ret;
			const char *dt;
			ndir = urlDecode(ndir);
			string fullndir = def_prefix + curr_path + "/" + ndir;
			DEBUG_LOG("%s: create new disk '%s'", __FUNCTION__, ndir.c_str());
			if (endsWith(fullndir, ".g64") || endsWith(fullndir, ".G64"))
			{
				dt = "g64";
				_m_IEC_Commands->SetNewDiskType(DiskImage::G64);
				ret = _m_IEC_Commands->CreateNewDisk((char *)fullndir.c_str(), "42", true);
			}
			else if (endsWith(fullndir, ".d81") || endsWith(fullndir, ".D81"))
			{
				dt = "d81";
				_m_IEC_Commands->SetNewDiskType(DiskImage::D81);
				ret = _m_IEC_Commands->CreateNewDisk((char *)fullndir.c_str(), "42", true);
			}
			else 
			{
				dt = "d64";
				_m_IEC_Commands->SetNewDiskType(DiskImage::D64);
				ret = _m_IEC_Commands->CreateNewDisk((char *)fullndir.c_str(), "42", true);
			}
			if (ret)
				snprintf(msg_str, 1023,"Failed to create new %s image <i>%s</i>", dt, fullndir.c_str());
			else
				snprintf(msg_str, 1023,"Successfully created new %s image <i>%s</i>", dt, fullndir.c_str());
			msg = msg_str;
		}
		if (fops == "[NEWLST]")
		{
			int ret;
			ndir = urlDecode(ndir);
			string fullndir = def_prefix + curr_path + "/" + ndir;
			DEBUG_LOG("%s: create new lst file '%s'", __FUNCTION__, fullndir.c_str());
			if (!fileBrowser->MakeLSTFromDir((def_prefix + curr_path).c_str(), fullndir.c_str()))
				snprintf(msg_str, 1023,"Failed to create new LST file <i>%s</i>", fullndir.c_str());
			else
				snprintf(msg_str, 1023,"Successfully created new LST file <i>%s</i>", fullndir.c_str());
			msg = msg_str;
		}
		if (fops == "[DOWNLOAD]")
		{
			int ret;
			ndir = urlDecode(ndir);
			string fullndir = def_prefix + curr_path + "/" + ndir;
			DEBUG_LOG("%s: download file '%s'", __FUNCTION__, fullndir.c_str());
			download_file(fullndir, pContent, nLength, msg);
			*ppContentType = "application/octet-stream";
			goto out;
		}
		if (type == "[CHMEDIUM]")
		{
			int ret;
			DEBUG_LOG("%s: request to change medium to '%s'", __FUNCTION__, curr_path.c_str());
			if ((ret = f_chdrive(curr_path.c_str())) == FR_OK)
			{
				def_prefix = curr_path + ":/1541";
				snprintf(msg_str, 1023,"Successfully changed medium to <i>%s</i>", def_prefix.c_str());
				drives = "";
				drives_html(drives);
			}
			else
				snprintf(msg_str, 1023,"Failed to change medium to <i>%s</i> (%d)", curr_path.c_str(), ret);
			msg = msg_str;
			curr_path = "";
		}
		if (GetMultipartFormPart (&pPartHeaderCB, &pPartDataCB, &nPartLengthCB))
		{
			bool xpath_set = false;
			noFormParts = false;
			extract_field(" name=\"", pPartHeaderCB, field);
			extract_field("filename=\"", pPartHeaderCB, filename, extension);
			bool do_remount = false;
			//DEBUG_LOG("%s: pPartHeader = '%s', field = '%s', filename = '%s', length = %d", __FUNCTION__, pPartHeaderCB, field, filename, nPartLengthCB);

			if ((nPartLengthCB > 0) && (strcmp(field, "xpath") == 0) && (nPartLengthCB < 256))
			{
				char tmp[256];
				memcpy(tmp, pPartDataCB, nPartLengthCB);
				tmp[nPartLengthCB] = '\0';
				//DEBUG_LOG("%s: setting path from POST action to '%s'", __FUNCTION__, tmp);
				curr_path = string(tmp);
				//direntry_table(header_NT, curr_dir, curr_path, page, AM_DIR, true);
				xpath_set=true;
			}
			msg = "Uploading...<br />";
			while (!xpath_set || GetMultipartFormPart (&pPartHeaderCB, &pPartDataCB, &nPartLengthCB)) 
			{
				xpath_set = true; // from now, get new part
				extract_field(" name=\"", pPartHeaderCB, field);
				extract_field("filename=\"", pPartHeaderCB, filename, extension);
				//DEBUG_LOG("%s: pPartHeader = '%s', field = '%s', filename = '%s', length = %d", __FUNCTION__, pPartHeaderCB, field, filename, nPartLengthCB);
				if ((nPartLengthCB > 0) && (strcmp(field, "directory") == 0))
				{
					string fn, dir;
					char *fp = strrchr(filename, '/');
					if (fp)
					{
						*fp = '\0';
						fn = string(fp + 1);
						dir = curr_path + "/" + string(filename) + "/";
					}
					else
					{
						fn = string(filename);
						dir = curr_path + "/";
					}
					DEBUG_LOG("%s: dirname = '%s', fn = '%s'", __FUNCTION__, dir.c_str(), fn.c_str());
					FRESULT fr;
					f_mkdir_full(dir.c_str(), msg);
					DEBUG_LOG("%s: going to write '%s'", __FUNCTION__, (dir + fn).c_str());
					write_image(dir + fn, extension, pPartDataCB, nPartLengthCB, msg);
				}
				if ((nPartLengthCB > 0) && (strcmp(field, "diskimage") == 0))
				{
					string targetfn = curr_path + '/' + filename;

					if (GetMultipartFormPart (&pPartHeader, &pPartData, &nPartLength))
					{	
						extract_field(" name=\"", pPartHeader, field);
						if ((strcmp(field, "am-cb1") == 0) && (pPartData[0] == '1'))
						{
							DEBUG_LOG("%s: automount requested for diskimage '%s'", __FUNCTION__, filename);
							const char *_t = options.GetAutoMountImageName();
							if (_t && (_t = strrchr(_t, '.')) && (strcasecmp(_t + 1, extension) == 0))
							{
								Kernel.log("%s: image '%s' shall be used as automount image", __FUNCTION__, filename);
								targetfn = string("/") + string(options.GetAutoMountImageName());
								do_remount = true;
								msg += "automounting<br />";
							}
							else
							{	
								DEBUG_LOG("%s: automount image extension doesn't match '%s' vs. '.%s'", __FUNCTION__, _t, extension);
								msg += (string("automount extension mismatch, uploading image <i>") + targetfn + "</i><br />");
							}
						}
					}
					else
						DEBUG_LOG("%s: missing automount checkbox section for '%s'", __FUNCTION__, targetfn.c_str());
					DEBUG_LOG("%s: going to write '%s'", __FUNCTION__, targetfn.c_str());
					write_image(targetfn, extension, pPartDataCB, nPartLengthCB, msg);
	
					if (do_remount)
						webserver_upload = true; // this re-mounts the automount image
					break;
				}
			}
		}
		direntry_table(header_NT, curr_dir, curr_path, page, AM_DIR, true);
		direntry_table(header_NTD, files, curr_path, page, ~AM_DIR, true);
		String.Format(s_Index, drives.c_str(), msg.c_str(), curr_path.c_str(), (
			"<I>" + def_prefix + curr_path + "</i>").c_str(), curr_dir.c_str(), files.c_str(),
			Kernel.get_version(), mem.c_str(), 
			curr_path.c_str(), // mkdir script
			curr_path.c_str(), // newD64 script
			curr_path.c_str(), // newLST script
			curr_path.c_str()); // newTXT script

		pContent = (const u8 *)(const char *)String;
		nLength = String.GetLength();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp(pPath, "/pistats.html") == 0)
	{
		unsigned int temp;
		DiskImage *di;
		char din[256], cwd[256];
		di = diskCaddy.GetCurrentDisk();
		f_getcwd(cwd, sizeof(cwd));
		snprintf(din, 256, "%s/%s", cwd, (di ? di->GetName() : "None"));
		GetTemperature(temp);
		CString *t = Kernel.get_timer()->GetTimeString();
		String.Format("DeviceID: <i>%d</i><br />Current Diskimage: <i>%s</i><br />Pi Temp: <i>%dC @%ldMHz</i><br />Time: <i>%s</i>",
				 _m_IEC_Commands->GetDeviceId(),
				 din,
				 temp / 1000,
				 CPUThrottle.GetClockRate() / 1000000L,
				 t->c_str());
		delete t;
		pContent = (const u8 *)(const char *)String;
		nLength = String.GetLength();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp(pPath, "/getindex.html") == 0)
	{
		string index;
		gen_index(index, string(""));
		pContent = (const u8 *)index.c_str();
		nLength = index.length();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp(pPath, "/update.html") == 0)
	{
		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;

		string kernelname = "Unknown";
		string modelstr = "Unknown";
		string msg = "unknown error";

		CMachineInfo *mi = CMachineInfo::Get();
		CKernel::get_machine_info(kernelname);
		modelstr = string("Version & Model: <i>") + Kernel.get_version() + "</i>";
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
			msg = (modelstr + "<br />Kernelname: <i>" + kernelname + "</i>");
		}
		String.Format(s_update, msg.c_str(), Kernel.get_version(), mem.c_str());
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
		String.Format(s_update, msg.c_str(), Kernel.get_version(), mem.c_str());
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
			//DEBUG_LOG("%s: options upload requested, header = '%s'", __FUNCTION__, pPartHeader);
			extract_field("name=\"", pPartHeader, filename, extension);
			string dfn = string("SD:/") + filename;			

			f_unlink((dfn + ".BAK").c_str());	// unconditionally remove backup
			f_rename(dfn.c_str(), (dfn + ".BAK").c_str());
			if (write_file(dfn.c_str(), pPartData, nPartLength))
				msg = string("Successfully wrote <i>") + dfn + "</i><br />";
			else
				msg = string("Failed to write <i>") + dfn + "</i><br />";
		}
		string options = "";
		string config = "";
		string dfn = "SD:/options.txt";
		read_file(dfn, msg2, options);
		msg += msg2 + "<br />";
		dfn = "SD:/config.txt";
		read_file(dfn, msg2, config);
		msg += msg2;
		String.Format(s_edit_config, msg.c_str(), options.c_str(), config.c_str(), Kernel.get_version(), mem.c_str());
		pContent = (const u8 *)(const char *)String;
		nLength = String.GetLength();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp(pPath, "/edit-file.html") == 0)
	{
		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;
		string msg = "", msg2 = "";
		string curr_path = urlDecode(pParams);
		string curr_dir;
		//DEBUG_LOG("curr_path = %s", curr_path.c_str());
		//DEBUG_LOG("pParams = %s", pParams);		// attention if activated. 'ccgms 2021.d64' will fail due to %20 subsitution in encoding
		stringstream ss(pParams);
		string type, fops, fname;
		getline(ss, type, '&');
		getline(ss, curr_path, '&');
		getline(ss, fops, '&');
		getline(ss, fname, '&');
		curr_path = urlDecode(curr_path);
		type = urlDecode(type);
		char *from_edit = "-1";
		string dfn = def_prefix + curr_path;
		string textfile = "";
		//DEBUG_LOG("type = %s / curr_path = %s", type.c_str(), curr_path.c_str());
		if (GetMultipartFormPart(&pPartHeader, &pPartData, &nPartLength))
		{
			//DEBUG_LOG("%s: options upload requested, header = '%s'", __FUNCTION__, pPartHeader);
			extract_field("name=\"", pPartHeader, filename, extension);
			dfn = filename;			

			f_unlink((dfn + ".BAK").c_str());	// unconditionally remove backup
			f_rename(dfn.c_str(), (dfn + ".BAK").c_str());
			if (write_file(dfn.c_str(), pPartData, nPartLength))
				msg = string("Successfully wrote <i>") + dfn + "</i><br />";
			else
				msg = string("Failed to write <i>") + dfn + "</i><br />";
			from_edit = "-2"; // need to go two levels back
		}
		if (fops == "[NEWTXT]")
		{
			fname = urlDecode(fname);
			dfn = def_prefix + curr_path + "/" + fname;
			if (write_file(dfn.c_str(), (const u8 *)"", 0))
				msg += string("Successfully created new file <i>") + dfn + "</i><br />";
			else
				msg += string("Failed to create new file <i>") + dfn + "</i><br />";
		}
		if (DiskImage::IsEditableExtention(dfn.c_str()) == false)
		{
			msg += "File not editable!";
			String.Format(s_edit_file, msg.c_str(), 
				dfn.c_str(),
				"", // form name
				"", // textfile content
				"Back", "onclick=\"history.back(); return false;\"",	// just back button
				"-1", // need to go 1 level back
				Kernel.get_version(), mem.c_str());
		}
		else 
		{
			read_file(dfn, msg2, textfile);
			msg += msg2 + "<br />";
			String.Format(s_edit_file, msg.c_str(),
						  dfn.c_str(),
						  dfn.c_str(),
						  textfile.c_str(),
						  ("Save " + dfn).c_str(), "",
						  from_edit,
						  Kernel.get_version(), mem.c_str());
		}
		pContent = (const u8 *)(const char *)String;
		nLength = String.GetLength();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp(pPath, "/mount-imgs.html") == 0)
	{
		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;
		string msg = "";
		string content = "No image selected";
		list<string> dir;
		string curr_path = urlDecode(pParams);
		string curr_dir, files;
		string page = "mount-imgs.html";
		string cwd, img;
		string drives = "";
		drives_html(drives);
		//DEBUG_LOG("curr_path = %s", curr_path.c_str());
		//DEBUG_LOG("pParams = %s", pParams);		// attention if activated. 'ccgms 2021.d64' will fail due to %20 subsitution in encoding
		stringstream ss(pParams);
		string type, fops;
		bool mount_it = false;
		bool is_dir = false;
		FILINFO fi;
		getline(ss, type, '&');
		getline(ss, curr_path, '&');
		getline(ss, fops, '&');
		curr_path = urlDecode(curr_path);
		type = urlDecode(type);
		//DEBUG_LOG("type = %s / curr_path = %s", type.c_str(), curr_path.c_str());
		if (type == "[CHMEDIUM]")
		{
			int ret;
			char msg_str[1024];
			DEBUG_LOG("%s: request to change medium to '%s'", __FUNCTION__, curr_path.c_str());
			if ((ret = f_chdrive(curr_path.c_str())) == FR_OK)
			{
				def_prefix = curr_path + ":/1541";
				snprintf(msg_str, 1023, "Successfully changed medium to <i>%s</i>", def_prefix.c_str());
				drives = "";
				drives_html(drives);
			}
			else
				snprintf(msg_str, 1023, "Failed to change medium to <i>%s</i> (%d)", curr_path.c_str(), ret);
			msg = string(msg_str);
			curr_path = "";
			type = "[DIR]";
		}
		if (type == "[DIR]" || type == "") 
		{
			if ((direntry_table(header_NT, curr_dir, curr_path, page, AM_DIR) < 0) ||
				(direntry_table(header_NTD, files, curr_path, page, ~AM_DIR) < 0))
				return HTTPNotFound;
			is_dir = true;
		}
		else if (f_stat((def_prefix + curr_path).c_str(), &fi) != FR_OK)
		{
			msg = "No Image selected, nothing mounted";
			return HTTPNotFound;
		}
		else
		{
			if (fi.fattrib & AM_DIR)
				is_dir = true;
			if (type == "[MOUNT]")
			{
				if (!is_dir)
				{
					type = "[FILE]";
					mount_it = true;
				}
				else
				{
					msg = "No Image selected, nothing mounted";
					curr_path += '/';
				}
			}
			else if (type == "[RENAME]" && fops == "[NEWNAME]")
			{
				if (curr_path.length() == 0)
					msg = "Refusing to rename root directory <i>" + def_prefix + "</i>";
				else 
				{
					string newname;
					getline(ss, newname, '&');
					newname = def_prefix + urlDecode(newname);
					string oldname = def_prefix + curr_path;
					FRESULT ret;
					//DEBUG_LOG("%s: rename '%s' to '%s'", __FUNCTION__, oldname.c_str(), newname.c_str());
					if ((ret = f_rename(oldname.c_str(), newname.c_str())) != FR_OK)
					{
						msg = "Failed to rename <i>" + oldname + "</i> to <i>" + newname + "</i> (" + to_string(ret) + ")";
						DEBUG_LOG("%s: rename of '%s' to '%s' failed %d", __FUNCTION__, oldname.c_str(), newname.c_str(), ret);
					}
					else
					{
						msg = "Successfully renamed <i>" + oldname + "</i> to <i>" + newname + "</i>";
						DEBUG_LOG("%s: successfully renamed '%s' to '%s'", __FUNCTION__, oldname.c_str(), newname.c_str());
						curr_path = newname;
					}
				}
			}
			if (curr_path.find(def_prefix) != string::npos)
				curr_path = curr_path.substr(def_prefix.length(), curr_path.length());
			if (curr_path[0] != '/') curr_path = "/" + curr_path;
			const size_t idx = curr_path.rfind('/');
			if (idx == string::npos)
			{
				img = curr_path;
				cwd = "";
				curr_path = "/" + curr_path;
			}
			else
			{
				cwd = curr_path.substr(0, idx);
				img = curr_path.substr(idx + 1, curr_path.length());
			}
			string fullname = def_prefix + curr_path;
			if (type == "[DEL]")
			{
				if (is_dir)
					msg += "Refusing to delete directory <i>" + fullname + "</i>";
				else
				{
					FRESULT ret;
					//DEBUG_LOG("%s: delete of '%s'", __FUNCTION__, fullname.c_str());
					if ((ret = f_unlink_full(fullname, msg)) != FR_OK)
						msg += "Failed to delete <i>" + fullname + "</i> (" + to_string(ret) + ")";
					else
						msg += "Successfully deleted <i>" + fullname + "</i>";
				}
			}
			if (type == "[DOWNLOAD]")
			{
				if (is_dir)
				{
					msg = "Can't download directory '" + fullname + "'";
					pContent = (const u8 *)msg.c_str();
					nLength = msg.length();
					*ppContentType = "text/html; charset=iso-8859-1";
				}
				else
				{
					//DEBUG_LOG("%s: DOWNLOAD of '%s'", __FUNCTION__, fullname.c_str());
					download_file(fullname, pContent, nLength, msg);
					*ppContentType = "application/octet-stream";
				}
				goto out;
			}
			if (type == "[FILE]")
			{
				// store for main emulation
				strncpy(mount_path, (def_prefix + cwd).c_str(), 255);
				strncpy(mount_img, img.c_str(), 255);
				//DEBUG_LOG("%s: mount_img = '%s'", __FUNCTION__, fullname.c_str());
				content = "";
				// check if it's really an image
				if (DiskImage::IsPicFileExtention(mount_img))
				{
					content = string("<img src=\"") + urlEncode(curr_path) + "\"/>";
				}
				else if (DiskImage::IsTextFileExtention(mount_img)) // LSTs are also TextFiles
				{
					// assume a textfile to show, e.g. .LST file
					string tmp;
					read_file(fullname, tmp, content);
					msg = msg + tmp;
					replaceAll(content, "\r\n", "<br />");
					content = curr_path + ":<br /><br />" + content;
					if (mount_it && DiskImage::IsLSTExtention(mount_img))
					{
						msg = "Mounted <i>" + def_prefix + curr_path + "</i><br />";
						mount_new = 2; /* indicate .lst mount */
						MsDelay(100);
					}
					else
						msg = "Selected <i>" + def_prefix + curr_path + "</i><br />";	
				}
				else if (DiskImage::IsDiskImageExtention(mount_img))
				{
					if (read_dir(def_prefix + curr_path, dir) < 0)
					{
						//DEBUG_LOG("%s: failed to image content of '%s'", __FUNCTION__, mount_img);
						msg = "Failed to read image content of <i>'" + string(mount_img) + "'</i>.";
					}
					int revers = 0;
					for (auto it = dir.begin(); it != dir.end(); it++)
					{
						// ugly special first line handling to show revers
						if (it == dir.begin())
							revers = 128;
						else
							revers = 0;
						for (long unsigned int i = 0; i < it->length(); i++)
						{
							char buf[16];
							sprintf(buf, "&#x0ee%02x;", petscii2screen((*it)[i]) + revers);
							content += string(buf);
						}
						content += "<br />";
					}
					if (mount_it)
					{
						msg = "Mounted <i>" + def_prefix + curr_path + "</i><br />";
						mount_new = 1;	/* indicate image mount */
						MsDelay(100);
					}
					else
						msg = "Selected <i>" + def_prefix + curr_path + "</i><br />";						
				}
				else
				{
					if (mount_it)
						msg = "Refusing to mount unknown image type <it>" + fullname + "</i>";
					else
						msg = "Unknown image type <it>" + fullname + "</i>";
				}
			}
			if ((direntry_table(header_NT, curr_dir, cwd, page, AM_DIR) < 0) ||
				(direntry_table(header_NTD, files, cwd, page, ~AM_DIR) < 0))
				return HTTPNotFound;
		}
		static char _t[256];
		string encURL = urlEncode(curr_path);
		strcpy(_t, encURL.c_str());
		String.Format(s_mount, drives.c_str(), msg.c_str(), 
					(def_prefix + curr_path).c_str(), 
					(is_dir ? "<!--" : ""),
					_t, // Mount
					_t, // Edit
					_t, // Delete
					_t, img.c_str(), // Download
					(is_dir ? "-->" : ""),
					curr_dir.c_str(), files.c_str(), content.c_str(),
					Kernel.get_version(), mem.c_str(),
					curr_path.c_str(), curr_path.c_str(), _t // rename script
					);
		pContent = (const u8 *)(const char *)String;
		nLength = String.GetLength();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp(pPath, "/logger.html") == 0)
	{
		if (strcmp(pParams, "[DOWNLOAD]") == 0)
		{
			DEBUG_LOG("%s: logger download, pParams = %s", __FUNCTION__, pParams);
			pContent = (const u8*)(logger.get_bootlogs(false) + logger.get_logs(false)).c_str();
			nLength = strlen((const char *)pContent);
			*ppContentType = "application/octet-stream";
			goto out;
		}
		String.Format(s_logger, logger.get_bootlogs().c_str(), logger.get_log_count(), logger.get_logs().c_str(), Kernel.get_version(), mem.c_str());
		pContent = (const u8 *)(const char *)String;
		nLength = String.GetLength();
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp(pPath, "/reset.html") == 0)
	{
		string msg;
extern int reboot_req;
		DEBUG_LOG("%s: reset requested.", __FUNCTION__);
		msg = "Reboot requested...";
		reboot_req++;
		String.Format(s_status, msg.c_str(), Kernel.get_version(), mem.c_str());
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
	else if ((endsWith(string(pPath), ".png")) || (endsWith(string(pPath), ".jpg")))
	{
		FIL fp;
		UINT br;
		string fn;
		fn = urlDecode(string(pPath));
		if (fn.find(def_prefix) == string::npos)
			fn = def_prefix + fn;
		//DEBUG_LOG("%s: icon: '%s'", __FUNCTION__, fn.c_str());
		if (f_open(&fp, fn.c_str(), FA_READ) == FR_OK)
		{
			f_read(&fp, (void *)icon_buf, MAX_ICON_SIZE, &br);
			pContent = (const unsigned char *)icon_buf;
			nLength = br;
			*ppContentType = "image/x-icon";
			f_close(&fp);
		}
		else
		{
			pContent = s_Favicon;
			nLength = sizeof s_Favicon;
			*ppContentType = "image/x-icon";
		}
	}
	else if (strcmp (pPath, "/favicon.ico") == 0)
	{
		pContent = s_Favicon;
		nLength = sizeof s_Favicon;
		*ppContentType = "image/x-icon";
	}
	else if (strcmp (pPath, "/C64_Pro_Mono.ttf") == 0)
	{
		pContent = s_font;
		nLength = sizeof s_font;
		*ppContentType = "font/ttf";
	}
	else if (strcmp(pPath, "/pi1541Version.html") == 0)
	{
		char buf[256];
		sprintf(buf, "%s, %s", Kernel.get_version(), mem.c_str());
		nLength = strlen(buf);
		pContent = (const u8 *)buf;
		*ppContentType = "text/html; charset=UTF-8";
	}
	else
	{
		return HTTPNotFound;
	}
out:
	//mem_stat("GETCONTENT:OUT", mem, false);
	//DEBUG_LOG("%s: page size = %d", __FUNCTION__, nLength);
	assert (pLength != 0);
	if (*pLength < nLength)
	{
		Kernel.log("%s: Increase MAX_CONTENT_SIZE to at least %u", __FUNCTION__, nLength);
		DisplayMessage(0, 0, true, "Page too large", RGBA(0xff, 0xff, 0xff, 0xff), RGBA(0xff, 0, 0, 0xff));

		return HTTPRequestEntityTooLarge;
	}

	assert (pBuffer != 0);
	assert (pContent != 0);
	assert (nLength > 0);
	memcpy (pBuffer, pContent, nLength);

	*pLength = nLength;
	return HTTPOK;
}
