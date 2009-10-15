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

bool g_initialized = false; 

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

CvScalar blue = cvScalar(255, 0, 0);
CvScalar green = cvScalar(0, 255, 0);
CvScalar red = cvScalar(0, 0, 255);
CvScalar cyan = cvScalar(255, 255, 0);
CvScalar pink = cvScalar(255, 0, 255);
CvScalar yellow = cvScalar(0, 255, 255);


typedef struct FingertipInfo
{
public:
	float x; 
	float y; 
} FingertipInfo; 

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

		CvSeq* poly = cvApproxPoly(contour, sizeof(CvContour), storage, CV_POLY_APPROX_DP, precision); 

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
void TrackBorders(IplImage* source, 
				  CvMemStorage* storage, 
				  CvSeq* borders, 
				  int maxAge, 
				  float polyPrecision, 
				  float segmentThreshold,
				  float minAreaPct,
				  float alignmentThreshold,
				  int trackingThreshold,
				  CvSeq** pContours = NULL)
{
	CvSeq* contours; 
	if (pContours == NULL)
	{
		pContours = &contours; 
	}

	*pContours = FindContours(
		source, 
		storage, 
		polyPrecision,			//(float) g_contourPolyPrecision / 10.0F, 
		segmentThreshold);		//(float) g_segmentThreshold); 
	float minArea = minAreaPct / 100.0F * (source->width * source->height); 
	CvSeq* quadrangles = FindQuadrangles(*pContours, storage, &IsC4S4Quadrangle, minArea, 0); 
	CvSeq* gapOneSiders = FindQuadrangles(*pContours, storage, &IsC4S3Quadrangle, minArea, alignmentThreshold); 
	CvSeq* missingCorners = FindQuadrangles(*pContours, storage, &IsC3Quadrangle, minArea, 0); 

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

			int update = UpdateOf(quadrangle, borders, trackingThreshold); 

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

void ThresholdOnLuminance(IplImage* source, IplImage* hls, IplImage* luminance, IplImage*, IplImage* thresholded)
{
	cvCvtColor(source, hls, CV_BGR2Luv); 
	// cvSmooth(hls, hl, 2, 5); 
	cvSplit(hls, luminance, NULL, NULL, NULL); 
	//cvPyrDown(luminance, downsampleScratch); 
	//cvPyrUp(downsampleScratch, luminance); 
	cvSmooth(luminance, luminance, 2, 5); 
	cvAdaptiveThreshold(luminance, thresholded, 255, 0, CV_THRESH_BINARY_INV); 
}
void test(CvCapture* capture)
{
	printf("Entering test mode.\n"); 

	TestQuadrangleArea();

	IplImage* raw = cvQueryFrame(capture);
	cvShowImage("raw", raw); 
	IplImage* source = cvCloneImage(raw); 
	IplImage* trackingImage = cvCloneImage(raw); 
	IplImage* hls = cvCloneImage(source); 
	IplImage* l = cvCreateImage(cvGetSize(hls), hls->depth, 1); 
	IplImage* downsampleScratch = cvCreateImage(cvSize(l->width / 2, l->height / 2), l->depth, 1); 
	IplImage* thresholded = cvCloneImage(l); 
	IplImage* trackingSource = cvCloneImage(thresholded); 

	cvNamedWindow("test source"); 

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
				raw = cvQueryFrame(capture); 
				cvShowImage("raw", raw); 
				cvCopyImage(raw, source); 
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
		cvShowImage("test source", source); 

		ThresholdOnLuminance(source, hls, l, downsampleScratch, thresholded); 
		cvShowImage("thresholded", thresholded); 
		cvCopy(thresholded, trackingSource); 

		float minArea = g_minArea / 100.0F * (source->width * source->height);
		CvSeq* contours; 
		TrackBorders(
			trackingSource, 
			storage, 
			borders, 
			maxAge, 
			(float) g_contourPolyPrecision / 10.0F, 
			(float) g_contourPolyPrecision / 10.0F,
			(float) g_minArea, 
			(float) g_alignmentThreshold, 
			g_trackingThreshold, 
			&contours); 
		CvSeq* quadrangles = FindQuadrangles(contours, storage, &IsC4S4Quadrangle, minArea, 0);
		CvSeq* gapOneSiders = FindQuadrangles(contours, storage, &IsC4S3Quadrangle, minArea, (float) g_alignmentThreshold); 
		CvSeq* missingCorner = FindQuadrangles(contours, storage, &IsC3Quadrangle, minArea, 0); 
		
		cvScale(source, trackingImage, 0.25); 
		if (drawSegments)
		{
			DrawContours(trackingImage, contours, red); 
		}
		DrawQuadrangles(trackingImage, quadrangles, green, 1); 
		DrawQuadrangles(trackingImage, gapOneSiders, blue, 1); 
		DrawQuadrangles(trackingImage, missingCorner, yellow, 1); 
		DrawBorders(trackingImage, borders, pink, CV_RGB(256/(maxAge * 2), 256/(maxAge * 2), 0), 2); 
		cvShowImage("tracking", trackingImage); 
	}

	cvReleaseMemStorage(&storage); 
	cvDestroyWindow("test source"); 

	printf("Exiting test mode.\n"); 
}

