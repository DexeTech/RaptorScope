/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  3D Mesh Formats (Room, Door, EMD Character)
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DC_FORMATS_MESH_H
#define DC_FORMATS_MESH_H

#include "core/types.h"
#include <stdlib.h>
#include <string.h>

/*─── Triangle face ──────────────────────────────────────────────*/
struct MeshTri {
    u32  idx[3];       /* vertex indices into parent vertex array */
    RGBA8 color;
    u16  tpage;
    u16  clut;         /* PS1 CLUT field (bits 0-5=X/16, bits 6-14=Y) */
    u8   uv[3][2];    /* (u, v) per vertex corner */
    u8   alt;          /* 1 = conditional variant geometry (SCD branch) */
    u32  src_off;      /* section header offset in RDT (0=unknown), for object picking */
};

/*─── Mesh vertex ────────────────────────────────────────────────*/
struct MeshVert {
    f32 x, y, z;
    f32 nx, ny, nz;    /* per-vertex normal (0,0,0 = not set) */
};

/*─── Generic mesh (shared by room, door, EMD) ───────────────────*/
struct Mesh {
    MeshVert* verts;
    int       vert_count;
    MeshTri*  tris;
    int       tri_count;

    Mesh() : verts(0), vert_count(0), tris(0), tri_count(0) {}
    ~Mesh() { free(verts); free(tris); }

    void alloc(int nv, int nt) {
        free(verts); free(tris);
        verts = (MeshVert*)calloc(nv, sizeof(MeshVert));
        tris  = (MeshTri*) calloc(nt, sizeof(MeshTri));
        vert_count = nv;
        tri_count  = nt;
    }

private:
    Mesh(const Mesh&);
    Mesh& operator=(const Mesh&);
};

/* Compute smooth normals by accumulating face normals at shared vertex
   positions.  Call after mesh is fully populated.  Skips vertices that
   already have a non-zero normal (e.g. from parsed EMD normal data). */
void mesh_compute_smooth_normals(Mesh& mesh);

/*─── Skeleton bone (EMD character models) ───────────────────────*/
struct EmdBone {
    s16 bx, by, bz;       /* local offset from parent */
    u8  parent;
    u8  flags;
    int vert_start;        /* first vertex in V_pool belonging to this bone */
    int vert_count;
    int tri_ptr_offset;    /* file offset to tri data */
    int quad_ptr_offset;   /* file offset to quad data */
    int n_tris;
    int n_quads;
};

struct EmdSkeleton {
    EmdBone bones[50];
    int     bone_count;
    f32     abs_pos[50][3];  /* world-space accumulated positions */
};

/*─── Animation clip (extracted from EMD data) ─────────────────*/
struct EmdAnimClip {
    int  offset;       /* offset in decompressed data of clip header */
    int  frame_data;   /* offset to first frame */
    int  frames;       /* number of frames in this clip */
    int  frame_size;   /* bytes per frame (from pointer table) */
};

/*─── EMD Character model ────────────────────────────────────────*/
struct EmdModel {
    Mesh       mesh;            /* expanded triangle soup */
    EmdSkeleton skeleton;
    int        n_anims;
    bool       valid;

    /* Per-vertex source tracking for animation */
    struct VertSrc {
        int  pool_idx;
        bool is_mirror;
        int  dest_bone;
    };
    VertSrc*  vert_src;
    int       vert_src_count;

    /* Raw vertex pool (local coords + bone idx) */
    struct PoolVert { s16 x, y, z; u16 bone_idx; };
    PoolVert* pool;
    int       pool_count;

    /* Animation clips */
    EmdAnimClip* clips;
    int          clip_count;

    /* Raw decompressed data (needed for animation playback) */
    u8*    raw_dec;
    size_t raw_dec_size;
    u32    base_addr;   /* PSX base address for pointer resolution */
    size_t norm_pool_off; /* offset of normal pool in raw_dec (targets[1]) */

