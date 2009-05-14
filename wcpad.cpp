// wcpad.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "geometry.h"

#define FRAME_AT_A_TIME 0

//enum AppState
//{
//	APPSTATE_ACQUIRING_BORDER, 
//	APPSTATE_TRACKING_BORDER
//} g_appState = APPSTATE_ACQUIRING_BORDER; 

int g_threshold = 5; 
int g_maxDepth = 5;
int g_contourPolyPrecision = 40; // Tenths of pixels
int g_segmentThreshold = 10; // Pixels
int g_collinearityThreshold = 104; // Tenths of pixels
int g_maxLineGap = 75; // Pixels
int g_maxAngle = 55; // Tenths of a degree. Zero = perfect angle match, 1 = any angle
int g_camera = 0; 
int g_minArea = 3; // Percent of total area
int g_trackingThreshold = 20; // Pixels - max distance a corner can move and be considered the same

CvCapture* g_capture; 
IplImage* g_raw; 
IplImage* g_test; 
IplImage* g_grabbed; 
IplImage* g_thresholded; 
IplImage* g_contours; 
IplImage* g_contourSource; 
IplConvKernel* g_morphKernel; 
CvMemStorage* g_storage; 

CvScalar blue = cvScalar(255, 0, 0);
CvScalar green = cvScalar(0, 255, 0);
CvScalar red = cvScalar(0, 0, 255);
CvScalar cyan = cvScalar(255, 255, 0);
CvScalar pink = cvScalar(255, 0, 255);
CvScalar yellow = cvScalar(0, 255, 255);




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
int LineSegmentLengthCompareDecreasingLength(const void* a, const void* b, void*)
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

CvPoint Point(CvPoint2D32f p)
{
	return cvPoint((int) (p.x + 0.5), (int) (p.y + 0.5)); 
}

void DrawQuadrangles(IplImage* image, CvSeq* quadrangles, CvScalar color, int thickness = 1)
{
	for (int i = 0; i < quadrangles->total; i++)
	{
		Quadrangle* q = (Quadrangle*) cvGetSeqElem(quadrangles, i); 
		for (int i = 0; i < 4; ++i)
		{
			cvLine(image, Point(q->p[i]), Point(q->p[(i+1)%4]), color, thickness); 
		}
	}
}

void DrawLines(IplImage* image, CvSeq* lines, CvScalar color, int thickness = 1)
{
	for (int i = 0; i < lines->total; i++)
	{
		LineSegment* s = (LineSegment*) cvGetSeqElem(lines, i); 
		cvLine(image, s->p1, s->p2, color, thickness); 
	}

}

void DrawContours(IplImage* image, CvSeq* contours, CvScalar color, int thickness = 1)
{
	for (int i = 0; i < contours->total; i++)
	{
		CvSeq* contour = *((CvSeq**) cvGetSeqElem(contours, i)); 
		for (int j = 1; j < contour->total; j++)
		{
			CvPoint p1 = *((CvPoint*) cvGetSeqElem(contour, j)); 
			CvPoint p0 = *((CvPoint*) cvGetSeqElem(contour, j-1)); 
			cvLine(image, p0, p1, color, thickness); 
		}
	}
}

