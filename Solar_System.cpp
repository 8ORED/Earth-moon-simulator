#include <GL/glut.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION   // image loader (public domain, single header)
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"
#define PI 3.14159265358979323846f
#define toRad(d) ((d)*PI/180.0f)

// OpenGL 1.2 constant that the old Windows gl.h does not declare
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// FreeGLUT's special-key codes for the Shift keys (not declared by every glut.h)
#ifndef GLUT_KEY_SHIFT_L
#define GLUT_KEY_SHIFT_L 0x0070
#define GLUT_KEY_SHIFT_R 0x0071
#endif

// ---- Scale (Mercury radius = 1 unit) ----
// Planet and Sun sizes are true relative to Mercury. E = Earth's radius in these units.
// AU is the orbit scale: lower it to shrink the system (the true Sun-Earth distance would be ~61240).
// It must stay above ~737 so Mercury (0.387 AU) clears the Sun.
const float E=2.611f, AU=2611, SUN_R=285.1f;

// Overview mode: planet sizes stay accurate, but orbit distances are compressed (sqrt) so the whole
// system fits on screen. Orbit radius = OV*(1+sqrt(AU count)): Mercury ~400 (clears the Sun), Neptune ~1600.
const float OV=246.6f, OV_VIEW_H=3000;

// Proximity behaviour (distances measured from a body's centre, in multiples of its radius):
// the camera can't get closer than STOP_MULT (this is also the height used by the jump keys), and
// revolution slows from full speed at SLOW_MULT down to MIN_TIME of normal at the stop distance.
const float STOP_MULT=3, SLOW_MULT=40, MIN_TIME=0.02f, SUN_STOP=1.2f;

// Saturn's rings: inner/outer edge in Saturn radii (C ring to outer A ring) and axial tilt in degrees
const float RING_IN=1.24f, RING_OUT=2.27f, RING_TILT=26.7f;

// ---- State ----
float moonAngle=0, earthAngle=0;
double earthOrbitAngle=0;             // never wrapped: each planet's angle is a multiple of it, so wrapping would make them jump
float moonOrbitRadius=3*E;                 // NOT to scale (true value is ~60*E)
float moonCamDistance=E, moonCamHeight=0;
float moonOrbitSpeed=-0.5f, earthRotationSpeed=2, earthOrbitSpeed=-0.04f;
float moveStep=1, tScale=1;                // set every frame by physics()
float camSpeed=0;                          // distance moved per frame while a movement key is held
bool  debug=false;                         // B toggles the camera position / speed readout
bool  animationPaused=false, lockToMoon=false, overview=true;   // overview = default view
bool  shiftHeld=false;                     // modifier keys don't auto-repeat, so Shift is tracked as held/released
bool  keys[256];                           // which keys are currently held (lower case)
int   windowWidth=800, windowHeight=600;

// ---- Planets (Earth is in the table; its Moon is drawn separately) ----
// radius (Mercury=1), distance from Sun (AU x scale above), diffuse RGB
// file = texture in the textures/ folder; id = OpenGL texture (0 = not loaded, the plain colour is used instead)
struct Planet{ float radius, distance, r,g,b; const char* file; GLuint id; };
Planet planets[]={
    { 1.000f,  0.387f*AU, 0.60f,0.60f,0.60f, "mercury.jpg",       0},   // Mercury
    { 2.478f,  0.723f*AU, 0.90f,0.80f,0.50f, "venus_surface.jpg", 0},   // Venus
    {     E,   1.000f*AU, 0.20f,0.40f,1.00f, "earth.jpg",         0},   // Earth
    { 1.389f,  1.524f*AU, 0.80f,0.30f,0.15f, "mars.jpg",          0},   // Mars
    {29.270f,  5.203f*AU, 0.80f,0.65f,0.50f, "jupiter.jpg",       0},   // Jupiter
    {24.670f,  9.580f*AU, 0.90f,0.80f,0.55f, "saturn.jpg",        0},   // Saturn
    {10.470f, 19.200f*AU, 0.55f,0.85f,0.90f, "uranus.jpg",        0},   // Uranus
    {10.130f, 30.100f*AU, 0.25f,0.35f,0.90f, "neptune.jpg",       0}    // Neptune
};
GLuint moonTex=0, sunTex=0, ringTex=0, starTex=0;   // Moon, Sun, Saturn's ring (saturn_ring_alpha.png) and the star background

// ---- Camera ----
float camX=0,camY=OV_VIEW_H,camZ=0, yaw=-90,pitch=-89;   // top-down view above the Sun, whole system in view
float camFrontX=0,camFrontY=-1,camFrontZ=0;              // view direction, recomputed from yaw/pitch every frame in display()
float mouseSensitivity=0.1f;

