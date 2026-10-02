"""Validate GLB layout, embedded PNG, and skinning against the viewer decoder.
Run from repository root after building/running export_fixture (see EXPORTING.md).
"""
import json, struct, zlib
from pathlib import Path
import numpy as np
p=Path('build/check/fixture.glb').read_bytes()
assert p==Path('build/check/fixture_after_playback.glb').read_bytes(), 'Export depends on current viewer frame'
magic,version,length=struct.unpack_from('<III',p)
assert (magic,version,length)==(0x46546c67,2,len(p))
jlen,jtype=struct.unpack_from('<II',p,12)
assert jtype==0x4e4f534a and jlen%4==0
g=json.loads(p[20:20+jlen]);off=20+jlen
blen,btype=struct.unpack_from('<II',p,off);binary=p[off+8:]
assert btype==0x004e4942 and blen==len(binary)==g['buffers'][0]['byteLength']
for v in g['bufferViews']:
    assert v['byteOffset']%4==0 and v['byteOffset']+v['byteLength']<=blen

def view(i):
    v=g['bufferViews'][i];return binary[v['byteOffset']:v['byteOffset']+v['byteLength']]
def acc(i):
    a=g['accessors'][i];cols={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']]
    dtype={5126:'<f4',5121:'u1'}[a['componentType']]
    x=np.frombuffer(view(a['bufferView']),dtype=dtype)
    assert x.size==a['count']*cols
    if a['type']=='MAT4':return x.reshape(-1,4,4).transpose(0,2,1)
    return x.reshape(-1,cols)
prim=g['meshes'][0]['primitives'][0]['attributes']
pos=acc(prim['POSITION']);joint=acc(prim['JOINTS_0'])[:,0]
weights=acc(prim['WEIGHTS_0']);np.testing.assert_array_equal(weights.sum(axis=1),1)
pa=g['accessors'][prim['POSITION']]
np.testing.assert_allclose(pa['min'],pos.min(axis=0));np.testing.assert_allclose(pa['max'],pos.max(axis=0))
ibm=acc(g['skins'][0]['inverseBindMatrices']);nb=len(ibm)
assert len(g['animations'])==2
reference=np.loadtxt('build/check/viewer_positions.txt').reshape(2,2,3,3)

def rot(q):
    x,y,z,w=q
    return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                     [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                     [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
for ci,anim in enumerate(g['animations']):
    assert len(anim['channels'])==nb
    for fi in range(2):
        local={}
        for ch in anim['channels']:
            sampler=anim['samplers'][ch['sampler']]
            assert sampler['interpolation']=='STEP'
            np.testing.assert_allclose(acc(sampler['input'])[:,0],[0,1/30],rtol=1e-6)
            q=acc(sampler['output'])[fi];np.testing.assert_allclose(np.linalg.norm(q),1,rtol=1e-6)
            node=ch['target']['node'];m=np.eye(4);m[:3,:3]=rot(q);m[:3,3]=g['nodes'][node]['translation'];local[node]=m
        world={0:np.eye(4)}
        for parent,node in enumerate(g['nodes']):
            for child in node.get('children',[]):
                if child in local:world[child]=world[parent]@local[child]
        actual=[]
        for vi,xyz in enumerate(pos):
            b=int(joint[vi]);node=g['skins'][0]['joints'][b]
            actual.append((world[node]@ibm[b]@np.r_[xyz,1])[:3])
        np.testing.assert_allclose(actual,reference[ci,fi],atol=0.0001)
png=view(g['images'][0]['bufferView']);assert png[:8]==b'\x89PNG\r\n\x1a\n'
i=8;idat=b''
while i<len(png):
    n=struct.unpack_from('>I',png,i)[0];tag=png[i+4:i+8];data=png[i+8:i+8+n]
    assert zlib.crc32(tag+data)==struct.unpack_from('>I',png,i+8+n)[0]
    if tag==b'IDAT':idat+=data
    i+=n+12
raw=zlib.decompress(idat);assert raw==bytes([0,255,0,0,255,0,255,0,255,0,0,0,255,255,255,255,255,0])
print('PASS: GLB structure, accessor bounds, PNG CRC/decompression, weights, two clips, mirrored bones, and frame-by-frame skinning match viewer.')
