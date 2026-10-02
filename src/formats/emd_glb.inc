/* GLB 2.0 export. Included by mesh.cpp to share its rotation convention.
   No dependency on Win32/OpenGL or third-party libraries. */
#include <vector>
#include <string>
#include <sstream>
#include <locale>
#include <limits>

namespace dcglb {
typedef std::vector<u8> Bytes;
static void word(Bytes& b, u32 v) { for (int i=0;i<4;i++) b.push_back((u8)(v>>(8*i))); }
static void real(Bytes& b, f32 v) { u32 u; memcpy(&u,&v,4); word(b,u); }
static void align(Bytes& b, u8 c=0) { while (b.size()%4) b.push_back(c); }
static u32 crc(const u8* p,size_t n) {
    u32 c=~0u; for(size_t i=0;i<n;i++) { c^=p[i]; for(int k=0;k<8;k++) c=(c>>1)^((c&1)?0xedb88320u:0); } return ~c;
}
static void be(Bytes& b,u32 v) { for(int i=3;i>=0;i--) b.push_back((u8)(v>>(8*i))); }
static void chunk(Bytes& b,const char* tag,const Bytes& data) {
    be(b,(u32)data.size()); size_t start=b.size();
    b.insert(b.end(),tag,tag+4); b.insert(b.end(),data.begin(),data.end());
    be(b,crc(&b[start],data.size()+4));
}
static Bytes png(const EmdExportAtlas& a) {
    Bytes raw;
    for(int y=0;y<a.height;y++) { raw.push_back(0); const u8* p=a.rgba+(size_t)y*a.width*4; raw.insert(raw.end(),p,p+a.width*4); }
    Bytes z; z.push_back(0x78); z.push_back(0x01);
    for(size_t off=0;off<raw.size();) {
        size_t n=raw.size()-off; if(n>65535) n=65535;
        z.push_back(off+n==raw.size()?1:0);
        z.push_back((u8)n); z.push_back((u8)(n>>8));
        u16 inv=(u16)~n; z.push_back((u8)inv); z.push_back((u8)(inv>>8));
        z.insert(z.end(),raw.begin()+off,raw.begin()+off+n); off+=n;
    }
    u32 s1=1,s2=0; for(size_t i=0;i<raw.size();i++) {s1=(s1+raw[i])%65521; s2=(s2+s1)%65521;} be(z,(s2<<16)|s1);
    const u8 sig[]={137,80,78,71,13,10,26,10}; Bytes out(sig,sig+8), h;
    be(h,a.width); be(h,a.height); h.push_back(8); h.push_back(6); h.push_back(0); h.push_back(0); h.push_back(0);
    chunk(out,"IHDR",h); chunk(out,"IDAT",z); Bytes empty; chunk(out,"IEND",empty); return out;
}
struct Writer {
    Bytes bin; std::vector<std::string> views, access;
    int view(const Bytes& b, int target=0) {
        align(bin); std::ostringstream s; s.imbue(std::locale::classic());
        s<<"{\"buffer\":0,\"byteOffset\":"<<bin.size()<<",\"byteLength\":"<<b.size();
        if(target)s<<",\"target\":"<<target; s<<"}";
        views.push_back(s.str()); bin.insert(bin.end(),b.begin(),b.end()); return (int)views.size()-1;
    }
    int acc(const Bytes& b,int component,const char* type,int count,const std::string& extra="", int target=0) {
        int v=view(b,target); std::ostringstream s; s.imbue(std::locale::classic());
        s<<"{\"bufferView\":"<<v<<",\"componentType\":"<<component<<",\"count\":"<<count<<",\"type\":\""<<type<<"\""<<extra<<"}";
        access.push_back(s.str()); return (int)access.size()-1;
    }
};
static void list(std::ostream& s,const std::vector<std::string>& v) {
    s<<'['; for(size_t i=0;i<v.size();i++) {if(i)s<<','; s<<v[i];} s<<']';
}
static void quat(const f32* m, f32 q[4]) {
    f32 tr=m[0]+m[4]+m[8];
    if(tr>0) { f32 s=sqrtf(tr+1)*2; q[3]=s/4; q[0]=(m[7]-m[5])/s; q[1]=(m[2]-m[6])/s; q[2]=(m[3]-m[1])/s; }
    else { int i=0; if(m[4]>m[0])i=1; if(m[8]>m[i*3+i])i=2; int j=(i+1)%3,k=(i+2)%3;
        f32 s=sqrtf(1+m[i*3+i]-m[j*3+j]-m[k*3+k])*2;
        q[i]=s/4; q[j]=(m[j*3+i]+m[i*3+j])/s; q[k]=(m[k*3+i]+m[i*3+k])/s; q[3]=(m[k*3+j]-m[j*3+k])/s;
    }
    f32 n=sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]); for(int i=0;i<4;i++)q[i]/=n;
}
}

