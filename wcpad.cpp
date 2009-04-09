// wcpad.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int g_threshold = 5; 
int g_erosions = 1;
int g_dilations = 1; 
int g_maxDepth = 5;

CvCapture* g_capture; 
IplImage* g_raw; 
IplImage* g_grabbed; 
IplImage* g_thresholded; 
IplImage* g_morphed; 
IplImage* g_contours; 
IplConvKernel* g_morphKernel; 
CvMemStorage* g_storage; 

void update_contours(int)
{
	cvScale(g_grabbed, g_contours, 0.5); 

	CvSeq* first_contour; 
	cvFindContours(g_morphed, g_storage, &first_contour); 
	printf("Found %d contours\n", first_contour->total);
	cvDrawContours(g_contours, first_contour, cvScalar(255, 255, 0), cvScalar(255, 0, 255), g_maxDepth); 

	cvShowImage("contours", g_contours); 
}

void update_morphs(int)
{
	cvCopy(g_thresholded, g_morphed); 

	if (g_dilations > 0)
	{
		cvDilate(g_morphed, g_morphed, g_morphKernel, g_dilations); 
	}
	if (g_erosions > 0)
	{
		cvErode(g_morphed, g_morphed, g_morphKernel, g_erosions); 
	}

	cvShowImage("morphed", g_morphed); 
	update_contours(0); 
}

void update_threshold(int)
{
	//cvThreshold(g_thresholded, g_thresholded, g_threshold, 255, CV_THRESH_BINARY); 
	cvCvtColor(g_grabbed, g_thresholded, CV_RGB2GRAY); 
	cvAdaptiveThreshold(g_thresholded, g_thresholded, 255, 0, CV_THRESH_BINARY_INV, 3, g_threshold); 
	cvShowImage("thresholded", g_thresholded); 
	update_morphs(0); 
}

void grab()
{
	cvCopy(g_raw, g_grabbed); 
	cvSmooth(g_grabbed, g_grabbed); 
	cvShowImage("grabbed", g_grabbed); 
	update_threshold(0); 
}

void update()
{
	g_raw = cvQueryFrame(g_capture); 
	cvShowImage("raw", g_raw); 
}

int _tmain(int argc, _TCHAR* argv[])
{
	g_capture = cvCreateCameraCapture(0); 

	cvNamedWindow("raw"); 
	cvNamedWindow("grabbed"); 
	cvNamedWindow("thresholded"); 
	cvNamedWindow("morphed"); 
	cvNamedWindow("contours"); 

	g_raw = cvQueryFrame(g_capture); 
	g_grabbed = cvCloneImage(g_raw); 
	g_thresholded = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_morphed = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_contours = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 3); 

	g_morphKernel = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT); 

	g_storage = cvCreateMemStorage(); 

	cvCreateTrackbar("threshold", "thresholded", &g_threshold, 255, update_threshold); 
	cvCreateTrackbar("erosions", "morphed", &g_erosions, 10, update_morphs); 
	cvCreateTrackbar("dilations", "morphed", &g_dilations, 10, update_morphs); 
	cvCreateTrackbar("max_depth", "contours", &g_maxDepth, 20, update_contours); 

	update(); 

	printf("g = grab frame\n"); 
	printf("\n"); 
	printf("Escape to exit\n"); 

	while (true)
	{
		int key = cvWaitKey(33); 
		update(); 
		if (key == 27)
		{
			break; 
		}
		else if (key == 'g')
		{
			grab(); 
		}
		else
		{
			update(); 
		}
	}

    /*
	CvMemStorage* storage = cvCreateMemStorage(); 
	CvSeq* firstContour = NULL; 
	cvFindContours(raw, storage, &firstContour);

	cvDrawContours(scaled, firstContour, 
    
	cvReleaseMemStorage(&storage); 
	*/

	// TODO: Other cleanup
	cvReleaseCapture(&g_capture); 
	cvDestroyWindow("raw"); 
	
	return 0;
}

