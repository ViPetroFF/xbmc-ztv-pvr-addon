/*
 *      Copyright (C) 2017 Viktor PetroFF
 *      Copyright (C) 2011 Pulse-Eight
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301  USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "client.h"
#include "xbmc_pvr_dll.h"
#include "PVRDemoData.h"
#include <p8-platform/util/util.h>

using namespace std;
using namespace ADDON;

enum EChannelsSource
{
  m3u = 0,
  online,
};

enum EM3uType
{
  file = 0,
  url,
};

const EChannelsSort   DEF_CHANNELS_SORT   = EChannelsSort::none;
const EChannelsSource DEF_CHANNELS_SOURCE = EChannelsSource::m3u;
const EM3uType DEF_CHANNELS_TYPE   = EM3uType::file;
const EInputStreamHandler DEF_STREAM_HANDLER = EInputStreamHandler::ztv;
const char* DEF_MAC_TEXT          = "00:00:00:00:00:00";
const char* DEF_CA_TEXT           = "";
const char* DEF_CA_URI            = "http://localhost:7781/ca/";
const char* DEF_M3U_FILE          = "pvr.ztv/iptv.m3u8";
const char* DEF_M3U_TEXT          = "special://home/addons/pvr.ztv/iptv.m3u8";
const char* DEF_MCASTIF           = "255.255.255.255";
const char* DEF_IPADDR_PROXY      = "127.0.0.1";
const char* DEF_HOST              = "localhost";
const int   DEF_PORT_PROXY        = 7781;
const bool  DEF_ENABLE_ONLINE_GRP = true;
const bool  DEF_ENABLE_ONLINE_EPG = true;
const bool  DEF_ENABLE_SUPPORT_CA = false;
const bool  DEF_ENABLE_PROXY_UDP  = false;

bool           m_bCreated       = false;
ADDON_STATUS   m_CurStatus      = ADDON_STATUS_UNKNOWN;
PVRDemoData*   m_data           = NULL;
bool           m_bIsPlaying     = false;
bool           m_bCaSupport     = false;
PVRDemoChannel m_currentChannel;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string g_strUserPath             = "";
std::string g_strClientPath           = "";

EChannelsSort   g_eChannelsSort       = DEF_CHANNELS_SORT;
EChannelsSource g_eChannelsSource     = DEF_CHANNELS_SOURCE;
EM3uType g_eChannelsType              = DEF_CHANNELS_TYPE;
EInputStreamHandler g_eStreamHandler  = DEF_STREAM_HANDLER;
string g_strMacText                   = DEF_MAC_TEXT;
string g_strCaText                    = DEF_CA_TEXT;
string g_strM3uText                   = DEF_M3U_TEXT;
string g_strMCastIf                   = DEF_MCASTIF;
string g_strIpAddrProxy               = DEF_IPADDR_PROXY;
string g_strHostname                  = DEF_HOST;
string g_strConnection                = DEF_HOST;
int    g_iPortProxy                   = DEF_PORT_PROXY;
bool g_bEnableOnLineGroups            = DEF_ENABLE_ONLINE_GRP;
bool g_bEnableOnLineEpg               = DEF_ENABLE_ONLINE_EPG;
bool g_bEnableSupportCa               = DEF_ENABLE_SUPPORT_CA;
bool g_bEnableUDPProxy                = DEF_ENABLE_PROXY_UDP;

CHelper_libXBMC_addon *XBMC           = NULL;
CHelper_libXBMC_pvr   *PVR            = NULL;

extern "C" {

void ADDON_ReadSettings(void);

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!hdl || !props)
    return ADDON_STATUS_UNKNOWN;

  PVR_PROPERTIES* pvrprops = (PVR_PROPERTIES*)props;

  XBMC = new CHelper_libXBMC_addon;
  if (!XBMC->RegisterMe(hdl))
  {
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  PVR = new CHelper_libXBMC_pvr;
  if (!PVR->RegisterMe(hdl))
  {
    SAFE_DELETE(PVR);
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  XBMC->Log(LOG_DEBUG, "%s - Creating the PVR ztv add-on", __FUNCTION__);

  m_CurStatus     = ADDON_STATUS_UNKNOWN;
  g_strUserPath   = pvrprops->strUserPath;
  g_strClientPath = pvrprops->strClientPath;

  ADDON_ReadSettings();

  m_data = new PVRDemoData(g_eStreamHandler, g_bEnableOnLineEpg, g_strMCastIf.c_str());
  bool bIsOnLine = (EChannelsSource::online == g_eChannelsSource);

  if (bIsOnLine)
  {
	  g_strHostname = m_data->GetCaServerHostname();
	  g_strConnection = m_data->GetCaServerUri();
  }

  string strProxyHost;

  if(g_bEnableUDPProxy)
  {
	  strProxyHost = m_data->ProxyAddrInit(g_strIpAddrProxy.c_str(), g_iPortProxy, g_bEnableSupportCa);
  }
  else if(g_bEnableSupportCa)
  {
	if(g_strCaText.empty())
	{
		g_strCaText= DEF_CA_URI;
	}

	m_bCaSupport = m_data->VLCInit(g_strCaText.c_str());

	if (!m_bCaSupport)
	{
		m_CurStatus = ADDON_STATUS_LOST_CONNECTION;
	}
  }

  if(ADDON_STATUS_UNKNOWN == m_CurStatus)
  {
	  string strM3uPath = (EChannelsSource::m3u == g_eChannelsSource)?g_strM3uText:g_strMacText;

	  bool bSuccess = m_data->LoadChannelsData(strM3uPath, bIsOnLine, g_bEnableOnLineGroups, g_eChannelsSort);
	  if (bSuccess)
	  {
			string strHostname = DEF_HOST;
			if (EChannelsSource::m3u == g_eChannelsSource)
			{
				if (EM3uType::file == g_eChannelsType)
				{
					char szHost[256] = {0};
					int rc = gethostname(szHost, sizeof(szHost)-1);
					if (!rc)
					{
						strHostname = szHost;
					}
				}
				else
				{
					strHostname = strM3uPath;
					string::size_type ndx = strHostname.find(':');
					if (string::npos != ndx)
					{
						strHostname = strHostname.substr(ndx + 1);
						ndx = strHostname.find_first_not_of("/\\");
					}
					if (string::npos != ndx)
					{
						strHostname = strHostname.substr(ndx);
						ndx = strHostname.find_first_of("/\\");
						strHostname = strHostname.substr(0, ndx);
						ndx = strHostname.rfind('@');
						if (string::npos == ndx)
						{
							ndx = strHostname.rfind(':');
						}
						else
						{
							strHostname = strHostname.substr(ndx + 1);
							ndx = strHostname.find(':');
						}
					}
					if (string::npos != ndx)
					{
						strHostname = strHostname.substr(0, ndx);
					}
				}
			}

			if (DEF_HOST == g_strHostname)
			{
				g_strHostname = strHostname;
			}

			bool bMac = false;

			if (DEF_HOST == g_strConnection)
			{
				g_strConnection.erase();
			}
			else
			{
				if (bIsOnLine)
				{
					g_strConnection += "|mac=";
					bMac = true;
				}
			}

			g_strConnection += strM3uPath;

			if (g_bEnableUDPProxy)
			{
				g_strConnection += (bMac)?"&":"|";
				g_strConnection += "proxy=" + strProxyHost;
			}
	  }

      m_CurStatus = (bSuccess)?ADDON_STATUS_OK:ADDON_STATUS_LOST_CONNECTION;
  }

  m_bCreated = true;

  return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus()
{
  return m_CurStatus;
}

void ADDON_Destroy()
{
  if (m_bCaSupport)
  {
    m_data->FreeVLC();
  }

  delete m_data;
  m_bCreated = false;

  m_CurStatus = ADDON_STATUS_UNKNOWN;
}

void ADDON_ReadSettings(void)
{
	/* Read setting "host" from settings.xml */
	char buffer[512];

	if (!XBMC)
		return;

	/* Source settings */
	/***********************/
	if (!XBMC->GetSetting("chansort", &g_eChannelsSort))
	{
		/* If setting is unknown fallback to defaults */
		XBMC->Log(LOG_ERROR, "Couldn't get 'chansort' setting, falling back to 'unsorted' as default");
		//g_eChannelsSort = DEF_CHANNELS_SORT;
	}

	if (!XBMC->GetSetting("chansource", &g_eChannelsSource))
	{
		/* If setting is unknown fallback to defaults */
		XBMC->Log(LOG_ERROR, "Couldn't get 'chansource' setting, falling back to 'm3u' as default");
		//g_eChannelsSource = DEF_CHANNELS_SOURCE;
	}

	if (EChannelsSource::m3u == g_eChannelsSource)
	{
		if (!XBMC->GetSetting("m3utype", &g_eChannelsType))
		{
			/* If setting is unknown fallback to defaults */
			XBMC->Log(LOG_ERROR, "Couldn't get 'm3utype' setting, falling back to 'file' as default");
			//g_eChannelsType = DEF_CHANNELS_TYPE;
		}

		if (EM3uType::file == g_eChannelsType)
		{
			/* Read setting "filem3u" from settings.xml */
			if (XBMC->GetSetting("filem3u", &buffer))
			{
				g_strM3uText = buffer;
			}
			else
			{
				/* If setting is unknown fallback to defaults */
				XBMC->Log(LOG_ERROR, "Couldn't get 'filem3u' setting, falling back to 'iptv.m3u8' as default");
				g_strM3uText = DEF_M3U_TEXT;
				g_strHostname = DEF_M3U_FILE;
			}
		}
		else
		{
			/* Read setting "urlm3u" from settings.xml */
			if (XBMC->GetSetting("urlm3u", &buffer))
			{
				g_strM3uText = buffer;
			}
			else
			{
				/* If setting is unknown fallback to defaults */
				XBMC->Log(LOG_ERROR, "Couldn't get 'urlm3u' setting, falling back to 'iptv.m3u8' as default");
				g_strM3uText = DEF_M3U_TEXT;
				g_strHostname = DEF_M3U_FILE;
			}
		}
	}
	else
	{
		/* Read setting "ca" from settings.xml */
		if (XBMC->GetSetting("mac", &buffer))
		{
			g_strMacText = buffer;
		}
		else
		{
			/* If setting is unknown fallback to defaults */
			XBMC->Log(LOG_ERROR, "Couldn't get 'mac' setting, falling back to '00:00:00:00:00:00' as default");
			//g_strMacText = DEF_MAC_TEXT;
		}
	}

	/* Read setting "proxyenable" from settings.xml */
	if (!XBMC->GetSetting("proxyenable", &g_bEnableUDPProxy))
	{
		/* If setting is unknown fallback to defaults */
		XBMC->Log(LOG_ERROR, "Couldn't get 'proxyenable' setting, falling back to 'false' as default");
		//g_bEnableUDPProxy = DEF_ENABLE_PROXY_UDP;
	}

	if (g_bEnableUDPProxy)
	{
		/* Read setting "proxyipaddr" from settings.xml */
		if (XBMC->GetSetting("proxyipaddr", &buffer))
		{
			g_strIpAddrProxy = buffer;
		}
		else
		{
			/* If setting is unknown fallback to defaults */
			XBMC->Log(LOG_ERROR, "Couldn't get 'proxyipaddr' setting, falling back to '127.0.0.1' as default");
			//g_strIpAddrProxy = DEF_IPADDR_PROXY;
		}

		/* Read setting "port" from settings.xml */
		if (!XBMC->GetSetting("proxyport", &g_iPortProxy))
		{
			/* If setting is unknown fallback to defaults */
			XBMC->Log(LOG_ERROR, "Couldn't get 'proxyport' setting, falling back to '7781' as default");
			//g_iPortProxy = DEF_PORT_PROXY;
		}
	}

	/* Read setting "groupenable" from settings.xml */
	if (!XBMC->GetSetting("groupenable", &g_bEnableOnLineGroups))
	{
		/* If setting is unknown fallback to defaults */
		XBMC->Log(LOG_ERROR, "Couldn't get 'groupenable' setting, falling back to 'true' as default");
		//g_bEnableOnLineGroups = DEF_ENABLE_ONLINE_GRP;
	}

	/* Read setting "epgenable" from settings.xml */
	if (!XBMC->GetSetting("epgenable", &g_bEnableOnLineEpg))
	{
		/* If setting is unknown fallback to defaults */
		XBMC->Log(LOG_ERROR, "Couldn't get 'epgenable' setting, falling back to 'true' as default");
		//g_bEnableOnLineEpg = DEF_ENABLE_ONLINE_EPG;
	}

	/* Read setting "caoffline" from settings.xml */
	if (!XBMC->GetSetting("caenable", &g_bEnableSupportCa))
	{
		/* If setting is unknown fallback to defaults */
		XBMC->Log(LOG_ERROR, "Couldn't get 'caenable' setting, falling back to 'false' as default");
		//g_bEnableSupportCa = DEF_ENABLE_SUPPORT_CA;
	}

	/* Read setting "ca" from settings.xml */
	if (XBMC->GetSetting("ca", &buffer))
	{
		g_strCaText = buffer;
	}
	else
	{
		/* If setting is unknown fallback to defaults */
		XBMC->Log(LOG_ERROR, "Couldn't get 'ca' setting, falling back to '' as default");
		//g_strCaText = DEF_CA_TEXT;
	}

	/* Read setting "mcastif" from settings.xml */
	if (XBMC->GetSetting("mcastif", &buffer))//&g_ulMCastIf))
	{
		g_strMCastIf = buffer;
	}
	else
	{
		/* If setting is unknown fallback to defaults */
		XBMC->Log(LOG_ERROR, "Couldn't get 'mcastif' setting, falling back to '255.255.255.255' as default");
		//g_strMCastIf = DEF_MCASTIF;
	}

	if (!XBMC->GetSetting("inputstream", &g_eStreamHandler))
	{
		/* If setting is unknown fallback to defaults */
		XBMC->Log(LOG_ERROR, "Couldn't get 'inputstream' setting, falling back to 'ztv' as default");
		//g_eStreamHandler = DEF_STREAM_HANDLER;
	}
	else if(g_bEnableSupportCa && !g_bEnableUDPProxy)
	{
		g_eStreamHandler = EInputStreamHandler::ztv;
	}

	/* Log the current settings for debugging purposes */
	XBMC->Log(LOG_DEBUG, "settings: chansource='%u', epgenable=%u", g_eChannelsSource, g_bEnableOnLineEpg);
}