CvSeq* FindContours(IplImage* image, CvMemStorage* storage, double precision, double minLength)
{
	CvContourScanner scanner = cvStartFindContours(image, storage, sizeof(CvContour), CV_RETR_CCOMP);
	CvSeq* contour; 

	CvSeq* contours = cvCreateSeq(0, sizeof(CvSeq), sizeof(CvSeq*), storage); 

	while ((contour = cvFindNextContour(scanner)) != NULL)
	{
		CvSeq* currentContour = NULL; 

		CvSeq* poly = cvApproxPoly(contour, sizeof(CvContour), g_storage, CV_POLY_APPROX_DP, precision); 

		for (int i = 0; i < poly->total; ++i)
		{
			CvPoint* p0 = (CvPoint*) cvGetSeqElem(poly, (i-1)%(poly->total)); 
			CvPoint* p1 = (CvPoint*) cvGetSeqElem(poly, i); 
			if (length2(p0, p1) < (minLength * minLength))
			{
				if (currentContour != NULL)
				{
					cvSeqPush(contours, &currentContour);
					currentContour = NULL; 
				}
			}
			else
			{
				if (currentContour == NULL)
				{
					currentContour = cvCreateSeq(CV_SEQ_ELTYPE_POINT, sizeof(CvSeq), sizeof(CvPoint), storage); 
					cvSeqPush(currentContour, p0); 
				}
				cvSeqPush(currentContour, p1); 
			}
		}

		if (currentContour != NULL)
		{
			cvSeqPush(contours, &currentContour); 
		}
	}
	
	cvEndFindContours(&scanner); 

	return contours; 

}

double TriangleArea(CvPoint2D32f* p0, CvPoint2D32f* p1, CvPoint2D32f* p2)
{
	double la = length(p0, p1); 
	double lb = length(p1, p2); 
	double lc = length(p2, p0); 
	double s = (la + lb + lc) / 2.0; 

	return sqrt(s * (s - la) * (s - lb) * (s - lc)); 
}

double QuadrangleArea(Quadrangle* q)
{
	return TriangleArea(&q->p[0], &q->p[1], &q->p[2]) + TriangleArea(&q->p[0], &q->p[2], &q->p[3]); 
}

bool IsQuadrangle(CvSeq* contour, double minArea, Quadrangle* q)
{
	if (contour->total != 5)
	{
		return false; 
	}

	CvPoint* p0 = (CvPoint*) cvGetSeqElem(contour, 0);
	CvPoint* p4 = (CvPoint*) cvGetSeqElem(contour, 4);

	// Is it closed? 
	if ((p0->x != p4->x) || (p0->y != p4->y))
	{
		return false;
	}

	for (int i = 0; i < 4; i++)
	{
		CvPoint* p = (CvPoint*) cvGetSeqElem(contour, i); 
		q->p[i] = cvPoint2D32f(p->x, p->y); 
	}

	// Is it big enough? 
	if (QuadrangleArea(q) < minArea)
	{
		return false; 
	}

	return true; 
}

CvSeq* FindQuadrangles(CvSeq* contours, CvMemStorage* storage, double minArea)
{
	CvSeq* quadrangles = cvCreateSeq(0, sizeof(CvSeq), sizeof(Quadrangle), storage); 

	for (int i = 0; i < contours->total; i++)
	{
		CvSeq* contour = *((CvSeq**) cvGetSeqElem(contours, i)); 

		Quadrangle q; 
		if (IsQuadrangle(contour, minArea, &q))
		{
			cvSeqPush(quadrangles, &q); 
		}
	}

	return quadrangles; 
}
CvSeq* FindCompleteQuadrangles(IplImage* image, CvMemStorage* storage, double precision, double minLength, double minArea)
{
	CvContourScanner scanner = cvStartFindContours(image, storage, sizeof(CvContour), CV_RETR_CCOMP);
	CvSeq* contour; 

	CvSeq* quadrangles = cvCreateSeq(0, sizeof(CvSeq), sizeof(Quadrangle), storage); 

	int points = -1; 
	while ((contour = cvFindNextContour(scanner)) != NULL)
	{
		points = contour->total;
		//cvDrawContours(g_contours, contour, colors[0], colors[1], 0); 

		CvSeq* poly = cvApproxPoly(contour, sizeof(CvContour), g_storage, CV_POLY_APPROX_DP, precision); 

		//for (int j = 0; j < contour->total; ++j)
		//{
		//	CvPoint* vertex = (CvPoint*) cvGetSeqElem(contour, j); 
		//	cvCircle(g_contours, *vertex, 2, colors[3]); 
		//}

		if (poly->total == 4)
		{
			//if (cvCheckContourConvexity(contour))
			//{
				bool longEnough = true; 
				for (int j = 0; j < 4; ++j)
				{
					LineSegment s;
					s.p1 = *((CvPoint*) cvGetSeqElem(poly, j)); 
					s.p2 = *((CvPoint*) cvGetSeqElem(poly, (j+1)%4)); 
					if (length(&s) < minLength)
					{
						longEnough = false;
						break; 
					}
				}

				if (longEnough)
				{
					if (fabs(cvContourArea(contour)) > minArea)
					{
						Quadrangle q; 
						for (int i = 0; i < 4; i++)
						{
							CvPoint p = *((CvPoint*) cvGetSeqElem(poly, i)); 
							q.p[i].x = (float) p.x; 
							q.p[i].y = (float) p.y; 
						}
						cvSeqPush(quadrangles, &q); 
					}
				}
			//}
		}
		
	}
	cvEndFindContours(&scanner); 

	return quadrangles; 
}

