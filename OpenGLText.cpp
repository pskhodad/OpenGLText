/*
    developed by Tristan Lorach - Copyright (c) 2012 NVIDIA Corporation. All rights reserved.

    TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
    *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
    OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, NONINFRINGEMENT,IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA 
    OR ITS SUPPLIERS BE  LIABLE  FOR  ANY  DIRECT, SPECIAL,  INCIDENTAL,  INDIRECT,  OR  
    CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS 
    OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY 
    OTHER PECUNIARY LOSS) ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, 
    EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

    Please direct any questions to tlorach@nvidia.com (Tristan Lorach)
    ===================================================================================
    Example on how to use it.
    init time:
        OpenGLText             oglText;
        ...
        if(!oglText.init(font_name, canvas_width, canvas_height))
            return false;

    render time:
        oglText.beginString();
        float bbStr[2];
        char *tmpStr = "Hello world";
        oglText.stringSize(tmpStr, bbStr);
        oglText.drawString(posX - bbStr[0]*0.5, posY - bbStr[1], tmpStr, 0,0xF0F0F0F0);
        ...
        oglText.endString(); // will render the whole at once
*/
#define USE_INSTANCED_ARRAYS
#define USEFONTMETRICASUBO // I have a bug, here...
#define NV_REPORT_COMPILE_ERRORS

#pragma warning(disable:4244) // dble to float conversion warning

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#ifdef WIN32
#include <direct.h>
#define MKDIR(file) _mkdir((file))
#else
#include <sys/stat.h>
#define MKDIR(file) mkdir((file),0777)
#endif
#include <errno.h>
#include <math.h>

#include <math.h>

#if defined(__APPLE__)
    #include <GLUT/glut.h>
#else
    #include <GL/glew.h>
#endif

#include "OpenGLText.h"
#include "tga.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

