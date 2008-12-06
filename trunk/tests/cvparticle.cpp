#ifdef _MSC_VER
#pragma warning(disable:4996)
#pragma comment(lib, "cv.lib")
#pragma comment(lib, "cxcore.lib")
#pragma comment(lib, "cvaux.lib")
#pragma comment(lib, "highgui.lib")
#endif

#include <stdio.h>
#include <stdlib.h>
#include "cv.h"
#include "cvaux.h"
#include "cxcore.h"
#include "highgui.h"

#include "../cvxmat.h"
#include "../cvxrectangle.h"
#include "../cvcropimageroi.h"
#include "../cvdrawrectangle.h"
#include "../cvskincolorpeer.h"
#include <time.h>

// General particle filter structure
#include "../cvparticle.h"
// Rotated rectangle + 2nd AR model particle filter
#include "../cvparticlerotrect2.h"

/****************************** Global *****************************/

extern int num_states; // @cvparticlerotrect2.h
int num_particles = 1000;
CvSize feature_size = cvSize(24, 24);

/******************************* Structures ************************/

typedef struct IcvMouseParam {
    CvPoint loc1;
    CvPoint loc2;
    char* win_name;
    IplImage* frame;
} IcvMouseParam;

/**************************** Function Prototypes ******************/

void icvGetRegion( IplImage*, CvRect* );
void icvMouseCallback( int, int, int, int, void* );
void cvParticleObserveLikelihood( CvParticle* p, IplImage* cur_frame, IplImage *pre_frame );

/*************************** Observation model *********************/

// kinds of template matching based likelihood measurement
void cvParticleObserveLikelihood( CvParticle* p, IplImage* frame, IplImage *reference )
{
    int i;
    double likeli;
    CvRect rect;
    double rotate;
    IplImage *patch;
    IplImage *resize = cvCreateImage( feature_size, frame->depth, frame->nChannels );
    for( i = 0; i < p->num_particles; i++ ) 
    {
        CvParticleState s = cvParticleStateGet( p, i );
        rect   = cvRect( cvRound( s.x ), cvRound( s.y ), cvRound( s.width ), cvRound( s.height ) );
        rotate = s.rotate;
        
        patch = cvCreateImage( cvSize(rect.width,rect.height), frame->depth, frame->nChannels );
        cvCropImageROI( frame, patch, rect, rotate );
        cvResize( patch, resize );

        // log likeli. kinds of Gaussian model
        // exp( -d^2 / sigma^2 )
        // sigma can be omitted because common param does not affect ML estimate
        likeli = -cvNorm( resize, reference, CV_L2 ); 
        cvmSet( p->probs, 0, i, likeli );
        
        cvReleaseImage( &patch );
    }
    cvReleaseImage( &resize );
}

