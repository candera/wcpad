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
int g_trackingThreshold = 60; // Pixels - max distance a corner can move and be considered the same
int g_alignmentThreshold = 20; // Pixels - max distance a C4S3 quad can misalign sides and corners

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


typedef struct Border
{
	Quadrangle quadrangle; 
	int age; 
	bool matched; 
} Border; 


CvPoint Point(CvPoint2D32f p)
{
	return cvPoint((int) (p.x + 0.5), (int) (p.y + 0.5)); 
}

void DrawQuadrangle(IplImage* image, Quadrangle* q, CvScalar color, int thickness = 1)
{
	for (int i = 0; i < 4; ++i)
	{
		cvLine(image, Point(q->c[i].p), Point(q->c[(i+1)%4].p), color, thickness); 
	}
}
void DrawQuadrangles(IplImage* image, CvSeq* quadrangles, CvScalar color, int thickness = 1)
{
	for (int i = 0; i < quadrangles->total; i++)
	{
		Quadrangle* q = (Quadrangle*) cvGetSeqElem(quadrangles, i); 
		DrawQuadrangle(image, q, color, thickness); 
	}
}
void DrawBorders(IplImage* image, CvSeq* borders, CvScalar color, CvScalar colorStep, int thickness = 1)
{
	for (int i = 0; i < borders->total; i++)
	{
		Border* b = (Border*) cvGetSeqElem(borders, i); 

		CvScalar c = cvScalar(0); 
		for (int j = 0; j < 4; ++j)
		{
			c.val[j] = color.val[j] - (b->age * colorStep.val[j]);
		}

		DrawQuadrangle(image, &b->quadrangle, c, thickness); 
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

CvSeq* FindContours(IplImage* image, CvMemStorage* storage, float precision, float minLength)
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

float TriangleArea(CvPoint2D32f* p0, CvPoint2D32f* p1, CvPoint2D32f* p2)
{
	float la = length(p0, p1); 
	float lb = length(p1, p2); 
	float lc = length(p2, p0); 
	float s = (la + lb + lc) / 2.0F; 

	return sqrt(s * (s - la) * (s - lb) * (s - lc)); 
}

float QuadrangleArea(Quadrangle* q)
{
	return TriangleArea(&q->c[0].p, &q->c[1].p, &q->c[2].p) + TriangleArea(&q->c[0].p, &q->c[2].p, &q->c[3].p); 
}

float ZCross(CvPoint p1, CvPoint p2, CvPoint p3)
{
	float xa = (float) (p2.x - p1.x); 
	float ya = (float) (p2.y - p1.y); 
	float xb = (float) (p3.x - p2.x); 
	float yb = (float) (p3.y - p2.y); 

	return (xa * yb) - (ya * xb); 
}

bool IsConvex(CvPoint* points, int count)
{
	float first = ZCross(points[0], points[1], points[2]); 

	// If we take the cross product of consecutive sides, they should
	// all point in the same z direction
	for (int i = 1; i < count-2; ++i)
	{
		float zdir = ZCross(points[i], points[i+1], points[i+2]); 

		if ((first * zdir) < 0)
		{
			return false; 
		}
	}

	return true; 
}

bool IsC4S4Quadrangle(CvSeq* contour, Quadrangle* q, float minArea, float)
{
	if (contour->total != 5)
	{
		return false; 
	}

	CvPoint p[5]; 
	for (int i = 0; i < 5; i++)
	{
		p[i] = *((CvPoint*) cvGetSeqElem(contour, i));
	}

	// Is it closed? 
	if ((p[0].x != p[4].x) || (p[0].y != p[4].y))
	{
		return false;
	}

	if (!IsConvex(p, 5))
	{
		return false; 
	}

	for (int i = 0; i < 4; i++)
	{
		q->c[i].p = cvPoint2D32f(p[i].x, p[i].y); 
		q->c[i].implied = false; 
	}

	// Is it big enough? 
	if (QuadrangleArea(q) < minArea)
	{
		return false; 
	}

	return true; 
}

bool SharpTest(CvPoint* p, int count, float min, float max)
{
	for (int i = 0; i < count - 2; i++)
	{
		LineSegment s1; 
		s1.p1 = p[i]; 
		s1.p2 = p[i+1]; 
		LineSegment s2; 
		s2.p1 = p[i+1]; 
		s2.p2 = p[i+2]; 

		float dn = fabs(DotNormal(&s1, &s2));

		if ((dn < min) || (dn > max))
		{
			return false; 
		}
	}

	return true; 
}


bool IsC3Quadrangle(CvSeq* contour, Quadrangle* q, float minArea, float)
{
	if (contour->total < 5)
	{
		return false; 
	}

	float maxArea = 0; 
	for (int start = 0; start < (contour->total - 5); start++)
	{
		Quadrangle candidate; 

		CvPoint points[5]; 
		for (int i = start; i < start + 5; i++)
		{
			points [i - start] = *((CvPoint*) cvGetSeqElem(contour, i)); 
		}

		// Are the corners sharp enough, but not too sharp?
		if (!SharpTest(points, 5, 0, 0.7f))
		{
			continue; 
		}

		// Find the implied corner
		LineSegment s1; 
		s1.p1 = points[1]; 
		s1.p2 = points[0]; 

		LineSegment s2; 
		s2.p1 = points[3]; 
		s2.p2 = points[4]; 

		CvPoint2D32f intersection; 
		float u1; 
		float u2; 
		if (!Intersection(&s1, &s2, &intersection, &u1, &u2))
		{
			continue; 
		}

		// If the intersection isn't off the end of both segments, then it doesn't fit the profile. 
		// Allow a little fudging in case something merges with one of the sides to create a 
		// longer-than-expected side. 
		if ((u1 < 0.9) || (u2 < 0.9))
		{
			continue; 
		}
		
		candidate.c[0].p = cvPoint2D32f(points[1].x, points[1].y); 
		candidate.c[0].implied = false; 
		candidate.c[1].p = cvPoint2D32f(points[2].x, points[2].y); 
		candidate.c[1].implied = false; 
		candidate.c[2].p = cvPoint2D32f(points[3].x, points[3].y); 
		candidate.c[2].implied = false; 
		candidate.c[3].p = intersection; 
		candidate.c[3].implied = true; 

		// Store the area and keep looking in case there's more than one candidate on this contour
		float area = QuadrangleArea(&candidate); 

		if (area > maxArea)
		{
			*q = candidate; 
			maxArea = area; 
		}
	}

	// Is it big enough? 
	if (maxArea < minArea)
	{
		return false; 
	}

	return true; 
}
bool IsC4S3Quadrangle(CvSeq* contour, Quadrangle* q, float minArea, float alignmentThreshold)
{
	if (contour->total < 6)
	{
		return false; 
	}

	float maxArea = 0; 
	for (int start = 0; start < (contour->total - 6); ++start)
	{
		Quadrangle candidate; 

		CvPoint points[6]; 
		for (int i = start; i < start + 6; i++)
		{
			points[i - start] = *((CvPoint*) cvGetSeqElem(contour, i)); 
		}

		// Is it convex? 
		if (!IsConvex(points, 6))
		{
			continue; 
		}

		// Do the ends point at each other? 

#if 0
		// Could check either by seeing if extending the interrupted lines comes near the corner
		LineSegment s1; 
		s1.p1 = points[1]; 
		s1.p2 = points[0]; 

		float d1 = PointToLineDistance(&points[4], &s1); 

		LineSegment s2; 
		s2.p1 = points[4];
		s2.p2 = points[5]; 

		float d2 = PointToLineDistance(&points[1], &s2); 
#else
		// Or by seeing if the line that joins the corners comes close to the pionts that form the gap
		LineSegment s; 
		s.p1 = points[1]; 
		s.p2 = points[4]; 

		float d1 = PointToLineDistance(&points[0], &s);
		float d2 = PointToLineDistance(&points[5], &s); 
#endif

		if ((d1 > alignmentThreshold) || (d2 > alignmentThreshold))
		{
			continue; 
		}

		for (int i = 0; i < 4; i++)
		{
			candidate.c[i].p = cvPoint2D32f(points[i+1].x, points[i+1].y); 
			candidate.c[i].implied = false; 
		}

		// Store the area and keep looking in case there's more than one candidate on this contour
		float area = QuadrangleArea(&candidate); 

		if (area > maxArea)
		{
			*q = candidate; 
			maxArea = area; 
		}
	}

	// Is it big enough? 
	if (maxArea < minArea)
	{
		return false; 
	}

	return true; 
}

CvSeq* FindQuadrangles(CvSeq* contours, CvMemStorage* storage, bool(*criteria)(CvSeq*, Quadrangle*, float, float), float param1, float param2)
{
	CvSeq* quadrangles = cvCreateSeq(0, sizeof(CvSeq), sizeof(Quadrangle), storage); 

	for (int i = 0; i < contours->total; i++)
	{
		CvSeq* contour = *((CvSeq**) cvGetSeqElem(contours, i)); 

		Quadrangle q; 
		if ((*criteria)(contour, &q, param1, param2))
		{
			cvSeqPush(quadrangles, &q); 
		}
	}

	return quadrangles; 
}
CvMat* QuadrangleToContour(Quadrangle* quadrangle)
{
	CvMat* contour = cvCreateMat(4, 1, CV_32FC2); 
	for (int i = 0; i < 4; i++)
	{
		cvSet1D(contour, i, cvScalar(quadrangle->c[i].p.x, quadrangle->c[i].p.y)); 
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
		float minLength = 1E20F; 
		for (int j = 0; j < 4; j++)
		{
			float l2 = length2(&q1->c[i].p, &q2->c[j].p); 
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
		Border* border = (Border*) cvGetSeqElem(borders, i); 

		if (IsUpdateOf(q, &border->quadrangle, threshold))
		{
			return i; 
		}
	}

	return -1; 
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
	q.c[0].p.x = 0; 
	q.c[0].p.y = 0; 
	q.c[1].p.x = 100; 
	q.c[1].p.y = 0; 
	q.c[2].p.x = 100; 
	q.c[2].p.y = 10; 
	q.c[3].p.x = 0; 
	q.c[3].p.y = 10; 

	printf("QuadrangleArea (unit square): %lf\n", QuadrangleArea(&q));
}


int GetClosestBorderCornerIndex(CvPoint2D32f* p, Border* border, CvSeq* borderCorners)
{
	float mind = 1.0e20f; 
	int mini = -1; 
	for (int i = 0; i < borderCorners->total; i++)
	{
		int cornerIndex = *((int*) cvGetSeqElem(borderCorners, i)); 
		CvPoint2D32f* corner = &border->quadrangle.c[cornerIndex].p; 

		float d = length2(p, corner); 

		if (d < mind)
		{
			mind = d; 
			mini = i; 
		}
	}

	return mini; 
}
void TrackBorders(IplImage* source, CvSeq* borders, int maxAge, CvSeq** pContours = NULL)
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

	*pContours = FindContours(g_contourSource, g_storage, (float) g_contourPolyPrecision / 10.0F, (float) g_segmentThreshold); 
	float minArea = g_minArea / 100.0F * (g_contourSource->width * g_contourSource->height); 
	CvSeq* quadrangles = FindQuadrangles(*pContours, g_storage, &IsC4S4Quadrangle, minArea, 0); 
	CvSeq* gapOneSiders = FindQuadrangles(*pContours, g_storage, &IsC4S3Quadrangle, minArea, (float) g_alignmentThreshold); 
	CvSeq* missingCorners = FindQuadrangles(*pContours, g_storage, &IsC3Quadrangle, minArea, 0); 

	CvSeq* sequences[3]; 
	sequences[0] = quadrangles; 
	sequences[1] = gapOneSiders; 
	sequences[2] = missingCorners; 

	for (int a = 0; a < 3; a++)
	{
		for (int b = 0; b < 3; b++)
		{
			CvSeq* seqa = sequences[a]; 
			CvSeq* seqb = sequences[b]; 
			for (int i = 0; i < seqa->total; i++)
			{
				Quadrangle* outer = (Quadrangle*) cvGetSeqElem(seqa, i); 

				for (int j = 0; j < seqb->total; j++)
				{
					if ((a != b) || (i != j))
					{
						Quadrangle* inner = (Quadrangle*) cvGetSeqElem(seqb, j); 

						if (Surrounds(outer, inner))
						{
							cvSeqRemove(seqb, j); 
							j--; 
						}
					}
				}
			}
		}
	}

	for (int i = 0; i < borders->total; i++)
	{
		Border* border = (Border*) cvGetSeqElem(borders, i); 
		border->age++;
	}

	CvMemStorage* scratchStorage = cvCreateMemStorage(); 

    CvSeq* borderCorners = cvCreateSeq(0, sizeof(CvSeq), sizeof(int), scratchStorage); 
	CvSeq* quadExactCorners = cvCreateSeq(0, sizeof(CvSeq), sizeof(int), scratchStorage); 
	CvSeq* quadImpliedCorners = cvCreateSeq(0, sizeof(CvSeq), sizeof(int), scratchStorage); 

	for (int a = 0; a < 3; a++)
	{
		for (int i = 0; i < sequences[a]->total; i++)
		{
			Quadrangle* quadrangle = (Quadrangle*) cvGetSeqElem(sequences[a], i); 

			int update = UpdateOf(quadrangle, borders, g_trackingThreshold); 

			if (update >= 0)
			{
				Border* border = (Border*) cvGetSeqElem(borders, update);
				if (border->age > 0)
				{

					for (int k = 0; k < 4; ++k)
					{
						cvSeqPush(borderCorners, &k); 

						if (quadrangle->c[k].implied)
						{
							cvSeqPush(quadImpliedCorners, &k);
						}
						else
						{
							cvSeqPush(quadExactCorners, &k); 
						}
					}

					// Match up the exact corners first
					while (quadExactCorners->total > 0)
					{
						int quadCorner = *((int*) cvGetSeqElem(quadExactCorners, 0)); 
						int closestCorner = GetClosestBorderCornerIndex(&quadrangle->c[quadCorner].p, border, borderCorners); 
						int borderCorner = *((int*) cvGetSeqElem(borderCorners, closestCorner)); 

						border->quadrangle.c[borderCorner].p = quadrangle->c[quadCorner].p; 

						cvSeqRemove(borderCorners, closestCorner); 
						cvSeqRemove(quadExactCorners, 0); 
					}

					// Next, update the implied corners
					while (quadImpliedCorners->total > 0)
					{
						int quadCorner = *((int*) cvGetSeqElem(quadImpliedCorners, 0)); 
						int closestCorner = GetClosestBorderCornerIndex(&quadrangle->c[quadCorner].p, border, borderCorners); 
						int borderCorner = *((int*) cvGetSeqElem(borderCorners, closestCorner)); 

						// For non-exact corners, don't copy exactly. Rather, move it in that direction somewhat
						CvPoint2D32f p1 = border->quadrangle.c[borderCorner].p; 
						CvPoint2D32f p2 = quadrangle->c[quadCorner].p; 

						border->quadrangle.c[borderCorner].p = cvPoint2D32f(
							p1.x + (0.5f * (p2.x - p1.x)), 
							p1.y + (0.5f * (p2.y - p1.y))); 

						cvSeqRemove(borderCorners, closestCorner); 
						cvSeqRemove(quadImpliedCorners, 0); 
					}

					border->age = 0; 
				}
			}
			// We only derive new borders from complete quadrangles
			else if (a == 0)
			{
				Border border; 
				border.quadrangle = *quadrangle; 
				border.age = 0; 
				cvSeqPush(borders, &border); 
			}
		}
	}
	for (int i = 0; i < borders->total; i++)
	{
		Border* border = (Border*) cvGetSeqElem(borders, i); 
		if (border->age > maxAge)
		{
			cvSeqRemove(borders, i--); 
		}
	}

	cvReleaseMemStorage(&scratchStorage); 

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
	IplImage* hls = cvCloneImage(source); 
	IplImage* l = cvCreateImage(cvSize(hls->width, hls->height), hls->depth, 1); 
	IplImage* thresholded = cvCloneImage(l); 

	CvMemStorage* storage = cvCreateMemStorage(); 

	CvSeq* borders = cvCreateSeq(0, sizeof(CvSeq), sizeof(Border), storage); 

	CvMat* square = Square(); 
	CvMat* gapOneSide = GapOneSide(); 
	CvMat* missingCorner = MissingCorner(); 

	int maxAge = 10; 

	int t = 0; 
	float w = 4.0F; 
	float vx = 0.10F; 
	float vy = 0.01F; 
	bool drawSegments = false; 
	bool paused = false; 
	int shape = 0; 
	int thickness = 10; 
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
			w += 0.1F; 
		}
		else if (key == 0x2D) // -
		{
			w -= 0.1F; 
		}
		else if (key == 0x250000) // left
		{
			vx -= 0.01F; 
		}
		else if (key == 0x260000) // up
		{
			vy -= 0.01F; 
		}
		else if (key == 0x270000) // right
		{
			vx += 0.01F; 
		}
		else if (key == 0x280000) // down
		{
			vy += 0.01F; 
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
		else if (key == 's')
		{
			shape = (shape + 1) % 4; 
		}
		else if (key == 't')
		{
			thickness++;
		}
		else if (key == 'T')
		{
			thickness--; 
			thickness = max(thickness, 0); 
		}
		else if (key == 'f')
		{
			++t;
		}
		else if (key == 'b')
		{
			--t; 
		}
		else if (key != -1)
		{
			printf("Keypress: 0x%X\n", key); 
		}

		if (shape == 0)
		{
			if (!paused)
			{
				update(); 
				cvCopyImage(g_raw, source); 
			}
		}
		else
		{
			CvMat* c; 
			if (shape == 1)
			{
				c = square; 
			}
			else if (shape == 2)
			{
				c = gapOneSide; 
			}
			else if (shape == 3)
			{
				c = missingCorner; 
			}
			else
			{
				throw "unknown shape"; 
			}

			CvMat* p = cvCreateMat(2, c->cols, CV_32FC1); 

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
				cvLine(source, a, b, CV_RGB(0, 0, 0), thickness); 
			}
		}
		cvShowImage("test", source); 

		cvCvtColor(source, hls, CV_BGR2HLS); 
//		cvSmooth(hls, hl, 2, 5); 
		cvSplit(hls, NULL, l, NULL, NULL); 
		cvAdaptiveThreshold(l, thresholded, 255, 0, CV_THRESH_BINARY_INV, 3, g_threshold); 
		cvShowImage("thresholded", thresholded); 

		float minArea = g_minArea / 100.0F * (source->width * source->height);
		CvSeq* contours; 
		TrackBorders(source, borders, maxAge, &contours); 
		CvSeq* quadrangles = FindQuadrangles(contours, g_storage, &IsC4S4Quadrangle, minArea, 0);
		CvSeq* gapOneSiders = FindQuadrangles(contours, g_storage, &IsC4S3Quadrangle, minArea, (float) g_alignmentThreshold); 
		CvSeq* missingCorner = FindQuadrangles(contours, g_storage, &IsC3Quadrangle, minArea, 0); 
		
		cvCopy(source, g_contours); 
		cvScale(g_contours, g_contours, 0.25); 
		if (drawSegments)
		{
			DrawContours(g_contours, contours, red); 
		}
		DrawQuadrangles(g_contours, quadrangles, green, 2); 
		DrawQuadrangles(g_contours, gapOneSiders, blue, 2); 
		DrawQuadrangles(g_contours, missingCorner, yellow, 2); 
		DrawBorders(g_contours, borders, pink, CV_RGB(256/(maxAge * 2), 256/(maxAge * 2), 0)); 
		cvShowImage("contours", g_contours); 
	}

	cvReleaseMemStorage(&storage); 

	printf("Exiting test mode.\n"); 
}

