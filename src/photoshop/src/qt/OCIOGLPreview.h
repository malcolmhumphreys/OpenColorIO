/*
 */

#ifndef OCIOGLPREVIEW_H
#define OCIOGLPREVIEW_H

#include "OCIOFormat.h"

class OCIOGLPreview : public QGLWidget
{
public:
    
    OCIOGLPreview(QWidget *parent);
    ~OCIOGLPreview();
    
    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();
    
    void allocate3DLutTexture();
    void buildGLTexture();
    void syncGLState();
    
    bool setOutputRole(RoleType rtype);
    bool setInputRole(RoleType rtype);
    
    OIIO::ImageSpec* m_spec;
    float* m_data;
    
private:
    
    OCIO::ConstConfigRcPtr m_config;
    RoleType m_inputRole;
    RoleType m_outputRole;
    
    int m_winWidth;
    int m_winHeight;
    
    GLuint m_imageTexID;
    float m_imageAspect;
    
    GLuint m_lut3dTexID;
    std::vector<float> m_lut3d;
    std::string m_lut3dcacheid;
    
    GLuint m_fragShader;
    GLuint m_program;
    
};

#endif // OCIOGLPREVIEW_H
