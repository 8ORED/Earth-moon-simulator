#include <GL/glut.h>
#include <math.h>
#define PI 3.14159265358979323846f
#define toRad(d) ((d)*PI/180.0f)

// ---- State ----
float moonAngle=0, earthAngle=0, moonOrbitRadius=3;
float moonCamDistance=2, moonCamHeight=0.5f;
float moonOrbitSpeed=-0.5f, earthRotationSpeed=2;
bool  animationPaused=false, lockToMoon=false;
int   windowWidth=800, windowHeight=600;

// ---- Camera ----
float camX=0,camY=0,camZ=8, yaw=-90,pitch=0;
float camFrontX=0,camFrontY=0,camFrontZ=-1;
float mouseSensitivity=0.1f, cameraSpeed=0.2f;

// ---- Lighting ----
GLfloat lightPos[]     = {10,0,0,1};
GLfloat lightAmbient[] = {0.2f,0.2f,0.2f,1};
GLfloat lightDiffuse[] = {0.9f,0.9f,0.9f,1};
GLfloat lightSpecular[]= {1,1,1,1};

// ---- Helpers ----
void getMoonPos(float &x,float &y,float &z){
    float r=toRad(moonAngle);
    x=moonOrbitRadius*cos(r); y=0; z=moonOrbitRadius*sin(r);
}

void updateCam(){
    float p=toRad(pitch), yw=toRad(yaw);
    float len;
    camFrontX=cos(p)*cos(yw);
    camFrontY=sin(p);
    camFrontZ=cos(p)*sin(yw);
    len=sqrt(camFrontX*camFrontX+camFrontY*camFrontY+camFrontZ*camFrontZ);
    if(len>0){ camFrontX/=len; camFrontY/=len; camFrontZ/=len; }
}

void setMat(const GLfloat* d,const GLfloat* s,float sh){
    glMaterialfv(GL_FRONT,GL_DIFFUSE,d);
    glMaterialfv(GL_FRONT,GL_SPECULAR,s);
    glMaterialf (GL_FRONT,GL_SHININESS,sh);
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

void drawText(float x,float y,const char* t){
    glRasterPos2f(x,y);
    while(*t) glutBitmapCharacter(GLUT_BITMAP_8_BY_13,*t++);
}

void drawOverlay(){
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0,windowWidth,0,windowHeight);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    glDisable(GL_LIGHTING);

    const char* lines[]={
        "=== Controls ===",
        "M - Toggle Moon Lock",
        "P - Pause Animation",
        "WASD - Move Camera",
        "Mouse - Look Around",
        "R - Reset Camera",
        "+/- - Moon Distance (Lock mode)",
        "Space/E - Move Up/Down",
        "ESC - Exit"
    };
    int n=sizeof(lines)/sizeof(*lines);
    for(int i=0;i<n;i++)
        drawText(10, 10+(n-1-i)*15, lines[i]);

    glEnable(GL_LIGHTING);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ---- Display ----
void display(){
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    float moonX,moonY,moonZ, lx,ly,lz;
    getMoonPos(moonX,moonY,moonZ);

    if(lockToMoon){
        float r=toRad(moonAngle);
        camX=moonX-cos(r)*moonCamDistance;
        camY=moonY+moonCamHeight;
        camZ=moonZ-sin(r)*moonCamDistance;
        lx=moonX; ly=moonY; lz=moonZ;
    } else {
        updateCam();
        lx=camX+camFrontX; ly=camY+camFrontY; lz=camZ+camFrontZ;
    }

    gluLookAt(camX,camY,camZ, lx,ly,lz, 0,1,0);
    glLightfv(GL_LIGHT0,GL_POSITION,lightPos);

    // Earth
    GLfloat ed[]={0.2f,0.4f,1,1}, es[]={0.3f,0.3f,0.3f,1};
    setMat(ed,es,30);
    glPushMatrix(); glRotatef(earthAngle,0,1,0); drawSphere(1,30,30); glPopMatrix();

    // Moon
    GLfloat md[]={0.7f,0.7f,0.7f,1}, ms[]={0.9f,0.9f,0.9f,1};
    setMat(md,ms,80);
    glPushMatrix(); glTranslatef(moonX,moonY,moonZ); drawSphere(0.27f,20,20); glPopMatrix();

    // Sun (drawn at the light position, unlit so it looks self-illuminated)
    glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.9f,0.2f);
    glPushMatrix(); glTranslatef(lightPos[0],lightPos[1],lightPos[2]); drawSphere(0.8f,30,30); glPopMatrix();
    glEnable(GL_LIGHTING);

    drawOverlay();
    glutSwapBuffers();
}

// ---- Input ----
void keyboard(unsigned char key,int,int){
    if(lockToMoon && key!='m' && key!='M' && key!='p' && key!='P' && key!='+' && key!='=' && key!='-' && key!=27) return;

    float rx=cos(toRad(yaw-90)), rz=sin(toRad(yaw-90));
    switch(key){
        case 'w': case 'W': camX+=cameraSpeed*camFrontX; camY+=cameraSpeed*camFrontY; camZ+=cameraSpeed*camFrontZ; break;
        case 's': case 'S': camX-=cameraSpeed*camFrontX; camY-=cameraSpeed*camFrontY; camZ-=cameraSpeed*camFrontZ; break;
        case 'd': case 'D': camX-=cameraSpeed*rx; camZ-=cameraSpeed*rz; break;
        case 'a': case 'A': camX+=cameraSpeed*rx; camZ+=cameraSpeed*rz; break;
        case ' ':            camY+=cameraSpeed; break;
        case 'e': case 'E': camY-=cameraSpeed; break;
        case 'r': case 'R': camX=0; camY=0; camZ=8; yaw=-90; pitch=0; updateCam(); break;
        case 'm': case 'M': lockToMoon=!lockToMoon; break;
        case 'p': case 'P': animationPaused=!animationPaused; break;
        case '-':            moonCamDistance+=0.2f; break;
        case '+': case '=':  moonCamDistance-=0.2f; if(moonCamDistance<0.5f)moonCamDistance=0.5f; break;
        case 27:             exit(0);
    }
    glutPostRedisplay();
}

void mouseMotion(int x,int y){
    if(lockToMoon) return;

    yaw  +=(x-windowWidth/2 )*mouseSensitivity;
    pitch+=(windowHeight/2-y)*mouseSensitivity;
    pitch=pitch>89?89:pitch<-89?-89:pitch;

    updateCam();
    glutWarpPointer(windowWidth/2,windowHeight/2);
}

void update(int){
    if(!animationPaused){
        moonAngle +=moonOrbitSpeed;  if(moonAngle >=360) moonAngle -=360;
        earthAngle+=earthRotationSpeed; if(earthAngle>=360) earthAngle-=360;
    }
    glutPostRedisplay();
    glutTimerFunc(16,update,0);
}

// ---- Init / Reshape / Main ----
void init(){
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_NORMALIZE);
    glLightfv(GL_LIGHT0,GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0,GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR,lightSpecular);
    glClearColor(0,0,0,1);
    updateCam();
    glutSetCursor(GLUT_CURSOR_NONE);
}

void reshape(int w,int h){
    windowWidth=w; windowHeight=h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(60.0,(float)w/h,1.0,50.0);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc,char** argv){
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
    glutInitWindowSize(windowWidth,windowHeight);
    glutCreateWindow("Earth-Moon Phases");
    init();
    glutWarpPointer(windowWidth/2,windowHeight/2);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(mouseMotion);
    glutReshapeFunc(reshape);
    glutTimerFunc(0,update,0);
    glutMainLoop();
}