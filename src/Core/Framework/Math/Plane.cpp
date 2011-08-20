/** @file
    @author Jukka Jyl�nki

    This work is copyrighted material and may NOT be used for any kind of commercial or 
    personal advantage and may NOT be copied or redistributed without prior consent
    of the author(s). 
*/
#include "StableHeaders.h"

#include "AABB.h"
#include "Circle.h"
#include "MathFunc.h"
#include "Plane.h"
#include "Line.h"
#include "OBB.h"
#include "Ray.h"
#include "Sphere.h"
#include "Triangle.h"
#include "LineSegment.h"
#include "float3x3.h"
#include "float3x4.h"
#include "float4.h"
#include "Quat.h"
#include "Frustum.h"

Plane::Plane(const float3 &normal_, float d_)
:normal(normal_), d(d_)
{
    assume(normal.IsNormalized());
}

Plane::Plane(const float3 &v1, const float3 &v2, const float3 &v3)
{
    Set(v1, v2, v3);
}

Plane::Plane(const float3 &point, const float3 &normal_)
{
    Set(point, normal_);
}

Plane::Plane(const Ray &ray, const float3 &normal)
{
	float3 perpNormal = normal - normal.ProjectToNorm(ray.dir);
	Set(ray.pos, perpNormal.Normalized());
}

Plane::Plane(const Line &line, const float3 &normal)
{
	float3 perpNormal = normal - normal.ProjectToNorm(line.dir);
	Set(line.pos, perpNormal.Normalized());
}

Plane::Plane(const LineSegment &lineSegment, const float3 &normal)
{
	float3 perpNormal = normal - normal.ProjectTo(lineSegment.b - lineSegment.a);
	Set(lineSegment.a, perpNormal.Normalized());
}

void Plane::Set(const float3 &v1, const float3 &v2, const float3 &v3)
{
    assume(!Line::AreCollinear(v1, v2, v3));
    normal = (v2-v1).Cross(v3-v1).Normalized();
    d = Dot(v1, normal);
}

void Plane::Set(const float3 &point, const float3 &normal_)
{
    normal = normal_;
    assume(normal.IsNormalized());
    d = Dot(point, normal);
}

float3 Plane::PointOnPlane() const
{
    return normal * d;
}

void Plane::Transform(const float3x3 &transform)
{
    float3x3 it = transform.InverseTransposed(); ///\todo Could optimize the inverse here by assuming orthogonality or orthonormality.
    normal = it * normal;
}

/// See Eric Lengyel's Mathematics for 3D Game Programming And Computer Graphics 2nd ed., p.110, chapter 4.2.3.
void Plane::Transform(const float3x4 &transform)
{
    ///\todo Could optimize this function by switching to plane convention ax+by+cz+d=0 instead of ax+by+cz=d.
    float3x3 r = transform.Float3x3Part();
    bool success = r.Inverse(); ///\todo Can optimize the inverse here by assuming orthogonality or orthonormality.
    assume(success);
    d = d + Dot(normal, r * transform.TranslatePart());
    normal = normal * r;
}

void Plane::Transform(const float4x4 &transform)
{
    assume(transform.Row(3).Equals(float4(0,0,0,1)));
    Transform(transform.Float3x4Part());
}

void Plane::Transform(const Quat &transform)
{
    float3x3 r = transform.ToFloat3x3();
    Transform(r);
}

bool Plane::IsInPositiveDirection(const float3 &directionVector) const
{
    assume(directionVector.IsNormalized());
    return normal.Dot(directionVector) >= 0.f;
}

bool Plane::IsOnPositiveSide(const float3 &point) const
{
    return SignedDistance(point) >= 0.f;
}

bool Plane::AreOnSameSide(const float3 &p1, const float3 &p2) const
{
    return SignedDistance(p1) * SignedDistance(p2) >= 0.f;
}

float Plane::Distance(const float3 &point) const
{
    return Abs(SignedDistance(point));
}