// ---- Lighting ----
GLfloat lightPos[]     = {0,0,0,1};   // Sun at the origin
GLfloat lightAmbient[] = {0.2f,0.2f,0.2f,1};
GLfloat lightDiffuse[] = {0.9f,0.9f,0.9f,1};
GLfloat lightSpecular[]= {1,1,1,1};

// Position on a circular orbit around the Sun (origin). The angle follows Kepler's 3rd law relative to
// Earth (closer = faster) using the true distance; the radius is the true distance, or the compressed
// one in overview mode.
void getPlanetPos(float distance,float &x,float &y,float &z){
    float a=toRad((float)fmod(earthOrbitAngle*pow(AU/distance,1.5),360.0));   // double maths, then wrapped per planet
    float r=overview ? OV*(1+sqrt(distance/AU)) : distance;
    x=r*cos(a); y=0; z=r*sin(a);
}

// Loads textures/<file> as an OpenGL texture with mipmaps. Returns 0 if the file is missing (the plain colour is used).
GLuint loadTexture(const char* file){
    char path[128]; sprintf(path,"textures/%s",file);
    int w,h,n; unsigned char* px=stbi_load(path,&w,&h,&n,4);   // always 4 channels (RGBA)
    if(!px){ printf("Texture not found: %s\n",path); return 0; }
    GLuint id; glGenTextures(1,&id); glBindTexture(GL_TEXTURE_2D,id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);   // longitude wraps around
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);   // latitude does not (plain GL_CLAMP blends in the black border at the poles)
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGBA,w,h,GL_RGBA,GL_UNSIGNED_BYTE,px);
    stbi_image_free(px);
    return id;
}

void drawSphere(float r,int stacks,int slices){
    for(int i=0;i<stacks;i++){
        float t1=i*PI/stacks, t2=(i+1)*PI/stacks;
        glBegin(GL_TRIANGLES);
        for(int j=0;j<slices;j++){
            float p1=j*2*PI/slices, p2=(j+1)*2*PI/slices;
            float theta[4]={t1,t2,t2,t1}, phi[4]={p1,p1,p2,p2};
            int idx[]={0,1,2, 0,2,3};   // triangle 1: 0,1,2  triangle 2: 0,2,3
            for(int k=0;k<6;k++){
                int n=idx[k];
                float x=sin(theta[n])*cos(phi[n]), y=cos(theta[n]), z=sin(theta[n])*sin(phi[n]);
                glTexCoord2f(1-phi[n]/(2*PI), theta[n]/PI);   // s: longitude (left to right = east), t: pole to pole
                glNormal3f(x,y,z);        // unit sphere normal
                glVertex3f(r*x,r*y,r*z);  // sphere vertex
            }
        }
        glEnd();
    }
}

// Per-frame camera and time rules, from every body's position (8 planets + the Sun):
//  - the camera is pushed back out to each body's stop distance (no passing through bodies)
//  - moveStep: 10% of the distance to the nearest body's surface (gradual approach)
//  - tScale: revolution speed, 1 far away, falling to MIN_TIME at a planet's stop distance
// Moon-lock mode skips the push and the slowdown, so the Moon keeps cycling through its phases.
void physics(){
    moveStep=1e30f; tScale=1;
    for(int i=0;i<9;i++){
        float bx=0,by=0,bz=0, r=SUN_R, stop=SUN_R*SUN_STOP;
        if(i<8){ getPlanetPos(planets[i].distance,bx,by,bz); r=planets[i].radius; stop=r*STOP_MULT; }
        float dx=camX-bx, dy=camY-by, dz=camZ-bz, d=sqrt(dx*dx+dy*dy+dz*dz);
        if(!lockToMoon && d<stop){
            if(d<1e-4f){ dx=0; dy=1; dz=0; d=1; }   // exactly at the centre: push straight up
            camX=bx+dx/d*stop; camY=by+dy/d*stop; camZ=bz+dz/d*stop; d=stop;
        }
        if(d-r<moveStep) moveStep=d-r;
        if(i<8 && !lockToMoon){
            float t=(d-stop)/(r*SLOW_MULT-stop); t=t<0?0:t>1?1:t;
            float f=MIN_TIME+(1-MIN_TIME)*t;
            if(f<tScale) tScale=f;
        }
    }
    moveStep=0.1f*(moveStep>0.01f?moveStep:0.01f);
}

