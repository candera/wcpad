// wcpad.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#define FRAME_AT_A_TIME 0

int g_threshold = 5; 
int g_maxDepth = 5;
int g_contourPolyPrecision = 40; // Tenths of pixels
int g_segmentThreshold = 10; // Pixels
int g_collinearityThreshold = 104; // Tenths of pixels
int g_maxLineGap = 75; // Pixels
int g_maxAngle = 55; // Tenths of a degree. Zero = perfect angle match, 1 = any angle
int g_camera = 0; 

CvCapture* g_capture; 
IplImage* g_raw; 
IplImage* g_test; 
IplImage* g_grabbed; 
IplImage* g_thresholded; 
IplImage* g_contours; 
IplImage* g_contourSource; 
IplConvKernel* g_morphKernel; 
CvMemStorage* g_storage; 

typedef struct LineSegment
{
	CvPoint p1; 
	CvPoint p2; 
} LineSegment; 

double min4(double a, double b, double c, double d)
{
	return min(a, min(b, min(c, d))); 
}

double max4(double a, double b, double c, double d)
{
	return max(a, max(b, max(c, d))); 
}

int min4(int a, int b, int c, int d)
{
	return min(a, min(b, min(c, d))); 
}

int max4(int a, int b, int c, int d)
{
	return max(a, max(b, max(c, d))); 
}

double length2(CvPoint* p1, CvPoint* p2)
{
	double dx = p1->x - p2->x; 
	double dy = p1->y - p2->y; 

	return ((dx * dx) + (dy * dy)); 
}

double length2(LineSegment* s)
{
	return length2(&(s->p1), &(s->p2)); 
}

double length(CvPoint* p1, CvPoint* p2)
{
	return sqrt(length2(p1, p2)); 
}

double length(LineSegment* s)
{
	return length(&(s->p1), &(s->p2)); 
}

double DotNormal(LineSegment* s1, LineSegment* s2)
{
	double l1 = length(s1); 
	double l2 = length(s2); 
	double x1 = (s1->p2.x - s1->p1.x) / l1; 
	double x2 = (s2->p2.x - s2->p1.x) / l2; 
	double y1 = (s1->p2.y - s1->p1.y) / l1; 
	double y2 = (s2->p2.y - s2->p1.y) / l1; 

	return x1 * x2 + y1 * y2; 
}

double Dot(LineSegment* s1, LineSegment* s2)
{
	int x1 = s1->p2.x - s1->p1.x; 
	int x2 = s2->p2.x - s2->p1.x; 
	int y1 = s1->p2.y - s1->p1.y; 
	int y2 = s2->p2.y - s2->p1.y; 

	return x1 * x2 + y1 * y2; 
}

//double Collinearity(LineSegment* s1, LineSegment* s2)
//{
//	return 1.0 - fabs(Dot(s1, s2) / (length(s1) * length(s2))); 
//}

double IndexOnLineNearestPoint(CvPoint* pt, LineSegment* s)
{
	double un = ((pt->x - s->p1.x) * (s->p2.x - s->p1.x)) + ((pt->y - s->p1.y) * (s->p2.y - s->p1.y));
	double ud = ((s->p1.x - s->p2.x) * (s->p1.x - s->p2.x)) + ((s->p1.y - s->p2.y) * (s->p1.y - s->p2.y));
	return un / ud; 
}

CvPoint PointAlongLine(double u, LineSegment* s)
{
	double x = (double) s->p1.x + (u * (double) (s->p2.x - s->p1.x)); 
	double y = (double) s->p1.y + (u * (double) (s->p2.y - s->p1.y)); 

	// TODO: Round? 
	return cvPoint((int)x, (int)y);
}

double PointToLineDistance(CvPoint* pt, LineSegment* s)
{
	double u = IndexOnLineNearestPoint(pt, s); 

	CvPoint nearest = PointAlongLine(u, s); 
	return length(pt, &nearest); 
}

CvPoint2D32f Unitize(LineSegment* s)
{
	double len = length(s); 
	return cvPoint2D32f((s->p2.x - s->p1.x) / len, (s->p2.y - s->p1.y) / len); 
}

// Returns the square of the "gap" between two line segments, which is the 
// distance between their two nearest endpoints
double Gap2(LineSegment* s1, LineSegment* s2)
{
	double gap11 = length2(&(s1->p1), &(s2->p1)); 
	double gap12 = length2(&(s1->p1), &(s2->p2)); 
	double gap21 = length2(&(s1->p2), &(s2->p1));
	double gap22 = length2(&(s1->p2), &(s2->p2)); 

	return min4(gap11, gap12, gap21, gap22); 
}

