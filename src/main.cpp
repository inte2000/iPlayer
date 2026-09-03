/*
20240425 添加 InitDecoderFactory()
大模型：GPT 5.3 Codex
任务说明：todo_task_27.txt
*/
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include "cmdline.h"
#include "UnicodeConvert.h"
#include "PlayInterface.h"
#include "DecoderFactory.h"
#include "PluginConfig.h"
#include "PluginsMgmt.h"
#include "GlobalConfig.h"
#include "StdFileSystem.h"
#include "ScopeGuard.h"

static void InitDecoderFactory()
{
    CDecoderFactory& factory = CDecoderFactory::GetInstance();
    factory.SetAppVersion(1, 0);

    const std::string pluginCfg = GetPluginConfigFilePathname();
    std::vector<PluginConfig> pluginItems;
    if (LoadPluginConfigFile(pluginCfg, pluginItems))
    {
        for (auto& plugCfg : pluginItems)
        {
            plugCfg.hostfile = MakeupDecoderPlugPathname(plugCfg.hostfile);
            auto plugObj = CreatePluginObjectByConfig(plugCfg);
            if (!plugObj)
            {
                std::cerr << "Skip invalid decoder plugin: " << plugCfg.hostfile << std::endl;
                continue;
            }

            auto [ok, msg] = factory.AddPluginObject(*plugObj);
            if (!ok)
            {
                std::cerr << "Add decoder plugin failed: " << msg << std::endl;
            }
        }
    }

    const std::string decodercfg = GetDecoderConfigFilePathname();
    factory.LoadCustomDecoderConfig(decodercfg);
}

static const char* DecoderTypeName(uint32_t decodeType)
{
    if (decodeType == DECODE_TYPE_NATIVE) {
        return "内置";
    }
    if (decodeType == DECODE_TYPE_PLUGIN) {
        return "插件";
    }
    return "未知";
}

static void PrintDecoderList()
{
    const auto decoderItems = CDecoderFactory::GetInstance().GetDecoders();
    std::cout << "当前解码器列表:" << std::endl;
    for (const auto& item : decoderItems)
    {
        const std::string& name = std::get<0>(item);
        const uint32_t decodeType = std::get<1>(item);
        const std::string& hostfile = std::get<2>(item);

        std::cout << "- 名称: " << name << ", 类型: " << DecoderTypeName(decodeType);
        if ((decodeType == DECODE_TYPE_PLUGIN) && !hostfile.empty()) {
            std::cout << ", 插件位置: " << hostfile;
        }
        std::cout << std::endl;
    }
}

static std::string PathToUtf8(const std::filesystem::path& path)
{
    const std::u8string u8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

static int RebuildPluginConfigByScan()
{
    CDecoderFactory& factory = CDecoderFactory::GetInstance();
    factory.SetAppVersion(1, 0);

    const std::filesystem::path pluginsDir = GetApplicationBasePath() / "plugins";
    if (!std::filesystem::exists(pluginsDir) || !std::filesystem::is_directory(pluginsDir))
    {
        std::cerr << "插件目录不存在: " << PathToUtf8(pluginsDir) << std::endl;
        return -1;
    }

    std::vector<PluginConfig> pluginItems;
    for (const auto& entry : std::filesystem::directory_iterator(pluginsDir))
    {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (ext != ".ipdplus") {
            continue;
        }

        auto plugObj = CreatePluginObjectByFile(entry.path().wstring());
        if (!plugObj)
        {
            std::cerr << "跳过无效插件: " << PathToUtf8(entry.path()) << std::endl;
            continue;
        }

        auto [ok, msg] = factory.AddPluginObject(*plugObj);
        if (!ok)
        {
            std::cerr << "加载插件失败: " << msg << std::endl;
            continue;
        }

        PluginConfig item;
        item.name = plugObj->name;
        item.publisher = plugObj->publisher;
        item.type = plugObj->type;
        item.hostfile = PathToUtf8(entry.path().filename());
        pluginItems.push_back(std::move(item));
    }

    const std::filesystem::path cfgPath = GetPluginConfigFilePathname();
    std::error_code ec;
    std::filesystem::create_directories(cfgPath.parent_path(), ec);
    if (!SavePluginConfigFile(cfgPath.string(), pluginItems))
    {
        std::cerr << "保存插件配置失败: " << PathToUtf8(cfgPath) << std::endl;
        return -1;
    }

    std::cout << "插件扫描完成，已保存 " << pluginItems.size() << " 个插件到: " << PathToUtf8(cfgPath) << std::endl;
    return 0;
}

static int ConfigureDecoderPlugin(const std::string& decoderName)
{
    CDecoderFactory& factory = CDecoderFactory::GetInstance();
    auto decoderOpt = factory.GetDecoderPlugin(decoderName);
    if (!decoderOpt.has_value())
    {
        std::cerr << "未找到插件解码器: " << decoderName << std::endl;
        return -1;
    }

    if (!decoderOpt->config)
    {
        std::cerr << "插件不支持配置: " << decoderName << std::endl;
        return -1;
    }

    std::cout << "配置插件解码器: " << decoderName << std::endl;
    decoderOpt->config(nullptr);
    return 0;
}

static std::vector<std::string> NormalizeCommandArgs(int argc, char* argv[])
{
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i)
    {
        std::string arg = (argv[i] == nullptr) ? std::string() : std::string(argv[i]);
        if (arg == "-ld") {
            arg = "--ld";
        }
        else if (arg == "-cd") {
            arg = "--cd";
        }
        else if (arg == "-rd") {
            arg = "--rd";
        }
        else if (arg == "-out") {
            arg = "--out";
        }
        else if (arg == "-ffmt") {
            arg = "--ffmt";
        }
        else if (arg == "-srate") {
            arg = "--srate";
        }
        else if (arg == "-cfmt") {
            arg = "--cfmt";
        }
        else if (arg == "-channel") {
            arg = "--channel";
        }
        else if (arg == "-ml") {
            arg = "--ml";
        }
        else if (arg == "-folder") {
            arg = "--folder";
        }
        else if (arg == "-recursion") {
            arg = "--recursion";
        }
        args.push_back(std::move(arg));
    }
    return args;
}

