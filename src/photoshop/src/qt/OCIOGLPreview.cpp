/*
 */

#include <iostream>
#include <sstream>

#include "OCIOGLPreview.h"

const int LUT3D_EDGE_SIZE = 32;

const char * g_fragShaderText = ""
"#extension GL_EXT_gpu_shader4 : enable\n"
"#extension GL_ARB_texture_rectangle : enable\n"
"\n"
"uniform sampler2D tex1;\n"
"uniform sampler3D tex2;\n"
"\n"
"void main()\n"
"{\n"
"    vec4 col = texture2D(tex1, gl_TexCoord[0].st);\n"
"    gl_FragColor = OCIODisplay(col, tex2);\n"
"}\n";

GLuint CompileShaderText(GLenum shaderType, const char *text)
{
    GLuint shader;
    GLint stat;
    shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, (const GLchar **) &text, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &stat);
    if (!stat)
    {
        GLchar log[1000];
        GLsizei len;
        glGetShaderInfoLog(shader, 1000, &len, log);
        std::cerr << "OCIOGLPreview Error: problem compiling shader: " << log << "\n";
        return 0;
    }
    return shader;
}

GLuint LinkShaders(GLuint fragShader)
{
    if (!fragShader) return 0;
    GLuint program = glCreateProgram();
    if (fragShader) glAttachShader(program, fragShader);
    glLinkProgram(program);
    /* check link */
    {
        GLint stat;
        glGetProgramiv(program, GL_LINK_STATUS, &stat);
        if (!stat) {
            GLchar log[1000];
            GLsizei len;
            glGetProgramInfoLog(program, 1000, &len, log);
            std::cerr << "OCIOGLPreview Error: shader link error: " << log << "\n";
            return 0;
        }
    }
    return program;
}

OCIOGLPreview::OCIOGLPreview(QWidget *parent) :
    QGLWidget(QGLFormat(QGL::SampleBuffers), parent),
    m_spec(NULL),
    m_data(NULL)
{
    m_config = OCIO::GetCurrentConfig();
    m_winWidth = 0;
    m_winHeight = 0;
    m_imageAspect = 1;
    m_fragShader = 0;
    m_program = 0;
    m_inputRole = kRole_scene_linear;
    m_outputRole = kRole_display;
}

OCIOGLPreview::~OCIOGLPreview()
{
    glDeleteShader(m_fragShader);
    glDeleteProgram(m_program);
    return;
}

void OCIOGLPreview::initializeGL()
{
    allocate3DLutTexture();
    buildGLTexture();
    syncGLState();
    return;
}

void OCIOGLPreview::resizeGL(int width, int height)
{
    m_winWidth = width;
    m_winHeight = height;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width, 0.0, height, -100.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    return;
}

void OCIOGLPreview::paintGL()
{
    
    float pts[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // (x0, y0, x1, y1)
    float windowAspect = (m_winHeight == 0) ? 1.0 : (float)m_winWidth
                                                  / (float)m_winHeight;
    if(windowAspect > m_imageAspect)
    {
        float imgWidthScreenSpace = m_imageAspect * m_winHeight;
        pts[0] = m_winWidth * 0.5 - imgWidthScreenSpace * 0.5;
        pts[2] = m_winWidth * 0.5 + imgWidthScreenSpace * 0.5;
        pts[1] = 0.0f;
        pts[3] = m_winHeight;
    }
    else
    {
        float imgHeightScreenSpace = m_winWidth / m_imageAspect;
        pts[0] = 0.0f;
        pts[2] = m_winWidth;
        pts[1] = m_winHeight * 0.5 - imgHeightScreenSpace * 0.5;
        pts[3] = m_winHeight * 0.5 + imgHeightScreenSpace * 0.5;
    }
    
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.1f, 0.1f, 0.1f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(1, 1, 1);
    glPushMatrix();
    
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); // p0
    glVertex2f(pts[0], pts[1]);
    glTexCoord2f(0.0f, 0.0f); // p1
    glVertex2f(pts[0], pts[3]);
    glTexCoord2f(1.0f, 0.0f); // p2
    glVertex2f(pts[2], pts[3]);
    glTexCoord2f(1.0f, 1.0f); // p3
    glVertex2f(pts[2], pts[1]);
    glEnd();
    
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
    return;
}

void OCIOGLPreview::allocate3DLutTexture()
{
    glGenTextures(1, &m_lut3dTexID);
    
    int num3Dentries = 3 * LUT3D_EDGE_SIZE * LUT3D_EDGE_SIZE * LUT3D_EDGE_SIZE;
    m_lut3d.resize(num3Dentries);
    memset(&m_lut3d[0], 0, sizeof(float)*num3Dentries);
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, m_lut3dTexID);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F_ARB,
                 LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE,
                 0, GL_RGB,GL_FLOAT, &m_lut3d[0]);
    return;
}

