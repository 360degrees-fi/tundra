/** @file
    @author Jukka Jyl�nki

    This work is copyrighted material and may NOT be used for any kind of commercial or 
    personal advantage and may NOT be copied or redistributed without prior consent
    of the author(s). 
*/
#include "StableHeaders.h"
#ifdef MATH_ENABLE_STL_SUPPORT
#include <utility>
#endif
#include "MathFunc.h"
#include "Array.h"
#include "OBB.h"
#include "AABB.h"
#include "LCG.h"
#include "LineSegment.h"
#include "Line.h"
#include "Ray.h"
#include "Plane.h"
#include "Sphere.h"
#include "float2.h"
#include "float3x3.h"
#include "float3x4.h"
#include "float4.h"
#include "float4x4.h"
#include "Quat.h"
#include "Triangle.h"

#ifdef ANDROID
#include "AndroidCore.h"
#endif

using namespace kNet; // for kNet::Array.

Sphere::Sphere(const float3 &center, float radius)
:pos(center), r(radius) 
{
}

Sphere::Sphere(const float3 &pointA, const float3 &pointB)
{
    pos = (pointA + pointB) / 2.f;
    r = (pointB - pos).Length();
}

Sphere::Sphere(const float3 &pointA, const float3 &pointB, const float3 &pointC)
{
    // See e.g. http://en.wikipedia.org/wiki/Circumcenter .

    float3 b = pointB - pointA;
    float3 c = pointC - pointA;
    float3 normal = Cross(b, c);
    float denom = 2.f * normal.LengthSq();
    if (EqualAbs(denom, 0.f))
    {
        SetNegativeInfinity();
        return;
    }

#if 0
    {
        // The three points are collinear. Construct a line through two most extremal points.
        float dC = Dot(b,c);

        if (dC < 0.f)
            *this = Sphere(pointB, pointC);
        else
        {
            float dB = Dot(b, b);
            if (dC > dB)
                *this = Sphere(pointA, pointC);
            else
                *this = sphere(pointA, pointB);
        }
        return;
    }
#endif

    pos = (c.LengthSq() * Cross(normal, c) + b.LengthSq() * Cross(b, normal)) / denom;
    r = pos.Length();
    pos += pointA;

/* // An alternate formulation that is probably correct, but the above contains fewer operations.
   // This one contains a matrix inverse operation.
    float3x3 m;
    m.SetRow(0, pointB - pointA);
    m.SetRow(1, pointC - pointA);
    m.SetRow(2, Cross(m.Row(0), m.Row(1)));
    float3 lengths = float3(m.Row(0).LengthSq(), m.Row(1).LengthSq(), 0.f) * 0.5f;

    bool success = m.Inverse();
    if (!success)
    {
        SetNegativeInfinity();
        return;
    }

    pos = m * lengths;
    r = pos.Length();
    pos += pointA;
*/
}

Sphere::Sphere(const float3 &pointA, const float3 &pointB, const float3 &pointC, const float3 &pointD)
{
    float3x3 m;
    m.SetRow(0, pointB - pointA);
    m.SetRow(1, pointC - pointA);
    m.SetRow(2, pointD - pointA);
    float3 lengths = float3(m.Row(0).LengthSq(), m.Row(1).LengthSq(), m.Row(2).LengthSq()) * 0.5f;

    bool success = m.Inverse();
    if (!success)
    {
        SetNegativeInfinity();
        return;
    }

    pos = m * lengths;
    r = pos.Length();
    pos += pointA;
}

AABB Sphere::MinimalEnclosingAABB() const
{
    AABB aabb;
    aabb.SetFrom(*this);
    return aabb;
}

AABB Sphere::MaximalContainedAABB() const
{
    AABB aabb;
    aabb.SetCenter(pos, float3(r,r,r));
    return aabb;
}

void Sphere::SetNegativeInfinity()
{
    pos = float3(0,0,0);
    r = -FLOAT_INF;
}

float Sphere::Volume() const
{
    return 4.f * pi * r*r*r / 3.f;
}

float Sphere::SurfaceArea() const
{
    return 4.f * pi * r*r;
}

bool Sphere::IsFinite() const
{
    return pos.IsFinite() && isfinite(r);
}

bool Sphere::IsDegenerate() const
{
    return r < 0.f;
}

bool Sphere::Contains(const float3 &point) const
{
    return pos.DistanceSq(point) <= r*r;
}