bool ADDON_HasSettings()
{
  return true;
}

unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{  
  return 0;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
#if 0
  string str = settingName;

  if (!XBMC)
    return ADDON_STATUS_OK;

  if (str == "chansort")
  {
    if (g_eChannelsSort != *(EChannelsSort*) settingValue)
    {
      XBMC->Log(LOG_INFO, "Changed setting 'chansort' from %u to %u", g_eChannelsSort, *(int*) settingValue);
      g_eChannelsSort = *(EChannelsSort*) settingValue;
    }
	else
	{
	  return ADDON_STATUS_OK;
	}
  }
  else if (str == "chansource")
  {
    if (g_eChannelsSource != *(EChannelsSource*) settingValue)
    {
      XBMC->Log(LOG_INFO, "Changed setting 'chansource' from %u to %u", g_eChannelsSource, *(int*) settingValue);
      g_eChannelsSource = *(EChannelsSource*) settingValue;
    }
	else
	{
	  return ADDON_STATUS_OK;
	}
  }
  else if (str == "proxyenable")
  {
    XBMC->Log(LOG_INFO, "Changed setting 'proxyenable' from %u to %u", g_bEnableUDPProxy, *(bool*) settingValue);
    g_bEnableUDPProxy = *(bool*) settingValue;
  }
  else if (str == "groupenable")
  {
    XBMC->Log(LOG_INFO, "Changed setting 'groupenable' from %u to %u", g_bEnableOnLineGroups, *(bool*) settingValue);
    g_bEnableOnLineGroups = *(bool*) settingValue;
  }
  else if (str == "epgenable")
  {
    XBMC->Log(LOG_INFO, "Changed setting 'epgenable' from %u to %u", g_bEnableOnLineEpg, *(bool*) settingValue);
    g_bEnableOnLineEpg = *(bool*) settingValue;
  }
  else if (str == "caenable")
  {
    XBMC->Log(LOG_INFO, "Changed setting 'caenable' from %u to %u", g_bEnableSupportCa, *(bool*) settingValue);
    g_bEnableSupportCa = *(bool*) settingValue;
  }
  else if (str == "mac")
  {
    XBMC->Log(LOG_INFO, "Changed setting 'mac' from '%s' to '%s'", g_strMacText.c_str(), (const char*) settingValue);
    g_strMacText = (const char*) settingValue;
  }
  else if (str == "ca")
  {
    XBMC->Log(LOG_INFO, "Changed setting 'ca' from '%s' to '%s'", g_strCaText.c_str(), (const char*) settingValue);
    g_strCaText = (const char*) settingValue;
  }
  else if (str == "mcastif")
  {
	XBMC->Log(LOG_INFO, "Changed setting 'mcastif' from %s to %s", g_strMCastIf.c_str(), (const char*) settingValue);
	g_strMCastIf = (const char*) settingValue;
  }
  else if (str == "proxyipaddr")
  {
	XBMC->Log(LOG_INFO, "Changed setting 'proxyipaddr' from %s to %s", g_strIpAddrProxy.c_str(), (const char*) settingValue);
	g_strIpAddrProxy = (const char*) settingValue;
  }
  else if (str == "proxyport")
  {
    XBMC->Log(LOG_INFO, "Changed Setting 'proxyport' from %u to %u", g_iPortProxy, *(int*) settingValue);
    if (g_iPortProxy != *(int*) settingValue)
    {
      g_iPortProxy = *(int*) settingValue;
    }
  }
#endif // 0
  return ADDON_STATUS_NEED_RESTART;
}

