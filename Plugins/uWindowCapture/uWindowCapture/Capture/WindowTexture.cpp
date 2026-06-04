#include <dwmapi.h>
#include "WindowTexture.h"
#include "WindowsGraphicsCapture.h"
#include "Window.h"
#include "WindowManager.h"
#include "CaptureManager.h"
#include "IsolatedCaptureDevice.h"
#include "../Interop/SharedTextureResource.h"
#include "../Graphics/GraphicsManager.h"
#include "../Core/Message.h"
#include "../Core/Debug.h"
#include "../Core/Util.h"

using namespace Microsoft::WRL;
using namespace uWindowCapture;

WindowTexture::WindowTexture(Window* window)
    : window_(window)
{
    if (const auto& wgcManager = WindowManager::GetWindowsGraphicsCaptureManager())
    {
        if (window_->IsDesktop()) windowsGraphicsCapture_ = wgcManager->Create(window_->GetMonitorHandle());
        else windowsGraphicsCapture_ = wgcManager->Create(window_->GetWindowHandle());
    }
}

WindowTexture::~WindowTexture()
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    DeleteBitmap();

    if (auto wgc = windowsGraphicsCapture_.lock())
    {
        if (const auto& wgcManager = WindowManager::GetWindowsGraphicsCaptureManager())
            wgcManager->Destroy(wgc);
    }
}

void WindowTexture::SetUnityTexturePtr(ID3D11Texture2D* ptr)
{
    unityTexture_ = ptr;
}

ID3D11Texture2D* WindowTexture::GetUnityTexturePtr() const
{
    return unityTexture_;
}

void WindowTexture::SetCaptureMode(CaptureMode mode) { captureMode_ = mode; }
CaptureMode WindowTexture::GetCaptureMode() const { return captureMode_; }

CaptureMode WindowTexture::GetCaptureModeInternal() const
{
    if (captureMode_ == CaptureMode::Auto)
    {
        if (IsWindowsGraphicsCaptureAvailable()) return CaptureMode::WindowsGraphicsCapture;
        else if (window_->IsDesktop() || isPrintWindowFailed_) return CaptureMode::BitBlt;
        else return CaptureMode::PrintWindow;
    }
    if (captureMode_ == CaptureMode::PrintWindow && window_->IsDesktop()) return CaptureMode::BitBlt;
    return captureMode_;
}

void WindowTexture::SetCursorDraw(bool draw) { drawCursor_ = draw; }
bool WindowTexture::GetCursorDraw() const { return drawCursor_; }
UINT WindowTexture::GetWidth() const { return textureWidth_; }
UINT WindowTexture::GetHeight() const { return textureHeight_; }
UINT WindowTexture::GetOffsetX() const { return offsetX_; }
UINT WindowTexture::GetOffsetY() const { return offsetY_; }

void WindowTexture::CreateBitmapIfNeeded(HDC hDc, UINT width, UINT height)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);

    if (bufferWidth_ == width && bufferHeight_ == height) return;
    if (width == 0 || height == 0) return;

    bufferWidth_ = width;
    bufferHeight_ = height;
    buffer_.ExpandIfNeeded(width * height * 4);

    DeleteBitmap();
    bitmap_ = ::CreateCompatibleBitmap(hDc, width, height);

    SetUnityTexturePtr(nullptr);
}

void WindowTexture::DeleteBitmap()
{
    if (bitmap_ != nullptr) 
    {
        if (!::DeleteObject(bitmap_)) OutputApiError(__FUNCTION__, "DeleteObject");
        bitmap_ = nullptr;
    }
}

bool WindowTexture::IsWindowsGraphicsCapture() const
{
    return GetCaptureModeInternal() == CaptureMode::WindowsGraphicsCapture;
}

std::shared_ptr<WindowsGraphicsCapture> WindowTexture::GetWindowsGraphicsCapture() const
{
    return windowsGraphicsCapture_.lock();
}