CvSeq* FindLines(IplImage* image, CvMemStorage* storage, double precision, double minLength)
{
	CvContourScanner scanner = cvStartFindContours(image, storage, sizeof(CvContour), CV_RETR_CCOMP);
	CvSeq* contour; 

	CvSeq* lines = cvCreateSeq(0, sizeof(CvSeq), sizeof(LineSegment), storage); 

	int points = -1; 
	while ((contour = cvFindNextContour(scanner)) != NULL)
	{
		points = contour->total;
		//cvDrawContours(g_contours, contour, colors[0], colors[1], 0); 

		CvSeq* poly = cvApproxPoly(contour, sizeof(CvContour), g_storage, CV_POLY_APPROX_DP, precision); 

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
			if (length(&s) > minLength)
			{
				cvSeqPush(lines, &s); 
			}
		}
	}
	cvEndFindContours(&scanner); 

	return lines; 
}

CvMat* QuadrangleToContour(Quadrangle* quadrangle)
{
	CvMat* contour = cvCreateMat(4, 1, CV_32FC2); 
	for (int i = 0; i < 4; i++)
	{
		cvSet1D(contour, i, cvScalar(quadrangle->p[i].x, quadrangle->p[i].y)); 
	}

	return contour; 
}

bool Surrounds(Quadrangle* outer, Quadrangle* inner)
{
	CvMat* contour = QuadrangleToContour(outer);
	CvMat* points = QuadrangleToContour(inner); 

	bool surrounds = true; 
	for (int i = 0; i < 4; ++i)
	{
		CvScalar p = cvGet1D(points, i); 
		CvPoint2D32f point = cvPoint2D32f(p.val[0], p.val[1]); 
		if (cvPointPolygonTest(contour, point, 0) < 0)
		{
			surrounds = false; 
			break; 
		}
	}

	cvReleaseMat(&contour); 
	cvReleaseMat(&points); 

	return surrounds; 
}
bool IsUpdateOf(Quadrangle* q1, Quadrangle* q2, int threshold)
{
	// Returns true if every corner of q1 is within threshold pixels of some corner in q2
	for (int i = 0; i < 4; i++)
	{
		double minLength = 1E20; 
		for (int j = 0; j < 4; j++)
		{
			double l2 = length2(&q1->p[i], &q2->p[j]); 
			if (l2 < minLength)
			{
				minLength = l2; 
			}
		}

		if (minLength > (threshold*threshold))
		{
			return false; 
		}
	}

	return true; 
}
int UpdateOf(Quadrangle* q, CvSeq* borders, int threshold)
{
	for (int i = 0; i < borders->total; i++)
	{
		Quadrangle* border = (Quadrangle*) cvGetSeqElem(borders, i); 

		if (IsUpdateOf(q, border, threshold))
		{
			return i; 
		}
	}

	return -1; 
}

