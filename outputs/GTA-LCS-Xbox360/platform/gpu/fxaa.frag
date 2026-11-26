#version 450
// Directional FXAA-style edge filter. One world-only pass, before HUD/menu.
layout(location=0) in vec4 v_diffuse;
layout(location=1) in vec4 v_specular;
layout(location=2) in vec4 v_uv;
layout(location=3) in float v_fog;
layout(location=0) out vec4 o_color;
layout(set=0,binding=0) uniform sampler2D scene;
layout(push_constant) uniform Params { vec4 c[32]; } p;
void main() {
 vec2 uv=v_uv.xy, px=p.c[0].xy;
 vec3 m=texture(scene,uv).rgb;
 vec3 nw=texture(scene,uv+vec2(-1,-1)*px).rgb;
 vec3 ne=texture(scene,uv+vec2(1,-1)*px).rgb;
 vec3 sw=texture(scene,uv+vec2(-1,1)*px).rgb;
 vec3 se=texture(scene,uv+vec2(1,1)*px).rgb;
 vec3 luma=vec3(0.299,0.587,0.114);
 float lm=dot(m,luma), lnw=dot(nw,luma),lne=dot(ne,luma),lsw=dot(sw,luma),lse=dot(se,luma);
 float lo=min(lm,min(min(lnw,lne),min(lsw,lse)));
 float hi=max(lm,max(max(lnw,lne),max(lsw,lse)));
 vec2 dir=vec2(-((lnw+lne)-(lsw+lse)),((lnw+lsw)-(lne+lse)));
 float reduce=max((lnw+lne+lsw+lse)*0.03125,0.0078125);
 float inv=1.0/(min(abs(dir.x),abs(dir.y))+reduce);
 dir=clamp(dir*inv,vec2(-8),vec2(8))*px;
 vec3 a=0.5*(texture(scene,uv+dir*(-0.166666667)).rgb+texture(scene,uv+dir*0.166666667).rgb);
 vec3 b=a*0.5+0.25*(texture(scene,uv+dir*(-0.5)).rgb+texture(scene,uv+dir*0.5).rgb);
 float lb=dot(b,luma);
 vec3 filtered=(lb<lo || lb>hi)?a:b;
 float edge=step(max(0.03125,hi*0.125),hi-lo);
 o_color=vec4(mix(m,filtered,edge)*v_fog,1)*v_diffuse+v_specular*p.c[31].x;
}