// ---- Display ----
void display(){
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    // Moon position: it orbits Earth
    float ex,ey,ez, mr=toRad(moonAngle), lx,ly,lz;
    getPlanetPos(AU,ex,ey,ez);
    float moonX=ex+moonOrbitRadius*cos(mr), moonY=ey, moonZ=ez+moonOrbitRadius*sin(mr);

    if(lockToMoon){
        // Camera sits on the Earth-Moon line, moonCamDistance from the Moon toward Earth
        camX=moonX-cos(mr)*moonCamDistance;
        camY=moonY+moonCamHeight;
        camZ=moonZ-sin(mr)*moonCamDistance;
        lx=moonX; ly=moonY; lz=moonZ;
    } else {
        camFrontX=cos(toRad(pitch))*cos(toRad(yaw));
        camFrontY=sin(toRad(pitch));
        camFrontZ=cos(toRad(pitch))*sin(toRad(yaw));
        lx=camX+camFrontX; ly=camY+camFrontY; lz=camZ+camFrontZ;
    }

    gluLookAt(camX,camY,camZ, lx,ly,lz, 0,1,0);
    glLightfv(GL_LIGHT0,GL_POSITION,lightPos);

    // Star background: a big textured sphere centred on the camera, drawn first and without writing depth,
    // so it always sits behind everything. Mirrored in X because it is seen from the inside.
    if(starTex){
        glDisable(GL_LIGHTING); glEnable(GL_TEXTURE_2D); glDepthMask(GL_FALSE);
        glBindTexture(GL_TEXTURE_2D,starTex);
        glColor3f(1,1,1);
        glPushMatrix(); glTranslatef(camX,camY,camZ); glScalef(-1,1,1); drawSphere(100000,60,60); glPopMatrix();
        glDepthMask(GL_TRUE); glEnable(GL_LIGHTING);
    }

    glEnable(GL_TEXTURE_2D);   // textures are on for the bodies below, off again before the lines and text

    // Planets (including Earth): each revolves around the Sun and spins
    float pos[8][3];
    for(int i=0;i<8;i++){
        Planet &p=planets[i];
        getPlanetPos(p.distance,pos[i][0],pos[i][1],pos[i][2]);
        // textured: white material so the texture shows its own colours; otherwise the plain colour
        GLfloat pd[]={p.id?1:p.r, p.id?1:p.g, p.id?1:p.b, 1}, ps[]={0.1f,0.1f,0.1f,1};
        glBindTexture(GL_TEXTURE_2D,p.id);
        glMaterialfv(GL_FRONT,GL_DIFFUSE,pd);
        glMaterialfv(GL_FRONT,GL_SPECULAR,ps);
        glMaterialf (GL_FRONT,GL_SHININESS,30);
        glPushMatrix(); glTranslatef(pos[i][0],pos[i][1],pos[i][2]); glRotatef(earthAngle,0,1,0);
        drawSphere(p.radius,30,30); glPopMatrix();

    }

    // Overview mode: planets are under a pixel at this zoom, so mark each with a small coloured dot
    if(overview){
        glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glPointSize(2);
        glBegin(GL_POINTS);
        for(int i=0;i<8;i++){
            glColor3f(planets[i].r,planets[i].g,planets[i].b);
            glVertex3f(pos[i][0],pos[i][1],pos[i][2]);
        }
        glEnd(); glEnable(GL_LIGHTING); glEnable(GL_TEXTURE_2D);
    }

    // Moon
    float mk=moonTex?1.0f:0.7f;   // white material when textured, grey otherwise
    GLfloat md[]={mk,mk,mk,1}, ms[]={0.2f,0.2f,0.2f,1};
    glBindTexture(GL_TEXTURE_2D,moonTex);
    glMaterialfv(GL_FRONT,GL_DIFFUSE,md);
    glMaterialfv(GL_FRONT,GL_SPECULAR,ms);
    glMaterialf (GL_FRONT,GL_SHININESS,80);
    // Tidally locked: it turns once per orbit (same angle, opposite sense), so its -X face always points at Earth
    glPushMatrix(); glTranslatef(moonX,moonY,moonZ); glRotatef(-moonAngle,0,1,0); drawSphere(0.27f*E,20,20); glPopMatrix();

    // Sun (drawn at the light position, unlit so it looks self-illuminated)
    glDisable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D,sunTex);
    if(sunTex) glColor3f(1,1,1); else glColor3f(0.93f,0.55f,0.21f);   // plain colour only if the texture is missing
    glPushMatrix(); glTranslatef(lightPos[0],lightPos[1],lightPos[2]); drawSphere(SUN_R,60,60); glPopMatrix();

    // Saturn's ring: flat, tilted, lit on both sides (a top and a bottom layer, back faces culled).
    // Textured with transparency, so it is drawn after the opaque bodies and does not write depth.
    glEnable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
    float rk=ringTex?1.0f:0.8f;   // white material when textured, tan otherwise
    GLfloat rd[]={rk,ringTex?1.0f:0.7f,ringTex?1.0f:0.5f,1}, rs[]={0,0,0,1};
    glMaterialfv(GL_FRONT,GL_DIFFUSE,rd);
    glMaterialfv(GL_FRONT,GL_SPECULAR,rs);
    glBindTexture(GL_TEXTURE_2D,ringTex);
    glPushMatrix(); glTranslatef(pos[5][0],pos[5][1],pos[5][2]); glRotatef(RING_TILT,1,0,0);   // pos[5] = Saturn
    glEnable(GL_CULL_FACE);
    for(int L=0;L<2;L++){                              // L=0: top face (normal +Y), L=1: bottom face (normal -Y)
        float a0=(L?RING_IN:RING_OUT)*planets[5].radius, a1=(L?RING_OUT:RING_IN)*planets[5].radius;   // vertex order sets the facing
        float u0=L?0:1, u1=L?1:0;                      // texture s across the ring: 0 at the inner edge, 1 at the outer edge
        glNormal3f(0,L?-1:1,0);
        glBegin(GL_QUAD_STRIP);
        for(int k=0;k<=96;k++){
            float a=2*PI*k/96;
            glTexCoord2f(u0,0.5f); glVertex3f(a0*cos(a),0,a0*sin(a));
            glTexCoord2f(u1,0.5f); glVertex3f(a1*cos(a),0,a1*sin(a));
        }
        glEnd();
    }
    glDisable(GL_CULL_FACE);
    glPopMatrix();
    glDepthMask(GL_TRUE); glDisable(GL_BLEND); glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);

    // Faint orbit lines: a circle through each planet, radius taken from its current position.
    // Drawn after the opaque objects, because blended lines must not block anything behind them.
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    for(int i=0;i<8;i++){
        float orad=sqrt(pos[i][0]*pos[i][0]+pos[i][2]*pos[i][2]);
        glColor4f(planets[i].r,planets[i].g,planets[i].b,0.3f);
        glBegin(GL_LINE_LOOP);
        for(int k=0;k<360;k++) glVertex3f(orad*cos(toRad(k)),0,orad*sin(toRad(k)));
        glEnd();
    }
    glDisable(GL_BLEND);
    glColor3f(1,1,1);   // controls text colour

    // Controls overlay (2D, drawn last)
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0,windowWidth,0,windowHeight);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    const char* lines[]={
        "=== Controls ===",
        "M - Toggle Moon Lock",
        "P - Pause Animation",
        "B - Debug (camera position / speed)",
        "WASD - Move Camera (level)",
        "Mouse - Look Around",
        "R/0 - Reset (above Sun)",
        "O - Overview / True-scale view",
        "1-8 - Jump to Planet (Mercury..Neptune)",
        "+/- - Moon Distance (Lock mode)",
        "Space/Shift - Move Up/Down",
        "ESC - Exit"
    };
    int n=sizeof(lines)/sizeof(*lines);
    for(int i=0;i<n;i++){
        glRasterPos2f(10, 10+(n-1-i)*15);
        for(const char* c=lines[i];*c;c++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13,*c);
    }
    if(debug){   // camera readout, top-left
        char dbg[2][96];
        sprintf(dbg[0],"Camera: %.1f, %.1f, %.1f",camX,camY,camZ);
        if(lockToMoon) sprintf(dbg[1],"Speed: n/a (Moon lock)");
        else           sprintf(dbg[1],"Speed: %.2f units/s",camSpeed*1000/16);   // update() runs every 16 ms
        for(int i=0;i<2;i++){
            glRasterPos2f(10, windowHeight-20-i*15);
            for(const char* c=dbg[i];*c;c++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13,*c);
        }
    }
    glEnable(GL_LIGHTING);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}

