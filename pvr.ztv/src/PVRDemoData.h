#pragma once
/*
 *      Copyright (C) 2017 Viktor PetroFF
 *      Copyright (C) 2011 Pulse-Eight
 *      http://www.pulse-eight.com/
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

#include <map>
#include <vector>
#include "p8-platform/util/StdString.h"
//#include "platform/threads/mutex.h"
#include "client.h"

/*!
* @brief PVR macros for string exchange
*/
//#define PVR_STRCPY(dest, source) do { strncpy(dest, source, sizeof(dest)-1); dest[sizeof(dest)-1] = '\0'; } while(0)
//#define PVR_STRCLR(dest) memset(dest, 0, sizeof(dest))

enum EChannelsSort
{
  none = 0,
  id,
  name,
  ip,
  uri,
};

enum EInputStreamHandler
{
	ztv = 0,
	kodi,
};

struct PVRDemoEpgEntry
{
  int         iBroadcastId;
  int         iChannelId;
  int         iGenreType;
  int         iGenreSubType;
  int         iParentalRating;
  time_t      startTime;
  time_t      endTime;
  std::string strTitle;
  std::string strPlotOutline;
  std::string strPlot;
  std::string strIconPath;
};

struct PVRDemoChannel
{
  bool                    bRadio;
  int                     iUniqueId;
  int                     iChannelNumber;
  int                     iGroupId;
  unsigned long           ulIpNumber;
  unsigned long           ulIpAndPortNumber;
  bool                    bIsTcpTransport;
  int                     iEncryptionSystem;
  std::string             strChannelName;
  std::string             strIconPath;
  std::string             strStreamURL;

  bool operator<(const PVRDemoChannel& other) const { return (iChannelNumber < other.iChannelNumber); }
  bool operator==(const PVRDemoChannel& other) const { return (iUniqueId == other.iUniqueId); }
};

struct PVRDemoChannelGroup
{
  bool             bRadio;
  int              iGroupId;
  std::string      strGroupName;
  std::vector<int> members;
};

namespace LibVLCCAPlugin
{
class ILibVLCModule;
} // LibVLCCAPlugin

class PVRDemoData
{
public:
  PVRDemoData(EInputStreamHandler handler, bool bIsEnableOnLineEpg, LPCSTR lpszMCastIf);
  ~PVRDemoData(void);

  bool VLCInit(LPCSTR lpszCA);
  void FreeVLC(void);
  std::string ProxyAddrInit(LPCSTR lpszIP, int iPort, bool bCaSupport);
  bool LoadChannelsData(const std::string& strM3uPath, bool bIsOnLineSource, bool bIsEnableOnLineGroups, EChannelsSort sortby);
  int GetChannelsAmount(void);
  PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
  bool GetChannel(const PVR_CHANNEL &channel, PVRDemoChannel &myChannel);

  int GetChannelGroupsAmount(void);
  PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio);
  PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);

  PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd);
  PVR_ERROR RequestWebEPGForChannel(ADDON_HANDLE handle, const PVRDemoChannel& channel, time_t iStart, time_t iEnd);

  bool OpenLiveStream(const PVR_CHANNEL &channelinfo);
  void CloseLiveStream();
  bool SwitchChannel(const PVR_CHANNEL &channelinfo);
  int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize);
  int GetCurrentClientChannel();
  const char * GetLiveStreamURL(const PVR_CHANNEL &channel);
  bool CanPauseStream();
  void PauseStream(bool bPaused);
  bool CanSeekStream();
  bool IsTimeshifting();
  time_t GetPlayingTime();
  time_t GetBufferTimeStart();
  time_t GetBufferTimeEnd();
  bool IsRealTimeStream();

  std::string GetIconPath(LPCSTR lpszIcoFName) const;

  const char* GetCaServerHostname() const { return ZTV_CASERVER_HOSTNAME; }
  const char* GetCaServerUri() const { return ZTV_CASERVER_URI; }
protected:
  bool LoadWebXmlData(const std::string& strMac, bool bIsEnableOnLineGroups);
  bool LoadWebXmlGroups();
  bool LoadWebXmlChannels(const std::string& strMac);
  bool LoadM3UList(const std::string& strM3uUri);
  int DoHttpRequest(const CStdString& resource, const CStdString& body, CStdString& response);
  CStdString PVRDemoData::ReadMarkerValue(std::string &strLine, const char* strMarkerName);
  int GetChannelId(const char * strStreamUrl);
  int GetChannelId(unsigned int uiChannelId);
private:
  static const char* ZTV_CASERVER_HOSTNAME;
  static const char* ZTV_CASERVER_URI;
  static const char* ZTV_EPGSERVER_URI;

  struct AverageSpeed
  {

	  const double MIN_DOWNLOAD_SPEED = 125000.0f;

	  AverageSpeed()
	  {
		  _llLength = 0;
		  _timeDuration = 0;
	  }

	  double GetAverage() const
	  {
		  //double speed = (0 == _llLength) ? 0 : (_llLength / _timeDuration);
		  //return (speed > 0 && speed < MIN_DOWNLOAD_SPEED)? MIN_DOWNLOAD_SPEED:speed;
		  return (0 == _llLength) ? 0 : (_llLength / _timeDuration);
	  }

	  double AddValue(int64_t len, double time)
	  {
		  if (time > 0)
		  {
			  _llLength += len;
			  _timeDuration += time;
		  }

		  return GetAverage();
	  }

  private:
	  int64_t _llLength;
	  double _timeDuration;
  };

  std::map<std::wstring, int>      m_mapLogo;
  std::vector<PVRDemoChannelGroup> m_groups;
  std::vector<PVRDemoChannel>      m_channels;
  AverageSpeed                     m_StreamSpeed;
  time_t                           m_timeSpeedLastChanged;
  int                              m_iPauseDuration;
  double                           m_LastSpeed;
  double                           m_LastSpeedDownload;
  int64_t                          m_llLastStreamPos;
  int64_t                          m_llStreamCurrentLength;
  CStdString                       m_strDefaultIcon;
  CStdString                       m_strDefaultMovie;
  CStdString                       m_strProxyAddr;
  //PLATFORM::CMutex                 m_mutex;
  LibVLCCAPlugin::ILibVLCModule*   m_ptrVLCCAModule;
  IStream*                         m_currentStream;
  PVRDemoChannel                   m_currentChannel;
  EInputStreamHandler              m_StreamHandler;
  ULONG                            m_ulMCastIf;
  bool                             m_bIsEnableOnLineEpg;
  bool                             m_bCaSupport;
};
