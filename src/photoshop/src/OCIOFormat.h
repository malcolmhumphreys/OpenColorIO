/*
 */

#ifndef OCIOFORMAT_H
#define OCIOFORMAT_H

#include <QtOpenGL/QtOpenGL>

// Photoshop SDK
#include <PIDefines.h>
#include <PIFormat.h>

// OpenImageIO
#include <OpenImageIO/imageio.h>
namespace OIIO = OIIO_NAMESPACE;

// OpenColorIO
#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

///////////////////////////////////////////////////////////////////////////////
//
//

enum RoleType {
    kRole_unknown = 0,
    kRole_display,
    kRole_scene_linear,
    kRole_compositing_log,
    kRole_data,
    kRole_matte_paint,
    kRole_texture_paint,
};

typedef struct Data
{
    OIIO::ImageInput *input;
    int subimage;
    int mipmap;
    float *oiiodata;
} Data;

extern FormatRecord* gFormatRecord;
extern SPPluginRef gPluginRef;
extern int16* gResult;
extern intptr_t* gDataHandle;
extern Data* gData;

///////////////////////////////////////////////////////////////////////////////
// Plugin Entrypoint
//

DLLExport MACPASCAL void PluginMain(const int16 selector,
                                    FormatRecordPtr formatParamBlock,
                                    intptr_t* data, int16* result);

///////////////////////////////////////////////////////////////////////////////
// Import UI
//

extern int DoImportOptions(OIIO::ImageSpec* spec, float* data,
                           RoleType* intputRole, RoleType* outputRole);

///////////////////////////////////////////////////////////////////////////////
// Export UI
//

//extern bool OCIOExportUI();

///////////////////////////////////////////////////////////////////////////////
// 
//

std::string RoleTypeStr(RoleType rtype);

#endif // OCIOFORMAT_H