    /* Part info for FK chain */
    struct PartInfo { s16 bx, by, bz; int parent; };
    PartInfo* parts_info;
    int       parts_count;

    EmdModel() : n_anims(0), valid(false), vert_src(0),
                 vert_src_count(0), pool(0), pool_count(0),
                 clips(0), clip_count(0), raw_dec(0), raw_dec_size(0),
                 base_addr(0), norm_pool_off(0), parts_info(0), parts_count(0) {}
    ~EmdModel() { free(vert_src); free(pool); free(clips);
                  free(raw_dec); free(parts_info); }
};

/*─── RDT Layout  (deterministic scene structure) ────────────────*/
#define RDT_MAX_SECTIONS  64
#define RDT_MAX_EMDS      16

struct RdtMeshSection {
    size_t offset;       /* file offset of 12-byte section header   */
    u16    tri_count;
    u16    quad_count;
};

struct RdtEmdEntry {
    size_t offset;       /* file offset of 24-byte EMD header       */
    int    vert_count;
    int    tri_count;
    int    quad_count;
    int    part_count;
    size_t data_end;     /* first byte after this EMD's quad data   */
};

struct RdtLayout {
    /* Header */
    u32    base;              /* PSX base address for this scene     */
    u32    hdr_ptrs[7];      /* 7 raw PSX pointer values            */

    /* Room mesh sections */
    RdtMeshSection sections[RDT_MAX_SECTIONS];
    int    section_count;
    int    total_room_tris;   /* sum of all section tri_count       */
    int    total_room_quads;  /* sum of all section quad_count      */
    size_t room_mesh_end;     /* first byte after last section      */

    /* EMD character models */
    RdtEmdEntry emds[RDT_MAX_EMDS];
    int    emd_count;
    size_t emds_end;          /* first byte after last EMD          */

    /* Validity */
    bool   valid;

    RdtLayout() : base(0), section_count(0), total_room_tris(0),
                  total_room_quads(0), room_mesh_end(0), emd_count(0),
                  emds_end(0), valid(false) {
        memset(hdr_ptrs, 0, sizeof(hdr_ptrs));
        memset(sections, 0, sizeof(sections));
        memset(emds, 0, sizeof(emds));
    }
};

/*─── Parsing functions ──────────────────────────────────────────*/

/* Walk decompressed RDT data deterministically:
   7-ptr header → mesh sections → EMD models.
   Fills layout.  Returns true if at least sections OR emds found.
   dec/dec_size = already-decompressed LZSS0 data.
   base = PSX base address. */
bool parse_rdt_layout(const u8* dec, size_t dec_size, u32 base,
                      RdtLayout& layout);

/* Convenience: decompress + parse layout in one call. */
bool parse_rdt_layout_from_entry(const u8* data, size_t size,
                                 u16 ey, u16 ex, RdtLayout& layout);

/* Parse room mesh.  Original API decompresses internally. */
bool parse_room_mesh(const u8* data, size_t size, u32 base_addr,
                     Mesh& mesh);

/* Parse room mesh from pre-decompressed buffer.
   Optional exclude_offsets: array of section file-offsets to skip
   (used to avoid rendering sections that only appear via non-zero xforms). */
bool parse_room_mesh_dec(const u8* dec, size_t dec_size, u32 base_addr,
                         Mesh& mesh,
                         const u32* exclude_offsets = 0, int n_exclude = 0);

/* Parse door mesh.  Original API decompresses internally. */
bool parse_door_mesh(const u8* data, size_t size, u32 base_addr,
                     Mesh& mesh);

/* Parse door mesh from pre-decompressed buffer. */
bool parse_door_mesh_dec(const u8* dec, size_t dec_size, u32 base_addr,
                         Mesh& mesh);

/* Parse RDT scene: room mesh + instanced sub-objects with positions. */
bool parse_rdt_scene(const u8* data, size_t size, u32 base_addr,
                     Mesh& mesh);

/* Parse RDT scene from pre-decompressed buffer.
   exclude_offsets: section offsets to skip (only rendered via xform instancing). */
