#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <unistd.h>
static int failures;
static unsigned long pixel(Display *d,Drawable w,int x,int y) {
    XImage *image=XGetImage(d,w,x,y,1,1,AllPlanes,ZPixmap);
    if(!image){++failures;return ~0ul;}
    unsigned long value=XGetPixel(image,0,0);XDestroyImage(image);return value&0xffffff;
}
static void check(const char *what,unsigned long value,unsigned long expected) {
    if(value!=expected){fprintf(stderr,"%s got=%06lx expected=%06lx\n",what,value,expected);++failures;}
}
int main(void) {
    Display *d=XOpenDisplay(NULL);if(!d)return 2;
    int attributes[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,None};
    XVisualInfo *v=glXChooseVisual(d,DefaultScreen(d),attributes);if(!v)return 3;
    Window root=RootWindow(d,v->screen);
    XSetWindowAttributes a={.colormap=XCreateColormap(d,root,v->visual,AllocNone),.override_redirect=True,.background_pixel=0};
    Window w=XCreateWindow(d,root,100,100,64,64,0,v->depth,InputOutput,v->visual,CWColormap|CWOverrideRedirect|CWBackPixel,&a);
    XMapRaised(d,w);XSync(d,False);GLXContext context=glXCreateContext(d,v,NULL,True);
    if(!context||!glXMakeCurrent(d,w,context))return 4;
    printf("Renderer: %s\n",glGetString(GL_RENDERER));
    GC gc=XCreateGC(d,w,0,NULL);glViewport(0,0,64,64);
    for(unsigned i=0;i<12;++i) {
        const unsigned long color=i%2?0x0000ff:0xff0000;
        glClearColor(i%2?0:1,0,i%2?1:0,1);glClear(GL_COLOR_BUFFER_BIT);glXSwapBuffers(d,w);glFinish();XSync(d,False);usleep(30000);glXWaitX();
        unsigned char rgba[4]={};glReadBuffer(GL_FRONT);glReadPixels(48,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
        check("GL front",((unsigned long)rgba[0]<<16)|((unsigned long)rgba[1]<<8)|rgba[2],color);glXWaitGL();XSync(d,False);
        check("window after swap",pixel(d,w,48,32),color);
        int x,y;Window child;XTranslateCoordinates(d,w,root,0,0,&x,&y,&child);
        check("screen after swap",pixel(d,root,x+48,y+32),color);
        XSetForeground(d,gc,0x00ff00);XFillRectangle(d,w,gc,0,0,16,64);XSync(d,False);glXWaitX();usleep(30000);
        check("window software overlay",pixel(d,w,8,32),0x00ff00);
        check("window untouched 3D",pixel(d,w,48,32),color);
        check("screen software overlay",pixel(d,root,x+8,y+32),0x00ff00);
        check("screen untouched 3D",pixel(d,root,x+48,y+32),color);
        XMoveWindow(d,w,i%2?100:200,100);XSync(d,False);glXWaitX();usleep(30000);
        XTranslateCoordinates(d,w,root,0,0,&x,&y,&child);
        check("moved window overlay",pixel(d,w,8,32),0x00ff00);
        check("moved window 3D",pixel(d,w,48,32),color);
        check("moved screen overlay",pixel(d,root,x+8,y+32),0x00ff00);
        check("moved screen 3D",pixel(d,root,x+48,y+32),color);
        if(glGetError()!=GL_NO_ERROR)++failures;
    }
    // Nonuniform front/screen pixels also check readback orientation.
    const unsigned long colors[4]={0xffff00,0x00ffff,0xff00ff,0xffffff};
    glEnable(GL_SCISSOR_TEST);
    for(unsigned q=0;q<4;++q) {
        glScissor((q&1)*32,(q>>1)*32,32,32);
        unsigned long c=colors[q];
        glClearColor(((c>>16)&255)/255.f,((c>>8)&255)/255.f,(c&255)/255.f,1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glDisable(GL_SCISSOR_TEST);glXSwapBuffers(d,w);glFinish();XSync(d,False);usleep(30000);glXWaitX();
    int px,py;Window child;XTranslateCoordinates(d,w,root,0,0,&px,&py,&child);
    for(unsigned q=0;q<4;++q) {
        int gx=(q&1)*32+16,gy=(q>>1)*32+16;
        unsigned char rgba[4]={};glReadBuffer(GL_FRONT);glReadPixels(gx,gy,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
        check("pattern GL front",((unsigned long)rgba[0]<<16)|((unsigned long)rgba[1]<<8)|rgba[2],colors[q]);
        check("pattern window",pixel(d,w,gx,63-gy),colors[q]);
        check("pattern screen",pixel(d,root,px+gx,py+63-gy),colors[q]);
    }
    XFreeGC(d,gc);glXMakeCurrent(d,None,NULL);glXDestroyContext(d,context);XDestroyWindow(d,w);XFree(v);XCloseDisplay(d);
    printf("GLX/X11 mixed drawing and movement: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
