#ifndef CLOUDS_H
#define CLOUDS_H

#include "detection.h"
#include "noise.h"
#include "sky.h"

// simple clouds 2D noise
float cloudNoise2D(vec2 p, highp float t, float rain) {
  t *= NL_CLOUD1_SPEED;
  p += t;
  p.y += 2.1*sin(0.24*p.x + 0.08*t);

  vec2 p0 = floor(p);
  vec2 u = p-p0;
  u *= u*(3.0-2.0*u);
  vec2 v = 1.0-u;

  float n = mix(
    mix(rand(p0),rand(p0+vec2(1.0,0.0)),u.x),
    mix(rand(p0+vec2(0.0,1.0)),rand(p0+vec2(1.0,1.0)),u.x),
    u.y
  );
  n *= 0.58+0.42*sin(p.x*0.45-0.35*t)*sin(p.y*0.45+0.55*t);
  n = min(n*(1.0+rain),1.0);
  return n*n;
}

// simple clouds
vec4 renderCloudsSimple(nl_skycolor skycol, vec3 pos, highp float t, float rain) {
  pos.xz *= NL_CLOUD1_SCALE;
  float d = cloudNoise2D(pos.xz,t,rain);

  vec4 col = vec4(
    0.72*skycol.horizonEdge+0.28*skycol.zenith,
    smoothstep(0.16,0.64,d)
  );

  col.rgb += 0.55*dot(
    col.rgb,
    vec3(0.3,0.4,0.3)
  )*smoothstep(0.6,0.2,d)*col.a;

  col.rgb *= 1.0-0.68*rain;

  // night cloud darkening
  float cloudLight = dot(
    0.5*(skycol.horizon+skycol.zenith),
    vec3(0.299,0.587,0.114)
  );

  float nightMask = 1.0-smoothstep(
    0.10,
    0.34,
    cloudLight
  );

  col.rgb *= mix(
    1.0,
    0.22,
    nightMask
  );

  // subtle moonlight
  col.rgb += vec3(0.012,0.020,0.045)*nightMask*col.a;

  return col;
}

// rounded clouds

// rounded clouds 3D density map
float cloudDf(vec3 pos,float rain,vec2 boxiness) {
  boxiness *= 0.999;

  vec2 p0 = floor(pos.xz);

  vec2 u = max(
    (pos.xz-p0-boxiness.x)/(1.0-boxiness.x),
    0.0
  );

  u *= u*(3.0-2.0*u);

  vec4 r = vec4(
    rand(p0),
    rand(p0+vec2(1.0,0.0)),
    rand(p0+vec2(1.0,1.0)),
    rand(p0+vec2(0.0,1.0))
  );

  // LMI rain transition
  r = smoothstep(
    0.1001+0.2*rain,
    0.1+0.2*rain*rain,
    r
  );

  float n = mix(
    mix(r.x,r.y,u.x),
    mix(r.w,r.z,u.x),
    u.y
  );

  // rounded vertical profile
  n *= 1.0-1.5*smoothstep(
    boxiness.y,
    2.0-boxiness.y,
    2.0*abs(pos.y-0.5)
  );

  // LMI cloud shaping
  n = max(
    1.25*(n-0.2),
    0.0
  );

  n *= n*(3.0-2.0*n);

  return n;
}

vec4 renderCloudsRounded(
    vec3 vDir,
    vec3 vPos,
    float rain,
    float time,
    vec3 horizonCol,
    vec3 zenithCol,
    const int steps,
    const float thickness,
    const float thickness_rain,
    const float speed,
    const vec2 scale,
    const float density,
    const vec2 boxiness
) {
  // LMI rounded cloud height
  float height = 9.0*mix(
    thickness,
    thickness_rain,
    rain
  );

  float stepsf = float(steps);

  // scaled ray offset
  vec3 deltaP;

  deltaP.y = 1.0;

  deltaP.xz =
    height*scale*vDir.xz/
    (0.02+0.98*abs(vDir.y));

  // local cloud position
  vec3 pos;

  pos.y = 0.0;

  pos.xz = scale*(
    vPos.xz+
    vec2(1.0,0.5)*
    (time*speed)
  );

  pos += deltaP;

  deltaP /= -stepsf;

  // LMI-style cloud displacement
  pos += deltaP*rand(
  vPos.xz+time
);
  // alpha and vertical gradient
  vec2 d = vec2(
    0.0,
    0.5
  );

  for (int i=1; i<=steps; i++) {
    float m = cloudDf(
      pos,
      rain,
      boxiness
    );

    d.x += m;

    d.y = mix(
      d.y,
      pos.y,
      m
    );

    pos += deltaP;
  }

  // LMI alpha shaping
  d.x *= smoothstep(
    0.7,
    1.0,
    d.x
  );

  d.x /= (
    stepsf/density
  )+d.x;

  if (vPos.y<0.0) {
    d.y = 1.0-d.y;
  }

  // Aetheris soft cloud color
  vec3 cloudBase = mix(
    horizonCol,
    zenithCol,
    0.28
  );

  vec4 col = vec4(
    cloudBase,
    d.x
  );

  // cloud lighting
  col.rgb += dot(
    col.rgb,
    vec3(0.3,0.4,0.3)
  )*d.y*d.y;

  col.rgb *= 1.0-0.68*rain;

  // ==========================================
  // NIGHT CLOUD DARKENING
  // ==========================================

  float cloudLight = dot(
    0.5*(
      horizonCol+
      zenithCol
    ),
    vec3(
      0.299,
      0.587,
      0.114
    )
  );

  float nightMask = 1.0-smoothstep(
    0.10,
    0.34,
    cloudLight
  );

  // darken clouds at night
  col.rgb *= mix(
    1.0,
    0.20,
    nightMask
  );

  // subtle moonlight
  float moonLight = smoothstep(
    0.15,
    0.85,
    d.y
  );

  col.rgb += vec3(
    0.012,
    0.020,
    0.050
  )*
  nightMask*
  moonLight*
  col.a;

  return col;
}