int CollinearWithAny(CvSeq* lines, LineSegment* s, double maxDistanceFromFitLine, double maxAllowableGap, double maxAngle)
{
	for (int i = 0; i < lines->total; i++)
	{
		LineSegment* t = (LineSegment*) cvGetSeqElem(lines, i); 
		
		double d1 = PointToLineDistance(&(t->p1), s);
		double d2 = PointToLineDistance(&(t->p2), s); 

		// Taylor series approximation for cosine in degrees
		double minCos = 1.0 - (3.0462E-4 * maxAngle * maxAngle); 

		double gap2 = Gap2(s, t); 

		if (	(d1 < maxDistanceFromFitLine) 
			&&	(d2 < maxDistanceFromFitLine) 
			&&	(gap2 < (maxAllowableGap * maxAllowableGap))
			&&  (fabs(DotNormal(s, t)) > minCos))
		{
			return i; 
		}
	}

	return -1; 
}

void MergeLineSegments(LineSegment* s1, LineSegment* s2, LineSegment* m) 
{
	CvPoint2D32f points[4]; 
	points[0] = cvPoint2D32f(s1->p1.x, s1->p1.y); 
	points[1] = cvPoint2D32f(s1->p2.x, s1->p2.y); 
	points[2] = cvPoint2D32f(s2->p1.x, s2->p1.y); 
	points[3] = cvPoint2D32f(s2->p2.x, s2->p2.y); 
	
	float line[4]; 

	cvFitLine2D(points, 4, CV_DIST_L2, 0, 0.01F, 0.01F, line); 

	LineSegment s3; 
	s3.p1 = cvPoint((int) line[2], (int) line[3]); 
	s3.p2 = cvPoint((int) (line[2] + (line[0] * 1000)), (int) (line[3] + (line[1] * 1000))); 

	double minu = 1e20; 
	double maxu = -1e20;
	int minIndex = 0;
	int maxIndex = 0; 

	for (int i = 0; i < 4; i++)
	{
		CvPoint pt = cvPoint((int) points[i].x, (int) points[i].y); 
		double u = IndexOnLineNearestPoint(&pt, &s3); 

		if (u < minu)
		{
			minIndex = i; 
			minu = u; 
		}

		if (u > maxu)
		{
			maxIndex = i; 
			maxu = u; 
		}
	}

	m->p1 = PointAlongLine(minu, &s3);
	m->p2 = PointAlongLine(maxu, &s3); 
}

// Used to sort line segments in order of decreasing length
int LineSegmentLengthCompareDecreasingLength(const void* a, const void* b, void* userdata)
{
	double la = length((LineSegment*)a); 
	double lb = length((LineSegment*)b); 

	return (la > lb) ? -1 : (la < lb) ? 1 : 0; 
}

CvSeq* MergeLines(CvSeq* lines, CvMemStorage* storage, double collinearityThreshold, double maxLineGap, double maxAngle)
{
	cvSeqSort(lines, LineSegmentLengthCompareDecreasingLength); 

	CvSeq* mergedLines = cvCreateSeq(0, sizeof(CvSeq), sizeof(LineSegment), storage); 

	for (int i = 0; i < lines->total; i++)
	{
		LineSegment* s = (LineSegment*) cvGetSeqElem(lines, i); 

		int index = -1; 
		if ((index = CollinearWithAny(mergedLines, s, collinearityThreshold, maxLineGap, maxAngle)) != -1)
		{
			LineSegment* s2 = (LineSegment*) cvGetSeqElem(mergedLines, index); 

			LineSegment m; 
			MergeLineSegments(s, s2, &m); 

			cvSeqRemove(mergedLines, index); 

			cvSeqPush(mergedLines, &m); 
		}
		else 
		{
			cvSeqPush(mergedLines, s); 
		}
	}

	return mergedLines; 

}

void DrawLines(IplImage* image, CvSeq* lines, CvScalar color, int thickness = 1)
{
	for (int i = 0; i < lines->total; i++)
	{
		LineSegment* s = (LineSegment*) cvGetSeqElem(lines, i); 
		cvLine(image, s->p1, s->p2, color, thickness); 
	}

}

void update_contours(int)
{
	cvScale(g_grabbed, g_contours, 0.5); 
	cvCopy(g_thresholded, g_contourSource); 

	CvScalar colors[6]; 
	colors[0] = cvScalar(255, 0, 0);
	colors[1] = cvScalar(0, 255, 0);
	colors[2] = cvScalar(0, 0, 255);
	colors[3] = cvScalar(255, 255, 0);
	colors[4] = cvScalar(255, 0, 255);
	colors[5] = cvScalar(0, 255, 255);

	CvContourScanner scanner = cvStartFindContours(g_contourSource, g_storage, sizeof(CvContour), CV_RETR_CCOMP);
	CvSeq* contour; 

	CvSeq* lines = cvCreateSeq(0, sizeof(CvSeq), sizeof(LineSegment), g_storage); 

	int points = -1; 
	while ((contour = cvFindNextContour(scanner)) != NULL)
	{
		points = contour->total;
		//cvDrawContours(g_contours, contour, colors[0], colors[1], 0); 

		CvSeq* poly = cvApproxPoly(contour, sizeof(CvContour), g_storage, CV_POLY_APPROX_DP, g_contourPolyPrecision / 10.0); 

		//for (int j = 0; j < contour->total; ++j)
		//{
		//	CvPoint* vertex = (CvPoint*) cvGetSeqElem(contour, j); 
		//	cvCircle(g_contours, *vertex, 2, colors[3]); 
		//}

		for (int j = 0; j < (poly->total)-1; ++j)
		{
			LineSegment s;
			s.p1 = *((CvPoint*) cvGetSeqElem(poly, j)); 
			s.p2 = *((CvPoint*) cvGetSeqElem(poly, j+1)); 
			CvScalar color = colors[3]; 
			if (length(&s) > g_segmentThreshold)
			{
				cvSeqPush(lines, &s); 
			}
		}
	}
	CvSeq* contours = cvEndFindContours(&scanner); 

	CvSeq* mergedLines = MergeLines(lines, g_storage, g_collinearityThreshold / 10.0, g_maxLineGap, 1.0 - (g_maxAngle / 10)); 

	DrawLines(g_contours, mergedLines, colors[1], 3); 
	DrawLines(g_contours, lines, colors[0]); 	

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
	//g_currentContour = 0; 
}

