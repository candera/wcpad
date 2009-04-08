// wcpad.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int g_edgeThreshold1 = 10; 
int g_edgeThreshold2 = 15; 
int g_lineRho = 1; 
int g_lineTheta = 1;
int g_lineThreshold = 50; 
int g_lineParam1 = 30; 
int g_lineParam2 = 5; 
int g_threshold = 5; 

CvCapture* g_capture; 
IplImage* g_raw; 
IplImage* g_background; 
IplImage* g_edges; 
IplImage* g_grey; 
IplImage* g_lines; 
IplImage* g_thresholded; 
CvMemStorage* g_lineStorage; 

void update_edges(int)
{
	cvCanny(g_background, g_edges, g_edgeThreshold1, g_edgeThreshold2); 
	cvShowImage("edges", g_edges); 

	CvSeq* lines = cvHoughLines2(
		g_edges, 
		g_lineStorage, 
		CV_HOUGH_PROBABILISTIC, 
		(double) g_lineRho, 
		g_lineTheta * 3.14/180.0, 
		g_lineThreshold, 
		g_lineParam1, 
		g_lineParam2); 

	int numLines = lines->total; 
	printf("Found %d lines\n", numLines); 

	cvCvtColor(g_background, g_lines, CV_GRAY2RGB); 
	cvConvertScale(g_lines, g_lines, 0.5); 
	//cvCopy(g_raw, g_lines); 

	for (int i = 0; i < numLines; ++i)
	{
		CvPoint* line = (CvPoint*) cvGetSeqElem(lines, i); 
		cvDrawLine(g_lines, line[0], line[1], cvScalar(255, 255, 0));
	}

	cvShowImage("lines", g_lines); 
}

void update_threshold(int)
{
	cvCvtColor(g_raw, g_thresholded, CV_RGB2GRAY); 
	//cvThreshold(g_thresholded, g_thresholded, g_threshold, 255, CV_THRESH_BINARY); 
	cvAdaptiveThreshold(g_thresholded, g_thresholded, 255, 0, 0, 3, g_threshold); 
	cvShowImage("thresholded", g_thresholded); 
}

void grab()
{
	g_raw = cvQueryFrame(g_capture); 
	cvCvtColor(g_raw, g_grey, CV_BGR2GRAY); 
}

void update()
{
	grab(); 
	update_threshold(0); 
	cvShowImage("raw", g_raw); 
	cvShowImage("grey", g_grey);
}
void grab_background()
{
	cvZero(g_background); 
	int iterations = 10; 
	for (int i = 0; i < iterations; ++i)
	{
		grab(); 
		cvAddWeighted(g_grey, 1.0/(float)iterations, g_background, 1.0, 0, g_background); 
	}
	
	cvShowImage("background", g_background); 
	update_edges(0); 
	update_threshold(0); 
}

int _tmain(int argc, _TCHAR* argv[])
{
	g_capture = cvCreateCameraCapture(0); 

	cvNamedWindow("raw"); 
	cvNamedWindow("background");
	cvNamedWindow("grey"); 
    cvNamedWindow("edges");
	cvNamedWindow("lines"); 
	cvNamedWindow("thresholded"); 

	g_raw = cvQueryFrame(g_capture); 
	g_grey = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
    g_background = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_edges = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_lines = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 3); 
	g_thresholded = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 

	g_lineStorage = cvCreateMemStorage();

	cvCreateTrackbar("threshold1", "edges", &g_edgeThreshold1, 255, update_edges); 
	cvCreateTrackbar("threshold2", "edges", &g_edgeThreshold2, 255, update_edges); 
	cvCreateTrackbar("rho", "lines", &g_lineRho, 255, update_edges); 
	cvCreateTrackbar("theta", "lines", &g_lineTheta, 255, update_edges); 
	cvCreateTrackbar("threshold", "lines", &g_lineThreshold, 255, update_edges); 
	cvCreateTrackbar("param1", "lines", &g_lineParam1, 255, update_edges); 
	cvCreateTrackbar("param2", "lines", &g_lineParam2, 255, update_edges); 
	cvCreateTrackbar("threshold", "thresholded", &g_threshold, 255, update_threshold); 

	update(); 

	// cvNamedWindow("lines"); 

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
			grab_background(); 
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
    */

	//cvReleaseMemStorage(&storage); 
	cvReleaseCapture(&g_capture); 
	cvReleaseImage(&g_lines); 
	cvReleaseImage(&g_edges); 
	cvReleaseImage(&g_grey); 
	cvReleaseImage(&g_background);
	cvDestroyWindow("raw"); 
	cvDestroyWindow("background");
	cvDestroyWindow("grey");
	cvDestroyWindow("edges"); 
	cvDestroyWindow("lines"); 
	
	return 0;
}

