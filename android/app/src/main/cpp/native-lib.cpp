#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <array>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <chrono>
#include <algorithm>
#include "ModelSkins.h"

namespace {
JavaVM* vm=nullptr; jobject activity=nullptr;
std::thread renderThread;
std::atomic<bool> stop{false},recenter{false};
std::atomic<int> panelCount{1},videoWidth{1280},videoHeight{720};
std::atomic<float> panelWidth{1.5f},panelDistance{1.25f},keyboardY{-.48f},keyboardZ{-.65f};
std::atomic<bool> measuringKeyboard{false},resetKeyboardPose{false};
std::atomic<float> measuredKeyboardWidth{0},measuredKeyboardDepth{0};
std::atomic<bool> passthroughEnabled{true},keyboardVisible{true};std::atomic<int> requestedRefresh{60};
struct PointerState{std::atomic<float> u{0},v{0};std::atomic<bool> visible{false};std::atomic<int64_t> tick{0};};std::array<PointerState,3> pointers;
int64_t milliseconds(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
GLuint shader(GLenum type,const char* source){GLuint value=glCreateShader(type);glShaderSource(value,1,&source,nullptr);glCompileShader(value);GLint ok;glGetShaderiv(value,GL_COMPILE_STATUS,&ok);if(!ok){char log[1024];glGetShaderInfoLog(value,1024,nullptr,log);throw std::runtime_error(log);}return value;}
struct Pixels { std::mutex lock; int w=0,h=0; uint64_t version=0; std::vector<uint8_t> bytes; };
std::array<Pixels,7> pixels;
void check(XrResult r,const char* label){if(XR_FAILED(r))throw std::runtime_error(std::string(label)+" ("+std::to_string(r)+")");}
XrPosef identity(){XrPosef p{};p.orientation.w=1;return p;}
XrVector3f rotate(XrQuaternionf q,XrVector3f v){
 XrVector3f t{2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
 return {v.x+q.w*t.x+q.y*t.z-q.z*t.y,v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
XrQuaternionf multiply(XrQuaternionf a,XrQuaternionf b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
struct Surface {XrSwapchain handle=XR_NULL_HANDLE;int w=0,h=0;uint64_t version=0;std::vector<XrSwapchainImageOpenGLESKHR> images;};
#include "Tracking.h"
void render(){
 JNIEnv* env=nullptr;vm->AttachCurrentThread(&env,nullptr);
 XrInstance instance=XR_NULL_HANDLE;XrSession session=XR_NULL_HANDLE;XrSpace local=XR_NULL_HANDLE,view=XR_NULL_HANDLE,aimSpace=XR_NULL_HANDLE;
 XrActionSet actionSet=XR_NULL_HANDLE; XrAction aim=XR_NULL_HANDLE,select=XR_NULL_HANDLE,menu=XR_NULL_HANDLE;
 EGLDisplay display=EGL_NO_DISPLAY;EGLContext context=EGL_NO_CONTEXT;EGLSurface pbuffer=EGL_NO_SURFACE;
 std::array<Surface,9> surfaces;TrackedVisuals tracked;XrAction grip[2]{};XrAction buttonActions[2][7]{};XrSpace gripSpace[2]{};XrPosef snappedKeyboard=identity();bool keyboardAnchored=false;float previousKeyboardY=keyboardY,previousKeyboardZ=keyboardZ;
 XrSwapchain background=XR_NULL_HANDLE;
 XrPassthroughFB passthrough=XR_NULL_HANDLE;XrPassthroughLayerFB passthroughLayer=XR_NULL_HANDLE;
 PFN_xrDestroyPassthroughFB destroyPassthrough=nullptr;PFN_xrDestroyPassthroughLayerFB destroyPassthroughLayer=nullptr;
 GLuint external[3]{},videoProgram=0,videoFbo=0,vao=0;bool srgbWriteControl=false;
 jclass javaClass=env->GetObjectClass(activity);
 jmethodID updateVideo=env->GetMethodID(javaClass,"updateVideo","(I[F)Z");jfloatArray matrix=env->NewFloatArray(16);
 bool texturesCreated=false;

 try {
  PFN_xrInitializeLoaderKHR init=nullptr; xrGetInstanceProcAddr(XR_NULL_HANDLE,"xrInitializeLoaderKHR",(PFN_xrVoidFunction*)&init);
  if(init){XrLoaderInitInfoAndroidKHR info{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};info.applicationVM=vm;info.applicationContext=activity;check(init((XrLoaderInitInfoBaseHeaderKHR*)&info),"Initialize loader");}
  std::vector<const char*> extensions{XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
  uint32_t extCount=0;xrEnumerateInstanceExtensionProperties(nullptr,0,&extCount,nullptr);std::vector<XrExtensionProperties> ext(extCount,{XR_TYPE_EXTENSION_PROPERTIES});xrEnumerateInstanceExtensionProperties(nullptr,extCount,&extCount,ext.data());
  bool hasPassthrough=false;for(auto& e:ext)if(strcmp(e.extensionName,XR_FB_PASSTHROUGH_EXTENSION_NAME)==0)hasPassthrough=true;
  bool hasRefresh=false,hasColor=false;for(auto& e:ext){if(strcmp(e.extensionName,XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME)==0)hasRefresh=true;if(strcmp(e.extensionName,XR_FB_COLOR_SPACE_EXTENSION_NAME)==0)hasColor=true;}
  bool hasHands=false,hasHandAim=false;for(auto& e:ext){if(strcmp(e.extensionName,XR_EXT_HAND_TRACKING_EXTENSION_NAME)==0)hasHands=true;if(strcmp(e.extensionName,XR_FB_HAND_TRACKING_AIM_EXTENSION_NAME)==0)hasHandAim=true;}if(hasHands)extensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);if(hasHands&&hasHandAim)extensions.push_back(XR_FB_HAND_TRACKING_AIM_EXTENSION_NAME);
  if(hasRefresh)extensions.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);if(hasColor)extensions.push_back(XR_FB_COLOR_SPACE_EXTENSION_NAME);
  if(hasPassthrough)extensions.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
  XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};androidInfo.applicationVM=vm;androidInfo.applicationActivity=activity;
  XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};create.next=&androidInfo;strcpy(create.applicationInfo.applicationName,"DeskHalo");create.applicationInfo.applicationVersion=1;create.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,34);create.enabledExtensionCount=extensions.size();create.enabledExtensionNames=extensions.data();
  check(xrCreateInstance(&create,&instance),"Create OpenXR instance");
  XrSystemGetInfo sys{XR_TYPE_SYSTEM_GET_INFO};sys.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;XrSystemId system;check(xrGetSystem(instance,&sys,&system),"Find headset");
  PFN_xrGetOpenGLESGraphicsRequirementsKHR requirements=nullptr;check(xrGetInstanceProcAddr(instance,"xrGetOpenGLESGraphicsRequirementsKHR",(PFN_xrVoidFunction*)&requirements),"GLES requirements function");
  XrGraphicsRequirementsOpenGLESKHR graphics{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};check(requirements(instance,system,&graphics),"GLES requirements");
  display=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(!eglInitialize(display,nullptr,nullptr))throw std::runtime_error("EGL initialization failed");
  EGLint attrs[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};EGLConfig config;EGLint found;
  eglChooseConfig(display,attrs,&config,1,&found);if(!found)throw std::runtime_error("No GLES3 config");
  EGLint ctx[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(display,config,EGL_NO_CONTEXT,ctx);
  EGLint pb[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};pbuffer=eglCreatePbufferSurface(display,config,pb);
  if(!eglMakeCurrent(display,pbuffer,pbuffer,context))throw std::runtime_error("EGL context failed");
  GLint extensionCount=0;glGetIntegerv(GL_NUM_EXTENSIONS,&extensionCount);
  for(GLint i=0;i<extensionCount;i++)if(strcmp((const char*)glGetStringi(GL_EXTENSIONS,i),"GL_EXT_sRGB_write_control")==0)srgbWriteControl=true;
  __android_log_print(ANDROID_LOG_INFO,"DeskHalo","Video color: encoded SDR passthrough, sRGB write control=%d",srgbWriteControl);
  const char* vertex=R"(#version 300 es
out vec2 uv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.0-1.0,0,1);})";
  const char* fragment=R"(#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision mediump float;
in vec2 uv;uniform samplerExternalOES video;uniform mat4 transform;uniform bool srgbTarget;out vec4 color;
void main(){color=texture(video,(transform*vec4(uv,0,1)).xy);if(srgbTarget){vec3 low=color.rgb/12.92;vec3 high=pow((color.rgb+0.055)/1.055,vec3(2.4));color.rgb=mix(high,low,lessThanEqual(color.rgb,vec3(0.04045)));}color.a=1.0;})";
  GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,fragment);videoProgram=glCreateProgram();glAttachShader(videoProgram,vs);glAttachShader(videoProgram,fs);glLinkProgram(videoProgram);glDeleteShader(vs);glDeleteShader(fs);
  GLint linked;glGetProgramiv(videoProgram,GL_LINK_STATUS,&linked);if(!linked)throw std::runtime_error("Video shader link failed");
  glGenFramebuffers(1,&videoFbo);glGenVertexArrays(1,&vao);glGenTextures(3,external);
  auto textureMethod=env->GetMethodID(javaClass,"onVideoTexture","(II)V");
  texturesCreated=true;
  for(int i=0;i<3;i++){glBindTexture(GL_TEXTURE_EXTERNAL_OES,external[i]);glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);env->CallVoidMethod(activity,textureMethod,i,(jint)external[i]);}
  XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};binding.display=display;binding.config=config;binding.context=context;
  XrSessionCreateInfo sc{XR_TYPE_SESSION_CREATE_INFO};sc.next=&binding;sc.systemId=system;check(xrCreateSession(instance,&sc,&session),"Create VR session");
  PFN_xrRequestDisplayRefreshRateFB requestRefresh=nullptr;PFN_xrGetDisplayRefreshRateFB getRefresh=nullptr;std::vector<float> refreshRates;
  if(hasRefresh){PFN_xrEnumerateDisplayRefreshRatesFB enumerate=nullptr;xrGetInstanceProcAddr(instance,"xrEnumerateDisplayRefreshRatesFB",(PFN_xrVoidFunction*)&enumerate);xrGetInstanceProcAddr(instance,"xrRequestDisplayRefreshRateFB",(PFN_xrVoidFunction*)&requestRefresh);xrGetInstanceProcAddr(instance,"xrGetDisplayRefreshRateFB",(PFN_xrVoidFunction*)&getRefresh);uint32_t n=0;if(enumerate&&XR_SUCCEEDED(enumerate(session,0,&n,nullptr))){refreshRates.resize(n);enumerate(session,n,&n,refreshRates.data());for(float rate:refreshRates)__android_log_print(ANDROID_LOG_INFO,"DeskHalo","Supported refresh %.0f Hz",rate);}}
  if(hasColor){PFN_xrSetColorSpaceFB setColor=nullptr;xrGetInstanceProcAddr(instance,"xrSetColorSpaceFB",(PFN_xrVoidFunction*)&setColor);if(setColor)__android_log_print(ANDROID_LOG_INFO,"DeskHalo","Rec.709 color space result %d",setColor(session,XR_COLOR_SPACE_REC709_FB));}
  int appliedRefresh=0;
  std::string passthroughStatus="Passthrough unavailable on this runtime";
  if(hasPassthrough){
   PFN_xrCreatePassthroughFB createPassthrough=nullptr;PFN_xrCreatePassthroughLayerFB createLayer=nullptr;
   xrGetInstanceProcAddr(instance,"xrCreatePassthroughFB",(PFN_xrVoidFunction*)&createPassthrough);xrGetInstanceProcAddr(instance,"xrCreatePassthroughLayerFB",(PFN_xrVoidFunction*)&createLayer);
   xrGetInstanceProcAddr(instance,"xrDestroyPassthroughFB",(PFN_xrVoidFunction*)&destroyPassthrough);xrGetInstanceProcAddr(instance,"xrDestroyPassthroughLayerFB",(PFN_xrVoidFunction*)&destroyPassthroughLayer);
   XrPassthroughCreateInfoFB pi{XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};pi.flags=XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
   XrResult pr=createPassthrough?createPassthrough(session,&pi,&passthrough):XR_ERROR_FUNCTION_UNSUPPORTED;
   if(XR_SUCCEEDED(pr)){XrPassthroughLayerCreateInfoFB li{XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};li.passthrough=passthrough;li.flags=XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;li.purpose=XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;pr=createLayer(session,&li,&passthroughLayer);}
   passthroughStatus=XR_SUCCEEDED(pr)?"Passthrough layer ready":"Passthrough initialization failed: "+std::to_string(pr);
  }
  __android_log_print(ANDROID_LOG_INFO,"DeskHalo","%s",passthroughStatus.c_str());jstring ps=env->NewStringUTF(passthroughStatus.c_str());env->CallVoidMethod(activity,env->GetMethodID(javaClass,"onPassthroughStatus","(Ljava/lang/String;)V"),ps);env->DeleteLocalRef(ps);
  XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;space.poseInReferenceSpace=identity();check(xrCreateReferenceSpace(session,&space,&local),"Local space");
  space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;check(xrCreateReferenceSpace(session,&space,&view),"View space");
  XrActionSetCreateInfo ac{XR_TYPE_ACTION_SET_CREATE_INFO};strcpy(ac.actionSetName,"workspace");strcpy(ac.localizedActionSetName,"Workspace");check(xrCreateActionSet(instance,&ac,&actionSet),"Action set");
  XrActionCreateInfo ai{XR_TYPE_ACTION_CREATE_INFO};ai.actionType=XR_ACTION_TYPE_POSE_INPUT;strcpy(ai.actionName,"aim");strcpy(ai.localizedActionName,"Aim");check(xrCreateAction(actionSet,&ai,&aim),"Aim action");
  ai.actionType=XR_ACTION_TYPE_BOOLEAN_INPUT;strcpy(ai.actionName,"select");strcpy(ai.localizedActionName,"Select");check(xrCreateAction(actionSet,&ai,&select),"Select action");
  strcpy(ai.actionName,"menu");strcpy(ai.localizedActionName,"Menu");check(xrCreateAction(actionSet,&ai,&menu),"Menu action");
  auto path=[&](const char* value){XrPath result;check(xrStringToPath(instance,value,&result),"Input path");return result;};
  for(int h=0;h<2;h++){ai.actionType=XR_ACTION_TYPE_POSE_INPUT;strcpy(ai.actionName,h?"right_grip":"left_grip");strcpy(ai.localizedActionName,h?"Right controller":"Left controller");check(xrCreateAction(actionSet,&ai,&grip[h]),"Grip action");}
  std::vector<XrActionSuggestedBinding> bindings={{aim,path("/user/hand/right/input/aim/pose")},{select,path("/user/hand/right/input/trigger/value")},{menu,path("/user/hand/left/input/menu/click")},{grip[0],path("/user/hand/left/input/grip/pose")},{grip[1],path("/user/hand/right/input/grip/pose")}};

  const char* suffix[7]={"trigger/value","squeeze/value","a/click","b/click","thumbstick/click","menu/click","thumbstick"};
  for(int h=0;h<2;h++)for(int b=0;b<7;b++){
   if(h==1&&b==5)continue; // System/Oculus button belongs to the runtime.
   ai.actionType=b<2?XR_ACTION_TYPE_FLOAT_INPUT:b==6?XR_ACTION_TYPE_VECTOR2F_INPUT:XR_ACTION_TYPE_BOOLEAN_INPUT;
   std::string name="visual_"+std::to_string(h)+"_"+std::to_string(b);
   strcpy(ai.actionName,name.c_str());strcpy(ai.localizedActionName,name.c_str());check(xrCreateAction(actionSet,&ai,&buttonActions[h][b]),"Visual button action");
   std::string component=!h&&b==2?"x/click":!h&&b==3?"y/click":suffix[b];
   std::string full=std::string("/user/hand/")+(h?"right":"left")+"/input/"+component;
   bindings.push_back({buttonActions[h][b],path(full.c_str())});
  }
  XrInteractionProfileSuggestedBinding suggest{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};suggest.interactionProfile=path("/interaction_profiles/oculus/touch_controller");suggest.countSuggestedBindings=bindings.size();suggest.suggestedBindings=bindings.data();check(xrSuggestInteractionProfileBindings(instance,&suggest),"Touch controller bindings");
  XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&actionSet;check(xrAttachSessionActionSets(session,&attach),"Attach actions");
  XrActionSpaceCreateInfo asp{XR_TYPE_ACTION_SPACE_CREATE_INFO};asp.action=aim;asp.poseInActionSpace=identity();check(xrCreateActionSpace(session,&asp,&aimSpace),"Aim space");for(int h=0;h<2;h++){asp.action=grip[h];check(xrCreateActionSpace(session,&asp,&gripSpace[h]),"Grip space");}
  uint32_t formatCount;check(xrEnumerateSwapchainFormats(session,0,&formatCount,nullptr),"Formats");std::vector<int64_t> formats(formatCount);check(xrEnumerateSwapchainFormats(session,formatCount,&formatCount,formats.data()),"Formats");
  int64_t format=GL_SRGB8_ALPHA8;if(std::find(formats.begin(),formats.end(),format)==formats.end())format=GL_RGBA8;
  tracked.init(instance,session,hasHands,hasHandAim,format);
  XrSwapchainCreateInfo backgroundInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};backgroundInfo.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;backgroundInfo.format=format;backgroundInfo.sampleCount=1;backgroundInfo.width=1024;backgroundInfo.height=512;backgroundInfo.faceCount=1;backgroundInfo.arraySize=1;backgroundInfo.mipCount=1;
  check(xrCreateSwapchain(session,&backgroundInfo,&background),"Background swapchain");uint32_t bgCount;xrEnumerateSwapchainImages(background,0,&bgCount,nullptr);std::vector<XrSwapchainImageOpenGLESKHR> bgImages(bgCount,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});xrEnumerateSwapchainImages(background,bgCount,&bgCount,(XrSwapchainImageBaseHeader*)bgImages.data());
  uint32_t bgIndex;XrSwapchainImageAcquireInfo bgAcquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};xrAcquireSwapchainImage(background,&bgAcquire,&bgIndex);XrSwapchainImageWaitInfo bgWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};bgWait.timeout=XR_INFINITE_DURATION;xrWaitSwapchainImage(background,&bgWait);
  GLuint framebuffer;glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,bgImages[bgIndex].image,0);glViewport(0,0,1024,512);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glFlush();glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&framebuffer);XrSwapchainImageReleaseInfo bgRelease{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};xrReleaseSwapchainImage(background,&bgRelease);
  {auto& p=pixels[5];std::lock_guard<std::mutex> guard(p.lock);p.w=p.h=16;p.bytes.assign(16*16*4,255);p.version++;}
  bool positioned=false,running=false,focused=false,previousSelect=false,previousMenu=false,menuVisible=false;XrPosef menuPose=identity();menuPose.position={0,-.1f,-.9f};std::array<bool,3> videoReady{};float transforms[3][16]{};
  __android_log_print(ANDROID_LOG_INFO,"DeskHalo","OpenXR initialized successfully");
  while(!stop){
   XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
   while(xrPollEvent(instance,&event)==XR_SUCCESS){
    if(event.type==XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB){auto* rate=(XrEventDataDisplayRefreshRateChangedFB*)&event;env->CallVoidMethod(activity,env->GetMethodID(javaClass,"onRefreshRate","(F)V"),rate->toDisplayRefreshRate);__android_log_print(ANDROID_LOG_INFO,"DeskHalo","Refresh now %.0f Hz",rate->toDisplayRefreshRate);}
    if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){auto* state=(XrEventDataSessionStateChanged*)&event;bool wasFocused=focused;focused=state->state==XR_SESSION_STATE_FOCUSED;if(focused!=wasFocused)env->CallVoidMethod(activity,env->GetMethodID(javaClass,"onSessionFocus","(Z)V"),(jboolean)focused);
     __android_log_print(ANDROID_LOG_INFO,"DeskHalo","Session state %d",state->state);
     if(focused&&!positioned){recenter=true;positioned=true;}
     if(state->state==XR_SESSION_STATE_READY){XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};bi.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;check(xrBeginSession(session,&bi),"Begin session");running=true;}
     if(state->state==XR_SESSION_STATE_STOPPING){xrEndSession(session);running=false;}
     if(state->state==XR_SESSION_STATE_EXITING||state->state==XR_SESSION_STATE_LOSS_PENDING)stop=true;
    }event={XR_TYPE_EVENT_DATA_BUFFER};
   }
   if(!running){std::this_thread::sleep_for(std::chrono::milliseconds(20));continue;}
   if(focused&&requestRefresh&&appliedRefresh!=requestedRefresh){appliedRefresh=requestedRefresh;float best=0;for(float rate:refreshRates)if(rate<=appliedRefresh&&rate>best)best=rate;if(best==0&&!refreshRates.empty())best=*std::min_element(refreshRates.begin(),refreshRates.end());if(best>0){XrResult result=requestRefresh(session,best);float actual=best;if(getRefresh)getRefresh(session,&actual);env->CallVoidMethod(activity,env->GetMethodID(javaClass,"onRefreshRate","(F)V"),actual);__android_log_print(ANDROID_LOG_INFO,"DeskHalo","Refresh request %.0f result %d current %.0f",best,result,actual);}}
   XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};XrFrameState frame{XR_TYPE_FRAME_STATE};check(xrWaitFrame(session,&wi,&frame),"Wait frame");
   XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};check(xrBeginFrame(session,&begin),"Begin frame");
   if(recenter.exchange(false)){keyboardAnchored=false;tracked.known[0]=tracked.known[1]=false;tracked.snapped=false;
    XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};if(XR_SUCCEEDED(xrLocateSpace(view,local,frame.predictedDisplayTime,&head))&&(head.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)){
     // Recreate a local reference space from the runtime origin, not from the old offset.
     XrReferenceSpaceCreateInfo baseInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};baseInfo.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;baseInfo.poseInReferenceSpace=identity();XrSpace base;
     if(XR_SUCCEEDED(xrCreateReferenceSpace(session,&baseInfo,&base))){xrLocateSpace(view,base,frame.predictedDisplayTime,&head);auto forward=rotate(head.pose.orientation,{0,0,-1});float yaw=atan2f(-forward.x,-forward.z);baseInfo.poseInReferenceSpace=head.pose;baseInfo.poseInReferenceSpace.orientation={0,sinf(yaw/2),0,cosf(yaw/2)};XrSpace fresh;if(XR_SUCCEEDED(xrCreateReferenceSpace(session,&baseInfo,&fresh))){xrDestroySpace(local);local=fresh;}xrDestroySpace(base);}
    }
   }
   XrSpaceLocation controllers[2]{{XR_TYPE_SPACE_LOCATION},{XR_TYPE_SPACE_LOCATION}};
   bool cursor=false;float cursorX=0,cursorY=0;
   if(focused){XrActiveActionSet active{actionSet,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;xrSyncActions(session,&sync);
    for(int h=0;h<2;h++){XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.action=grip[h];XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};xrGetActionStatePose(session,&g,&state);if(state.isActive)xrLocateSpace(gripSpace[h],local,frame.predictedDisplayTime,&controllers[h]);}

    for(int h=0;h<2;h++){
     auto& value=tracked.buttons[h];value={};
     for(int b=0;b<7;b++){
      if(!buttonActions[h][b])continue;
      XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.action=buttonActions[h][b];
      if(b<2){XrActionStateFloat s{XR_TYPE_ACTION_STATE_FLOAT};xrGetActionStateFloat(session,&g,&s);if(s.isActive){if(!b)value.trigger=s.currentState;else value.squeeze=s.currentState;}}
      else if(b==6){XrActionStateVector2f s{XR_TYPE_ACTION_STATE_VECTOR2F};xrGetActionStateVector2f(session,&g,&s);if(s.isActive){value.stickX=s.currentState.x;value.stickY=s.currentState.y;}}
      else{XrActionStateBoolean s{XR_TYPE_ACTION_STATE_BOOLEAN};xrGetActionStateBoolean(session,&g,&s);bool down=s.isActive&&s.currentState;if(b==2)value.first=down;if(b==3)value.second=down;if(b==4)value.stick=down;if(b==5)value.menu=down;}
     }
    }
    tracked.update(local,frame.predictedDisplayTime,controllers);
    if(keyboardY!=previousKeyboardY||keyboardZ!=previousKeyboardZ){keyboardAnchored=false;previousKeyboardY=keyboardY;previousKeyboardZ=keyboardZ;}

    if(resetKeyboardPose.exchange(false))keyboardAnchored=false;
    if(measuringKeyboard){
     float points[24];jfloatArray input=nullptr;
     if(tracked.framePoints(points)){input=env->NewFloatArray(24);env->SetFloatArrayRegion(input,0,24,points);}
     jfloatArray result=(jfloatArray)env->CallObjectMethod(activity,env->GetMethodID(javaClass,"onKeyboardFrame","([F)[F"),input);
     if(input)env->DeleteLocalRef(input);
     if(result){if(env->GetArrayLength(result)==9){float m[9];env->GetFloatArrayRegion(result,0,9,m);snappedKeyboard.position={m[0],m[1],m[2]};snappedKeyboard.orientation={m[3],m[4],m[5],m[6]};measuredKeyboardWidth=m[7];measuredKeyboardDepth=m[8];keyboardAnchored=true;measuringKeyboard=false;}env->DeleteLocalRef(result);}
    }

    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=select;XrActionStateBoolean selected{XR_TYPE_ACTION_STATE_BOOLEAN};xrGetActionStateBoolean(session,&get,&selected);
    get.action=menu;XrActionStateBoolean menuState{XR_TYPE_ACTION_STATE_BOOLEAN};xrGetActionStateBoolean(session,&get,&menuState);
    bool menuPressed=(menuState.isActive&&menuState.currentState)||(!measuringKeyboard&&tracked.pinch(0));if(menuPressed&&!previousMenu){menuVisible=!menuVisible;
     if(menuVisible){XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};if(XR_SUCCEEDED(xrLocateSpace(view,local,frame.predictedDisplayTime,&head))){auto forward=rotate(head.pose.orientation,{0,0,-1});float yaw=atan2f(-forward.x,-forward.z);menuPose.orientation={0,sinf(yaw/2),0,cosf(yaw/2)};auto offset=rotate(menuPose.orientation,{0,-.1f,-.9f});menuPose.position={head.pose.position.x+offset.x,head.pose.position.y+offset.y,head.pose.position.z+offset.z};}}
     env->CallVoidMethod(activity,env->GetMethodID(javaClass,"onMenuChanged","(Z)V"),(jboolean)menuVisible);
    }previousMenu=menuPressed;
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};xrLocateSpace(aimSpace,local,frame.predictedDisplayTime,&loc);
    if(tracked.active[1]&&tracked.hasAim&&(tracked.aim[1].status&XR_HAND_TRACKING_AIM_VALID_BIT_FB)){loc.pose=tracked.aim[1].aimPose;loc.locationFlags=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT;selected.currentState=tracked.pinch(1);}
    if(menuVisible&&(loc.locationFlags&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)&&(loc.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)){
     XrQuaternionf inverse{-menuPose.orientation.x,-menuPose.orientation.y,-menuPose.orientation.z,menuPose.orientation.w};auto dir=rotate(inverse,rotate(loc.pose.orientation,{0,0,-1}));auto pos=rotate(inverse,{loc.pose.position.x-menuPose.position.x,loc.pose.position.y-menuPose.position.y,loc.pose.position.z-menuPose.position.z});
     if(dir.z<-.001f){float t=-pos.z/dir.z;cursorX=pos.x+t*dir.x;cursorY=pos.y+t*dir.y;cursor=t>0&&fabsf(cursorX)<.525f&&fabsf(cursorY)<.307617f;
      if(cursor&&selected.currentState&&!previousSelect)env->CallVoidMethod(activity,env->GetMethodID(javaClass,"onVrClick","(FF)V"),(cursorX/.525f+1)*512,(1-cursorY/.307617f)*300);
     }
    }
    if(!menuVisible&&(selected.currentState||previousSelect)&&(loc.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)&&(loc.locationFlags&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)){
     for(int slot=0;slot<panelCount;slot++){float width=panelWidth,radius=panelDistance,spacing=2*atanf(width/(2*radius))+.08f,angle=slot==1?spacing:slot==2?-spacing:0;XrQuaternionf inverse{0,-sinf(angle/2),0,cosf(angle/2)};auto direction=rotate(inverse,rotate(loc.pose.orientation,{0,0,-1}));auto position=rotate(inverse,{loc.pose.position.x+sinf(angle)*radius,loc.pose.position.y,loc.pose.position.z+cosf(angle)*radius});if(direction.z<-.001f){float t=-position.z/direction.z,x=position.x+t*direction.x,y=position.y+t*direction.y;if(t>0&&fabsf(x)<width/2&&fabsf(y)<width*9/32){env->CallVoidMethod(activity,env->GetMethodID(javaClass,"onDesktopPointer","(IFFZ)V"),slot,x/width+.5f,.5f-y/(width*9/16),(jboolean)selected.currentState);break;}}}
    }
    previousSelect=selected.currentState;
   }else {previousSelect=false;previousMenu=false;tracked.vertices.clear();}
   std::array<XrCompositionLayerQuad,9> quads{};std::vector<const XrCompositionLayerBaseHeader*> layers;
   XrCompositionLayerPassthroughFB cameraLayer{XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};cameraLayer.layerHandle=passthroughLayer;
   if(frame.shouldRender&&passthroughLayer&&passthroughEnabled)layers.push_back((const XrCompositionLayerBaseHeader*)&cameraLayer);
   XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};projection.space=local;if(passthroughLayer&&passthroughEnabled)projection.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
   std::array<XrCompositionLayerProjectionView,2> projectionViews{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
   std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=frame.predictedDisplayTime;locate.space=local;XrViewState viewState{XR_TYPE_VIEW_STATE};uint32_t viewsCount;
   if(frame.shouldRender&&XR_SUCCEEDED(xrLocateViews(session,&locate,&viewState,2,&viewsCount,views.data()))&&(viewState.viewStateFlags&XR_VIEW_STATE_ORIENTATION_VALID_BIT)){
    for(int eye=0;eye<2;eye++){projectionViews[eye].pose=views[eye].pose;projectionViews[eye].fov=views[eye].fov;projectionViews[eye].subImage.swapchain=background;projectionViews[eye].subImage.imageRect={{eye*512,0},{512,512}};}
    projection.viewCount=2;projection.views=projectionViews.data();layers.push_back((const XrCompositionLayerBaseHeader*)&projection);
   }
   if(frame.shouldRender)for(int i=0;i<9;i++){
    if(i<3&&i>=panelCount)continue;if(i==4&&!keyboardVisible)continue;if(i>=6&&(i-6>=panelCount||!pointers[i-6].visible||milliseconds()-pointers[i-6].tick>500))continue;if(i==3&&!menuVisible)continue;if(i==5&&!cursor)continue;
    auto& p=pixels[std::min(i,6)];auto& s=surfaces[i];std::lock_guard<std::mutex> guard(p.lock);
    bool newVideo=false;float* transform=transforms[std::min(i,2)];
    if(i<3){newVideo=env->CallBooleanMethod(activity,updateVideo,i,matrix);if(env->ExceptionCheck()){env->ExceptionClear();throw std::runtime_error("SurfaceTexture update failed");}if(newVideo){videoReady[i]=true;env->GetFloatArrayRegion(matrix,0,16,transform);p.version++;}if(!videoReady[i])continue;p.w=videoWidth;p.h=videoHeight;}
    if(p.w==0)continue;
    if(!s.handle||s.w!=p.w||s.h!=p.h){if(s.handle)xrDestroySwapchain(s.handle);s.handle=XR_NULL_HANDLE;s.version=0;
     XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.usageFlags=XR_SWAPCHAIN_USAGE_SAMPLED_BIT|XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;ci.format=format;ci.sampleCount=1;ci.width=p.w;ci.height=p.h;ci.faceCount=1;ci.arraySize=1;ci.mipCount=1;check(xrCreateSwapchain(session,&ci,&s.handle),"Create panel texture");s.w=p.w;s.h=p.h;
     uint32_t n;check(xrEnumerateSwapchainImages(s.handle,0,&n,nullptr),"Texture count");s.images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});check(xrEnumerateSwapchainImages(s.handle,n,&n,(XrSwapchainImageBaseHeader*)s.images.data()),"Textures");
    }
    if(s.version!=p.version){uint32_t index;XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};check(xrAcquireSwapchainImage(s.handle,&acquire,&index),"Acquire texture");XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=XR_INFINITE_DURATION;check(xrWaitSwapchainImage(s.handle,&wait),"Wait texture");
     if(i<3){

      glBindFramebuffer(GL_FRAMEBUFFER,videoFbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,s.images[index].image,0);glViewport(0,0,p.w,p.h);glUseProgram(videoProgram);// OES yields encoded RGB. Preserve those bytes in the sRGB image; the compositor decodes once.
      // If write control is unavailable, GLES automatically encodes linear shader output.
      if(srgbWriteControl)glDisable(GL_FRAMEBUFFER_SRGB_EXT);
      glUniform1i(glGetUniformLocation(videoProgram,"srgbTarget"),format==GL_SRGB8_ALPHA8&&!srgbWriteControl);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_EXTERNAL_OES,external[i]);glUniform1i(glGetUniformLocation(videoProgram,"video"),0);glUniformMatrix4fv(glGetUniformLocation(videoProgram,"transform"),1,GL_FALSE,transform);glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);if(srgbWriteControl)glEnable(GL_FRAMEBUFFER_SRGB_EXT);glBindFramebuffer(GL_FRAMEBUFFER,0);
     }else{glBindTexture(GL_TEXTURE_2D,s.images[index].image);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,p.w,p.h,GL_RGBA,GL_UNSIGNED_BYTE,p.bytes.data());}glFlush();
     XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(xrReleaseSwapchainImage(s.handle,&release),"Release texture");s.version=p.version;
    }
    auto& q=quads[i];q={XR_TYPE_COMPOSITION_LAYER_QUAD};q.space=local;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;q.subImage.swapchain=s.handle;q.subImage.imageRect={{0,0},{s.w,s.h}};q.pose=identity();
    if(i<3){float width=panelWidth;float radius=panelDistance;float spacing=2*atanf(width/(2*radius))+.08f;float angle=i==1?spacing:i==2?-spacing:0;
     q.pose.position={-sinf(angle)*radius,0,-cosf(angle)*radius};q.pose.orientation={0,sinf(angle/2),0,cosf(angle/2)};q.size={width,width*9/16};
    }else if(i==3){q.pose=menuPose;q.size={1.05f,.615234f};q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;}
    else if(i==4){q.pose.position={0,keyboardY,keyboardZ};q.pose.orientation={sinf(-85.f*3.14159265f/360.f),0,0,cosf(-85.f*3.14159265f/360.f)};if(keyboardAnchored){q.pose=snappedKeyboard;q.pose.orientation=multiply(q.pose.orientation,{sinf(5.f*3.14159265f/360.f),0,0,cosf(5.f*3.14159265f/360.f)});}q.size={measuredKeyboardWidth>0?float(measuredKeyboardWidth):.8f*s.w/1280.f,measuredKeyboardDepth>0?float(measuredKeyboardDepth):.8f*s.h/1280.f};}
    else if(i>=6){int slot=i-6;float width=panelWidth,radius=panelDistance,spacing=2*atanf(width/(2*radius))+.08f,angle=slot==1?spacing:slot==2?-spacing:0;
     q.pose.orientation={0,sinf(angle/2),0,cosf(angle/2)};float size=width*32.f/videoWidth;auto offset=rotate(q.pose.orientation,{(pointers[slot].u-.5f)*width+size/2,(.5f-pointers[slot].v)*width*9/16-size/2,.004f});q.pose.position={-sinf(angle)*radius+offset.x,offset.y,-cosf(angle)*radius+offset.z};q.size={size,size};q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    }
    else {q.pose=menuPose;auto offset=rotate(menuPose.orientation,{cursorX,cursorY,.005f});q.pose.position.x+=offset.x;q.pose.position.y+=offset.y;q.pose.position.z+=offset.z;q.size={.008f,.008f};}
    layers.push_back((const XrCompositionLayerBaseHeader*)&q);
   }
   std::array<XrCompositionLayerProjectionView,2> handViews;XrCompositionLayerProjection handLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
   if(frame.shouldRender&&!tracked.vertices.empty()&&(viewState.viewStateFlags&XR_VIEW_STATE_ORIENTATION_VALID_BIT)){tracked.draw(views,handViews);handLayer.space=local;handLayer.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;handLayer.viewCount=2;handLayer.views=handViews.data();layers.push_back((const XrCompositionLayerBaseHeader*)&handLayer);}
   XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frame.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;end.layerCount=layers.size();end.layers=layers.data();check(xrEndFrame(session,&end),"Submit frame");
  }
 }catch(const std::exception& e){__android_log_print(ANDROID_LOG_ERROR,"DeskHalo","%s",e.what());jclass cls=env->GetObjectClass(activity);auto method=env->GetMethodID(cls,"onNativeError","(Ljava/lang/String;)V");jstring message=env->NewStringUTF(e.what());env->CallVoidMethod(activity,method,message);env->DeleteLocalRef(message);env->DeleteLocalRef(cls);}
 tracked.cleanup();for(auto space:gripSpace)if(space)xrDestroySpace(space);
 for(auto& s:surfaces)if(s.handle)xrDestroySwapchain(s.handle);if(background)xrDestroySwapchain(background);
 if(texturesCreated)env->CallVoidMethod(activity,env->GetMethodID(javaClass,"releaseVideoTextures","()V"));
 glDeleteTextures(3,external);if(videoProgram)glDeleteProgram(videoProgram);if(videoFbo)glDeleteFramebuffers(1,&videoFbo);if(vao)glDeleteVertexArrays(1,&vao);
 if(passthroughLayer&&destroyPassthroughLayer)destroyPassthroughLayer(passthroughLayer);if(passthrough&&destroyPassthrough)destroyPassthrough(passthrough);
 env->DeleteLocalRef(matrix);env->DeleteLocalRef(javaClass);
 if(aimSpace)xrDestroySpace(aimSpace);if(view)xrDestroySpace(view);if(local)xrDestroySpace(local);if(session)xrDestroySession(session);if(actionSet)xrDestroyActionSet(actionSet);if(instance)xrDestroyInstance(instance);
 if(display!=EGL_NO_DISPLAY){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(pbuffer!=EGL_NO_SURFACE)eglDestroySurface(display,pbuffer);if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);eglTerminate(display);}vm->DetachCurrentThread();
}
}
extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_VrActivity_nativeStart(JNIEnv* env,jobject obj){
 if(renderThread.joinable())return;
 for(auto& p:pixels){std::lock_guard<std::mutex> guard(p.lock);p.w=p.h=0;p.bytes.clear();p.version=0;}
 recenter=false;env->GetJavaVM(&vm);activity=env->NewGlobalRef(obj);stop=false;renderThread=std::thread(render);
}
extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_VrActivity_nativeStop(JNIEnv* env,jobject){stop=true;if(renderThread.joinable())renderThread.join();if(activity){env->DeleteGlobalRef(activity);activity=nullptr;}}
extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_VrActivity_nativeSettings(JNIEnv*,jobject,jint count,jfloat width,jfloat distance,jfloat ky,jfloat kz,jboolean camera,jint vw,jint vh){panelCount=std::clamp((int)count,1,3);panelWidth=std::clamp((float)width,.6f,3.5f);panelDistance=std::clamp((float)distance,.6f,3.5f);keyboardY=ky;keyboardZ=kz;passthroughEnabled=camera;videoWidth=vw;videoHeight=vh;}
extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_VrActivity_nativeRecenter(JNIEnv*,jobject){recenter=true;}
extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_VrActivity_nativeUpload(JNIEnv* env,jobject,jint slot,jobject bitmap){
 if(slot<0||slot>6)return;AndroidBitmapInfo info;if(AndroidBitmap_getInfo(env,bitmap,&info)!=0||info.format!=ANDROID_BITMAP_FORMAT_RGBA_8888)return;
 void* data;if(AndroidBitmap_lockPixels(env,bitmap,&data)!=0)return;
 auto& p=pixels[slot];{std::lock_guard<std::mutex> guard(p.lock);p.w=info.width;p.h=info.height;p.bytes.resize(p.w*p.h*4);
 // OpenGL images start at the bottom row; Android bitmaps start at the top.
 for(int y=0;y<p.h;y++)memcpy(p.bytes.data()+(p.h-1-y)*p.w*4,(uint8_t*)data+y*info.stride,p.w*4);p.version++;}
 AndroidBitmap_unlockPixels(env,bitmap);
}

extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_VrActivity_nativeOptions(JNIEnv*,jobject,jboolean visible,jint rate){keyboardVisible=visible;requestedRefresh=rate;}
extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_VrActivity_nativeCursor(JNIEnv*,jobject,jint slot,jfloat u,jfloat v,jboolean visible){if(slot<0||slot>2)return;auto& p=pointers[slot];p.u=u;p.v=v;p.visible=visible;p.tick=milliseconds();}

#include "SkinJni.h"
extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_VrActivity_nativeMeasureKeyboard(JNIEnv*,jobject,jboolean enabled){measuringKeyboard=enabled;}
extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_VrActivity_nativeKeyboardBounds(JNIEnv*,jobject,jfloat width,jfloat depth,jboolean reset){if(std::isfinite(width)&&std::isfinite(depth)){measuredKeyboardWidth=std::clamp(width,0.f,1.1f);measuredKeyboardDepth=std::clamp(depth,0.f,.4f);}if(reset)resetKeyboardPose=true;}