bool parse_rdt_scene_dec(const u8* dec, size_t dec_size, u32 base_addr,
                         Mesh& mesh,
                         const u32* exclude_offsets = 0, int n_exclude = 0);

/* Check if a DatEntry contains a door mesh. */
bool is_door_mesh_entry(const u8* data, size_t size,
                        u16 ey, u16 ew, u16 eh);

/* Check if decompressed LZSS0 blob is a standalone character EMD
   (p/h file entry 3: skeleton + animation + mesh at end).
   Uses 48-byte tris / 64-byte quads with interleaved normals. */
bool is_standalone_emd_dec(const u8* dec, size_t dec_size, u32 base);

/* Parse standalone character EMD mesh from pre-decompressed buffer. */
bool parse_standalone_emd_mesh_dec(const u8* dec, size_t dec_size,
                                   u32 base, Mesh& mesh);

/* Check if a DatEntry contains an EMD character model. */
bool is_emd_entry(const u8* data, size_t size, u16 ey, u16 ex);

/* Find all EMD headers in a decompressed LZSS0 blob.
   Returns count of EMDs found (up to max_out).
   out_offsets[] receives the byte offset of each EMD header. */
int find_emd_headers(const u8* data, size_t size, u16 ey, u16 ex,
                     size_t* out_offsets, int max_out);

/* Check if decompressed LZSS0 data is MIPS R3000 code overlay. */
bool is_mips_code(const u8* data, size_t size, u16 ey, u16 ex);
bool is_weapon_data(const u8* data, size_t size, u16 ey, u16 ex);

/* Pre-decompressed variants of detection functions. */
bool is_door_mesh_entry_dec(const u8* dec, size_t dec_size, u16 ey);
bool is_weapon_data_dec(const u8* dec, size_t dec_size);
bool is_mips_code_dec(const u8* dec, size_t dec_size);

/* Check if decompressed LZSS0 data is an RDT scene package
   (room mesh + instanced objects + metadata). */
bool is_rdt_scene(const u8* data, size_t size, u16 ey, u16 ex);

/* Parse EMD character model.  Fills model with mesh + skeleton.
   If hint_offset >= 0, skip directly to that offset in decompressed data. */
bool parse_emd_model(const u8* entry_data, size_t entry_size,
                     u16 ey, u16 ex, EmdModel& model,
                     int hint_offset = -1);

/* Parse EMD from pre-decompressed buffer. */
bool parse_emd_model_dec(const u8* dec, size_t dec_size,
                         u16 ey, u16 ex, EmdModel& model,
                         int hint_offset = -1);

/* Extract animation clips from EMD entry and store in model. */
void extract_emd_anims(EmdModel& model);

/* Compute animated vertex positions for a given clip/frame.
   Updates model.mesh.verts in place.  Returns false if invalid. */
bool compute_anim_frame(EmdModel& model, int clip_idx, int frame_idx);

/*─── OBJ Export ─────────────────────────────────────────────────*/
bool export_mesh_obj(const char* path, const Mesh& mesh);
/* Shared, non-mutating pose decoder; same motion as viewer playback. */
bool decode_emd_pose(const EmdModel& model, int clip, int frame,
                     f32 positions[50][3], f32 matrices[50][9], f32 rotations[50][3]);
/* Optional viewer atlas; pixels are top-down RGBA. */
struct EmdExportAtlas {
    const u8* rgba;
    int width, height, slices, bpp, vram_x, vram_y, clut_x, clut_y;
    const int* slice_map;
};
bool export_emd_glb(const char* path, const EmdModel& model,
                    const EmdExportAtlas* atlas = 0, f32 fps = 30.0f);

bool export_emd_smd(const char* path, const EmdModel& emd);
bool export_mesh_smd(const char* path, const Mesh& mesh);

/*─── Bone name lookup (auto-detects skeleton type by bone count) ─*/
const char* get_bone_name(int bone_count, int bone_idx);