void Window(IplImage* src, IplImage* dest, int lowThresh, int highThresh, unsigned char value1, unsigned char value2)
{
	for (int y = 0; y < src->height; ++y)
	{
		char* srcRow = src->imageData + (y * src->widthStep); 
		char* destRow = dest->imageData + (y * dest->widthStep); 

		for (int x = 0; x < src->width; ++x)
		{
			unsigned char* srcData = (unsigned char*) srcRow + x; 
			unsigned char* destData = (unsigned char*) destRow + x; 

			int v = *srcData; 

			if (lowThresh < highThresh)
			{
				if ((v <= highThresh) && (v >= lowThresh))
				{
					*destData = value1; 
				}
				else
				{
					*destData = value2; 
				}
			}
			else
			{
				if ((v <= highThresh) || (v >= lowThresh))
				{
					*destData = value1; 
				}
				else
				{
					*destData = value2; 
				}
			}
		}
	}

}
void GreyDistance(IplImage* src, IplImage* grey, IplImage* dest)
{
	for (int y = 0; y < src->height; ++y)
	{
		char* srcRow = src->imageData + (y * src->widthStep); 
		char* destRow = dest->imageData + (y * dest->widthStep); 
		char* greyRow = grey->imageData + (y * grey->widthStep); 

		for (int x = 0; x < src->width; ++x)
		{
			unsigned char* srcData = (unsigned char*) srcRow + (x * 3); 
			unsigned char* destData = (unsigned char*) destRow + x; 
			unsigned char* greyData = (unsigned char*) greyRow + x; 

			int b = srcData[0]; 
			int g = srcData[1];
			int r = srcData[2]; 

			int m = (b + g + r) / 3; 

			*greyData = (unsigned char) m; 
			*destData = (unsigned char) sqrt((float) ((b - m) * (b - m)) + (float) ((g - m) * (g - m)) + (float) ((r - m) + (r -m))); 
		}
	}
}
void ThresholdExperiment(CvCapture* capture)
{
	IplImage* raw = cvQueryFrame(capture); 
	cvShowImage("raw", raw); 

	IplImage* converted = cvCloneImage(raw); 
	IplImage* channel1 = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* channel2 = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* channel3 = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* channel1Thresholded = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* channel2Thresholded = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* channel3Thresholded = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* combined = cvCreateImage(cvGetSize(raw), raw->depth, 1);  
	IplImage* combinedThresholded = cvCloneImage(combined); 
	IplImage* contourScratch = cvCloneImage(combinedThresholded); 
	IplImage* closed = cvCloneImage(combinedThresholded); 
	IplImage* final = cvCloneImage(raw); 

	cvNamedWindow("channel1");
	cvNamedWindow("channel2");
	cvNamedWindow("channel3");
	cvNamedWindow("channel1 thresholded");
	cvNamedWindow("channel2 thresholded");
	cvNamedWindow("channel3 thresholded");
	cvNamedWindow("combined"); 
	cvNamedWindow("combined thresholded"); 
	cvNamedWindow("closed"); 
	cvNamedWindow("final"); 

	int channel1HighThreshold = 255; 
	int channel1LowThreshold = 0; 
	cvCreateTrackbar("high", "channel1", &channel1HighThreshold, 255, NULL); 
	cvCreateTrackbar("low", "channel1", &channel1LowThreshold, 255, NULL); 

	int channel2HighThreshold = 128; 
	int channel2LowThreshold = 0; 
	cvCreateTrackbar("high", "channel2", &channel2HighThreshold, 255, NULL); 
	cvCreateTrackbar("low", "channel2", &channel2LowThreshold, 255, NULL); 

	int channel3HighThreshold = 128; 
	int channel3LowThreshold = 0; 
	cvCreateTrackbar("high", "channel3", &channel3HighThreshold, 255, NULL); 
	cvCreateTrackbar("low", "channel3", &channel3LowThreshold, 255, NULL); 

	int closings = 1; 
	int openings = 1; 
	cvCreateTrackbar("closings", "closed", &closings, 10, NULL); 
	cvCreateTrackbar("openings", "closed", &openings, 10, NULL); 

	int combinedThreshold = 2; 
	cvCreateTrackbar("threshold", "combined thresholded", &combinedThreshold, 3, NULL); 

	CvMemStorage* storage = cvCreateMemStorage(); 

	unsigned char channel1Value1 = 85; 
	unsigned char channel1Value2 = 0; 
	unsigned char channel2Value1 = 85; 
	unsigned char channel2Value2 = 0; 
	unsigned char channel3Value1 = 85; 
	unsigned char channel3Value2 = 0; 

	IplConvKernel* convKernel = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT); 

	bool drawContours = true; 
	bool smooth = false; 
	bool drawPolys = false; 

	int colorSpaceIndex = 3; 
	int colorSpaces[8]; 
	char* colorSpaceNames[8]; 
	colorSpaces[0] = CV_BGR2HLS; colorSpaceNames[0] = "HLS";
	colorSpaces[1] = CV_BGR2HSV; colorSpaceNames[1] = "HSV"; 
	colorSpaces[2] = CV_BGR2Lab;  colorSpaceNames[2] = "Lab";
	colorSpaces[3] = CV_BGR2Luv;  colorSpaceNames[3] = "Luv";
	colorSpaces[4] = CV_BGR2XYZ;  colorSpaceNames[4] = "XYZ";
	colorSpaces[5] = CV_BGR2YCrCb;  colorSpaceNames[5] = "YCrCb";
	colorSpaces[6] = CV_BGR2RGB;  colorSpaceNames[6] = "RGB";
	colorSpaces[7] = CV_BGR2GRAY; colorSpaceNames[7] = "Custom1"; 
	for (;;)
	{
		raw = cvQueryFrame(capture); 
		cvShowImage("raw", raw); 

		int key = cvWaitKey(33); 

		if (key == 'q')
		{
			break; 
		}
		else if (key == '1')
		{
			unsigned char temp = channel1Value2; 
			channel1Value2 = channel1Value1; 
			channel1Value1 = temp; 
		}
		else if (key == '2')
		{
			unsigned char temp = channel2Value2; 
			channel2Value2 = channel2Value1; 
			channel2Value1 = temp; 
		}
		else if (key == '3')
		{
			unsigned char temp = channel3Value2; 
			channel3Value2 = channel3Value1; 
			channel3Value1 = temp; 
		}
		else if (key == 'c')
		{
			colorSpaceIndex = (colorSpaceIndex + 1) % 8; 
			printf("Switching to %s color space\n", colorSpaceNames[colorSpaceIndex]); 
		}
		else if (key == 'd')
		{
			drawContours = !drawContours; 
		}
		else if (key == 's')
		{
			smooth = !smooth; 
		}
		else if (key == 'p')
		{
			drawPolys = !drawPolys; 
		}

		if (colorSpaceIndex < 7)
		{
			cvCvtColor(raw, converted, colorSpaces[colorSpaceIndex]); 
			cvSplit(converted, channel1, channel2, channel3, NULL); 
		}
		else
		{
			cvCvtColor(raw, converted, CV_BGR2HLS); 
			cvSplit(converted, NULL, channel3, NULL, NULL); 
			//cvSet(channel3, cvScalar(85)); 
			GreyDistance(raw, channel1, channel2); 
		}

		if (smooth)
		{
			cvSmooth(channel1, channel1, CV_GAUSSIAN, 5);
			cvSmooth(channel2, channel2, CV_GAUSSIAN, 5);
			cvSmooth(channel3, channel3, CV_GAUSSIAN, 5);
		}

		Window(channel1, channel1Thresholded, channel1LowThreshold, channel1HighThreshold, channel1Value1, channel1Value2); 
		Window(channel2, channel2Thresholded, channel2LowThreshold, channel2HighThreshold, channel2Value1, channel2Value2); 
		Window(channel3, channel3Thresholded, channel3LowThreshold, channel3HighThreshold, channel3Value1, channel3Value2); 

		cvAdd(channel1Thresholded, channel2Thresholded, combined); 
		cvAdd(channel3Thresholded, combined, combined); 

		cvThreshold(combined, combinedThresholded, (combinedThreshold * 85) - 1, 255, CV_THRESH_BINARY); 
		cvMorphologyEx(combinedThresholded, closed, NULL, convKernel, CV_MOP_OPEN, openings); 
		cvMorphologyEx(closed, closed, NULL, convKernel, CV_MOP_CLOSE, closings); 

		cvCopyImage(closed, contourScratch); 

		CvSeq* contours; 
		cvFindContours(contourScratch, storage, &contours, sizeof(CvContour), CV_RETR_LIST); 
		cvScale(raw, final, 0.25); 

		if (drawContours)
		{
			cvDrawContours(final, contours, green, blue, 5, 1, 8, cvPoint(closings, closings)); 
		}

		CvSeq* contour = contours; 
		while(contour != NULL)
		{
			CvMoments moments; 
			cvContourMoments(contour, &moments); 

			double m00 = cvGetSpatialMoment(&moments, 0, 0); 
			double m10 = cvGetSpatialMoment(&moments, 1, 0); 
			double m01 = cvGetSpatialMoment(&moments, 0, 1); 

			double x = m10/m00; 
			double y = m01/m00; 

			cvCircle(final, cvPoint((int)x + closings, (int)y + closings), 1, cyan, 1, 8);

			if (drawPolys)
			{
				CvSeq* poly = cvApproxPoly(contour, sizeof(CvContour), storage, CV_POLY_APPROX_DP, 4, 0); 
				cvDrawContours(final, poly, red, red, 5, 1, 8, cvPoint(closings, closings)); 
			}

			contour = contour->h_next; 
		}


		cvShowImage("channel1", channel1); 
		cvShowImage("channel2", channel2); 
		cvShowImage("channel3", channel3); 
		cvShowImage("channel1 thresholded", channel1Thresholded); 
		cvShowImage("channel2 thresholded", channel2Thresholded); 
		cvShowImage("channel3 thresholded", channel3Thresholded); 
		cvShowImage("combined", combined); 
		cvShowImage("combined thresholded", combinedThresholded); 
		cvShowImage("closed", closed); 
		cvShowImage("final", final); 
	}

	cvDestroyWindow("channel1");
	cvDestroyWindow("channel2");
	cvDestroyWindow("channel3");
	cvDestroyWindow("channel1 thresholded");
	cvDestroyWindow("channel2 thresholded");
	cvDestroyWindow("channel3 thresholded");
	cvDestroyWindow("combined"); 
	cvDestroyWindow("combined thresholded"); 
	cvDestroyWindow("closed"); 
	cvDestroyWindow("final"); 

	cvReleaseMemStorage(&storage); 
}

