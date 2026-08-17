/*
20250802 AI 生成（Web 问答，手工粘贴代码）
大模型：ChatGPT 4/Deepseek V2
*/
#ifndef STRING_EX_H
#define STRING_EX_H

#include <string>
#include <vector>

void SplitString(const std::string& strMultiLine, std::vector<std::string>& multiString, char chMark);


void TrimLeft(std::string& s, std::string_view chars = " \t\n\r\f\v");
void TrimRight(std::string& s, std::string_view chars = " \t\n\r\f\v");
inline void Trim(std::string& s, std::string_view chars = " \t\n\r\f\v") 
{
	TrimLeft(s, chars);
	TrimRight(s, chars);
}

void SubstituteString(std::string& text, const std::string& srcPart, const std::string& dstPart);
void RemoveAllChars(std::string& text, const std::string& chars);
void RemoveAllChars(std::wstring& text, const std::wstring& chars);
void RemoveAllSubstrings(std::string& text, const std::string& target);
void RemoveAllSubstrings(std::wstring& text, const std::wstring& target);

std::string GetFileSizeString(long long fileSize);
std::string GetFileFolderName(const std::string& filename);
std::wstring GetFileFolderName(const std::wstring& filename);
std::string GetFileNamePart(const std::string& filename);
std::wstring GetFileNamePart(const std::wstring& filename);
std::wstring GetFileStemNamePart(const std::wstring& filename);
std::string GetFileExtNamePart(const std::string& pathname);
std::wstring GetFileExtNamePart(const std::wstring& pathname);
std::string GetImageMimeTypeByExtName(const std::string& extname);
std::string GetExtNameByMimeType(const std::string& mime);
std::string ReplaceExtName(const std::string &filename, const std::string& extName);
std::wstring ReplaceExtName(const std::wstring &filename, const std::wstring& extName);

bool IsAsciiLetter(wchar_t c);
std::wstring ToLowerAscii(std::wstring value);


#endif //STRING_EX_H