float Plane::SignedDistance(const float3 &point) const
{
    return normal.Dot(point) - d;
}

float3x4 Plane::OrthoProjection() const
{
    return float3x4::OrthographicProjection(*this);
}

float3x4 Plane::ObliqueProjection(const float3 &obliqueProjectionDir) const
{
    assume(false && "Not implemented!"); ///\todo
    return float3x4();
}

float3x4 Plane::ReflectionMatrix() const
{
    return float3x4::Reflect(*this);
}

float3 Plane::Reflect(const float3 &point) const
{
    assume(normal.IsNormalized());
    float3 reflected = point - 2.f * (Dot(point, normal) + d) * normal;
    assert(reflected.Equals(ReflectionMatrix().MulPos(point)));
    return reflected;
}

float3 Plane::Refract(const float3 &normal, float negativeSideRefractionIndex, float positiveSideRefractionIndex) const
{
    assume(false && "Not implemented!"); ///\todo
    return float3();
}

float3 Plane::Project(const float3 &point) const
{
    float3 projected = point - (Dot(normal, point) - d) * normal;
    assert(projected.Equals(OrthoProjection().MulPos(point)));
    return projected;
}

float3 Plane::ObliqueProject(const float3 &point, const float3 &obliqueProjectionDir) const
{
    assume(false && "Not implemented!"); ///\todo
    return float3();
}

bool Plane::Contains(const float3 &point, float distanceThreshold) const
{
    return Distance(point) <= distanceThreshold;
}

bool Plane::Intersects(const Plane &plane, Line *outLine) const
{
    float3 perp = Cross(normal, plane.normal);

    float3x3 m;
    m.SetRow(0, normal);
    m.SetRow(1, plane.normal);
    m.SetRow(2, perp); // This is arbitrarily chosen, to produce m invertible.
    bool success = m.Inverse();
    if (!success) // Inverse failed, so the planes must be parallel.
    {
        if (EqualAbs(d, plane.d)) // The planes are equal?
        {
            if (outLine)
                *outLine = Line(plane.PointOnPlane(), plane.normal.Perpendicular());
            return true;
        }
        else
            return false;
    }
    if (outLine)
        *outLine = Line(m * float3(d, plane.d, 0.f), perp.Normalized());
    return true;
}

bool Plane::Intersects(const Plane &plane, const Plane &plane2, Line *outLine, float3 *outPoint) const
{
    Line dummy;
    if (!outLine)
        outLine = &dummy;

    // First check all planes for parallel pairs.
    if (this->IsParallel(plane) || this->IsParallel(plane2))
    {
        if (EqualAbs(d, plane.d) || EqualAbs(d, plane2.d))
        {
            bool intersect = plane.Intersects(plane2, outLine);
            if (intersect && outPoint)
                *outPoint = outLine->GetPoint(0);
            return intersect;
        }
        else
            return false;
    }
    if (plane.IsParallel(plane2))
    {
        if (EqualAbs(plane.d, plane2.d))
        {
            bool intersect = this->Intersects(plane, outLine);
            if (intersect && outPoint)
                *outPoint = outLine->GetPoint(0);
            return intersect;
        }
        else
            return false;
    }

    // All planes point to different directions.
    float3x3 m;
    m.SetRow(0, normal);
    m.SetRow(1, plane.normal);
    m.SetRow(2, plane2.normal);
    bool success = m.Inverse();
    if (!success)
        return false;
    if (outPoint)
        *outPoint = m * float3(d, plane.d, plane2.d);
    return true;
}