static bool BuildCdromDevicePath(const std::string& value, std::string& devicePath)
{
    if (value.empty()) {
        return false;
    }

    char driveLetter = '\0';
    if ((value.size() == 1) && std::isalpha(static_cast<unsigned char>(value[0])))
    {
        driveLetter = value[0];
    }
    else if ((value.size() == 2) && std::isalpha(static_cast<unsigned char>(value[0])) && (value[1] == ':'))
    {
        driveLetter = value[0];
    }
    else
    {
        return false;
    }

    driveLetter = static_cast<char>(std::toupper(static_cast<unsigned char>(driveLetter)));
    devicePath = "CDDevice--";
    devicePath.push_back(driveLetter);
    devicePath.push_back(':');
    return true;
}

bool MakeParser(cmdline::parser& a)
{
    a.add("help", '?', "print this message");
    a.add("play", 'p', "play media file or playlist");
    a.add("ml", '\0', "scan folder and generate playlist file");
    a.add("convert", 'c', "convert media file format");
    a.add("ld", '\0', "list all decoders");
    a.add("rd", '\0', "rebuild plugin.config by scanning plugins folder");
    a.add<std::string>("cd", '\0', "config decoder plugin by name", false, "");
    a.add("tui", '\0', "launch TUI interface");
    a.add<std::string>("devicetype", 't', "device type", false, "Native", cmdline::oneof<std::string>("Native", "Plusin"));
    a.add<std::string>("devicename", 'n', "device name", false, "Wasapi (Share mode)", cmdline::oneof<std::string>("Wasapi (Share mode)", "Wasapi (Exclusive mode)", "DirectSound"));
    a.add<std::string>("deviceid", 'i', "device id", false, "");
    a.add<std::string>("speakerlayout", 'o', "speaker layout config file", false, "");
    a.add<std::string>("filename", 'f', "media file name", false, "");
    a.add<std::string>("cdimage", '\0', "audio cd image file name", false, "");
    a.add<std::string>("cdrom", '\0', "audio cd-rom drive letter, e.g. F or F:", false, "");
    a.add<std::string>("folder", '\0', "folder path for playlist generation", false, "");
    a.add("recursion", '\0', "scan sub folders recursively");
    a.add<std::string>("playlist", 'l', "playlist file name", false, "");
    a.add<std::string>("out", '\0', "output media file name", false, "");
    a.add<std::string>("ffmt", '\0', "output media format", false, "wav");
    a.add<uint32_t>("srate", '\0', "output sample rate", false, 44100);
    a.add<std::string>("cfmt", '\0', "output sample data format", false, "S16");
    a.add<uint32_t>("channel", '\0', "output channels", false, 2);

    return true;
}


