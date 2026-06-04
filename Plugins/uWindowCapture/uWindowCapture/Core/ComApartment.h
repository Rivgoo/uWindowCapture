#pragma once
#include <Windows.h>
#include <winrt/base.h>
#include "Debug.h"

namespace uWindowCapture {

    class ScopedComApartment {
    public:
        ScopedComApartment() {
            try {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
                isInitialized_ = true;
            }
            catch (const winrt::hresult_error& e) {
                const HRESULT hr = e.code();
                if (hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
                    Debug::Error("ScopedComApartment: winrt::init_apartment failed. HRESULT: ", hr);
                }
            }
        }

        ~ScopedComApartment() {
            if (isInitialized_) {
                winrt::uninit_apartment();
            }
        }

        ScopedComApartment(const ScopedComApartment&) = delete;
        ScopedComApartment& operator=(const ScopedComApartment&) = delete;

    private:
        bool isInitialized_ = false;
    };

}