void HistogramExperiment(CvCapture* capture)
{
	IplImage* raw = cvQueryFrame(capture); 
	IplImage* grabbed = cvCloneImage(raw); 
	IplImage* thresholded = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* hls = cvCloneImage(grabbed); 
	IplImage* l = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* downsampleScratch = cvCreateImage(cvSize(l->width / 2, l->height / 2), l->depth, 1); 
	IplImage* trackingSource = cvCloneImage(thresholded); 
	IplImage* mask = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	
	CvMemStorage* storage = cvCreateMemStorage(); 
	CvSeq* borders = cvCreateSeq(0, sizeof(CvSeq), sizeof(Border), storage); 

	while (borders->total == 0)
	{
		raw = cvQueryFrame(capture); 
		cvShowImage("raw", raw); 
		ThresholdOnLuminance(raw, hls, l, downsampleScratch, thresholded); 
		cvCopy(thresholded, trackingSource); 

		TrackBorders(
			trackingSource, 
			storage, 
			borders, 
			10, 
			(float) g_contourPolyPrecision / 10.0F, 
			(float) g_segmentThreshold, 
			(float) g_minArea, 
			(float) g_alignmentThreshold, 
			g_trackingThreshold); 
	}

	CvPoint points[4]; 
	Border* border = (Border*) cvGetSeqElem(borders, 0); 
	for (int i = 0; i < 4; i++)
	{
		points[i] = Point(border->quadrangle.c[i].p); 
	}
	cvFillConvexPoly(mask, points, 4, cvScalar(255, 255, 255)); 

	cvNamedWindow("mask"); 
	cvShowImage("mask", mask); 

	IplImage* h_plane = cvCreateImage( cvGetSize(raw), 8, 1 );
	IplImage* s_plane = cvCreateImage( cvGetSize(raw), 8, 1 );
	IplImage* v_plane = cvCreateImage( cvGetSize(raw), 8, 1 );
	IplImage* planes[] = { h_plane, s_plane };
	IplImage* hsv = cvCreateImage( cvGetSize(raw), 8, 3 );
	int h_bins = 30, s_bins = 32;
	int hist_size[] = {h_bins, s_bins};
	float h_ranges[] = { 0, 180 }; /* hue varies from 0 (~0°red) to 180 (~360°red again) */
	float s_ranges[] = { 0, 255 }; /* saturation varies from 0 (black-gray-white) to 255 (pure spectrum color) */
	float* ranges[] = { h_ranges, s_ranges };
	int scale = 10;
	IplImage* hist_img = cvCreateImage( cvSize(h_bins*scale,s_bins*scale), 8, 3 );
	CvHistogram* hist;
	float max_value = 0;
	int h, s;

	cvNamedWindow( "H-S Histogram", 1 );

	for (;;)
	{
		int key = cvWaitKey(33); 
		if (key == 'q')
		{
			break; 
		}

		raw = cvQueryFrame(capture); 
		cvShowImage("raw", raw); 

		cvCvtColor(raw, hsv, CV_BGR2HSV);
		cvSplit(hsv, h_plane, s_plane, v_plane, 0);
		hist = cvCreateHist(2, hist_size, CV_HIST_ARRAY, ranges, 1);
		cvCalcHist(planes, hist, 0, mask);
		cvGetMinMaxHistValue(hist, 0, &max_value, 0, 0);
		cvZero( hist_img );

		for( h = 0; h < h_bins; h++ )
		{
			for( s = 0; s < s_bins; s++ )
			{
				float bin_val = cvQueryHistValue_2D( hist, h, s );
				int intensity = cvRound(bin_val*255/max_value);
				cvRectangle(hist_img, cvPoint( h*scale, s*scale ),
					cvPoint((h+1)*scale - 1, (s+1)*scale - 1),
					CV_RGB(intensity,intensity,intensity), 
					CV_FILLED);
				cvShowImage( "H-S Histogram", hist_img );
			}
		}

		cvScale(hist_img, hist_img, 0.5, 127); 
		cvShowImage("H-S Histogram", hist_img); 
	}
//	cvNamedWindow("back projection"); 
//
	//int patchSize = 3; 
	//IplImage* backProjection = cvCreateImage(cvSize(raw->width - patchSize + 1, raw->height - patchSize + 1), IPL_DEPTH_32F, 1); 
//
	//for (;;)
	//{
	//	int key = cvWaitKey(33); 

	//	cvCvtColor(raw, hsv, CV_BGR2HSV);
	//	cvSplit(hsv, h_plane, s_plane, v_plane, 0);

	//	cvCalcBackProjectPatch(planes, backProjection, cvSize(patchSize, patchSize), hist, CV_COMP_INTERSECT, 255); 

	//	if (key == 'q')
	//	{
	//		break; 
	//	}

	//	cvShowImage("raw", raw);
	//	cvShowImage("back projection", backProjection); 

	//	raw = cvQueryFrame(capture); 
	//}
	cvDestroyWindow("Source"); 
	cvDestroyWindow("H-S Histogram");
	cvDestroyWindow("mask"); 
	cvDestroyWindow("back projection"); 

}
void ThreeChannelWindow(IplImage* src, IplImage* dest, int ch1low, int ch1high, int ch2low, int ch2high, int ch3low, int ch3high)
{
	for (int y = 0; y < src->height; ++y)
	{
		char* srcRow = src->imageData + (y * src->widthStep); 
		char* destRow = dest->imageData + (y * dest->widthStep); 

		for (int x = 0; x < src->width; ++x)
		{
			unsigned char* srcData = (unsigned char*) srcRow + (x*3); 
			unsigned char* destData = (unsigned char*) destRow + x; 

			int v1 = *srcData; 
			int v2 = *(srcData + 1); 
			int v3 = *(srcData + 2); 

			if ((v1 <= ch1high) && (v1 >= ch1low) &&
				(v2 <= ch2high) && (v2 >= ch2low) &&
				(v3 <= ch3high) && (v1 >= ch3low))
			{
				*destData = 255; 
			}
			else
			{
				*destData = 0; 
			}
		}
	}
}
int _tmain(int, _TCHAR*)
{
	CvCapture* capture = cvCreateCameraCapture(g_camera); 

	cvNamedWindow("raw"); 
	cvNamedWindow("grabbed"); 
	cvNamedWindow("thresholded"); 
	cvNamedWindow("tracking"); 
	cvNamedWindow("fingertips"); 

	IplImage* raw = cvQueryFrame(capture); 
	cvShowImage("raw", raw); 
	IplImage* grabbed = cvCloneImage(raw); 
	IplImage* thresholded = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* luv = cvCloneImage(grabbed); 
	IplImage* l = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* fingertips = cvCreateImage(cvGetSize(raw), raw->depth, 1); 
	IplImage* contourScratch = cvCloneImage(fingertips); 
	//IplImage* downsampleScratch = cvCreateImage(cvSize(l->width / 2, l->height / 2), l->depth, 1); 
	IplImage* trackingSource = cvCloneImage(thresholded); 
	IplImage* trackingImage = cvCloneImage(raw); 

	CvMemStorage* storage = cvCreateMemStorage(); 
	IplConvKernel* convKernel = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT); 

	int maxAge = 10; 

	//cvCreateTrackbar("threshold", "thresholded", &g_threshold, 50, update_threshold); 
	//cvCreateTrackbar("max_depth", "tracking", &g_maxDepth, 20, update_contours); 
	//cvCreateTrackbar("poly precision", "tracking", &g_contourPolyPrecision, 50, update_contours); 
	//cvCreateTrackbar("seg thresh", "tracking", &g_segmentThreshold, 100, update_contours); 
	//cvCreateTrackbar("collin thresh", "tracking", &g_collinearityThreshold, 200, update_contours); 
	//cvCreateTrackbar("line gap", "tracking", &g_maxLineGap, 200, update_contours); 
	//cvCreateTrackbar("angle", "tracking", &g_maxAngle, 100, update_contours); 
	cvCreateTrackbar("area", "tracking", &g_minArea, 100, NULL); 
	cvCreateTrackbar("move thresh", "tracking", &g_trackingThreshold, 100, NULL); 
	cvCreateTrackbar("align thresh", "tracking", &g_alignmentThreshold, 300, NULL); 
	cvCreateTrackbar("max age", "tracking", &maxAge, 200, NULL); 
	
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

	CvSeq* borders = cvCreateSeq(0, sizeof(CvSeq), sizeof(Border), storage); 

	for (;;)
	{
		int key = cvWaitKey(33); 
		raw = cvQueryFrame(capture); 
		cvShowImage("raw", raw); 

#if !FRAME_AT_A_TIME
		cvCopy(raw, grabbed); 
#endif

		if ((key == 27) || (key == 'q'))
		{
			break; 
		}
		else if (key == 'c')
		{
			g_camera = g_camera ? 0 : 1; 
			cvReleaseCapture(&capture); 
			capture = cvCreateCameraCapture(g_camera); 
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
			cvCopy(raw, grabbed);  
		}
#endif
		else if (key == 't')
		{
			test(capture); 
		}
		else if (key == 'h')
		{
			ThresholdExperiment(capture); 
		}
		else if (key == 'i')
		{
			HistogramExperiment(capture); 
		}
		else
		{
			// ThresholdOnLuminance(grabbed, hls, l, downsampleScratch, thresholded); 
			cvCvtColor(grabbed, luv, CV_BGR2Luv); 
			cvSplit(luv, l, NULL, NULL, NULL); 
			cvSmooth(l, l, 2, 5); 
			cvAdaptiveThreshold(l, thresholded, 255, 0, CV_THRESH_BINARY_INV); 

			cvShowImage("thresholded", thresholded); 

			cvCopy(thresholded, trackingSource); 

			TrackBorders(
				trackingSource, 
				storage, 
				borders, 
				maxAge, 
				(float) g_contourPolyPrecision / 10.0F, 
				(float) g_segmentThreshold, 
				(float) g_minArea, 
				(float) g_alignmentThreshold, 
				g_trackingThreshold); 
			cvScale(grabbed, trackingImage, 0.25); 
			//DrawContours(g_contours, contours, red); 
			//DrawQuadrangles(g_contours, rejects, yellow); 
			//DrawQuadrangles(g_contours, quadrangles, blue); 
			DrawBorders(trackingImage, borders, pink, CV_RGB(256/(maxAge*2), 256/(maxAge * 2), 0)); 

			// Track blue regions and draw them onto the tracking image
			ThreeChannelWindow(luv, fingertips, 22, 239, 0, 95, 0, 128);

			int closings = 1; 
			int openings = 1; 
			cvMorphologyEx(fingertips, fingertips, NULL, convKernel, CV_MOP_OPEN, openings); 
			cvMorphologyEx(fingertips, fingertips, NULL, convKernel, CV_MOP_CLOSE, closings); 

			CvSeq* contours;
			cvCopy(fingertips, contourScratch); 
			cvFindContours(contourScratch, storage, &contours, sizeof(CvContour), CV_RETR_LIST); 

			cvDrawContours(trackingImage, contours, green, blue, 5, 1, 8, cvPoint(closings, closings)); 

			CvSeq* contour = contours; 
			while(contour != NULL)
			{
				CvMoments moments; 
				cvContourMoments(contour, &moments); 

				double m00 = cvGetSpatialMoment(&moments, 0, 0); 
				double m10 = cvGetSpatialMoment(&moments, 1, 0); 
				double m01 = cvGetSpatialMoment(&moments, 0, 1); 

				double x = m10/m00; 
				double y = m01/m00; 

				cvCircle(trackingImage, cvPoint((int)x + closings, (int)y + closings), 1, cyan, 1, 8);

				//if (drawPolys)
				//{
				//	CvSeq* poly = cvApproxPoly(contour, sizeof(CvContour), storage, CV_POLY_APPROX_DP, 4, 0); 
				//	cvDrawContours(final, poly, red, red, 5, 1, 8, cvPoint(closings, closings)); 
				//}

				contour = contour->h_next; 
			}

			cvShowImage("tracking", trackingImage); 
			cvShowImage("fingertips", fingertips); 
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
	cvReleaseCapture(&capture); 
	cvDestroyWindow("raw"); 
	
	return 0;
}


CvCapture* g_capture; 
IplImage* g_raw; 
IplImage* g_grabbed;
IplImage* g_thresholded;
IplImage* g_luv;
IplImage* g_l;
IplImage* g_fingertips;
IplImage* g_contourScratch;
IplImage* g_trackingSource;
IplImage* g_trackingImage;
CvMemStorage* g_storage;
IplConvKernel* g_convKernel;
CvSeq* g_borders;
CvSeq* g_fingertipInfo; 

int g_orientContourPolyPrecision = 10; 

WCPADAPI(void) Initialize()
{
	g_initialized = true; 

    g_capture = cvCreateCameraCapture(g_camera); 

	cvNamedWindow("raw"); 
	cvNamedWindow("grabbed"); 
	cvNamedWindow("thresholded"); 
	cvNamedWindow("tracking"); 
	cvNamedWindow("fingertips"); 
	cvNamedWindow("contour source"); 
	cvNamedWindow("contours"); 

	cvCreateTrackbar("poly", "contours", &g_orientContourPolyPrecision, 100, NULL); 

	g_raw = cvQueryFrame(g_capture); 
	cvShowImage("raw", g_raw); 
	
	g_grabbed = cvCloneImage(g_raw); 
	g_thresholded = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_luv = cvCloneImage(g_grabbed); 
	g_l = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_fingertips = cvCreateImage(cvGetSize(g_raw), g_raw->depth, 1); 
	g_contourScratch = cvCloneImage(g_fingertips); 
	g_trackingSource = cvCloneImage(g_thresholded); 
	g_trackingImage = cvCloneImage(g_raw); 

	g_storage = cvCreateMemStorage(); 
	g_convKernel = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT); 

	g_borders = cvCreateSeq(0, sizeof(CvSeq), sizeof(Border), g_storage); 

	g_fingertipInfo = cvCreateSeq(0, sizeof(CvSeq), sizeof(FingertipInfo), g_storage); 

}