bool WindowTexture::Capture()
{
    if (IsWindowsGraphicsCapture()) return CaptureByWindowsGraphicsCapture();
    else return CaptureByWin32API();
}

bool WindowTexture::CaptureByWin32API()
{
    auto hWnd = window_->GetWindowHandle();
    auto hDc = ::GetDC(hWnd);
    ScopedReleaser hDcReleaser([&] { ::ReleaseDC(hWnd, hDc); });

    BITMAP bmpHeader;
    ZeroMemory(&bmpHeader, sizeof(BITMAP));
    auto hBitmap = ::GetCurrentObject(hDc, OBJ_BITMAP);
    GetObject(hBitmap, sizeof(BITMAP), &bmpHeader);
    auto dcWidth = bmpHeader.bmWidth;
    auto dcHeight = bmpHeader.bmHeight;

    if (dcWidth == 0 || dcHeight == 0 || window_->IsDesktop())
    {
        dcWidth = window_->GetWidth();
        dcHeight = window_->GetHeight();
    }

    if (dcWidth == 0 || dcHeight == 0) return false;

    dpiScaleX_ = std::fmax(static_cast<float>(window_->GetWidth()) / dcWidth, 0.01f);
    dpiScaleY_ = std::fmax(static_cast<float>(window_->GetHeight()) / dcHeight, 0.01f);

    if (GetCaptureModeInternal() == CaptureMode::BitBlt && !window_->IsDesktop())
    {
        const auto frameWidth = window_->GetWidth() - window_->GetClientWidth();
        const auto frameHeight = window_->GetHeight() - window_->GetClientHeight();
        dcWidth -= static_cast<LONG>(ceil(frameWidth / dpiScaleX_));
        dcHeight -= static_cast<LONG>(ceil(frameHeight / dpiScaleY_));
    }

    CreateBitmapIfNeeded(hDc, dcWidth, dcHeight);

    {
        UWC_SCOPE_TIMER(DwmGetWindowAttribute)
        const UINT preTextureWidth = textureWidth_;
        const UINT preTextureHeight = textureHeight_;

        if (GetCaptureModeInternal() == CaptureMode::PrintWindow)
        {
            RECT windowRect;
            ::GetWindowRect(hWnd, &windowRect);
            RECT dwmRect;
            ::DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &dwmRect, sizeof(RECT));

            offsetX_ = max(dwmRect.left - windowRect.left, 0);
            offsetY_ = max(dwmRect.top - windowRect.top, 0);
            textureWidth_ = static_cast<UINT>((dwmRect.right - dwmRect.left) / dpiScaleX_);
            textureHeight_ = static_cast<UINT>((dwmRect.bottom - dwmRect.top) / dpiScaleY_);
        }
        else 
        {
            offsetX_ = 0;
            offsetY_ = 0;
            textureWidth_ = bufferWidth_.load();
            textureHeight_ = bufferHeight_.load();
        }

        if (textureWidth_ != preTextureWidth || textureHeight_ != preTextureHeight)
        {
            MessageManager::Get().Add({ MessageType::WindowSizeChanged, window_->GetId(), window_->GetWindowHandle() });
        }
    }

    auto hDcMem = ::CreateCompatibleDC(hDc);
    ScopedReleaser hDcMemRelaser([&] { ::DeleteDC(hDcMem); });

    HGDIOBJ preObject = ::SelectObject(hDcMem, bitmap_);
    ScopedReleaser selectObject([&] { ::SelectObject(hDcMem, preObject); });

    switch (GetCaptureModeInternal())
    {
        case CaptureMode::PrintWindow:
            if (!::PrintWindow(hWnd, hDcMem, PW_RENDERFULLCONTENT)) 
            {
                isPrintWindowFailed_ = true;
                return false;
            }
            break;
        case CaptureMode::BitBlt:
            {
                const bool isDesktop = window_->IsDesktop();
                const auto x = isDesktop ? window_->GetX() : 0;
                const auto y = isDesktop ? window_->GetY() : 0;
                if (!::BitBlt(hDcMem, 0, 0, bufferWidth_, bufferHeight_, hDc, x, y, SRCCOPY | CAPTUREBLT)) return false;
            }
            break;
        default: return false;
    }

    if (drawCursor_) DrawCursorByWin32API(hWnd, hDcMem);

    BITMAPINFOHEADER bmi {};
    bmi.biWidth       = static_cast<LONG>(bufferWidth_);
    bmi.biHeight      = -static_cast<LONG>(bufferHeight_);
    bmi.biPlanes      = 1;
    bmi.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.biBitCount    = 32;
    bmi.biCompression = BI_RGB;

    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        if (!::GetDIBits(hDcMem, bitmap_, 0, bufferHeight_, buffer_.Get(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS))
        {
            return false;
        }
    }
    return true;
}

