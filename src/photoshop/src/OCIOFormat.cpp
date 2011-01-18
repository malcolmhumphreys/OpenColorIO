
#include <iostream>
#include <iterator>

// Photoshop SDK
#include <PIUI.h>
#include <PIGetPathSuite.h>
#include <SPPlugs.h>

#ifdef __APPLE__
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#endif

// PITypes.h defines this which clashes with huge() in fmath
#undef huge

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

#include "OCIOFormat.h"

#define FILEPATH_LEN_MAX 500

///////////////////////////////////////////////////////////////////////////////
// Globals
//

FormatRecord* gFormatRecord = NULL;
SPPluginRef gPluginRef = NULL;
int16* gResult = NULL;
intptr_t* gDataHandle = NULL;
Data* gData = NULL;
SPBasicSuite* sSPBasic = NULL;

///////////////////////////////////////////////////////////////////////////////
//
//

std::string RoleTypeStr(RoleType rtype)
{
    std::string stype("unknown");
    if(rtype == kRole_display)
        stype = "display";
    else if(rtype == kRole_scene_linear)
        stype = OCIO::ROLE_SCENE_LINEAR;
    else if(rtype == kRole_compositing_log)
        stype = OCIO::ROLE_COMPOSITING_LOG;
    else if(rtype == kRole_data)
        stype = OCIO::ROLE_DATA;
    else if(rtype == kRole_matte_paint)
        stype = OCIO::ROLE_MATTE_PAINT;
    else if(rtype == kRole_texture_paint)
        stype = OCIO::ROLE_TEXTURE_PAINT;
    return stype;
}

// print a std::vector to std::out
template<class T>
std::ostream& operator<< (std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(std::cout, " "));
    return os;
}

static void ConvertHFSPath(char *filepath)
{
#ifdef __APPLE__
    // Convert from HFS path to POSIX path
    CFStringRef cf_filepath_hfs = CFStringCreateWithCString(NULL, filepath, kCFStringEncodingUTF8);
    CFURLRef cf_url = CFURLCreateWithFileSystemPath(NULL, cf_filepath_hfs, kCFURLHFSPathStyle, false);
    CFRelease(cf_filepath_hfs);
    CFStringRef cf_filepath_posix = CFURLCopyFileSystemPath(cf_url, kCFURLPOSIXPathStyle);
    CFRelease(cf_url);
    CFStringGetCString(cf_filepath_posix, filepath, FILEPATH_LEN_MAX, kCFStringEncodingUTF8);
    CFRelease(cf_filepath_posix);
#else
    // do nothing on windows
#endif
}

static VPoint GetFormatImageSize(void)
{
    VPoint returnPoint = {0, 0};
    if(gFormatRecord->HostSupports32BitCoordinates &&
       gFormatRecord->PluginUsing32BitCoordinates)
    {
        returnPoint.v = gFormatRecord->imageSize32.v;
        returnPoint.h = gFormatRecord->imageSize32.h;
    }
    else
    {
        returnPoint.v = gFormatRecord->imageSize.v;
        returnPoint.h = gFormatRecord->imageSize.h;
    }
    return returnPoint;
}

static void SetFormatImageSize(VPoint inPoint)
{
    if (gFormatRecord->HostSupports32BitCoordinates &&
        gFormatRecord->PluginUsing32BitCoordinates)
    {
        gFormatRecord->imageSize32.v = inPoint.v;
        gFormatRecord->imageSize32.h = inPoint.h;
    }
    else
    {
        gFormatRecord->imageSize.v = static_cast<int16>(inPoint.v);
        gFormatRecord->imageSize.h = static_cast<int16>(inPoint.h);
    }
}

static void SetFormatTheRect(VRect inRect)
{
    if (gFormatRecord->HostSupports32BitCoordinates &&
        gFormatRecord->PluginUsing32BitCoordinates)
    {
        gFormatRecord->theRect32.top = inRect.top;
        gFormatRecord->theRect32.left = inRect.left;
        gFormatRecord->theRect32.bottom = inRect.bottom;
        gFormatRecord->theRect32.right = inRect.right;
    }
    else
    {
        gFormatRecord->theRect.top = static_cast<int16>(inRect.top);
        gFormatRecord->theRect.left = static_cast<int16>(inRect.left);
        gFormatRecord->theRect.bottom = static_cast<int16>(inRect.bottom);
        gFormatRecord->theRect.right = static_cast<int16>(inRect.right);
    }
}