void update()
{
	g_raw = cvQueryFrame(g_capture); 
	cvShowImage("raw", g_raw); 
}

void test()
{
	printf("Entering test mode.\n"); 

	CvSeq* lines = cvCreateSeq(0, sizeof(CvSeq), sizeof(LineSegment), g_storage); 

	int numLines = 10; 

	CvMat* points = cvCreateMat(numLines, 4, CV_8U); 
	cvZero(points); 

	CvRNG rng = cvRNG(); 

	CvScalar minxy = cvScalar(0); 
	CvScalar maxxy = cvScalar(min(g_grabbed->width, g_grabbed->height));

	while (true)
	{
		int key = cvWaitKey(33); 
		if ((key == 27) || (key == 'q'))
		{
			break; 
		}
		else if (key == 'g')
		{
			cvClearSeq(lines); 
			cvRandArr(&rng, points, CV_RAND_UNI, minxy, maxxy); 

			for (int i = 0; i < numLines; i++)
			{
				LineSegment s; 
				s.p1.x = (int) cvGet2D(points, i, 0).val[0]; 
				s.p1.y = (int) cvGet2D(points, i, 1).val[0]; 
				s.p2.x = (int) cvGet2D(points, i, 2).val[0]; 
				s.p2.y = (int) cvGet2D(points, i, 3).val[0]; 
				cvSeqPush(lines, &s); 
			}
		}

		CvSeq* mergedLines = MergeLines(lines, g_storage, g_collinearityThreshold / 10.0, g_maxLineGap, 1.0 - (g_maxAngle / 10)); 

		cvZero(g_test); 
		DrawLines(g_test, mergedLines, CV_RGB(255, 255, 0), 3); 
		DrawLines(g_test, lines, CV_RGB(0, 0, 255)); 
		cvShowImage("test", g_test); 
	}
	printf("Exiting test mode.\n"); 
}

int _tmain(int argc, _TCHAR* argv[])
{
	g_capture = cvCreateCameraCapture(g_camera); 

	cvNamedWindow("raw"); 
	cvNamedWindow("grabbed"); 
	cvNamedWindow("thresholded"); 
	cvNamedWindow("contours"); 
	cvNamedWindow("test"); 

	g_raw = cvQueryFrame(g_capture); 
	g_grabbed = cvCloneImage(g_raw); 
	g_test = cvCloneImage(g_raw); 
	g_thresholded = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_contourSource = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_contours = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 3); 

	g_morphKernel = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT); 

	g_storage = cvCreateMemStorage(); 

	cvCreateTrackbar("threshold", "thresholded", &g_threshold, 50, update_threshold); 
	cvCreateTrackbar("max_depth", "contours", &g_maxDepth, 20, update_contours); 
	cvCreateTrackbar("poly precision", "contours", &g_contourPolyPrecision, 50, update_contours); 
	cvCreateTrackbar("seg thresh", "contours", &g_segmentThreshold, 100, update_contours); 
	cvCreateTrackbar("collin thresh", "contours", &g_collinearityThreshold, 200, update_contours); 
	cvCreateTrackbar("line gap", "contours", &g_maxLineGap, 200, update_contours); 
	cvCreateTrackbar("angle", "contours", &g_maxAngle, 100, update_contours); 

	//update(); 

#if FRAME_AT_A_TIME
	printf("g = grab frame\n"); 
#endif
	printf("c = camera switch\n"); 
	printf("\n"); 
	printf("Escape (or q) to exit\n"); 

	while (true)
	{
		int key = cvWaitKey(33); 
		update(); 
#if !FRAME_AT_A_TIME
		grab();
#endif
		if ((key == 27) || (key == 'q'))
		{
			break; 
		}
		else if (key == 'c')
		{
			g_camera = g_camera ? 0 : 1; 
			cvReleaseCapture(&g_capture); 
			g_capture = cvCreateCameraCapture(g_camera); 
		}
#if FRAME_AT_A_TIME
		else if (key == 'g')
		{
			grab(); 
		}
#endif
		else if (key == 't')
		{
			test(); 
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