/// Computes the intersection of a line and a plane.
/// @param ptOnPlane An arbitrary point on the plane.
/// @param planeNormal The plane normal direction vector, which must be normalized.
/// @param lineStart The starting point of the line.
/// @param lineDir The line direction vector. This vector does not need to be normalized.
/// @param t [out] If this function returns true, this parameter will receive the distance along the line where intersection occurs.
///                That is, the point lineStart + t * lineDir will be the intersection point.
/// @return If an intersection occurs, this function returns true.
bool IntersectLinePlane(const float3 &ptOnPlane, const float3 &planeNormal, const float3 &lineStart, const float3 &lineDir, float *t)
{
    float denom = Dot(lineDir, planeNormal);
    if (EqualAbs(denom, 0.f))
        return false; // Either we have no intersection, or the whole line is on the plane. \todo distinguish these cases.
    if (t)
        *t = Dot(ptOnPlane - lineStart, planeNormal);
    return true;
}

bool Plane::Intersects(const Ray &ray, float *d) const
{
    float t;
    bool success = IntersectLinePlane(PointOnPlane(), normal, ray.pos, ray.dir, &t);
    if (d)
        *d = t;
    return success && t >= 0.f;
}

bool Plane::Intersects(const Line &line, float *d) const
{
    return IntersectLinePlane(PointOnPlane(), normal, line.pos, line.dir, d);
}

bool Plane::Intersects(const LineSegment &lineSegment, float *d) const
{
    float t;
    bool success = IntersectLinePlane(PointOnPlane(), normal, lineSegment.a, lineSegment.Dir(), &t);
    const float lineSegmentLength = lineSegment.Length();
    if (d)
        *d = t / lineSegmentLength;
    return success && t >= 0.f && t <= lineSegmentLength;
}

bool Plane::Intersects(const Sphere &sphere) const
{
    return Distance(sphere.pos) <= sphere.r;
}

/// Set Christer Ericson's Real-Time Collision Detection, p.164.
bool Plane::Intersects(const AABB &aabb) const
{
    float3 c = aabb.CenterPoint();
    float3 e = aabb.HalfDiagonal();

    // Compute the projection interval radius of the AABB onto L(t) = aabb.center + t * plane.normal;
    float r = e[0]*Abs(normal[0]) + e[1]*Abs(normal[1]) + e[2]*Abs(normal[2]);
    // Compute the distance of the box center from plane.
    float s = Dot(normal, c) - d;
    return Abs(s) <= r;
}

bool Plane::Intersects(const OBB &obb) const
{
    return obb.Intersects(*this);
}

bool Plane::Intersects(const Triangle &triangle) const
{
    float a = SignedDistance(triangle.a);
    float b = SignedDistance(triangle.b);
    float c = SignedDistance(triangle.c);
    return (a*b <= 0.f || a*c <= 0.f);
}

bool Plane::Intersects(const Frustum &frustum) const
{
    bool sign = IsOnPositiveSide(frustum.CornerPoint(0));
    for(int i = 1; i < 8; ++i)
        if (sign != IsOnPositiveSide(frustum.CornerPoint(i)))
            return true;
    return false;
}
/*
bool Plane::Intersects(const Polyhedron &polyhedron) const
{
    assume(false && "Not implemented!"); ///\todo
    return false;
}
*/
bool Plane::Clip(float3 &a, float3 &b) const
{
    float t;
    bool intersects = IntersectLinePlane(PointOnPlane(), normal, a, b-a, &t);
    if (!intersects || t <= 0.f || t >= 1.f)
    {
        if (SignedDistance(a) <= 0.f)
            return false; // Discard the whole line segment, it's completely behind the plane.
        else
            return true; // The whole line segment is in the positive halfspace. Keep all of it.
    }
    float3 pt = a + (b-a) * t; // The intersection point.
    // We are either interested in the line segment [a, pt] or the segment [pt, b]. Which one is in the positive side?
    if (IsOnPositiveSide(a))
        b = pt;
    else
        a = pt;

    return true;
}

bool Plane::Clip(LineSegment &line) const
{
    return Clip(line.a, line.b);
}

