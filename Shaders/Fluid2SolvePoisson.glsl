// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

uniform Parameters
{
    vec4 spaceDelta;    // (dx, dy, 0, 0)
    vec4 halfDivDelta;  // (0.5/dx, 0.5/dy, 0, 0)
    vec4 timeDelta;     // (dt/dx, dt/dy, 0, dt)
    vec4 viscosityX;    // (velVX, velVX, 0, denVX)
    vec4 viscosityY;    // (velVX, velVY, 0, denVY)
    vec4 epsilon;       // (epsilonX, epsilonY, 0, epsilon0)
};

layout(r32f) uniform readonly image2D divergence;
layout(r32f) uniform readonly image2D poisson;
layout(r32f) uniform writeonly image2D outPoisson;

layout (local_size_x = NUM_X_THREADS, local_size_y = NUM_Y_THREADS, local_size_z = 1) in;
void main()
{
    ivec2 c = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dim = imageSize(divergence);

    int x = int(c.x);
    int y = int(c.y);
    int xm = max(x - 1, 0);
    int xp = min(x + 1, dim.x - 1);
    int ym = max(y - 1, 0);
    int yp = min(y + 1, dim.y - 1);

    // Sample the divergence at (x,y).
    float div = imageLoad(divergence, c).x;

    // Sample Poisson values at (x+dx,y), (x-dx,y), (x,y+dy), (x,y-dy).
    float poisPZ = imageLoad(poisson, ivec2(xp, y)).x;
    float poisMZ = imageLoad(poisson, ivec2(xm, y)).x;
    float poisZP = imageLoad(poisson, ivec2(x, yp)).x;
    float poisZM = imageLoad(poisson, ivec2(x, ym)).x;

    vec4 temp = vec4(poisPZ + poisMZ, poisZP + poisZM, 0.0f, div);
    float outPoissonValue = dot(epsilon, temp);
    imageStore(outPoisson, c, vec4(outPoissonValue, 0.0f, 0.0f, 0.0f));
};
