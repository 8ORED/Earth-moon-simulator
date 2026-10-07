#include <GL/glut.h>
#include <stdlib.h>
#include <math.h>
#define PI 3.14159265358979323846f
#define toRad(d) ((d)*PI/180.0f)

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
bool  animationPaused=false, lockToMoon=false, overview=true;   // overview = default view
bool  shiftHeld=false;                     // modifier keys don't auto-repeat, so Shift is tracked as held/released
bool  keys[256];                           // which keys are currently held (lower case)
int   windowWidth=800, windowHeight=600;

// ---- Planets (Earth is in the table; its Moon is drawn separately) ----
// radius (Mercury=1), distance from Sun (AU x scale above), diffuse RGB
struct Planet{ float radius, distance, r,g,b; };
Planet planets[]={
    { 1.000f,  0.387f*AU, 0.60f,0.60f,0.60f},   // Mercury
    { 2.478f,  0.723f*AU, 0.90f,0.80f,0.50f},   // Venus
    {     E,   1.000f*AU, 0.20f,0.40f,1.00f},   // Earth
    { 1.389f,  1.524f*AU, 0.80f,0.30f,0.15f},   // Mars
    {29.270f,  5.203f*AU, 0.80f,0.65f,0.50f},   // Jupiter
    {24.670f,  9.580f*AU, 0.90f,0.80f,0.55f},   // Saturn
    {10.470f, 19.200f*AU, 0.55f,0.85f,0.90f},   // Uranus
    {10.130f, 30.100f*AU, 0.25f,0.35f,0.90f}    // Neptune
};

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

void drawSphere(float r,int stacks,int slices){
    for(int i=0;i<stacks;i++){
        float t1=i*PI/stacks, t2=(i+1)*PI/stacks;
        glBegin(GL_TRIANGLES);
        for(int j=0;j<slices;j++){
            float p1=j*2*PI/slices, p2=(j+1)*2*PI/slices;
            float v[4][3]={
                {r*sin(t1)*cos(p1), r*cos(t1), r*sin(t1)*sin(p1)},
                {r*sin(t2)*cos(p1), r*cos(t2), r*sin(t2)*sin(p1)},
                {r*sin(t2)*cos(p2), r*cos(t2), r*sin(t2)*sin(p2)},
                {r*sin(t1)*cos(p2), r*cos(t1), r*sin(t1)*sin(p2)}
            };
            // Triangle 1: 0,1,2  Triangle 2: 0,2,3
            int idx[]={0,1,2, 0,2,3};
            for(int k=0;k<6;k++){
                int n=idx[k];
                glNormal3f(v[n][0]/r, v[n][1]/r, v[n][2]/r);
                glVertex3f(v[n][0],   v[n][1],   v[n][2]);
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

    // Planets (including Earth): each revolves around the Sun and spins
    float pos[8][3];
    for(int i=0;i<8;i++){
        Planet &p=planets[i];
        getPlanetPos(p.distance,pos[i][0],pos[i][1],pos[i][2]);
        GLfloat pd[]={p.r,p.g,p.b,1}, ps[]={0.3f,0.3f,0.3f,1};
        glMaterialfv(GL_FRONT,GL_DIFFUSE,pd);
        glMaterialfv(GL_FRONT,GL_SPECULAR,ps);
        glMaterialf (GL_FRONT,GL_SHININESS,30);
        glPushMatrix(); glTranslatef(pos[i][0],pos[i][1],pos[i][2]); glRotatef(earthAngle,0,1,0);
        drawSphere(p.radius,30,30); glPopMatrix();

        if(i==5){   // Saturn: flat ring, tilted, lit on both sides (a top and a bottom layer, back faces culled)
            GLfloat rd[]={0.85f,0.75f,0.55f,1}, rs[]={0.1f,0.1f,0.1f,1};
            glMaterialfv(GL_FRONT,GL_DIFFUSE,rd);
            glMaterialfv(GL_FRONT,GL_SPECULAR,rs);
            glMaterialf (GL_FRONT,GL_SHININESS,10);
            glPushMatrix(); glTranslatef(pos[i][0],pos[i][1],pos[i][2]); glRotatef(RING_TILT,1,0,0);
            glEnable(GL_CULL_FACE);
            for(int s=0;s<2;s++){                         // s=0: top face (normal +Y), s=1: bottom face (normal -Y)
                float a0=(s?RING_IN:RING_OUT)*p.radius, a1=(s?RING_OUT:RING_IN)*p.radius;   // vertex order sets the facing
                glNormal3f(0,s?-1:1,0);
                glBegin(GL_QUAD_STRIP);
                for(int k=0;k<=96;k++){
                    float a=2*PI*k/96;
                    glVertex3f(a0*cos(a),0,a0*sin(a));
                    glVertex3f(a1*cos(a),0,a1*sin(a));
                }
                glEnd();
            }
            glDisable(GL_CULL_FACE);
            glPopMatrix();
        }
    }

    // Overview mode: planets are under a pixel at this zoom, so mark each with a small coloured dot
    if(overview){
        glDisable(GL_LIGHTING); glPointSize(4);
        glBegin(GL_POINTS);
        for(int i=0;i<8;i++){
            glColor3f(planets[i].r,planets[i].g,planets[i].b);
            glVertex3f(pos[i][0],pos[i][1],pos[i][2]);
        }
        glEnd(); glEnable(GL_LIGHTING);
    }

    // Moon
    GLfloat md[]={0.7f,0.7f,0.7f,1}, ms[]={0.9f,0.9f,0.9f,1};
    glMaterialfv(GL_FRONT,GL_DIFFUSE,md);
    glMaterialfv(GL_FRONT,GL_SPECULAR,ms);
    glMaterialf (GL_FRONT,GL_SHININESS,80);
    // Tidally locked: it turns once per orbit (same angle, opposite sense), so its -X face always points at Earth
    glPushMatrix(); glTranslatef(moonX,moonY,moonZ); glRotatef(-moonAngle,0,1,0); drawSphere(0.27f*E,20,20); glPopMatrix();

    // Sun (drawn at the light position, unlit so it looks self-illuminated)
    glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.9f,0.2f);
    glPushMatrix(); glTranslatef(lightPos[0],lightPos[1],lightPos[2]); drawSphere(SUN_R,60,60); glPopMatrix();

    // Controls overlay (2D, drawn last)
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0,windowWidth,0,windowHeight);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    const char* lines[]={
        "=== Controls ===",
        "M - Toggle Moon Lock",
        "P - Pause Animation",
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
    if(lockToMoon && key!='m' && key!='p' && key!='=' && key!='-' && key!=27) return;
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
    float step=moveStep*0.5f;   // per frame: about the rate of a repeating key at 60 FPS
    if(!lockToMoon){
        float fx=cos(toRad(yaw)),    fz=sin(toRad(yaw));      // forward, flattened onto the horizontal plane
        float rx=cos(toRad(yaw-90)), rz=sin(toRad(yaw-90));   // sideways
        float f=keys['w']-keys['s'], r=keys['d']-keys['a'], u=keys[' ']-shiftHeld;   // each -1, 0 or +1
        camX+=step*(f*fx-r*rx);   // W/A/S/D follow where you look but never change height (Y)
        camZ+=step*(f*fz-r*rz);
        camY+=step*u;             // Space up, Shift down
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