Sphere Sphere::FastEnclosingSphere(const float3 *pts, int numPoints)
{
    Sphere s;
    if (numPoints == 0)
    {
        s.SetNegativeInfinity();
        return s;
    }
    assert(pts);

    // First pass: Pick the cardinal axis (X,Y or Z) which has the two most distant points.
    int minx, maxx, miny, maxy, minz, maxz;
    AABB::ExtremePointsAlongAABB(pts, numPoints, minx, maxx, miny, maxy, minz, maxz);
    float dist2x = pts[minx].DistanceSq(pts[maxx]);
    float dist2y = pts[miny].DistanceSq(pts[maxy]);
    float dist2z = pts[minz].DistanceSq(pts[maxz]);

    int min = minx;
    int max = maxx;
    if (dist2y > dist2x && dist2y > dist2z)
    {
        min = miny;
        max = maxy;
    }
    else if (dist2z > dist2x && dist2z > dist2y)
    {
        min = minz;
        max = maxz;
    }

    // The two points on the longest axis define the initial sphere.
    s.pos = (pts[min] + pts[max]) / 2.f;
    s.r = pts[min].Distance(s.pos);

    // Second pass: Make sure each point lies inside this sphere, expand if necessary.
    for(int i = 0; i < numPoints; ++i)
        s.Enclose(pts[i]);
    return s;
}

/** This implementation was adapted from Christer Ericson's Real-time Collision Detection, pp. 99-100. */
/*
Sphere WelzlSphere(const float3 *pts, int numPoints, float3 *support, int numSupports)
{
    if (numPoints == 0)
    {
        switch(numSupports)
        {
        default: assert(false);
        case 0: return Sphere();
        case 1: return Sphere(support[0], 0.f);
        case 2: return Sphere(support[0], support[1]);
        case 3: return Sphere(support[0], support[1], support[2]);
        case 4: return Sphere(support[0], support[1], support[2], support[3]);
        }
    }

    ///\todo The following recursion can easily crash the stack for large inputs.  Convert this to proper form.
    Sphere smallestSphere = WelzlSphere(pts, numPoints - 1, support, numSupports);
    if (smallestSphere.Contains(pts[numPoints-1]))
        return smallestSphere;
    support[numSupports] = pts[numPoints-1];
    return WelzlSphere(pts, numPoints - 1,  support, numSupports + 1);
}
*/
/*
Sphere Sphere::OptimalEnclosingSphere(const float3 *pts, int numPoints)
{
    float3 support[4];
    WelzlSphere(pts, numPoints, &support, 0);
}
*/
/*
Sphere Sphere::ApproximateEnclosingSphere(const float3 *pointArray, int numPoints)

*/
float Sphere::Distance(const float3 &point) const
{
    return Max(0.f, pos.Distance(point) - r);
}

float3 Sphere::ClosestPoint(const float3 &point) const
{
    float d = pos.Distance(point);
    float t = (d >= r ? r : d);
    return pos + (point - pos) * (t / d);
}

bool Sphere::Intersects(const Sphere &sphere) const
{
    return (pos - sphere.pos).LengthSq() <= r*r + sphere.r*sphere.r;
}

bool IntersectLineSphere(const float3 &lPos, const float3 &lDir, const Sphere &s, float &t)
{
    assume(lDir.IsNormalized());

    const float3 dist = lPos - s.pos;
	const float distSq = dist.LengthSq();
	const float radSq = s.r*s.r;

	const float b = 2.f * Dot(lDir, dist);
	const float c = distSq - radSq;
	const float D = b*b - 4.f*c;
	if (D <= 0.f)
		return false;  // The ray doesn't even come near.

	t = (-b - sqrtf(D)) * 0.5f;
    return true;
}

bool Sphere::Intersects(const Ray &r, float3 *intersectionPoint, float3 *intersectionNormal, float *d) const
{
    float t;
    IntersectLineSphere(r.pos, r.dir, *this, t);
	if (t < 0.f)
		return false; // The intersection position is on the negative direction of the ray.

    float3 hitPoint = r.pos + t * r.dir;
    if (intersectionPoint)
	    *intersectionPoint = hitPoint;
    if (intersectionNormal)
        *intersectionNormal = (hitPoint - pos).Normalized();
    if (d)
        *d = t;
//	return (distSq <= radSq) ? IntersectBackface : IntersectFrontface;
    return true;
}