bool CalculateFingertipCoordinates(CvSeq* borders, float x, float y, FingertipInfo* info)
{
	// The trick here is to find alpha such that m, n, and p are colinear, defining 
	// m as the point alpha*(c1-c0)+c0 (along the c0, c1 side), and n as the point
	// alpha*(c2-c3)+c3 (along the c2, c3) side. p is the fingertip point. 
	// The algorithm here does it by minimizing the area of the triangle made up
	// of m,n,p, using the formula 2A = |mx*ny+nx*py+px*my-mx*py-nx*my-px*ny|

	if (borders->total == 0)
	{
		return false; 
	}

	// For now we'll only consider the first border
	// TODO: Add support for multiple maps

	// For now, we just assume the first corner is the upper-left, and the 
	// third corner is the lower right. We'll have to change this when we 
	// get map orientation detection working. 
	Border* border = (Border*) cvGetSeqElem(borders, 0);

	CvPoint2D32f c0; 
	CvPoint2D32f c1; 
	CvPoint2D32f c2; 
	CvPoint2D32f c3; 

	c0 = border->quadrangle.c[0].p;
	c1 = border->quadrangle.c[1].p;
	c2 = border->quadrangle.c[2].p;
	c3 = border->quadrangle.c[3].p;

	CvPoint2D32f p = cvPoint2D32f(x, y);

	float a = (c1.x-c0.x)*(c2.y-c3.y) - (c2.x-c3.x)*(c1.y-c0.y); 
	float b = c0.x*(c2.y-c3.y)
			+ c3.y*(c1.x-c0.x)
			+  p.y*(c2.x-c3.x)
			+  p.x*(c1.y-c0.y)
			-  p.y*(c1.x-c0.x)
			- c0.y*(c2.x-c3.x)
			- c3.x*(c1.y-c0.y)
			-  p.x*(c2.y-c3.y); 
	float c = c0.x*c3.y
			+ c3.x*p.y
			+ p.x*c0.y
			- c0.x*p.y
			- c3.x*c0.y
			- p.x*c3.y; 

	float alpha; 

	// If a is zero, there's only one solution
	if (a == 0)
	{
		alpha = -c/b; 
	}
	// Otherwise, we solve the quadratic equation to find both
	// roots and pick the one that lies on [0,1]
	else
	{
		float d = b*b - 4*a*c; 

		if (d < 0)
		{
			return false; 
		}

		float sqrtd = sqrt(d); 

		float alpha1 = (-b + sqrtd) / (2*a); 
		float alpha2 = (-b - sqrtd) / (2*a); 


		if (alpha1 > 0 && alpha1 < 1)
		{
			alpha = alpha1; 
		}
		else if (alpha2 > 0 && alpha2 < 1)
		{
			alpha = alpha2; 
		}
		else
		{
			return false; 
		}
	}

	// Once we have alpha, beta is just the proportion of mn that mp makes up. 
	float mx = alpha * (c1.x-c0.x) + c0.x; 
	float my = alpha * (c1.y-c0.y) + c0.y; 
	float nx = alpha * (c2.x-c3.x) + c3.x;
	float ny = alpha * (c2.y-c3.y) + c3.y; 

	float ux = p.x - mx; 
	float uy = p.y - my; 
	float vx = nx - mx; 
	float vy = ny - my; 

	float l2u = ux*ux + uy*uy; 
	float l2v = vx*vx + vy*vy; 

	float beta = sqrt(l2u/l2v); 

	// If it's not inside the quadrangle, then it's not a hit
	if (beta > 0 && beta < 1)
	{
		info->x = alpha; 
		info->y = beta; 

		return true; 
	}
	
    return false; 
}

