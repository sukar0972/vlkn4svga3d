#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int failures;
static void check_error(const char *where) {
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) { fprintf(stderr, "%s: GL error 0x%x\n", where, e); ++failures; }
}
static void verify(int flip_x, int flip_y) {
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
        int source = (flip_x ? 1-x : x) + 2*(flip_y ? 1-y : y);
        GLfloat depth; GLubyte stencil;
        glReadPixels(2+4*x, 2+4*y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        glReadPixels(2+4*x, 2+4*y, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &stencil);
        float expected = .125f + .25f*source;
        if (fabsf(depth-expected) > .00001f || stencil != 17*(source+1)) {
            fprintf(stderr,"flip=%d,%d quadrant=%d,%d: depth %.6f expected %.6f; stencil %u expected %d\n",flip_x,flip_y,x,y,depth,expected,stencil,17*(source+1)); ++failures;
        }
    }
    check_error("depth/stencil probes");
}
int main(void) {
    Display *d = XOpenDisplay(NULL); if (!d) return 2;
    int attrs[] = {GLX_RGBA,GLX_DEPTH_SIZE,24,GLX_STENCIL_SIZE,8,None};
    XVisualInfo *v = glXChooseVisual(d,DefaultScreen(d),attrs); if (!v) return 2;
    Colormap cm = XCreateColormap(d,RootWindow(d,v->screen),v->visual,AllocNone);
    XSetWindowAttributes wa = {.colormap=cm};
    Window w = XCreateWindow(d,RootWindow(d,v->screen),0,0,16,16,0,v->depth,InputOutput,v->visual,CWColormap,&wa);
    XMapWindow(d,w);XSync(d,False);
    GLXContext context=glXCreateContext(d,v,NULL,True);if (!context || !glXMakeCurrent(d,w,context)) return 2;
    printf("Renderer: %s; version: %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
    GLuint fbo,depth,color;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,8,8);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
    glGenRenderbuffers(1,&color);glBindRenderbuffer(GL_RENDERBUFFER,color);glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,8,8);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,color);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) return 2;
    glEnable(GL_SCISSOR_TEST);
    for(int y=0;y<2;++y) for(int x=0;x<2;++x) {
        int q=x+2*y;glScissor(x*4,y*4,4,4);glClearDepth(.125+.25*q);glClearStencil(17*(q+1));glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    }
    glDisable(GL_SCISSOR_TEST);check_error("source clear");
    for(int fy=0;fy<2;++fy) for(int fx=0;fx<2;++fx) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);glClearDepth(1);glClearStencil(0);glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
        glBlitFramebuffer(fx?8:0,fy?8:0,fx?0:8,fy?0:8,0,0,8,8,GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT,GL_NEAREST);
        check_error("framebuffer blit");glBindFramebuffer(GL_READ_FRAMEBUFFER,0);verify(fx,fy);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glBlitFramebuffer(0,0,4,8,2,0,6,8,GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT,GL_NEAREST);
    for (int y=0;y<2;++y) for(int x=3;x<=5;x+=2) {
        GLfloat z; GLubyte stencil;
        glReadPixels(x,2+4*y,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&z);
        glReadPixels(x,2+4*y,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&stencil);
        if (fabsf(z-(.125f+.5f*y))>.00001f || stencil!=17*(1+2*y)) {
            fprintf(stderr,"Overlapping copy changed source pixels: depth %.6f stencil %u\n",z,stencil);++failures;
        }
    }
    check_error("overlapping depth/stencil copy");
    GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_CUBE_MAP,texture);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP,1,GL_DEPTH_STENCIL,16,16);
    GLenum error=glGetError();
    printf("Unsized depth-stencil cube storage error: 0x%x\n",error);
    if(error!=GL_INVALID_ENUM && error!=GL_INVALID_VALUE) {++failures;fprintf(stderr,"Expected one of the applicable INVALID_ENUM/INVALID_VALUE errors\n");}
    GLint immutable=1;glGetTexParameteriv(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_IMMUTABLE_FORMAT,&immutable);
    if(immutable!=0){++failures;fprintf(stderr,"Rejected storage call modified texture state\n");}
    check_error("texture state after error");
    glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteTextures(1,&texture);glDeleteFramebuffers(1,&fbo);glDeleteRenderbuffers(1,&depth);glDeleteRenderbuffers(1,&color);
    glXMakeCurrent(d,None,NULL);glXDestroyContext(d,context);XDestroyWindow(d,w);XFreeColormap(d,cm);XFree(v);XCloseDisplay(d);
    printf("Depth/stencil orientation and storage controls: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
