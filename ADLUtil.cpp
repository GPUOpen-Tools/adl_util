//==============================================================================
// Copyright (c) 2011-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Interface from Developer Tools to ADL.
//==============================================================================

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <charconv>
#include <cassert>
#include <string_view>

#include "ADLUtil.h"

#include <Windows.h>

// Callback so that ADL can allocate memory
void* __stdcall ADL_Main_Memory_Alloc(int iSize)
{
    void* lpBuffer = malloc(iSize);
    return lpBuffer;
}

// Optional ADL Memory de-allocation function
void __stdcall ADL_Main_Memory_Free(void** lpBuffer)
{
    if (nullptr != *lpBuffer)
    {
        free(*lpBuffer);
        *lpBuffer = nullptr;
    }
}

// Converts a string to its decimal equivalent, the integer base is specified as a template argument
template <int base>
static int adl_from_chars(std::string_view str)
{
    int result{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result, base);
    if (ec == std::errc())
    {
        return result;
    }
    // Otherwise conversion failed
    assert(false);
    return 0;
}

ADLUtil_Result ADLUtil_GetASICInfo(AsicInfoList& asicInfoList)
{
    return AMDTADLUtils::Instance()->GetAsicInfoList(asicInfoList);
}


ADLUtil_Result
ADLUtil_GetVersionsInfo(
    struct ADLVersionsInfo& info)
{
    return AMDTADLUtils::Instance()->GetADLVersionsInfo(info);
}

AMDTADLUtils::AMDTADLUtils() :
    m_libHandle(nullptr),
    m_adlContext(nullptr),
    m_asicInfoListRetVal(ADL_RESULT_NONE),
    m_versionRetVal(ADL_RESULT_NONE)
{
#define X(SYM) m_##SYM = nullptr;
    ADL_INTERFACE_TABLE
#undef X
}

AMDTADLUtils::~AMDTADLUtils()
{
    Unload();
}

ADLUtil_Result AMDTADLUtils::LoadAndInit()
{
    ADLUtil_Result result = ADL_SUCCESS;

    if (nullptr == m_libHandle)
    {
#ifdef _WIN64
        // 64 bit Windows library
        m_libHandle = LoadLibraryA("atiadlxx.dll");
#elif defined(_WIN32)
        // 32 bit Windows library
        m_libHandle = LoadLibraryA("atiadlxy.dll");
#endif

        if (nullptr == m_libHandle)
        {
            result = ADL_NOT_FOUND;
        }

#define X(SYM)                                               \
    m_##SYM = (SYM##_fn)::GetProcAddress(m_libHandle, #SYM); \
    if (nullptr == m_##SYM)                                  \
    {                                                        \
        Unload();                                            \
        result = ADL_MISSING_ENTRYPOINTS;                    \
    }
        ADL_INTERFACE_TABLE;
#undef X

        if (ADL_SUCCESS == result)
        {
            int adlResult;

            // Initialize ADL. The second parameter is 1, which means:
            // retrieve adapter information only for adapters that are physically present and enabled in the system
            if (nullptr != m_ADL2_Main_Control_Create)
            {
                adlResult = m_ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &m_adlContext);
            }
            else
            {
                adlResult = m_ADL_Main_Control_Create(ADL_Main_Memory_Alloc, 1);
            }

            if (ADL_OK != adlResult && ADL_OK_WARNING != adlResult)
            {
                Unload();
                return ADL_INITIALIZATION_FAILED;
            }
        }
    }

    return result;
}

ADLUtil_Result AMDTADLUtils::Unload()
{
    ADLUtil_Result result = ADL_SUCCESS;

    if (nullptr != m_libHandle)
    {
        if (nullptr != m_ADL2_Main_Control_Destroy)
        {
            if (nullptr != m_adlContext)
            {
                m_ADL2_Main_Control_Destroy(m_adlContext);
                m_adlContext = nullptr;
            }
        }
        else if (nullptr != m_ADL_Main_Control_Destroy)
        {
            m_ADL_Main_Control_Destroy();
        }

        FreeLibrary(m_libHandle);
        m_libHandle = nullptr;

#define X(SYM) m_##SYM = nullptr;
        ADL_INTERFACE_TABLE
#undef X
    }

    Reset();

    return result;
}