CvSeq* FilterQuadrangles(CvSeq* quadrangles, CvMemStorage* storage)
{
	CvSeq* filtered = cvCreateSeq(0, sizeof(CvSeq), sizeof(Quadrangle), storage); 

	for (int i = 0; i < quadrangles->total; i++)
	{
		Quadrangle* candidate = (Quadrangle*) cvGetSeqElem(quadrangles, i); 

		bool contained = false; 
		for (int j = 0; j < quadrangles->total; j++)
		{
			if (i != j)
			{
				Quadrangle* outer = (Quadrangle*) cvGetSeqElem(quadrangles, j); 

				if (Surrounds(outer, candidate))
				{
					contained = true; 
					break; 
				}
			}
		}

		if (!contained)
		{
			cvSeqPush(filtered, candidate); 
		}
	}

	return filtered; 
}

CvSeq* RefineQuadrangles(CvSeq* quadrangles, CvArr* image, CvMemStorage* storage)
{
	CvSeq* refined = cvCreateSeq(0, sizeof(CvSeq), sizeof(Quadrangle), storage); 

	for (int i = 0; i < quadrangles->total; i++)
	{
		Quadrangle q = *((Quadrangle*) cvGetSeqElem(quadrangles, i)); 
		cvFindCornerSubPix(image, (CvPoint2D32f*) &q, 4, cvSize(5, 5), cvSize(-1, -1), cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03)); 

		cvSeqPush(refined, &q); 
	}

	return refined; 
}
void update_contours(int)
{
	//cvCvtColor(g_thresholded, g_contours, CV_GRAY2BGR); 
	cvCopy(g_raw, g_contours); 
	cvScale(g_contours, g_contours, 0.5); 
	cvCopy(g_thresholded, g_contourSource); 

	//CvScalar blue = cvScalar(255, 0, 0);
	//CvScalar green = cvScalar(0, 255, 0);
	//CvScalar red = cvScalar(0, 0, 255);
	//CvScalar cyan = cvScalar(255, 255, 0);
	//CvScalar pink = cvScalar(255, 0, 255);
	//CvScalar yellow = cvScalar(0, 255, 255);

	//CvScalar colors[6]; 
	//colors[0] = blue; 
	//colors[1] = green; 
	//colors[2] = red;
	//colors[3] = cyan; 
	//colors[4] = pink; 
	//colors[5] = yellow;

	//CvSeq* lines = FindLines(g_contourSource, g_storage, g_contourPolyPrecision / 10.0, g_segmentThreshold); 
	//DrawLines(g_contours, lines, yellow); 	

	
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
//	update_threshold(0); 
	//g_currentContour = 0; 
}

void update()
{
	g_raw = cvQueryFrame(g_capture); 
	cvShowImage("raw", g_raw); 
}

void TestQuadrangleArea()
{
	Quadrangle q; 
	q.p[0].x = 0; 
	q.p[0].y = 0; 
	q.p[1].x = 100; 
	q.p[1].y = 0; 
	q.p[2].x = 100; 
	q.p[2].y = 10; 
	q.p[3].x = 0; 
	q.p[3].y = 10; 

	printf("QuadrangleArea (unit square): %lf\n", QuadrangleArea(&q));
}