float cloudsNoiseVr(vec2 p, float t) {
  float n = fastVoronoi2(p+t,1.8);
  n *= fastVoronoi2(3.0*p+t,1.5);
  n *= fastVoronoi2(9.0*p+t,0.4);
  n *= fastVoronoi2(27.0*p+t,0.1);
  //n *= fastVoronoi2(82.0*pos+t,0.02); // more quality
  return n*n;
}

vec4 renderClouds(
    vec2 p,
    float t,
    float rain,
    vec3 horizonCol,
    vec3 zenithCol,
    const vec2 scale,
    const float velocity,
    const float shadow
) {
  p *= scale;
  t *= velocity;

  // layer 1
  float a = cloudsNoiseVr(p,t);
  float b = cloudsNoiseVr(
    p+NL_CLOUD3_SHADOW_OFFSET*scale,
    t
  );

  // layer 2
  p = 1.4*p.yx+vec2(7.8,9.2);
  t *= 0.5;

  float c = cloudsNoiseVr(p,t);
  float d = cloudsNoiseVr(
    p+NL_CLOUD3_SHADOW_OFFSET*scale,
    t
  );

  // cloud thickness
  vec2 tr = vec2(0.56,0.68)-0.12*rain;

  a = smoothstep(
    tr.x,
    tr.y,
    a
  );

  c = smoothstep(
    tr.x,
    tr.y,
    c
  );

  // shadow
  b *= smoothstep(
    0.2,
    0.8,
    b
  );

  d *= smoothstep(
    0.2,
    0.8,
    d
  );

  vec4 col;

  col.a = a+c*(1.0-a);

  // softer cloud color
  col.rgb = 0.72*horizonCol+0.28*zenithCol;

  // cloud shadow
  col.rgb = mix(
    col.rgb,
    0.58*zenithCol,
    shadow*mix(b,d,c)
  );

  col.rgb *= 1.0-0.55*rain;

  // ==========================================
  // NIGHT CLOUD DARKENING
  // ==========================================

  float cloudLight = dot(
    0.5*(horizonCol+zenithCol),
    vec3(0.299,0.587,0.114)
  );

  float nightMask = 1.0-smoothstep(
    0.10,
    0.34,
    cloudLight
  );

  // dark navy cloud at night
  col.rgb *= mix(
    1.0,
    0.18,
    nightMask
  );

  // subtle blue moonlight
  col.rgb += vec3(
    0.012,
    0.020,
    0.050
  )*nightMask*col.a;

  return col;
}

// aurora is rendered on clouds layer
#ifdef NL_AURORA

vec4 renderAurora(
    vec3 p,
    float t,
    float rain,
    vec3 FOG_COLOR
) {
  t *= NL_AURORA_VELOCITY;
  p.xz *= NL_AURORA_SCALE;
  p.xz += 0.03*sin(
    p.x*4.0+20.0*t
  );

  float d0 = sin(
    p.x*0.1+t+sin(p.z*0.2)
  );

  float d1 = sin(
    p.z*0.1-t+sin(p.x*0.2)
  );

  float d2 = sin(
    p.z*0.1+
    1.0*sin(d0+d1*2.0)+
    d1*2.0+
    d0*1.0
  );

  d0 *= d0;
  d1 *= d1;
  d2 *= d2;

  d2 = d0/(1.0+d2/NL_AURORA_WIDTH);

  float mask =
    (1.0-0.8*rain)*
    max(
      1.0-4.5*max(
        FOG_COLOR.b,
        FOG_COLOR.g
      ),
      0.0
    );

  return vec4(
    NL_AURORA*
    mix(
      NL_AURORA_COL1,
      NL_AURORA_COL2,
      d1
    ),
    1.0
  )*d2*mask;
}

#endif

vec4 nlCloudAuroraReflection(
    nl_skycolor skycol,
    nl_environment env,
    vec3 viewDir,
    vec3 wPos,
    vec3 CAMERA_POS,
    highp float t
) {
  vec2 cloudPos = wPos.xz;

  cloudPos += (
    194.0-
    (wPos.y+CAMERA_POS.y)
  )*viewDir.xz/viewDir.y;

  float fade = clamp(
    2.0-0.0040*length(cloudPos),
    0.0,
    1.0
  );

  cloudPos += CAMERA_POS.xz;

  vec4 refl = vec4_splat(0.0);

  #ifdef NL_AURORA

    vec4 aurora = renderAurora(
      cloudPos.xyy,
      t,
      env.rainFactor,
      env.fogCol
    );

    aurora.a *= fade;

    refl = vec4(
      1.45*aurora.rgb*aurora.a,
      aurora.a
    );

  #endif

  #if NL_CLOUD_TYPE == 1

    vec4 clouds = renderCloudsSimple(
      skycol,
      cloudPos.xyy,
      t,
      env.rainFactor
    );

    clouds.a *= fade;

    refl = vec4(
      mix(
        refl.rgb,
        clouds.rgb,
        clouds.a
      ),
      min(
        refl.a+clouds.a*0.80,
        1.0
      )
    );

  #endif

  return refl;
}

#endif
