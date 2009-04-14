// wcpad.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int g_threshold = 5; 
int g_contourPolyPrecision = 40; // Tenths of pixels
int g_contourArea = 10; // Percent

CvCapture* g_capture; 
IplImage* g_raw; 
IplImage* g_grabbed; 
IplImage* g_thresholded; 
IplImage* g_contours; 
IplImage* g_contourSource; 
IplConvKernel* g_morphKernel; 
CvMemStorage* g_storage; 

double pad_border_area(CvSeq* contour, CvSeq* poly)
{
	if (poly->total != 4)
	{
		return -1.0; 
	}

    double area = fabs(cvContourArea(contour)) / ((double)(g_raw->width * g_raw->height));

	if (area < (g_contourArea / 100.0))
	{
		return -1.0; 
	}

	return area; 
}

void update_contours(int)
{
	cvScale(g_grabbed, g_contours, 0.5); 
	cvCopy(g_thresholded, g_contourSource); 

	//cvFindContours(g_contourSource, g_storage, &contours, sizeof(CvContour), CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE); 
	//cvFindContours(g_contourSource, g_storage, &contours, sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE); 
	//printf("Found %d contours\n", contours->total);

	CvScalar colors[6]; 
	colors[0] = cvScalar(255, 0, 0);
	colors[1] = cvScalar(0, 255, 0);
	colors[2] = cvScalar(0, 0, 255);
	colors[3] = cvScalar(255, 255, 0);
	colors[4] = cvScalar(255, 0, 255);
	colors[5] = cvScalar(0, 255, 255);

	CvContourScanner scanner = cvStartFindContours(g_contourSource, g_storage, sizeof(CvContour), CV_RETR_CCOMP);
	CvSeq* contour; 

	double max_area = -1; 
	CvSeq* max_poly = NULL; 
	while ((contour = cvFindNextContour(scanner)) != NULL)
	{
	    CvSeq* poly = cvApproxPoly(contour, sizeof(CvContour), g_storage, CV_POLY_APPROX_DP, g_contourPolyPrecision / 10.0); 

		double area = pad_border_area(contour, poly); 
		if (area > -1 && area > max_area)
		{
			max_area = area; 
			max_poly = poly; 
		}
	}
	CvSeq* contours = cvEndFindContours(&scanner); 

	if (max_poly != NULL)
	{
		for (int j = 0; j < 4; ++j)
		{
			CvPoint* vertex = (CvPoint*) cvGetSeqElem(max_poly, j); 
			CvPoint* nextVertex = (CvPoint*) cvGetSeqElem(max_poly, (j+1)%4);
			//cvCircle(g_contours, vertex, 2, colors[(i+1)%6]); 
			cvLine(g_contours, *vertex, *nextVertex, colors[2]); 
		}
	}

	cvClearMemStorage(g_storage); 

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
	grab(); 
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
	cvCreateTrackbar("poly prec", "contours", &g_contourPolyPrecision, 50, update_contours); 
	cvCreateTrackbar("area %", "contours", &g_contourArea, 100, update_contours); 

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

