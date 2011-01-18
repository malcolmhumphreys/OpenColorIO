/*
 */

#include <iostream>

#include "OCIOFormat.h"
#include "OCIOImportDialog.h"
#include "OCIOGLPreview.h"

#define MENU_SCENE_LINEAR    "Scene Linear"
#define MENU_COMPOSITING_LOG "Compositing Log"
#define MENU_MATTE_PAINT     "Matte Paint"
#define MENU_TEXTURE_PAINT   "Texture Paint"
#define MENU_DATA            "Data"
#define MENU_DISPLAY         "Display"

OCIOImportDialog::OCIOImportDialog(QWidget *parent, OIIO::ImageSpec* spec,
                                   float* data, RoleType* in_inputRole,
                                   RoleType* in_outputRole) :
    QDialog(parent), m_init_gl(false)
{
    // Setup the UI
    setupUi(this);
    
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
    
    // Add supported roles inputRole widget
    QStringList inputTypes;
    for(int i = 0; i < config->getNumRoles(); i++) {
        std::string name(config->getRoleName(i));
             if(name == OCIO::ROLE_SCENE_LINEAR)
            inputTypes << MENU_SCENE_LINEAR;
        else if(name == OCIO::ROLE_COMPOSITING_LOG)
            inputTypes << MENU_COMPOSITING_LOG;
        else if(name == OCIO::ROLE_MATTE_PAINT)
            inputTypes << MENU_MATTE_PAINT;
        else if(name == OCIO::ROLE_TEXTURE_PAINT)
            inputTypes << MENU_TEXTURE_PAINT;
        else if(name == OCIO::ROLE_DATA)
            inputTypes << MENU_DATA;
    }
    inputRole->clear();
    inputRole->insertItems(0, inputTypes);
    
    // Add items to importRole widget
    QStringList convertTypes;
    for(int i = 0; i < config->getNumRoles(); i++) {
        std::string name(config->getRoleName(i));
        if(name == OCIO::ROLE_SCENE_LINEAR)
            convertTypes << MENU_SCENE_LINEAR;
        else if(name == OCIO::ROLE_COMPOSITING_LOG)
            convertTypes << MENU_COMPOSITING_LOG;
        else if(name == OCIO::ROLE_MATTE_PAINT)
            convertTypes << MENU_MATTE_PAINT;
        else if(name == OCIO::ROLE_TEXTURE_PAINT)
            convertTypes << MENU_TEXTURE_PAINT;
        else if(name == OCIO::ROLE_DATA)
            convertTypes << MENU_DATA;
    }
    convertTypes << MENU_DISPLAY;
    outputRole->clear();
    outputRole->insertItems(0, convertTypes);
    
    // Give the GLPreview the image ptrs
    GLPreview->m_spec = spec;
    GLPreview->m_data = data;
    
    // set the inital roles
    m_init_gl = true;
    setInputRole(*in_inputRole);
    setOutputRole(*in_outputRole);
}

QString RoleTypeMenu(RoleType rtype)
{
    QString stype("unknown");
    if(rtype == kRole_display)
        stype = MENU_DISPLAY;
    else if(rtype == kRole_scene_linear)
        stype = MENU_SCENE_LINEAR;
    else if(rtype == kRole_compositing_log)
        stype = MENU_COMPOSITING_LOG;
    else if(rtype == kRole_data)
        stype = MENU_DATA;
    else if(rtype == kRole_matte_paint)
        stype = MENU_MATTE_PAINT;
    else if(rtype == kRole_texture_paint)
        stype = MENU_TEXTURE_PAINT;
    return stype;
}

RoleType RoleMenuType(QString mtype)
{
    RoleType rtype = kRole_unknown;
    if(mtype == MENU_SCENE_LINEAR)         rtype = kRole_scene_linear;
    else if(mtype == MENU_COMPOSITING_LOG) rtype = kRole_compositing_log;
    else if(mtype == MENU_MATTE_PAINT)     rtype = kRole_matte_paint;
    else if(mtype == MENU_TEXTURE_PAINT)   rtype = kRole_texture_paint;
    else if(mtype == MENU_DATA)            rtype = kRole_data;
    else if(mtype == MENU_DISPLAY)         rtype = kRole_display;
    return rtype;
}

void OCIOImportDialog::setInputRole(RoleType rtype)
{
    int index = inputRole->findText(RoleTypeMenu(rtype));
    inputRole->setCurrentIndex(index);
}

void OCIOImportDialog::setOutputRole(RoleType rtype)
{
    int index = outputRole->findText(RoleTypeMenu(rtype));
    outputRole->setCurrentIndex(index);
}


void OCIOImportDialog::on_inputRole_currentIndexChanged(QString item)
{
    if(!m_init_gl) return;
    
    RoleType rtype = RoleMenuType(item);
    
    // Try and set the output role
    if(!GLPreview->setInputRole(rtype))
    {
        // if we can't reset to last role selected
        setInputRole(m_outputRole);
        return;
    }
    m_inputRole = rtype;
    
    return;
}

void OCIOImportDialog::on_outputRole_currentIndexChanged(QString item)
{
    if(!m_init_gl) return;
    
    RoleType rtype = RoleMenuType(item);
    
    // Try and set the output role
    if(!GLPreview->setOutputRole(rtype))
    {
        // if we can't reset to last role selected
        setOutputRole(m_inputRole);
        return;
    }
    m_outputRole = rtype;
    
    return;
}
