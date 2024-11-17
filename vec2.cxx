#include <stdbool.h>
#include <math.h>

#include "vec2.h"

// Vec2 Vec2_unit(const Vec2& v) {
//     Vec2 u(v.x, v.y);
//     u.normalize();
//     return u;
// }

// // Returns true if point b lies between points a and c
// bool Vec2_colinear(const Vec2& a, const Vec2& b, const Vec2& c) {
//     float ac = a.dist(c);
//     float ab = a.dist(b);
//     float bc = b.dist(c);
//     return fabsf(ac - ab - bc) <= VEC2_EPSILON;
// }

// Returns the closest point to the line segment ab and p
Vec2 Vec2_closest(const Vec2& a, const Vec2& b, const Vec2& p) {
    // vector along line ab
    Vec2 ab = b - a;
    float t = ab.dot(ab);
    if(t == 0.0f) {
        return a;
    }
    t = fmax(0.0f, fmin(1.0f, (p.dot(ab) - a.dot(ab)) / t));
    return a + ab * t;
}

// // // Ehhh - this should work??
// // // Returns the closest point to the line segment ab and p
// // Vec2 Vec2_closest(const Vec2& a, const Vec2& b, const Vec2& p) {
// //     // vector along line ab
// //     Vec2 ab = b - a;
// //     // vector aline line ap
// //     Vec2 ap = p - a;
// //     // projection of p onto ab
// //     float proj = ap.dot(ab);
// //     // line segment ab length, squared
// //     float ab_l2 = a.dist2(b);
// //     // ratio of the projection along line. segment is [0,1] - beyond isn't on line
// //     float d = proj / ab_l2;
// //     if(d <= 0) {
// //         return a;
// //     } else if(d >= 1) {
// //         return b;
// //     }
// //     return a + ab * d;
// // }

// // Returns the closest point to the infinite ray ab and p
// Vec2 Vec2_closest_ray(const Vec2& a, const Vec2& b, const Vec2& p) {
//     // vector along line ab
//     Vec2 ab = b - a;
//     // vector along line ap
//     Vec2 ap = p - a;
//     // projection of p onto ab
//     float proj = ap.dot(ab);
//     // line segment ab length, squared
//     float ab_l2 = a.dist2(b);
//     // ratio of the projection along line. segment is [0,1] - beyond isn't on line
//     float d = proj / ab_l2;
//     return a + ab * d;
// }

// // Returns if A, B, C are listed in counterclockwise order
// // Also tells us if C is "to the left of" line AB
// bool Vec2_ccw(const Vec2& A, const Vec2& B, const Vec2& C) {
//     return (C.y - A.y) * (B.x - A.x) > (B.y - A.y) * (C.x - A.x);
// }

// // Returns true if line AB intersects with line CD
// bool Vec2_intersect(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& D) {
//     return Vec2_ccw(A, C, D) != Vec2_ccw(B, C, D) && Vec2_ccw(A, B, C) != Vec2_ccw(A, B, D);
// }

// // Returns intersection point of two lines AB and CD.
// // The intersection point may not lie on either segment.
// Vec2 Vec2_intersection(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& D) {
//     float a = (D.x - C.x) * (C.y - A.y) - (D.y - C.y) * (C.x - A.x);
//     float b = (D.x - C.x) * (B.y - A.y) - (D.y - C.y) * (B.x - A.x);
//     // int c = (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
//     if(b == 0) {
//         // lines are parallel
//         return (Vec2){NAN, NAN};
//     }
//     float alpha = a / b;
//     // float beta = c / b;
//     return (Vec2){A.x + alpha * (B.x - A.x), A.y + alpha * (B.y - A.y)};
// }

// // Returns distance of ray origin to intersection point on line segment. -1 if no intersection
// float Vec2_ray_line_segment_intersect(
//     const Vec2& origin,
//     const Vec2& dir,
//     const Vec2& l1,
//     const Vec2& l2) {
//     Vec2 v1 = origin - l1;
//     Vec2 v2 = l2 - l1;
//     Vec2 v3 = Vec2(-dir.y, dir.x);

//     float dot = v2.dot(v3);
//     if(fabsf(dot) < (float)0.00001) {
//         return -1.0f;
//     }

//     float t1 = v2.cross(v1) / dot;
//     float t2 = v1.dot(v3) / dot;
//     if(t1 >= 0 && (t2 >= 0 && t2 <= 1)) {
//         return t1;
//     }
//     return -1.0f;
// }

// // Projects P onto AB and returns true if P lies on line segment AB
// // [dst] is the location of P projected onto AB
// bool Vec2_project(const Vec2& A, const Vec2& B, const Vec2& P, Vec2& dst) {
//     Vec2 AB(B - A);
//     Vec2 AP(P - A);
//     float k = AP.dot(AB) / AB.dot(AB);
//     if(k < 0 || k > 1) return false;
//     dst = A + AB * k;
//     return true;
// }

// bool Vec2_point_circle_collide(const Vec2& point, const Vec2& circle, float radius) {
//     Vec2 d = {circle.x - point.x, circle.y - point.y};
//     return d.mag() <= radius * radius;
// }

// bool Vec2_line_circle_collide(
//     const Vec2& a,
//     const Vec2& b,
//     const Vec2& circle,
//     float radius,
//     Vec2* nearest) {
//     // check if start or end points lie within circle
//     if(Vec2_point_circle_collide(a, circle, radius)) {
//         if(nearest) {
//             *nearest = a;
//         }
//         return true;
//     }
//     if(Vec2_point_circle_collide(b, circle, radius)) {
//         if(nearest) {
//             *nearest = b;
//         }
//         return true;
//     }

//     float x1 = a.x;
//     float y1 = a.y;
//     float x2 = b.x;
//     float y2 = b.y;
//     float cx = circle.x;
//     float cy = circle.y;

//     float dx = x2 - x1;
//     float dy = y2 - y1;

//     float lcx = cx - x1;
//     float lcy = cy - y1;

//     // project lc onto d, resulting in vector p
//     float d_len2 = dx * dx + dy * dy;
//     float px = dx;
//     float py = dy;
//     if(d_len2 > 0) {
//         float dp = (lcx * dx + lcy * dy) / d_len2;
//         px *= dp;
//         py *= dp;
//     }

//     Vec2 nearest_tmp(x1 + px, y1 + py);
//     float p_len2 = px * px + py * py;

//     if(nearest) {
//         *nearest = nearest_tmp;
//     }

//     return Vec2_point_circle_collide(nearest_tmp, circle, radius) && p_len2 <= d_len2 &&
//            (px * dx + py * dy) >= 0;
// }
