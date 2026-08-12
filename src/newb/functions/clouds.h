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
    mix(rand(p0),rand(p0+vec2(1.0,0.0)), u.x),
    mix(rand(p0+vec2(0.0,1.0)),rand(p0+vec2(1.0,1.0)), u.x),
    u.y
  );
  n *= 0.58 + 0.42*sin(p.x*0.45 - 0.35*t)*sin(p.y*0.45 + 0.55*t);
  n = min(n*(1.0+rain), 1.0);
  return n*n;
}

// simple clouds
vec4 renderCloudsSimple(nl_skycolor skycol, vec3 pos, highp float t, float rain) {
  pos.xz *= NL_CLOUD1_SCALE;
  float d = cloudNoise2D(pos.xz, t, rain);
  vec4 col = vec4(skycol.horizonEdge + skycol.zenith, smoothstep(0.16,0.64,d));
  col.rgb += 1.18*dot(col.rgb, vec3(0.3,0.4,0.3))*smoothstep(0.6,0.2,d)*col.a;
  col.rgb *= 1.0 - 0.8*rain;
  return col;
}

// rounded clouds

// rounded clouds 3D density map
float cloudDf(vec3 pos, float rain, vec2 boxiness) {
  boxiness *= 0.999;
  vec2 p0 = floor(pos.xz);
  vec2 u = max((pos.xz-p0-boxiness.x)/(1.0-boxiness.x), 0.0);
  u *= u*(3.0 - 2.0*u);

  vec4 r = vec4(rand(p0), rand(p0+vec2(1.0,0.0)), rand(p0+vec2(1.0,1.0)), rand(p0+vec2(0.0,1.0)));
  r = smoothstep(0.1001+0.2*rain, 0.1+0.2*rain*rain, r); // rain transition

  float n = mix(mix(r.x,r.y,u.x), mix(r.w,r.z,u.x), u.y);

  // round y
  n *= 1.0 - 1.5*smoothstep(boxiness.y, 2.0 - boxiness.y, 2.0*abs(pos.y-0.5));

  n = max(1.18*(n-0.18), 0.0); // smoothstep(0.2, 1.0, n)
  n *= n*(3.0 - 2.0*n);
  return n;
}

vec4 renderCloudsRounded(
    vec3 vDir, vec3 vPos, float rain, float time, vec3 horizonCol, vec3 zenithCol,
    const int steps, const float thickness, const float thickness_rain, const float speed,
    const vec2 scale, const float density, const vec2 boxiness
) {
  float height = 6.3*mix(thickness, thickness_rain, rain);
  float stepsf = float(steps);

  // scaled ray offset
  vec3 deltaP;
  deltaP.y = 1.0;
  deltaP.xz = height*scale*vDir.xz/(0.02+0.98*abs(vDir.y));

  // local cloud pos
  vec3 pos;
  pos.y = 0.0;
  pos.xz = scale*(vPos.xz + vec2(1.0,0.5)*(time*speed));
  pos += deltaP;

  deltaP /= -stepsf;

  // alpha, gradient
  vec2 d = vec2(0.0,1.0);
  for (int i=1; i<=steps; i++) {
    float m = cloudDf(pos, rain, boxiness);
    d.x += m;
    d.y = mix(d.y, pos.y, m);
    pos += deltaP;
  }

  d.x *= smoothstep(0.03,0.1,d.x);
  d.x /= (stepsf/density) + d.x;

  if (vPos.y < 0.0) {
    d.y = 1.0-d.y;
  }

  // light blue rounded cloud colors
  vec3 cloudTop = vec3(0.72,0.92,1.08);
  vec3 cloudBottom = vec3(0.34,0.68,0.92);

  // smooth vertical color variation
  vec3 cloudColor = mix(cloudBottom,cloudTop,d.y);

  // soft atmospheric brightness
  cloudColor += 0.10*(zenithCol + horizonCol);

  vec4 col = vec4(cloudColor,d.x);

  // soft cloud highlights
  col.rgb += dot(col.rgb,vec3(0.30,0.42,0.38))*d.y*d.y;

  // retain rain darkening
  col.rgb *= 1.0-0.48*rain;

  return col;
}

float cloudsNoiseVr(vec2 p, float t) {
  float n = fastVoronoi2(p + t, 1.8);
  n *= fastVoronoi2(3.0*p + t, 1.5);
  n *= fastVoronoi2(9.0*p + t, 0.4);
  n *= fastVoronoi2(27.0*p + t, 0.1);
  //n *= fastVoronoi2(82.0*pos + t, 0.02); // more quality
  return n*n;
}

