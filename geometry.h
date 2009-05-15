typedef struct LineSegment
{
	CvPoint p1; 
	CvPoint p2; 
} LineSegment; 

typedef struct Quadrangle
{
	CvPoint2D32f p[4]; 
} Quadrangle; 

float min4(float a, float b, float c, float d)
{
	return min(a, min(b, min(c, d))); 
}

float max4(float a, float b, float c, float d)
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

float length2(CvPoint* p1, CvPoint* p2)
{
	float dx = (float) (p1->x - p2->x); 
	float dy = (float) (p1->y - p2->y); 

	return ((dx * dx) + (dy * dy)); 
}

float length2(CvPoint2D32f* p1, CvPoint2D32f* p2)
{
	float dx = p1->x - p2->x; 
	float dy = p1->y - p2->y; 

	return ((dx * dx) + (dy * dy)); 
}

float length2(LineSegment* s)
{
	return length2(&(s->p1), &(s->p2)); 
}

float length(CvPoint* p1, CvPoint* p2)
{
	return sqrt(length2(p1, p2)); 
}

float length(CvPoint2D32f* p1, CvPoint2D32f* p2)
{
	return sqrt(length2(p1, p2)); 
}

float length(LineSegment* s)
{
	return length(&(s->p1), &(s->p2)); 
}

float DotNormal(LineSegment* s1, LineSegment* s2)
{
	float l1 = length(s1); 
	float l2 = length(s2); 
	float x1 = (s1->p2.x - s1->p1.x) / l1; 
	float x2 = (s2->p2.x - s2->p1.x) / l2; 
	float y1 = (s1->p2.y - s1->p1.y) / l1; 
	float y2 = (s2->p2.y - s2->p1.y) / l2; 

	return x1 * x2 + y1 * y2; 
}

float Dot(LineSegment* s1, LineSegment* s2)
{
	float x1 = (float) (s1->p2.x - s1->p1.x); 
	float x2 = (float) (s2->p2.x - s2->p1.x); 
	float y1 = (float) (s1->p2.y - s1->p1.y); 
	float y2 = (float) (s2->p2.y - s2->p1.y); 

	return x1 * x2 + y1 * y2; 
}

//float Collinearity(LineSegment* s1, LineSegment* s2)
//{
//	return 1.0 - fabs(Dot(s1, s2) / (length(s1) * length(s2))); 
//}

float IndexOnLineNearestPoint(CvPoint* pt, LineSegment* s)
{
	float un = (float) (((pt->x - s->p1.x) * (s->p2.x - s->p1.x)) + ((pt->y - s->p1.y) * (s->p2.y - s->p1.y)));
	float ud = (float) (((s->p1.x - s->p2.x) * (s->p1.x - s->p2.x)) + ((s->p1.y - s->p2.y) * (s->p1.y - s->p2.y)));
	return un / ud; 
}
bool Intersection(LineSegment* s1, LineSegment* s2, CvPoint2D32f* p, float* u1, float* u2)
{
	float x1 = (float) s1->p1.x; 
	float x2 = (float) s1->p2.x; 
	float x3 = (float) s2->p1.x; 
	float x4 = (float) s2->p2.x; 

	float y1 = (float) s1->p1.y; 
	float y2 = (float) s1->p2.y; 
	float y3 = (float) s2->p1.y; 
	float y4 = (float) s2->p2.y; 

	float udenom = ((y4 - y3) * (x2 - x1)) - ((x4 - x3) * (y2 - y1)); 

	if (udenom == 0)
	{
		return false; // Parallel or coincident
	}

	float u1num = ((x4 - x3) * (y1 - y3)) - ((y4 - y3) * (x1 - x3)); 
	float u2num = ((x2 - x1) * (y1 - y3)) - ((y2 - y1) * (x1 - x3)); 

	*u1 = u1num / udenom; 
	*u2 = u2num / udenom; 

	p->x = x1 + (*u1 * (x2 - x1)); 
	p->y = y1 + (*u1 * (y2 - y1)); 

	return true; 
}
CvPoint PointAlongLine(float u, LineSegment* s)
{
	float x = (float) s->p1.x + (u * (float) (s->p2.x - s->p1.x)); 
	float y = (float) s->p1.y + (u * (float) (s->p2.y - s->p1.y)); 

	// TODO: Round? 
	return cvPoint((int)x, (int)y);
}

float PointToLineDistance(CvPoint* pt, LineSegment* s)
{
	float u = IndexOnLineNearestPoint(pt, s); 

	CvPoint nearest = PointAlongLine(u, s); 
	return length(pt, &nearest); 
}

CvPoint2D32f Unitize(LineSegment* s)
{
	float len = length(s); 
	return cvPoint2D32f((s->p2.x - s->p1.x) / len, (s->p2.y - s->p1.y) / len); 
}

// Returns the square of the "gap" between two line segments, which is the 
// distance between their two nearest endpoints
float Gap2(LineSegment* s1, LineSegment* s2)
{
	float gap11 = length2(&(s1->p1), &(s2->p1)); 
	float gap12 = length2(&(s1->p1), &(s2->p2)); 
	float gap21 = length2(&(s1->p2), &(s2->p1));
	float gap22 = length2(&(s1->p2), &(s2->p2)); 

	return min4(gap11, gap12, gap21, gap22); 
}
