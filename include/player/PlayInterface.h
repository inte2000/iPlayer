#ifndef _PLAY_INTERFACE_H
#define _PLAY_INTERFACE_H


#include <string>
#include <cstdint>

void SetupDevice(const std::string& type, const std::string& deviceName, const std::string& deviceId);
int MakePlayListFileInterface(const std::string& folder, bool recursion, const std::string& playlistFile);
void StartPlayingInterface(const std::string& filename, bool bPlaylist, bool bCdSource, const std::string& speakerLayout);
void StartPlayingTuiInterface(const std::string& filename, bool bPlaylist, bool bCdSource, const std::string& speakerLayout);
int ConvertMediaFileInterface(const std::string& srcFilename,
    const std::string& outFilename,
    const std::string& formatName,
    uint32_t sampleRate,
    const std::string& cfmt,
    uint32_t channels);

#endif //_PLAY_INTERFACE_H
