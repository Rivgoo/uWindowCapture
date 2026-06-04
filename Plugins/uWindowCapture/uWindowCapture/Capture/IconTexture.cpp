#include <fstream>
#include <regex>
#include <string>
#include <shlwapi.h>
#include <gdiplus.h>
#include "IconTexture.h"
#include "Window.h"
#include "CaptureManager.h"
#include "IsolatedCaptureDevice.h"
#include "../Interop/SharedTextureResource.h"
#include "../Graphics/GraphicsManager.h"
#include "../Core/Debug.h"
#include "../Core/Util.h"
#include "../Core/Message.h"

#pragma comment(lib, "shlwapi")
#pragma comment(lib, "gdiplus")

using namespace Microsoft::WRL;

IconTexture::IconTexture(Window* window) : window_(window) { InitIcon(); }
IconTexture::~IconTexture() { if (!appLogoPath_.empty()) ::DestroyIcon(hIcon_); }

void IconTexture::InitIcon()
{
    try { if (window_->IsUWP()) InitIconHandleForStoreApp(); else InitIconHandleForWin32App(); }
    catch (const std::exception& e) { Debug::Error(__FUNCTION__, " => Exception ", e.what()); }

    if (!hIcon_) hIcon_ = ::LoadIcon(0, IDI_APPLICATION);
    if (!hIcon_) return;

    ICONINFO info;
    if (!::GetIconInfo(hIcon_, &info)) return;
    ScopedReleaser iconReleaser([&] { ::DeleteObject(info.hbmColor); ::DeleteObject(info.hbmMask); });

    BITMAP bmpColor { 0 };
    ::GetObject(info.hbmColor, sizeof(bmpColor), &bmpColor);
    width_ = bmpColor.bmWidth;
    height_ = bmpColor.bmHeight;
}

void IconTexture::InitIconHandleForWin32App()
{
    const auto hWnd = window_->GetWindowHandle();
    hIcon_ = reinterpret_cast<HICON>(::GetClassLongPtr(hWnd, GCLP_HICON));
    if (hIcon_) return;
    ::SendMessageTimeoutW(hWnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, reinterpret_cast<PDWORD_PTR>(&hIcon_));
}

void IconTexture::InitIconHandleForStoreApp()
{
    DWORD processId = window_->IsApplicationFrameWindow() ? GetStoreAppProcessId(window_->GetWindowHandle()) : window_->GetProcessId();
    if (processId == 0) processId = window_->GetProcessId();

    const auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!hProcess) return;
    ScopedReleaser processCloser([&] { ::CloseHandle(hProcess); });

    WCHAR dirPathBuf[512];
    DWORD len = 512;
    if (!::QueryFullProcessImageNameW(hProcess, 0, dirPathBuf, &len)) return;
    if (!::PathRemoveFileSpecW(dirPathBuf)) return;

    const std::wstring dirPath = dirPathBuf;
    std::wifstream fs(dirPath + L"\\AppxManifest.xml");
    if (!fs) return;

    std::wstring logoRelativePath;
    std::wregex regex(L"<Logo>([^<]+)</Logo>");
    std::wstring line;
    while (std::getline(fs, line)) {
        std::wsmatch match {};
        if (std::regex_search(line, match, regex)) { logoRelativePath = match[1].str().c_str(); break; }
    }
    if (logoRelativePath.empty()) return;

    const auto logoPath = dirPath + L"\\" + logoRelativePath;
    if (::PathFileExistsW(logoPath.c_str())) { appLogoPath_ = logoPath; CreateIconFromAppLogoPath(); return; }

    std::wregex regexFile(L"(.+)\\\\([^\\.\\\\]+?)\\.(.+)$");
    std::wsmatch match {};
    if (!std::regex_search(logoRelativePath, match, regexFile)) return;

    const auto searchQuery = dirPath + L"\\" + match[1].str() + L"\\" + match[2].str() + L".scale-*." + match[3].str();
    WIN32_FIND_DATAW fd;
    auto hFile = ::FindFirstFileW(searchQuery.c_str(), &fd);
    ScopedReleaser fileCloser([&] { ::FindClose(hFile); });

    int maxScale = 0;
    do {
        std::wregex sRegex(L"\\.scale-([^\\.]+)\\.");
        std::wsmatch sMatch {};
        std::wstring fName = fd.cFileName;
        if (std::regex_search(fName, sMatch, sRegex)) {
            const int scale = std::stoi(sMatch[1].str());
            if (scale > maxScale) maxScale = scale;
        }
    } while (FindNextFileW(hFile, &fd));

    if (maxScale == 0) return;
    appLogoPath_ = dirPath + L"\\" + match[1].str() + L"\\" + match[2].str() + L".scale-" + std::to_wstring(maxScale) + L"." + match[3].str();
    CreateIconFromAppLogoPath();
}