void TrackQuadrangles(IplImage* source, CvSeq* borders, CvSeq** pContours = NULL)
{
	cvCvtColor(source, g_thresholded, CV_RGB2GRAY); 
	cvSmooth(g_thresholded, g_thresholded, 2, 5); 
	cvAdaptiveThreshold(g_thresholded, g_thresholded, 255, 0, CV_THRESH_BINARY_INV, 3, g_threshold); 
	cvShowImage("thresholded", g_thresholded); 

	cvCopy(g_thresholded, g_contourSource); 

	CvSeq* contours; 
	if (pContours == NULL)
	{
		pContours = &contours; 
	}

	*pContours = FindContours(g_contourSource, g_storage, g_contourPolyPrecision / 10.0, g_segmentThreshold); 
	CvSeq* quadrangles = FindQuadrangles(*pContours, g_storage, g_minArea / 100.0 * (g_contourSource->width * g_contourSource->height)); 

	CvSeq* rejects = cvCreateSeq(0, sizeof(CvSeq), sizeof(Quadrangle), g_storage); 

	for (int i = 0; i < quadrangles->total; i++)
	{
		Quadrangle* outer = (Quadrangle*) cvGetSeqElem(quadrangles, i); 

		for (int j = 0; j < quadrangles->total; j++)
		{
			if (i != j)
			{
				Quadrangle* inner = (Quadrangle*) cvGetSeqElem(quadrangles, j); 

				if (Surrounds(outer, inner))
				{
					cvSeqRemove(quadrangles, j); 
					cvSeqPush(rejects, inner); 
					j--; 
				}
			}
		}
	}

	for (int i = 0; i < quadrangles->total; i++)
	{
		Quadrangle* quadrangle = (Quadrangle*) cvGetSeqElem(quadrangles, i); 

		int update = UpdateOf(quadrangle, borders, g_trackingThreshold); 

		if (update >= 0)
		{
			Quadrangle* border = (Quadrangle*) cvGetSeqElem(borders, update);
			for (int k = 0; k < 4; ++k)
			{
				border->p[k] = quadrangle->p[k]; 
			}
		}
		else
		{
			cvSeqPush(borders, quadrangle); 
		}
	}
}

CvMat* Square()
{
	CvMat* c = cvCreateMat(3, 5, CV_32FC1); 
	cvSet2D(c, 0, 0, cvScalar(-1)); 
	cvSet2D(c, 1, 0, cvScalar(-1)); 
	cvSet2D(c, 2, 0, cvScalar(1)); 
	cvSet2D(c, 0, 1, cvScalar(1)); 
	cvSet2D(c, 1, 1, cvScalar(-1)); 
	cvSet2D(c, 2, 1, cvScalar(1)); 
	cvSet2D(c, 0, 2, cvScalar(1)); 
	cvSet2D(c, 1, 2, cvScalar(1)); 
	cvSet2D(c, 2, 2, cvScalar(1)); 
	cvSet2D(c, 0, 3, cvScalar(-1)); 
	cvSet2D(c, 1, 3, cvScalar(1)); 
	cvSet2D(c, 2, 3, cvScalar(1)); 
	cvSet2D(c, 0, 4, cvScalar(-1)); 
	cvSet2D(c, 1, 4, cvScalar(-1)); 
	cvSet2D(c, 2, 4, cvScalar(1)); 

	return c; 
}

CvMat* GapOneSide()
{
	CvMat* c = cvCreateMat(3, 6, CV_32FC1); 
	cvSet2D(c, 0, 0, cvScalar(-0.5)); 
	cvSet2D(c, 1, 0, cvScalar(1)); 
	cvSet2D(c, 2, 0, cvScalar(1)); 
	cvSet2D(c, 0, 1, cvScalar(-1)); 
	cvSet2D(c, 1, 1, cvScalar(1)); 
	cvSet2D(c, 2, 1, cvScalar(1)); 
	cvSet2D(c, 0, 2, cvScalar(-1)); 
	cvSet2D(c, 1, 2, cvScalar(-1)); 
	cvSet2D(c, 2, 2, cvScalar(1)); 
	cvSet2D(c, 0, 3, cvScalar(1)); 
	cvSet2D(c, 1, 3, cvScalar(-1)); 
	cvSet2D(c, 2, 3, cvScalar(1)); 
	cvSet2D(c, 0, 4, cvScalar(1)); 
	cvSet2D(c, 1, 4, cvScalar(1)); 
	cvSet2D(c, 2, 4, cvScalar(1)); 
	cvSet2D(c, 0, 5, cvScalar(0.5)); 
	cvSet2D(c, 1, 5, cvScalar(1)); 
	cvSet2D(c, 2, 5, cvScalar(1)); 

	return c; 
}