bool export_emd_glb(const char* path,const EmdModel& m,const EmdExportAtlas* atlas,f32 fps) {
    using namespace dcglb;
    if(!path || !m.valid || !m.parts_info || !m.pool || !m.vert_src || !m.mesh.tris ||
       m.parts_count<1 || m.parts_count>50 || m.mesh.tri_count<1 || !(fps>0 && fps<=1000) ||
       m.clip_count<0 || (m.clip_count>0 && !m.clips)) return false;
    if(atlas && (!atlas->rgba || atlas->width<1 || atlas->height<1 || atlas->width>16384 || atlas->height>16384 ||
                 atlas->slices<1 || atlas->height%atlas->slices || !atlas->slice_map)) return false;
    int nb=m.parts_count; f32 bind[50][3];
    for(int b=0;b<nb;b++) {
        int p=m.parts_info[b].parent; if(p < -1 || p>=b)return false;
        const EmdModel::PartInfo& v=m.parts_info[b];
        bind[b][0]=-v.bx; bind[b][1]=-v.by; bind[b][2]=-v.bz;
        if(p>=0) for(int a=0;a<3;a++)bind[b][a]+=bind[p][a];
    }
    Writer w; Bytes pos, norm, uv, joints, weights, colors, ibm;
    f32 lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};
    for(int t=0;t<m.mesh.tri_count;t++) {
        const MeshTri& tri=m.mesh.tris[t]; f32 p[3][3]; int bone[3];
        for(int c=0;c<3;c++) {
            u32 vi=tri.idx[c]; if(vi>=(u32)m.vert_src_count)return false;
            const EmdModel::VertSrc& vs=m.vert_src[vi]; if(vs.pool_idx<0 || vs.pool_idx>=m.pool_count)return false;
            const EmdModel::PoolVert& pv=m.pool[vs.pool_idx];
            bone[c]=vs.is_mirror?vs.dest_bone:pv.bone_idx; if(bone[c]<0 || bone[c]>=nb)return false;
            p[c][0]=bind[bone[c]][0]-(vs.is_mirror?-pv.x:pv.x);
            p[c][1]=bind[bone[c]][1]-pv.y; p[c][2]=bind[bone[c]][2]-pv.z;
        }
        f32 a[3],b[3],n[3]; for(int k=0;k<3;k++){a[k]=p[1][k]-p[0][k];b[k]=p[2][k]-p[0][k];}
        n[0]=a[1]*b[2]-a[2]*b[1]; n[1]=a[2]*b[0]-a[0]*b[2]; n[2]=a[0]*b[1]-a[1]*b[0];
        f32 len=sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]); if(len<1e-8f){n[0]=0;n[1]=1;n[2]=0;len=1;}
        for(int c=0;c<3;c++) {
            for(int k=0;k<3;k++){ real(pos,p[c][k]); if(p[c][k]<lo[k])lo[k]=p[c][k]; if(p[c][k]>hi[k])hi[k]=p[c][k]; real(norm,n[k]/len); }
            f32 u=tri.uv[c][0]/256.0f,v=tri.uv[c][1]/256.0f;
            if(atlas) {
                const EmdExportAtlas& at=*atlas; int tp=(tri.tpage>>7)&3, ppw=at.bpp==4?4:at.bpp==16?1:2;
                int fppw=tp==0?4:tp==2?1:2, slice=0;
                if(at.slices>1){int row=((tri.clut>>6)&511)-at.clut_y; if(row<0)row=0;if(row>15)row=15;
                    int slot=0; if(tp==0){int rel=((tri.clut&63)*16-at.clut_x)/16;if(rel<0)rel=0;if(rel>63)rel=63;slot=rel+1;}
                    slice=at.slice_map[row*65+slot];if(slice<0)slice=at.slice_map[row*65];if(slice<0)slice=0;
                }
                if(slice>=at.slices)return false;
                u=(((tri.tpage&15)*64-at.vram_x)*ppw+(f32)tri.uv[c][0]*ppw/fppw)/at.width;
                v=(((tri.tpage>>4)&1)*256-at.vram_y+tri.uv[c][1]+slice*(at.height/at.slices))/(f32)at.height;
            }
            real(uv,u);real(uv,v);
            joints.push_back((u8)bone[c]); for(int k=0;k<3;k++)joints.push_back(0);
            real(weights,1);for(int k=0;k<3;k++)real(weights,0);
            const u8 rgb[]={tri.color.r,tri.color.g,tri.color.b};for(int k=0;k<3;k++)real(colors,(rgb[k]<128?rgb[k]*2:255)/255.0f);real(colors,1);
        }
    }
    std::ostringstream bounds; bounds.imbue(std::locale::classic()); bounds.precision(9);
    bounds<<",\"min\":["<<lo[0]<<','<<lo[1]<<','<<lo[2]<<"],\"max\":["<<hi[0]<<','<<hi[1]<<','<<hi[2]<<']';
    int nv=m.mesh.tri_count*3;
    int pa=w.acc(pos,5126,"VEC3",nv,bounds.str(),34962),na=w.acc(norm,5126,"VEC3",nv,"",34962),ua=w.acc(uv,5126,"VEC2",nv,"",34962);
    int ja=w.acc(joints,5121,"VEC4",nv,"",34962),wa=w.acc(weights,5126,"VEC4",nv,"",34962),ca=w.acc(colors,5126,"VEC4",nv,"",34962);
    for(int b=0;b<nb;b++)for(int col=0;col<4;col++)for(int row=0;row<4;row++)real(ibm,col==3&&row<3?-bind[b][row]:(col==row?1.0f:0.0f));
    int ia=w.acc(ibm,5126,"MAT4",nb);
    std::vector<std::string> anims;
    for(int ci=0;ci<m.clip_count;ci++) {
        const EmdAnimClip& clip=m.clips[ci]; if(clip.frames<1 || clip.frame_data<0 || clip.frame_size<12)return false;
        Bytes times; for(int f=0;f<clip.frames;f++)real(times,f/fps);
        std::ostringstream tb;tb.imbue(std::locale::classic());tb.precision(9);tb<<",\"min\":[0],\"max\":["<<(clip.frames-1)/fps<<']';
        int ta=w.acc(times,5126,"SCALAR",clip.frames,tb.str());
        std::vector<Bytes> qs(nb);f32 prev[50][4];memset(prev,0,sizeof(prev));
        for(int f=0;f<clip.frames;f++) {
            f32 bp[50][3],bm[50][9],br[50][3];if(!decode_emd_pose(m,ci,f,bp,bm,br))return false;
            for(int b=0;b<nb;b++){f32 mat[9],q[4];build_rot_matrix(br[b][0],br[b][1],br[b][2],mat);quat(mat,q);
                f32 dot=0;for(int k=0;k<4;k++)dot+=prev[b][k]*q[k];
                for(int k=0;k<4;k++){if(dot<0)q[k]=-q[k];real(qs[b],q[k]);prev[b][k]=q[k];}
            }
        }
        std::ostringstream s;s.imbue(std::locale::classic());s<<"{\"name\":\"Anim_"<<ci<<"\",\"samplers\":[";
        for(int b=0;b<nb;b++){int qa=w.acc(qs[b],5126,"VEC4",clip.frames);if(b)s<<',';s<<"{\"input\":"<<ta<<",\"output\":"<<qa<<",\"interpolation\":\"STEP\"}";}
        s<<"],\"channels\":[";for(int b=0;b<nb;b++){if(b)s<<',';s<<"{\"sampler\":"<<b<<",\"target\":{\"node\":"<<b+1<<",\"path\":\"rotation\"}}";}s<<"]}";anims.push_back(s.str());
    }
    int image=-1;if(atlas)image=w.view(png(*atlas));align(w.bin);
    std::ostringstream s;s.imbue(std::locale::classic());s.precision(9);
    s<<"{\"asset\":{\"version\":\"2.0\",\"generator\":\"RaptorScope DC1 animation exporter\"},\"scene\":0,\"scenes\":[{\"nodes\":[0,"<<nb+1<<"]}],\"nodes\":[{\"name\":\"DC1_Model\",\"children\":[";
    bool first=true;for(int b=0;b<nb;b++)if(m.parts_info[b].parent<0){if(!first)s<<',';s<<b+1;first=false;}s<<"]}";
    for(int b=0;b<nb;b++){const EmdModel::PartInfo& v=m.parts_info[b];s<<",{\"name\":\"bone_"<<b<<"\",\"translation\":["<<-v.bx<<','<<-v.by<<','<<-v.bz<<"]";first=true;
        for(int k=b+1;k<nb;k++)if(m.parts_info[k].parent==b){if(first)s<<",\"children\":[";else s<<',';s<<k+1;first=false;}if(!first)s<<']';s<<"}";}
    s<<",{\"name\":\"Mesh\",\"mesh\":0,\"skin\":0}],\"skins\":[{\"inverseBindMatrices\":"<<ia<<",\"joints\":[";
    for(int b=0;b<nb;b++){if(b)s<<',';s<<b+1;}s<<"]}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":"<<pa<<",\"NORMAL\":"<<na<<",\"TEXCOORD_0\":"<<ua<<",\"JOINTS_0\":"<<ja<<",\"WEIGHTS_0\":"<<wa<<",\"COLOR_0\":"<<ca<<"},\"material\":0}]}],\"materials\":[{\"doubleSided\":true,\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":1";
    if(atlas)s<<",\"baseColorTexture\":{\"index\":0}";s<<"},\"alphaMode\":\"MASK\",\"alphaCutoff\":0.5}]";
    if(atlas)s<<",\"images\":[{\"bufferView\":"<<image<<",\"mimeType\":\"image/png\"}],\"samplers\":[{\"magFilter\":9728,\"minFilter\":9728,\"wrapS\":33071,\"wrapT\":33071}],\"textures\":[{\"sampler\":0,\"source\":0}]";
    if(!anims.empty()){s<<",\"animations\":";list(s,anims);}s<<",\"bufferViews\":";list(s,w.views);s<<",\"accessors\":";list(s,w.access);
    s<<",\"buffers\":[{\"byteLength\":"<<w.bin.size()<<"}],\"extras\":{\"animationFps\":"<<fps<<",\"motionSource\":\"RaptorScope heuristic clip decoder; entity velocity metadata not exported\"}}";
    std::string json=s.str();while(json.size()%4)json+=' ';
    if(json.size()+w.bin.size()+28>0xffffffffu)return false;
    Bytes out;word(out,0x46546c67);word(out,2);word(out,(u32)(json.size()+w.bin.size()+28));
    word(out,(u32)json.size());word(out,0x4e4f534a);out.insert(out.end(),json.begin(),json.end());
    word(out,(u32)w.bin.size());word(out,0x004e4942);out.insert(out.end(),w.bin.begin(),w.bin.end());
    FILE* f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(&out[0],1,out.size(),f)==out.size();if(fclose(f)!=0)ok=false;return ok;
}
