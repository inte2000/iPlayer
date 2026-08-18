#ifndef _AUDIOCD_H_
#define _AUDIOCD_H_


#include <windows.h>
#include <tchar.h>
#include <vector>
#include <string>

extern "C" {
#include <cdio/cdio.h>
#include <cdio/cd_types.h>
}

#define CD_BLOCKS_PER_SECOND	75
#define RAW_SECTOR_SIZE			2352

typedef struct tagCD_TRACK_INFO
{
	int32_t lsn;
	int32_t length;
	BOOL isAudio;
	ULONG ctrl;
	std::wstring title;
	std::wstring artist;
}CD_TRACK_INFO;

class CAudioCD
{
public:
	CAudioCD(const std::wstring& deviceName = L"");
	~CAudioCD();

	BOOL Open(const std::wstring& deviceName);
	BOOL IsOpened();
	void Close();
	uint32_t GetTrackCount();
	const std::wstring& GetTitle() const { return m_title; }
	const std::wstring& GetArtist() const { return m_artist; }
	float GetTrackTime(uint32_t Track);
	std::size_t GetTrackSize(uint32_t Track);
	int32_t GetLeadoutTrackLba() const { return m_LeadoutTrackLba; }
	const CD_TRACK_INFO& GetTrackInfo(uint32_t Track) const {
		if (Track < static_cast<uint32_t>(m_aTracks.size()))
			return m_aTracks[Track];

		return m_NullTrack;
	}
	CD_TRACK_INFO& GetTrackInfo(uint32_t Track) {
		if (Track < static_cast<uint32_t>(m_aTracks.size()))
			return m_aTracks[Track];

		return m_NullTrack;
	}
	std::wstring GetTrackTitle(uint32_t Track);
	std::wstring GetTrackPerformer(uint32_t Track);
	std::wstring GetTrackArtist(uint32_t Track);
	std::wstring GetTrackAlbum(uint32_t Track);

	ULONG ReadTrack(uint32_t Track, int32_t startSector, uint32_t sectorCount, uint8_t* pBuf, uint32_t bufSize);

protected:
	BOOL QueryCDTracksInfo(CdIo_t* cdio, std::vector<CD_TRACK_INFO>& aTracks);
	BOOL QueryCDTextInfo(CdIo_t* cdio, std::vector<CD_TRACK_INFO>& aTracks);
	CdIo_t* m_cdio;

	std::vector<CD_TRACK_INFO>	m_aTracks;
	int32_t m_LeadoutTrackLba;
	BOOL m_bReadTextInfo;
	std::wstring m_title; //query from text
	std::wstring m_artist; //query from text

	static CD_TRACK_INFO m_NullTrack;
};


#endif //_AUDIOCD_H_