void OCIOGLPreview::buildGLTexture()
{
    if(m_spec == NULL || m_data == NULL) return;
    
    glGenTextures(1, &m_imageTexID);
    GLenum format = 0;
         if(m_spec->nchannels == 4) format = GL_RGBA;
    else if(m_spec->nchannels == 3) format = GL_RGB;
    else
    {
        std::cerr << "Cannot build a GL image with " << m_spec->nchannels << " components." << std::endl;
        return;
    }
    
    m_imageAspect = (m_spec->height == 0) ? 1.f : (float)m_spec->width
                                                / (float)m_spec->height;
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_imageTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, m_spec->width,
                 m_spec->height, 0, format, GL_FLOAT, m_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    return;
}

bool OCIOGLPreview::setOutputRole(RoleType rtype)
{
    m_outputRole = rtype;
    if(m_spec != NULL && m_data != NULL)
    {
        try
        {
            syncGLState();
            updateGL();
        }
        catch(OCIO::Exception & exception)
        {
            QMessageBox::critical(this, tr("OCIO Error"), tr(exception.what()));
            return false;
        }
        catch(...)
        {
            std::cerr << "Unknown OCIO error encountered." << std::endl;
        }
    }
    return true;
}

bool OCIOGLPreview::setInputRole(RoleType rtype)
{
    m_inputRole = rtype;
    if(m_spec != NULL && m_data != NULL)
    {
        try
        {
            syncGLState();
            updateGL();
        }
        catch(OCIO::Exception & exception)
        {
            QMessageBox::critical(this, tr("OCIO Error"), tr(exception.what()));
            return false;
        }
        catch(...)
        {
            std::cerr << "Unknown OCIO error encountered." << std::endl;
        }
    }
    return true;
}

void OCIOGLPreview::syncGLState()
{
    if(m_spec == NULL || m_data == NULL) return;
    
    //std::cerr << "OCIOGLPreview::syncGLState()" << std::endl;
    
    std::string ocioShaderText = "";
    OCIO::ConstProcessorRcPtr processor;
    
    //
    std::string intput_role = RoleTypeStr(m_inputRole);
    std::string output_role = RoleTypeStr(m_outputRole);
    
    //
    if(m_outputRole == kRole_display)
    {
        OCIO::DisplayTransformRcPtr trans = OCIO::DisplayTransform::Create();
        
        trans->setInputColorSpaceName(intput_role.c_str());
        
        const char * device = m_config->getDefaultDisplayDeviceName();
        const char * transformName = m_config->getDefaultDisplayTransformName(device);
        const char * displayColorSpace = m_config->getDisplayColorSpaceName(device, transformName);
        trans->setDisplayColorSpaceName(displayColorSpace);
        
        processor = m_config->getProcessor(trans);
    }
    else
    {
        processor = m_config->getProcessor(intput_role.c_str(), output_role.c_str());
    }
    
    // Create a GPU Shader Description
    OCIO::GpuShaderDesc shaderDesc;
    shaderDesc.setLanguage(OCIO::GPU_LANGUAGE_GLSL_1_0);
    shaderDesc.setFunctionName("OCIODisplay");
    shaderDesc.setLut3DEdgeLen(LUT3D_EDGE_SIZE);
    
    // Compute the 3D LUT
    std::string lut3dCacheID = processor->getGpuLut3DCacheID(shaderDesc);
    if(lut3dCacheID != m_lut3dcacheid)
    {
        m_lut3dcacheid = lut3dCacheID;
        processor->getGpuLut3D(&m_lut3d[0], shaderDesc);
        glBindTexture(GL_TEXTURE_3D, m_lut3dTexID);
        glTexSubImage3D(GL_TEXTURE_3D, 0,
                        0, 0, 0,
                        LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE,
                        GL_RGB,GL_FLOAT, &m_lut3d[0]);
    }
        
    if(m_fragShader)
        glDeleteShader(m_fragShader);
    
    std::ostringstream os;
    os << processor->getGpuShaderText(shaderDesc) << "\n";
    os << g_fragShaderText;
        
    // Print the shader text
    //std::cerr << std::endl;
    //std::cerr << os.str() << std::endl;
        
    m_fragShader = CompileShaderText(GL_FRAGMENT_SHADER, os.str().c_str());
    
    if(m_program)
        glDeleteProgram(m_program);
    
    m_program = LinkShaders(m_fragShader);
    glUseProgram(m_program);
    
    glUniform1i(glGetUniformLocation(m_program, "tex1"), 1);
    glUniform1i(glGetUniformLocation(m_program, "tex2"), 2);
    
}
