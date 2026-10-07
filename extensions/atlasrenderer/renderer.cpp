#include "renderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace atlas {
namespace {
constexpr double INF = 1e100, EPS = 1e-7;
struct Reader {
    const std::string &data; size_t pos = 0;
    void need(size_t n) { if(n > data.size() - pos) throw std::invalid_argument("Truncated ABW1 snapshot"); }
    uint8_t u8() { need(1); return uint8_t(data[pos++]); }
    uint16_t u16() { uint16_t a = u8(); return a | uint16_t(u8()) << 8; }
    uint32_t u32() { uint32_t a = u16(); return a | uint32_t(u16()) << 16; }
    int32_t i32() { uint32_t v = u32(); int32_t n; std::memcpy(&n, &v, 4); return n; }
    float f32() { uint32_t v = u32(); float f; std::memcpy(&f, &v, 4); if(!std::isfinite(f)) throw std::invalid_argument("Non-finite model coordinate"); return f; }
    std::string bytes(size_t n) { need(n); auto s = data.substr(pos,n); pos += n; return s; }
};
void put_i32(std::string &out, size_t off, int32_t value) {
    uint32_t v; std::memcpy(&v,&value,4);
    for(int i=0;i<4;++i) out[off+i] = char((v >> (i*8)) & 255);
}
struct Vec { double x,y,z; double operator[](int a) const { return a==0?x:(a==1?y:z); } };
Vec add(Vec a,Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec mul(Vec a,double k) { return {a.x*k,a.y*k,a.z*k}; }
struct Basis { Vec right,up,dir; };
Basis basis(const Camera &c) {
    double a=c.azimuth*std::acos(-1)/180, i=c.inclination*std::acos(-1)/180;
    return {{std::cos(a),0,-std::sin(a)}, {-std::sin(a)*std::sin(i),std::cos(i),-std::cos(a)*std::sin(i)}, {std::sin(a)*std::cos(i),-std::sin(i),std::cos(a)*std::cos(i)}};
}
void validate(const Camera &c) {
    if(c.width < 1 || c.width > 1024 || c.height < 1 || c.height > 1024 || int64_t(c.width)*c.height > 524288)
        throw std::invalid_argument("Invalid viewport size");
    for(double d:{c.x,c.y,c.z,c.scale,c.azimuth,c.inclination}) if(!std::isfinite(d)) throw std::invalid_argument("Non-finite camera");
    if(std::abs(c.x)>30000000 || std::abs(c.z)>30000000 || std::abs(c.y)>4096 || c.scale<0.5 || c.scale>32 || c.inclination<20 || c.inclination>85 || c.max_steps<64 || c.max_steps>8192)
        throw std::invalid_argument("Camera outside supported range");
}
struct Box { std::array<double,6> v; };
struct Model { std::array<uint8_t,4> rgba; uint8_t tint,kind; std::array<uint16_t,3> tex; std::vector<Box> boxes; };
struct Key { int x,y,z; bool operator==(const Key&o)const{return x==o.x&&y==o.y&&z==o.z;} };
struct KeyHash { size_t operator()(const Key &k)const {
    uint64_t h=uint32_t(k.x)*UINT64_C(0x9e3779b1); h^=uint32_t(k.z)*UINT64_C(0x85ebca77); h^=uint32_t(k.y)*UINT64_C(0xc2b2ae3d); return size_t(h^(h>>32));
}};
struct Palette {
    uint8_t bits=0; std::vector<uint32_t> values,words;
    uint32_t at(int x,int y,int z)const {
        if(bits==0) return values[0];
        unsigned index=((x&15)<<8)|((z&15)<<4)|(y&15), per=32/bits;
        unsigned p=(words[index/per] >> ((index%per)*bits)) & ((1u<<bits)-1);
        return values[p];
    }
};
Palette read_palette(Reader &r, bool models, size_t model_count) {
    Palette p; p.bits=r.u8();
    if(p.bits!=0&&p.bits!=1&&p.bits!=2&&p.bits!=3&&p.bits!=4&&p.bits!=5&&p.bits!=6&&p.bits!=8&&p.bits!=16) throw std::invalid_argument("Invalid palette bits");
    unsigned count=r.u16();
    if(count==0 || count>4096 || (p.bits==0 && count!=1) || (p.bits>0 && count>(1u<<p.bits))) throw std::invalid_argument("Invalid palette size");
    p.values.reserve(count);
    for(unsigned n=0;n<count;++n) { auto v=r.u32(); if(models&&v>=model_count) throw std::invalid_argument("Unknown model index"); p.values.push_back(v); }
    auto len=r.u32(); unsigned expected=p.bits==0?0:((4096+(32/p.bits)-1)/(32/p.bits))*4;
    if(len!=expected) throw std::invalid_argument("Invalid palette word length");
    for(unsigned n=0;n<len/4;++n) p.words.push_back(r.u32());
    if(p.bits) for(unsigned index=0;index<4096;++index) {
        auto value=(p.words[index/(32/p.bits)]>>((index%(32/p.bits))*p.bits))&((1u<<p.bits)-1);
        if(value>=count) throw std::invalid_argument("Palette index out of range");
    }
    return p;
}
struct Section { std::vector<Palette> layers; Palette biome; };
struct World {
    std::vector<Model> models; std::vector<std::string> textures;
    std::unordered_map<Key,Section,KeyHash> sections;
    std::unordered_map<Key,uint8_t,KeyHash> chunks;
    int minx=INT32_MAX,minz=INT32_MAX,maxx=INT32_MIN,maxz=INT32_MIN,miny=-64,maxy=320;
    explicit World(const std::string &blob) {
        if(blob.size()>64*1024*1024) throw std::invalid_argument("Snapshot exceeds 64 MiB");
        Reader r{blob}; if(r.bytes(4)!="ABW1") throw std::invalid_argument("Expected ABW1 snapshot");
        miny=r.i32(); maxy=r.i32(); if(miny < -4096 || maxy>4096 || miny>=maxy) throw std::invalid_argument("Invalid world height");
        unsigned mc=r.u32(); if(mc==0||mc>65536) throw std::invalid_argument("Invalid model count"); models.reserve(mc);
        for(unsigned n=0;n<mc;++n) {
            Model m; for(auto &v:m.rgba)v=r.u8(); uint8_t flags=r.u8(); m.tint=flags&3; m.kind=flags>>2; if(m.kind>1) throw std::invalid_argument("Invalid model flags");
            unsigned bc=r.u8(); if(bc>16)throw std::invalid_argument("Too many model boxes"); for(auto &v:m.tex)v=r.u16();
            for(unsigned j=0;j<bc;++j) { Box b; for(auto &v:b.v)v=r.f32(); for(int a=0;a<3;++a) if(b.v[a]<0 || b.v[a+3]>1 || b.v[a]>=b.v[a+3])throw std::invalid_argument("Invalid model box"); m.boxes.push_back(b); }
            if(m.kind==1 && m.boxes.size()!=9) throw std::invalid_argument("Fence model needs nine boxes");
            models.push_back(std::move(m));
        }
        unsigned tc=r.u16(); if(tc>4096)throw std::invalid_argument("Too many textures");
        for(unsigned n=0;n<tc;++n)textures.push_back(r.bytes(1024));
        for(const auto&m:models)for(auto id:m.tex)if(id!=65535&&id>=tc)throw std::invalid_argument("Unknown texture");
        unsigned cc=r.u32(); if(cc>4096)throw std::invalid_argument("Too many chunks");
        for(unsigned n=0;n<cc;++n) {
            int cx=r.i32(),cz=r.i32(); if(std::abs(int64_t(cx))>1875000||std::abs(int64_t(cz))>1875000)throw std::invalid_argument("Chunk coordinate outside supported range");
            uint8_t preview=r.u8(); if(preview>1)throw std::invalid_argument("Invalid preview flag");
            if(!chunks.emplace(Key{cx,0,cz},preview).second)throw std::invalid_argument("Duplicate chunk");
            minx=std::min(minx,cx*16);maxx=std::max(maxx,cx*16+16); minz=std::min(minz,cz*16);maxz=std::max(maxz,cz*16+16);
            unsigned sc=r.u16(); if(sc>512)throw std::invalid_argument("Too many sections");
            for(unsigned j=0;j<sc;++j) {
                int sy=r.i32(); if(sy*int64_t(16)<miny || sy*int64_t(16)>=maxy)throw std::invalid_argument("Section outside world");
                unsigned lc=r.u8(); if(lc==0||lc>2)throw std::invalid_argument("Invalid layer count"); Section s;
                for(unsigned k=0;k<lc;++k)s.layers.push_back(read_palette(r,true,mc));
                s.biome=read_palette(r,false,0);
                if(!sections.emplace(Key{cx,sy,cz},std::move(s)).second)throw std::invalid_argument("Duplicate section");
            }
        }
        if(r.pos!=blob.size())throw std::invalid_argument("Unexpected trailing snapshot bytes");
    }
};
int floor16(int n) { return int(std::floor(n/16.0)); }
bool intersect(Vec o,Vec d,const Box&b,double &near,double &far,int &face) {
    near=-INF; far=INF;face=0;
    for(int a=0;a<3;++a) {
        if(std::abs(d[a])<EPS) { if(o[a]<b.v[a]||o[a]>b.v[a+3])return false; continue; }
        double t0=(b.v[a]-o[a])/d[a],t1=(b.v[a+3]-o[a])/d[a]; int f=a*2;
        if(t0>t1){std::swap(t0,t1);f++;}
        if(t0>near){near=t0;face=f;}far=std::min(far,t1);if(far<near)return false;
    }
    return far>=std::max(0.0,near);
}
uint8_t byte(double v) { return uint8_t(std::clamp(std::lround(v),0l,255l)); }
}

std::vector<double> project(const Camera &c,double x,double y,double z) {
    validate(c);auto b=basis(c); Vec d{x-c.x,y-c.y,z-c.z};
    return {c.width*0.5+(d.x*b.right.x+d.z*b.right.z)*c.scale, c.height*0.5-(d.x*b.up.x+d.y*b.up.y+d.z*b.up.z)*c.scale};
}

Result render(const std::string &snapshot,const Camera &c) {
    validate(c); World w(snapshot); auto b=basis(c); Result out;
    size_t pixels=size_t(c.width)*c.height; out.rgb.resize(pixels*3);out.hits.resize(pixels*12);out.preview.resize(pixels);
    Box bounds{{double(w.minx),double(w.miny),double(w.minz),double(w.maxx),double(w.maxy),double(w.maxz)}};
    double distance=std::max(8192.0,std::abs(c.y)+std::abs(w.maxy)+std::abs(w.miny)+c.width/c.scale+c.height/c.scale);
    for(int py=0;py<c.height;++py)for(int px=0;px<c.width;++px) {
        size_t p=size_t(py)*c.width+px; for(int a=0;a<3;++a)put_i32(out.hits,p*12+a*4,INT32_MIN);
        double sx=(px+0.5-c.width*0.5)/c.scale,sy=(c.height*0.5-py-0.5)/c.scale;
        Vec o=add(add(add(Vec{c.x,c.y,c.z},mul(b.right,sx)),mul(b.up,sy)),mul(b.dir,-distance));
        double start,end;int face; double color[3]={0,0,0},remain=1; bool unknown=false,any_preview=false,has_hit=false;
        if(!w.chunks.empty()&&intersect(o,b.dir,bounds,start,end,face)) {
            double t=std::max(0.0,start)+EPS; int steps=0; uint32_t last_transparent=UINT32_MAX;
            while(t<end&&steps++<c.max_steps&&remain>0.015) {
                ++out.traced_voxels; Vec q=add(o,mul(b.dir,t));int x=int(std::floor(q.x)),y=int(std::floor(q.y)),z=int(std::floor(q.z));
                int cx=floor16(x),cz=floor16(z),sY=floor16(y);
                auto known=w.chunks.find(Key{cx,0,cz}); if(known==w.chunks.end())unknown=true;
                auto section=w.sections.find(Key{cx,sY,cz});
                // Empty sections and unknown chunks are skipped in 16-block steps.
                int unit=section==w.sections.end()?16:1;
                double next=INF;
                for(int a=0;a<3;++a)if(std::abs(b.dir[a])>EPS) {
                    int cell=int(std::floor(q[a]/unit));double edge=(b.dir[a]>0?cell+1:cell)*double(unit);
                    next=std::min(next,t+(edge-q[a])/b.dir[a]);
                }
                if(section!=w.sections.end()) {
                    const auto&s=section->second;
                    struct Hit {double t;int face;uint32_t model;}; std::vector<Hit> candidates;
                    for(const auto&layer:s.layers) {
                        uint32_t id=layer.at(x,y,z);const Model&m=w.models[id];if(m.rgba[3]==0||m.boxes.empty())continue;
                        double best=INF;int bestface=0;
                        for(size_t box_index=0;box_index<m.boxes.size();++box_index) {
                            if(m.kind==1 && box_index>0) {
                                unsigned side=unsigned((box_index-1)/2);
                                int nx=x+(side==0?-1:(side==1?1:0)),nz=z+(side==2?-1:(side==3?1:0));
                                auto neighbour=w.sections.find(Key{floor16(nx),sY,floor16(nz)});
                                if(neighbour==w.sections.end())continue;
                                const auto&nm=w.models[neighbour->second.layers[0].at(nx,y,nz)];
                                bool connects=nm.kind==1;
                                if(nm.rgba[3]==255)for(const auto&nb:nm.boxes)if(nb.v==std::array<double,6>{0,0,0,1,1,1})connects=true;
                                if(!connects)continue;
                            }
                            const auto&box=m.boxes[box_index];
                            double nt,ft;int f;Vec local{o.x-x,o.y-y,o.z-z};
                            if(intersect(local,b.dir,box,nt,ft,f)&&ft>=t-EPS&&nt<=next+EPS&&nt>=t-EPS&&nt<best){best=std::max(nt,t);bestface=f;}
                        }
                        if(best<INF)candidates.push_back({best,bestface,id});
                    }
                    std::sort(candidates.begin(),candidates.end(),[](const Hit&a,const Hit&d){return a.t<d.t;});
                    if(candidates.empty())last_transparent=UINT32_MAX;
                    for(const auto&h:candidates) {
                        const auto&m=w.models[h.model]; if(m.rgba[3]<255&&last_transparent==h.model)continue;
                        auto rgba=m.rgba;unsigned ti=h.face==3?0:(h.face==2?2:1); auto tex=m.tex[ti];
                        Vec hit=add(o,mul(b.dir,h.t+EPS));double u=0,v=0;
                        if(h.face/2==1){u=hit.x-x;v=hit.z-z;}else if(h.face/2==0){u=hit.z-z;v=1-(hit.y-y);}else{u=hit.x-x;v=1-(hit.y-y);}
                        if(tex!=65535) {int tx=std::clamp(int(u*16),0,15),ty=std::clamp(int(v*16),0,15);size_t off=(ty*16+tx)*4;for(int a=0;a<4;++a)rgba[a]=uint8_t(unsigned(uint8_t(w.textures[tex][off+a]))*m.rgba[a]/255);rgba[3]=uint8_t(unsigned(rgba[3])*m.rgba[3]/255);}
                        if(rgba[3]==0)continue;
                        if((m.tint==1&&h.face==3)||m.tint==2||m.tint==3) {uint32_t tint=s.biome.at(x,y,z);if(m.tint==3)tint=0xe4763f; for(int a=0;a<3;++a)rgba[a]=uint8_t(unsigned(rgba[a])*((tint>>(a*8))&255)/255);}
                        double shade=h.face==3?1.0:(h.face==2?0.50:(h.face/2==0?0.72:0.84));
                        double alpha=rgba[3]/255.0;for(int a=0;a<3;++a)color[a]+=remain*alpha*rgba[a]*shade;remain*=1-alpha;
                        if(!has_hit) {has_hit=true;put_i32(out.hits,p*12,x);put_i32(out.hits,p*12+4,y);put_i32(out.hits,p*12+8,z);}
                        any_preview=any_preview||(known!=w.chunks.end()&&known->second==1);last_transparent=rgba[3]<255?h.model:UINT32_MAX;
                    }
                }
                t=std::max(t+EPS,next+EPS);
            }
            if(steps>=c.max_steps && remain>0.015)unknown=true;
        }else unknown=true;
        double background[3]={23,39,48};if(unknown&&remain>0.015){int s=((px/8+py/8)%2)*10;background[0]=45+s;background[1]=48+s;background[2]=54+s;}
        for(int a=0;a<3;++a)out.rgb[p*3+a]=char(byte(color[a]+remain*background[a]));
        out.preview[p]=char((any_preview?1:0)|(unknown&&remain>0.015?2:0));
    }
    return out;
}
std::vector<std::string> split_tiles(const std::string&rgb,int width,int height) {
    if(width<128||height<128||width>1024||height>1024||width%128||height%128||rgb.size()!=size_t(width)*height*3)throw std::invalid_argument("Invalid tile image");
    std::vector<std::string> result;
    for(int ty=0;ty<height;ty+=128)for(int tx=0;tx<width;tx+=128) {
        std::string tile;tile.reserve(49152);for(int row=0;row<128;++row)tile.append(rgb,(size_t(ty+row)*width+tx)*3,384);result.push_back(std::move(tile));
    }return result;
}
}