// ---- Input ----
// Held keys are tracked (keyboard = pressed, keyboardUp = released) and applied every frame in update(),
// so movement is smooth instead of depending on the operating system's key repeat.
void keyboard(unsigned char key,int,int){
    if(key>='A'&&key<='Z') key+=32;   // treat Shift+letter as the letter
    if(key=='+') key='=';             // '+' and '=' are the same key
    if(lockToMoon && key!='m' && key!='p' && key!='b' && key!='=' && key!='-' && key!=27) return;
    keys[key]=true;

    switch(key){   // one-shot actions; W/A/S/D/Space/+/- are held keys handled in update()
        case 'o': overview=!overview;   // then fall through to reset the camera for the new mode
        case 'r': case '0':
            camX=0; camY=overview?OV_VIEW_H:3*SUN_R; camZ=0; yaw=-90; pitch=-89; break;
        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': {
            Planet &pl=planets[key-'1'];                       // 1=Mercury ... 3=Earth ... 8=Neptune
            float px,py,pz; getPlanetPos(pl.distance,px,py,pz);
            camX=px; camY=pl.radius*STOP_MULT; camZ=pz; yaw=-90; pitch=-89;   // directly above, at the stop distance
            break;
        }
        case 'm': lockToMoon=!lockToMoon; break;
        case 'p': animationPaused=!animationPaused; break;
        case 'b': debug=!debug; break;
        case 27:  exit(0);
    }
    physics();   // apply the stop distance immediately
    glutPostRedisplay();
}

