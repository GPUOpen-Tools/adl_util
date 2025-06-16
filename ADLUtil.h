//==============================================================================
// Copyright (c) 2011-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Interface from Developer Tools to ADL.
//==============================================================================

#ifndef _ADL_UTIL_H_
#define _ADL_UTIL_H_

#ifdef _LINUX
    #ifndef LINUX
        #define LINUX
    #endif
#endif

#include <mutex>
#include <string>
#include <vector>

#include "adl_sdk.h"
#include "TSingleton.h"

#ifdef __GNUC__
    #define ADLUTIL_DEPRECATED __attribute__((deprecated))
    typedef void* ADLModule;
#elif _WIN32
    #include <windows.h>
    #define ADLUTIL_DEPRECATED __declspec(deprecated)
    typedef HINSTANCE ADLModule;
#else
    #define ADLUTIL_DEPRECATED
#endif

/// Stores ASIC information that is parsed from data supplied by ADL
struct ADLUtil_ASICInfo
{
    std::string adapterName;      ///< description of the adapter ie "ATI Radeon HD 5800 series"
    std::string deviceIDString;   ///< string version of the deviceID (for easy comparing since the deviceID is hex, but stored as int)
    int vendorID;                 ///< the vendor ID
    int deviceID;                 ///< the device ID (hex value stored as int)
    int revID;                    ///< the revision ID (hex value stored as int)
    unsigned int gpuIndex;        ///< GPU index in the system
#ifdef _WIN32
    std::string registryPath;     ///< Adapter registry path
    std::string registryPathExt;  ///< Adapter registry path
#endif // _WIN32
};

typedef std::vector<ADLUtil_ASICInfo> AsicInfoList;

/// Return values from the ADLUtil
enum ADLUtil_Result
{
    ADL_RESULT_NONE,                  ///< Undefined ADL result
    ADL_SUCCESS = 1,                  ///< Data was retrieved successfully.
    ADL_NOT_FOUND,                    ///< ADL DLLs were not found.
    ADL_MISSING_ENTRYPOINTS,          ///< ADL did not expose necessary entrypoints.
    ADL_INITIALIZATION_FAILED,        ///< ADL could not be initialized.
    ADL_GET_ADAPTER_COUNT_FAILED,     ///< ADL was unable to return the number of adapters.
    ADL_GET_ADAPTER_INFO_FAILED,      ///< ADL was unable to return adapter info.
    ADL_GRAPHICS_VERSIONS_GET_FAILED, ///< ADL was unable to return graphics versions info.
    ADL_WARNING,                      ///< ADL Operation succeeded, but generated a warning.
};

/// Uses ADL to obtain information about the available ASICs. This is deprecated -- use AMDTADLUtils::Instance()->GetAsicInfoList() instead.
/// @param   asicInfoList A list to populate with the available ASICs.
/// @returns              an enum ADLUtil_Result status code.
ADLUTIL_DEPRECATED ADLUtil_Result ADLUtil_GetASICInfo(AsicInfoList& asicInfoList);

/// Uses ADL to obtain version information about installed drivers. This is deprecated -- use AMDTADLUtils::Instance()->GetADLVersionsInfo() instead
/// @param   info The version information.
/// @returns      an enum ADLUtil_Result status code.
ADLUTIL_DEPRECATED ADLUtil_Result ADLUtil_GetVersionsInfo(struct ADLVersionsInfo& info);

// Typedefs of the ADL function pointers. If additional entry points are needed, add them here
typedef int(*ADL_Main_Control_Create_fn)(ADL_MAIN_MALLOC_CALLBACK, int);
typedef int(*ADL_Main_Control_Destroy_fn)();
typedef int(*ADL2_Main_Control_Create_fn)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
typedef int(*ADL2_Main_Control_Destroy_fn)(ADL_CONTEXT_HANDLE);

typedef int(*ADL_Adapter_NumberOfAdapters_Get_fn)(int*);
typedef int(*ADL_Adapter_AdapterInfo_Get_fn)(LPAdapterInfo, int);
typedef int(*ADL2_Adapter_NumberOfAdapters_Get_fn)(ADL_CONTEXT_HANDLE, int*);
typedef int(*ADL2_Adapter_AdapterInfo_Get_fn)(ADL_CONTEXT_HANDLE, LPAdapterInfo, int);