CvMat* MissingCorner()
{
	CvMat* c = cvCreateMat(3, 5, CV_32FC1); 
	cvSet2D(c, 0, 0, cvScalar(-1)); 
	cvSet2D(c, 1, 0, cvScalar(0)); 
	cvSet2D(c, 2, 0, cvScalar(1)); 
	cvSet2D(c, 0, 1, cvScalar(-1)); 
	cvSet2D(c, 1, 1, cvScalar(-1)); 
	cvSet2D(c, 2, 1, cvScalar(1)); 
	cvSet2D(c, 0, 2, cvScalar(1)); 
	cvSet2D(c, 1, 2, cvScalar(-1)); 
	cvSet2D(c, 2, 2, cvScalar(1)); 
	cvSet2D(c, 0, 3, cvScalar(1)); 
	cvSet2D(c, 1, 3, cvScalar(1)); 
	cvSet2D(c, 2, 3, cvScalar(1)); 
	cvSet2D(c, 0, 4, cvScalar(0)); 
	cvSet2D(c, 1, 4, cvScalar(1)); 
	cvSet2D(c, 2, 4, cvScalar(1)); 

	return c; 
}

void test()
{
	printf("Entering test mode.\n"); 

	TestQuadrangleArea();

	IplImage* source = cvCloneImage(g_raw); 


	CvMemStorage* storage = cvCreateMemStorage(); 

	CvSeq* borders = cvCreateSeq(0, sizeof(CvSeq), sizeof(Quadrangle), storage); 

	//DrawContours(g_contours, contours, red); 

	//CvMat* c = Square(); 
	//CvMat* c = GapOneSide(); 
	CvMat* c = MissingCorner(); 
	CvMat* p = cvCreateMat(2, c->cols, CV_32FC1); 

	int t = 0; 
	double w = 4.0; 
	double vx = 0.10; 
	double vy = 0.01; 
	bool drawSegments = false; 
	bool paused = false; 
	for (;;)
	{
		if (!paused)
		{
			t++; 
		}
		int key = cvWaitKey(33); 

		if (key == 'q')
		{
			break; 
		}
		else if (key == 0x2B) // +
		{
			w += 0.1; 
		}
		else if (key == 0x2D) // -
		{
			w -= 0.1; 
		}
		else if (key == 0x250000) // left
		{
			vx -= 0.01; 
		}
		else if (key == 0x260000) // up
		{
			vy -= 0.01; 
		}
		else if (key == 0x270000) // right
		{
			vx += 0.01; 
		}
		else if (key == 0x280000) // down
		{
			vy += 0.01; 
		}
		else if (key == 'c')
		{
			cvClearSeq(borders); 
		}
		else if (key == 'l')
		{
			drawSegments = !drawSegments; 
		}
		else if (key == 'p')
		{
			paused = !paused; 
		}
		else if (key != -1)
		{
			printf("Keypress: 0x%X\n", key); 
		}

		cvSet(source, CV_RGB(255, 255, 255)); 

		CvMat* rot = cvCreateMat(2, 3, CV_32FC1);
		cv2DRotationMatrix(cvPoint2D32f(0, 0), t*w, 100, rot); 

		cvMatMul(rot, c, p); 

		for (int i = 1; i < c->cols; i++)
		{
			int j = i-1; 
			CvPoint center = cvPoint((int) (300.0 + 100.0 * sin(t * vx)), (int) (300.0 + 100.0 * cos(t * vy))); 
			CvPoint a = cvPoint((int) (cvGetReal2D(p, 0, i) + center.x), (int) (cvGetReal2D(p, 1, i) + center.y)); 
			CvPoint b = cvPoint((int) (cvGetReal2D(p, 0, j) + center.x), (int) (cvGetReal2D(p, 1, j) + center.y)); 
			cvLine(source, a, b, CV_RGB(0, 0, 0), 10); 
		}

		cvShowImage("test", source); 

		CvSeq* contours; 
		TrackQuadrangles(source, borders, &contours); 


		cvCopy(source, g_contours); 
		cvScale(g_contours, g_contours, 0.25); 
		DrawQuadrangles(g_contours, borders, green); 
		if (drawSegments)
		{
			DrawContours(g_contours, contours, red); 
		}
		cvShowImage("contours", g_contours); 
	}

	cvReleaseMemStorage(&storage); 

	printf("Exiting test mode.\n"); 
}

