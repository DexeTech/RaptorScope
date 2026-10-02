/* Portable command-line companion. Textures are exported by the GUI atlas path. */
#include "formats/dat.h"
#include "formats/mesh.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char** argv) {
    if(argc<2){fprintf(stderr,"Usage: dc1_export file.DAT [entry-index output.glb [fps [emd-offset]]]\nNo entry-index: list model candidates. GUI GLB export also embeds textures.\n");return 2;}
    DatArchive a;if(!dat_parse_file(argv[1],a)){fprintf(stderr,"Cannot parse DAT.\n");return 1;}
    if(argc==2){for(int i=0;i<a.count;i++){const DatEntry& e=a.entries[i];if(e.type!=DAT_LZSS0)continue;EmdModel m;if(parse_emd_model(e.data,e.size,e.y,e.x,m))printf("Entry %d: %d bones, %d triangles, %d detected clips\n",i,m.parts_count,m.mesh.tri_count,m.clip_count);}return 0;}
    if(argc<4)return 2;int i=atoi(argv[2]);if(i<0 || i>=a.count){fprintf(stderr,"Invalid entry index.\n");return 2;}
    const DatEntry& e=a.entries[i];EmdModel m;
    if(!parse_emd_model(e.data,e.size,e.y,e.x,m,argc>5?(int)strtol(argv[5],0,0):-1)){fprintf(stderr,"Model not recognized.\n");return 1;}
    if(!export_emd_glb(argv[3],m,0,argc>4?(f32)atof(argv[4]):30.0f)){fprintf(stderr,"Export failed.\n");return 1;}
    printf("Exported rig and %d clips (no texture atlas in CLI).\n",m.clip_count);return 0;
}