typedef int(*ADL_Graphics_Versions_Get_fn)(struct ADLVersionsInfo*);
typedef int(*ADL2_Graphics_Versions_Get_fn)(ADL_CONTEXT_HANDLE, struct ADLVersionsInfo*);

// Table of ADL entry points. If additional entry points are needed, add them to this table and the code will automatically initialize them
#define ADL_INTERFACE_TABLE              \
    X(ADL_Main_Control_Create)           \
    X(ADL_Main_Control_Destroy)          \
    X(ADL2_Main_Control_Create)          \
    X(ADL2_Main_Control_Destroy)         \
    X(ADL_Adapter_NumberOfAdapters_Get)  \
    X(ADL_Adapter_AdapterInfo_Get)       \
    X(ADL2_Adapter_NumberOfAdapters_Get) \
    X(ADL2_Adapter_AdapterInfo_Get)      \
    X(ADL_Graphics_Versions_Get)         \
    X(ADL2_Graphics_Versions_Get)

//------------------------------------------------------------------------------------
/// Singleton class to provide caching of values returned by various ADLUtil functions
//------------------------------------------------------------------------------------
class AMDTADLUtils : public TSingleton<AMDTADLUtils>
{
    /// TSingleton needs to be able to use our constructor.
    friend class TSingleton<AMDTADLUtils>;

public:
    /// Loads ADL library, initializes the function entry points, and calls ADL2_Main_Control_Create
    ADLUtil_Result LoadAndInit();

    /// Calls ADL2_Main_Control_Destroy, unloads ADL library, and clears the function entry points
    ADLUtil_Result Unload();

    /// Get the AsicInfoList from ADL. The value is cached so that multiple calls don't need to requery ADL
    /// @param[out] asicInfoList the AsicInfoList from ADL
    /// @returns    an enum ADLUtil_Result status code.
    ADLUtil_Result GetAsicInfoList(AsicInfoList& asicInfoList);

    /// Get the Catalyst version info from ADL. The value is cached so that multiple calls don't need to requery ADL
    /// @param[out] adlVersionInfo the Catalyst version info from ADL
    /// @returns    an enum ADLUtil_Result status code.
    ADLUtil_Result GetADLVersionsInfo(ADLVersionsInfo& adlVersionInfo);

    /// Gets the major, minor and subminor number of the driver version. For
    /// instance, if the driver version string is 14.10.1005-140115n-021649E-ATI,
    /// the major number is "14", the minor is "10", and the sub-minor is "1005".
    /// If minor or subminor do not exist in the driver string, 0 will be returned for those values.
    /// @param[out] majorVer the major version number
    /// @param[out] minorVer the minor version number
    /// @param[out] subMinorVer the sub-minor version number
    /// @return an enum ADLUtil_Result status code.
    ADLUtil_Result GetDriverVersion(unsigned int& majorVer, unsigned int& minorVer, unsigned int& subMinorVer) const;

    /// Resets the singleton data so that the next call with requery the data rather than using any cached data
    void Reset();

private:
    /// constructor
    AMDTADLUtils();

    /// destructor
    ~AMDTADLUtils();

    ADLModule          m_libHandle;          ///< Handle to ADL Module
    ADL_CONTEXT_HANDLE m_adlContext;         ///< ADL Context for use with ADL2 functions
    std::mutex         m_asicInfoMutex;      ///< Mutex to protect access to the m_asicInfoList
    std::mutex         m_adlVersionsMutex;   ///< Mutex to protect access to the m_adlVersionsInfo
    AsicInfoList       m_asicInfoList;       ///< the ADL ASIC list
    ADLVersionsInfo    m_adlVersionsInfo;    ///< the ADL Version info

    ADLUtil_Result     m_asicInfoListRetVal; ///< the result of the AsicInfoList query
    ADLUtil_Result     m_versionRetVal;      ///< the result of the ADL Version query

#define X(SYM) SYM##_fn m_##SYM; ///< ADL entry point
    ADL_INTERFACE_TABLE;
#undef X

};
#endif //_ADL_UTIL_H_