WCPADAPI(void) Test()
{
	CvMemStorage* storage = cvCreateMemStorage(); 

	CvSeq* borders = cvCreateSeq(0, sizeof(CvSeq), sizeof(Border), storage); 

	Border border; 
	border.quadrangle.c[0].p.x = 0; 
	border.quadrangle.c[0].p.y = 0; 
	border.quadrangle.c[1].p.x = 2; 
	border.quadrangle.c[1].p.y = 0; 
	border.quadrangle.c[2].p.x = 2; 
	border.quadrangle.c[2].p.y = 2; 
	border.quadrangle.c[3].p.x = 0; 
	border.quadrangle.c[3].p.y = 2; 

	cvSeqPush(borders, &border); 

	FingertipInfo info; 
	CalculateFingertipCoordinates(borders, 1, 1, &info); 
}

WCPADAPI(int) Update()
{
	cvWaitKey(1); // Easiest way to pump messages so the windows will update
	g_raw = cvQueryFrame(g_capture); 
	cvShowImage("raw", g_raw); 

	cvCopy(g_raw, g_grabbed); 

	cvCvtColor(g_grabbed, g_luv, CV_BGR2Luv); 
	cvSplit(g_luv, g_l, NULL, NULL, NULL); 
	cvSmooth(g_l, g_l, 2, 5); 
	cvAdaptiveThreshold(g_l, g_thresholded, 255, 0, CV_THRESH_BINARY_INV); 

	cvShowImage("thresholded", g_thresholded); 

	cvCopy(g_thresholded, g_trackingSource); 

#if 1
	IplImage* contourImage = cvCloneImage(g_raw); 
	IplImage* contourSource = cvCloneImage(g_l); 
	cvThreshold(contourSource, contourSource, 192, 255, CV_THRESH_BINARY); 

	cvShowImage("contour source", contourSource); 

	cvScale(g_raw, contourImage, 0.25); 

	CvSeq* lContours = FindContours(
		contourSource, 
		g_storage, 
		(float) g_orientContourPolyPrecision / 10.0F, 
		0.0F);		//(float) g_segmentThreshold); 

	DrawContours(contourImage, lContours, CV_RGB(256, 0, 0)); 
	cvShowImage("contours", contourImage); 
#endif

	const int maxAge = 10; 

	TrackBorders(
		g_trackingSource, 
		g_storage, 
		g_borders, 
		maxAge,
		(float) g_contourPolyPrecision / 10.0F, 
		(float) g_segmentThreshold, 
		(float) g_minArea, 
		(float) g_alignmentThreshold, 
		g_trackingThreshold); 
	cvScale(g_grabbed, g_trackingImage, 0.25); 
	//DrawContours(g_contours, contours, red); 
	//DrawQuadrangles(g_contours, rejects, yellow); 
	//DrawQuadrangles(g_contours, quadrangles, blue); 
	DrawBorders(g_trackingImage, g_borders, pink, CV_RGB(256/(maxAge*2), 256/(maxAge * 2), 0)); 

	// Track blue regions and draw them onto the tracking image
	cvClearSeq(g_fingertipInfo); 

	ThreeChannelWindow(g_luv, g_fingertips, 22, 239, 0, 95, 0, 128);

	int closings = 1; 
	int openings = 1; 
	cvMorphologyEx(g_fingertips, g_fingertips, NULL, g_convKernel, CV_MOP_OPEN, openings); 
	cvMorphologyEx(g_fingertips, g_fingertips, NULL, g_convKernel, CV_MOP_CLOSE, closings); 

	CvSeq* contours;
	cvCopy(g_fingertips, g_contourScratch); 
	cvFindContours(g_contourScratch, g_storage, &contours, sizeof(CvContour), CV_RETR_LIST); 

	cvDrawContours(g_trackingImage, contours, green, blue, 5, 1, 8, cvPoint(closings, closings)); 

	CvSeq* contour = contours; 
	while(contour != NULL)
	{
		CvMoments moments; 
		cvContourMoments(contour, &moments); 

		double m00 = cvGetSpatialMoment(&moments, 0, 0); 
		double m10 = cvGetSpatialMoment(&moments, 1, 0); 
		double m01 = cvGetSpatialMoment(&moments, 0, 1); 

		double x = m10/m00; 
		double y = m01/m00; 

		cvCircle(g_trackingImage, cvPoint((int)x + closings, (int)y + closings), 1, cyan, 1, 8);

		if (g_borders->total > 0)
		{
			FingertipInfo info; 
			if (CalculateFingertipCoordinates(g_borders, (float) x, (float) y, &info))
			{
				cvSeqPush(g_fingertipInfo, &info); 
			}
		}
		//if (drawPolys)
		//{
		//	CvSeq* poly = cvApproxPoly(contour, sizeof(CvContour), storage, CV_POLY_APPROX_DP, 4, 0); 
		//	cvDrawContours(final, poly, red, red, 5, 1, 8, cvPoint(closings, closings)); 
		//}

		contour = contour->h_next; 
	}

	cvShowImage("tracking", g_trackingImage); 
	cvShowImage("fingertips", g_fingertips); 

	if (g_borders->total > 0)
	{
		return g_fingertipInfo->total; 
	}
	else
	{
		return -1;
	}
}


WCPADAPI(FingertipInfo) GetFingertipInfo(int n)
{
	FingertipInfo* info = (FingertipInfo*) cvGetSeqElem(g_fingertipInfo, n); 
	return *info; 
}

WCPADAPI(void) Cleanup()
{
	g_initialized = false; 
}