int _tmain(int, _TCHAR*)
{
	g_capture = cvCreateCameraCapture(g_camera); 

	cvNamedWindow("raw"); 
	cvNamedWindow("raw1"); 
	cvNamedWindow("raw2");
	cvNamedWindow("raw3"); 
	cvNamedWindow("grabbed"); 
	cvNamedWindow("thresholded"); 
	cvNamedWindow("contours"); 
	cvNamedWindow("test"); 

	g_raw = cvQueryFrame(g_capture); 
	IplImage* rawConvert = cvCloneImage(g_raw); 
	IplImage* raw1 = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	IplImage* raw2 = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	IplImage* raw3 = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_grabbed = cvCloneImage(g_raw); 
	g_test = cvCloneImage(g_raw); 
	g_thresholded = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_contourSource = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_contours = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 3); 

	g_morphKernel = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT); 

	g_storage = cvCreateMemStorage(); 

	//cvCreateTrackbar("threshold", "thresholded", &g_threshold, 50, update_threshold); 
	//cvCreateTrackbar("max_depth", "contours", &g_maxDepth, 20, update_contours); 
	//cvCreateTrackbar("poly precision", "contours", &g_contourPolyPrecision, 50, update_contours); 
	//cvCreateTrackbar("seg thresh", "contours", &g_segmentThreshold, 100, update_contours); 
	//cvCreateTrackbar("collin thresh", "contours", &g_collinearityThreshold, 200, update_contours); 
	//cvCreateTrackbar("line gap", "contours", &g_maxLineGap, 200, update_contours); 
	//cvCreateTrackbar("angle", "contours", &g_maxAngle, 100, update_contours); 
	cvCreateTrackbar("area", "contours", &g_minArea, 100, NULL); 
	cvCreateTrackbar("move thresh", "contours", &g_trackingThreshold, 100, NULL); 

	//update(); 

	CvScalar colors[6]; 
	colors[0] = blue; 
	colors[1] = green; 
	colors[2] = red;
	colors[3] = cyan; 
	colors[4] = pink; 
	colors[5] = yellow;


#if FRAME_AT_A_TIME
	printf("g = grab frame\n"); 
#endif
	printf("c = camera switch\n"); 
	printf("r = reset\n"); 
	printf("\n"); 
	printf("Escape (or q) to exit\n"); 

	CvSeq* borders = cvCreateSeq(0, sizeof(CvSeq), sizeof(Quadrangle), g_storage); 

	for (;;)
	{
		int key = cvWaitKey(33); 
		update(); 
#if !FRAME_AT_A_TIME
		grab();
#endif

		cvCvtColor(g_grabbed, rawConvert, CV_BGR2HLS); 
		cvSplit(rawConvert, raw1, raw2, raw3, NULL); 

		cvShowImage("raw1", raw1); 
		cvShowImage("raw2", raw2); 
		cvShowImage("raw3", raw3); 

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
		else if (key == 8)
		{
			cvClearSeq(borders); 
		}
		//else if (key == 'r')
		//{
		//	g_appState = APPSTATE_ACQUIRING_BORDER; 
		//	printf("Acquiring border.\n"); 
		//}
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
		else
		{
			TrackQuadrangles(g_grabbed, borders); 
			cvCopy(g_grabbed, g_contours); 
			cvScale(g_contours, g_contours, 0.25); 
			//DrawContours(g_contours, contours, red); 
			//DrawQuadrangles(g_contours, rejects, yellow); 
			//DrawQuadrangles(g_contours, quadrangles, blue); 
			DrawQuadrangles(g_contours, borders, green); 

			cvShowImage("contours", g_contours); 
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

