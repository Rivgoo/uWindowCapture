#include "Cursor.h"
#include "../Core/Debug.h"
#include "../Core/Util.h"
#include "WindowManager.h"
#include "CaptureManager.h"
#include "IsolatedCaptureDevice.h"
#include "../Interop/SharedTextureResource.h"
#include "../Graphics/GraphicsManager.h"
#include "../Core/Message.h"

using namespace Microsoft::WRL;

Cursor::Cursor() {}
Cursor::~Cursor() { DeleteBitmap(); }
void Cursor::StartCapture() {}
void Cursor::StopCapture() {}
void Cursor::RequestCapture() { isCaptureRequested_ = true; }
UINT Cursor::GetX() const { return x_; }
UINT Cursor::GetY() const { return y_; }
UINT Cursor::GetWidth() const { return width_; }
UINT Cursor::GetHeight() const { return height_; }
bool Cursor::HasCaptured() const { return hasCaptured_; }
bool Cursor::HasUploaded() const { return hasUploaded_; }
void Cursor::SetUnityTexturePtr(ID3D11Texture2D* ptr) { unityTexture_ = ptr; }
ID3D11Texture2D* Cursor::GetUnityTexturePtr() const { return unityTexture_; }

bool Cursor::Capture()
{
    if (!isCaptureRequested_) return false;
    isCaptureRequested_ = false;

    std::lock_guard<std::mutex> lock(cursorMutex_);

    CURSORINFO cursorInfo;
    cursorInfo.cbSize = sizeof(CURSORINFO);
    if (!::GetCursorInfo(&cursorInfo)) return false;

    x_ = cursorInfo.ptScreenPos.x;
    y_ = cursorInfo.ptScreenPos.y;

    ICONINFO iconInfo;
    ::ZeroMemory(&iconInfo, sizeof(ICONINFO));
    if (!::GetIconInfo(cursorInfo.hCursor, &iconInfo)) return false;
    ScopedReleaser iconReleaser([&] { 
        if (iconInfo.hbmColor) ::DeleteObject(iconInfo.hbmColor); 
        if (iconInfo.hbmMask) ::DeleteObject(iconInfo.hbmMask); 
    });

    int width = 0, height = 0;
    {
        BITMAP bmp;
        ::ZeroMemory(&bmp, sizeof(BITMAP));
        if (iconInfo.hbmColor) {
            ::GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);
            width = bmp.bmWidth; height = bmp.bmHeight;
        } else if (iconInfo.hbmMask) {
            ::GetObject(iconInfo.hbmMask, sizeof(BITMAP), &bmp);
            width = bmp.bmWidth; height = bmp.bmHeight / 2;
        } else return false;
    }

    auto desktopDc = ::GetDC(GetDesktopWindow());
    ScopedReleaser hDcReleaser([&] { ::ReleaseDC(GetDesktopWindow(), desktopDc); });
    auto hDcMem = ::CreateCompatibleDC(NULL);
    ScopedReleaser hDcMemReleaser([&] { ::DeleteDC(hDcMem); });

    CreateBitmapIfNeeded(desktopDc, width, height);

    BITMAPINFOHEADER bmi {};
    bmi.biWidth = static_cast<LONG>(width_);
    bmi.biHeight = -static_cast<LONG>(height_);
    bmi.biPlanes = 1;
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biBitCount = 32;
    bmi.biCompression = BI_RGB;

    Buffer<BYTE> desktop(width_ * height_ * 4);
    Buffer<BYTE> desktopWithIcon(width_ * height_ * 4);
    Buffer<BYTE> icon(width_ * height_ * 4);

    HGDIOBJ preObject = ::SelectObject(hDcMem, bitmap_);
    {
        ::BitBlt(hDcMem, 0, 0, width_, height_, desktopDc, x_ - iconInfo.xHotspot, y_ - iconInfo.yHotspot, SRCCOPY);
        ::GetDIBits(hDcMem, bitmap_, 0, height_, desktop.Get(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
        ::DrawIcon(hDcMem, 0, 0, cursorInfo.hCursor);
        ::GetDIBits(hDcMem, bitmap_, 0, height_, desktopWithIcon.Get(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
        ::GetDIBits(hDcMem, iconInfo.hbmColor, 0, height_, icon.Get(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
    }
    ::SelectObject(hDcMem, preObject);

    {
        std::lock_guard<std::mutex> block(bufferMutex_);
        auto buffer32 = buffer_.As<UINT>();
        const auto desktop32 = desktop.As<UINT>();
        const auto desktopWithIcon32 = desktopWithIcon.As<UINT>();
        const auto icon32 = icon.As<UINT>();

        for (UINT x = 0; x < width_; ++x) {
            for (UINT y = 0; y < height_; ++y) {
                const auto i = y * width_ + x;
                const auto j = (height_ - 1 - y) * width_ + x;
                if (icon[4 * j + 3] > 0) {
                    buffer32[i] = icon32[j];
                } else {
                    buffer_[4 * i + 0] = desktopWithIcon[4 * j + 0];
                    buffer_[4 * i + 1] = desktopWithIcon[4 * j + 1];
                    buffer_[4 * i + 2] = desktopWithIcon[4 * j + 2];
                    buffer_[4 * i + 3] = (desktop32[j] != desktopWithIcon32[j]) ? 255 : 0;
                }
            }
        }
    }
    hasCaptured_ = true;
    return true;
}

bool Cursor::Upload()
{
    if (!hasCaptured_ || !unityTexture_.load() || buffer_.Empty()) return false;

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

    hasCaptured_ = false;
    hasUploaded_ = true;
    return true;
}

bool Cursor::Render()
{
    if (!hasUploaded_ || !unityTexture_.load()) return false;

    std::lock_guard<std::mutex> lock(sharedTextureMutex_);
    if (!sharedResource_) return false;

    auto context = uWindowCapture::GraphicsManager::Get().GetContext();
    if (context) {
        context->RegisterSharedResource(unityTexture_.load(), sharedResource_.get());
    }

    MessageManager::Get().Add({ MessageType::CursorCaptured, -1, nullptr });
    hasUploaded_ = false;
    return true;
}

void Cursor::CreateBitmapIfNeeded(HDC hDc, UINT width, UINT height)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (width_ == width && height_ == height) return;
    width_ = width; height_ = height;
    buffer_.ExpandIfNeeded(width * height * 4);
    DeleteBitmap();
    bitmap_ = ::CreateCompatibleBitmap(hDc, width, height);
}

void Cursor::DeleteBitmap()
{
    if (bitmap_ != nullptr) { ::DeleteObject(bitmap_); bitmap_ = nullptr; }
}
