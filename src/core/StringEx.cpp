/*
20250802 AI 生成（Web 问答，手工粘贴代码）
大模型：ChatGPT 4/Deepseek V2
*/
#include <filesystem>
#include <format>
#include <ranges>
#include "StringEx.h"



void SplitString(const std::string& strMultiLine, std::vector<std::string>& multiString, char chMark)
{
	if (strMultiLine.length() == 0)
		return;

	std::size_t fend, fbegin = 0;
	while ((fend = strMultiLine.find(chMark, fbegin)) != std::string::npos)
	{
		auto count = fend - fbegin;
		multiString.push_back(strMultiLine.substr(fbegin, count));
		fbegin = fend + 1;
	}

	if (fbegin < strMultiLine.length())
	{
		multiString.push_back(strMultiLine.substr(fbegin, -1));
	}
}

void TrimLeft(std::string& s, std::string_view chars) 
{
	if (s.empty()) 
		return;
	auto it = std::find_if(s.begin(), s.end(),
		[&](unsigned char ch) {
			return chars.find(ch) == std::string_view::npos;
		});
	s.erase(s.begin(), it);
}

// 去掉右侧指定字符
void TrimRight(std::string& s, std::string_view chars) 
{
	if (s.empty()) 
		return;
	auto it = std::find_if(s.rbegin(), s.rend(),
		[&](unsigned char ch) {
			return chars.find(ch) == std::string_view::npos;
		});

	s.erase(it.base(), s.end());
}

void SubstituteString(std::string& text, const std::string& srcPart, const std::string& dstPart)
{
	std::string::size_type pos = 0;
	while ((pos = text.find(srcPart, pos)) != std::string::npos)
	{
		text.replace(pos, srcPart.size(), dstPart);
		pos += dstPart.size(); // 跳过刚替换的部分
	}
}

//std::string str = "a,b.c!d,e";
//RemoveAllChars(str, ",.!"); //剩下 abcde
//erase-remove 惯用法,
void RemoveAllChars(std::string& text, const std::string& chars)
{
	text.erase(std::remove_if(text.begin(), text.end(),
		[&](unsigned char c) {
			return chars.find(c) != std::string::npos;
		}), text.end());
}

void RemoveAllChars(std::wstring& text, const std::wstring& chars)
{
	text.erase(std::remove_if(text.begin(), text.end(),
		[&](wchar_t c) {
			return chars.find(c) != std::string::npos;
		}), text.end());
}

void RemoveAllSubstrings(std::string& text, const std::string& target)
{
	if (target.empty()) 
		return;

	std::string::size_type pos = 0;
	while ((pos = text.find(target, pos)) != std::string::npos) 
	{
		text.erase(pos, target.size());
	}
}

void RemoveAllSubstrings(std::wstring& text, const std::wstring& target)
{
	if (target.empty())
		return;

	std::wstring::size_type pos = 0;
	while ((pos = text.find(target, pos)) != std::wstring::npos)
	{
		text.erase(pos, target.size());
	}
}


std::string GetFileSizeString(long long fileSize)
{
	double bytes = (double)fileSize;
	int cIter = 0;
	const char* pszUnits[] = { "B", "KB", "MB", "GB", "TB" };
	int cUnits = sizeof(pszUnits) / sizeof(pszUnits[0]);

	while ((bytes >= 1024) && (cIter < (cUnits - 1)))
	{
		bytes /= 1024.0;
		cIter++;
	}
	std::string strLength = std::format("{:.2f} {}", bytes, pszUnits[cIter]);
	//_snprintf(strBuf, cchs, _T("%.2f %s"), );

	return strLength;
}

std::string GetFileFolderName(const std::string& pathname)
{
	auto pos = pathname.rfind('\\', pathname.length());
	if(pos == std::string::npos)
		pos = pathname.rfind('/', pathname.length());

	if (pos != std::string::npos)
		return pathname.substr(0, pos);
	else
		return pathname;
}

std::wstring GetFileFolderName(const std::wstring& pathname)
{
	auto pos = pathname.rfind(L'\\', pathname.length());
	if(pos == std::string::npos)
		pos = pathname.rfind(L'/', pathname.length());

	if (pos != std::string::npos)
		return pathname.substr(0, pos);
	else
		return pathname;
}

std::string GetFileNamePart(const std::string& filename)
{
	auto pos = filename.rfind('\\', filename.length());
	if (pos == std::string::npos)
		pos = filename.rfind('/', filename.length());

	if (pos != std::string::npos)
		return filename.substr(pos + 1);
	else
		return filename;
}