ADLUtil_Result AMDTADLUtils::GetAsicInfoList(AsicInfoList& asicInfoList)
{
    std::lock_guard<std::mutex> lock(m_asicInfoMutex);

    if (ADL_RESULT_NONE == m_asicInfoListRetVal)
    {
        m_asicInfoListRetVal = LoadAndInit();

        if (ADL_SUCCESS == m_asicInfoListRetVal)
        {
            int adlResult = ADL_OK;

            int numAdapter = 0;

            // Obtain the number of logical adapters for the system
            // EX: Even if you only have 2 physical GPUs you may logically have 10 adapters.
            if (nullptr != m_ADL2_Adapter_NumberOfAdapters_Get)
            {
                adlResult = m_ADL2_Adapter_NumberOfAdapters_Get(m_adlContext, &numAdapter);
            }
            else
            {
                adlResult = m_ADL_Adapter_NumberOfAdapters_Get(&numAdapter);
            }

            if (ADL_OK != adlResult)
            {
                m_asicInfoListRetVal = ADL_GET_ADAPTER_COUNT_FAILED;
            }
            else
            {
                if (0 < numAdapter)
                {
                    LPAdapterInfo lpAdapterInfo = reinterpret_cast<LPAdapterInfo>(malloc(sizeof(AdapterInfo) * numAdapter));

                    if (nullptr == lpAdapterInfo)
                    {
                        return ADLUtil_Result::ADL_GET_ADAPTER_INFO_FAILED;
                    }
                    else
                    {
                        memset(lpAdapterInfo, '\0', sizeof(AdapterInfo) * numAdapter);

                        // Get the AdapterInfo structure for all adapters in the system
                        if (nullptr != m_ADL2_Adapter_AdapterInfo_Get)
                        {
                            adlResult = m_ADL2_Adapter_AdapterInfo_Get(m_adlContext, lpAdapterInfo, sizeof(AdapterInfo) * numAdapter);
                        }
                        else
                        {
                            adlResult = m_ADL_Adapter_AdapterInfo_Get(lpAdapterInfo, sizeof(AdapterInfo) * numAdapter);
                        }

                        if (ADL_OK == adlResult)
                        {
                            for (int i = 0; i < numAdapter; ++i)
                            {
                                std::string_view adapterName = lpAdapterInfo[i].strAdapterName;
                                std::string_view adapterInfo = lpAdapterInfo[i].strUDID;

                                // trim trailing whitespace
                                size_t lastNonSpace = adapterName.length() - 1;

                                while (adapterName[lastNonSpace] == ' ')
                                {
                                    --lastNonSpace;
                                }

                                ADLUtil_ASICInfo asicInfo;
                                asicInfo.adapterName = adapterName.substr(0, lastNonSpace + 1);
                                asicInfo.gpuIndex = 0;

                                size_t vendorIndex = adapterInfo.find("PCI_VEN_") + strlen("PCI_VEN_");
                                size_t devIndex = adapterInfo.find("&DEV_") + strlen("&DEV_");
                                size_t revIndex = adapterInfo.find("&REV_") + strlen("&REV_");

                                if (vendorIndex != std::string_view::npos)
                                {
                                    std::string_view vendorIDString = adapterInfo.substr(vendorIndex, 4);
                                    asicInfo.vendorID = adl_from_chars<16>(vendorIDString);
                                }
                                else
                                {
                                    asicInfo.vendorID = 0;
                                }

                                if (devIndex != std::string_view::npos)
                                {
                                    asicInfo.deviceIDString = adapterInfo.substr(devIndex, 4);
                                    asicInfo.deviceID = adl_from_chars<16>(asicInfo.deviceIDString);
                                }
                                else
                                {
                                    asicInfo.deviceIDString.clear();
                                    asicInfo.deviceID = 0;
                                }

                                if (revIndex != std::string_view::npos)
                                {
                                    std::string_view revIDString = adapterInfo.substr(revIndex, 2);
                                    asicInfo.revID = adl_from_chars<16>(revIDString);
                                }
                                else
                                {
                                    asicInfo.revID = 0;
                                }

                                asicInfo.registryPath = lpAdapterInfo[i].strDriverPath;
                                asicInfo.registryPathExt = lpAdapterInfo[i].strDriverPathExt;

                                m_asicInfoList.push_back(asicInfo);
                            }

                            ADL_Main_Memory_Free(reinterpret_cast<void**>(&lpAdapterInfo));
                        }
                        else
                        {
                            m_asicInfoListRetVal = ADL_GET_ADAPTER_INFO_FAILED;
                        }
                    }
                }
            }
        }
    }

    asicInfoList = m_asicInfoList;
    return m_asicInfoListRetVal;
}