/**************************** Main ********************************/
int main( int argc, char** argv )
{
    IplImage *frame, *reference, *tmp;
    char* vid_file;
    CvCapture* video;
    int i;

    // arg_parse
    if( argc > 1 ) {
        vid_file = argv[1];
    } else {
        fprintf( stderr, "./cvparticle <vid_file>\n" );
        exit( 1 );
    }

    // read a video
    video = cvCaptureFromFile( vid_file );
    if( ! video ) {
        fprintf( stderr, "couldn't open video file %s", vid_file );
        exit( 1 );
    }
    frame = cvQueryFrame( video );

    // allows user to select initial region
    printf( "Select a region to record an appearance to track\n" );
    CvRect region;
    icvGetRegion( frame, &region );

    // configure particle filter
    bool logprob = true;
    CvParticle *particle = cvCreateParticle( num_states, 1, num_particles, logprob );
    CvParticleState std = cvParticleState (
        3.0,
        3.0,
        2.0,
        2.0,
        1.0,
        0, 0, 0, 0, 0
    );
    cvParticleStateConfig( particle, cvGetSize(frame), std );

    // initialize particle filter
    CvParticleState s;
    CvParticle *init_particle;
    init_particle = cvCreateParticle( num_states, 1, 1 );
    region = cvConvRect( region, 0.0, CV_RECT_NORMAL, CV_RECT_CENTER );
    s = cvParticleState( region.x, region.y, region.width, region.height, 0.0,
                         region.x, region.y, region.width, region.height, 0.0 );
    cvParticleStateSet( init_particle, 0, s );
    cvParticleInit( particle, init_particle );
    cvReleaseParticle( &init_particle );

    // template 
    reference = cvCreateImage( feature_size, frame->depth, frame->nChannels );
    tmp = cvCreateImage( cvSize(region.width,region.height), frame->depth, frame->nChannels );
    cvCropImageROI( frame, tmp, region );
    cvResize( tmp, reference );
    cvReleaseImage( &tmp );

    while( frame = cvQueryFrame( video ) )
    {
        // state transition (and bouding inside)
        cvParticleTransition( particle );

        // likelihood measurements
        cvParticleObserveLikelihood( particle, frame, reference );

        // draw all particles
        cvParticleStateDraw( particle, frame, CV_RGB(0,0,255), -1 );
        // draw most probable particles
        int maxp = cvParticleMaxParticle( particle ); 
        cvParticleStateDraw( particle, frame, CV_RGB(255,0,0), maxp );
        printf( "Most probable particle's state w/ prob:" );
        double maxprob = cvmGet( particle->particle_probs, 0, maxp );
        printf( "%f\n", logprob ? exp( maxprob ) : maxprob );
        cvParticlePrint( particle, maxp );

        // resampling
        cvParticleResample( particle );
        //cvPrintMat( particle->particle_probs );

        cvShowImage( "Show", frame );
        char c = cvWaitKey( 1000 );
        if(c == '\x1b')
            break;
    }
    cvDestroyWindow( "Show" );
    cvReleaseImage( &tmp );
    cvReleaseImage( &reference );
    cvReleaseParticle( &particle );
    return 0;
}

/**
 * Allows the user to interactively select the initial object region
 *
 * @param frame  The frame of video in which objects are to be selected
 * @param region A pointer to an array to be filled with rectangles
 */
void icvGetRegion( IplImage* frame, CvRect* region )
{
    IcvMouseParam p;

    /* use mouse callback to allow user to define object regions */
    p.win_name = "Show";
    p.frame = frame;

    cvNamedWindow( p.win_name, 1 );
    cvShowImage( p.win_name, frame );
    cvSetMouseCallback( p.win_name, &icvMouseCallback, &p );
    cvWaitKey( 0 );
    //cvDestroyWindow( win_name );

    /* extract regions defined by user; store as a rectangle */
    region->x      = min( p.loc1.x, p.loc2.x );
    region->y      = min( p.loc1.y, p.loc2.y );
    region->width  = max( p.loc1.x, p.loc2.x ) - region->x + 1;
    region->height = max( p.loc1.y, p.loc2.y ) - region->y + 1;
}

/**
 * Mouse callback function that allows user to specify the 
 * initial object region. 
 * Parameters are as specified in OpenCV documentation.
 */
void icvMouseCallback( int event, int x, int y, int flags, void* param )
{
    IcvMouseParam* p = (IcvMouseParam*)param;
    IplImage* clone;
    static int pressed = FALSE;

    /* on left button press, remember first corner of rectangle around object */
    if( event == CV_EVENT_LBUTTONDOWN )
    {
        p->loc1.x = x;
        p->loc1.y = y;
        pressed = TRUE;
    }

    /* on left button up, finalize the rectangle and draw it */
    else if( event == CV_EVENT_LBUTTONUP )
    {
        p->loc2.x = x;
        p->loc2.y = y;
        clone = (IplImage*)cvClone( p->frame );
        cvRectangle( clone, p->loc1, p->loc2, CV_RGB(255,255,255), 1, 8, 0 );
        cvShowImage( p->win_name, clone );
        cvReleaseImage( &clone );
        pressed = FALSE;
    }

    /* on mouse move with left button down, draw rectangle */
    else if( event == CV_EVENT_MOUSEMOVE  &&  flags & CV_EVENT_FLAG_LBUTTON )
    {
        clone = (IplImage*)cvClone( p->frame );
        cvRectangle( clone, p->loc1, cvPoint(x, y), CV_RGB(255,255,255), 1, 8, 0 );
        cvShowImage( p->win_name, clone );
        cvReleaseImage( &clone );
    }
}
