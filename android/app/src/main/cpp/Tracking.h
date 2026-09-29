#pragma once
// Native OpenXR joint tracking and lightweight procedural hand/controller models.
// Uses one stereo projection layer rather than a compositor layer per joint.
struct TrackedVisuals {
 PFN_xrCreateHandTrackerEXT create=nullptr;PFN_xrDestroyHandTrackerEXT destroy=nullptr;PFN_xrLocateHandJointsEXT locate=nullptr;
 XrHandTrackerEXT tracker[2]{};XrHandJointLocationEXT joints[2][XR_HAND_JOINT_COUNT_EXT]{};
 XrHandTrackingAimStateFB aim[2]{{XR_TYPE_HAND_TRACKING_AIM_STATE_FB},{XR_TYPE_HAND_TRACKING_AIM_STATE_FB}};
 bool active[2]{},hasAim=false,known[2]{};XrPosef last[2]{},motion[2]{};int64_t moved[2]{};
 int64_t formation=0;bool snapped=false;XrVector3f formationCenter{};
 XrSwapchain chain=XR_NULL_HANDLE;std::vector<XrSwapchainImageOpenGLESKHR> images;
 GLuint program=0,vbo=0,vao=0,fbo=0,depthBuffer=0;
 struct Buttons{float trigger=0,squeeze=0,stickX=0,stickY=0;bool first=false,second=false,stick=false,menu=false;};Buttons buttons[2];
 std::shared_ptr<SkinModel> models[4];
 struct Batch{size_t first,count;GLuint texture;};std::vector<Batch> batches;
 struct Vertex{float x,y,z,r,g,b,a,u=0,v=0;};std::vector<Vertex> vertices;
 void init(XrInstance instance,XrSession session,bool hands,bool aimAvailable,int64_t format){
  hasAim=aimAvailable;
  if(hands){xrGetInstanceProcAddr(instance,"xrCreateHandTrackerEXT",(PFN_xrVoidFunction*)&create);xrGetInstanceProcAddr(instance,"xrDestroyHandTrackerEXT",(PFN_xrVoidFunction*)&destroy);xrGetInstanceProcAddr(instance,"xrLocateHandJointsEXT",(PFN_xrVoidFunction*)&locate);
   for(int h=0;h<2;h++){XrHandTrackerCreateInfoEXT info{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};info.hand=h?XR_HAND_RIGHT_EXT:XR_HAND_LEFT_EXT;info.handJointSet=XR_HAND_JOINT_SET_DEFAULT_EXT;if(XR_FAILED(create(session,&info,&tracker[h])))tracker[h]=XR_NULL_HANDLE;}}
  const char* vs=R"(#version 300 es
layout(location=0) in vec3 p;layout(location=1) in vec4 c;layout(location=2) in vec2 tc;out vec2 uv;uniform vec4 rotation;uniform vec3 eye;uniform vec4 tangents;out vec4 color;
void main(){vec3 v=p-eye;vec3 t=2.0*cross(rotation.xyz,v);v=v+rotation.w*t+cross(rotation.xyz,t);float l=tangents.x,r=tangents.y,b=tangents.z,u=tangents.w;gl_Position=vec4(2.0*v.x/(r-l)+(r+l)*v.z/(r-l),2.0*v.y/(u-b)+(u+b)*v.z/(u-b),-1.002*v.z-0.02002,-v.z);color=c;uv=tc;})";
  const char* fs=R"(#version 300 es
precision mediump float;in vec4 color;in vec2 uv;uniform sampler2D tex;uniform bool textured;out vec4 o;void main(){vec4 c=color;if(textured)c*=texture(tex,uv);if(c.a<.01)discard;float shade=clamp(dot(c.rgb,vec3(.2126,.7152,.0722)),0.0,1.0);vec3 amber=mix(vec3(.32,.085,.008),vec3(1.0,.55,.12),sqrt(shade));float alpha=c.a*.62;o=vec4(amber*alpha,alpha);})";
  GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);
  glGenBuffers(1,&vbo);glGenVertexArrays(1,&vao);glGenFramebuffers(1,&fbo);glGenRenderbuffers(1,&depthBuffer);glBindRenderbuffer(GL_RENDERBUFFER,depthBuffer);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,2048,1024);
  XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;ci.format=format;ci.sampleCount=1;ci.width=2048;ci.height=1024;ci.faceCount=ci.arraySize=ci.mipCount=1;check(xrCreateSwapchain(session,&ci,&chain),"Hand visual swapchain");uint32_t n;xrEnumerateSwapchainImages(chain,0,&n,nullptr);images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});xrEnumerateSwapchainImages(chain,n,&n,(XrSwapchainImageBaseHeader*)images.data());
 }
 static float distance(XrVector3f a,XrVector3f b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return sqrtf(x*x+y*y+z*z);}
 void bone(XrVector3f a,XrVector3f b,float radius,float alpha,float red=.95f,float green=.55f,float blue=.13f){
  XrVector3f d{b.x-a.x,b.y-a.y,b.z-a.z};float len=distance(a,b);if(len<.0001f)return;d={d.x/len,d.y/len,d.z/len};
  XrVector3f u=fabsf(d.y)<.9f?XrVector3f{-d.z,0,d.x}:XrVector3f{0,d.z,-d.y};float ul=sqrtf(u.x*u.x+u.y*u.y+u.z*u.z);u={u.x/ul,u.y/ul,u.z/ul};XrVector3f v{d.y*u.z-d.z*u.y,d.z*u.x-d.x*u.z,d.x*u.y-d.y*u.x};
  auto point=[&](XrVector3f p,float angle){return XrVector3f{p.x+radius*(u.x*cosf(angle)+v.x*sinf(angle)),p.y+radius*(u.y*cosf(angle)+v.y*sinf(angle)),p.z+radius*(u.z*cosf(angle)+v.z*sinf(angle))};};
  auto add=[&](XrVector3f p,float shade){vertices.push_back({p.x,p.y,p.z,red*shade,green*shade,blue*shade,alpha});};
  for(int i=0;i<8;i++){float angle=i*6.283185f/8,next=(i+1)*6.283185f/8;auto p=point(a,angle),q=point(a,next),r=point(b,angle),s=point(b,next);float shade=.65f+.35f*fabsf(cosf(angle));add(p,shade);add(q,shade);add(r,shade);add(r,shade);add(q,shade);add(s,shade);add(a,shade);add(q,shade);add(p,shade);add(b,shade);add(r,shade);add(s,shade);}
 }
 bool valid(int h,int j){return (joints[h][j].locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)!=0;}
 void update(XrSpace space,XrTime time,const XrSpaceLocation controllers[2]){
  vertices.clear();batches.clear();
  for(int i=0;i<4;i++){std::shared_ptr<SkinModel> next;{std::lock_guard<std::mutex> guard(skinLock);next=skins[i];}if(next!=models[i]){if(models[i])for(auto& t:models[i]->textures)if(t.gpu){glDeleteTextures(1,&t.gpu);t.gpu=0;}models[i]=next;}}
  for(int h=0;h<2;h++){
   active[h]=false;aim[h]={XR_TYPE_HAND_TRACKING_AIM_STATE_FB};
   if(tracker[h]){XrHandJointsLocateInfoEXT info{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};info.baseSpace=space;info.time=time;XrHandJointLocationsEXT out{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};out.jointCount=XR_HAND_JOINT_COUNT_EXT;out.jointLocations=joints[h];if(hasAim)out.next=&aim[h];active[h]=XR_SUCCEEDED(locate(tracker[h],&info,&out))&&out.isActive;}
   if(active[h]&&valid(h,XR_HAND_JOINT_WRIST_EXT)){
    if(models[h]){custom(h,h,joints[h][XR_HAND_JOINT_WRIST_EXT].pose,1);continue;}
    const int parents[26]={1,1,1,2,3,4,1,6,7,8,9,1,11,12,13,14,1,16,17,18,19,1,21,22,23,24};
    for(int j=2;j<26;j++)if(valid(h,j)&&valid(h,parents[j]))bone(joints[h][j].pose.position,joints[h][parents[j]].pose.position,std::clamp(joints[h][j].radius,.004f,.013f),.85f);
   }else{
    auto loc=controllers[h];bool tracked=(loc.locationFlags&(XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))==(XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
    if(tracked){auto a=loc.pose.orientation,b=motion[h].orientation;float dot=fabsf(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w);if(!known[h]||distance(loc.pose.position,motion[h].position)>.004f||dot<.99985f||buttons[h].trigger>.05f||buttons[h].squeeze>.05f||buttons[h].first||buttons[h].second||buttons[h].stick||buttons[h].menu){moved[h]=milliseconds();motion[h]=loc.pose;}known[h]=true;last[h]=loc.pose;}

    if(known[h]){
     float opacity=tracked?1.f-.9f*std::clamp((milliseconds()-moved[h]-10000)/1000.f,0.f,1.f):.1f;
     XrPosef visual=last[h];visual.orientation=multiply(visual.orientation,{-0.70710678f,0,0,0.70710678f});
     auto world=[&](XrVector3f p){auto v=rotate(visual.orientation,p);return XrVector3f{v.x+last[h].position.x,v.y+last[h].position.y,v.z+last[h].position.z};};
     if(models[h+2])custom(h+2,h,visual,opacity);
     else{
      bone(world({0,-.070f,.018f}),world({0,.003f,-.020f}),.025f,opacity,.16f,.18f,.22f);
      bone(world({-.018f,.014f,-.025f}),world({.018f,.014f,-.025f}),.026f,opacity,.23f,.25f,.29f);
      for(int i=0;i<24;i++){float a=i*6.283185f/24,b=(i+1)*6.283185f/24;bone(world({.051f*cosf(a),.028f,.051f*sinf(a)-.026f}),world({.051f*cosf(b),.028f,.051f*sinf(b)-.026f}),.004f,opacity,.3f,.32f,.36f);}
     }
     bool hasControls=false;if(models[h+2])for(auto& mesh:models[h+2]->meshes)hasControls|=mesh.control>=0;
     if(hasControls)continue;
     // Fallback indicators for unnamed custom models.
     auto button=[&](float x,float z,float radius,float press){
      float y=.043f-.003f*press;
      bone(world({x,y,z}),world({x,y+.003f,z}),radius,opacity,press>.01f?1.f:.12f,press>.01f?.55f:.14f,press>.01f?.04f:.18f);
     };
     float side=h?1.f:-1.f;
     button(side*.014f,-.014f,.007f,buttons[h].first);
     button(side*.024f,-.032f,.007f,buttons[h].second);
     button(-side*.015f+buttons[h].stickX*.006f,-.030f-buttons[h].stickY*.006f,.009f,buttons[h].stick);
     if(!h)button(.002f,-.004f,.0045f,buttons[h].menu);
     bone(world({-.012f,.018f,-.05f+buttons[h].trigger*.004f}),world({.012f,.018f,-.05f+buttons[h].trigger*.004f}),.008f,opacity,.18f+.8f*buttons[h].trigger,.18f+.37f*buttons[h].trigger,.2f*(1-buttons[h].trigger));
     bone(world({side*.022f,-.041f,0}),world({side*.022f,-.016f,-.006f}),.006f,opacity,.18f+.8f*buttons[h].squeeze,.18f+.37f*buttons[h].squeeze,.2f*(1-buttons[h].squeeze));
    }

   }
  }
 }
 bool pinch(int h){return active[h]&&((hasAim&&(aim[h].status&XR_HAND_TRACKING_AIM_INDEX_PINCHING_BIT_FB))||(!hasAim&&valid(h,5)&&valid(h,10)&&distance(joints[h][5].pose.position,joints[h][10].pose.position)<.022f));}

 bool framePoints(float* points){
  const int jointIds[4]={XR_HAND_JOINT_INDEX_PROXIMAL_EXT,XR_HAND_JOINT_INDEX_TIP_EXT,XR_HAND_JOINT_THUMB_METACARPAL_EXT,XR_HAND_JOINT_THUMB_TIP_EXT};
  for(int h=0;h<2;h++)for(int j=0;j<4;j++){
   if(!active[h]||!valid(h,jointIds[j])||!(joints[h][jointIds[j]].locationFlags&XR_SPACE_LOCATION_POSITION_TRACKED_BIT))return false;
   auto p=joints[h][jointIds[j]].pose.position;int k=h*12+j*3;points[k]=p.x;points[k+1]=p.y;points[k+2]=p.z;
  }return true;
 }
 void custom(int slot,int hand,XrPosef pose,float alpha){
  auto model=models[slot];float scale=skinScale[slot],yaw=skinYaw[slot];
  XrQuaternionf adjustment{0,sinf(yaw/2),0,cosf(yaw/2)};
  auto world=[&](XrVector3f p){p=rotate(adjustment,{p.x*scale,p.y*scale,p.z*scale});auto v=rotate(pose.orientation,p);return XrVector3f{pose.position.x+v.x,pose.position.y+v.y,pose.position.z+v.z};};
  float states[6]={buttons[hand].trigger,buttons[hand].squeeze,float(buttons[hand].first),float(buttons[hand].second),float(buttons[hand].stick),float(buttons[hand].menu)};
  for(auto& mesh:model->meshes){
   size_t start=vertices.size();float press=mesh.control>=0?states[mesh.control]:0;
   for(auto& v:mesh.vertices){
    XrVector3f p=world({v.position.x,v.position.y-.003f*press,v.position.z});
    if(slot<2&&!v.weights.empty()){
     XrVector3f blended{};float total=0;
     for(auto& w:v.weights)if(valid(hand,w.joint)&&w.weight>0){
      auto jp=joints[hand][w.joint].pose;auto d=rotate(jp.orientation,{w.local.x*scale,w.local.y*scale,w.local.z*scale});
      blended.x+=(jp.position.x+d.x)*w.weight;blended.y+=(jp.position.y+d.y)*w.weight;blended.z+=(jp.position.z+d.z)*w.weight;total+=w.weight;
     }if(total>.001f)p={blended.x/total,blended.y/total,blended.z/total};
    }
    vertices.push_back({p.x,p.y,p.z,mesh.r*v.r*(1-press)+press,mesh.g*v.g*(1-press)+.55f*press,mesh.b*v.b*(1-press)+.03f*press,mesh.a*v.a*alpha,v.u,v.v});
   }
   GLuint texture=0;
   if(mesh.texture>=0){auto& t=model->textures[mesh.texture];if(!t.gpu){glGenTextures(1,&t.gpu);glBindTexture(GL_TEXTURE_2D,t.gpu);glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,t.width,t.height,0,GL_RGBA,GL_UNSIGNED_BYTE,t.rgba.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);}texture=t.gpu;}
   batches.push_back({start,vertices.size()-start,texture});
  }
 }
 void draw(const std::array<XrView,2>& views,std::array<XrCompositionLayerProjectionView,2>& output){
  uint32_t index;XrSwapchainImageAcquireInfo a{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};check(xrAcquireSwapchainImage(chain,&a,&index),"Acquire hand visuals");XrSwapchainImageWaitInfo w{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};w.timeout=XR_INFINITE_DURATION;check(xrWaitSwapchainImage(chain,&w),"Wait hand visuals");
  glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,images[index].image,0);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depthBuffer);glDepthMask(GL_TRUE);glViewport(0,0,2048,1024);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(3*sizeof(float)));
  // Opaque geometry per pixel; premultiplied alpha is blended by the compositor.
  glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(7*sizeof(float)));
  for(int eye=0;eye<2;eye++){auto q=views[eye].pose.orientation;auto p=views[eye].pose.position;auto f=views[eye].fov;glViewport(eye*1024,0,1024,1024);glUniform4f(glGetUniformLocation(program,"rotation"),-q.x,-q.y,-q.z,q.w);glUniform3f(glGetUniformLocation(program,"eye"),p.x,p.y,p.z);glUniform4f(glGetUniformLocation(program,"tangents"),tanf(f.angleLeft),tanf(f.angleRight),tanf(f.angleDown),tanf(f.angleUp));
   glActiveTexture(GL_TEXTURE0);glUniform1i(glGetUniformLocation(program,"tex"),0);size_t first=0;
   auto drawRange=[&](size_t at,size_t count,GLuint tex){if(!count)return;glUniform1i(glGetUniformLocation(program,"textured"),tex!=0);glBindTexture(GL_TEXTURE_2D,tex);glDrawArrays(GL_TRIANGLES,at,count);};
   for(auto& batch:batches){drawRange(first,batch.first-first,0);drawRange(batch.first,batch.count,batch.texture);first=batch.first+batch.count;}drawRange(first,vertices.size()-first,0);
   output[eye]={XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};output[eye].pose=views[eye].pose;output[eye].fov=f;output[eye].subImage.swapchain=chain;output[eye].subImage.imageRect={{eye*1024,0},{1024,1024}};}
  glDisable(GL_DEPTH_TEST);glBindFramebuffer(GL_FRAMEBUFFER,0);glFlush();XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(xrReleaseSwapchainImage(chain,&r),"Release hand visuals");
 }
 void cleanup(){for(auto& model:models)if(model){for(auto& t:model->textures)if(t.gpu){glDeleteTextures(1,&t.gpu);t.gpu=0;}model.reset();}if(depthBuffer)glDeleteRenderbuffers(1,&depthBuffer);for(auto h:tracker)if(h&&destroy)destroy(h);if(chain)xrDestroySwapchain(chain);if(program)glDeleteProgram(program);if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);if(fbo)glDeleteFramebuffers(1,&fbo);}
};
