
#include <iostream>
#include <iterator>

// Photoshop SDK
#include <PIDefines.h>
#include <PIFormat.h>
#include <PIUI.h>
#include <PIGetPathSuite.h>
#include <SPPlugs.h>

#ifdef __APPLE__
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#endif

// PITypes.h defines this which clashes with huge() in fmath
#undef huge

#include <OpenImageIO/imageio.h>
namespace OIIO = OIIO_NAMESPACE;

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

#define FILEPATH_LEN_MAX 500

// Plugin Main Entry Point
DLLExport MACPASCAL void PluginMain(const int16 selector,
                                    FormatRecordPtr formatParamBlock,
                                    intptr_t* data, int16* result);

typedef struct Data
{
    OIIO::ImageInput *input;
    int subimage;
    int mipmap;
} Data;

extern FormatRecord* gFormatRecord;
extern SPPluginRef gPluginRef;
extern int16* gResult;
extern intptr_t* gDataHandle;
extern Data* gData;

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

//!:c:function:: Create a handle to our Data structure. Photoshop will take
// ownership of this handle and delete it when necessary.
static void CreateDataHandle()
{
    Handle h = sPSHandle->New(sizeof(Data));
    if (h != NULL) *gDataHandle = reinterpret_cast<intptr_t>(h);
    else *gResult = memFullErr;
}

//!:c:function:: Initalize any global values here.   Called only once when
// global space is reserved for the first time.
static void InitData(void)
{
    gData->subimage = 0;
    gData->mipmap = 0;
}

//!:c:function:: Lock the handles and get the pointers for gData and gParams
// Set the global error, *gResult, if there is trouble
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

//!:c:function:: Unlock the handles used by the data and params pointers
static void UnlockHandles(void)
{
    if (*gDataHandle)
        sPSHandle->SetLock(reinterpret_cast<Handle>(*gDataHandle), false, 
                           reinterpret_cast<Ptr *>(&gData), FALSE);
}

//!:c:function:: empty on OSX
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
    
    //spec = new ImageSpec;
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
    
    // The resolution of the image in bits per pixel per plane. The plug-in
    // should set this field in the formatSelectorReadStart handler.
    // Valid settings are 1, 8, 16, and 32.
    //
    // TODO: this need to be smarter on choosing depth based on the file being
    // selected, for now we force this to 16bit till we have this in place.
    gFormatRecord->depth = 16;
    
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

static void DoReadContinue(void)
{
#ifdef __DEBUG
    std::cout << "OIIOFormatPlugin: DoReadContinue() called" << std::endl;
#endif
    
    OIIO::ImageSpec spec;
    gData->input->seek_subimage(gData->subimage, gData->mipmap, spec);
    
    // DEBUG //
#ifdef __DEBUG
    std::cout << "OIIOFormatPlugin: layer / subimage: " << gFormatRecord->layerData << " " << gData->subimage << "\n";
    std::cerr << "OIIOFormatPlugin: channelnames: " << spec.channelnames << "\n";
    std::cerr << "OIIOFormatPlugin: format: " << spec.format.c_str() << "\n";
    std::cerr << "OIIOFormatPlugin: nchannels: " << spec.nchannels << "\n";
    std::cerr << "OIIOFormatPlugin: width x height x depth : " << spec.width << " x " << spec.height << " x " << spec.depth << "\n";
    std::cerr << "OIIOFormatPlugin: full_xyz: " << spec.full_x << " x " << spec.full_y << " x " << spec.full_z << "\n";
    std::cerr << "OIIOFormatPlugin: full_whd: " << spec.full_width << " x " << spec.full_height << " x " << spec.full_depth << "\n";
    std::cerr << "OIIOFormatPlugin: tile_whd: " << spec.tile_width << " x " << spec.tile_height << " x " << spec.tile_depth << "\n";
#endif
    
    // get the BitsPerSample
    int BitsPerSample = gData->input->spec().get_int_attribute("BitsPerSample", -1);
    
    // setup the read format based on ps depth
    switch (gFormatRecord->depth) {
        case 32: spec.set_format(OIIO::TypeDesc::FLOAT);   break;
        case 16: spec.set_format(OIIO::TypeDesc::UINT16);  break;
        case  8: spec.set_format(OIIO::TypeDesc::UINT8);   break;
        default: spec.set_format(OIIO::TypeDesc::UNKNOWN); break;
    }
    
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
    
    // Allocate a buffer for the entire image
    unsigned32 bufferSize = spec.image_bytes();
    Ptr psbuffer = sPSBuffer->New(&bufferSize, bufferSize);
    if (psbuffer == NULL)
    {
        *gResult = memFullErr;
        return;
    }
    gFormatRecord->data = psbuffer;
    
    //////
    
    // Read into the buffer
    float *data = new float [spec.width * spec.height * spec.nchannels];
    gData->input->read_image(OIIO::TypeDesc::FLOAT, data,
                             OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride,
                             &PSProgressCallback);
    
    //
    // Start OpenColorIO //
    
    // TODO: this is just demo code, just to check we are working
    //       note. this transform is *NOT* reversable.
    
    if (gData->input->spec().format == OIIO::TypeDesc::HALF ||
        gData->input->spec().format == OIIO::TypeDesc::FLOAT ||
        BitsPerSample == 10)
    {
        
        OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
        
        const char * device = config->getDefaultDisplayDeviceName();
        const char * transformName = config->getDefaultDisplayTransformName(device);
        const char * displayColorSpace = config->getDisplayColorSpaceName(device, transformName);
        
        OCIO::DisplayTransformRcPtr trans = OCIO::DisplayTransform::Create();
        if(BitsPerSample == 10)
            trans->setInputColorSpaceName(OCIO::ROLE_COMPOSITING_LOG);
        else
            trans->setInputColorSpaceName(OCIO::ROLE_SCENE_LINEAR);
        trans->setDisplayColorSpaceName(displayColorSpace);    
        
        OCIO::ConstProcessorRcPtr processor = config->getProcessor(trans);
        OCIO::PackedImageDesc img(data, spec.width, spec.height, gFormatRecord->planes);
        processor->apply(img);
        
    }
    
    // update the progress
    gFormatRecord->progressProc(1.1, 1.2);
    
    // Copy the data into the PS buffer
    OIIO::ColorTransfer *tfunc = OIIO::ColorTransfer::create("null");
    OIIO::convert_types(OIIO::TypeDesc::FLOAT, data, spec.format, psbuffer,
                        (spec.width * spec.height * spec.nchannels),
                        tfunc, spec.alpha_channel, spec.z_channel);
    
    // End OpenColorIO //
    //
    
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
    free(data);
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
            //case formatSelectorReadLayerStart:    DoReadLayerStart();    break;
            case formatSelectorReadContinue:      DoReadContinue();      break;
            //case formatSelectorReadLayerContinue: DoReadLayerContinue(); break;
            case formatSelectorReadFinish:        DoReadFinish();        break;
            //case formatSelectorReadLayerFinish:   DoReadLayerFinish();   break;
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