/*═══════════════════════════════════════════════════════════════════
 *  RDT Scene Overlay  –  non-mesh spatial data from room scripts
 *
 *  The Dino Crisis RDT blob contains 7 header pointers:
 *    ptr[0] = SCD main script table
 *    ptr[1] = Collision rectangles  (gs+336, 8-byte index + 48-byte rects)
 *    ptr[2] = Camera cut table      (gs+340, 16-byte groups + 20-byte zones)
 *    ptr[3] = Script thread table   (gs+356)
 *    ptr[4] = Door/transition zones (gs+488)
 *    ptr[5] = SCD bytecode base     (gs+616)
 *    ptr[6] = Floor zones           (gs+620, 20-byte quads)
 *
 *  The SCD bytecode contains:
 *    op 0x23 (32B) – room model registration with world-space transforms
 *    op 0x42 (20B) – dynamic object spawn with position
 *    op 0x20 (24B) – EMD character placement
 *    op 0x4C (32B) – camera eye/target setup
 *    op 0x28 (var) – collision zone setup
 *═══════════════════════════════════════════════════════════════════*/

/*─── Floor zone (trigger/transition quad) ──────────────────────*/
struct RdtFloorZone {
    s16 x[4], z[4];          /* 4 corner XZ coords (world-space) */
    s16 y;                    /* floor Y position */
    s16 height;               /* zone height above Y (0 = flat) */
    u8  collision_group;
    u8  target_index;         /* type for 0x28 zones */
    u8  flags;                /* 0x28 = SCD trigger zone */
    u8  pad;
};

/*─── Collision rectangle ───────────────────────────────────────*/
struct RdtCollisionRect {
    s16 x, z;                /* top-left corner (world XZ) */
    u16 w, h;                /* width, height */
    u8  group;               /* collision group index */
    u8  flags;
};

/*─── Camera cut entry ──────────────────────────────────────────*/
struct RdtCameraEntry {
    s16 eye_x, eye_y, eye_z;       /* camera eye position */
    s16 tgt_x, tgt_y, tgt_z;       /* camera target position */
    u16 fov;
    u16 flags;
};

/*─── Door / room transition ────────────────────────────────────*/
/* ptr[4] is a runtime pointer table filled by SCD opcode 0x28.
   Door zones come from 0x28 type 4, stored in the zones[] array. */

/*─── Camera cut trigger zone ──────────────────────────────────*/
/* From ptr[2] camera cut table — spatial region activating a camera.
   Format: type(u8), flags(u8), pad(u16), x(s16), z(s16), w(s16), h(s16) */
struct RdtCameraCutZone {
    u8  type;                 /* zone shape type */
    u8  cam_group;            /* which camera group this zone belongs to */
    s16 x, z;                 /* top-left corner */
    s16 w, h;                 /* width, height (XZ plane) */
};

/*─── SCD opcode 0x23: section transform ────────────────────────*/
struct RdtSectionXform {
    u32 section_off;          /* file offset of inline section header */
    u8  slot;
    u8  flags;                /* byte[2] */
    u8  render_mode;          /* byte[3]: 0=opaque, 1=semi-trans, 2=fixed-depth, 3=mirror */
    u8  pad;
    u16 ot_depth;             /* bytes[4..5]: OT depth for mode 2/3 (w74) */
    s16 px, py, pz;
    s16 rx, ry, rz;
};

/* Apply SCD 0x23 section transforms (instancing) to a parsed room mesh. */
void mesh_apply_xforms(Mesh& mesh, const u8* dec, size_t dec_size, u32 base,
                       const RdtSectionXform* xforms, int n_xforms);

/*─── SCD opcode 0x42: object spawn ─────────────────────────────*/
struct RdtObjectSpawn {
    u8  slot;
    u8  mesh_type;
    u8  mesh_ref;
    u8  flags;
    s16 px, py, pz;
    s16 rot_y;
};