void IconTexture::CreateIconFromAppLogoPath()
{
    if (appLogoPath_.empty()) return;
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);
    ScopedReleaser gdiShutdowner([&] { Gdiplus::GdiplusShutdown(token); });

    auto image = Gdiplus::Bitmap::FromFile(appLogoPath_.c_str());
    if (!image || image->GetLastStatus() != Gdiplus::Ok) return;
    ScopedReleaser imageDeleter([&] { delete image; });

    width_ = image->GetWidth();
    height_ = image->GetHeight();
    image->GetHICON(&hIcon_);
}

UINT IconTexture::GetWidth() const { return width_ > 0 ? width_ : ::GetSystemMetrics(SM_CXICON); }
UINT IconTexture::GetHeight() const { return height_ > 0 ? height_ : ::GetSystemMetrics(SM_CYICON); }
void IconTexture::SetUnityTexturePtr(ID3D11Texture2D* ptr) { unityTexture_ = ptr; }
ID3D11Texture2D* IconTexture::GetUnityTexturePtr() const { return unityTexture_; }

bool IconTexture::Capture()
{
    if (!hIcon_) return false;
    ICONINFO info;
    if (!::GetIconInfo(hIcon_, &info)) return false;
    ScopedReleaser iconReleaser([&] { ::DeleteObject(info.hbmColor); ::DeleteObject(info.hbmMask); });

    auto hDcMem = ::CreateCompatibleDC(NULL);
    ScopedReleaser hDcReleaser([&] { ::DeleteDC(hDcMem); });

    BITMAPINFOHEADER bmi {};
    bmi.biWidth = static_cast<LONG>(width_);
    bmi.biHeight = -static_cast<LONG>(height_);
    bmi.biPlanes = 1;
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biBitCount = 32;
    bmi.biCompression = BI_RGB;

    Buffer<BYTE> color;
    color.ExpandIfNeeded(width_ * height_ * 4);
    if (!::GetDIBits(hDcMem, info.hbmColor, 0, height_, color.Get(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS)) return false;

    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        buffer_.ExpandIfNeeded(width_ * height_ * 4);

        auto* buffer32 = buffer_.As<UINT>();
        const auto* color32 = color.As<UINT>();
        bool areAllPixelsAlphaZero = true;

        for (UINT x = 0; x < width_; ++x) {
            for (UINT y = 0; y < height_; ++y) {
                const auto i = y * width_ + x;
                const auto j = (height_ - 1 - y) * width_ + x;
                buffer32[j] = color32[i];
                if ((color32[i] & 0xff000000) != 0) areAllPixelsAlphaZero = false;
            }
        }
        if (areAllPixelsAlphaZero) {
            for (UINT i = 0; i < width_ * height_; ++i) buffer32[i] |= 0xff000000;
        }
    }
    hasCaptured_ = true;
    return true;
}

bool IconTexture::CaptureOnce() { if (hasCaptured_) return false; return Capture(); }

bool IconTexture::Upload()
{
    if (!unityTexture_.load() || buffer_.Empty()) return false;
    {
        D3D11_TEXTURE2D_DESC desc;
        unityTexture_.load()->GetDesc(&desc);
        if (desc.Width != GetWidth() || desc.Height != GetHeight()) return false;
    }

    std::lock_guard<std::mutex> lock(sharedTextureMutex_);
    auto device = uWindowCapture::CaptureManager::Get().GetCaptureDevice();
    if (!device) return false;

    if (!sharedResource_ || sharedResource_->GetWidth() != GetWidth() || sharedResource_->GetHeight() != GetHeight()) {
        sharedResource_ = std::make_shared<uWindowCapture::SharedTextureResource>();
        sharedResource_->Initialize(device->GetDevice(), GetWidth(), GetHeight());
    }

    {
        std::lock_guard<std::mutex> block(bufferMutex_);
        auto context = device->GetContext();
        context->UpdateSubresource(sharedResource_->GetTexture(), 0, nullptr, buffer_.Get(), GetWidth() * 4, 0);
        sharedResource_->SignalFence(context);
    }

    hasUploaded_ = true;
    return true;
}

bool IconTexture::UploadOnce() { if (hasUploaded_) return false; return Upload(); }

bool IconTexture::Render()
{
    if (!unityTexture_.load()) return false;

    std::lock_guard<std::mutex> lock(sharedTextureMutex_);
    if (!sharedResource_) return false;

    auto context = uWindowCapture::GraphicsManager::Get().GetContext();
    if (context) {
        context->RegisterSharedResource(unityTexture_.load(), sharedResource_.get());
    }

    MessageManager::Get().Add({ MessageType::IconCaptured, window_->GetId(), window_->GetWindowHandle() });
    hasRendered_ = true;
    return true;
}

bool IconTexture::RenderOnce() { if (hasRendered_) return true; return Render(); }
