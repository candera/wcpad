// wcpad.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int g_threshold = 5; 
int g_contourPolyPrecision = 40; // Tenths of pixels
int g_contourArea = 10; // Percent
int g_foregroundThreshold = 0; // Hundredths, offset by 1.0

#define BACKGROUND_FRAMES 30

CvCapture* g_capture; 
IplImage* g_raw; 
IplImage* g_grabbed; 
IplImage* g_thresholded; 
IplImage* g_contours; 
IplImage* g_contourSource; 
IplImage* g_forehsv; 
IplImage* g_foreh; 
IplImage* g_fores; 
IplImage* g_forev; 
IplImage* g_foreground; 
CvMemStorage* g_storage; 

CvPoint g_border[4]; 

enum AppState
{
	APPSTATE_ACQUIRING_BORDER, 
	APPSTATE_ACCUMULATING_BACKGROUND,
	APPSTATE_READY
} g_appState = APPSTATE_ACQUIRING_BORDER; 

void CopyImageToMatrix(IplImage* image, CvMat* matrix)
{
	//CvMat buffer; 
	//cvCopy(cvGetMat(g_raw, &buffer), background[backgroundFrame]); 
	// TODO: Optimize this once I figure out what the hell is wrong with the above line	
	for (int y = 0; y < image->height; y++)
	{
		for (int x = 0; x < image->width; x++)
		{
			cvSet2D(matrix, y, x, cvGet2D(image, y, x)); 
		}
	}
}