bool PSProgressCallback(void *opaque_data, float portion_done)
{
    gFormatRecord->progressProc(portion_done, 1.2);
    return false;
}

///////////////////////////////////////////////////////////////////////////////
//
//

// Create a handle to our Data structure. Photoshop will take ownership of this
// handle and delete it when necessary.
static void CreateDataHandle()
{
    Handle h = sPSHandle->New(sizeof(Data));
    if (h != NULL) *gDataHandle = reinterpret_cast<intptr_t>(h);
    else *gResult = memFullErr;
}

// Initalize any global values here. Called only once when global space is
// reserved for the first time.
static void InitData(void)
{
    gData->subimage = 0;
    gData->mipmap = 0;
}

// Lock the handles and get the pointers for gData and gParams. Set the global
// error, *gResult, if there is trouble
static void LockHandles(void)
{
    if (!(*gDataHandle))
    {
        *gResult = formatBadParameters;
        return;
    }
    sPSHandle->SetLock(reinterpret_cast<Handle>(*gDataHandle), true, 
                       reinterpret_cast<Ptr *>(&gData), NULL);
    if (gData == NULL)
    {
        *gResult = memFullErr;
        return;
    }
}

// Unlock the handles used by the data and params pointers
static void UnlockHandles(void)
{
    if (*gDataHandle)
        sPSHandle->SetLock(reinterpret_cast<Handle>(*gDataHandle), false, 
                           reinterpret_cast<Ptr *>(&gData), FALSE);
}