void ADDON_Stop()
{
}

void ADDON_FreeSettings()
{
}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

void OnSystemSleep()
{
}

void OnSystemWake()
{
}

void OnPowerSavingActivated()
{
}

void OnPowerSavingDeactivated()
{
}

const char* GetPVRAPIVersion(void)
{
  static const char *strApiVersion = XBMC_PVR_API_VERSION;
  return strApiVersion;
}

const char* GetMininumPVRAPIVersion(void)
{
  static const char *strMinApiVersion = XBMC_PVR_MIN_API_VERSION;
  return strMinApiVersion;
}

const char* GetGUIAPIVersion(void)
{
  static const char *strGuiApiVersion = ""; // GUI API not used KODI_GUILIB_API_VERSION;
  return strGuiApiVersion;
}

const char* GetMininumGUIAPIVersion(void)
{
	static const char *strMinGuiApiVersion = ""; // GUI API not used KODI_GUILIB_MIN_API_VERSION;
  return strMinGuiApiVersion;
}

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
  pCapabilities->bSupportsEPG             = true;
  pCapabilities->bSupportsTV              = true;
  pCapabilities->bSupportsRadio           = true;
  pCapabilities->bSupportsChannelGroups   = true;
  pCapabilities->bSupportsRecordings      = false;
  pCapabilities->bHandlesInputStream      = true;

  return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{
  static CStdString strBackendName = CStdString(GetBackendHostname()) + " " + ((EChannelsSource::online == g_eChannelsSource)?"(interzet cas server)":"(m3u file)");
  //static const char *strBackendName = "VP ztv pvr add-on";
  return strBackendName.c_str();
}