void ThresholdExperiment()
{
	update(); 

	IplImage* hue = cvCreateImage(cvSize(g_raw->width, g_raw->height), g_raw->depth, 1); 
	IplImage* luminance = cvCreateImage(cvSize(g_raw->width, g_raw->height), g_raw->depth, 1); 
	IplImage* saturation = cvCreateImage(cvSize(g_raw->width, g_raw->height), g_raw->depth, 1); 
	IplImage* black = cvCreateImage(cvSize(g_raw->width, g_raw->height), g_raw->depth, 1); 
	IplImage* white = cvCreateImage(cvSize(g_raw->width, g_raw->height), g_raw->depth, 1); 
	IplImage* hueWindowed = cvCloneImage(hue); 
	IplImage* luminanceThresholded = cvCloneImage(luminance); 
	IplImage* saturationThresholded = cvCloneImage(saturation); 
	IplImage* slThresholded = cvCloneImage(luminance); 
	IplImage* converted = cvCloneImage(g_raw); 
	IplImage* blackDilated = cvCloneImage(black); 
	IplImage* intersection = cvCloneImage(black); 
	IplImage* intersectionContours = cvCloneImage(g_raw); 

	cvNamedWindow("hue");
	cvNamedWindow("luminance");
	cvNamedWindow("saturation");
	cvNamedWindow("hue windowed");
	cvNamedWindow("luminance thresholded");
	cvNamedWindow("saturation thresholded");
	cvNamedWindow("sl thresholded");
	cvNamedWindow("black"); 
	cvNamedWindow("white"); 
	cvNamedWindow("black dilated"); 
	cvNamedWindow("intersection"); 
	cvNamedWindow("intersection contours"); 

	int blackThreshold = 255; 
	cvCreateTrackbar("level", "black", &blackThreshold, 256, NULL); 

	int whiteThreshold = 200; 
	cvCreateTrackbar("level", "white", &whiteThreshold, 256, NULL); 

	int luminanceThreshold = 200; 
	cvCreateTrackbar("level", "luminance thresholded", &luminanceThreshold, 256, NULL); 

	int saturationThreshold = 200; 
    cvCreateTrackbar("level", "saturation thresholded", &saturationThreshold, 256, NULL); 

	// 150 appears to be the color of the sticky notes I'm testing with
	int hueCenter = 150; 
	cvCreateTrackbar("center", "hue windowed", &hueCenter, 255, NULL); 

	int hueWindow = 10; 
	cvCreateTrackbar("window", "hue windowed", &hueWindow, 128, NULL); 

	int dilations = 1;
	cvCreateTrackbar("iterations", "black dilated", &dilations, 5, NULL); 

	IplConvKernel* morphKernel = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT); 

	CvMemStorage* storage = cvCreateMemStorage(); 

	for (;;)
	{
		update(); 

		int key = cvWaitKey(33); 

		if (key == 'q')
		{
			break; 
		}

		cvCvtColor(g_raw, converted, CV_BGR2HLS); 
		cvSplit(converted, hue, luminance, saturation, NULL); 
		cvThreshold(luminance, black, blackThreshold, 127, CV_THRESH_BINARY_INV); 
		cvThreshold(luminance, white, whiteThreshold, 127, CV_THRESH_BINARY); 

		// TODO: Deal with the fact that hue wraps around
		for (int y = 0; y < hue->height; ++y)
		{
			char* hueRow = hue->imageData + (y * hue->widthStep); 
			char* hueWindowedRow = hueWindowed->imageData + (y * hueWindowed->widthStep); 

			for (int x = 0; x < hue->width; ++x)
			{
				unsigned char* src = (unsigned char*) hueRow + x; 
				unsigned char* dest = (unsigned char*) hueWindowedRow + x; 
				
				int h = *src; 

				if (abs(h - hueCenter) < hueWindow)
				{
					*dest = 85; 
				}
				else
				{
					*dest = 0; 
				}
			}
		}

		cvThreshold(luminance, luminanceThresholded, luminanceThreshold, 85, CV_THRESH_BINARY); 
		cvThreshold(saturation, saturationThresholded, saturationThreshold, 85, CV_THRESH_BINARY); 
		cvAdd(luminanceThresholded, saturationThresholded, slThresholded); 
		cvAdd(slThresholded, hueWindowed, slThresholded); 
		cvThreshold(slThresholded, slThresholded, 250, 255, CV_THRESH_BINARY); 

		cvDilate(black, blackDilated, morphKernel, dilations); 
		cvAdd(blackDilated, white, intersection); 
		cvThreshold(intersection, intersection, 250, 255, CV_THRESH_BINARY); 

		CvSeq* contours = FindContours(intersection, storage, g_contourPolyPrecision / 10.0F, (float) g_segmentThreshold);
		cvScale(g_raw, intersectionContours, 0.25); 
		DrawContours(intersectionContours, contours, green); 

		cvShowImage("hue", hue); 
		cvShowImage("luminance", luminance); 
		cvShowImage("saturation", saturation); 
		cvShowImage("black", black); 
		cvShowImage("white", white);
		cvShowImage("black dilated", blackDilated); 
		cvShowImage("intersection", intersection); 
		cvShowImage("intersection contours", intersectionContours); 
		cvShowImage("hue windowed", hueWindowed); 
		cvShowImage("luminance thresholded", luminanceThresholded); 
		cvShowImage("saturation thresholded", saturationThresholded); 
		cvShowImage("sl thresholded", slThresholded); 
	}

	cvDestroyWindow("hue"); 
	cvDestroyWindow("luminance"); 
	cvDestroyWindow("saturation"); 
	cvDestroyWindow("black"); 
	cvDestroyWindow("white"); 
	cvDestroyWindow("blackDilated"); 
	cvDestroyWindow("intersection"); 
	cvDestroyWindow("intersection contours"); 
	cvDestroyWindow("hue windowed"); 
	cvDestroyWindow("luminance thresholded"); 
	cvDestroyWindow("saturation thresholded"); 
	cvDestroyWindow("sl thresholded"); 
	cvReleaseImage(&hue);
	cvReleaseImage(&luminance);
	cvReleaseImage(&saturation);
	cvReleaseImage(&black);
	cvReleaseImage(&white);
	cvReleaseImage(&blackDilated);
	cvReleaseImage(&intersection);
	cvReleaseImage(&luminanceThresholded); 
	cvReleaseImage(&saturationThresholded); 
	cvReleaseImage(&slThresholded); 
}