// empty on OSX
void DoAbout(SPPluginRef plugin, int dialogID)
{
#ifdef __DEBUG
    std::cout << "OCIOFormat: DoAbout() called" << std::endl;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Reading
//

static void DoReadPrepare(void)
{
#ifdef __DEBUG
    std::cout << "OCIOFormat: DoReadPrepare() called" << std::endl;
#endif
    gFormatRecord->maxData = 0;
    return;
}

static void DoReadStart(void)
{
#ifdef __DEBUG
    std::cout << "OCIOFormat: DoReadStart() called" << std::endl;
#endif
    
    PSGetPathSuite1 *sPSGetPath = NULL;
    *gResult = sSPBasic->AcquireSuite(kPSGetPathSuite,
                                      kPSGetPathSuiteVersion1, 	  
                                      (const void**)&sPSGetPath);
    
    char filepath[FILEPATH_LEN_MAX];
    sPSGetPath->GetPathName((SPPlatformFileSpecification*)gFormatRecord->fileSpec2,
                            filepath, FILEPATH_LEN_MAX);
    ConvertHFSPath(filepath);
    
    // Find an ImageIO plugin that can open the input file, and open it.
    gData->input = OIIO::ImageInput::create(filepath, "");
    if(!gData->input)
    {
#ifdef __DEBUG
        std::cerr << "OIIOFormatPlugin: couldn't find plugin to open file" << "\n";
        std::cerr << "OIIOFormatPlugin: " << filepath << " : " << OpenImageIO::geterror() << "\n";
#endif
        *gResult = formatCannotRead;
        return;
    }
    
    // Open the file and fill out the spec
    OIIO::ImageSpec spec;
    if (!gData->input->open(filepath, spec))
    {
#ifdef __DEBUG
        std::cerr << "OIIOFormatPlugin: Could not open " << filepath << " : " << gData->input->geterror() << "\n";
#endif
        delete gData->input;
        *gResult = formatCannotRead;
        return;
    }
    
    // Set the image size
    gData->subimage = 0; // TODO: force this to the largest subimage
    gData->mipmap = 0;   // TODO: force this to the largest mipmap
    VPoint imageSize;
    imageSize.v = spec.height;
    imageSize.h = spec.width;
    SetFormatImageSize(imageSize);
    
    // The number of channels in the image. The plug-in should set this field
    // in the formatSelectorReadStart handler.
    gFormatRecord->planes = spec.nchannels;
    
    // Index of the plane containing transparency information
    gFormatRecord->transparencyPlane = spec.alpha_channel;
    
    // The mode of the image being imported (grayscale, RGB Color, and so on).
    // The plug-in should set this field during the formatSelectorReadStart
    // handler.
    gFormatRecord->imageMode = plugInModeRGBColor;
    if(spec.nchannels == 1) gFormatRecord->imageMode = plugInModeGrayScale;
    
    //////////////////////
    
    gData->input->seek_subimage(gData->subimage, gData->mipmap, spec);
    
    // Read into the oiio buffer
    gData->oiiodata = new float [spec.width * spec.height * spec.nchannels];
    gData->input->read_image(OIIO::TypeDesc::FLOAT, gData->oiiodata,
                             OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride,
                             &PSProgressCallback);
    
    // Get the BitsPerSample
    int BitsPerSample = gData->input->spec().get_int_attribute("BitsPerSample", -1);
    
    //
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
    OCIO::ConstProcessorRcPtr processor;
    
    //
    RoleType intputRole(kRole_unknown);
    if(gData->input->spec().format == OIIO::TypeDesc::HALF ||
       gData->input->spec().format == OIIO::TypeDesc::FLOAT)
        intputRole = kRole_scene_linear;
    else if(BitsPerSample == 10)
        intputRole = kRole_compositing_log;
    else
        intputRole = kRole_data;
    
    //
    RoleType outputRole(kRole_unknown);
    if(config->getRoleColorSpaceName(OCIO::ROLE_MATTE_PAINT) != NULL)
        outputRole = kRole_matte_paint;
    else if(config->getRoleColorSpaceName(OCIO::ROLE_TEXTURE_PAINT) != NULL)
        outputRole = kRole_texture_paint;
    else if(config->getRoleColorSpaceName(OCIO::ROLE_COMPOSITING_LOG) != NULL)
        outputRole = kRole_compositing_log;
    
    //
    OIIO::ImageSpec* spec_ptr = &spec;
    *gResult = DoImportOptions(spec_ptr, gData->oiiodata, &intputRole, &outputRole);
    if(*gResult != noErr) return;
    
    //
    std::string intput_role;
    if(intputRole == kRole_scene_linear)
        intput_role = OCIO::ROLE_SCENE_LINEAR;
    else if(intputRole == kRole_compositing_log)
        intput_role = OCIO::ROLE_COMPOSITING_LOG;
    else if(intputRole == kRole_data)
        intput_role = OCIO::ROLE_DATA;
    else if(intputRole == kRole_matte_paint)
        intput_role = OCIO::ROLE_MATTE_PAINT;
    else if(intputRole == kRole_texture_paint)
        intput_role = OCIO::ROLE_TEXTURE_PAINT;
    
    //
    std::string output_role;
    if(outputRole == kRole_scene_linear)
        output_role = OCIO::ROLE_SCENE_LINEAR;
    else if(outputRole == kRole_compositing_log)
        output_role = OCIO::ROLE_COMPOSITING_LOG;
    else if(outputRole == kRole_data)
        output_role = OCIO::ROLE_DATA;
    else if(outputRole == kRole_matte_paint)
        output_role = OCIO::ROLE_MATTE_PAINT;
    else if(outputRole == kRole_texture_paint)
        output_role = OCIO::ROLE_TEXTURE_PAINT;
    
    //
    if(outputRole == kRole_display)
    {
        OCIO::DisplayTransformRcPtr trans = OCIO::DisplayTransform::Create();
        
        trans->setInputColorSpaceName(intput_role.c_str());
        
        const char * device = config->getDefaultDisplayDeviceName();
        const char * transformName = config->getDefaultDisplayTransformName(device);
        const char * displayColorSpace = config->getDisplayColorSpaceName(device, transformName);
        trans->setDisplayColorSpaceName(displayColorSpace);
        
        processor = config->getProcessor(trans);
    }
    else
    {
        processor = config->getProcessor(intput_role.c_str(), output_role.c_str());
    }
    
    OCIO::PackedImageDesc img(gData->oiiodata, spec.width, spec.height,
                              gFormatRecord->planes);
    processor->apply(img);
    
    // The resolution of the image in bits per pixel per plane. The plug-in
    // should set this field in the formatSelectorReadStart handler.
    // Valid settings are 1, 8, 16, and 32.
    //
    // TODO: this need to be smarter on choosing depth based on the file being
    // selected, for now we force this to 16bit till we have this in place.
    gFormatRecord->depth = 16;
    
    *gResult = noErr;
    return;
}

/*
static void DoReadLayerStart(void)
{
#ifdef __DEBUG
    std::cout << "OCIOFormat: DoReadLayerStart() called" << std::endl;
#endif
    // set the next subimage to seek to
    gData->subimage = gFormatRecord->layerData;
    
    return;
}
*/

static bool hasRole(OCIO::ConstConfigRcPtr config, const char * role)
{
    if(role == NULL) return false;
    for(int i = 0; i < config->getNumRoles(); i++)
        if(config->getRoleName(i) == role) return true;
    return false;
}

static void DoReadContinue(void)
{
#ifdef __DEBUG
    std::cout << "OIIOFormatPlugin: DoReadContinue() called" << std::endl;
#endif
    
    // TODO: make this cleaner
    OIIO::ImageSpec spec;
    gData->input->seek_subimage(gData->subimage, gData->mipmap, spec);
    
    // setup the read format based on ps depth
    switch (gFormatRecord->depth) {
        case 32: spec.set_format(OIIO::TypeDesc::FLOAT);   break;
        case 16: spec.set_format(OIIO::TypeDesc::UINT16);  break;
        case  8: spec.set_format(OIIO::TypeDesc::UINT8);   break;
        default: spec.set_format(OIIO::TypeDesc::UNKNOWN); break;
    }
    
    // Allocate a buffer for the entire image
    unsigned32 bufferSize = spec.image_bytes();
    Ptr psbuffer = sPSBuffer->New(&bufferSize, bufferSize);
    if (psbuffer == NULL)
    {
        *gResult = memFullErr;
        return;
    }
    gFormatRecord->data = psbuffer;
    
    // The first plane covered by the buffer specified in data. The start and
    // continue selector handlers should set this field.
    gFormatRecord->loPlane = 0;
    
    // The first and last planes covered by the buffer specified in data. The
    // start and continue selector handlers should set this field.
    gFormatRecord->hiPlane = gFormatRecord->planes - 1;
    
    // The offset in bytes between columns of data in the buffer. The start and
    // continue selector handlers should set this field. 
    gFormatRecord->colBytes = spec.pixel_bytes();
    
    // The offset in bytes between rows of data in the buffer. The start and
    // continue selector handlers should set this field. 
    gFormatRecord->rowBytes = spec.scanline_bytes();
    
    // The offset in bytes between planes of data in the buffers. The start and
    // continue selector handlers should set this field. 
    // set to 1 for 8bit interleaved data
    gFormatRecord->planeBytes = spec.channel_bytes();
    
    // update the progress
    gFormatRecord->progressProc(1.1, 1.2);
    
    // Copy the data into the PS buffer
    OIIO::ColorTransfer *tfunc = OIIO::ColorTransfer::create("null");
    OIIO::convert_types(OIIO::TypeDesc::FLOAT, gData->oiiodata, spec.format,
                        psbuffer, (spec.width * spec.height * spec.nchannels),
                        tfunc, spec.alpha_channel, spec.z_channel);
    
    // setup theRect
    VRect theRect;
	theRect.left = 0;
	theRect.right = spec.width;
    theRect.top = 0;
    theRect.bottom = spec.height;
    SetFormatTheRect(theRect);
    
    // update PS
    if (*gResult == noErr)
        *gResult = gFormatRecord->advanceState();
    
    // update the progress
    gFormatRecord->progressProc(1.2, 1.2);
    
    // free buffer
    sPSBuffer->Dispose(&psbuffer);
    gFormatRecord->data = NULL;
    
    return;
}

/*
void DoReadLayerContinue(void)
{
#ifdef __DEBUG
    std::cout << "OIIOFormatPlugin: DoReadLayerContinue() called" << std::endl;
#endif
    DoReadContinue();
    return;
}
*/

static void DoReadFinish(void)
{
#ifdef __DEBUG
    std::cout << "OIIOFormatPlugin: DoReadFinish() called" << std::endl;
#endif
    free(gData->oiiodata);
    return;
}

/*
void DoReadLayerFinish(void)
{
#ifdef __DEBUG
    std::cout << "OIIOFormatPlugin: DoReadLayerFinish() called" << std::endl;
#endif
    return;
}
*/

///////////////////////////////////////////////////////////////////////////////
// PluginMain
//
// All calls to the plug-in module come through this routine

DLLExport MACPASCAL void PluginMain(const int16 selector,
                                    FormatRecordPtr formatParamBlock,
                                    intptr_t* data, int16* result)
{    
    // Update our global parameters from args
    gFormatRecord = reinterpret_cast<FormatRecordPtr>(formatParamBlock);
    gPluginRef = reinterpret_cast<SPPluginRef>(gFormatRecord->plugInRef);
    gResult = result;
    gDataHandle = data;
    
    // Check for about box request.
    if (selector == formatSelectorAbout)
    {
        AboutRecordPtr aboutRecord = reinterpret_cast<AboutRecordPtr>(formatParamBlock);
        sSPBasic = aboutRecord->sSPBasic;
        gPluginRef = reinterpret_cast<SPPluginRef>(aboutRecord->plugInRef);
        DoAbout(gPluginRef, AboutID);
    }
    else
    {
        sSPBasic = ((FormatRecordPtr)formatParamBlock)->sSPBasic;
        
        if (gFormatRecord->resourceProcs->countProc == NULL ||
            gFormatRecord->resourceProcs->getProc   == NULL ||
            gFormatRecord->resourceProcs->addProc   == NULL ||
            gFormatRecord->advanceState             == NULL)
        {
            *gResult = errPlugInHostInsufficient;
            return;
        }
        
        // new for Photoshop 8, big documents, rows and columns are now > 30000 pixels
        if (gFormatRecord->HostSupports32BitCoordinates)
            gFormatRecord->PluginUsing32BitCoordinates = true;
        
        // Allocate and initalize globals.
        if(!(*gDataHandle))
        {
            CreateDataHandle();
            if (*gResult != noErr) return;
            LockHandles();
            if (*gResult != noErr) return;
            InitData();
        }
        
        if (*gResult == noErr)
        {
            LockHandles();
            if (*gResult != noErr) return;
        }
        
        // Dispatch selector
        switch (selector)
        {
            //// Reading ////
            case formatSelectorReadPrepare:       DoReadPrepare();       break;
            case formatSelectorReadStart:         DoReadStart();         break;
            case formatSelectorReadContinue:      DoReadContinue();      break;
            case formatSelectorReadFinish:        DoReadFinish();        break;
            // TODO: Layer Support 
            //case formatSelectorReadLayerStart:    DoReadLayerStart();    break;
            //case formatSelectorReadLayerContinue: DoReadLayerContinue(); break;                
            //case formatSelectorReadLayerFinish:   DoReadLayerFinish();   break;
            
            //// Writing ////
            
                
            //
#ifdef __DEBUG
            default:
                std::cout << "OCIOFormat: format selector: [" << selector << "]" << std::endl;
#endif
        }
        
        // Unlock data, and exit resource.
        UnlockHandles();
    }
    
    // release any suites that we may have acquired
    if (selector == formatSelectorAbout ||
        selector == formatSelectorWriteFinish ||
        selector == formatSelectorReadFinish ||
        selector == formatSelectorOptionsFinish ||
        selector == formatSelectorEstimateFinish ||
        selector == formatSelectorFilterFile ||
        *gResult != noErr) PIUSuitesRelease();
    return;
} // end PluginMain