ADLUtil_Result AMDTADLUtils::GetADLVersionsInfo(ADLVersionsInfo& adlVersionInfo)
{
    std::lock_guard<std::mutex> lock(m_adlVersionsMutex);

    if (ADL_RESULT_NONE == m_versionRetVal)
    {
        m_versionRetVal = LoadAndInit();

        if (ADL_SUCCESS == m_versionRetVal)
        {
            int adlResult = ADL_OK;

            if (nullptr != m_ADL2_Graphics_Versions_Get)
            {
                adlResult = m_ADL2_Graphics_Versions_Get(m_adlContext, &m_adlVersionsInfo);
            }
            else
            {
                adlResult = m_ADL_Graphics_Versions_Get(&m_adlVersionsInfo);
            }

            if (ADL_OK != adlResult)
            {
                if (ADL_OK_WARNING == adlResult)
                {
                    m_versionRetVal = ADL_WARNING;
                }
                else // ADL_OK_WARNING != adlResult
                {
                    m_versionRetVal = ADL_GRAPHICS_VERSIONS_GET_FAILED;
                }
            }
        }
    }

    adlVersionInfo = m_adlVersionsInfo;
    return m_versionRetVal;
}

ADLUtil_Result AMDTADLUtils::GetDriverVersion(unsigned int& majorVer, unsigned int& minorVer, unsigned int& subMinorVer) const
{
    majorVer = 0;
    minorVer = 0;
    subMinorVer = 0;

    ADLVersionsInfo driverVerInfo;
    ADLUtil_Result adlResult = AMDTADLUtils::Instance()->GetADLVersionsInfo(driverVerInfo);

    if (adlResult == ADL_SUCCESS)
    {
        std::string_view strDriverVersion(driverVerInfo.strDriverVer);

        // driver version looks like:  13.35.1005-140131a-167669E-ATI or 14.10-140115n-021649E-ATI, etc...
        // truncate at the first dash
        strDriverVersion = strDriverVersion.substr(0, strDriverVersion.find('-', 0));

        constexpr char strDelimiter = '.';

        // parse the major driver version
        size_t pos = strDriverVersion.find(strDelimiter);

        if (pos != std::string_view::npos)
        {
            std::string_view strToken = strDriverVersion.substr(0, pos);
            std::stringstream ss;
            ss.str(strToken.data());

            if ((ss >> majorVer).fail())
            {
                majorVer = 0;
            }
            else
            {
                adlResult = ADL_SUCCESS;
                strDriverVersion.remove_prefix(pos + 1);
            }

            // parse the minor driver version
            bool subMinorAvailable = false;

            pos = strDriverVersion.find(strDelimiter);

            if (pos != std::string_view::npos)
            {
                strToken = strDriverVersion.substr(0, pos);
                strDriverVersion.remove_prefix(pos + 1);
                subMinorAvailable = true;
            }
            else
            {
                strToken = strDriverVersion;
            }

            ss.clear();
            ss.str(strToken.data());

            if ((ss >> minorVer).fail())
            {
                minorVer = 0;
            }

            // parse the sub-minor driver version
            if (subMinorAvailable)
            {
                pos = strDriverVersion.find(strDelimiter);

                if (pos != std::string_view::npos)
                {
                    strToken = strDriverVersion.substr(0, pos);
                    strDriverVersion.remove_prefix(pos + 1);
                }
                else
                {
                    strToken = strDriverVersion;
                }

                ss.clear();
                ss.str(strToken.data());

                if ((ss >> subMinorVer).fail())
                {
                    subMinorVer = 0;
                }
            }
        }
    }

    return adlResult;
}

void AMDTADLUtils::Reset()
{
    m_asicInfoList.clear();
    m_asicInfoListRetVal = ADL_RESULT_NONE;
    m_versionRetVal = ADL_RESULT_NONE;
}