const char *GetBackendVersion(void)
{
  static const char *strBackendName = "1.1";
  return strBackendName;
}

const char *GetConnectionString(void)
{
  //static CStdString strConnectionString = "connected";
  //return strConnectionString.c_str();
	return g_strConnection.c_str();
}

const char *GetBackendHostname(void)
{
	//return "localhost";
	return g_strHostname.c_str();
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
  //*iTotal = 1024 * 1024 * 1024;
  //*iUsed  = 0;
	return PVR_ERROR_NOT_IMPLEMENTED;// PVR_ERROR_SERVER_ERROR; PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  if (m_data)
    return m_data->GetEPGForChannel(handle, channel, iStart, iEnd);

  return PVR_ERROR_SERVER_ERROR;
}

int GetChannelsAmount(void)
{
  if (m_data)
    return m_data->GetChannelsAmount();

  return -1;
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  if (m_data)
    return m_data->GetChannels(handle, bRadio);

  return PVR_ERROR_SERVER_ERROR;
}

/*******************************************/
/** PVR Live Stream Functions             **/

bool OpenLiveStream(const PVR_CHANNEL &channelinfo)
{
  if (!m_data)
    return false;
  else
    return m_data->OpenLiveStream(channelinfo);
}

void CloseLiveStream()
{
  if (m_data)
    m_data->CloseLiveStream();
}