/*─── SCD opcode 0x5B: item/pickup spawn (44B) ─────────────────*/
struct RdtItemSpawn {
    u8  slot;
    u8  type;
    u8  anim_set;
    s16 px, py, pz;
    s16 rot_y;
    u32 mesh_ptr;
    u32 anim_ptr;
};

/*─── SCD opcode 0x20: EMD character placement ──────────────────*/
struct RdtCharPlace {
    u8  slot;
    u8  flags;
    u8  type;
    u8  anim_set;
    s16 px, py, pz;
    s16 rot_y;
    u32 mesh_ptr;             /* PSX address of EMD data */
    u32 anim_ptr;
};

/*─── SCD opcode 0x2E: examine/interact zone (20B) ─────────────*/
struct RdtExamineZone {
    s16 x[4], z[4];          /* 4 corner XZ coords (world-space) */
    u8  mask;                 /* active zone bitmask */
    u8  pad;
};

/*─── SCD opcode 0x3A: scene light source (12B) ────────────────*/
struct RdtSceneLight {
    u8  slot;
    s16 px, py, pz;           /* world-space position */
    s16 radius;               /* light falloff radius */
    u8  r, g, b;              /* light color (0-255) */
    u8  has_pos;              /* set if position was parsed */
    u8  has_color;            /* set if color was parsed */
};

/*─── SCD opcode 0x42: enemy/entity spawn (20B) ───────────────*/
struct RdtEnemySpawn {
    u8  type;                 /* entity type ID */
    u8  id;                   /* entity slot ID */
    u8  flags;
    s16 px, py, pz;           /* world-space position */
    s16 rot_y;
};

/*─── SCD opcode 0x3D: fog/atmosphere effect (12B) ────────────*/
struct RdtFogParams {
    u8  type;                 /* effect type */
    s16 dist_near, dist_far;  /* fog range */
    u8  r, g, b;              /* fog color */
};

/*─── SCD opcode 0x3C: dynamic light color (8B) ──────────────*/
struct RdtLightColor {
    u8  light_idx;            /* which light slot */
    u8  component;            /* 0=intensity, 1=on/off, 2=color */
    u16 value;                /* intensity/color value */
};

/*─── Complete RDT scene overlay ────────────────────────────────*/
struct RdtSceneOverlay {
    RdtFloorZone      zones[64];       int n_zones;
    RdtCollisionRect  collisions[256]; int n_collisions;
    RdtCameraEntry    cameras[32];     int n_cameras;
    RdtSectionXform   xforms[64];      int n_xforms;
    RdtObjectSpawn    spawns[64];      int n_spawns;
    RdtCharPlace      chars[16];       int n_chars;
    RdtCameraCutZone  camcuts[128];    int n_camcuts;
    RdtExamineZone    examines[32];    int n_examines;
    RdtSceneLight     lights[8];       int n_lights;
    u8  ambient_r, ambient_g, ambient_b;  /* room ambient from ptr[0]+8 */
    RdtItemSpawn      items[32];       int n_items;
    RdtEnemySpawn     enemies[32];     int n_enemies;
    RdtFogParams      fog[8];          int n_fog;
    RdtLightColor     light_cols[64];  int n_light_cols;

    /* Camera thread activation zones from ptr[3].
       Each thread has a 4-corner XZ quad at +4..+19 defining where
       the camera angle activates when the player enters. */
    struct CamThread {
        s16 x[4], z[4];      /* activation quad corners */
        u8  type;             /* thread type (byte[0]) */
        u8  cam_id;           /* camera ID (byte[2]) */
    };
    CamThread  cam_threads[32]; int n_cam_threads;

    f32 floor_y;  /* rendered Y of the floor plane (from mesh data) */

    RdtSceneOverlay() { memset(this, 0, sizeof(*this)); }
};

/*─── Parse RDT scene overlay from decompressed blob ────────────*/
bool parse_rdt_overlay(const u8* dec, size_t dec_size, u32 base_addr,
                       RdtSceneOverlay& overlay);

#endif /* DC_FORMATS_MESH_H */