int main(int argc, char *argv[])
{
    cmdline::parser parser;
    if (!MakeParser(parser))
    {
        std::cout << "Fail to create command line parser!" << std::endl;
        return -1;
    }

    if (argc <= 1)
    {
        if ((argv != nullptr) && (argv[0] != nullptr)) {
            parser.set_program_name(argv[0]);
        }
        std::cout << parser.usage();
        return 0;
    }
    
    std::string deviceType, devideName, deviceId;
    try
    {
        std::vector<std::string> normArgs = NormalizeCommandArgs(argc, argv);
        std::vector<char*> argPtrs;
        argPtrs.reserve(normArgs.size());
        for (std::string& arg : normArgs)
        {
            argPtrs.push_back(arg.data());
        }
        parser.parse_check(static_cast<int>(argPtrs.size()), argPtrs.data());

        if (parser.exist("rd"))
        {
            return RebuildPluginConfigByScan();
        }
        
        InitDecoderFactory();

        if (parser.exist("ld"))
        {
            PrintDecoderList();
            return 0;
        }

        if (parser.exist("cd"))
        {
            const std::string decoderName = parser.get<std::string>("cd");
            return ConfigureDecoderPlugin(decoderName);
        }

        if (parser.exist("convert"))
        {
            const std::string srcFilename = parser.get<std::string>("filename");
            const std::string outFilename = parser.get<std::string>("out");
            const std::string outFormat = parser.get<std::string>("ffmt");
            const uint32_t outSampleRate = parser.get<uint32_t>("srate");
            const std::string outDataFormat = parser.get<std::string>("cfmt");
            const uint32_t outChannels = parser.get<uint32_t>("channel");

            return ConvertMediaFileInterface(srcFilename, outFilename, outFormat, outSampleRate, outDataFormat, outChannels);
        }

        if (parser.exist("ml"))
        {
            const std::string folder = parser.get<std::string>("folder");
            if (folder.empty())
            {
                std::cerr << "missing folder path (--folder)" << std::endl;
                return -1;
            }

            const bool recursion = parser.exist("recursion");
            std::string playlistFile;
            if (parser.exist("playlist"))
            {
                playlistFile = parser.get<std::string>("playlist");
            }

            return MakePlayListFileInterface(folder, recursion, playlistFile);
        }
    
        if (parser.exist("play"))
        {
            deviceType = parser.get<std::string>("devicetype");
            devideName = parser.get<std::string>("devicename");
            deviceId = parser.get<std::string>("deviceid");   
            SetupDevice(deviceType, devideName, deviceId);

            bool bPlaylist = false;
            bool bCdSource = false;
            std::string filename;
            if (parser.exist("playlist"))
            {
                filename = parser.get<std::string>("playlist");
                bPlaylist = true;
                bCdSource = false;
            }
            else if (parser.exist("cdimage"))
            {
                filename = parser.get<std::string>("cdimage");
                bPlaylist = false;
                bCdSource = true;
                if (filename.empty())
                {
                    std::cerr << "missing cd image file path (--cdimage)" << std::endl;
                    return -1;
                }
            }
            else if (parser.exist("cdrom"))
            {
                const std::string driveName = parser.get<std::string>("cdrom");
                if (!BuildCdromDevicePath(driveName, filename))
                {
                    std::cerr << "invalid cd-rom drive name (--cdrom), use 'F' or 'F:'" << std::endl;
                    return -1;
                }
                bPlaylist = false;
                bCdSource = true;
            }
            else
            {
                filename = parser.get<std::string>("filename");
                bPlaylist = false;
                bCdSource = false;
                if (filename.empty())
                {
                    std::cerr << "missing media source, use --filename/--playlist/--cdimage/--cdrom" << std::endl;
                    return -1;
                }
            }
            std::string speakerCfg = parser.get<std::string>("speakerlayout");
            //std::cout << "audio source name: " << audioSource->GetName() << std::endl;
            if(parser.exist("tui"))
            {
                StartPlayingTuiInterface(filename, bPlaylist, bCdSource, speakerCfg);
            }
            else
            {
                std::cout << "Play media file: " << filename << std::endl;
                StartPlayingInterface(filename, bPlaylist, bCdSource, speakerCfg);
            }
        }        
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    return 0;
}
