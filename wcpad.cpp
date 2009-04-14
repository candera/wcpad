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

CvPoint g_border[4]; 

enum AppState
{
	APPSTATE_ACQUIRING_BORDER, 
	APPSTATE_ACQUIRED_BORDER
} g_appState = APPSTATE_ACQUIRING_BORDER; 

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

bool find_border()
{
	cvShowImage("grabbed", g_grabbed); 

	cvCvtColor(g_grabbed, g_thresholded, CV_RGB2GRAY); 
	cvAdaptiveThreshold(g_thresholded, g_thresholded, 255, 0, CV_THRESH_BINARY_INV, 3, g_threshold); 
	cvShowImage("thresholded", g_thresholded); 

	bool found = false; 
	cvCopy(g_thresholded, g_contourSource); 

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
			g_border[j].x = vertex->x;
			g_border[j].y = vertex->y; 
		}
		found = true; 
	}

	cvClearMemStorage(g_storage); 

	return found; 
}

void update()
{
	g_raw = cvQueryFrame(g_capture); 
	cvShowImage("raw", g_raw); 

	cvCopy(g_raw, g_grabbed); 
	cvSmooth(g_grabbed, g_grabbed); 

	cvScale(g_grabbed, g_contours, 0.3); 

	if (g_appState == APPSTATE_ACQUIRED_BORDER)
	{
		for (int j = 0; j < 4; ++j)
		{
			CvPoint vertex = g_border[j]; 
			CvPoint nextVertex = g_border[(j+1)%4];
			cvLine(g_contours, vertex, nextVertex, cvScalar(0, 0, 255)); 
			cvCircle(g_contours, vertex, 2, cvScalar(0, 255, 255)); 
		}
	}

	cvShowImage("contours", g_contours); 

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

	cvCreateTrackbar("threshold", "thresholded", &g_threshold, 50, NULL); 
	cvCreateTrackbar("poly prec", "contours", &g_contourPolyPrecision, 50, NULL); 
	cvCreateTrackbar("area %", "contours", &g_contourArea, 100, NULL); 

	update(); 

	printf("r = reset\n"); 
	printf("\n"); 
	printf("Escape to exit\n"); 
	printf("\n"); 
	printf("Acquiring border...\n"); 

	while (true)
	{
		int key = cvWaitKey(33); 
		update(); 
		if ((key == 27) || (key == 'q'))
		{
			break; 
		}
		else if ((key == 'r') || (key == 'R'))
		{
			g_appState = APPSTATE_ACQUIRING_BORDER; 
			printf("Acquiring border...\n"); 
		}
		else
		{
			update(); 
			if (g_appState == APPSTATE_ACQUIRING_BORDER)
			{
				if (find_border())
				{
					g_appState = APPSTATE_ACQUIRED_BORDER; 

					printf("Border acquired\n"); 
					for (int i = 0; i < 4; ++i)
					{
						printf("%d: (%d, %d)\n", i, g_border[i].x, g_border[i].y); 
					}
				}
			}
			else if (g_appState == APPSTATE_ACQUIRED_BORDER)
			{
				// TODO
			}
		}
	}

	// TODO: Other cleanup
	cvReleaseCapture(&g_capture); 
	cvDestroyWindow("raw"); 
	
	return 0;
}