bool Sphere::Intersects(const Line &l, float3 *intersectionPoint, float3 *intersectionNormal, float *d) const
{
    float t;
    IntersectLineSphere(l.pos, l.dir, *this, t);

    float3 hitPoint = l.pos + t * l.dir;
    if (intersectionPoint)
	    *intersectionPoint = hitPoint;
    if (intersectionNormal)
        *intersectionNormal = (hitPoint - pos).Normalized();
    if (d)
        *d = t;
//	return (distSq <= radSq) ? IntersectBackface : IntersectFrontface;
    return true;
}

bool Sphere::Intersects(const LineSegment &l, float3 *intersectionPoint, float3 *intersectionNormal, float *d) const
{
    float t;
    IntersectLineSphere(l.a, l.Dir(), *this, t);

    float lineLength = l.Length();
    if (t < 0.f || t > lineLength)
        return false;
    float3 hitPoint = l.GetPoint(t / lineLength);
    if (intersectionPoint)
	    *intersectionPoint = hitPoint;
    if (intersectionNormal)
        *intersectionNormal = (hitPoint - pos).Normalized();
    if (d)
        *d = t;
//	return (distSq <= radSq) ? IntersectBackface : IntersectFrontface;
    return true;
}

bool Sphere::Intersects(const Plane &plane) const
{
    return plane.Intersects(*this);
}

bool Sphere::Intersects(const AABB &aabb, float3 *closestPointOnAABB) const
{
    return aabb.Intersects(*this, closestPointOnAABB);
}

bool Sphere::Intersects(const OBB &obb, float3 *closestPointOnOBB) const
{
    return obb.Intersects(*this, closestPointOnOBB);
}

bool Sphere::Intersects(const Triangle &triangle, float3 *closestPointOnTriangle) const
{
    return triangle.Intersects(*this, closestPointOnTriangle);
}

/*
float Sphere::Distance(const float3 &point, float3 &outClosestPointOnSphere) const

bool Sphere::Intersect(const AABB &aabb) const
bool Sphere::Intersect(const OBB &obb) const
bool Sphere::Intersect(const Plane &plane) const
bool Sphere::Intersect(const Ellipsoid &ellipsoid) const
bool Sphere::Intersect(const Triangle &triangle) const
bool Sphere::Intersect(const Cylinder &cylinder) const
bool Sphere::Intersect(const Torus &torus) const
bool Sphere::Intersect(const Frustum &frustum) const
bool Sphere::Intersect(const Polygon &polygon) const
bool Sphere::Intersect(const Polyhedron &polyhedron) const

void Sphere::Enclose(const Triangle &triangle)
void Sphere::Enclose(const Polygon &polygon)
bool Sphere::Enclose(const Polyhedron &polyhedron)
*/

void Sphere::Enclose(const float3 &point)
{
    float3 d = point - pos;
    float dist2 = d.LengthSq();
    if (dist2 > r*r)
    {
        float dist = sqrt(dist2);
        float newRadius = (r + dist) / 2.f;
        pos += d * (newRadius - r) / dist;
        r = newRadius;
    }
}

void Sphere::Enclose(const AABB &aabb)
{
    ///\todo This might not be very optimal at all. Perhaps better to enclose the farthest point first.
    for(int i = 0; i < 8; ++i)
        Enclose(aabb.CornerPoint(i));
}

void Sphere::Enclose(const OBB &obb)
{
    ///\todo This might not be very optimal at all. Perhaps better to enclose the farthest point first.
    for(int i = 0; i < 8; ++i)
        Enclose(obb.CornerPoint(i));
}

void Sphere::Enclose(const Sphere &sphere)
{
    // To enclose another sphere into this sphere, we can simply enclose the farthest point
    // of that sphere to this sphere.
    float3 farthestPoint = sphere.pos - pos;
    farthestPoint = sphere.pos + farthestPoint * (sphere.r / farthestPoint.Length());
    Enclose(farthestPoint);
}

void Sphere::Enclose(const LineSegment &lineSegment)
{
    ///\todo This might not be very optimal at all. Perhaps better to enclose the farthest point first.
    Enclose(lineSegment.a);
    Enclose(lineSegment.b);
}

void Sphere::Enclose(const float3 *pointArray, int numPoints)
{
    ///\todo This might not be very optimal at all. Perhaps better to enclose the farthest point first.
    for(int i = 0; i < numPoints; ++i)
        Enclose(pointArray[i]);
}

