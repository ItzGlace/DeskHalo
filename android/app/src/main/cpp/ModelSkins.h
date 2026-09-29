
#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/IOSystem.hpp>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cctype>
#include <fstream>

struct SkinTexture {int width=0,height=0;std::vector<unsigned char> rgba;GLuint gpu=0;};
struct SkinWeight {int joint;float weight;XrVector3f local;};
struct SkinVertex {XrVector3f position;float u=0,v=0,r=1,g=1,b=1,a=1;std::vector<SkinWeight> weights;};
struct SkinMesh {std::vector<SkinVertex> vertices;float r=.7f,g=.72f,b=.76f,a=1;int texture=-1,control=-1;};
struct SkinModel {std::vector<SkinMesh> meshes;std::vector<SkinTexture> textures;bool rigged=false;};
std::mutex skinLock;
std::shared_ptr<SkinModel> skins[4];
std::atomic<float> skinScale[4]{{1},{1},{1},{1}},skinYaw[4]{{0},{0},{0},{0}};
using DecodeSkinImage=std::function<SkinTexture(const unsigned char*,size_t)>;

// Imported files are self-contained. Never resolve model-supplied filesystem paths.
struct SkinIO final:Assimp::IOSystem {
 bool Exists(const char*)const override{return false;}
 char getOsSeparator()const override{return '/';}
 Assimp::IOStream* Open(const char*,const char* = "rb")override{return nullptr;}
 void Close(Assimp::IOStream*)override{}
};
std::string skinName(std::string s){std::string o;for(unsigned char c:s)if(std::isalnum(c))o+=std::tolower(c);return o;}
int skinJoint(const std::string& raw){
 auto s=skinName(raw);
 // WebXR and OpenXR use matching joint frames. Normalize WebXR bone labels.
 for(const std::string token:{"finger","phalanx"}){size_t at;while((at=s.find(token))!=std::string::npos)s.erase(at,token.size());}
 auto pinky=s.find("pinky");if(pinky!=std::string::npos)s.replace(pinky,5,"little");
 // OpenXR names and common side/prefix variants. Ambiguous rigs remain rigid.
 const char* names[26]={"palm","wrist","thumbmetacarpal","thumbproximal","thumbdistal","thumbtip",
 "indexmetacarpal","indexproximal","indexintermediate","indexdistal","indextip",
 "middlemetacarpal","middleproximal","middleintermediate","middledistal","middletip",
 "ringmetacarpal","ringproximal","ringintermediate","ringdistal","ringtip",
 "littlemetacarpal","littleproximal","littleintermediate","littledistal","littletip"};
 for(int i=0;i<26;i++)if(s.find(names[i])!=std::string::npos)return i;
 return -1;
}
int skinControl(const std::string& raw){
 auto s=skinName(raw);
 if(s.find("trigger")!=std::string::npos)return 0;
 if(s.find("squeeze")!=std::string::npos||s=="gripbutton")return 1;
 if(s=="a"||s=="x"||s=="abutton"||s=="xbutton"||s.find("buttona")!=std::string::npos||s.find("buttonx")!=std::string::npos)return 2;
 if(s=="b"||s=="y"||s=="bbutton"||s=="ybutton"||s.find("buttonb")!=std::string::npos||s.find("buttony")!=std::string::npos)return 3;
 if(s.find("thumbstick")!=std::string::npos||s.find("joystick")!=std::string::npos)return 4;
 if(s.find("menu")!=std::string::npos)return 5;
 return -1;
}
std::shared_ptr<SkinModel> importSkin(const std::vector<char>& bytes,const std::string& extension,int slot,DecodeSkinImage decode){
 if(bytes.empty()||bytes.size()>32*1024*1024)throw std::runtime_error("Model must be between 1 byte and 32 MB");
 Assimp::Importer importer;importer.SetIOHandler(new SkinIO);
 const aiScene* scene=importer.ReadFileFromMemory(bytes.data(),bytes.size(),aiProcess_Triangulate|aiProcess_JoinIdenticalVertices|aiProcess_ValidateDataStructure|aiProcess_LimitBoneWeights,extension.c_str());
 if(!scene||!scene->mRootNode)throw std::runtime_error(std::string("Model could not be imported: ")+importer.GetErrorString());
 if(scene->mNumMeshes>128)throw std::runtime_error("Use at most 128 mesh parts");
 auto model=std::make_shared<SkinModel>();

 std::unordered_map<unsigned,int> materialTextures;
 size_t triangles=0;float extent=0;
 std::function<void(aiNode*,aiMatrix4x4)> visit=[&](aiNode* node,aiMatrix4x4 parent){
  auto transform=parent*node->mTransformation;
  for(unsigned index=0;index<node->mNumMeshes;index++){
   aiMesh* mesh=scene->mMeshes[node->mMeshes[index]];triangles+=mesh->mNumFaces;
   if(triangles>20000)throw std::runtime_error("Reduce model to 20,000 triangles or fewer");
   SkinMesh part;part.control=skinControl(node->mName.C_Str());if(part.control<0)part.control=skinControl(mesh->mName.C_Str());
   aiMaterial* mat=scene->mMaterials[mesh->mMaterialIndex];aiColor4D color(.7f,.72f,.76f,1);
   if(aiGetMaterialColor(mat,AI_MATKEY_BASE_COLOR,&color)!=AI_SUCCESS)aiGetMaterialColor(mat,AI_MATKEY_COLOR_DIFFUSE,&color);
   auto linear=[](float x){return std::isfinite(x)?std::clamp(x,0.f,1.f):1.f;};
   part.r=linear(color.r);part.g=linear(color.g);part.b=linear(color.b);part.a=std::clamp(color.a,0.f,1.f);
   if(materialTextures.count(mesh->mMaterialIndex))part.texture=materialTextures[mesh->mMaterialIndex];
   else {
    aiString path;auto ok=mat->GetTexture(aiTextureType_BASE_COLOR,0,&path);if(ok!=AI_SUCCESS)ok=mat->GetTexture(aiTextureType_DIFFUSE,0,&path);
    if(ok==AI_SUCCESS){
     const aiTexture* t=scene->GetEmbeddedTexture(path.C_Str());
     if(!t)throw std::runtime_error("Texture is external. Embed textures in a GLB/FBX, or export an untextured OBJ");
     SkinTexture tex;
     if(!t->mHeight){if(t->mWidth>16*1024*1024)throw std::runtime_error("Embedded texture exceeds 16 MB");tex=decode((unsigned char*)t->pcData,t->mWidth);}
     else {if(t->mWidth>2048||t->mHeight>2048)throw std::runtime_error("Textures must be at most 2048 x 2048");tex.width=t->mWidth;tex.height=t->mHeight;tex.rgba.resize(tex.width*tex.height*4);for(size_t p=0;p<size_t(tex.width*tex.height);p++){auto c=t->pcData[p];tex.rgba[4*p]=c.r;tex.rgba[4*p+1]=c.g;tex.rgba[4*p+2]=c.b;tex.rgba[4*p+3]=c.a;}}
     if(tex.width<=0||tex.height<=0)throw std::runtime_error("Embedded texture is not a supported PNG/JPEG");
     if(model->textures.size()>=8)throw std::runtime_error("Use at most eight textures");
     part.texture=model->textures.size();model->textures.push_back(std::move(tex));
    }
    materialTextures[mesh->mMaterialIndex]=part.texture;
   }
   std::vector<std::vector<SkinWeight>> weights(mesh->mNumVertices);
   if(slot<2)for(unsigned b=0;b<mesh->mNumBones;b++){
    auto bone=mesh->mBones[b];int joint=skinJoint(bone->mName.C_Str());
    if(joint<0)throw std::runtime_error("Hand bone names must use OpenXR joint names; see custom-models.md");
    for(unsigned w=0;w<bone->mNumWeights;w++){auto weight=bone->mWeights[w];if(weight.mVertexId>=mesh->mNumVertices)continue;auto p=bone->mOffsetMatrix*mesh->mVertices[weight.mVertexId];weights[weight.mVertexId].push_back({joint,weight.mWeight,{p.x,p.y,p.z}});}
    model->rigged=true;
   }
   for(unsigned f=0;f<mesh->mNumFaces;f++){auto& face=mesh->mFaces[f];if(face.mNumIndices!=3)continue;
    for(unsigned k=0;k<3;k++){unsigned v=face.mIndices[k];auto p=transform*mesh->mVertices[v];if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::runtime_error("Model contains invalid vertex positions");
     extent=std::max(extent,std::max({fabsf(p.x),fabsf(p.y),fabsf(p.z)}));
     SkinVertex out;out.position={p.x,p.y,p.z};out.weights=weights[v];
     if(mesh->HasVertexColors(0)){auto c=mesh->mColors[0][v];out.r=linear(c.r);out.g=linear(c.g);out.b=linear(c.b);out.a=linear(c.a);}
     if(mesh->HasTextureCoords(0)){out.u=mesh->mTextureCoords[0][v].x;out.v=1-mesh->mTextureCoords[0][v].y;}part.vertices.push_back(std::move(out));
    }
   }
   model->meshes.push_back(std::move(part));
  }
  for(unsigned i=0;i<node->mNumChildren;i++)visit(node->mChildren[i],transform);
 };visit(scene->mRootNode,aiMatrix4x4());
 if(!triangles)throw std::runtime_error("No triangles in this model");
 if(extent>2.f)throw std::runtime_error("Export in metres with the wrist/grip at the origin (model exceeds 2 m)");
 return model;
}
