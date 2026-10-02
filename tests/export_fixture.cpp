#include "formats/mesh.h"
#include <stdio.h>
#include <math.h>
static void pack(u8* p,int bone,int axis,u32 value) {
    int bit=(bone*3+axis)*12;
    for(int i=0;i<12;i++) if(value&(1u<<i))p[(bit+i)/8]|=(u8)(1u<<((bit+i)%8));
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    EmdModel m;m.valid=true;m.parts_count=3;
    m.parts_info=(EmdModel::PartInfo*)calloc(3,sizeof(EmdModel::PartInfo));
    m.parts_info[0].parent=-1;m.parts_info[0].bx=20;
    m.parts_info[1].parent=0;m.parts_info[1].by=100;
    m.parts_info[2].parent=1;m.parts_info[2].bx=50;
    m.pool_count=3;m.pool=(EmdModel::PoolVert*)calloc(3,sizeof(EmdModel::PoolVert));
    m.pool[0].x=10;m.pool[0].bone_idx=1;m.pool[1].y=10;m.pool[1].bone_idx=2;m.pool[2].z=10;m.pool[2].bone_idx=1;
    m.vert_src_count=3;m.vert_src=(EmdModel::VertSrc*)calloc(3,sizeof(EmdModel::VertSrc));
    for(int i=0;i<3;i++){m.vert_src[i].pool_idx=i;m.vert_src[i].dest_bone=m.pool[i].bone_idx;}
    m.vert_src[2].is_mirror=true;m.vert_src[2].dest_bone=2;
    m.mesh.alloc(3,1);for(int i=0;i<3;i++){m.mesh.tris[0].idx[i]=i;m.mesh.tris[0].uv[i][0]=i*64;}
    m.mesh.tris[0].color=RGBA8(128,128,128,255);
    m.clip_count=2;m.clips=(EmdAnimClip*)calloc(2,sizeof(EmdAnimClip));
    m.raw_dec_size=112;m.raw_dec=(u8*)calloc(m.raw_dec_size,1);
    m.clips[0].frames=2;m.clips[0].frame_size=28;
    m.clips[1].frames=2;m.clips[1].frame_size=28;m.clips[1].frame_data=56;
    pack(m.raw_dec+28+12,0,2,1024);pack(m.raw_dec+28+12,1,0,512);pack(m.raw_dec+28+12,2,1,1536);
    pack(m.raw_dec+84+12,1,2,3072);
    u8 pixels[]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,0};int slices[1040];memset(slices,0,sizeof(slices));
    EmdExportAtlas atlas={pixels,2,2,1,8,0,0,0,0,slices};
    if(!export_emd_glb(argv[1],m,&atlas))return 3;
    for(int c=0;c<2;c++)for(int f=0;f<2;f++) {
        if(!compute_anim_frame(m,c,f))return 4;
        for(int v=0;v<3;v++)printf("%.9g %.9g %.9g\n",m.mesh.verts[v].x,m.mesh.verts[v].y,m.mesh.verts[v].z);
    }
    if(!export_emd_glb("build/check/fixture_after_playback.glb",m,&atlas))return 7;
    int saved=m.clip_count;m.clip_count=0;
    if(!export_emd_glb("build/check/fixture_static.glb",m))return 8;
    m.clip_count=saved;
    // Bounds failures must be rejected before creating output.
    m.clips[1].frame_data=1000;if(export_emd_glb("invalid.glb",m))return 5;
    m.clips[1].frame_data=56;m.parts_info[0].parent=2;if(export_emd_glb("invalid.glb",m))return 6;
    return 0;
}