int Sphere::Triangulate(float3 *outPos, float3 *outNormal, float2 *outUV, int numVertices)
{
	assert(outPos != 0);
	assume(this->r > 0.f);

	Array<Triangle> temp;

	// Start subdividing from a tetrahedron.
	float3 xp(r,0,0);
	float3 xn(-r,0,0);
	float3 yp(0,r,0);
	float3 yn(0,-r,0);
	float3 zp(0,0,r);
	float3 zn(0,0,-r);

	temp.push_back(Triangle(yp,xp,zp));
	temp.push_back(Triangle(xp,yp,zn));
	temp.push_back(Triangle(yn,zp,xp));
	temp.push_back(Triangle(yn,xp,zn));
	temp.push_back(Triangle(zp,xn,yp));
	temp.push_back(Triangle(yp,xn,zn));
	temp.push_back(Triangle(yn,xn,zp));
	temp.push_back(Triangle(xn,yn,zn));

	int oldEnd = 0;
	while(((int)temp.size()-oldEnd+3)*3 <= numVertices)
	{
		Triangle cur = temp[oldEnd];
		float3 a = ((cur.a + cur.b) * 0.5f).ScaledToLength(this->r);
		float3 b = ((cur.a + cur.c) * 0.5f).ScaledToLength(this->r);
		float3 c = ((cur.b + cur.c) * 0.5f).ScaledToLength(this->r);

		temp.push_back(Triangle(cur.a, a, b));
		temp.push_back(Triangle(cur.b, c, a));
		temp.push_back(Triangle(cur.c, b, c));
		temp.push_back(Triangle(a, c, b));

		++oldEnd;
	}
	// Check that we really did tessellate as many new triangles as possible.
	assert(((int)temp.size()-oldEnd)*3 <= numVertices && ((int)temp.size()-oldEnd)*3 + 9 > numVertices);

	for(size_t i = oldEnd, j = 0; i < temp.size(); ++i, ++j)
	{
		outPos[3*j] = this->pos + temp[i].a;
		outPos[3*j+1] = this->pos + temp[i].b;
		outPos[3*j+2] = this->pos + temp[i].c;
	}

	if (outNormal)
		for(size_t i = oldEnd, j = 0; i < temp.size(); ++i, ++j)
		{
			outNormal[3*j] = temp[i].a.Normalized();
			outNormal[3*j+1] = temp[i].b.Normalized();
			outNormal[3*j+2] = temp[i].c.Normalized();
		}

	if (outUV)
		for(size_t i = oldEnd, j = 0; i < temp.size(); ++i, ++j)
		{
			outUV[3*j] = float2(atan2(temp[i].a.y, temp[i].a.x) / (2.f * 3.141592654f) + 0.5f, (temp[i].a.z + r) / (2.f * r));
			outUV[3*j+1] = float2(atan2(temp[i].b.y, temp[i].b.x) / (2.f * 3.141592654f) + 0.5f, (temp[i].b.z + r) / (2.f * r));
			outUV[3*j+2] = float2(atan2(temp[i].c.y, temp[i].c.x) / (2.f * 3.141592654f) + 0.5f, (temp[i].c.z + r) / (2.f * r));
		}

	return ((int)temp.size() - oldEnd) * 3;
}

float3 Sphere::RandomPointInside(LCG &lcg)
{
    assume(r > 1e-3f);
	for(int i = 0; i < 1000; ++i)
	{
		float x = lcg.Float(-r, r);
		float y = lcg.Float(-r, r);
		float z = lcg.Float(-r, r);
		if (x*x + y*y + z*z <= r*r)
			return pos + float3(x,y,z);
	}
    assume(false && "Sphere::RandomPointInside failed!");

	/// Failed to generate a point inside this sphere. Return the sphere center as fallback.
	return pos;
}

float3 Sphere::RandomPointOnSurface(LCG &lcg)
{
    assume(r > 1e-3f);
	for(int i = 0; i < 1000; ++i)
	{
		float x = lcg.Float(-r, r);
		float y = lcg.Float(-r, r);
		float z = lcg.Float(-r, r);
		float lenSq = x*x + y*y + z*z;
		if (lenSq >= 1e-6f && lenSq <= r*r)
			return pos + r / sqrt(lenSq) * float3(x,y,z);
	}
    assume(false && "Sphere::RandomPointOnSurface failed!");

	/// Failed to generate a point inside this sphere. Return an arbitrary point on the surface as fallback.
	return pos + float3(r, 0, 0);
}

float3 Sphere::RandomPointInside(LCG &lcg, const float3 &center, float radius)
{
    return Sphere(center, radius).RandomPointInside(lcg);
}

float3 Sphere::RandomPointOnSurface(LCG &lcg, const float3 &center, float radius)
{
    return Sphere(center, radius).RandomPointOnSurface(lcg);
}