std::wstring GetFileNamePart(const std::wstring& filename)
{
	auto pos = filename.rfind(L'\\', filename.length());
	if (pos == std::string::npos)
		pos = filename.rfind(L'/', filename.length());

	if (pos != std::string::npos)
		return filename.substr(pos + 1);
	else
		return filename;
}

std::wstring GetFileStemNamePart(const std::wstring& filename)
{
	std::wstring namepart;
	auto pos = filename.rfind(L'\\', filename.length());
	if (pos == std::string::npos)
		pos = filename.rfind(L'/', filename.length());

	if (pos != std::string::npos)
	{
		auto posDot = filename.rfind('.', filename.length());
		if (posDot == std::string::npos)
			return namepart.substr(pos + 1);

		return filename.substr(pos + 1, posDot - pos - 1);
	}
	else
	{
		auto posDot = filename.rfind('.', filename.length());
		if (posDot == std::string::npos)
			return filename;

		return filename.substr(0, posDot);
	}
}

std::string GetFileExtNamePart(const std::string& filename)
{
	auto pos = filename.rfind('.', filename.length());
	if (pos == std::string::npos)
		return "";

	std::string extName = filename.substr(pos);
	std::transform(extName.begin(), extName.end(), extName.begin(), [](unsigned char c) { return std::tolower(c); });

	return extName;
}

std::wstring GetFileExtNamePart(const std::wstring& filename)
{
	auto pos = filename.rfind(L'.', filename.length());
	if (pos == std::string::npos)
		return L"";

	std::wstring extName = filename.substr(pos);
	std::transform(extName.begin(), extName.end(), extName.begin(), [](wchar_t c) { return std::tolower(c); });

	return extName;
}

std::string GetImageMimeTypeByExtName(const std::string& extname)
{
	if (extname.empty())
		return "image/jpeg";

	std::string extName = extname;
	std::transform(extName.begin(), extName.end(), extName.begin(), [](unsigned char c) { return std::tolower(c); });

	if ((extName.compare(".jpeg") == 0) || (extName.compare(".jpg") == 0))
		return "image/jpeg";
	else if(extName.compare(".png") == 0)
		return "image/png";
	else if(extName.compare(".gif") == 0)
		return "image/gif";
	else if(extName.compare(".webp") == 0)
		return "image/webp";
	else
		return "image/jpeg";
}

//防止出现类似 image/png;charset=utf-8 这样的情况
std::string GetExtNameByMimeType(const std::string& mime)
{
	auto findslash = mime.find("/", 0);
	if (findslash != std::string::npos)
	{
		std::string ext = ".";
		ext += mime.substr(findslash + 1);
		auto pos = ext.rfind(';');
		if (pos != std::string::npos)
			ext = ext.substr(0, pos);

		return ext;
	}

	return ".dat";
}
std::string get_ext_from_mime(const std::string& mime)
{
	auto findslash = mime.find("/", 0);
	if (findslash != std::string::npos)
	{
		std::string ext = ".";
		ext += mime.substr(findslash + 1);
		auto pos = ext.rfind(';');
		if (pos != std::string::npos)
			ext = ext.substr(0, pos);

		return ext;
	}
	return ".dat";
}

//text/html：HTML 文件。﻿
//text/css：CSS 样式表文件。﻿
//text/csv：逗号分隔值(CSV) 文件。﻿

std::string ReplaceExtName(const std::string& filename, const std::string& extName)
{
	auto pos = filename.rfind('.', filename.length());
	if (pos == std::string::npos)
		return filename + extName;

	std::string substring = filename.substr(0, pos);
	substring += extName;

	return substring;
}

std::wstring ReplaceExtName(const std::wstring& filename, const std::wstring& extName)
{
	auto pos = filename.rfind(L'.', filename.length());
	if (pos == std::wstring::npos)
		return filename + extName;

	std::wstring substring = filename.substr(0, pos);
	substring += extName;

	return substring;
}

bool IsAsciiLetter(wchar_t c)
{
    return ((c >= L'a') && (c <= L'z')) || ((c >= L'A') && (c <= L'Z'));
}

std::wstring ToLowerAscii(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        if ((c >= L'A') && (c <= L'Z')) {
            return static_cast<wchar_t>(c - L'A' + L'a');
        }
        return c;
    });
    return value;
}
