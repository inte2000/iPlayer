#include <format>
#include <map>
#include <windows.h>
#include <winioctl.h>
#include <ntddcdrm.h>   // for CDROM_READ_AUDIO
#include "StringEx.h"
#include "UnicodeConvert.h"
#include "AudioCDCommon.h"
#include "AudioCD.h"

using namespace std::literals;

#define SECTORS_AT_READ			20
#define SECTORS_PER_BLOCK   20  // 太大容易造成读错误


CD_TRACK_INFO CAudioCD::m_NullTrack;

CAudioCD::CAudioCD(const std::wstring& deviceName) : m_bReadTextInfo(TRUE), m_LeadoutTrackLba(0)
{
	m_cdio = nullptr;
	if (!deviceName.empty())
		Open(deviceName);
}

CAudioCD::~CAudioCD()
{
	Close();
}

BOOL CAudioCD::Open(const std::wstring& deviceName)
{
	Close();

	std::string utf8_name = Utf16ToUtf8(deviceName);
	// Open drive-handle
	auto pos = utf8_name.find("CDDevice--", 0);
	if ((pos != std::string::npos) && (pos == 0)) //CDDevice--X:
	{
		char device[3] = { utf8_name[10], ':', '\0' };
		m_cdio = cdio_open(device, DRIVER_UNKNOWN);
		// Lock drive
		if (!lock_win32_device(m_cdio))
		{
			unlock_win32_device(m_cdio);
			cdio_destroy(m_cdio);
			m_cdio = nullptr;
			return FALSE;
		}
	}
	else
	{
		m_cdio = cdio_open(utf8_name.c_str(), DRIVER_UNKNOWN); //光盘映像文件
	}
	if (m_cdio == nullptr)
		return FALSE;


	return QueryCDTracksInfo(m_cdio, m_aTracks);
}

BOOL CAudioCD::IsOpened()
{
	return m_cdio != nullptr;
}

void CAudioCD::Close()
{
	unlock_win32_device(m_cdio);
	m_aTracks.clear();
	if(m_cdio)
	    cdio_destroy(m_cdio);

	m_cdio = nullptr;
}

uint32_t CAudioCD::GetTrackCount()
{
	if (m_cdio == nullptr)
		return 0;
	return static_cast<uint32_t>(m_aTracks.size());
}

float CAudioCD::GetTrackTime(uint32_t Track)
{
	if (m_cdio == nullptr)
		return 0;
	if ( Track >= m_aTracks.size() )
		return 0;

	CD_TRACK_INFO& Tr = m_aTracks.at(Track);
	return float(Tr.length) / 75.0f;
}

std::size_t CAudioCD::GetTrackSize(uint32_t Track)
{
	if (m_cdio == nullptr)
		return 0;
	if ( Track >= m_aTracks.size() )
		return 0;

	CD_TRACK_INFO& Tr = m_aTracks.at(Track);
	return Tr.length * RAW_SECTOR_SIZE;
}

std::wstring CAudioCD::GetTrackTitle(uint32_t Track)
{
	std::wstring title;

	if (Track < m_aTracks.size())
	{
		CD_TRACK_INFO& Tr = m_aTracks.at(Track);
		if (!Tr.title.empty())
			title = Tr.title;
		else
			title = std::format(L"CD Audio Track {}", Track + 1);
	}

	return title;
}

std::wstring CAudioCD::GetTrackPerformer(uint32_t Track)
{
	return GetTrackArtist(Track);
}

std::wstring CAudioCD::GetTrackArtist(uint32_t Track)
{
	std::wstring artist;

	if (Track < m_aTracks.size())
	{
		CD_TRACK_INFO& Tr = m_aTracks.at(Track);
		if (!Tr.artist.empty())
			artist = Tr.artist;
		else
			artist = m_artist; //使用整张 CD 的信息
	}

	return artist;
}

std::wstring CAudioCD::GetTrackAlbum(uint32_t Track)
{
	return m_title; //对于 CD 来说，就使用整张 CD 的信息
}