vec4 renderClouds(vec2 p, float t, float rain, vec3 horizonCol, vec3 zenithCol, const vec2 scale, const float velocity, const float shadow) {
  p *= scale;
  t *= velocity;

  // layer 1
  float a = cloudsNoiseVr(p, t);
  float b = cloudsNoiseVr(p + NL_CLOUD3_SHADOW_OFFSET*scale, t);

  // layer 2
  p = 1.4 * p.yx + vec2(7.8, 9.2);
  t *= 0.5;
  float c = cloudsNoiseVr(p, t);
  float d = cloudsNoiseVr(p + NL_CLOUD3_SHADOW_OFFSET*scale, t);

  // higher = less clouds thickness
  // lower separation betwen x & y = sharper
  vec2 tr = vec2(0.56, 0.68) - 0.12*rain;
  a = smoothstep(tr.x, tr.y, a);
  c = smoothstep(tr.x, tr.y, c);

  // shadow
  b *= smoothstep(0.2, 0.8, b);
  d *= smoothstep(0.2, 0.8, d);

  vec4 col;
  col.a = a + c*(1.0-a);
  col.rgb = horizonCol + horizonCol.ggg;
  col.rgb = mix(col.rgb, 0.5*(zenithCol + zenithCol.ggg), shadow*mix(b, d, c));
  col.rgb *= 1.0-0.55*rain;

  return col;
}

// aurora is rendered on clouds layer
#ifdef NL_AURORA
vec4 renderAurora(vec3 p, float t, float rain, vec3 FOG_COLOR) {
  t *= NL_AURORA_VELOCITY;

  // large-scale curtain movement
  float x = p.x*NL_AURORA_SCALE;
  float z = p.z*NL_AURORA_SCALE;
  float wave1 = sin(x*1.8 + t*2.0);
  float wave2 = sin(x*3.7 - t*1.2 + sin(z*0.7));
  float wave3 = sin(x*7.0 + t*0.8);

  // vertical curtain folds
  float curtain = 0.5 + 0.5*(0.62*wave1 + 0.28*wave2 + 0.10*wave3);
  curtain = smoothstep(0.22,0.78,curtain);

  // long vertical hanging strands
  float vertical = 0.5 + 0.5*sin(z*1.2 + wave1*2.5 + wave2*1.2 + t*0.35);
  vertical = smoothstep(0.18,0.82,vertical);

  // combine curtain folds
  float d = curtain*vertical;

  // soften the curtain edges
  d *= d;
  d = smoothstep(0.04,0.72,d);

  // fade toward the top and bottom
  float heightFade = 1.0-abs(p.y);
  heightFade = smoothstep(0.0,1.0,heightFade);
  heightFade *= heightFade;
  d *= heightFade;

  // multiple luminous curtain layers
  float layer1 = sin(x*2.4 + t + sin(z*0.6));
  float layer2 = sin(x*5.2 - t*0.7 + layer1);
  float layer3 = sin(x*9.0 + t*0.4);

  float detail = 0.5 + 0.5*(0.55*layer1 + 0.30*layer2 + 0.15*layer3);
  detail = smoothstep(0.25,0.80,detail);

  d *= 0.65 + 0.35*detail;

  // weather fade
  float mask = (1.0-0.8*rain)*max(1.0-4.5*max(FOG_COLOR.b,FOG_COLOR.g),0.0);

  // aurora color gradient
  vec3 auroraColor = mix(NL_AURORA_COL1,NL_AURORA_COL2,0.5+0.5*sin(x*1.8+t+detail*2.0));
  return vec4(auroraColor,d*NL_AURORA*mask);
}
#endif

vec4 nlCloudAuroraReflection(nl_skycolor skycol, nl_environment env, vec3 viewDir, vec3 wPos, vec3 CAMERA_POS, highp float t) {
  vec2 cloudPos = wPos.xz;
  cloudPos += (194.0-(wPos.y+CAMERA_POS.y))*viewDir.xz/viewDir.y;
  float fade = clamp(2.0 - 0.0040*length(cloudPos), 0.0, 1.0);
  cloudPos += CAMERA_POS.xz;

  vec4 refl = vec4_splat(0.0);

  #ifdef NL_AURORA
    vec4 aurora = renderAurora(cloudPos.xyy, t, env.rainFactor, env.fogCol);
    aurora.a *= fade;
    refl = vec4(1.45*aurora.rgb*aurora.a, aurora.a);
  #endif

  #if NL_CLOUD_TYPE == 1
    vec4 clouds = renderCloudsSimple(skycol, cloudPos.xyy, t, env.rainFactor);
    clouds.a *= fade;
    refl = vec4(mix(refl.rgb, clouds.rgb, clouds.a), min(refl.a + clouds.a*0.80, 1.0));
  #endif

  return refl;
}

#endif