void keyboardUp(unsigned char key,int,int){
    if(key>='A'&&key<='Z') key+=32;
    if(key=='+') key='=';
    keys[key]=false;
}

// Shift on its own moves the camera down while held (FreeGLUT reports it as a special key; update() does the
// moving every frame).
void special(int key,int,int){
    if(key==GLUT_KEY_SHIFT_L || key==GLUT_KEY_SHIFT_R) shiftHeld=true;
}

void specialUp(int key,int,int){
    if(key==GLUT_KEY_SHIFT_L || key==GLUT_KEY_SHIFT_R) shiftHeld=false;
}

void mouseMotion(int x,int y){
    if(lockToMoon) return;

    yaw  +=(x-windowWidth/2 )*mouseSensitivity;
    pitch+=(windowHeight/2-y)*mouseSensitivity;
    pitch=pitch>89?89:pitch<-89?-89:pitch;

    glutWarpPointer(windowWidth/2,windowHeight/2);
}

void update(int){
    camSpeed=moveStep*0.5f;   // per frame: about the rate of a repeating key at 60 FPS
    if(!lockToMoon){
        float fx=cos(toRad(yaw)),    fz=sin(toRad(yaw));      // forward, flattened onto the horizontal plane
        float rx=cos(toRad(yaw-90)), rz=sin(toRad(yaw-90));   // sideways
        float f=keys['w']-keys['s'], r=keys['d']-keys['a'], u=keys[' ']-shiftHeld;   // each -1, 0 or +1
        camX+=camSpeed*(f*fx-r*rx);   // W/A/S/D follow where you look but never change height (Y)
        camZ+=camSpeed*(f*fz-r*rz);
        camY+=camSpeed*u;         // Space up, Shift down
    } else {
        moonCamDistance+=(keys['-']-keys['='])*0.02f*E;   // '-' moves the camera toward Earth, '+' toward the Moon
        if(moonCamDistance>1.5f*E) moonCamDistance=1.5f*E;
        if(moonCamDistance<0.5f*E) moonCamDistance=0.5f*E;
    }
    physics();   // planets move, so re-check the camera against them every frame
    if(!animationPaused){
        moonAngle +=moonOrbitSpeed*(0.5f+0.5f*tScale);  if(moonAngle <=-360) moonAngle +=360;   // Moon slows to half speed at most
        earthAngle+=earthRotationSpeed;     if(earthAngle>=360) earthAngle-=360;
        earthOrbitAngle+=earthOrbitSpeed*tScale;
    }
    glutPostRedisplay();
    glutTimerFunc(16,update,0);
}

void reshape(int w,int h){
    windowWidth=w; windowHeight=h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(60.0,(float)w/h,0.26,261000.0);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc,char** argv){
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
    glutInitWindowSize(windowWidth,windowHeight);
    glutCreateWindow("Solar System");

    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_NORMALIZE);
    glLightfv(GL_LIGHT0,GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0,GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR,lightSpecular);
    glClearColor(0,0,0,1);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);   // texture colour x lighting
    for(int i=0;i<8;i++) planets[i].id=loadTexture(planets[i].file);
    moonTex=loadTexture("moon.jpg"); sunTex=loadTexture("sun.jpg"); ringTex=loadTexture("saturn_ring_alpha.png"); starTex=loadTexture("stars_milky_way.jpg");
    glutSetCursor(GLUT_CURSOR_NONE);
    glutWarpPointer(windowWidth/2,windowHeight/2);

    glutDisplayFunc(display);
    glutIgnoreKeyRepeat(1);                    // held keys are tracked, so ignore the OS auto-repeat
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(special);
    glutSpecialUpFunc(specialUp);
    glutPassiveMotionFunc(mouseMotion);
    glutReshapeFunc(reshape);
    glutTimerFunc(0,update,0);
    glutMainLoop();
}