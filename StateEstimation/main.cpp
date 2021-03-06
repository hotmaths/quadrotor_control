#include <iostream>
#include <stdio.h>
#include <math.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "Eigen/Core"

#include "KalmanFilter.h"
#include "DrawUtilities.h"
#include "SerialReader.h"

using namespace std;
using namespace cv;
using namespace Eigen;

Point2f mousePos(0,0);

const float timeStep = 0.01f;
const float initParticleVelocity = 10;
const float gravityMag = 1000;
const float meas_noise = 1;
const int particleCount = 5;

bool pauseSim = false;
bool stepSim = false;
bool hideTruth = true;
bool hideEstimate = false;

int width = 800;
int height = 800;
int serial_fd = 0;


void onMouse(int event, int x, int y, int flags, void * data){
    mousePos.x = x;
    mousePos.y = y;
}

Point toPoint(const Vector2f v, Point & p) {
    p.x = v[0];
    p.y = v[1];
    return p;
}

class Patricle {
public:

    Matrix<float,3,2> state; //pos,vel,accel for 2 independent axis
    Matrix<float,3,3> updateFunction;//really should only be 3x3 and called on different chunks
    float dt;
    float drag;

    void Init(float x, float y, float v, float dt) {
        this->dt = dt;

        //x' = x + v*dt + a*0.5*dt*dt
        //v' = v + a*dt
        //a' = a

        drag = 0.998;

        updateFunction <<
        1,dt,0.5*dt*dt,
        0,drag,dt,
        0,0,1;

        setPos(x,y);

        float m = randf();
        setVel(v*cos(2*M_PI*m),v*sin(2*M_PI*m));
    }

    void ComputeGravity(float x, float y, float g) {
        float dx = state(0,0) - x;
        float dy = state(0,1) - y;

        float mag = sqrt(dx*dx + dy*dy);

        dx /= mag;
        dy /= mag;

        setAccel(-dx*g/(mag + 10),-dy*g/(mag + 10));
    }

    void setPos(float x, float y) {
        state(0,0) = x;
        state(0,1) = y;
    }
    void setVel(float x, float y) {
        state(1,0) = x;
        state(1,1) = y;
    }
    void setAccel(float x, float y) {
        state(2,0) = x;
        state(2,1) = y;
    }


    Point getPos() {
        Point p;
        p.x = state(0,0);
        p.y = state(0,1);
        return p;
    }
    Point getVel() {
        Point p;
        p.x = state(1,0);
        p.y = state(1,1);
        return p;
    }

    Point getAccel() {
        Point p;
        p.x = state(2,0);
        p.y = state(2,1);
        return p;
    }


    void Update() {
        state = updateFunction*state;
    }
};

Patricle particles[particleCount];
KalmanFilter2DPosVelAccel kf[particleCount];

KalmanFilterGyro kf_gyro;


bool ParseSerial(Matrix<float,3,1> & values){


    if (serial_fd <= 0) {
        printf("Cannot read serial\n");
        return false;
    }

    unsigned char b;
    read(serial_fd, &b,1);

    if(b != 170) {
//        printf("No sync byte\n", b);
        return false;
    }

    values(0) = serial_read_Nbytes(serial_fd,2);
    values(1) = serial_read_Nbytes(serial_fd,2);
    values(2) = serial_read_Nbytes(serial_fd,2);

    return true;
}

int main(int argc, char** argv)
{
    printf("StateEstimation\n");

    Mat img(width, height,CV_8UC3, Scalar(0,0,0));
    char * windowName = "Image";
    namedWindow(windowName);

    setMouseCallback(windowName, onMouse);

    for(int i = 0; i < particleCount; i++) {
        particles[i].Init(width*randf(),height*randf(),initParticleVelocity,timeStep);
    }


    Matrix<float,2,2> Ez;
    Ez << meas_noise*meas_noise, 0,
          0, meas_noise*meas_noise;
    Matrix<float,2,1> z;


    Matrix<float,1,1> u;
    u << 0.0f;

    Point p1;
    Point p2;

    serial_fd = init_serial_input("/dev/ttyACM0");

    while(1) {
        if(ParseSerial(kf_gyro.raw_data_sample)) {
            kf_gyro.UpdateRawDataSample(timeStep);
        }

//        char c = waitKey(5);
//        if(c == 27)
//            break;
//        if(c == 'r') {
//            for(int i = 0; i < particleCount; i++) {
//                kf[i].ResetEstimation();
//            }
//        }
//        if(c == ' ')
//            pauseSim = !pauseSim;
//        if(c == 'h')
//            hideTruth = !hideTruth;
//        if(c == 'e')
//            hideEstimate = !hideEstimate;
//        if(c == '.')
//            stepSim = true;

//        if(pauseSim & !stepSim)
//            continue;
//        stepSim = false;

//        printf("*************\n");

//        img.setTo(Scalar(0,0,0));
//        DrawCrossHair(img, mousePos,5, Scalar(0,0,255));
//        for(int i = 0; i < particleCount; i++) {

//            //update particle
//            particles[i].ComputeGravity(width/2, height/2,gravityMag);
//            particles[i].Update();

//            if(!hideTruth) {
//                p1 = particles[i].getPos();
//                p2 = p1 - particles[i].getVel();
//                DrawCrossHair(img, p1,3, Scalar(255,255,255));
//                line(img, p1,p2, Scalar(255,255,255));
//            }

//            //update kalman filter
//            z <<    (particles[i].state(0,0) + randfGaussian(0,meas_noise)),
//                    (particles[i].state(0,1) + randfGaussian(0,meas_noise));
//            DrawCrossHair(img, toPoint(z,p1),3, Scalar(0,255,255));

//            kf[i].Update(z,Ez,u,timeStep);

//            if(!hideEstimate){

////                printMatrix("P", kf[i].Covariance());

//                p2 = toPoint(kf[i].Position(),p1) - toPoint(kf[i].Velocity(),p2);
//                circle(img, p1,4,Scalar(0,255,0));
//                line(img, p1, p2, Scalar(0,255,0));

//                for(float t = 0; t < 2; t+= 2*timeStep) {
//                    toPoint(kf[i].PredictedPosition(t),p2);
//                    line(img, p1, p2, Scalar(0,128,0));
//                    p1 = p2;
//                }
//            }
//        }

//        imshow(windowName, img);
    }

    return 0;
}
