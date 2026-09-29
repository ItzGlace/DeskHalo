
#pragma once
extern "C" JNIEXPORT jstring JNICALL Java_itz_glace_deskhalo_ModelSkins_load(JNIEnv* env,jclass cls,jint slot,jstring file,jstring extension){
 try{
  if(slot<0||slot>4)throw std::runtime_error("Invalid skin slot");
  const char* p=env->GetStringUTFChars(file,nullptr);std::string path(p);env->ReleaseStringUTFChars(file,p);
  const char* e=env->GetStringUTFChars(extension,nullptr);std::string ext(e);env->ReleaseStringUTFChars(extension,e);
  if(ext!="obj"&&ext!="glb"&&ext!="fbx")throw std::runtime_error("Choose an OBJ, GLB or FBX");
  std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot read imported model");
  auto size=in.tellg();if(size<=0||size>32*1024*1024)throw std::runtime_error("Model exceeds 32 MB");
  std::vector<char> bytes(static_cast<size_t>(size));in.seekg(0);if(!in.read(bytes.data(),bytes.size()))throw std::runtime_error("Incomplete model file");
  auto model=importSkin(bytes,ext,slot==4?2:slot,[&](const unsigned char* data,size_t n){
   SkinTexture texture;jbyteArray encoded=env->NewByteArray(n);env->SetByteArrayRegion(encoded,0,n,(const jbyte*)data);
   jmethodID decode=env->GetStaticMethodID(cls,"decode","([B)Landroid/graphics/Bitmap;");
   jobject bitmap=env->CallStaticObjectMethod(cls,decode,encoded);env->DeleteLocalRef(encoded);
   if(env->ExceptionCheck()){env->ExceptionClear();throw std::runtime_error("Texture decoding failed");}
   if(!bitmap)return texture;
   AndroidBitmapInfo info;void* pixels=nullptr;
   if(AndroidBitmap_getInfo(env,bitmap,&info)==ANDROID_BITMAP_RESULT_SUCCESS&&info.format==ANDROID_BITMAP_FORMAT_RGBA_8888&&AndroidBitmap_lockPixels(env,bitmap,&pixels)==ANDROID_BITMAP_RESULT_SUCCESS){
    texture.width=info.width;texture.height=info.height;texture.rgba.resize(info.width*info.height*4);
    for(unsigned y=0;y<info.height;y++)memcpy(texture.rgba.data()+y*info.width*4,(unsigned char*)pixels+y*info.stride,info.width*4);
    AndroidBitmap_unlockPixels(env,bitmap);
   }
   env->DeleteLocalRef(bitmap);return texture;
  });
  if(slot<4){std::lock_guard<std::mutex> guard(skinLock);skins[slot]=model;}else return env->NewStringUTF("Validated model");
  return env->NewStringUTF(slot<2?(model->rigged?"Loaded • articulated OpenXR hand":"Loaded • rigid hand follows wrist"):"Loaded • controller skin");
 }catch(const std::exception& e){return env->NewStringUTF((std::string("Error: ")+e.what()).c_str());}
}
extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_ModelSkins_clear(JNIEnv*,jclass,jint slot){
 if(slot<0||slot>3)return;std::lock_guard<std::mutex> guard(skinLock);skins[slot].reset();
}
extern "C" JNIEXPORT void JNICALL Java_itz_glace_deskhalo_ModelSkins_transform(JNIEnv*,jclass,jint slot,jfloat scale,jfloat yaw){
 if(slot<0||slot>3||!std::isfinite(scale)||!std::isfinite(yaw))return;
 skinScale[slot]=std::clamp(scale,.25f,4.f);skinYaw[slot]=yaw;
}
