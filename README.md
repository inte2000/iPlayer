# imPlayer
imPlayer music player

## Description

这是一个使用 AI 生成代码的方式创作一个音乐播放器程序的测试项目

## Build

使用 cmake 正常编译，如果使用 vscode，可以使用 "cmake-config-debug (MSVC C++20)" 配置项目，用 "cmake-build-debug (MSVC C++20)" 任务编译，这些任务都定义在 .vscode/tasks.json 文件中。

关于第三方库，有两种方式，一种是用 vcpkg 配置，然后让你的 Code Agent 修改 cmake 文件，引入这些库。另一种是将编译好的库放在 thirdparty 目录下，然后让 Code Agent 生成类似 FindLibXXX.cmake，然后在项目 cmake 文件中引入这个 配置。目前的例子代码中两种方式都有，你可以根据自己系统配置和安装的情况自行决定如何引入第三方库。

编译完成所有解码器插件后，执行以下命令自动生成插件配置：
```
implayer.exe -rd
```

可通过以下命令查看当前配置的解码器：
```
implayer.exe -ld
```

可通过以下命令设置解码器的私有参数：
```
implayer.exe -cd decoder_name
```

播放音频文件使用 -p 参数，直接播放文件配合 filename 参数：
```
imPlayer.exe -p --tui --filename=...
```

播放音乐列表使用 -p 参数配合 playlist 参数（-ml 参数创建播放列表），比如：
```
imPlayer.exe -p --tui --playlist=...
```

播放音乐CD，可以直接指定设备，也支持直接打开 CD 映像文件播放：
```
imPlayer.exe -p --tui --cdrom=F
imPlayer.exe -p --tui --cdimage=...
```

可使用 -ml 参数创建播放列表文件：
```
imPlayer.exe -ml --folder=... [--recursion] [--playlist=...]
```

folder 参数指定一个音乐文件目录，--recursion 参数表示递归搜索子目录，默认不搜索子目录。playlist 参数指定保存播放列表文件的绝对位置，不指定这个参数的话播放列表会被保存到当前程序的 playlists 目录中。

当前编码器已经设计完成，可使用 convert 参数做音频文件格式转换：
```
imPlayer.exe --convert --filename=e:/test_music/test.mp3 --out=e:/test_music/test.wav --ffmt=wav --cfmt=S16 --srate=48000 --channel=2
```


## 第三方库

Librarys
-------

cmdline
libSndFile
mpeg123
ffmpeg
CLI11
zlib
libvgm
ftxui
libsox
libcdio
Catch2 
libiconv
ICU
nlohmann_json
libogg
libFLAC
libgme
wavpack
dr_wav



Credits
-------

Ogg Vorbis is copyright (c) 1994-2023 Xiph.Org
http://xiph.org/vorbis/

Opus is copyright (c) 2011-2024 Xiph.Org
https://opus-codec.org/

FLAC is copyright (c) 2000-2009 Josh Coalson, 2011-2026 Xiph.Org
http://xiph.org/flac/

WavPack is copyright (c) 1998-2026 David Bryant
http://www.wavpack.com

MusicBrainz is copyright (c) The MetaBrainz Foundation
https://musicbrainz.org/

Audioscrobbler is copyright (c) 2026 Last.fm Ltd.
http://www.last.fm

JSON for Modern C++ is copyright (c) 2013-2026 Niels Lohmann
https://github.com/nlohmann/json

This software uses libraries from the FFmpeg project under the LGPLv2.1
https://ffmpeg.org/

This software uses the libopenmpt library under the BSD-3-Clause License
Copyright (c) 2004-2026, OpenMPT Project Developers and Contributors
Copyright (c) 1997-2003, Olivier Lapicque
https://lib.openmpt.org/libopenmpt/