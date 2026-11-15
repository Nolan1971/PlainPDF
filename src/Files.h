#pragma once

#include "Pdf.h"

struct FileDropTarget;
using FileDropped = void (*)(void*, std::wstring&&) noexcept;

FileDropTarget* RegisterFileDrop(HWND hwnd, FileDropped handler, void* context);
void RevokeFileDrop(FileDropTarget*& target);
std::wstring ChoosePdf(HWND owner);
std::wstring DroppedPdf(WPARAM wParam);
std::wstring CommandLinePdf();
std::wstring FileName(const std::wstring& path);
std::unique_ptr<Password> AskPassword(HWND owner, HINSTANCE hInstance, bool retry);
void LoadRecent(std::vector<std::wstring>& files);
void SaveRecent(const std::vector<std::wstring>& files);
void RememberRecent(std::vector<std::wstring>& files, const std::wstring& path);
void ForgetRecent(std::vector<std::wstring>& files, const std::wstring& path);
void FillRecentMenu(HMENU mainMenu, const std::vector<std::wstring>& files);
const wchar_t* OpenErrorMessage(PdfError error);
