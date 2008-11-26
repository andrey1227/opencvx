/** @file
// cvxpca.h
//
// Copyright (c) 2008, Naotoshi Seo. All rights reserved.
//
// The program is free to use for non-commercial academic purposes,
// but for course works, you must understand what is going inside to 
// use. The program can be used, modified, or re-distributed for any 
// purposes only if you or one of your group understand not only 
// programming codes but also theory and math behind (if any). 
// Please contact the authors if you are interested in using the 
// program without meeting the above conditions.
*/
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4996 )
#pragma comment( lib, "cv.lib" )
#pragma comment( lib, "cxcore.lib" )
#pragma comment( lib, "cvaux.lib" )
#endif

#include "cv.h"
#include "cvaux.h"
#include <stdio.h>
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>

#ifndef CV_PCADIFFS_INCLUDED
#define CV_PCADIFFS_INCLUDED

//void cvCalcPCA( const CvArr* data, CvArr* avg,
//                CvArr* eigenvalues, CvArr* eigenvectors, int flags );
//void cvProjectPCA( const CvArr* data, const CvArr* avg,
//                   const CvArr* eigenvectors, CvArr* result );
//void cvBackProjectPCA( const CvArr* proj, const CvArr* avg,
//                       const CvArr* eigenvects, CvArr* result );
void cvMatPcaDiffs( const CvMat* samples, const CvMat* avg, const CvMat* eigenvalues, 
                    const CvMat* eigenvectors, CvMat* probs, 
                    bool normalize = false, bool logprob = true );
double cvPcaDiffs( const CvMat* sample, const CvMat* avg, const CvMat* eigenvalues, 
                   const CvMat* eigenvectors, bool normalize = false, bool logprob = true );

/**
 * cvPcaDiffs - Distance "in" and "from" feature space [1]
 *
 * Synopsis
 *   [p] = cvPcaDiffs(X, U, Me, Lambda, normalize, logprob, lambdapad)
 *
 * Description
 *   This computes a distance between a point to a PCA subspace as sum of
 *   distance-from-feature space (DFFS) and distance-in-feature-space
 *   (DIFS). The DFFS is essentially a reconstruction error and the
 *   DIFS measures (mahalanobis) distance between projected point and 
 *   origin of PCA subpsace. DFFS and DIFS can be used to measure 
 *   approximated likelihood Gaussian density distribution. 
 *   See [1] for more details. 
 *
 * @param samples             D x N sample vectors
 * @param avg                 D x 1 mean vector
 * @param eigenvalues         nEig x 1 eigen values
 * @param eigenvectors        M x D eigen vectors
 * @param probs               1 x N computed likelihood probabilities
 * @param [normalize = false] Compute normalization term or not
 * @param [logprob   = false] Log probability or not
 *
 * References
 *   [1] @INPROCEEDINGS{Moghaddam95probabilisticvisual,
 *     author = {Baback Moghaddam and Alex Pentl},
 *     title = {Probabilistic visual learning for object detection},
 *     booktitle = {},
 *     year = {1995},
 *     pages = {786--793}
 *   }
 *   [2] @ARTICLE{Moghaddam02principalmanifolds,
 *     author = {Baback Moghaddam},
 *     title = {Principal manifolds and probabilistic subspaces for visual recognition},
 *     journal = {IEEE Transactions on Pattern Analysis and Machine Intelligence},
 *     year = {2002},
 *     volume = {24},
 *     pages = {780--788}
 *
 * Authors
 *   Naotoshi Seo <sonots(at)sonots.com>
 *
 * License
 *   The program is free to use for non-commercial academic purposes,
 *   but for course works, you must understand what is going inside to use.
 *   The program can be used, modified, or re-distributed for any purposes
 *   if you or one of your group understand codes (the one must come to
 *   court if court cases occur.) Please contact the authors if you are
 *   interested in using the program without meeting the above conditions.
 *
 * Changes
 *   11/18/2008  First Edition
 */
