#ifndef FOG_H
#define FOG_H

float nlRenderFogFade(float relativeDist, vec3 FOG_COLOR, vec2 FOG_CONTROL) {
  #ifdef NL_FOG
    float fade = smoothstep(FOG_CONTROL.x,FOG_CONTROL.y,relativeDist);

    // volumetric mist
    float density = NL_MIST_DENSITY*(20.0-18.0*FOG_COLOR.g);

    float mist = 1.0-exp(
      -relativeDist*relativeDist*density
    );

    // thicker atmospheric fog
    mist = smoothstep(0.0,0.82,mist);

    // soft layered fog
    float layer = 0.5+0.5*sin(
      relativeDist*2.2+
      FOG_COLOR.r*4.0+
      FOG_COLOR.b*2.0
    );

    layer = mix(0.78,1.18,layer);

    fade += (1.0-fade)*mist*0.20*layer;

    // stronger distant atmosphere
    float distanceFog = smoothstep(
      FOG_CONTROL.x*0.55,
      FOG_CONTROL.y,
      relativeDist
    );

    fade = max(fade,distanceFog*0.12);

    return NL_FOG*clamp(fade,0.0,1.0);
  #else
    return 0.0;
  #endif
}

float nlRenderGodRayIntensity(vec3 cPos, vec3 worldPos, float t, vec2 uv1, float relativeDist, vec3 FOG_COLOR) {
  vec3 offset = cPos-16.0*fract(worldPos*0.0625);
  offset = abs(2.0*fract(offset*0.0625)-1.0);
  offset = offset*offset*(3.0-2.0*offset);

  vec3 nrmof = normalize(worldPos);

  float u = nrmof.z/length(nrmof.zy);

  float diff = dot(offset,vec3(0.06,0.14,0.85))+0.035*t;

  float mask = nrmof.x*nrmof.x;

  // broad soft light pattern
  float ray1 = 0.5+0.5*sin(2.4*u+0.75*diff);
  float ray2 = 0.5+0.5*sin(1.15*u-0.45*diff);

  // combine wide patterns
  float vol = mix(0.72,1.0,ray1);
  vol *= mix(0.82,1.0,ray2);

  // very soft variation
  float fantasyGlow = 0.5+0.5*sin(
    1.8*u+0.35*diff
  );

  fantasyGlow = mix(0.88,1.12,fantasyGlow);

  vol *= fantasyGlow;

  // broad volumetric shape
  vol = smoothstep(0.55,0.90,vol);

  // atmospheric mask
  vol *= mask;
  vol *= uv1.y;

  // stronger toward distance, but never becoming a hard beam
  float distanceFade = smoothstep(0.0,1.0,relativeDist);
  vol *= mix(0.70,1.0,distanceFade);

  // dawn / sunset color mask
  float sunsetMask = clamp(
    3.0*(FOG_COLOR.r-FOG_COLOR.b),
    0.0,
    1.0
  );

  vol *= sunsetMask;

  // soft final transition
  vol = smoothstep(0.0,0.32,vol);

  return vol*1.15;
}

#endif