int Plane::Clip(const Line &line, Ray &outRay) const
{
    float t;
    bool intersects = IntersectLinePlane(PointOnPlane(), normal, line.pos, line.dir, &t);
    if (!intersects)
    {
        if (SignedDistance(line.pos) <= 0.f)
            return 0; // Discard the whole line, it's completely behind the plane.
        else
            return 2; // The whole line is in the positive halfspace. Keep all of it.
    }

    outRay.pos = line.pos + line.dir * t; // The intersection point
    if (Dot(line.dir, normal) >= 0.f)
        outRay.dir = line.dir;
    else
        outRay.dir = -line.dir;

    return 1; // Clipping resulted in a ray being generated.
}

int Plane::Clip(const Triangle &triangle, Triangle &t1, Triangle &t2) const
{
    bool side[3];
    side[0] = IsOnPositiveSide(triangle.a);
    side[1] = IsOnPositiveSide(triangle.b);
    side[2] = IsOnPositiveSide(triangle.c);
    int nPos = (side[0] ? 1 : 0) + (side[1] ? 1 : 0) + (side[2] ? 1 : 0);
    if (nPos == 0) // Everything should be clipped?
        return 0;
    // We will output at least one triangle, so copy the input to t1 for processing.
    t1 = triangle;

    if (nPos == 3) // All vertices of the triangle are in positive side?
        return 1;

    if (nPos == 1)
    {
        if (side[1])
        {
            float3 tmp = t1.a;
            t1.a = t1.b;
            t1.b = t1.c;
            t1.c = tmp;
        }
        else if (side[2])
        {
            float3 tmp = t1.a;
            t1.a = t1.c;
            t1.c = t1.b;
            t1.b = tmp;
        }

        // After the above cycling, t1.a is the triangle on the positive side.
        float t;
        Intersects(LineSegment(t1.a, t1.b), &t);
        t1.b = t1.a + (t1.b-t1.a)*t;
        Intersects(LineSegment(t1.a, t1.c), &t);
        t1.c = t1.a + (t1.c-t1.a)*t;
        return 1;
    }
    // Must be nPos == 2.
    if (!side[1])
    {
        float3 tmp = t1.a;
        t1.a = t1.b;
        t1.b = t1.c;
        t1.c = tmp;
    }
    else if (!side[2])
    {
        float3 tmp = t1.a;
        t1.a = t1.c;
        t1.c = t1.b;
        t1.b = tmp;
    }
    // After the above cycling, t1.a is the triangle on the negative side.

    float t, r;
    Intersects(LineSegment(t1.a, t1.b), &t);
    float3 ab = t1.a + (t1.b-t1.a)*t;
    Intersects(LineSegment(t1.a, t1.c), &r);
    float3 ac = t1.a + (t1.c-t1.a)*t;
    t1.a = ab;

    t2.a = t1.c;
    t2.b = ac;
    t2.c = ab;

    return 2;
}

bool Plane::IsParallel(const Plane &plane, float epsilon) const
{
    return normal.Equals(plane.normal, epsilon);
}

bool Plane::PassesThroughOrigin(float epsilon) const
{
    return fabs(d) <= epsilon;
}

float Plane::DihedralAngle(const Plane &plane) const
{
    assume(false && "Not implemented!"); ///\todo
    return false;
}

bool Plane::Equals(const Plane &other, float epsilon) const
{
    return IsParallel(other, epsilon) && EqualAbs(d, other.d, epsilon);
}

Circle Plane::GenerateCircle(const float3 &circleCenter, float radius) const
{
    assume(false && "Not implemented!"); ///\todo
    return Circle();
}

Plane operator *(const float3x3 &transform, const Plane &plane)
{
    Plane p(plane);
    p.Transform(transform);
    return p;
}

Plane operator *(const float3x4 &transform, const Plane &plane)
{
    Plane p(plane);
    p.Transform(transform);
    return p;
}

Plane operator *(const float4x4 &transform, const Plane &plane)
{
    Plane p(plane);
    p.Transform(transform);
    return p;
}

Plane operator *(const Quat &transform, const Plane &plane)
{
    Plane p(plane);
    p.Transform(transform);
    return p;
}