bool SwitchChannel(const PVR_CHANNEL &channelinfo)
{
  if (!m_data)
    return false;
  else
    return m_data->SwitchChannel(channelinfo);
}

int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  if (!m_data)
    return 0;
  else
    return m_data->ReadLiveStream(pBuffer, iBufferSize);
}

int GetCurrentClientChannel()
{
  if (!m_data)
    return 0;
  else
    return m_data->GetCurrentClientChannel();
}

const char * GetLiveStreamURL(const PVR_CHANNEL &channel)
{
  if (!m_data)
    return NULL;
  else
    return m_data->GetLiveStreamURL(channel);

  //return NULL;
}

bool CanPauseStream(void)
{
  if (!m_data)
    return false;
  else
    return m_data->CanPauseStream();
}

void PauseStream(bool bPaused)
{
	if (m_data)
		m_data->PauseStream(bPaused);
}

bool CanSeekStream(void)
{
  if (!m_data)
    return false;
  else
    return m_data->CanSeekStream();
}

bool IsTimeshifting(void)
{
	if (!m_data)
		return false;
	else
		return m_data->IsTimeshifting();// m_data->CanPauseStream();
}

time_t GetPlayingTime()
{
	if (!m_data)
		return 0;
	else
		return m_data->GetPlayingTime();
}