void cvMatPcaDiffs( const CvMat* samples, const CvMat* avg, const CvMat* eigenvalues, 
                    const CvMat* eigenvectors, CvMat* probs, bool normalize, bool logprob )
{
    int D = samples->rows;
    int N = samples->cols;
    int M = eigenvectors->rows;
    int type = samples->type;
    int nEig = eigenvalues->rows;
    int d, n;
    double normterm = 0;
    CvMat *proj = cvCreateMat( N, M, type );
    CvMat *subproj = cvCreateMat( 1, M, type );
    CvMat *scsubproj = cvCreateMat( 1, M, type );
    CvMat *sqrt_pLambda = cvCreateMat( M, 1, type );
    CvMat *rLambda = cvCreateMat( nEig - M, 1, type );
    CvMat *DIFS = cvCreateMat( 1, N, type );
    CvMat *DFFS = cvCreateMat( 1, N, type );
    CvMat *samples0 = cvCreateMat( D, N, type ); // mean subtracted samples
    CvMat *subsamples0 = cvCreateMat( D, 1, type );
    CvScalar rho;
    CV_FUNCNAME( "cvMatPcaDiffs" );
    __BEGIN__;
    CV_ASSERT( CV_IS_MAT(samples) );
    CV_ASSERT( CV_IS_MAT(avg) );
    CV_ASSERT( CV_IS_MAT(eigenvalues) );
    CV_ASSERT( CV_IS_MAT(eigenvectors) );
    CV_ASSERT( D == avg->rows && 1 == avg->cols );
    CV_ASSERT( D == eigenvectors->cols );
    CV_ASSERT( 1 == probs->rows && N == probs->cols );

    cvZero( DIFS );
    cvZero( DFFS );
    cvProjectPCA( samples, avg, eigenvectors, proj );

    // distance in feature space
    if( M > 0 ) {
        for( d = 0; d < M; d++ ) {
            cvmSet( sqrt_pLambda, d, 0, sqrt( cvmGet( eigenvalues, d, 0 ) ) );
        }
        for( n = 0; n < N; n++ ) {
            cvGetRow( proj, subproj, n );
            cvCopy( subproj, scsubproj );
            for( d = 0; d < M; d++ ) {
                cvmSet( scsubproj, 0, d, cvmGet( scsubproj, 0, d ) / cvmGet( sqrt_pLambda, d, 0 ) );
            }
            cvmSet( DIFS, 0, n, pow(cvNorm( scsubproj, NULL, CV_L2 ), 2) );
        }
        if( normalize ) {
            for( d = 0; d < M; d++ ) {
                normterm += log( cvmGet( sqrt_pLambda, d, 0 ) );
            }
            normterm += log(2*M_PI)*(M/2.0);
        }
    }

    // distance from feature space
    if( nEig > M ) {
        for( n = 0; n < N; n++ ) {
            for( d = 0; d < D; d++ ) {
                cvmSet( samples0, d, n, cvmGet( samples, d, n ) - cvmGet( avg, d, 0 ) );
            }
        }
        for( n = 0; n < N; n++ ) {
            cvGetCol( samples0, subsamples0, n );
            cvGetRow( proj, subproj, n );
            cvmSet( DFFS, 0, n, pow(cvNorm( subsamples0, NULL, CV_L2 ),2) - pow(cvNorm( subproj, NULL, CV_L2 ),2) );
        }
        for( d = 0; d < nEig - M; d++ ) {
            cvmSet( rLambda, d, 0, cvmGet( eigenvalues, d + M, 0 ) );
        }
        rho = cvAvg( rLambda );
        for( n = 0; n < N; n++ ) {
            cvmSet( DFFS, 0, n, cvmGet( DFFS, 0, n ) / rho.val[0] );
        }
        if( normalize ) {
            normterm += log(2*M_PI*rho.val[0]) * ((nEig - M)/2.0);
        }
    }
    
    // logp sum
    for( n = 0; n < N; n++ ) {
        cvmSet( probs, 0, n, cvmGet( DIFS, 0, n ) / (-2) + cvmGet( DFFS, 0, n) / (-2) - normterm );
    }
    if( !logprob ) {
        for( n = 0; n < N; n++ ) {
            cvmSet( probs, 0, n, exp(cvmGet( probs, 0, n )) );
        }
    }
    cvReleaseMat( &proj );
    cvReleaseMat( &subproj );
    cvReleaseMat( &scsubproj );
    cvReleaseMat( &sqrt_pLambda );
    cvReleaseMat( &rLambda );
    cvReleaseMat( &DIFS );
    cvReleaseMat( &DFFS );
    cvReleaseMat( &samples0 );
    cvReleaseMat( &subsamples0 );
    __END__;
}

/**
// cvPcaDiffs - Distance "in" and "from" feature space
//
// @param avg                 D x 1 mean vector
// @param eigenvalues         nEig x 1 eigen values
// @param eigenvectors        M x D eigen vectors
// @param [normalize = false] Compute normalization term or not
// @param [logprob   = false] Log probability or not
// @return double likelihood
*/
double cvPcaDiffs( const CvMat* sample, const CvMat* avg, const CvMat* eigenvalues, 
                   const CvMat* eigenvectors, bool normalize, bool logprob )
{
    double prob;
    CvMat *_probs  = cvCreateMat( 1, 1, sample->type );

    cvMatPcaDiffs( sample, avg, eigenvalues, eigenvectors, _probs, normalize, logprob );
    prob = cvmGet(_probs, 0, 0);

    cvReleaseMat( &_probs );
    return prob;
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#endif