int _tmain(int, _TCHAR*)
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

	int maxAge = 10; 

	//cvCreateTrackbar("threshold", "thresholded", &g_threshold, 50, update_threshold); 
	//cvCreateTrackbar("max_depth", "contours", &g_maxDepth, 20, update_contours); 
	//cvCreateTrackbar("poly precision", "contours", &g_contourPolyPrecision, 50, update_contours); 
	//cvCreateTrackbar("seg thresh", "contours", &g_segmentThreshold, 100, update_contours); 
	//cvCreateTrackbar("collin thresh", "contours", &g_collinearityThreshold, 200, update_contours); 
	//cvCreateTrackbar("line gap", "contours", &g_maxLineGap, 200, update_contours); 
	//cvCreateTrackbar("angle", "contours", &g_maxAngle, 100, update_contours); 
	cvCreateTrackbar("area", "contours", &g_minArea, 100, NULL); 
	cvCreateTrackbar("move thresh", "contours", &g_trackingThreshold, 100, NULL); 
	cvCreateTrackbar("align thresh", "contours", &g_alignmentThreshold, 300, NULL); 
	cvCreateTrackbar("max age", "contours", &maxAge, 200, NULL); 
	
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

	CvSeq* borders = cvCreateSeq(0, sizeof(CvSeq), sizeof(Border), g_storage); 

	for (;;)
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
			//g_camera = g_camera ? 0 : 1; 
			//cvReleaseCapture(&g_capture); 
			//g_capture = cvCreateCameraCapture(g_camera); 
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
		else if (key == 'h')
		{
			ThresholdExperiment(); 
		}
		else
		{
			TrackBorders(g_grabbed, borders, maxAge); 
			cvCopy(g_grabbed, g_contours); 
			cvScale(g_contours, g_contours, 0.25); 
			//DrawContours(g_contours, contours, red); 
			//DrawQuadrangles(g_contours, rejects, yellow); 
			//DrawQuadrangles(g_contours, quadrangles, blue); 
			DrawBorders(g_contours, borders, green, CV_RGB(0, 256/(maxAge * 2), 0)); 

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


