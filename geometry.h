typedef struct LineSegment
{
	CvPoint p1; 
	CvPoint p2; 
} LineSegment; 

typedef struct Quadrangle
{
	CvPoint2D32f p[4]; 
} Quadrangle; 

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