void CopyMatrixToImage(CvMat* matrix, IplImage* image)
{
	// TODO: Optimize this 
	for (int y = 0; y < image->height; y++)
	{
		for (int x = 0; x < image->width; x++)
		{
			cvSet2D(image, y, x, cvGet2D(matrix, y, x)); 
		}
	}
}

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

	if (g_appState == APPSTATE_READY)
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
	cvNamedWindow("foreground"); 
	cvNamedWindow("background"); 
	cvNamedWindow("backstd"); 

	g_raw = cvQueryFrame(g_capture); 
	
	CvMat* background[BACKGROUND_FRAMES]; 
	for (int i = 0; i < BACKGROUND_FRAMES; ++i)
	{
		background[i] = cvCreateMat(g_raw->height, g_raw->width, CV_32FC3); 
	}
	CvMat* backgroundMean = cvCreateMat(g_raw->height, g_raw->width, CV_32FC3); 
	CvMat* backgroundVariance = cvCreateMat(g_raw->height, g_raw->width, CV_32FC3); 
	CvMat* backgroundStd = cvCreateMat(g_raw->height, g_raw->width, CV_32FC3);  
	CvMat* backgroundScratch = cvCreateMat(g_raw->height, g_raw->width, CV_32FC3);  
	CvMat* backgroundMin = cvCreateMat(g_raw->height, g_raw->width, CV_32FC3);  
	CvMat* backgroundMax = cvCreateMat(g_raw->height, g_raw->width, CV_32FC3);  
	IplImage* backgroundMeanImg = cvCreateImage(cvGetSize(g_raw), IPL_DEPTH_32F, 3); 
	IplImage* backgroundStdImg = cvCreateImage(cvGetSize(g_raw), IPL_DEPTH_32F, 3); 
	IplImage* basis = cvCloneImage(g_raw); 
	g_foreground = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_forehsv = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 3); 
	g_foreh = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_fores = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_forev = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_grabbed = cvCloneImage(g_raw); 
	g_thresholded = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_contourSource = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_contours = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 3); 
	CvMat* borderPoly = cvCreateMat(4, 1, CV_32FC2);

	g_storage = cvCreateMemStorage(); 

	cvCreateTrackbar("threshold", "thresholded", &g_threshold, 50, NULL); 
	cvCreateTrackbar("poly prec", "contours", &g_contourPolyPrecision, 50, NULL); 
	cvCreateTrackbar("area %", "contours", &g_contourArea, 100, NULL); 
	cvCreateTrackbar("threshold", "foreground", &g_foregroundThreshold, 40, NULL); 

	update(); 

	printf("r = reset\n"); 
	printf("\n"); 
	printf("Escape to exit\n"); 
	printf("\n"); 
	printf("Acquiring border...\n"); 

	int backgroundFrame = 0; 
	int conversion = CV_BGR2XYZ; 
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
			backgroundFrame = 0; 
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
					for (int i = 0; i < 4; ++i)
					{
						CvPoint point = g_border[i]; 
						cvSet1D(borderPoly, i, cvScalar(point.x, point.y)); 
					}

					g_appState = APPSTATE_ACCUMULATING_BACKGROUND; 
					printf("Border acquired - accumulating background\n"); 

					for (int i = 0; i < 4; ++i)
					{
						printf("%d: (%d, %d)\n", i, g_border[i].x, g_border[i].y); 
					}
				}
			}
			else if (g_appState == APPSTATE_ACCUMULATING_BACKGROUND)
			{
				if (backgroundFrame == 0)
				{
					cvZero(backgroundMean); 
					cvZero(backgroundVariance); 
					cvZero(backgroundMax); 
					cvSet(backgroundMin, cvScalar(255, 255, 255)); 
				}

				//cvCvtColor(g_raw, g_background, CV_RGB2HSV); 
				//cvSplit(g_background, g_backh, g_backs, g_backv, NULL); 
				if (backgroundFrame < BACKGROUND_FRAMES)
				{
					CopyImageToMatrix(g_raw, background[backgroundFrame]); 
					cvCvtColor(background[backgroundFrame], background[backgroundFrame], conversion); 
					++backgroundFrame; 
				}
				else
				{
					for (int i = 0; i < BACKGROUND_FRAMES; i++)
					{
						cvAcc(background[i], backgroundMean); 
						cvMin(backgroundMin, background[i], backgroundMin); 
						cvMax(backgroundMax, background[i], backgroundMax); 
					}

					cvScale(backgroundMean, backgroundMean, 1.0/(double)BACKGROUND_FRAMES); 

					for (int i = 0; i < BACKGROUND_FRAMES; i++)
					{
						cvSub(background[i], backgroundMean, backgroundScratch); 
						cvSquareAcc(backgroundScratch, backgroundVariance); 
					}

					cvScale(backgroundVariance, backgroundVariance, 1.0/(double)BACKGROUND_FRAMES); 
					cvPow(backgroundVariance, backgroundStd, 0.5); 

					CopyMatrixToImage(backgroundMean, backgroundMeanImg); 
					CopyMatrixToImage(backgroundStd, backgroundStdImg); 

					cvScale(backgroundMeanImg, backgroundMeanImg, 1.0/256.0); 
					cvScale(backgroundStdImg, backgroundStdImg, 1.0/256.0);	
					cvShowImage("background", backgroundMeanImg); 
					cvShowImage("backstd", backgroundStdImg);

					g_appState = APPSTATE_READY; 
					printf("Background acquired. Ready to record clicks.\n"); 
				}
			}
			else if (g_appState == APPSTATE_READY)
			{
				//cvCvtColor(g_raw, g_forehsv, CV_RGB2HSV); 
				//cvSplit(g_forehsv, g_foreh, g_fores, g_forev, NULL); 
				//cvAbsDiff(g_fores, g_backs, g_foreground); 
				//cvSmooth(g_foreground, g_foreground); 
				//cvAdaptiveThreshold(g_foreground, g_foreground, 255, 0, CV_THRESH_BINARY_INV, 11, g_foregroundThreshold); 
				//cvThreshold(g_foreground, g_foreground, g_foregroundThreshold, 255, CV_THRESH_BINARY);

				cvZero(g_foreground); 
				cvCvtColor(g_raw, basis, conversion); 

				double threshold = (g_foregroundThreshold / 100.0) + 1.0; 

				for (int y = 0; y < basis->height; y++)
				{
					for (int x = 0; x < basis->width; x++)
					{
						CvPoint2D32f point; 
						point.x = (float) x; 
						point.y = (float) y; 
						if (cvPointPolygonTest(borderPoly, point, 0) > 0)
						{

							CvScalar raw = cvGet2D(basis, y, x); 
							CvScalar bgm = cvGet2D(backgroundMean, y, x); 
							CvScalar bgs = cvGet2D(backgroundStd, y, x); 
							CvScalar bgmin = cvGet2D(backgroundMin, y, x); 
							CvScalar bgmax = cvGet2D(backgroundMax, y, x); 

							bool result = false; 

							// If at least one channels is outside the min/max background, then it's foreground
							for (int c = 0; c < 3; c++)
							{
								if ((raw.val[c] > (bgmax.val[c] * threshold)) 
									|| (raw.val[c] < (bgmin.val[c] / threshold)))
									//							if (fabs(raw.val[c] - bgm.val[c]) > (bgs.val[c] * threshold))
								{
									result = true; 	
									break;
								}
							}
							if (result)
							{
								cvSet2D(g_foreground, y, x, cvScalar(255));
							}
						}
					}
				}

				cvShowImage("foreground", g_foreground); 
			}
		}
	}

	// TODO: Other cleanup
	cvReleaseCapture(&g_capture); 
	cvDestroyWindow("raw"); 
	
	return 0;
}