ULONG CAudioCD::ReadTrack(uint32_t Track, int32_t startSector, uint32_t sectorCount, uint8_t* pBuf, uint32_t bufSize)
{
	if (m_cdio == nullptr)
		return 0;
	if (Track >= m_aTracks.size())
		return 0;

	CD_TRACK_INFO& ti = m_aTracks.at(Track);
	if (startSector > ti.length)
		return 0;

	if (bufSize < (sectorCount * RAW_SECTOR_SIZE))
		return 0;

	uint32_t readBlockBytes = SECTORS_PER_BLOCK * RAW_SECTOR_SIZE;
	uint32_t limitSectors = ti.length - startSector;
	uint32_t readSectors = (sectorCount < limitSectors) ? sectorCount : limitSectors;
	ULONG i = 0;
	for (i = 0; i < readSectors / SECTORS_PER_BLOCK; i++)
	{
		lsn_t start_lsn = ti.lsn + startSector + i * SECTORS_PER_BLOCK;
		//lsn_t start_lsn = cdio_get_track_lba(m_cdio, Track + 1) + startSector + i * SECTORS_PER_BLOCK;
		driver_return_code_t rtn = cdio_read_audio_sectors(m_cdio, 
			pBuf + i * readBlockBytes, start_lsn, SECTORS_PER_BLOCK);
		if (rtn != DRIVER_OP_SUCCESS)
		{
			return i * readBlockBytes; //出错，就返回已经读取到 buffer 中的字节数
		}
	}
	uint32_t SectorCount = readSectors % SECTORS_PER_BLOCK;
	if (SectorCount > 0) //
	{
		lsn_t start_lsn = ti.lsn + startSector + i * SECTORS_PER_BLOCK;
		//lsn_t start_lsn = cdio_get_track_lba(m_cdio, Track + 1) + startSector + i * SECTORS_PER_BLOCK;
		driver_return_code_t rtn = cdio_read_audio_sectors(m_cdio,
			pBuf + i * readBlockBytes, start_lsn, SectorCount);
		if (rtn != DRIVER_OP_SUCCESS)
		{
			//UINT err = ::GetLastError();
			return i * readBlockBytes; //出错，就返回已经读取到 buffer 中的字节数
		}
	}

	return readSectors * RAW_SECTOR_SIZE;
}

/*
LBA 模式，音轨起始地址在 .Address[1..3] 中组成一个整数：
int lba = (addr[1] << 16) | (addr[2] << 8) | addr[3];

MSF 模式，钟、秒、帧（每秒 75 帧）：
    int minutes = Address[1]； //分钟（0–99）
    int seconds = Address[2]；// 秒（0–59）
    int frames = Address[3]；// 帧（0–74）
double timeSec = minutes * 60 + seconds + frames / 75.0;
*/
BOOL CAudioCD::QueryCDTracksInfo(CdIo_t* cdio, std::vector<CD_TRACK_INFO>& aTracks)
{
	track_t first_track = cdio_get_first_track_num(cdio);
	track_t last_track = cdio_get_last_track_num(cdio);

	for (track_t i = first_track; i <= last_track; i++)
	{
		CD_TRACK_INFO NewTrack;
		int32_t lba = cdio_get_track_lba(cdio, i);
		int32_t pregap = cdio_get_track_pregap_lsn(cdio, i);
		if (pregap != CDIO_INVALID_LSN)
		{
		}
		NewTrack.lsn = cdio_get_track_lsn(cdio, i);
		NewTrack.ctrl = 0;
		NewTrack.isAudio = (cdio_get_track_format(cdio, i) == TRACK_FORMAT_AUDIO);
		NewTrack.length = cdio_get_track_sec_count(cdio, i);

		m_aTracks.push_back(std::move(NewTrack));
	}

	if (m_aTracks.size() == 0)
		return FALSE;

	m_LeadoutTrackLba = cdio_get_track_lba(cdio, CDIO_CDROM_LEADOUT_TRACK);
	if (m_LeadoutTrackLba == CDIO_INVALID_LBA)
	{
		CD_TRACK_INFO& lastTrack = m_aTracks[m_aTracks.size() - 1];
		m_LeadoutTrackLba = lastTrack.lsn + 150 + lastTrack.length;
	}

	if(m_bReadTextInfo)
	    QueryCDTextInfo(cdio, m_aTracks); //CD 不一定都含有 text 信息

	return TRUE;
}

BOOL CAudioCD::QueryCDTextInfo(CdIo_t* cdio, std::vector<CD_TRACK_INFO>& aTracks)
{
	//不需要显式调用 cdtext_destroy(cdtext)，cdio_destroy 的时候会自动删除这块内存
	cdtext_t* cdtext = cdio_get_cdtext(cdio);
	if (cdtext)
	{
		// 获取语言数量
		cdtext_lang_t lang = cdtext_get_language(cdtext);
		// 光盘级信息
		const char* title = cdtext_get_const(cdtext, CDTEXT_FIELD_TITLE, 0);
		if(title)
		    m_title = LocalMBCSToUtf16Le(title);
		const char* artist = cdtext_get_const(cdtext, CDTEXT_FIELD_PERFORMER, 0);
		if(artist)
		    m_artist = LocalMBCSToUtf16Le(artist);
		for (std::size_t i = 0; i < aTracks.size(); i++)
		{
			const char* track_title = cdtext_get_const(cdtext, CDTEXT_FIELD_TITLE, track_t(i + 1));
			if (track_title)
				aTracks[i].title = LocalMBCSToUtf16Le(track_title);

			const char* track_artist = cdtext_get_const(cdtext, CDTEXT_FIELD_PERFORMER, track_t(i + 1));
			if(track_artist)
				aTracks[i].artist = LocalMBCSToUtf16Le(track_artist);
		}

		return TRUE;
	}

	return FALSE;
}