///////////////////////////////////////////////////////////////////////////////////////
// Backup only the states that we really need to setup
// the rendering. We might want to back them up once
//
struct TextBackupState
{
    struct vtxAttribData {
        GLvoid* ptr;
#ifdef USE_INSTANCED_ARRAYS
        int     divisor;
#endif
        int     enabled;
        int     size;
        int     type;
        int     normalized;
        int     stride;
        int     bufferBinding;
    };
    TextBackupState() { valid = false; }
    ~TextBackupState() { if(valid) {
        free(attribs);
    } }
    void backup()
    {
        glGetIntegerv(GL_POLYGON_MODE, (GLint*)mode);
        cull = glIsEnabled(GL_CULL_FACE);
        //glGetIntegerv(GL_CULL_FACE_MODE, (GLint*)&);
        stenciltest = glIsEnabled(GL_STENCIL_TEST);
        glGetIntegerv(GL_STENCIL_VALUE_MASK, (GLint*)&stencilmask);
        depthtest = glIsEnabled(GL_DEPTH_TEST);
        glGetIntegerv(GL_DEPTH_WRITEMASK, (GLint*)&depthmask);
        blend = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_BLEND_SRC, (GLint*)&sfactor);
        glGetIntegerv(GL_BLEND_DST, (GLint*)&dfactor);
        glGetIntegerv(GL_COLOR_WRITEMASK, (GLint*)&red);
        primrestart = glIsEnabled(GL_PRIMITIVE_RESTART);
        glGetIntegerv(GL_PRIMITIVE_RESTART_INDEX, (GLint*)&primrestartid);
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVtxAttribs);
        attribs = (vtxAttribData*)malloc(sizeof(vtxAttribData)*maxVtxAttribs);
        for(int i=0; i<maxVtxAttribs; i++)
        {
#ifdef USE_INSTANCED_ARRAYS
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &(attribs[i].divisor));
#endif
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &(attribs[i].enabled));
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &(attribs[i].size));
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &(attribs[i].type));
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &(attribs[i].normalized));
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &(attribs[i].stride));
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &(attribs[i].bufferBinding));
            glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &(attribs[i].ptr));
        }
        valid = true;
    }
    void restore()
    {
        if(!valid)
            return;
        glPolygonMode(GL_FRONT_AND_BACK, mode[0]);
        //glPolygonMode(GL_BACK, mode[1]); deprecated in Core GL !!!!!!
        if(cull) glEnable(GL_CULL_FACE);
        else     glDisable(GL_CULL_FACE);
        if(stenciltest) glEnable(GL_STENCIL_TEST);
        else            glDisable(GL_STENCIL_TEST);
        glStencilMask( stencilmask );
        if(depthtest)   glEnable(GL_DEPTH_TEST);
        else            glDisable(GL_DEPTH_TEST);
        glDepthMask( depthmask );
        if(blend)       glEnable(GL_BLEND);
        else            glDisable(GL_BLEND);
        glBlendFunc(sfactor, dfactor);
        glColorMask( red,green,blue,alpha );
        glPrimitiveRestartIndex(primrestartid);
        if(primrestart) glEnable(GL_PRIMITIVE_RESTART);
        else            glDisable(GL_PRIMITIVE_RESTART);
        for(int i=0; i<maxVtxAttribs; i++)
        {
            if(attribs[i].enabled)
                glEnableVertexAttribArray(i);
            else
                glDisableVertexAttribArray(i);
            glBindBuffer(GL_ARRAY_BUFFER, attribs[i].bufferBinding);
            glVertexAttribPointer(i, attribs[i].size, attribs[i].type, attribs[i].normalized, attribs[i].stride, attribs[i].ptr);
#ifdef USE_INSTANCED_ARRAYS
            glVertexAttribDivisor(i, attribs[i].divisor);
#endif
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // TODO: texture
    }
    void setStates()
    {
        // fill mode always
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable( GL_CULL_FACE );
        // Stencil / Depth buffer and test disabled
        glDisable(GL_STENCIL_TEST);
        glStencilMask( 0 );
        glDisable(GL_DEPTH_TEST);
        glDepthMask( GL_FALSE );
        // Blend on for alpha
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Color active
        glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
        glPrimitiveRestartIndex(-1);
        glEnable(GL_PRIMITIVE_RESTART);
    }
    bool    valid;
    int     maxVtxAttribs;
    vtxAttribData *attribs;

    GLenum mode[2];
    GLenum cull;
    GLenum stenciltest;
    int stencilmask;
    GLenum depthtest;
    GLint depthmask;
    GLenum blend;
    GLenum sfactor; GLenum dfactor;
    GLint red; GLint green; GLint blue; GLint alpha;
    GLuint primrestartid;
    GLenum primrestart;
};
static TextBackupState bs;