time_t GetBufferTimeStart()
{
	if (!m_data)
		return 0;
	else
		return m_data->GetBufferTimeStart();
}

time_t GetBufferTimeEnd()
{
	if (!m_data)
		return 0;
	else
		return m_data->GetBufferTimeEnd();
}

bool IsRealTimeStream(void)
{
	if (!m_data)
		return true;
	else
		return m_data->IsRealTimeStream();
}

PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

int GetChannelGroupsAmount(void)
{
  if (m_data)
    return m_data->GetChannelGroupsAmount();

  return -1;
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  if (m_data)
    return m_data->GetChannelGroups(handle, bRadio);

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  if (m_data)
    return m_data->GetChannelGroupMembers(handle, group);

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
  //snprintf(signalStatus.strAdapterName, sizeof(signalStatus.strAdapterName), "pvr ztv iptv adapter 1");
  //snprintf(signalStatus.strAdapterStatus, sizeof(signalStatus.strAdapterStatus), "OK");

	return PVR_ERROR_NOT_IMPLEMENTED;//PVR_ERROR_NO_ERROR;
}

/** UNUSED API FUNCTIONS */
PVR_ERROR OpenDialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingsAmount(bool deleted) { return -1; }
PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted) { return PVR_ERROR_NOT_IMPLEMENTED; }
//PVR_ERROR DialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR CallMenuHook(const PVR_MENUHOOK &menuhook, const PVR_MENUHOOK_DATA &item) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR MoveChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
//PVR_ERROR DialogChannelSettings(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
//PVR_ERROR DialogAddChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
bool OpenRecordedStream(const PVR_RECORDING &recording) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */) { return 0; }
long long PositionRecordedStream(void) { return -1; }
long long LengthRecordedStream(void) { return 0; }
void DemuxReset(void) {}
void DemuxFlush(void) {}
long long SeekLiveStream(long long iPosition, int iWhence /* = SEEK_SET */) { return -1; }
long long PositionLiveStream(void) { return -1; }
long long LengthLiveStream(void) { return -1; }
PVR_ERROR DeleteRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording) { return -1; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; };
PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetTimersAmount(void) { return -1; }
PVR_ERROR GetTimers(ADDON_HANDLE handle) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR AddTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UpdateTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
void DemuxAbort(void) {}
DemuxPacket* DemuxRead(void) { return NULL; }
unsigned int GetChannelSwitchDelay(void) { return 0; }
bool SeekTime(double, bool, double*) { return false; }
void SetSpeed(int speed) {};
PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }
}
