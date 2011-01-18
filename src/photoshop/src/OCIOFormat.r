
// Definitions -- Required by include files.
// 
// The About box and resources are created in PIUtilities.r.
// You can easily override them, if you like.
#define plugInName "OCIO"
#define plugInCopyrightYear	"2010"
#define plugInDescription "A file format plugin which uses OIIO and OCIO"

// Definitions -- Required by other resources in this rez file.
//
#define vendorName "Sony Imageworks"
#define plugInAETEComment "A file format plugin which uses OIIO and OCIO"
#define plugInSuiteID 'sdK4'
#define plugInClassID 'simP'
#define plugInEventID typeNull // must be this

// Set up included files for Macintosh and Windows.
//
#include "PIDefines.h"
#if __PIMac__
    #include "Types.r"
    #include "SysTypes.r"
    #include "PIGeneral.r"
    #include "PIUtilities.r"
#elif defined(__PIWin__)
    #define Rez
    #include "PIGeneral.h"
    #include "PIUtilities.r"
#endif

// PiPL resource
//
resource 'PiPL' (ResourceID, plugInName " PiPL", purgeable)
{
    {
        Kind { ImageFormat },
        Name { plugInName },
        Version { (latestFormatVersion << 16) | latestFormatSubVersion },
        
        #ifdef __PIMac__
            #if (defined(__x86_64__))
                CodeMacIntel64 { "PluginMain" },
            #endif
            #if (defined(__i386__))
                CodeMacIntel32 { "PluginMain" },
            #endif
        #else
            #if defined(_WIN64)
                CodeWin64X86 { "PluginMain" },
            #else
                CodeWin32X86 { "PluginMain" },
            #endif
        #endif
        
        // ClassID, eventID, aete ID, uniqueString:
        HasTerminology { plugInClassID, 
                         plugInEventID, 
                         ResourceID, 
                         vendorName " " plugInName },
        
        SupportedModes
        {
            doesSupportBitmap,
            doesSupportGrayScale,
            doesSupportIndexedColor,
            doesSupportRGBColor,
            doesSupportCMYKColor,
            doesSupportHSLColor,
            doesSupportHSBColor,
            doesSupportMultichannel,
            doesSupportDuotone,
            doesSupportLABColor
        },
        
        // If you want your format module always enabled.	
        EnableInfo { "true" },
        
        // New for Photoshop 8, document sizes that are really big 
        // 32 bit row and columns, 2,000,000 current limit but we can handle more
        PlugInMaxSize { 2147483647, 2147483647 },
        
        // For older Photoshops that only support 30000 pixel documents,
        // 16 bit row and columns
        FormatMaxSize { { 32767, 32767 } },
        
        FormatMaxChannels { { 1, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24 } },
        
        //
        // Note: Layer Support still needs some work, it just doesn't work as
        // advertised for 16Bits
        //
        //FormatLayerSupport { doesSupportFormatLayers },
        
        //FmtFileType { 'OIIO', '8BIM' },
        //ReadTypes { { '8B1F', '    ' } },
        //FilteredTypes { { '8B1F', '    ' } },
        
        ReadExtensions { {
            'TIF ',
            'TIFF',
            'EXR ',
            'DPX ',
            'CIN ',
            'JPG ',
            'JPEG',
            'ICO ',
            'BMP ',
            'PNG ',
            //
            'TEX ', // Mipmap Textures
            'TDL ', // 3Delight Textures
        } },
        
        //WriteExtensions { {
        //    'TIF ',
        //    'EXR ',
        //    'ICO ',
        //    'TDL '
        //} },
        
        //FilteredExtensions { {
        //    'TIF ',
        //    'EXR ',
        //    'ICO ',
        //    'TDL '
        //} },
        
        // we always want to use OCIO over inbuilt PS formats and other
        // FormatPlugins
        Priority { 100 },
        
        FormatFlags { fmtSavesImageResources, 
		              fmtCanRead, 
					  fmtCanWrite, 
					  fmtCanWriteIfRead, 
					  fmtCanWriteTransparency, 
					  fmtCanCreateThumbnail },
        
        FormatICCFlags { iccCannotEmbedGray,
                         iccCanEmbedIndexed,
                         iccCannotEmbedRGB,
                         iccCanEmbedCMYK }
    }
};
