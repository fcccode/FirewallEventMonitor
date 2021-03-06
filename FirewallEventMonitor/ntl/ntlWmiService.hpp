// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// cpp headers
#include <exception>
#include <algorithm>
// os headers
#include <windows.h>
#include <Wbemidl.h>
// local headers
#include "ntlException.hpp"
#include "ntlWmiException.hpp"
#include "ntlComInitialize.hpp"
#include "ntlVersionConversion.hpp"


namespace ntl {

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// class WmiService
    ///
    /// Callers must instantiate a WmiService instance in order to use any Wmi* class.  This class
    ///   tracks the WMI initialization of the IWbemLocator and IWbemService interfaces - which
    ///   maintain a connection to the specified WMI Service through which WMI calls are made.
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    class WmiService
    {
    public:
        /////////////////////////////////////////////////////////////////////////////////////
        ///
        /// Note: current only connecting to the local machine.
        ///
        /// Note: CoInitializeSecurity is not called by the Wmi* classes. This security
        ///   policy should be defined by the code consuming these libraries, as these
        ///   libraries cannot assume the security context to apply to the process.
        ///
        /// Argument:
        /// LPCWSTR _path: this is the WMI namespace path to connect with
        ///
        /////////////////////////////////////////////////////////////////////////////////////
        explicit WmiService(_In_ LPCWSTR _path) : coinit(), wbemLocator(), wbemServices()
        {
            this->wbemLocator = ntl::ComPtr<IWbemLocator>::createInstance(CLSID_WbemLocator, IID_IWbemLocator);

            ntl::ComBstr path(_path);
            HRESULT hr = this->wbemLocator->ConnectServer(
                path.get(),              // Object path of WMI namespace
                nullptr,                 // User name. NULL = current user
                nullptr,                 // User password. NULL = current
                nullptr,                 // Locale. NULL indicates current
                0,                    // Security flags.
                nullptr,                 // Authority (e.g. Kerberos)
                nullptr,                 // Context object 
                this->wbemServices.get_addr_of());  // receive pointer to IWbemServices proxy
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"IWbemLocator::ConnectServer", L"WmiService::connect", false);
            }

            hr = ::CoSetProxyBlanket(
                this->wbemServices.get_IUnknown(), // Indicates the proxy to set
                RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
                RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
                nullptr,                     // Server principal name 
                RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
                RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
                nullptr,                     // client identity
                EOAC_NONE                    // proxy capabilities 
               );
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"CoSetProxyBlanket", L"WmiService::connect", false);
            }
        }
        ~WmiService() NOEXCEPT
        {
            // empty
        }
        WmiService(const WmiService& _service) : coinit(), wbemLocator(_service.wbemLocator), wbemServices(_service.wbemServices)
        {
            // empty
        }
        WmiService& operator=(const WmiService& _service)
        {
            WmiService temp(_service);
            using std::swap;
            swap(this->wbemLocator, temp.wbemLocator);
            swap(this->wbemServices, temp.wbemServices);
            return *this;
        }

        ///
        /// TODO: add move c'tor and move assignment operator
        ///

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// operator ->
        /// - exposes the underlying IWbemServices* 
        ///
        /// A no-fail/no-throw operation
        ////////////////////////////////////////////////////////////////////////////////
        IWbemServices* operator->() NOEXCEPT
        {
            return this->wbemServices.get();
        }
        const IWbemServices* operator ->() const NOEXCEPT
        {
            return this->wbemServices.get();
        }

        bool operator ==(const WmiService& _service) const NOEXCEPT
        {
            return (this->wbemLocator == _service.wbemLocator) &&
                (this->wbemServices == _service.wbemServices);
        }
        bool operator != (const WmiService& _service) const NOEXCEPT
        {
            return !(*this == _service);
        }

        IWbemServices* get() NOEXCEPT
        {
            return this->wbemServices.get();
        }
        const IWbemServices* get() const NOEXCEPT
        {
            return this->wbemServices.get();
        }

        void delete_path(_In_ LPCWSTR _objPath, const ntl::ComPtr<IWbemContext>& _context)
        {
            ntl::ComBstr bstrObjectPath(_objPath);
            ntl::ComPtr<IWbemCallResult> result;
            HRESULT hr = this->wbemServices->DeleteInstance(
                bstrObjectPath.get(),
                WBEM_FLAG_RETURN_IMMEDIATELY,
                const_cast<IWbemContext*>(_context.get()),
                result.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, L"IWbemServices::DeleteInstance", L"WmiService::delete_path", false);
            }

            // wait for the call to complete
            HRESULT status;
            hr = result->GetCallStatus(WBEM_INFINITE, &status);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, L"IWbemCallResult::GetCallStatus", L"WmiService::delete_path", false);
            }
            if (FAILED(status)) {
                throw ntl::WmiException(status, L"IWbemServices::DeleteInstance", L"WmiService::delete_path", false);
            }
        }
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// void delete_path(LPCWSTR)
        /// - Deletes the WMI object based off the object path specified in the input
        /// - Throws ntl::WmiException on failures
        ///
        /// The object path takes the form of:
        ///    MyClass.MyProperty1='33',MyProperty2='value'
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void delete_path(_In_ LPCWSTR _objPath)
        {
            ntl::ComPtr<IWbemContext> null_context;
            delete_path(_objPath, null_context);
        }

    private:
        //
        // TODO: tracking CoInitialize should not be occuring within these objects
        // - as this is a per-thread call the caller should be enforcing
        //
        ntl::ComInitialize coinit;
        ntl::ComPtr<IWbemLocator> wbemLocator;
        ntl::ComPtr<IWbemServices> wbemServices;
    };

} // namespace ntl