void WindowTexture::DrawCursorByWin32API(HWND hWnd, HDC hDcMem)
{
    const auto cursorWindow = WindowManager::Get().GetCursorWindow();
    const bool isCursorWindow = cursorWindow && cursorWindow->GetWindowHandle() == window_->GetWindowHandle();
    if (!isCursorWindow && !window_->IsDesktop()) return;

    CURSORINFO cursorInfo { 0 };
    cursorInfo.cbSize = sizeof(CURSORINFO);
    if (!::GetCursorInfo(&cursorInfo)) return;
    POINT pos = cursorInfo.ptScreenPos;
    if (cursorInfo.flags != CURSOR_SHOWING) return;

    int localX = pos.x;
    int localY = pos.y;

    if (GetCaptureModeInternal() == CaptureMode::PrintWindow) {
        localX = static_cast<int>((pos.x - window_->GetX()) / dpiScaleX_);
        localY = static_cast<int>((pos.y - window_->GetY()) / dpiScaleY_);
    } else if (GetCaptureModeInternal() == CaptureMode::BitBlt) {
        if (window_->IsDesktop()) {
            localX -= window_->GetX();
            localY -= window_->GetY();
        } else {
            if (::ScreenToClient(hWnd, &pos)) {
                localX = static_cast<int>(pos.x / dpiScaleX_);
                localY = static_cast<int>(pos.y / dpiScaleY_);
            }
        }
    }
    ::DrawIcon(hDcMem, localX, localY, cursorInfo.hCursor);
}

bool WindowTexture::CaptureByWindowsGraphicsCapture()
{
    auto wgc = windowsGraphicsCapture_.lock();
    if (!wgc) return false;

    if (!wgc->IsStarted()) wgc->RequestStart();
    wgc->EnableCursorCapture(GetCursorDraw());

    textureWidth_ = wgc->GetWidth();
    textureHeight_ = wgc->GetHeight();
    offsetX_ = 0;
    offsetY_ = 0;

    return true;
}

bool WindowTexture::Upload()
{
    if (!RecreateSharedTextureIfNeeded()) return false;

    if (IsWindowsGraphicsCapture()) return UploadByWindowsGraphicsCapture();
    else return UploadByWin32API();
}

bool WindowTexture::RecreateSharedTextureIfNeeded()
{
    if (!unityTexture_.load()) 
    {
        MessageManager::Get().Add({ MessageType::TextureNullError, window_->GetId(), nullptr });
        return false;
    }

    if (GetWidth() == 0 || GetHeight() == 0) return false;

    {
        D3D11_TEXTURE2D_DESC desc;
        unityTexture_.load()->GetDesc(&desc);
        if (desc.Width != GetWidth() || desc.Height != GetHeight())
        {
            MessageManager::Get().Add({ MessageType::TextureSizeError, window_->GetId(), nullptr });
            return false;
        }
    }

    bool shouldUpdateTexture = true;
    std::lock_guard<std::mutex> lock(sharedTextureMutex_);

    if (sharedResource_)
    {
        if (sharedResource_->GetWidth() == GetWidth() && sharedResource_->GetHeight() == GetHeight())
            shouldUpdateTexture = false;
    }

    if (shouldUpdateTexture)
    {
        auto device = CaptureManager::Get().GetCaptureDevice();
        if (!device) return false;

        sharedResource_ = std::make_shared<SharedTextureResource>();
        if (!sharedResource_->Initialize(device->GetDevice(), GetWidth(), GetHeight()))
        {
            sharedResource_.reset();
            return false;
        }
    }

    return true;
}

