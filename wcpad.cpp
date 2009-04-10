// wcpad.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int g_threshold = 5; 
int g_maxDepth = 5;

CvCapture* g_capture; 
IplImage* g_raw; 
IplImage* g_grabbed; 
IplImage* g_thresholded; 
IplImage* g_contours; 
IplImage* g_contourSource; 
IplConvKernel* g_morphKernel; 
CvMemStorage* g_storage; 

void update_contours(int)
{
	cvScale(g_grabbed, g_contours, 0.5); 
	cvCopy(g_thresholded, g_contourSource); 

	//cvFindContours(g_contourSource, g_storage, &contours, sizeof(CvContour), CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE); 
	//cvFindContours(g_contourSource, g_storage, &contours, sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE); 
	//printf("Found %d contours\n", contours->total);

	CvContourScanner scanner = cvStartFindContours(g_contourSource, g_storage);
	CvSeq* contour; 

	CvScalar colors[6]; 
	colors[0] = cvScalar(255, 0, 0);
	colors[1] = cvScalar(0, 255, 0);
	colors[2] = cvScalar(0, 0, 255);
	colors[3] = cvScalar(255, 255, 0);
	colors[4] = cvScalar(255, 0, 255);
	colors[5] = cvScalar(0, 255, 255);

	int i = 0; 
	while ((contour = cvFindNextContour(scanner)) != NULL)
	{
		++i; 
		cvDrawContours(g_contours, contour, colors[i%6], colors[(i+1)%6], 0); 
		//CvMat* points = cvCreateMat(contour->total, 1, CV_); 
		//cvCvtSeqToArray(contour, (void*)&(points->data)); 
		//printf("Contour %d has area %lf\n", i, cvContourArea(points)); 
	}
	CvSeq* contours = cvEndFindContours(&scanner); 

	//cvDrawContours(g_contours, contours, cvScalar(255, 255, 0), cvScalar(255, 0, 255), g_maxDepth, CV_FILLED); 
	//cvDrawContours(g_contours, contours, cvScalar(255, 255, 0), cvScalar(255, 0, 255), -1, CV_FILLED, 8); 

	cvShowImage("contours", g_contours); 
}


void update_threshold(int)
{
	//cvThreshold(g_thresholded, g_thresholded, g_threshold, 255, CV_THRESH_BINARY); 
	cvCvtColor(g_grabbed, g_thresholded, CV_RGB2GRAY); 
	cvAdaptiveThreshold(g_thresholded, g_thresholded, 255, 0, CV_THRESH_BINARY_INV, 3, g_threshold); 
	cvShowImage("thresholded", g_thresholded); 
	update_contours(0); 
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
	cvNamedWindow("contours"); 

	g_raw = cvQueryFrame(g_capture); 
	g_grabbed = cvCloneImage(g_raw); 
	g_thresholded = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_contourSource = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_contours = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 3); 

	g_morphKernel = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT); 

	g_storage = cvCreateMemStorage(); 

	cvCreateTrackbar("threshold", "thresholded", &g_threshold, 50, update_threshold); 
	cvCreateTrackbar("max_depth", "contours", &g_maxDepth, 20, update_contours); 

	update(); 

	printf("g = grab frame\n"); 
	printf("\n"); 
	printf("Escape to exit\n"); 

	while (true)
	{
		int key = cvWaitKey(33); 
		update(); 
		if ((key == 27) || (key == 'q'))
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