///////////////////////////////////////////////////////////////////////////////////////
//
//
OpenGLText::OpenGLText()
{
    c_fontNbChars           = 128;
    c_fontHeight            = 16;
    c_fontWidth             = 724;
    m_widgetProgram         = 0;
    m_vShader               = 0;
    m_fShader               = 0;
    m_vbo                   = 0;
#ifdef USEFONTMETRICASUBO
    m_boGlyphTexOffset      = 0;
    m_GlyphTexOffset        = 0;
#endif
    m_vbosz                 = 0;
    m_canvasVar             = 0;
    m_fontTex               = 0;
    m_vertexDepth           = 1.f;
    m_indexOffset           = 0;
    glyphInfos              = NULL;
    allocated               = false;
    //m_window;
};
OpenGLText::~OpenGLText()
{
    //if(m_vShader)
        //glDeleteSh
    m_widgetProgram         = 0;
    m_vShader               = 0;
    m_fShader               = 0;
    if(glyphInfos && allocated)
        delete glyphInfos;
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
inline GLuint OpenGLText::CompileGLSLShader( GLenum target, const char* shader)
{
    GLuint object;

    object = glCreateShader( target);

    if (!object)
        return object;

    glShaderSource( object, 1, &shader, NULL);

    glCompileShader(object);

    // check if shader compiled
    GLint compiled = 0;
    glGetShaderiv(object, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
#ifdef NV_REPORT_COMPILE_ERRORS
        char temp[256] = "";
        glGetShaderInfoLog( object, 256, NULL, temp);
        fprintf( stderr, "Compile failed:\n%s\n", temp);
#endif
        glDeleteShader( object);
        return 0;
    }

    return object;
}

// Create a program composed of vertex and fragment shaders.
inline GLuint OpenGLText::LinkGLSLProgram( GLuint vertexShader, GLuint fragmentShader)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

#ifdef NV_REPORT_COMPILE_ERRORS
    // Get error log.
    GLint charsWritten, infoLogLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

    char * infoLog = new char[infoLogLength];
    glGetProgramInfoLog(program, infoLogLength, &charsWritten, infoLog);
    if((infoLogLength > 0)&&(infoLog[0] != '\0'))
        fprintf( stderr, "Link failed:\n%s\n", infoLog);
    delete [] infoLog;
#endif

    // Test linker result.
    GLint linkSucceed = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkSucceed);
    
    if (linkSucceed == GL_FALSE)
    {
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
#ifdef USE_INSTANCED_ARRAYS
char* OpenGLText::cWidgetVSSource2 = {
    "#version 140\n\
    uniform vec4 canvas; \n"
#ifdef USEFONTMETRICASUBO
    "layout(std140) uniform GlyphTexOffset { \n\
        vec4 glyphTexOffset[256]; // Bugged !!\n\
    };\n"
    "uniform vec4 glyphTexOffset2[256]; // Working\n"
#else
   "in vec4 TexCoord;\n"
#endif
   "in vec4 Position;\n"
   "in vec4 Color;\n\
    out vec2 vsTC;\n\
    out vec4 vsColor;\n\
\n\
    void main()\n\
    {\n\
        int id = gl_VertexID;\n\
        float x = Position.x;\n\
        float y = Position.y;\n\
        float w = Position[2];\n\
        float h = Position[3];\n\
        vec4 p = vec4(x,y,0,1);\n\
        if ((id & 3)==1) { p.x += w; }\n\
        else if ((id & 3)==2) { p.x += w; p.y += h; }\n\
        else if ((id & 3)==3) { p.y += h; }\n\
        \n"
#ifdef USEFONTMETRICASUBO
       "int g = int(Color.w);\n\
        x = glyphTexOffset2[g].x;\n\
        y = glyphTexOffset2[g].y;\n\
        w = glyphTexOffset2[g].z;\n\
        h = glyphTexOffset2[g].w;\n"
#else
       "x = TexCoord.x;\n\
        y = TexCoord.y;\n\
        w = TexCoord[2];\n\
        h = TexCoord[3];\n"
#endif
       "vec2 tc = vec2(x,y);\n\
        if ((id & 3)==1) { tc.x += w; }\n\
        else if ((id & 3)==2) { tc.x += w; tc.y += h; }\n\
        else if ((id & 3)==3) { tc.y += h; }\n\
        \
        gl_Position = vec4( ((p.x / canvas.x)*canvas.z*2.0 - 1.0), \n\
                            ((p.y / canvas.y)*2.0 - 1.0), 0, 1.0); \n\
        vsTC    = tc; \n\
        vsColor = vec4(Color.xyz, 1); \n\
    }\n\
    "};

char* OpenGLText::cWidgetFSSource2 = {
    "#version 140\n\
    uniform sampler2D fontTex;\n\
    in vec2 vsTC;\n\
    in vec4 vsColor;\n\
    out vec4 fragColor;\n\
\n\
    void main()\n\
    {\n\
        vec4 color; \n\
        float distance = (texture2D( fontTex, vsTC ).x); \n\
        color = vsColor;\n\
        color.a *= distance;\n\
        fragColor = color; \n\
    }\n\
    "};
#else
char* OpenGLText::cWidgetVSSource2 = {
    "#version 130\n\
    uniform vec4 canvas; \n\
\n\
    in vec4 Position;\n\
    in vec4 TexCoord;\n\
    in vec4 Color;\n\
    out vec2 vsTC;\n\
    out vec4 vsColor;\n\
\n\
    void main()\n\
    {\n\
        gl_Position = vec4( (((Position.x) / canvas.x)*canvas.z*2.0 - 1.0), \n\
                   (((Position.y) / canvas.y)*2.0 - 1.0), 0, 1.0); \n\
        vsTC = TexCoord.xy; \n\
        vsColor = Color; \n\
    }\n\
    "};

char* OpenGLText::cWidgetFSSource2 = {
    "#version 130\n\
    uniform sampler2D fontTex;\n\
    in vec2 vsTC;\n\
    in vec4 vsColor;\n\
    out vec4 fragColor;\n\
\n\
    void main()\n\
    {\n\
        vec2 texUV = vsTC; \n\
        vec4 color; \n\
        float distance = (texture2D( fontTex, texUV.xy ).x); \n\
        color = vsColor;\n\
        color.a *= distance;\n\
        fragColor = color; \n\
    }\n\
    "};
#endif
///////////////////////////////////////////////////////////////////////////////////////
//
//
void OpenGLText::changeCanvas(int w, int h)
{
    m_canvas.w = w;
    m_canvas.h = h;
    m_canvas.ratio = ((float)m_canvas.winh*w)/((float)m_canvas.winw*h);
}
///////////////////////////////////////////////////////////////////////////////////////
//
//
void OpenGLText::changeSize(int w, int h)
{
    m_canvas.winw = w;
    m_canvas.winh = h;
    m_canvas.ratio = ((float)h*m_canvas.w)/((float)w*m_canvas.h);
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
bool OpenGLText::init(unsigned char *imageData, FileHeader *glyphInfos_, int w, int h)
{
    if(glyphInfos && allocated)
        delete glyphInfos;
    glyphInfos = glyphInfos_;
    allocated = false;

    if(m_fontTex == 0)
        glGenTextures( 1, &m_fontTex );
    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, m_fontTex );
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLenum extFmt = GL_RED;
    glTexImage2D(GL_TEXTURE_2D, 0, extFmt, glyphInfos->texwidth, glyphInfos->texheight, 0, extFmt, GL_UNSIGNED_BYTE, imageData);
    GLenum err = glGetError();

    glBindTexture( GL_TEXTURE_2D, 0 );
    return init(w,h);
}
///////////////////////////////////////////////////////////////////////////////////////
//
//
bool OpenGLText::init(const char * fontName, int w, int h)
{
    //
    // Load the Font if possible
    //
    if(fontName)
    {
        char fname[200];
        sprintf(fname, "%s.tga", fontName);
        TGA* fontTGA = new TGA;
        TGA::TGAError err = fontTGA->load(fname);
        if(err != TGA::TGA_NO_ERROR)
            return false;
        sprintf(fname, "%s.bin", fontName);
        FILE *fd = fopen(fname, "rb");
        if(fd)
        {
            if(glyphInfos && allocated)
                delete glyphInfos;
            glyphInfos = new FileHeader;
            allocated = true;
            int r = (int)fread(glyphInfos, 1, sizeof(FileHeader), fd);
            fclose(fd);
            // Change the shaders to use for this case

            int u = fontTGA->m_nImageWidth;
            int h = fontTGA->m_nImageHeight;

            if(m_fontTex == 0)
                glGenTextures( 1, &m_fontTex );
            glActiveTexture( GL_TEXTURE0 );
            glBindTexture( GL_TEXTURE_2D, m_fontTex );
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GLenum extFmt;
            switch(fontTGA->m_texFormat)
            {
            case TGA::RGB:
                extFmt = GL_RGB;
                break;
            case TGA::RGBA:
                extFmt = GL_RGBA;
                break;
            case TGA::ALPHA:
                extFmt = GL_LUMINANCE;
                break;
            }
            glTexImage2D(GL_TEXTURE_2D, 0, extFmt, u, h, 0, extFmt, GL_UNSIGNED_BYTE, fontTGA->m_nImageData);
            GLenum err = glGetError();

            glBindTexture( GL_TEXTURE_2D, 0 );
        }
        delete fontTGA;
    }
    return init(w,h);
}
bool OpenGLText::init(int w, int h)
{
    m_canvas.w = w;
    m_canvas.h = h;
    m_canvas.ratio = 1.0;
    if (m_widgetProgram == 0)
    {
        //#if defined(NV_WINDOWS)
        //#define GET_PROC_ADDRESS(name)          glGetProcAddress(name)
        //#elif defined(NV_MACINTOSH_OSX)
        //#define GET_PROC_ADDRESS(name)          dlsym(RTLD_NEXT, name)
        //#elif defined(NV_LINUX)
        //#define GET_PROC_ADDRESS(name)          glXGetProcAddressARB((GLubyte *) name)
        //#endif
        //#define INITGL(n)\
        //    if(!gl##n)\
        //    {\
        //    gl##n = (PFN_##n)GET_PROC_ADDRESS("gl" #n);\
        //    if(!gl##n) {\
        //    PRINTF(("ERROR>> App queried " #n " - unsupported by driver\n"));\
        //            return;\
        //        }\
        //    }
        //INITGL(CreateShader);
        //INITGL(ShaderSource);
        //INITGL(CompileShader);
        //INITGL(GetShaderiv);
        //INITGL(DeleteShader);
        //INITGL(CreateProgram);
        //INITGL(AttachShader);
        //INITGL(LinkProgram);
        //INITGL(GetProgramiv);
        //INITGL(GetProgramInfoLog);
        //INITGL(UseProgram);
        //INITGL(GetUniformLocation);
        //INITGL(GetAttribLocation);
        //INITGL(Uniform4fv);
        //INITGL(Uniform4f);
        //INITGL(Uniform1i);
        //INITGL(ActiveTexture);
        //INITGL(PrimitiveRestartIndex);
        //INITGL(EnableVertexAttribArray);
        //INITGL(DisableVertexAttribArray);
        //INITGL(VertexAttribPointer);
        //INITGL(BindBuffer);

        m_vShader = CompileGLSLShader( GL_VERTEX_SHADER, cWidgetVSSource2);
        if (!m_vShader) 
            fprintf(stderr, "Vertex shader compile failed\n");
        m_fShader = CompileGLSLShader( GL_FRAGMENT_SHADER, cWidgetFSSource2);
        if (!m_fShader) 
            fprintf(stderr, "Fragment shader compile failed\n");

        //CHECKGLERRORS();

        m_widgetProgram = LinkGLSLProgram( m_vShader, m_fShader );
        //CHECKGLERRORS();

        GLint fontTexLoc = glGetUniformLocation(m_widgetProgram, "fontTex");
        m_canvasVar     = glGetUniformLocation( m_widgetProgram, "canvas" );
        glUseProgram(m_widgetProgram);
        {
            glUniform1i(fontTexLoc, 0); //Texture unit 0 is for font Tex.
        }
        glUseProgram(0);

        glGenBuffers(1, &m_vbo);
#ifdef USEFONTMETRICASUBO
        // Upload the font metric in a UBO
        float *tcs = new float[256*4];
        for(int i=0; i<256; i++)
        {
            tcs[(4*i)+0] = glyphInfos->glyphs[i].norm.u;
            tcs[(4*i)+1] = glyphInfos->glyphs[i].norm.v;
            tcs[(4*i)+2] = glyphInfos->glyphs[i].norm.width;
            tcs[(4*i)+3] = glyphInfos->glyphs[i].norm.height;
        }
        
        glGenBuffers(1, &m_boGlyphTexOffset);
        glBindBuffer( GL_UNIFORM_BUFFER, m_boGlyphTexOffset );
        glBufferData(GL_UNIFORM_BUFFER, 256*4*sizeof(float), tcs, GL_STATIC_DRAW);
        // BUG HERE...
        m_GlyphTexOffset = glGetUniformBlockIndex(m_widgetProgram, "GlyphTexOffset");
        // we must bind this block index to our own number... yeah... let's take the same
        // http://www.opengl.org/sdk/docs/man4/xhtml/glUniformBlockBinding.xml
        glUniformBlockBinding(m_widgetProgram, m_GlyphTexOffset, m_GlyphTexOffset);
        GLint blockSize; // for debug purpose...
        glGetActiveUniformBlockiv(m_widgetProgram, m_GlyphTexOffset, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);
        //assert(blockSize == 256*4*sizeof(float));
        glBindBufferBase( GL_UNIFORM_BUFFER, m_GlyphTexOffset, m_boGlyphTexOffset );
        // This one is working
        int glyphTexOffset2 = glGetUniformLocation( m_widgetProgram, "glyphTexOffset2" );
        glProgramUniform4fv(m_widgetProgram, glyphTexOffset2, 256, tcs);
        delete [] tcs;
#else
        locTc  = glGetAttribLocation( m_widgetProgram, "TexCoord" );
#endif
        locCol = glGetAttribLocation( m_widgetProgram, "Color" );
        locPos = glGetAttribLocation( m_widgetProgram, "Position" );
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
void OpenGLText::pushVertex( Vertex* v )
{
    m_vertices.push_back( *v );
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
void OpenGLText::beginString()
{
    m_indexOffset = 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
void OpenGLText::endString()
{
    if(!m_vertices.empty())
    {
        bs.setStates();
        glBindBuffer( GL_ARRAY_BUFFER, m_vbo );
        // Note: we could also downsize the buffer if the size is really smaller than the one currently allocated
        // this could be done after few iterations, if we see that the size if still smaller than the allocation...
        if(m_vbosz < m_vertices.size())
        {
            glBufferData(GL_ARRAY_BUFFER, m_vertices.size()*sizeof(Vertex), &(m_vertices.front()), GL_STREAM_DRAW);
            m_vbosz = m_vertices.size();
        } else
            glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertices.size()*sizeof(Vertex), &(m_vertices.front()));

        static Vertex* pVtxOffset = NULL;
        glEnableVertexAttribArray( locPos );
        glVertexAttribPointer( locPos, 4, GL_FLOAT, false, sizeof(Vertex), pVtxOffset->pos );
#ifndef USEFONTMETRICASUBO
        glEnableVertexAttribArray( locTc );
        glVertexAttribPointer( locTc , 4, GL_FLOAT, false, sizeof(Vertex), pVtxOffset->tc );
#endif
        glEnableVertexAttribArray( locCol );
        glVertexAttribPointer( locCol, 4, GL_FLOAT, false, sizeof(Vertex), pVtxOffset->color );

#ifdef USE_INSTANCED_ARRAYS
        glVertexAttribDivisor(locPos, 4);
#ifndef USEFONTMETRICASUBO
        glVertexAttribDivisor(locTc, 4);
#endif
        glVertexAttribDivisor(locCol, 4);
#endif

        //glActiveTexture( GL_TEXTURE0 );
        glBindTexture(GL_TEXTURE_2D, m_fontTex);

        glUseProgram( m_widgetProgram );
        glUniform4f( m_canvasVar, m_canvas.w, m_canvas.h, m_canvas.ratio, 0 );

#ifdef USE_INSTANCED_ARRAYS
        glDrawArraysInstanced( GL_QUADS, 0, 4, m_vbosz*4);
#else
        glDrawArrays( GL_QUADS, 0, m_vbosz);
#endif

        glUseProgram( 0 );

        // switch vertex attribs back off
        glDisableVertexAttribArray( locPos );
#ifndef USEFONTMETRICASUBO
        glDisableVertexAttribArray( locTc );
#endif
        glDisableVertexAttribArray( locCol );
        GLenum err = glGetError();
        //assert(err == 0);
        m_vertices.resize(0);
    }
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
void OpenGLText::stringSize(const char *text, float *sz)
{
    if(!glyphInfos)
        return;
    int h = glyphInfos->pix.ascent + glyphInfos->pix.descent + glyphInfos->pix.linegap;
    float lPosX = 0+1;
    float lPosY = 0 ;

    float lLinePosX = lPosX;
    float lLinePosY = lPosY;
    const char* c = text;
    while (*c != '\0')
    {
        if ( *c == '\n')
        {
            lPosX = lLinePosX;
            lLinePosY -= h;
            lPosY = lLinePosY; 
        }
        else if ( *c > 128 || *c < 0 )
        {

        }
        else
        {
            GlyphInfo &g = glyphInfos->glyphs[*c];
            //int pX = lPosX + g.pix.offX;
            //int pY = lPosY - g.pix.height - g.pix.offY;
            lPosX += g.pix.advance;
            lPosY += 0;
        }
        c++;
    }
    sz[0] = lPosX;
    sz[1] = lPosY + h;
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
void OpenGLText::drawString( int x, int y, const char * text, int nbLines, unsigned long color)
{
    float color4f[4];
    color4f[0] = (float)(color>>24 & 0xFF)/255.0;
    color4f[1] = (float)(color>>16 & 0xFF)/255.0;
    color4f[2] = (float)(color>>8 & 0xFF)/255.0;
    color4f[3] = (float)(color & 0xFF)/255.0;
    drawString( x, y, text, nbLines, color4f);
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
void OpenGLText::drawString( int x, int y, const char * text, int nbLines, float * color4f)
{
    if(!glyphInfos)
        return;
    int h = glyphInfos->pix.ascent + glyphInfos->pix.descent + glyphInfos->pix.linegap;
    float lPosX = x+1;
    float lPosY = y ;
    if (nbLines > 1)
        lPosY +=  h * (nbLines - 1);

    float lLinePosX = lPosX;
    float lLinePosY = lPosY;
    const char* c = text;
    Vertex v;
    v.color[0] = color4f[0];
    v.color[1] = color4f[1];
    v.color[2] = color4f[2];
    v.color[3] = color4f[3];
 
    m_vertexDepth = 1.0;
    while (*c != '\0')
    {
        if ( *c == '\n')
        {
            lPosX = lLinePosX;
            lLinePosY -= h;
            lPosY = lLinePosY; 
        }
        else if ( *c > 128 || *c < 0 )
        {

        }
        else
        {
            GlyphInfo &g = glyphInfos->glyphs[*c];
            int pX = lPosX + g.pix.offX;
            int pY = lPosY - g.pix.height - g.pix.offY;
#ifdef USE_INSTANCED_ARRAYS
            v.setPos( pX ,             pY,                  g.pix.width,     g.pix.height);
            v.setTC ( g.norm.u,        g.norm.v,            g.norm.width,    g.norm.height);
            // to find back the Texture coords in atlas
            v.color[3] = (float)(*c);
            pushVertex(&v);
#else

            v.setPos( pX ,                   pY,                     0,1);
            v.setTC ( g.norm.u,              g.norm.v,               0,0);
            pushVertex(&v);
            v.setPos( pX + g.pix.width ,     pY,                     0,1);
            v.setTC ( g.norm.u+g.norm.width, g.norm.v,               0,0);  
            pushVertex(&v);
            v.setPos( pX + g.pix.width,     pY + g.pix.height,      0,1);
            v.setTC ( g.norm.u+g.norm.width, g.norm.v+g.norm.height, 0,0);           
            pushVertex(&v);
            v.setPos( pX,                    pY + g.pix.height,      0,1);
            v.setTC ( g.norm.u,              g.norm.v+g.norm.height, 0,0);  
            pushVertex(&v);
#endif
            lPosX += g.pix.advance;
            lPosY += 0;
        }

        c++;
    }
    m_vertexDepth = 1.f;
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
void OpenGLText::BackupStates()
{
    if(!bs.valid)
        bs.backup();
}
void OpenGLText::RestoreStates()
{
    bs.restore();
}