bool WindowTexture::UploadByWin32API()
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    auto device = CaptureManager::Get().GetCaptureDevice();
    if (!device) return false;

    const UINT rawPitch = bufferWidth_ * 4;
    const int startIndex = offsetX_ * 4 + offsetY_ * rawPitch;
    const auto* start = buffer_.Get(startIndex);

    {
        std::lock_guard<std::mutex> slock(sharedTextureMutex_);
        if (!sharedResource_) return false;
        auto context = device->GetContext();
        context->UpdateSubresource(sharedResource_->GetTexture(), 0, nullptr, start, rawPitch, 0);
        sharedResource_->SignalFence(context);
    }
    return true;
}

bool WindowTexture::UploadByWindowsGraphicsCapture()
{
    auto wgc = windowsGraphicsCapture_.lock();
    if (!wgc) return false;

    auto sharedRes = wgc->GetSharedResource();
    if (!sharedRes) return false;

    {
        std::lock_guard<std::mutex> lock(sharedTextureMutex_);
        sharedResource_ = sharedRes; // WGC already copied and signaled the fence!
    }
    return true;
}

bool WindowTexture::Render()
{
    if (!unityTexture_.load()) return false;

    std::lock_guard<std::mutex> lock(sharedTextureMutex_);
    if (!sharedResource_) return false;

    auto context = GraphicsManager::Get().GetContext();
    if (context) {
        context->RegisterSharedResource(unityTexture_.load(), sharedResource_.get());
    }

    MessageManager::Get().Add({ MessageType::WindowCaptured, window_->GetId(), window_->GetWindowHandle() });
    return true;
}

BYTE* WindowTexture::GetBuffer()
{
    if (buffer_.Empty()) return nullptr;
    std::lock_guard<std::mutex> lock(bufferMutex_);
    bufferForGetBuffer_.ExpandIfNeeded(buffer_.Size());
    memcpy(bufferForGetBuffer_.Get(), buffer_.Get(), buffer_.Size());
    return bufferForGetBuffer_.Get();
}

UINT WindowTexture::GetPixel(int x, int y) const
{
    BYTE output[4];
    if (GetPixels(output, x, y, 1, 1)) return *reinterpret_cast<UINT*>(output);
    return 0;
}

bool WindowTexture::GetPixels(BYTE* output, int x, int y, int width, int height) const
{
    if (!buffer_) return false;
    int bufferWidth = bufferWidth_.load();
    int bufferHeight = bufferHeight_.load();
    if (x < 0 || x + width >= bufferWidth || y < 0 || y + height >= bufferHeight) return false;

    std::lock_guard<std::mutex> lock(bufferMutex_);
    constexpr int rgba = 4;
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            for (int c = 0; c < rgba; ++c) {
                const int indexOut = i + j * width;
                const int indexIn = (x + i) + (y + (height - 1 - j)) * bufferWidth_;
                output[indexOut * rgba + 0] = buffer_[indexIn * rgba + 2];
                output[indexOut * rgba + 1] = buffer_[indexIn * rgba + 1];
                output[indexOut * rgba + 2] = buffer_[indexIn * rgba + 0];
                output[indexOut * rgba + 3] = buffer_[indexIn * rgba + 3];
            }
        }
    }
    return true;
}

bool WindowTexture::IsWindowsGraphicsCaptureAvailable() const
{
    auto wgc = windowsGraphicsCapture_.lock();
    return wgc && wgc->IsAvailable();
}
